#include "popcorn.h"

int pop_API pop_main(pop_Services* svc, int argc, CHAR16** argv) {
    popf_File* f = svc->fileopen(svc, L"/hello.txt", (popf_FileMode){ 
        .read = 1,
        .write = 1,
        .create = 1,
        .bytes = 0
    });
    if (!f) {
        svc->println(svc, L"Oh no!");
        return 1;
    }
    f->write(f, L"Hello world!");
    f->close(f);
    return 0;
}