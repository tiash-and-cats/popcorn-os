#include <efi.h>
#include <efilib.h>
#include "popcorn.h"

EFI_GUID gEfiGraphicsOutputProtocolGuid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;

int pop_API popgk_init(popg_GraphicsServices* sgfx) {
    EFI_GRAPHICS_OUTPUT_PROTOCOL* gop;
    EFI_STATUS status = gBS->LocateProtocol(
        &gEfiGraphicsOutputProtocolGuid,
        NULL,
        (void**)&gop
    );
    if (EFI_ERROR(status)) {
        return pop_EUEFIPR;
    }

    // Force Mode 0 for simplicity
    gop->SetMode(gop, 0);

    // Allocate framebuffer
    sgfx->w = gop->Mode->Info->HorizontalResolution;
    sgfx->h = gop->Mode->Info->VerticalResolution;
    sgfx->frame = (popg_Pixel*)sgfx->svc->memalloc(sgfx->svc, sizeof(popg_Pixel) * sgfx->w * sgfx->h);
    sgfx->shndl = (void*)gop;
    return pop_SUCCESS;
}

static inline int ctz(unsigned int mask) {
    int shift = 0;
    while (((mask >> shift) & 1) == 0 && shift < 32) {
        shift++;
    }
    return shift;
}

void pop_API popgk_blit(popg_GraphicsServices* sgfx) {
    EFI_GRAPHICS_OUTPUT_PROTOCOL* gop =
        (EFI_GRAPHICS_OUTPUT_PROTOCOL*)sgfx->shndl;

    UINT32 stride = sgfx->w;
    UINTN bufsize = sgfx->w * sgfx->h * sizeof(EFI_GRAPHICS_OUTPUT_BLT_PIXEL);

    EFI_GRAPHICS_OUTPUT_BLT_PIXEL* bltbuf;
    gBS->AllocatePool(EfiLoaderData, bufsize, (void**)&bltbuf);

    // Fill temp buffer
    for (UINT32 y = 0; y < sgfx->h; y++) {
        for (UINT32 x = 0; x < sgfx->w; x++) {
            popg_Pixel px = sgfx->frame[y * sgfx->w + x];
            bltbuf[y * stride + x].Blue  = px.b;
            bltbuf[y * stride + x].Green = px.g;
            bltbuf[y * stride + x].Red   = px.r;
            bltbuf[y * stride + x].Reserved = 0;
        }
    }

    // Push buffer to video in one call
    gop->Blt(
        gop,
        bltbuf,
        EfiBltBufferToVideo,
        0, 0,            // source coords
        0, 0,            // dest coords
        sgfx->w, sgfx->h,
        0                // Delta (0 = tightly packed)
    );

    gBS->FreePool(bltbuf);
}

void pop_API popgk_putpixel(popg_GraphicsServices* sgfx, unsigned int x, unsigned int y,
                    unsigned char r, unsigned char g, unsigned char b) {
    sgfx->frame[y * sgfx->w + x] = (popg_Pixel){ r, g, b };
}

popg_Pixel pop_API popgk_getpixel(popg_GraphicsServices* sgfx, unsigned int x, unsigned int y) {
    return sgfx->frame[y * sgfx->w + x];
}

void pop_API popgk_deinit(popg_GraphicsServices* sgfx) {
    sgfx->svc->memfree(sgfx->svc, sgfx->frame);
}