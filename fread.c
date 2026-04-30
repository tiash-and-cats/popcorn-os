#include "popcorn.h"

int pop_API pop_main(pop_Services* svc, int argc, CHAR16** argv) {
    popf_File* f = svc->fileopen(svc, L"/hello.txt", (popf_FileMode){ .read = 1 });
    if (!f) {
        svc->print(svc, L"Oh no!\r\n");
        return 1;
    }
    CHAR16* text = f->read(f);
    f->close(f);
    svc->print(svc, text);
    svc->memfree(svc, text);
    svc->print(svc, L"\r\n");
    return 0;
}