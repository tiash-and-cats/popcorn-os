#include "popcorn.h"

int pop_API pop_main(pop_Services* svc, int argc, CHAR16** argv) {
    svc->sgfx->init(svc->sgfx);

    unsigned int w = svc->sgfx->w;
    unsigned int h = svc->sgfx->h;

    // Divide screen into 4 vertical bars
    unsigned int barw = w / 4;

    for (unsigned int y = 0; y < h; y++) {
        for (unsigned int x = 0; x < w; x++) {
            popg_Pixel px;
            if (x < barw) {
                px = (popg_Pixel){255, 0, 0};   // Red
            } else if (x < 2 * barw) {
                px = (popg_Pixel){0, 255, 0};   // Green
            } else if (x < 3 * barw) {
                px = (popg_Pixel){0, 0, 255};   // Blue
            } else {
                px = (popg_Pixel){255, 255, 255}; // White
            }
            svc->sgfx->frame[y * w + x] = px;
        }
    }

    svc->sgfx->blit(svc->sgfx);
    // keep framebuffer alive until exit
    return 0;
}
