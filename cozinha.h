#ifndef __COZINHA_H__
#define __COZINHA_H__

#include "pedido.h"

extern void cozinha_init(int num_cozinheiros, int num_bocas, int num_frigideiras, int num_garcons, int tam_balcao);
extern void cozinha_destroy();
extern void processar_pedido(pedido_t p);

#endif /*__COZINHA_H__*/
