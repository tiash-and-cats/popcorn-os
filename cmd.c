#include "popcorn.h"

typedef struct {
    int code;
    BOOL shouldexit;
} ExecResult;

inline CHAR16** split_whitespace(pop_Services* svc, CHAR16* line, int* argc_out);
inline ExecResult cmdexec(pop_Services* svc, CHAR16* line);
inline int wstrcmp(const CHAR16* a, const CHAR16* b);

int pop_API pop_main(pop_Services* svc, int argc, CHAR16** argv) {
    svc->println(svc, L"Popcorn OS version pre-v1.0.0");
    svc->println(svc, L"");
    while (1) {
        svc->print(svc, svc->curdir);
        svc->print(svc, L">");
        CHAR16* line = svc->readline(svc);
        if (cmdexec(svc, line).shouldexit) {
            break;
        }
    }
    return 0;
}

// Splits a CHAR16 string by spaces/tabs into tokens.
// Returns argv array (NULL-terminated) and sets *argc_out.
// Splits a CHAR16 string into tokens, respecting quotes.
// Supports "double quotes" and 'single quotes'.
// Returns argv array (NULL-terminated) and sets *argc_out.
inline CHAR16** split_shlex(pop_Services* svc, CHAR16* line, int* argc_out) {
    int len = 0;
    while (line[len]) len++;

    CHAR16** argv = svc->memalloc(svc, (len + 1) * sizeof(CHAR16*));
    int argc = 0;

    int i = 0;
    while (i < len) {
        // Skip whitespace
        while (i < len && (line[i] == L' ' || line[i] == L'\t')) i++;
        if (i >= len) break;

        // Start of token
        CHAR16 quote = 0;
        if (line[i] == L'"' || line[i] == L'\'') {
            quote = line[i++];
        }

        int start = i;
        while (i < len) {
            if (quote) {
                if (line[i] == quote) break;
            } else {
                if (line[i] == L' ' || line[i] == L'\t') break;
            }
            i++;
        }
        int toklen = i - start;

        // Allocate token string
        CHAR16* tok = svc->memalloc(svc, (toklen + 1) * sizeof(CHAR16));
        for (int j = 0; j < toklen; j++) {
            tok[j] = line[start + j];
        }
        tok[toklen] = 0;
        argv[argc++] = tok;

        if (quote && i < len && line[i] == quote) i++; // skip closing quote
    }

    argv[argc] = NULL;
    *argc_out = argc;
    return argv;
}

inline int wstrcmp(const CHAR16* a, const CHAR16* b) {
    while (*a && *b) {
        if (*a != *b) return (*a - *b);
        a++; b++;
    }
    return (*a - *b);
}

inline ExecResult cmdexec(pop_Services* svc, CHAR16* line) {
    ExecResult res;
    res.code = 0;
    res.shouldexit = FALSE;

    int argc2;
    CHAR16** argv2 = split_shlex(svc, line, &argc2);

    if (argc2 == 0) {
        svc->memfree(svc, argv2);
        svc->memfree(svc, line);
        return res;
    }

    if (wstrcmp(argv2[0], L"exit") == 0) {
        res.shouldexit = TRUE;
    } else if (wstrcmp(argv2[0], L"cd") == 0 && argc2 > 1) {
        if (svc->setcwd(svc, argv2[1]) != pop_SUCCESS) {
            svc->print(svc, L"cmd: cd: Could not go into ");
            svc->println(svc, argv2[1]);
            svc->print(svc, L"cmd: cd: ");
            svc->perrno(svc, svc->errcode);
            res.code = 1;
        }
    } else if (wstrcmp(argv2[0], L"echo") == 0 && argc2 > 1) {
        for (int i = 1; i < argc2; i++) {
            svc->print(svc, argv2[i]);
            svc->print(svc, L" ");
        }
        svc->println(svc, L"");
    } else {
        pop_Services* svc2 = svc->newsvc(svc);
        res.code = svc->pwait(svc, svc2, argc2, argv2);
        if (res.code < 0) {
            svc->print(svc, L"cmd: ");
            svc->perrno(svc, res.code);
        }
        svc->freesvc(svc, svc2);
    }

    // Free tokens
    for (int i = 0; i < argc2; i++) svc->memfree(svc, argv2[i]);
    svc->memfree(svc, argv2);
    svc->memfree(svc, line);
    
    return res;
}