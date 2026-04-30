#include "popcorn.h"

popl_Node* pop_API poplk_newnode(popl_ListServices* slist, void* data) {
    popl_Node* node = (popl_Node*) slist->svc->memalloc(slist->svc, sizeof(popl_Node));
    node->data = data;
    node->next = NULL;
    node->prev = NULL;
    return node;
}

void pop_API poplk_freelist(popl_ListServices* slist, popl_Node* head) {
    popl_Node* current = head;
    while (current) {
        popl_Node* next = current->next;
        slist->svc->memfree(slist->svc, current);
        current = next;
    }
}

popl_Node* pop_API poplk_append(popl_ListServices* slist, popl_Node* head, void* data) {
    if (!head) {
        return poplk_newnode(slist, data);
    }
    popl_Node* last = head;
    while (last->next) {
        last = last->next;
    }
    last->next = poplk_newnode(slist, data);
    last->next->prev = last;
    return last->next;
}