#include "popcorn.h"

// Helper: compare wide strings
inline int wstrcmp(const CHAR16* a, const CHAR16* b) {
    while (*a && *b) {
        if (*a != *b) return (*a - *b);
        a++; b++;
    }
    return (*a - *b);
}

int pop_API pop_main(pop_Services* svc, int argc, CHAR16** argv) {
    int colors[6][3] = {
        {255,   0,   0}, // red
        {128, 128,   0}, // olive
        {  0, 255,   0}, // green
        {  0, 128, 128}, // teal
        {  0,   0, 255}, // blue
        {128,   0, 128}  // purple
    };
    int collen   = 6;
    int colindex = 0;

    if (argc > 1 && wstrcmp(argv[1], L"help") == 0) {
        svc->println(svc, L"        bsquare\r\n"
                          L"    or: bsquare help\r\n"
                          L"    or: bsquare forever\r\n\n"
                          L"A bouncing square animation. 'bsquare help' gives this help message.\r\n"
                          L"By default it runs for 500 frames. 'bsquare forever' makes it run\r\n"
                          L"until the system is powered off.");
        return 0;
    }
    BOOL forever = (argc > 1 && wstrcmp(argv[1], L"forever") == 0);

    popg_GraphicsServices* gfx = svc->sgfx;
    int code;
    if ((code = gfx->init(gfx)) != pop_SUCCESS) {
        svc->perrno(svc, code);
        return 1;
    }

    unsigned int x = 0, y = 0;
    int dx = 30, dy = 20;
    unsigned int size = 20;

    for (int frame = 0; forever || frame < 500; frame++) {
        // Clear screen to black
        for (unsigned int i = 0; i < gfx->w; i++) {
            for (unsigned int j = 0; j < gfx->h; j++) {
                gfx->putpixel(gfx, i, j, 0, 0, 0);
            }
        }

        // Draw the square in current color
        for (unsigned int i = 0; i < size; i++) {
            for (unsigned int j = 0; j < size; j++) {
                gfx->putpixel(gfx, x + i, y + j,
                              colors[colindex][0],
                              colors[colindex][1],
                              colors[colindex][2]);
            }
        }

        gfx->blit(gfx);

        // Update position
        x += dx;
        y += dy;

        // Bounce off edges and cycle color
        if (x + size >= gfx->w || x <= 0) {
            colindex = (colindex + 1) % collen;
            dx = -dx;
        }
        if (y + size >= gfx->h || y <= 0) {
            colindex = (colindex + 1) % collen;
            dy = -dy;
        }
    }

    gfx->deinit(gfx);
    return pop_SUCCESS;
}