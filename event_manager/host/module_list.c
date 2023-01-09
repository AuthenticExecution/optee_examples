#include "module.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "logging.h"

typedef struct CTX_Node {
    ModuleContext ctx;
    struct CTX_Node* next;
} CTX_Node;

static CTX_Node* ctx_head = NULL;

int add_module(ModuleContext* ctx) {
    CTX_Node* node = malloc(sizeof(CTX_Node));

    if (node == NULL)
        return 0;

    node->ctx = *ctx;
    node->next = ctx_head;
    ctx_head = node;
    return 1;
}

ModuleContext *get_module_from_id(uint16_t id) {
    CTX_Node* current = ctx_head;

    while (current != NULL) {
        ModuleContext* ctx = &current->ctx;

        if (ctx->module_id == id) {
            return ctx;
        }

        current = current->next;
    }

    return NULL;
}