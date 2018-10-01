#include "cozinha.h"
#include "tarefas.h"
#include "pedido.h"
#include <semaphore.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>

// Semáfaros para todos os recursos que possuem número limitado
sem_t balcao;
sem_t bocas;
sem_t frigideiras;
sem_t cozinheiros;
sem_t garcons;
pthread_t* buffer_pedidos; // Buffer circular de threads representando os pedidos
int inicio_pedidos, fim_pedidos; // Indice do primeiro e ultimo pedido atualmente no buffer circular
pthread_mutex_t mtxIndice; // Mutex para garantir consistência na alteração dos indíces globais acima
int gnum_cozinheiros; // Global contendo o número de cozinheiros para ser utilizada como tamanho do buffer

void cozinha_init(int num_cozinheiros, int num_bocas, int num_frigideiras, int num_garcons, int tam_balcao){

	inicio_pedidos = 0;
	fim_pedidos = 0;

	buffer_pedidos = malloc(sizeof(pthread_t) * num_cozinheiros); // Incializa o buffer com tamanho num_cozinheiros

	pthread_mutex_init(&mtxIndice, NULL);
	sem_init(&balcao, 0, tam_balcao);
	sem_init(&bocas, 0, num_bocas);
	sem_init(&frigideiras, 0, num_frigideiras);
	sem_init(&cozinheiros, 0, gnum_cozinheiros = num_cozinheiros);
	sem_init(&garcons, 0, num_garcons);

}

void cozinha_destroy(){

	// Cria versão local do valor inicio_pedidos para não ocorrer data race
	// Ñão é necessário para fim_pedidos, pois ele não é modificado ou acessado pelas threads
	pthread_mutex_lock(&mtxIndice);
	int inicio = inicio_pedidos; 
	pthread_mutex_unlock(&mtxIndice);

	// Aguarda a conclusão de todas as threads, para garantir que o programa
	// não encerre enquanto alguma thread ainda está em execução
	for(int i = inicio; i < fim_pedidos; i++){
		pthread_join(buffer_pedidos[i], NULL);
	}

	free(buffer_pedidos);

	pthread_mutex_destroy(&mtxIndice);
	sem_destroy(&balcao);
	sem_destroy(&bocas);
	sem_destroy(&frigideiras);
	sem_destroy(&cozinheiros);
	sem_destroy(&garcons);

}

// Função utilizada em toda receita para finalizar um prato
void finalizar_prato(prato_t* prato){
	sem_wait(&balcao); // Aguarda espaço no balcão
	notificar_prato_no_balcao(prato);
	sem_post(&cozinheiros); // Adquiriu um espaço para o prato no balcão, cozinheiro pode ser liberado
	sem_wait(&garcons); // Espera um garçom
	sem_post(&balcao); // Garçom tirou o prato do balcão, liberando espaço
	entregar_pedido(prato);
	sem_post(&garcons); // Pedido entregue, garçom disponível
	pthread_mutex_lock(&mtxIndice);
	inicio_pedidos = (inicio_pedidos+1)%gnum_cozinheiros; // Pedido finalizado, incrementa o indíce que marca o primeiro pedido do buffer
	pthread_mutex_unlock(&mtxIndice);
}

typedef enum {
	ESQUENTAR_MOLHO,
	DOURAR_BACON,
	FERVER_AGUA
} atividade_t;

// Estrutura auxiliar para guardar argumentos e tipo de atividades
// para a função shell. Utilizada para atividades paralelas.
typedef struct {
	pthread_t thread; // thread em que a atividade será executada
	atividade_t atividade;
	void* argumento; // Pode ser utilizado como molho_t*, agua_t* ou bacon_t*
} atividade;

// Encapsula as funções de tarefas.h para poderem ser utilizadas
// como função de thread em pthread_create
void *shell(void *arg){

	atividade temp = *((atividade *) arg);

	sem_wait(&bocas); // Todas as atividades utilizam uma boca

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
// Ordem de execução: Cortar -> Temperar -> Grelhar

void *processar_carne(void * arg){

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
	pthread_detach(pthread_self()); // Devolve os recursos do sistema, pois apenas as threads presentes no buffer
									// na destrução da cozinha são "joinadas". Chamado ao final de toda receita.
	pthread_exit(NULL);
}

/*******SPAGHETTI********/
// Ordem de execução: Esquentar molho | Ferver água | Dourar Bacon -> 
//                    (Aguardar água ferver) -> Cozinhar spaghetti

void *processar_spaghetti(void * arg){

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
	pthread_detach(pthread_self());
	pthread_exit(NULL);
}

/*******SOPA********/
// Ordem de execução: Por água para ferver -> Cortar legumes -> Preparar caldo -> Cozinhar legumes

void *processar_sopa(void * arg){

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
	pthread_detach(pthread_self());
	pthread_exit(NULL);
}

void processar_pedido(pedido_t p){

	// Guarda p em um novo ponteiro, para não passar um potencial dangling 
	// pointer para as funções auxiliares
	pedido_t* pedido = (pedido_t*) malloc(sizeof(pedido_t));
	*pedido = p;

	// Aloca um cozinheiro para o pedido
	// Cozinheiro é devolvido ao final de cada pedido, em suas próprias funções
	sem_wait(&cozinheiros);

	// Decodifica o pedido e inicia uma thread para sua execução
	switch(p.prato){
		case PEDIDO_SPAGHETTI:
			pthread_create(&buffer_pedidos[fim_pedidos], NULL, processar_spaghetti, (void *) pedido); 
			printf("Pedido (SPAGHETTI) submetido!\n");
			break;
 
		case PEDIDO_SOPA:
			pthread_create(&buffer_pedidos[fim_pedidos], NULL, processar_sopa, (void *) pedido);
			printf("Pedido (SOPA) submetido!\n");
			break;

		case PEDIDO_CARNE:
			pthread_create(&buffer_pedidos[fim_pedidos], NULL, processar_carne, (void *) pedido);
			printf("Pedido (CARNE) submetido!\n");
			break;

		default:
			printf("Prato inválido!");
			sem_post(&cozinheiros);
			break;

	}

	fim_pedidos = (fim_pedidos + 1) % gnum_cozinheiros; // Não é necesário mutex, pois apenas a thread principal
														// altera fim_pedidos. A única outra função que utiliza 
														// fim_pedidos é cozinha_destroy(), que só será chamada
														// após todas as chamadas de processar_pedido().
}