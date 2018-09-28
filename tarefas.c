#include "tarefas.h"
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <stdatomic.h>
#include <time.h>

static void simulation_sleep(unsigned minutes) {
    // Cada minuto do mundo simulado simulação equivale a 200 ms no mundo real
    unsigned ms = 200*minutes;
    struct timespec ts = {ms/1000, (ms % 1000)*1000000L};
    nanosleep(&ts, NULL);
}

/** Exercício 1 de mutex sem mutex, entender isso não é parte do
    trabalho.  

    Se continuou lendo, saiba que algumas operações [1] podem ser
    feitas nesse contador de maneira atômica sem que seja usado um
    mutex. Uma dessas operações é fetch_add, que é equivalente à um
    g_id++ (retorna o valor antigo e incrementa). Há diversas outras
    operações, como compare_exchange (também chamada de TSL --
    test-and-set-lock) em alguns lugares. Essa operações atômicas tem
    garantia do processador para funcionar e são usadas na
    implementação de mutexes e semáforos (embora normalmente se     use
    builtins para instruções assembly, por razões históricas).

    *NÃO TENTE USAR ATOMICS PARA IMPLEMENTAR O T1!* Você vai acabar
    implementando semáforos e mutexes piores que os da pthread (ou
    bugados) ao invés de usar os prontos.

   [1]: https://en.cppreference.com/w/c/atomic
*/
static atomic_uint_fast64_t g_id;

//Detecta alguns erros de memória, como uso após o destroy() ou buffer 
//overflows que afetaram esse objeto
#define CHECK_PTR(type, p)                                           \
    if (p->id & 0xf0000000) {                                        \
        fprintf(stderr, "%s %p acessado em %s:%d, função %s, está "  \
                        "corrompido ou já foi destruído.",           \
                #type, p,  __FILE__, __LINE__, __func__);            \
    }

// Define funções create_* e destroy_*
#define CREATE_F(type)                                                     \
    type##_t* create_##type() {                                            \
        type##_t* p = calloc(1, sizeof(type##_t));                         \
        p->id = atomic_fetch_add_explicit(&g_id, 1, memory_order_relaxed); \
        return p;                                                          \
    } 
#define DESTROY_F(type)                \
    void destroy_##type(type##_t* p) { \
        CHECK_PTR(type##_t, p);        \
        p->id |= 0xf0000000;           \
        free(p);                       \
    }
#define LIFECYCLE_FS(type) CREATE_F(type) DESTROY_F(type)

LIFECYCLE_FS(agua);
LIFECYCLE_FS(molho);
LIFECYCLE_FS(spaghetti);
LIFECYCLE_FS(bacon);
LIFECYCLE_FS(legumes);
LIFECYCLE_FS(carne);

prato_t* create_prato(pedido_t pedido) {
    assert(pedido.prato && pedido.prato < PEDIDO__SIZE);
    prato_t* p = calloc(1, sizeof(prato_t));
    p->id = atomic_fetch_add_explicit(&g_id, 1, memory_order_relaxed); 
    p->pedido = pedido;
    return p;
} 
void entregar_pedido(prato_t* p) {
    simulation_sleep(3);
    printf("Pedido %d (%s) entregue!\n", 
           p->pedido.id, pedido_prato_to_name(p->pedido.prato));
    destroy_prato(p);
}
void notificar_prato_no_balcao(prato_t* prato) {
    /* não faço nada, mas o script corretor coloca código aqui para 
       corrigir o trabalho. */
}
void destroy_prato(prato_t* p) {
    CHECK_PTR(prato_t, p);
    p->id |= 0xf0000000;
    for (int i = 0; i < p->coisas_len; ++i) {
        *((unsigned*)p->coisas[i]) |= 0xf0000000;
        free(p->coisas[i]);
    }
    free(p->coisas);
    free(p);
}

caldo_t* preparar_caldo(agua_t* agua_ferv) {
    CHECK_PTR(agua_t, agua_ferv);
    simulation_sleep(1);  
    caldo_t* p = calloc(sizeof(caldo_t), 1);
    p->id = atomic_fetch_add_explicit(&g_id, 1, memory_order_relaxed);
    destroy_agua(agua_ferv);
    return p;
}
DESTROY_F(caldo)

#define TAREFA(type, tarefa, flag, mins) \
    void tarefa##_##type(type##_t* i) {  \
        CHECK_PTR(type##_t, i);          \
        simulation_sleep(mins);          \
        i->flag = 1;                     \
    }
#define TAREFA2(type1, type2, tarefa, flag, mins) \
    void tarefa##_##type1(type1##_t* i1, type2##_t* i2) { \
        CHECK_PTR(type1##_t, i1);               \
        CHECK_PTR(type2##_t, i2);               \
        simulation_sleep(mins);                 \
        i1->flag = 1;                           \
    }

TAREFA(agua,    ferver,     fervida,    3)
TAREFA(molho,   esquentar,  quente,     3)
TAREFA(bacon,   dourar,     dourado,    2)
TAREFA(legumes, cortar,     fatiados,   10)
TAREFA(carne,   cortar,     fatiada,    5)
TAREFA(carne,   temperar,   temperada,  3)
TAREFA(carne,   grelhar,    grelhada,   3)

TAREFA2(spaghetti, agua,  cozinhar, cozido,  5)
TAREFA2(legumes,   caldo, cozinhar, cozidos, 8)

void empratar_spaghetti(spaghetti_t* spaghetti, molho_t* molho,  
                        bacon_t* bacon, prato_t* prato) {
    CHECK_PTR(spaghetti_t, spaghetti);
    CHECK_PTR(molho_t, molho);
    CHECK_PTR(bacon_t, bacon);
    CHECK_PTR(prato_t, prato);
    simulation_sleep(3);
    prato->coisas = calloc(prato->coisas_len = 3, sizeof(void*));
    prato->coisas[0] = spaghetti;
    prato->coisas[1] = molho;
    prato->coisas[2] = bacon;
}

void empratar_sopa(legumes_t* legumes, caldo_t* caldo, prato_t* prato) {
    CHECK_PTR(legumes_t, legumes);
    CHECK_PTR(caldo_t, caldo);
    CHECK_PTR(prato_t, prato);
    simulation_sleep(1);
    prato->coisas = calloc(prato->coisas_len = 2, sizeof(void*));
    prato->coisas[0] = legumes;
    prato->coisas[1] = caldo;
}

void empratar_carne(carne_t* carne, prato_t* prato) {
    CHECK_PTR(carne_t, carne);
    CHECK_PTR(prato_t, prato);
    simulation_sleep(1);
    prato->coisas = calloc(prato->coisas_len = 1, sizeof(void*));
    prato->coisas[0] = carne;
}

