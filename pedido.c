#include "pedido.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

const char* pedido_prato_names[] = {
    "<PEDIDO NULO>",
    "SPAGHETTI",
    "SOPA",
    "CARNE"
};

pedido_prato_t pedido_prato_from_name(const char* name) {
    for (int i = 0; i < PEDIDO__SIZE; ++i) {
        if (strcmp(name, pedido_prato_names[i]) == 0)
            return (pedido_prato_t)i;
    }
    return PEDIDO_NULL;
}

const char* pedido_prato_to_name(pedido_prato_t pedido) {
    if (pedido >= PEDIDO__SIZE) {
        fprintf(stderr, "pedido_prato_t %d é inválido!\n", pedido);
        abort();
    }
    return pedido_prato_names[pedido];
}
