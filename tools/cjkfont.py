#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 The RadioFoundry Authors
"""Convert a CJK TrueType font into the C data the UI draws for a translated catalog (core/ui/font.h font_cjk_t).

    tools/cjkfont.py FONT.ttf CHARS.txt NAME LICENSE_FILE SPDX_ID WxH:ASCENT > core/ui/fonts/NAME.inc

or, to take the characters from a catalog instead of a text file:

    tools/cjkfont.py --po core/ui/lang/zh.po FONT.ttf NAME LICENSE_FILE SPDX_ID WxH:ASCENT > core/ui/fonts/NAME.inc

Output: `NAME_cp[]` (the codepoints, sorted), `NAME_off[]` (each glyph's byte offset in `NAME_bits`), `NAME_bits[]`
(a glyph is its first inked row, its rows from there to the last inked one, then those rows of
`ceil(width / 8)` bytes, MSB = leftmost pixel), and a `NAME` font_cjk_t. CHARS.txt holds the characters to include
(any bytes are read as UTF-8), or `--po` takes them from the catalog's msgstr values. Only the characters the
catalog actually draws are stored (a subset font: the flash cost is the text, not a whole writing system); the
character set is the argument to the script that regenerates it, so the subset grows with the catalog. The font's
own license text is carried into the header of the generated file.

The glyphs are drawn 1 bit a pixel, scaled so the cell is WxH with the baseline ASCENT rows from the top (the Latin
cell's ascent, so CJK sits on the same baseline). It needs Pillow with FreeType, and a CJK font under a permissive
license (Noto Sans CJK, OFL-1.1: core/ui/fonts/cjk_16x16.inc and LICENSES/OFL-1.1.txt).
"""
import argparse
import os
import re
import sys

from PIL import Image, ImageDraw, ImageFont


def po_unquote(s, where):
    """The characters a gettext quoted string stands for (i18n.py's, so the two agree)"""
    if not (s.startswith('"') and s.endswith('"')):
        raise ValueError("%s: expected a quoted string" % where)
    out, i, s = [], 0, s[1:-1]
    while i < len(s):
        if s[i] == "\\" and i + 1 < len(s):
            out.append({"n": "\n", "t": "\t", "\\": "\\", '"': '"'}.get(s[i + 1], s[i + 1]))
            i += 2
        else:
            out.append(s[i])
            i += 1
    return "".join(out)


def read_chars(args):
    if args.po:
        # The characters the catalog's translations draw (a single msgstr may span continuation lines); the language's
        # own name, in the header, is one of them
        chars = set()
        msgstr = None
        where = os.path.basename(args.po)
        with open(args.po, encoding="utf-8") as f:
            for line in f:
                m = re.match(r'msgstr "(.*)"$', line)
                if m:
                    msgstr = [m.group(1)]
                    continue
                m = re.match(r'"(.*)"$', line) if msgstr is not None else None
                if m:
                    msgstr.append(m.group(1))
                elif msgstr is not None:
                    chars.update(po_unquote('"' + "".join(msgstr) + '"', where))
                    msgstr = None
        if msgstr is not None:
            chars.update(po_unquote('"' + "".join(msgstr) + '"', where))
        return sorted(c for c in chars if ord(c) > 0x7F)
    chars = set()
    with open(args.chars, encoding="utf-8") as f:
        for line in f:
            chars.update(line.rstrip("\n"))
    return sorted(c for c in chars if ord(c) > 0x7F)


def render_glyphs(font_path, chars, w, h, ascent):
    font = ImageFont.truetype(font_path, w)  # a CJK glyph is a full-width cell, so the pixel size is the cell width
    out = {}
    for ch in chars:
        img = Image.new("L", (w, h), 0)
        d = ImageDraw.Draw(img)
        # The baseline lands at `ascent` rows from the top of the cell
        d.text((0, ascent - font.getmetrics()[0]), ch, fill=255, font=font)
        out[ord(ch)] = img
    return out


