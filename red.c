#include "popcorn.h"

int pop_API pop_main(pop_Services* svc, int argc, CHAR16** argv) {
    svc->sgfx->init(svc->sgfx);
    for (int i = 0; i < (svc->sgfx->w * svc->sgfx->h); i++) {
        svc->sgfx->frame[i] = (popg_Pixel){ .r = 255, .g = 0, .b = 0 };
    }
    svc->sgfx->blit(svc->sgfx);
    svc->sgfx->deinit(svc->sgfx);
    return 0;
}