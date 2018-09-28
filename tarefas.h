#ifndef __TAREFAS_H__
#define __TAREFAS_H__

#include "pedido.h"

/* --- --- insumos usados em várias receitas --- --- */

typedef struct {
    unsigned id;
    char fervida;
} agua_t;

/* Nesse restaurante a água não é pega da torneira, ela é criada.
   Retorna um ponteiro para um objeto agua_t alocado com malloc(). 
   Use destroy_agua(). Quando não precisar mais da água. */
extern agua_t* create_agua();
/** Ferve a água. Bloqueante, só retorna quando a água estiver fervida  */
extern void ferver_agua(agua_t* agua);
/* Libera o objeto agua_t que foi criado anteriormente com create_agua() */
extern void destroy_agua(agua_t* agua);

typedef struct {
    unsigned id;
    pedido_t pedido;
    void** coisas;
    unsigned coisas_len;
} prato_t;

/** Cria um prato onde receitas podem ser empratadas. 
    Cada receita tem um método empratar_* correspondente */
extern prato_t* create_prato(pedido_t pedido);

/* Garçom leva pedido para quem pediu. Chama destroy_prato()
   IMPORTANTE: Essa função deve ser chamada para todo prato! */
extern void entregar_pedido(prato_t* prato);
/* Notifica que pedido está no balcão 
   IMPORTANTE: Essa função deve ser chamada para todo prato! */
extern void notificar_prato_no_balcao(prato_t* prato);

/** Limpa um prato. Não se preocupe, o prato será reciclado.
    Destruir um prato, tmbém destroi tudo que foi empratado nele */
extern void destroy_prato(prato_t* prato);




/* --- --- insumos para receita de spaghetti --- --- */
/* ~ operações são equivalentes as operações em agua_t */

typedef struct {
    unsigned id;
    char quente;
} molho_t;

extern molho_t* create_molho();
extern void esquentar_molho(molho_t* molho);
extern void destroy_molho(molho_t* molho);

typedef struct {
    unsigned id;
    char dourado;
} bacon_t;

extern bacon_t* create_bacon();
extern void dourar_bacon(bacon_t* bacon);
extern void destroy_bacon(bacon_t* bacon);

typedef struct {
    unsigned id;
    char cozido;
} spaghetti_t;

extern spaghetti_t* create_spaghetti();
extern void cozinhar_spaghetti(spaghetti_t* spaghetti, agua_t* agua);
extern void destroy_spaghetti(spaghetti_t* spaghetti);

extern void empratar_spaghetti(spaghetti_t* spaghetti, molho_t* molho, 
                               bacon_t* bacon, prato_t* prato);


/* --- --- insumos para receita de sopa --- --- */
typedef struct {
    unsigned id;
} caldo_t;

/** Cria caldo a partir de água fervente. A água deixa de existir como água
    (destroy_agua(agua_ferv) não pode ser chamado posteriormente). */
extern caldo_t* preparar_caldo(agua_t* agua_ferv);  //1min
extern void destroy_caldo(caldo_t* caldo);

typedef struct {
    unsigned id;
    char fatiados;
    char cozidos;
} legumes_t;

extern legumes_t* create_legumes();
extern void cortar_legumes(legumes_t* legumes);
extern void cozinhar_legumes(legumes_t* legumes, caldo_t* caldo);
extern void destroy_legumes(legumes_t* legumes);

extern void empratar_sopa(legumes_t* legumes, caldo_t* caldo, prato_t* prato);


/* --- --- insumos para receita de carne grelhada --- --- */

typedef struct {
    unsigned id;
    char fatiada;
    char temperada;
    char grelhada;
} carne_t;
extern carne_t* create_carne();
extern void cortar_carne(carne_t* carne);
extern void temperar_carne(carne_t* carne);
extern void grelhar_carne(carne_t* carne);
extern void destroy_carne(carne_t* carne);

extern void empratar_carne(carne_t* carne, prato_t* prato);

#endif /*#ifndef __TAREFAS_H__*/
