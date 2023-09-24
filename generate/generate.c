#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <stdint.h>

#define MAX_LINE_LEN 512

#define GLYPH_COUNT ('~' - ' ' + 1)

// could probably get this info from the bdf file but I don't care
#define FONT_WIDTH 6
#define FONT_HEIGHT 11
#define ORIGIN_X 0
#define ORIGIN_Y 9

#pragma pack(push, 1)
typedef struct {
    uint16_t file_type;
    uint32_t file_size;
    uint16_t reserved0;
    uint16_t reserved1;
    uint32_t bitmap_offset;
    uint32_t dib_hdr_size;
    int32_t width;
    int32_t height;
    uint16_t planes;
    uint16_t bits_per_pixel;
    uint32_t compression;
    uint32_t image_size;
    int32_t horizontal_resolution;
    int32_t vertical_resolution;
    uint32_t num_colors_used;
    uint32_t num_colors_important;
    uint32_t red_mask;
    uint32_t green_mask;
    uint32_t blue_mask;
    uint32_t alpha_mask;
} BMPHeader;
#pragma pack(pop)

static void error(char *msg) {
    fprintf(stderr, "%s\n", msg);
    exit(1);
}

static void next_line(char *buf, int size, FILE *file) {
    if (!fgets(buf, size, file)) {
        error("fgets() failed");
    }
    if (*buf && buf[strlen(buf)-1] == '\n') {
        buf[strlen(buf)-1] = 0;
    }
}

static int starts_with(char *str, char *substr) {
    return strncmp(str, substr, strlen(substr)) == 0;
}

static int is_visible_ascii(int c) {
    return c >= 32 && c < 127;
}

static char *hex_to_bits(char c) {
    switch (tolower(c)) {
        case '0': return "0000";
        case '1': return "0001";
        case '2': return "0010";
        case '3': return "0011";
        case '4': return "0100";
        case '5': return "0101";
        case '6': return "0110";
        case '7': return "0111";
        case '8': return "1000";
        case '9': return "1001";
        case 'a': return "1010";
        case 'b': return "1011";
        case 'c': return "1100";
        case 'd': return "1101";
        case 'e': return "1110";
        case 'f': return "1111";
        default: error("not a hex digit"); return "";
    }
}

static int parse_glyph(FILE *bdf_file, char *line, char *bmp) {
    int encoding;
    int width;
    int height;
    int xoff;
    int yoff;

    int got_encoding = 0;
    int got_bbx = 0;
    int got_bitmap = 0;
    
    while (strcmp(line, "ENDCHAR") != 0) {
        next_line(line, MAX_LINE_LEN, bdf_file);
        if (starts_with(line, "ENCODING ")) {
            if (sscanf(line, "ENCODING %i", &encoding) != 1) {
                error("sscanf() on ENCODING failed");
            }
            got_encoding = 1;
        } else if (starts_with(line, "BBX ")) {
            if (sscanf(line, "BBX %i %i %i %i", &width, &height, &xoff, &yoff) != 4) {
                error("sscanf() on BBX failed");
            }
            got_bbx = 1;
        } else if (strcmp(line, "BITMAP") == 0) {
            if (!got_bbx) {
                error("need BBX before BITMAP");
            }
            
            memset(bmp, '0', FONT_WIDTH * FONT_HEIGHT);

            for (int y = 0; y < height; ++y) {
                next_line(line, MAX_LINE_LEN, bdf_file);

                if (is_visible_ascii(encoding)) {
                    // non-ascii characters can be outside of the font bounds.. i don't know why,
                    // but it doesnt matter anyway, since we only care about ascii.

                    char row[512] = "";
                    for (char *s = line; *s; ++s) {
                        char *bits = hex_to_bits(*s);
                        strcat(row, bits);
                    }
                    if (strlen(row) < width) {
                        error("bitmap row too small");
                    }
                    
                    for (int x = 0; x < width; ++x) {
                        int actual_y = ORIGIN_Y - (yoff + height) + y;
                        int actual_x = ORIGIN_X + x + xoff;
                        assert(actual_x >= 0 && actual_x < FONT_WIDTH);
                        assert(actual_y >= 0 && actual_y < FONT_HEIGHT);
                        bmp[actual_y*FONT_WIDTH + actual_x] = row[x];
                    }
                }
            }
            
            next_line(line, MAX_LINE_LEN, bdf_file);
            if (strcmp(line, "ENDCHAR") != 0) {
                error("expected ENDCHAR");
            }
            
            got_bitmap = 1;
        }
    }
    
    if (!got_encoding || !got_bbx || !got_bitmap) {
        error("glyph did not contain ENCODING, BBX or BITMAP");
    }

    return encoding;
}

