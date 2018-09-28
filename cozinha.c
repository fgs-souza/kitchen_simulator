#include "cozinha.h"
#include "tarefas.h"
#include "pedido.h"
#include <semaphore.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>

sem_t balcao;
sem_t bocas;
sem_t frigideiras;
sem_t cozinheiros;
sem_t garcons;
pthread_t* buffer_pedidos;
int pedidos;
int gnum_cozinheiros;

void cozinha_init(int num_cozinheiros, int num_bocas, int num_frigideiras, int num_garcons, int tam_balcao){

	pedidos = 0;

	buffer_pedidos = malloc(sizeof(pthread_t) * num_cozinheiros);

	sem_init(&balcao, 0, tam_balcao);
	sem_init(&bocas, 0, num_bocas);
	sem_init(&frigideiras, 0, num_frigideiras);
	sem_init(&cozinheiros, 0, gnum_cozinheiros = num_cozinheiros);
	sem_init(&garcons, 0, num_garcons);

}

void cozinha_destroy(){

	for(int i = 0; i < pedidos; i++)
		pthread_join(buffer_pedidos[i], NULL);

	free(buffer_pedidos);

	sem_destroy(&balcao);
	sem_destroy(&bocas);
	sem_destroy(&frigideiras);
	sem_destroy(&cozinheiros);
	sem_destroy(&garcons);

}

void finalizar_prato(prato_t* prato){


	sem_wait(&balcao); // Aguarda espaço no balcão
	notificar_prato_no_balcao(prato);
	sem_post(&cozinheiros); // Adquiriu um espaço para o prato no balcão, cozinheiro pode ser liberado
	sem_wait(&garcons); // Espera um garçom
	sem_post(&balcao); // Garçom tirou o prato do balcão, liberando espaço
	entregar_pedido(prato);
	sem_post(&garcons); // Pedido entregue, garçom disponível
}

typedef enum {
	ESQUENTAR_MOLHO,
	DOURAR_BACON,
	FERVER_AGUA
} atividade_t;

// Estrutura auxiliar para guardar argumentos e tipo de atividades
typedef struct {
	pthread_t thread;
	atividade_t atividade;
	void* argumento;
} atividade;

// Encapsula as funções de tarefas.h para poderem ser utilizadas
// como função de thread em pthread_create
void *shell(void *arg){

	atividade temp = *((atividade *) arg);

	sem_wait(&bocas);

	switch(temp.atividade){
		case ESQUENTAR_MOLHO:
			esquentar_molho((molho_t *) temp.argumento);
			break;
 
		case DOURAR_BACON:
			sem_wait(&frigideiras);
			dourar_bacon((bacon_t *) temp.argumento);
			sem_post(&frigideiras);
			break;

		case FERVER_AGUA:
			ferver_agua((agua_t *) temp.argumento);
			break;
	}

	sem_post(&bocas);
	pthread_exit(NULL);

}

/*******CARNE********/

void *processar_carne(void * arg){

	printf("Pedido (CARNE) iniciando!\n");

	pedido_t p = *((pedido_t *) arg);

	carne_t* carne = create_carne();
	prato_t* prato = create_prato(p);

	cortar_carne(carne);
	temperar_carne(carne);

	sem_wait(&bocas);
	sem_wait(&frigideiras);
	grelhar_carne(carne);
	sem_post(&bocas);
	sem_post(&frigideiras);

	empratar_carne(carne, prato);

	finalizar_prato(prato);
	free(arg);
	pthread_exit(NULL);
}

/*******SPAGHETTI********/

void *processar_spaghetti(void * arg){

	printf("Pedido (SPAGHETTI) iniciando!\n");

	pedido_t p = *((pedido_t *) arg);

	molho_t* molho = create_molho();
	agua_t* agua = create_agua();
	bacon_t* bacon = create_bacon();
	spaghetti_t* spaghetti = create_spaghetti();
	prato_t* prato = create_prato(p);

	atividade atividades[3];

	atividades[0].atividade = ESQUENTAR_MOLHO;
	atividades[1].atividade = FERVER_AGUA;
	atividades[2].atividade = DOURAR_BACON;

	atividades[0].argumento = (void*) molho;
	atividades[1].argumento = (void*) agua;
	atividades[2].argumento = (void*) bacon;

	// Executa as três atividades em paralelo
	pthread_create(&atividades[0].thread, NULL, shell, (void*) &atividades[0]); // Esquenta molho
	pthread_create(&atividades[1].thread, NULL, shell, (void*) &atividades[1]); // Ferve água
	pthread_create(&atividades[2].thread, NULL, shell, (void*) &atividades[2]); // Doura bacon

	pthread_join(atividades[1].thread, NULL); // Aguarda o fervimento da água para cozinhar spaghetti

	sem_wait(&bocas);
	cozinhar_spaghetti(spaghetti, agua);
	sem_post(&bocas);

	pthread_join(atividades[0].thread, NULL);
	pthread_join(atividades[2].thread, NULL);

	free(agua);
	empratar_spaghetti(spaghetti, molho, bacon, prato);

	finalizar_prato(prato);
	free(arg);
	pthread_exit(NULL);
}

/*******SOPA********/

void *processar_sopa(void * arg){

	printf("Pedido (SOPA) iniciando!\n");

	pedido_t p = *((pedido_t *) arg);

	legumes_t* legumes = create_legumes();
	agua_t* agua = create_agua();
	prato_t* prato = create_prato(p);

	atividade* ferver_agua = (atividade*) malloc(sizeof(atividade));
	ferver_agua->atividade = FERVER_AGUA;
	ferver_agua->argumento = (void*) agua;

	pthread_create(&ferver_agua->thread, NULL, shell, (void*) ferver_agua);

	cortar_legumes(legumes);

	pthread_join(ferver_agua->thread, NULL); // Aguarda o fervimento da água para cozinhar caldo

	sem_wait(&bocas);
	caldo_t* caldo = preparar_caldo((agua_t*) ferver_agua->argumento);
	sem_post(&bocas);

	sem_wait(&bocas);
	cozinhar_legumes(legumes, caldo);
	sem_post(&bocas);

	empratar_sopa(legumes, caldo, prato);

	finalizar_prato(prato);
	free(arg);
	free(ferver_agua);
	pthread_exit(NULL);
}

void processar_pedido(pedido_t p){

	pedido_t* pedido = (pedido_t*) malloc(sizeof(pedido_t));
	*pedido = p;

	sem_wait(&cozinheiros);

	switch(p.prato){
		case PEDIDO_SPAGHETTI:
			pthread_create(&buffer_pedidos[pedidos], NULL, processar_spaghetti, (void *) pedido); 
			printf("Pedido (SPAGHETTI) submetido!\n");
			break;
 
		case PEDIDO_SOPA:
			pthread_create(&buffer_pedidos[pedidos], NULL, processar_sopa, (void *) pedido);
			printf("Pedido (SOPA) submetido!\n");
			break;

		case PEDIDO_CARNE:
			pthread_create(&buffer_pedidos[pedidos], NULL, processar_carne, (void *) pedido);
			printf("Pedido (CARNE) submetido!\n");
			break;

		default:
			printf("Prato inválido!");
			sem_post(&cozinheiros);
			break;

	}

	pedidos = (pedidos + 1) % gnum_cozinheiros;

}