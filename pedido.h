#ifndef __PEDIDO_H__
#define __PEDIDO_H__

typedef enum {
    PEDIDO_NULL,
    PEDIDO_SPAGHETTI,
    PEDIDO_SOPA,
    PEDIDO_CARNE,
    PEDIDO__SIZE
} pedido_prato_t;

typedef struct {
    unsigned id;
    pedido_prato_t prato;
} pedido_t;

extern pedido_prato_t pedido_prato_from_name(const char* name);
extern const char* pedido_prato_to_name(pedido_prato_t pedido);

#endif /*__PEDIDO_H__*/