static void save_bmp(char *path, uint8_t *rgba, int width, int height) {
    FILE *f = fopen(path, "wb");
    if (!f) {
        error("could not create file");
    }
    
    int img_size_in_bytes = width * height * 4;
    
    BMPHeader header = {0};
    header.file_type = 0x4D42;
    header.file_size = sizeof(BMPHeader) + img_size_in_bytes;
    header.bitmap_offset = sizeof(BMPHeader);
    header.dib_hdr_size = sizeof(BMPHeader) - 14;
    header.width = width;
    header.height = -height;
    header.planes = 1;
    header.bits_per_pixel = 32;
    header.compression = 3; // RGBA
    header.image_size = img_size_in_bytes;
    header.horizontal_resolution = 1024; // ??
    header.vertical_resolution = 1024; // ??
    header.red_mask = 0x000000FF;
    header.green_mask = 0x0000FF00;
    header.blue_mask = 0x00FF0000;
    header.alpha_mask = 0xFF000000;

    fwrite(&header, sizeof(BMPHeader), 1, f);
    fwrite(rgba, 1, img_size_in_bytes, f);
    
    fclose(f);
}

static char *read_file(char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        error("could not open file");
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *mem = malloc(size + 1);
    fread(mem, size, 1, f);
    mem[size] = 0;
    return mem;
}

static void gen_txt(char **bmps) {
    FILE *f = fopen("quickfont.txt", "wb");
    if (!f) {
        error("could not create file");
    }
    
    fprintf(f, "%i\n", FONT_WIDTH);
    fprintf(f, "%i\n", FONT_HEIGHT);
    fprintf(f, "\n");

    for (int i = ' '; i <= '~'; ++i) {
        char *bmp = bmps[i - ' '];
        for (int y = 0; y < FONT_HEIGHT; ++y) {
            for (int x = 0; x < FONT_WIDTH; ++x) {
                char pix = bmp[y*FONT_WIDTH + x];
                fprintf(f, "%c", pix == '0' ? '.' : '@');
            }
            fprintf(f, "\n");
        }
        if (i != '~') {
            fprintf(f, "\n");
        }
    }
    
    fclose(f);
}

static void gen_bmp(char **bmps) {
    int width = FONT_WIDTH*GLYPH_COUNT;
    int height = FONT_HEIGHT;
    uint8_t *black_bg = malloc(width*height*4);
    uint8_t *alpha_bg = malloc(width*height*4);

    for (int i = 0; i < GLYPH_COUNT; ++i) {
        char *bmp = bmps[i];

        for (int y = 0; y < FONT_HEIGHT; ++y) {
            for (int x = 0; x < FONT_WIDTH; ++x) {
                char src = bmp[y*FONT_WIDTH + x];
                int ptr_offset = y*width*4 + (i*FONT_WIDTH*4) + x*4;
                uint8_t *black_dst = black_bg + ptr_offset;
                uint8_t *alpha_dst = alpha_bg + ptr_offset;
                uint8_t pix = src == '0' ? 0 : 255;

                black_dst[0] = pix;
                black_dst[1] = pix;
                black_dst[2] = pix;
                black_dst[3] = 255;

                alpha_dst[0] = pix;
                alpha_dst[1] = pix;
                alpha_dst[2] = pix;
                alpha_dst[3] = pix;
            }
        }
    }
    
    save_bmp("quickfont.bmp", black_bg, width, height);
    save_bmp("quickfont_alpha.bmp", alpha_bg, width, height);
}

