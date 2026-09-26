#include "font8x8_basic.h"

#include "sdl_inc.h"
#include <string.h>

/* Public-domain 8×8 glyphs (see font8x8_basic.h). Bit 0 is the left-hand pixel. */

int tom_text_width(const char *text, int scale)
{
    if (!text || scale < 1)
        return 0;
    return (int)strlen(text) * 8 * scale;
}

void tom_draw_text(SDL_Renderer *ren, int x, int y, int scale, const char *text,
                   Uint8 r, Uint8 g, Uint8 b)
{
    int cursor = x;
    if (!text || scale < 1)
        return;
    SDL_SetRenderDrawColor(ren, r, g, b, 255);
    for (; *text; text++) {
        unsigned char ch = (unsigned char)*text;
        int row;
        int col;
        if (ch > 127)
            ch = '?';
        for (row = 0; row < 8; row++) {
            unsigned char bits = font8x8_basic[ch][row];
            for (col = 0; col < 8; col++) {
                SDL_Rect px;
                if ((bits & (1u << col)) == 0)
                    continue;
                px.x = cursor + col * scale;
                px.y = y + row * scale;
                px.w = scale;
                px.h = scale;
                SDL_RenderFillRect(ren, &px);
            }
        }
        cursor += 8 * scale;
    }
}
