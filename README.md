## What is this?

This collection of utilities tries to make it as easy as possible for you to display some text in your program. The goal is not to have a lot of functionality, to be very performant or for the fonts to look good. The goal is simply to display some text as quickly as possible.

Everything is ASCII only and based on the 6x11 [Tewi](https://github.com/lucy/tewi-font) font.

## Documentation

### `quickfont_sdl2.h`
Simply include this file in your code wherever you need it. You must include `SDL.h` *before* `quickfont_sdl2.h`. Supports surfaces and SDL_Renderer.

Three functions are defined:
- `void qfsdl2_render_text(SDL_Renderer *renderer, char *text, int x, int y, int scale)`: Uses SDL_Renderer to render text at coordinates `(x, y)`. Calls `SDL_RenderFillRect`, so you can use all the functions that apply to it, like `SDL_SetRenderDrawColor` or `SDL_SetRenderDrawBlendMode`. The `scale` parameter scales each whole glyph. For example, a scale of `1` means the resulting glyph would be of size `(6, 11)`. With a scale of `2`, it would be `(12, 22)`.

- `void qfsdl2_surface_render_text(SDL_Surface *surface, char *text, int x, int y, int scale, Uint32 color)`: Renders text onto a surface. Calls `SDL_FillRect`. Works pretty much the same as `qfsdl2_render_text`, except that you have pass in a color the same way you would pass one into `SDL_FillRect`.

- `SDL_Rect qfsdl2_get_text_bounding_box(char *text, int x, int y, int scale)`: Gets the bounding box of some text. This is useful if, for example, you want to make the background of the text a different color, or you want to know if the text fits in a certain place.

### `quickfont.h`
This file has a bunch of stuff you might find useful. Simply include this file in your code wherever you need it.

This file contains:
- `QF_GLYPH_WIDTH`: The width of each glyph.
- `QF_GLYPH_HEIGHT `: The height of each glyph.
- `qf_glyphs_str`: A table with each glyph being a string. Indexed by ASCII codepoint - 32. One glyph is one string. Transparent pixels are '.' and visible pixels are '@'.
- `qf_glyphs_8bpp`: A table with each glyph as an 8 bits per pixel bitmap. Indexed by ASCII codepoint - 32.
- `qf_glyphs_24bpp`: The same as `qf_glyphs_8bpp`, except the glyphs are 24 bits per pixel RGB bitmaps.
- `qf_glyphs_32bpp`: The same as `qf_glyphs_8bpp` and `qf_glyphs_24bpp`, except the glyphs are 32 bits per pixel RGBA bitmaps.
- `qf_atlas_32bpp`: A 32 bits per pixel RGBA atlas. This is the same as `quickfont_alpha.bmp`, except that you don't need to load a bitmap from a file.
- `QF_ATLAS_WIDTH`: The width of the atlas.
- `QF_ATLAS_HEIGHT`: The height of the atlas.
- `void qf_atlas_uv(char c, float *x0, float *y0, float *x1, float *y1)`: Gives you 4 UV coordinates for an ASCII character from either `qf_atlas_32bpp`, `quickfont.bmp` or `quickfont_alpha.bmp`. Useful if you are using OpenGL/D3D/Vulkan/etc. directly.

### `quickfont.txt`
A plain text description of the font that is supposed to be extremely easy to parse. The first three lines are the width and height of each glyph, followed by an empty line. After this, the glyphs, separated by empty lines, are laid out line by line. A '.' represents a transparent pixel, and a '@' represents a visible pixel.

### `quickfont.bmp`
A bitmap file with all glyphs laid out one after another. To get the X coordinate of a glyph in the bitmap, simply take the ASCII number, subtract 32 (as 32 is the first visual character) and multiply by 6 (the width of each glyph): `x = (char_code - 32) * 6`

### `quickfont_alpha.bmp`
Same as 'quickfont.bmp`, except that the background is transparent instead of black.

## Building

This is normally not necessary; simply grab whatever files you need from this repo. However, if for some reason you want to build quickfont yourself, it's extremely simple:

1. Make sure you are in the quickfont directory
2. Compile `generate.c` (any C compiler on any platform should work). For example:
    - `gcc generate/generate.c -o generate/generate`
3. Run the resulting executable
