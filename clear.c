#include "popcorn.h"
int pop_API pop_main(pop_Services* svc, int argc, CHAR16** argv) {
    for (int i = 0; i < 100; i++) {
        svc->println(svc, L"");
    }
    svc->curmove(svc, 0, 0);
    return 0;
}