def glyph_to_data(img, w):
    """(top, rows, bytes) for a 1bpp glyph: first inked row, rows from there, packed rows MSB = left"""
    px = img.load()
    top = None
    bottom = None
    for y in range(img.height):
        inked = any(px[x, y] >= 128 for x in range(img.width))
        if inked:
            if top is None:
                top = y
            bottom = y
    if top is None:
        return (0, 0, b"")
    rows = bottom - top + 1
    stride = (w + 7) // 8
    data = bytearray()
    for y in range(top, top + rows):
        byte = 0
        for x in range(w):
            if px[x, y] >= 128:
                byte |= 0x80 >> (x & 7)
            if (x & 7) == 7:
                data.append(byte)
                byte = 0
        if (w & 7) != 0:
            data.append(byte)
    return (top, rows, bytes(data))


def c_array(name, values, per_line=16, fmt="0x%04X"):
    lines = []
    for i in range(0, len(values), per_line):
        lines.append("\t" + ",".join(fmt % v for v in values[i:i + per_line]) + ",")
    return "static const uint16_t %s[] = {\n%s\n};\n" % (name, "\n".join(lines))


def main():
    ap = argparse.ArgumentParser(description="Convert a CJK TTF into the firmware's subset font")
    ap.add_argument("--po", metavar="CATALOG", help="take the characters from a .po catalog's msgstr values")
    ap.add_argument("font", help="the source TrueType/OpenType font")
    ap.add_argument("chars", nargs="?", help="a text file of the characters to include (not with --po)")
    ap.add_argument("name", help="the output name (also the C symbols' prefix)")
    ap.add_argument("license", help="the license text file to carry into the header")
    ap.add_argument("spdx", help="the license's SPDX id (e.g. OFL-1.1)")
    ap.add_argument("size", help="the cell as WxH:ASCENT, e.g. 16x16:12")
    args = ap.parse_args()

    if not args.po and not args.chars:
        ap.error("a CHARS.txt file or --po is required")
    if args.po and args.chars:
        ap.error("--po and CHARS.txt are mutually exclusive")

    m = re.match(r"(\d+)x(\d+):(\d+)$", args.size)
    if not m:
        ap.error("size must be WxH:ASCENT")
    w, h, ascent = (int(m.group(i)) for i in (1, 2, 3))

    chars = read_chars(args)
    glyphs = render_glyphs(args.font, chars, w, h, ascent)

    cps = sorted(glyphs)
    bits = bytearray()
    off = []
    data = {}
    for cp in cps:
        off.append(len(bits))
        top, rows, glyph = glyph_to_data(glyphs[cp], w)
        data[cp] = (top, rows, glyph)
        bits += bytes((top, rows))
        bits += glyph

    # Header with the license text
    license_text = open(args.license, encoding="utf-8").read()
    license_lines = ["// " + l for l in license_text.splitlines()]
    name = args.name
    print("// SPDX-License-Identifier: %s" % args.spdx)
    print("//")
    print("// Generated by tools/cjkfont.py from %s. Do not edit." % args.font)
    print("//")
    for l in license_lines:
        print(l)
    print("//")
    print("#define %s_W %d" % (name.upper(), w))
    print("#define %s_H %d" % (name.upper(), h))
    print("#define %s_ASCENT %d" % (name.upper(), ascent))
    print("#define %s_BPP 1" % name.upper())
    print()

    print(c_array("%s_cp" % name, cps))
    print(c_array("%s_off" % name, off, per_line=12, fmt="0x%04X"))
    print("static const uint8_t %s_bits[] = {" % name)
    for i in range(0, len(bits), 16):
        print("\t" + ",".join("0x%02X" % v for v in bits[i:i + 16]) + ",")
    print("};")
    print()
    print("static const font_cjk_t %s = { %s_W, %s_H, %s_ASCENT, %s_BPP,"
          % (name, name.upper(), name.upper(), name.upper(), name.upper()))
    print("\t%s_cp, %s_off, %s_bits, %d };" % (name, name, name, len(cps)))


if __name__ == "__main__":
    main()
