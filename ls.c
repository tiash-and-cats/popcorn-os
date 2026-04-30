#include "popcorn.h"

int pop_main(pop_Services* svc, int argc, CHAR16** argv) {    popl_Node* ls = svc->dirlist(svc, argc > 1 ? argv[1] : L".");    if (!ls) {        svc->print(svc, L"ls: ");        svc->perrno(svc, svc->errcode);        return 1;    }    popl_Node* n = ls->next;    while (n) {        svc->println(svc, (CHAR16*)n->data);
        svc->memfree(svc, n->data);        n = n->next;    }    svc->slist->freelist(svc->slist, ls);
    return 0;
}