static void gen_sdl2(char **bmps) {
    FILE *f = fopen("quickfont_sdl2.h", "wb");
    if (!f) {
        error("could not create file");
    }
    
    fprintf(f, "#ifndef INCLUDE_QUICKFONT_SDL2_H_\n");
    fprintf(f, "#define INCLUDE_QUICKFONT_SDL2_H_\n");
    fprintf(f, "\n");
    fprintf(f, "#define QFSDL2_NUM_GLYPHS %i\n", GLYPH_COUNT);
    fprintf(f, "#define QFSDL2_GLYPH_WIDTH %i\n", FONT_WIDTH);
    fprintf(f, "#define QFSDL2_GLYPH_HEIGHT %i\n", FONT_HEIGHT);
    fprintf(f, "\n");

    fprintf(f, "static const char qfsdl2__data[QFSDL2_NUM_GLYPHS][QFSDL2_GLYPH_WIDTH*QFSDL2_GLYPH_HEIGHT + 1] = { // + 1 to include null terminator\n");
    for (int i = 0; i < GLYPH_COUNT; ++i) {
        fprintf(f, "    {\"%.*s\"},\n", FONT_WIDTH * FONT_HEIGHT, bmps[i]);
    }
    
    fprintf(f, "};\n");
    fprintf(f, "\n");

    fprintf(f,
        "static SDL_Rect *qfsdl2__get_text_rects(int x, int y, int scale, const char *text, int *num, SDL_Rect *bb) {\n"
        "    if (scale < 1) {\n"
        "        scale = 1;\n"
        "    }\n"
        "    \n"
        "    int at_x = x;\n"
        "    int at_y = y;\n"
        "\n"
        "    if (bb) {\n"
        "        bb->x = x;\n"
        "        bb->y = y;\n"
        "        bb->w = 0;\n"
        "        bb->h = 0;\n"
        "    }\n"
        "\n"
        "    int text_len = (int)SDL_strlen(text);\n"
        "\n"
        "    // we are caching the allocation here; no need to spam SDL_malloc every time we call this function\n"
        "    // it should be fine to use static here because SDL_Renderer is not thread-safe anyway\n"
        "    static int cached_max_possible_rects = 0;\n"
        "    static SDL_Rect *rects = 0;\n"
        "\n"
        "    int max_possible_rects = text_len * QFSDL2_GLYPH_WIDTH * QFSDL2_GLYPH_HEIGHT;\n"
        "    if (cached_max_possible_rects < max_possible_rects) {\n"
        "        SDL_free(rects);\n"
        "        rects = (SDL_Rect *)SDL_malloc(sizeof(SDL_Rect) * max_possible_rects);\n"
        "        cached_max_possible_rects = max_possible_rects;\n"
        "    }\n"
        "    \n"
        "    int num_rects = 0;\n"
        "\n"
        "    for (int i = 0; i < text_len; ++i) {\n"
        "        char c = text[i];\n"
        "\n"
        "        if (i == 0 && bb) {\n"
        "            bb->h = QFSDL2_GLYPH_HEIGHT * scale;\n"
        "        }\n"
        "\n"
        "        if ((c < ' ' || c > '~') && (c != '\\n')) {\n"
        "            // if not a visible ascii character, just render a space instead\n"
        "            c = ' ';\n"
        "        }\n"
        "        \n"
        "        if (c == '\\n') {\n"
        "            int adv = QFSDL2_GLYPH_HEIGHT * scale;\n"
        "            at_x = x;\n"
        "            at_y += adv;\n"
        "            if (bb) {\n"
        "                bb->h += adv;\n"
        "            }\n"
        "        } else {\n"
        "            for (int gy = 0; gy < QFSDL2_GLYPH_HEIGHT; ++gy) {\n"
        "                for (int gx = 0; gx < QFSDL2_GLYPH_WIDTH; ++gx) {\n"
        "                    char pix = qfsdl2__data[c - ' '][gy*QFSDL2_GLYPH_WIDTH + gx];\n"
        "                    if (pix != '0') {\n"
        "                        SDL_Rect rect;\n"
        "                        rect.w = scale;\n"
        "                        rect.h = scale;\n"
        "                        rect.x = at_x + gx*scale;\n"
        "                        rect.y = at_y + gy*scale;\n"
        "                        rects[num_rects++] = rect;\n"
        "                    }\n"
        "                }\n"
        "            }\n"
        "            at_x += QFSDL2_GLYPH_WIDTH * scale;\n"
        "            if (bb && ((at_x - x) > bb->w)) {\n"
        "                bb->w = at_x - x;\n"
        "            }\n"
        "        }\n"
        "    }\n"
        "    \n"
        "    *num = num_rects;\n"
        "    return rects;\n"
        "}\n"
        "\n"
        "static void qfsdl2_render_text(SDL_Renderer *renderer, const char *text, int x, int y, int scale) {\n"
        "    int num_rects = 0;\n"
        "    SDL_Rect *rects = qfsdl2__get_text_rects(x, y, scale, text, &num_rects, 0);\n"
        "    SDL_RenderFillRects(renderer, rects, num_rects);\n"
        "}\n"
        "\n"
        "static void qfsdl2_surface_render_text(SDL_Surface *surface, const char *text, int x, int y, int scale, Uint32 color) {\n"
        "    int num_rects = 0;\n"
        "    SDL_Rect *rects = qfsdl2__get_text_rects(x, y, scale, text, &num_rects, 0);\n"
        "    SDL_FillRects(surface, rects, num_rects, color);\n"
        "}\n"
        "\n"
        "static SDL_Rect qfsdl2_get_text_bounding_box(char *text, int x, int y, int scale) {\n"
        "    SDL_Rect bb = {0};\n"
        "    int num_rects = 0;\n"
        "    SDL_Rect *rects = qfsdl2__get_text_rects(x, y, scale, text, &num_rects, &bb);\n"
        "    return bb;\n"
        "}\n"
    );
    
    fprintf(f, "\n");
    fprintf(f, "#endif\n");

    fclose(f);
}

