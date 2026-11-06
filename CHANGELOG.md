<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 The RadioFoundry Authors -->

# Changelog

## rfoundry-zh

Simplified Chinese for the radio UI, and the CJK display fixes that make it readable.

- **Simplified Chinese interface** — a multi-byte CJK renderer and the Simplified Chinese catalog (`core/ui/lang/zh.po`,
  925 strings), switchable in Settings > Display > Language, with Chinese an optional default language.
- **CJK display fixes** — fullwidth forms drawn as two cells, a CJK glyph subset in every font size, glyph tops no
  longer clipped at the cell top, and text truncated at a character boundary (`fit_end`).
