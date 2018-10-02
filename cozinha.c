#include "cozinha.h"
#include "tarefas.h"
#include "pedido.h"
#include <semaphore.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>

// Semáfaros para todos os recursos que possuem número limitado
sem_t balcao, bocas, frigideiras, cozinheiros, garcons;
pthread_t *buffer_pedidos, *buffer_garcons; // Buffer circular de threads representando os pedidos
char *possui_pedido;  // possui_pedido[i] = 1, se buffer_pedidos[i] possui uma thread ainda em execução. 0 caso contrário.
pthread_mutex_t mtx, mtx_garcon; // Mutex para garantir consistência na alteração de possui_pedido
int gnum_cozinheiros, gnum_garcons, g_index; // Global contendo o número de cozinheiros e garcons para ser utilizada como tamanho dos buffers

void cozinha_init(int num_cozinheiros, int num_bocas, int num_frigideiras, int num_garcons, int tam_balcao){

	buffer_pedidos = malloc(sizeof(pthread_t) * num_cozinheiros); // Incializa o buffer com tamanho num_cozinheiros
	buffer_garcons = malloc(sizeof(pthread_t) * num_garcons);
	possui_pedido = calloc(num_cozinheiros, sizeof(char)*num_cozinheiros); // Inicializa e preenche com 0 o vetor auxiliar
	g_index = 0;

	pthread_mutex_init(&mtx, NULL);
	pthread_mutex_init(&mtx_garcon, NULL);
	sem_init(&balcao, 0, tam_balcao);
	sem_init(&bocas, 0, num_bocas);
	sem_init(&frigideiras, 0, num_frigideiras);
	sem_init(&cozinheiros, 0, gnum_cozinheiros = num_cozinheiros);
	sem_init(&garcons, 0, gnum_garcons = num_garcons);

}

void cozinha_destroy(){

	// Aguarda a conclusão de todas as threads, para garantir que o programa
	// não encerre enquanto alguma thread ainda está em execução

	for(int i = 0; i < gnum_cozinheiros; i++){
		pthread_mutex_lock(&mtx);
		if(possui_pedido[i] == 1){
			pthread_mutex_unlock(&mtx);
			pthread_join(buffer_pedidos[i], NULL);
		}
		pthread_mutex_unlock(&mtx);
	}

	for(int i = 0; i < gnum_garcons; i++){
		pthread_join(buffer_garcons[i], NULL);
	}

	free(buffer_pedidos);

	pthread_mutex_destroy(&mtx);
	sem_destroy(&balcao);
	sem_destroy(&bocas);
	sem_destroy(&frigideiras);
	sem_destroy(&cozinheiros);
	sem_destroy(&garcons);

}

void* worker_garcons(void* arg){
	prato_t* p = (prato_t *) arg;
	entregar_pedido(p);
	sem_post(&garcons);
	pthread_exit(NULL);
}
// Função utilizada em toda receita para finalizar um prato
void finalizar_prato(pedido_t* pedido, prato_t* prato){
	sem_wait(&balcao); // Aguarda espaço no balcão
	notificar_prato_no_balcao(prato);
	pthread_mutex_lock(&mtx);
	possui_pedido[pedido->index] = 0;
	pthread_mutex_unlock(&mtx);
	sem_post(&cozinheiros); // Adquiriu um espaço para o prato no balcão, cozinheiro pode ser liberado
	sem_wait(&garcons); // Espera um garçom
	sem_post(&balcao); // Garçom tirou o prato do balcão, liberando espaço
	pthread_create(&buffer_garcons[g_index], NULL, worker_garcons, (void *) prato);
	pthread_mutex_lock(&mtx_garcon);
	g_index = (g_index+1) % gnum_garcons;
	pthread_mutex_unlock(&mtx_garcon);
	free(pedido);
	pthread_detach(pthread_self());
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

	finalizar_prato(arg, prato);
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

	finalizar_prato(arg, prato);
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

	free(ferver_agua);
	finalizar_prato(arg, prato);
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

	for(int i = 0; i < gnum_cozinheiros; i++){
		pthread_mutex_lock(&mtx);
		if(possui_pedido[i] == 0){
			pthread_mutex_unlock(&mtx);
			pedido->index = i;
			break;
		}
		pthread_mutex_unlock(&mtx);
	}

	possui_pedido[pedido->index] = 1; // Não é necessário mutex aqui, pois acima foi checado que possui_pedido[index] possui 0,
									  // o que significa que nenhuma thread está executando em buffer_pedidos[index], e threads
									  // apenas alteram possui_pedidos no mesmo index em que se encontram em buffer_pedidos.

									  // A outra utilização de possui_pedido é em cozinha_destroy(), que só é chamado pela própria
									  // thread principal, após todos os pedidos terem passado por processar_pedido();

	// Decodifica o pedido e inicia uma thread para sua execução
	switch(p.prato){
		case PEDIDO_SPAGHETTI:
			pthread_create(&buffer_pedidos[pedido->index], NULL, processar_spaghetti, (void *) pedido); 
			printf("Pedido (SPAGHETTI) submetido!\n");
			break;
 
		case PEDIDO_SOPA:
			pthread_create(&buffer_pedidos[pedido->index], NULL, processar_sopa, (void *) pedido);
			printf("Pedido (SOPA) submetido!\n");
			break;

		case PEDIDO_CARNE:
			pthread_create(&buffer_pedidos[pedido->index], NULL, processar_carne, (void *) pedido);
			printf("Pedido (CARNE) submetido!\n");
			break;

		default:
			printf("Prato inválido!");
			sem_post(&cozinheiros);
			break;

	}


}