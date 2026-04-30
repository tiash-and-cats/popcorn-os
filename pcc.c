#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "libtcc.h"

int main(int argc, char **argv) {
    puts("Popcorn OS Specialized C Compiler, version 1.0\n");
    
    if (argc < 3) {
        fprintf(stderr, "Usage: %s input.c output.bin\n", argv[0]);
        return 1;
    }

    const char *input = argv[1];
    const char *output = argv[2];

    TCCState *tcc = tcc_new();
    if (!tcc) { fprintf(stderr, "%s: could not create TCC state\n", argv[0]); return 1; }

    // Specialized flags
    tcc_set_output_type(tcc, TCC_OUTPUT_OBJ);

    // Force no stdlib/inc
    tcc_set_options(tcc, "-nostdlib -nostdinc -Wall -fno-builtin");

    // Compile
    FILE* f = fopen(input, "r");
    fseek(f, 0, SEEK_END);
    size_t len = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* code = (char*) malloc(len * sizeof(char));
    fread(code, sizeof(char), len, f);
    fclose(f);
    tcc_compile_string(tcc, code);
    free(code);
    
    // Output .bin
    f = fopen(output, "w");
    size_t blen = tcc_relocate(tcc, NULL);
    void* bytes = malloc(len);
    tcc_relocate(tcc, bytes);
    tcc_delete(tcc);
    fwrite(bytes, 1, blen, f);
    fclose(f);
    
    printf("Output written to %s\n", output);
    return 0;
}