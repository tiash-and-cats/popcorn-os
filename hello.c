#include "popcorn.h"

int pop_API pop_main(pop_Services* svc, int argc, CHAR16** argv) {
    svc->print(svc, L"Hello from C app!\r\n");
    return 0;
}