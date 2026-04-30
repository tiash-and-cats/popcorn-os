#include "popcorn.h"

int pop_main(pop_Services* svc, int argc, CHAR16** argv) {
    for (int i = 0; i < argc; i++) {
        svc->println(svc, argv[i]);
    }
    return 0;
}