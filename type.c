#include "popcorn.h"

int pop_API pop_main(pop_Services* svc, int argc, CHAR16** argv) {
    if (argc < 2) {
        svc->println(svc, L"usage: cat <filename>");
        return 1;
    }
    popf_File* f = svc->fileopen(svc, argv[1], (popf_FileMode){ .read = 1 });
    if (!f) {
        svc->print(svc, L"cat: could not open file ");
        svc->println(svc, argv[1]);
        return 1;
    }
    CHAR16* text = f->read(f);
    f->close(f);
    svc->println(svc, text);
    svc->memfree(svc, text);
    return 0;
}