static void gen_h(char **bmps) {
    FILE *f = fopen("quickfont.h", "wb");
    if (!f) {
        error("could not create file");
    }
    
    fprintf(f, "#ifndef INCLUDE_QUICKFONT_H_\n");
    fprintf(f, "#define INCLUDE_QUICKFONT_H_\n");
    fprintf(f, "\n");
    fprintf(f, "#define QF_NUM_GLYPHS %i\n", GLYPH_COUNT);
    fprintf(f, "#define QF_GLYPH_WIDTH %i\n", FONT_WIDTH);
    fprintf(f, "#define QF_GLYPH_HEIGHT %i\n", FONT_HEIGHT);
    fprintf(f, "\n");
    fprintf(f, "#define QF_ATLAS_WIDTH (QF_NUM_GLYPHS * QF_GLYPH_WIDTH)\n");
    fprintf(f, "#define QF_ATLAS_HEIGHT QF_GLYPH_HEIGHT\n");
    fprintf(f, "\n");

    fprintf(f, "static void qf_atlas_uv(char c, float *x0, float *y0, float *x1, float *y1) {\n");
    fprintf(f, "    if (c < 32 || c > 126) {\n");
    fprintf(f, "        c = ' ';\n");
    fprintf(f, "    }\n");
    fprintf(f, "    c -= 32;\n");
    fprintf(f, "    *x0 = (float)(c * QF_GLYPH_WIDTH) / (float)QF_ATLAS_WIDTH;\n");
    fprintf(f, "    *x1 = (float)((c+1) * QF_GLYPH_WIDTH) / (float)QF_ATLAS_WIDTH;\n");
    fprintf(f, "    *y0 = 0.0f;\n");
    fprintf(f, "    *y1 = 1.0f;\n");
    fprintf(f, "}\n");
    fprintf(f, "\n");

    fprintf(f, "static const char *qf_glyphs_str[] = {\n");
    for (int i = 0; i < GLYPH_COUNT; ++i) {
        char *bmp = bmps[i];
        fprintf(f, "    /* '%c' (%i) */ \"", i + 32, i + 32);
        for (int j = 0; j < FONT_WIDTH * FONT_HEIGHT; ++j) {
            fprintf(f, "%c", bmp[j] == '0' ? '.' : '@');
        }
        fprintf(f, "\",\n");
    }
    fprintf(f, "};\n\n");

    fprintf(f, "static const unsigned char qf_glyphs_8bpp[][QF_GLYPH_WIDTH*QF_GLYPH_HEIGHT] = {\n");
    for (int i = 0; i < GLYPH_COUNT; ++i) {
        char *bmp = bmps[i];
        fprintf(f, "    /* '%c' (%i) */ {", i + 32, i + 32);
        for (int j = 0; j < FONT_WIDTH * FONT_HEIGHT; ++j) {
            fprintf(f, "%i%s", bmp[j] == '0' ? 0 : 255, (j != FONT_WIDTH*FONT_HEIGHT - 1) ? ", " : "");
        }
        fprintf(f, "},\n");
    }
    fprintf(f, "};\n\n");
    
    fprintf(f, "static const unsigned char qf_glyphs_24bpp[][QF_GLYPH_WIDTH*QF_GLYPH_HEIGHT*3] = {\n");
    for (int i = 0; i < GLYPH_COUNT; ++i) {
        char *bmp = bmps[i];
        fprintf(f, "    /* '%c' (%i) */ {", i + 32, i + 32);
        for (int j = 0; j < FONT_WIDTH * FONT_HEIGHT; ++j) {
            for (int k = 0; k < 3; ++k) {
                fprintf(f, "%i%s", bmp[j] == '0' ? 0 : 255, (j != FONT_WIDTH*FONT_HEIGHT - 1 || k != 2) ? ", " : "");
            }
        }
        fprintf(f, "},\n");
    }
    fprintf(f, "};\n\n");
    
    fprintf(f, "static const unsigned int qf_glyphs_32bpp[][QF_GLYPH_WIDTH*QF_GLYPH_HEIGHT] = {\n");
    for (int i = 0; i < GLYPH_COUNT; ++i) {
        char *bmp = bmps[i];
        fprintf(f, "    /* '%c' (%i) */ {", i + 32, i + 32);
        for (int j = 0; j < FONT_WIDTH * FONT_HEIGHT; ++j) {
            fprintf(f, "%s%s", bmp[j] == '0' ? "0" : "0xFFFFFFFF", (j != FONT_WIDTH*FONT_HEIGHT - 1) ? ", " : "");
        }
        fprintf(f, "},\n");
    }
    fprintf(f, "};\n\n");
    
    uint8_t *bmp_file = (uint8_t *)read_file("quickfont_alpha.bmp");
    BMPHeader *bmp_hdr = (BMPHeader *)bmp_file;
    uint32_t *bmp_pixels = (uint32_t *)(bmp_file + bmp_hdr->bitmap_offset);
    fprintf(f, "static const unsigned int qf_atlas_32bpp[] = {\n    ");
    for (int i = 0; i < abs(bmp_hdr->width*bmp_hdr->height); ++i) {
        fprintf(f, "0x%x, ", bmp_pixels[i]);
        if (i % 32 == 0 && i > 0) {
            fprintf(f, "\n    ");
        }
    }
    fprintf(f, "\n};\n\n");

    fprintf(f, "#endif\n");

    fclose(f);
}

