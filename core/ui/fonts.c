// SPDX-License-Identifier: MIT
// Copyright (c) 2026 The RadioFoundry Authors
// The UI's fonts. The bitmaps are Spleen 2.1.0 by Frederic Cambus (BSD-2-Clause, see core/ui/fonts/spleen_*.inc and
// LICENSES/BSD-2-Clause.txt). CONFIG_FONT_AA replaces the sizes from 6x12 up with JetBrains Mono Bold, anti-aliased,
// for color panels (font.h), and CONFIG_FONT_BIG adds its 9x18 (OFL-1.1, core/ui/fonts/jbmono_*.inc and
// LICENSES/OFL-1.1.txt). Both converted by tools/fontconv.py.
#include "font.h"

#define FONT(var, n, N, bpp, cjk) \
	const font_t var = { N##_W, N##_H, N##_ASCENT, N##_MISSING, bpp, FONT_LATIN1 && N##_LATIN1, n##_bits, n##_index, cjk }

// The CJK subsets, one per font size (a full-width cell, twice each font's width): the translated catalog's glyphs,
// from Noto Sans CJK (OFL-1.1, core/ui/fonts/cjk_*.inc and LICENSES/OFL-1.1-Noto.txt), converted by tools/cjkfont.py
#if CONFIG_FONT_CJK
#include "fonts/cjk_6x12.inc"
#include "fonts/cjk_8x16.inc"
#if CONFIG_FONT_BIG
#include "fonts/cjk_9x18.inc"
#endif
#include "fonts/cjk_12x24.inc"
#if CONFIG_FONT_XL
#include "fonts/cjk_16x32.inc"
#endif
#define CJK_6X12	(&cjk_6x12)
#define CJK_8X16	(&cjk_8x16)
#if CONFIG_FONT_BIG
#define CJK_9X18	(&cjk_9x18)
#else
#define CJK_9X18	NULL
#endif
#define CJK_12X24	(&cjk_12x24)
#if CONFIG_FONT_XL
#define CJK_16X32	(&cjk_16x32)
#else
#define CJK_16X32	NULL
#endif
#else
#define CJK_6X12	NULL
#define CJK_8X16	NULL
#define CJK_9X18	NULL
#define CJK_12X24	NULL
#define CJK_16X32	NULL
#endif

#include "fonts/spleen_5x8.inc"
FONT(font_5x8, spleen_5x8, SPLEEN_5X8, 1, NULL);

#if CONFIG_FONT_AA
#include "fonts/jbmono_6x12.inc"
#include "fonts/jbmono_8x16.inc"
#include "fonts/jbmono_12x24.inc"
#if CONFIG_FONT_BIG
#include "fonts/jbmono_9x18.inc"
#endif
#if CONFIG_FONT_XL
#include "fonts/jbmono_16x32.inc"
#endif
#ifdef FONT_BOTH
bool font_aa_on = true;
#if CONFIG_FONT_BIG
bool font_big_on = true;
#endif
#define AA(n)	font_aa_##n
#else
#define AA(n)	font_##n
#endif
FONT(AA(6x12), jbmono_6x12, JBMONO_6X12, JBMONO_6X12_BPP, CJK_6X12);
FONT(AA(8x16), jbmono_8x16, JBMONO_8X16, JBMONO_8X16_BPP, CJK_8X16);
FONT(AA(12x24), jbmono_12x24, JBMONO_12X24, JBMONO_12X24_BPP, CJK_12X24);
#if CONFIG_FONT_BIG
FONT(AA(9x18), jbmono_9x18, JBMONO_9X18, JBMONO_9X18_BPP, CJK_9X18);
#endif
#if CONFIG_FONT_XL
FONT(AA(16x32), jbmono_16x32, JBMONO_16X32, JBMONO_16X32_BPP, CJK_16X32);
#endif
#endif

#if !CONFIG_FONT_AA || defined(FONT_BOTH)
#include "fonts/spleen_6x12.inc"
#include "fonts/spleen_8x16.inc"
#include "fonts/spleen_12x24.inc"
FONT(font_6x12, spleen_6x12, SPLEEN_6X12, 1, CJK_6X12);
FONT(font_8x16, spleen_8x16, SPLEEN_8X16, 1, CJK_8X16);
FONT(font_12x24, spleen_12x24, SPLEEN_12X24, 1, CJK_12X24);
#if CONFIG_FONT_XL
#include "fonts/spleen_16x32.inc"
FONT(font_16x32, spleen_16x32, SPLEEN_16X32, 1, CJK_16X32);
#endif
#endif

// Latin-1 0xA0..0xFF as plain ASCII, for a font or a build without those glyphs
static const char fold[] = " !cLoY|S ca<--r-o+23'uP.,1o>///?"
			     "AAAAAAACEEEEIIIIDNOOOOOxOUUUUYPs"
			     "aaaaaaaceeeeiiiidnooooo/ouuuuypy";

char font_fold(char c)
{
	return (uint8_t)c >= FONT_LATIN1_FIRST ? fold[(uint8_t)c - FONT_LATIN1_FIRST] : c;
}

const uint8_t *font_glyph(const font_t *f, uint8_t c, int *top, int *rows)
{
	unsigned slot;
	if (c >= 0x20 && c < 0x7F) {
		slot = c - 0x20u;
	} else if (c >= GLYPH_FIRST_EXTRA && c < GLYPH_FIRST_EXTRA + GLYPH_EXTRA_COUNT) {
		if (f->missing & (1u << (c - GLYPH_FIRST_EXTRA)))
			return NULL;
		slot = 95u + (c - GLYPH_FIRST_EXTRA);
	} else if (c >= FONT_LATIN1_FIRST && f->latin1) {
		slot = 95u + GLYPH_EXTRA_COUNT + (c - FONT_LATIN1_FIRST);
	} else if (c >= FONT_LATIN1_FIRST) {
		slot = (unsigned)fold[c - FONT_LATIN1_FIRST] - 0x20u;	// the letter without its accent
	} else {
		return NULL;
	}
	const uint8_t *g = f->bits + f->index[slot];
	*top = g[0];
	*rows = g[1];
	return g + 2;
}

// A CJK glyph by codepoint: its first inked row, the rows from there, then the rows of packed pixels. A subset
// keeps only the catalog's characters, so a search is a binary lookup in the sorted codepoint array
static const uint8_t *cjk_glyph(const font_cjk_t *c, unsigned cp, int *top, int *rows)
{
	int lo = 0, hi = (int)c->count - 1;
	while (lo <= hi) {
		int mid = (lo + hi) / 2;
		unsigned m = c->cp[mid];
		if (cp < m)
			hi = mid - 1;
		else if (cp > m)
			lo = mid + 1;
		else {
			const uint8_t *g = c->bits + c->off[mid];
			*top = g[0];
			*rows = g[1];
			return g + 2;
		}
	}
	return NULL;
}

void font_glyph_utf8(const font_t *f, const char **p, font_glyph_utf8_t *g)
{
	const uint8_t *s = (const uint8_t *)*p;
	uint8_t b = s[0];
	g->w = f->w;
	g->h = f->h;
	g->bpp = f->bpp;
	// A byte that isn't a UTF-8 lead: ASCII, the glyph bytes (0x80..0x8F) and invalid bytes, one cell exactly as
	// font_glyph
	if (b < 0xC0 || b >= 0xF8) {
		*p = (const char *)(s + 1);
		g->advance = f->w;
		g->bits = font_glyph(f, b, &g->top, &g->rows);
		return;
	}
	// Decode a UTF-8 sequence (2, 3 or 4 bytes); a byte whose continuations are wrong is drawn as itself
	unsigned cp;
	int len;
	if (b < 0xE0) {
		len = 2;
		cp = b & 0x1Fu;
	} else if (b < 0xF0) {
		len = 3;
		cp = b & 0x0Fu;
	} else {
		len = 4;
		cp = b & 0x07u;
	}
	for (int i = 1; i < len; i++) {
		if ((s[i] & 0xC0u) != 0x80u) {
			*p = (const char *)(s + 1);
			g->advance = f->w;
			g->bits = font_glyph(f, b, &g->top, &g->rows);
			return;
		}
		cp = (cp << 6) | (s[i] & 0x3Fu);
	}
	*p = (const char *)(s + len);
	// A CJK character or a fullwidth form: a full-width cell from the font's CJK subset, or a box (bits == NULL) for
	// one it lacks (the same range i18n.py's cjk_drawable allows)
	if (f->cjk && ((cp >= 0x3000 && cp <= 0x9FFF) || (cp >= 0xFF00 && cp <= 0xFFEF))) {
		g->w = f->cjk->w;
		g->h = f->cjk->h;
		g->bpp = f->cjk->bpp;
		g->advance = 2 * f->w;
		g->bits = cjk_glyph(f->cjk, cp, &g->top, &g->rows);
		return;
	}
	// Any other multi-byte character (a symbol the font lacks): one cell, no glyph (the caller draws a box)
	g->advance = f->w;
	g->bits = cp <= 0xFF ? font_glyph(f, (uint8_t)cp, &g->top, &g->rows) : NULL;
}