int main(int argc, char **argv) {
    FILE *bdf_file = fopen("generate/tewi-medium-11.bdf", "rb");
    if (!bdf_file) {
        error("could not open font file");
    }

    char *bmps[GLYPH_COUNT] = {0};
    int num_bmps = 0;

    for (;;) {
        char line[MAX_LINE_LEN];
        next_line(line, MAX_LINE_LEN, bdf_file);

        if (strcmp(line, "ENDFONT") == 0) {
            break;
        }
        
        if (starts_with(line, "COMMENT")) {
            continue;
        }
        
        if (starts_with(line, "STARTCHAR")) {
            char *bmp = malloc(FONT_WIDTH * FONT_HEIGHT);
            int codepoint = parse_glyph(bdf_file, line, bmp);
            if (is_visible_ascii(codepoint)) {
                int idx = codepoint - ' ';
                assert(idx >= 0 && idx < GLYPH_COUNT);
                if (bmps[idx]) {
                    error("duplicate glyph");
                }
                bmps[idx] = bmp;
                ++num_bmps;
                if (num_bmps == GLYPH_COUNT) {
                    break;
                }
            }
        }
    }

    gen_txt(bmps);
    gen_bmp(bmps);
    gen_sdl2(bmps);
    gen_h(bmps);

    return 0;
}
