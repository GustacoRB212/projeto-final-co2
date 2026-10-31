/* =============================================================================
 *  ECOTECH MONITOR CO2  -  ESTEIRA DE DECISAO LIFO / FIFO
 *  ---------------------------------------------------------------------------
 *  Projeto final - Algoritmo e Estrutura de Dados (Inovatech / A2)
 *  Tema fisico: concentracao de CO2 (ppm) em ambiente agricola protegido.
 *
 *  Fluxo da planta de execucao:
 *
 *    [SENSOR] -> [FILA FIFO] -> [CLASSIFICADOR] -> [PILHA LIFO] -> [ATUADOR]
 *                 chegada        regra de           rollback       exaustor
 *                 temporal       negocio            de comando     (0/50/100%)
 *
 *  Estruturas:
 *    - Fila  (FIFO) : leituras processadas na ordem exata de chegada. O(1).
 *    - Pilha (LIFO) : comandos do atuador; o ultimo comando e o primeiro a
 *                     ser revertido (rollback de leitura defeituosa). O(1).
 *    - Arvore (ABB): indice nao linear por ppm; consulta "quantos acima do
 *                     limiar" em O(h), h = altura (log n no caso medio).
 *
 *  Memoria: toda alocacao passa por alocar() e toda liberacao por liberar().
 *  Ao final o programa compara os dois contadores: saldo zero = sem memory leak.
 *
 *  Compilar:  gcc -std=c11 -Wall -Wextra -Wpedantic -O2 src/main.c -o build/ecotech
 *  Executar:  ./build/ecotech
 * ========================================================================== */

#define _POSIX_C_SOURCE 199309L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ------------------------------- CONSTANTES ------------------------------- */

#define TAM_SETOR                 48
#define LIMIAR_ATENCAO_PADRAO    800.0f
#define LIMIAR_CRITICO_PADRAO   1000.0f
#define LEITURAS_PADRAO             24UL
#define LEITURAS_MAXIMAS          5000UL
#define PROFUNDIDADE_MAX_ARVORE      64
#define SEMENTE_PADRAO            2026U

/* --------------------- CONTABILIDADE DE MEMORIA (AUDITORIA) --------------- */

static size_t g_alocacoes = 0;   /* quantas vezes malloc() foi chamado  */
static size_t g_liberacoes = 0;  /* quantas vezes free()  foi chamado  */

/* Aloca e contabiliza. Falha de malloc e tratada como erro fatal (fail-fast):
 * um no de fila sem memoria tornaria a leitura do sensor inconsistente.       */
static void *alocar(size_t bytes) {
    void *bloco = malloc(bytes);
    if (bloco == NULL) {
        fprintf(stderr, "[FATAL] Falha de alocacao (%zu bytes). Encerrando.\n", bytes);
        exit(EXIT_FAILURE);
    }
    g_alocacoes++;
    return bloco;
}

/* Libera de forma segura (aceita NULL) e contabiliza. */
static void liberar(void *bloco) {
    if (bloco != NULL) {
        free(bloco);
        g_liberacoes++;
    }
}

/* --------------------------------- MODELO --------------------------------- */

typedef enum {
    NIVEL_NORMAL  = 0,
    NIVEL_ATENCAO = 1,
    NIVEL_CRITICO = 2
} Nivel;

/* Struct central do dominio: um unico bloco de memoria descreve a leitura.
 * Manter os campos juntos garante que a fila, a pilha e a arvore trafeguem
 * o dado por valor, sem ponteiros soltos para o buffer do sensor.             */
typedef struct {
    unsigned long id;             /* identificador sequencial da leitura     */
    char          setor[TAM_SETOR]; /* nome do setor agricola (texto fixo)   */
    float         ppm;            /* concentracao medida de CO2              */
    long          epoch;          /* instante da leitura (segundos)          */
    Nivel         nivel;          /* classificacao derivada, calculada 1 vez */
} Leitura;

/* Comando enviado ao atuador (exaustor). Guardamos o estado anterior para
 * permitir o rollback exato quando o comando precisa ser desfeito.           */
typedef struct {
    unsigned long id_leitura;
    char          setor[TAM_SETOR];
    float         ppm;
    Nivel         nivel;
    int           duty_anterior;  /* % do exaustor antes do comando          */
    int           duty_novo;      /* % do exaustor depois do comando         */
    long          epoch;
} Comando;

/* Prototipos usados antes da definicao (mantem o arquivo legivel). */
static const char *rotulo_nivel(Nivel nivel);
static void formatar_hora(long epoch, char *buffer, size_t tamanho);

/* ---------------------------------- FILA ---------------------------------- */

typedef struct NoFila {
    Leitura        dados;
    struct NoFila *proximo;
} NoFila;

typedef struct {
    NoFila *inicio;   /* primeiro a sair (front) */
    NoFila *fim;      /* ultimo a entrar (rear)  */
    size_t  tamanho;
} Fila;

static void fila_iniciar(Fila *f) {
    f->inicio  = NULL;
    f->fim     = NULL;
    f->tamanho = 0;
}

/* Enfileirar: O(1). Encadeia no fim usando o ponteiro de cauda. */
static int fila_enfileirar(Fila *f, Leitura valor) {
    NoFila *novo = (NoFila *)alocar(sizeof *novo);   /* sizeof do tipo, nao do ponteiro */
    if (novo == NULL) {
        return 0;
    }
    novo->dados   = valor;
    novo->proximo = NULL;

    if (f->fim != NULL) {
        f->fim->proximo = novo;
    } else {
        f->inicio = novo;
    }
    f->fim = novo;
    f->tamanho++;
    return 1;
}

/* Desenfileirar: O(1). Remove o primeiro e devolve o conteudo por copia. */
static int fila_desenfileirar(Fila *f, Leitura *destino) {
    NoFila *alvo = f->inicio;
    if (alvo == NULL) {
        return 0;                     /* fila vazia: evita underflow */
    }
    f->inicio = alvo->proximo;
    if (f->inicio == NULL) {
        f->fim = NULL;
    }
    *destino = alvo->dados;           /* copia antes de liberar o no */
    liberar(alvo);
    f->tamanho--;
    return 1;
}

/* Destroi a fila inteira: nenhum no sobrevive, logo sem memory leak. */
static void fila_destruir(Fila *f) {
    Leitura descarte;
    while (fila_desenfileirar(f, &descarte)) {
        /* corpo vazio: a remocao ja libera o no */
    }
}

/* ---------------------------------- PILHA --------------------------------- */

typedef struct NoPilha {
    Comando         dados;
    struct NoPilha *abaixo;
} NoPilha;

typedef struct {
    NoPilha *topo;
    size_t   tamanho;
} Pilha;

static void pilha_iniciar(Pilha *p) {
    p->topo    = NULL;
    p->tamanho = 0;
}

/* Empilhar: O(1). Insere no topo. */
static int pilha_empilhar(Pilha *p, Comando valor) {
    NoPilha *novo = (NoPilha *)alocar(sizeof *novo);
    if (novo == NULL) {
        return 0;
    }
    novo->dados  = valor;
    novo->abaixo = p->topo;
    p->topo      = novo;
    p->tamanho++;
    return 1;
}

/* Desempilhar: O(1). Remove o topo; devolve 0 se vazia (underflow tratado). */
static int pilha_desempilhar(Pilha *p, Comando *destino) {
    NoPilha *alvo = p->topo;
    if (alvo == NULL) {
        return 0;
    }
    p->topo = alvo->abaixo;
    *destino = alvo->dados;
    liberar(alvo);
    p->tamanho--;
    return 1;
}

/* Apenas espia o topo, sem remover. */
static const Comando *pilha_topo(const Pilha *p) {
    return p->topo != NULL ? &p->topo->dados : NULL;
}

static void pilha_destruir(Pilha *p) {
    Comando descarte;
    while (pilha_desempilhar(p, &descarte)) {
        /* a remocao ja libera o no */
    }
}

/* ----------------------- ARVORE BINARIA DE BUSCA (ABB) -------------------- */

typedef struct NoArvore {
    Leitura           dados;
    struct NoArvore  *esquerda;
    struct NoArvore  *direita;
    size_t            subarvore;   /* tamanho da subarvore enraizada aqui */
} NoArvore;

typedef struct {
    NoArvore *raiz;
    size_t    tamanho;
} Arvore;

static void arvore_iniciar(Arvore *a) {
    a->raiz    = NULL;
    a->tamanho = 0;
}

/* Insercao iterativa: O(h). O limite de profundidade garante que as rotinas
 * recursivas (altura, contagem, destruicao) nunca passem de 64 frames.       */
static int arvore_inserir(Arvore *a, Leitura valor) {
    NoArvore **elo = &a->raiz;
    int profundidade = 1;

    while (*elo != NULL) {
        if (valor.ppm < (*elo)->dados.ppm) {
            elo = &(*elo)->esquerda;
        } else {
            elo = &(*elo)->direita;      /* duplicados vao para a direita */
        }
        profundidade++;
    }

    if (profundidade > PROFUNDIDADE_MAX_ARVORE) {
        return 0;                        /* rejeita e evita arvore degenerada */
    }

    NoArvore *novo = (NoArvore *)alocar(sizeof *novo);
    if (novo == NULL) {
        return 0;
    }
    novo->dados     = valor;
    novo->esquerda  = NULL;
    novo->direita   = NULL;
    novo->subarvore = 1;
    *elo = novo;
    a->tamanho++;

    /* Recalcula os tamanhos de subarvore no caminho raiz -> novo no. */
    NoArvore *atual = a->raiz;
    while (atual != novo) {
        atual->subarvore++;
        atual = (valor.ppm < atual->dados.ppm) ? atual->esquerda : atual->direita;
    }
    return 1;
}

/* Conta quantos ppm sao >= limiar em O(h), sem percorrer a arvore inteira.
 * Se o no atual ja atende, toda a subarvore direita tambem atende.           */
static size_t arvore_contar_acima(const NoArvore *no, float limiar) {
    if (no == NULL) {
        return 0;
    }
    if (no->dados.ppm < limiar) {
        return arvore_contar_acima(no->direita, limiar);
    }
    size_t direita = (no->direita != NULL) ? no->direita->subarvore : 0;
    return 1 + direita + arvore_contar_acima(no->esquerda, limiar);
}

static int arvore_altura(const NoArvore *no) {
    if (no == NULL) {
        return 0;
    }
    int esq = arvore_altura(no->esquerda);
    int dir = arvore_altura(no->direita);
    return 1 + (esq > dir ? esq : dir);
}

/* Caminhamento em ordem ITERATIVO com pilha local de 64 ponteiros.
 * Nao usa recursao e nao aloca heap: 512 bytes fixos de stack.               */
static void arvore_listar_em_ordem(const Arvore *a, size_t limite) {
    const NoArvore *pilha[PROFUNDIDADE_MAX_ARVORE];
    size_t topo = 0;
    size_t impressos = 0;
    const NoArvore *atual = a->raiz;
    char hora[16];

    while ((atual != NULL || topo > 0) && impressos < limite) {
        while (atual != NULL) {
            if (topo >= PROFUNDIDADE_MAX_ARVORE) {
                return;                  /* defensivo: nunca deve ocorrer */
            }
            pilha[topo++] = atual;
            atual = atual->esquerda;
        }
        atual = pilha[--topo];
        formatar_hora(atual->dados.epoch, hora, sizeof hora);
        printf("  id=%04lu  %7.1f ppm  %-8s  %-16s  %s\n",
               atual->dados.id,
               (double)atual->dados.ppm,
               rotulo_nivel(atual->dados.nivel),
               atual->dados.setor,
               hora);
        impressos++;
        atual = atual->direita;
    }
}

/* Destruicao pos-ordem: profundidade maxima 64, portanto segura. */
static void arvore_destruir_rec(NoArvore *no) {
    if (no == NULL) {
        return;
    }
    arvore_destruir_rec(no->esquerda);
    arvore_destruir_rec(no->direita);
    liberar(no);
}

static void arvore_destruir(Arvore *a) {
    arvore_destruir_rec(a->raiz);
    a->raiz    = NULL;
    a->tamanho = 0;
}

/* ----------------------------- REGRAS DE NEGOCIO -------------------------- */

static Nivel classificar(float ppm, float limiar_atencao, float limiar_critico) {
    if (ppm >= limiar_critico) return NIVEL_CRITICO;
    if (ppm >= limiar_atencao) return NIVEL_ATENCAO;
    return NIVEL_NORMAL;
}

static const char *rotulo_nivel(Nivel nivel) {
    switch (nivel) {
        case NIVEL_CRITICO: return "CRITICO";
        case NIVEL_ATENCAO: return "ATENCAO";
        default:            return "NORMAL";
    }
}

/* Metrica de acionamento logico do atuador (exaustor). */
static int calcular_duty(float ppm, float limiar_atencao, float limiar_critico) {
    if (ppm >= limiar_critico) return 100;   /* exaustao maxima              */
    if (ppm >= limiar_atencao) return 50;    /* ventilacao preventiva        */
    return 0;                                /* ambiente em faixa segura     */
}

/* ------------------------------ SENSOR SIMULADO --------------------------- */

/* xorshift32: PRNG deterministico e barato. Semente fixa => demonstracao
 * reproduzivel, o que facilita a arguicao ("rode de novo e mostre").         */
static unsigned proximo_aleatorio(unsigned *estado) {
    unsigned x = *estado;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *estado = x;
    return x;
}

static float simular_ppm(unsigned *estado) {
    unsigned x = proximo_aleatorio(estado);
    float ppm = 480.0f + (float)(x % 420U);          /* faixa base 480..899   */
    if ((x % 100U) < 18U) {
        ppm += (float)(proximo_aleatorio(estado) % 900U); /* 18% de picos     */
    }
    if (ppm < 350.0f)  ppm = 350.0f;
    if (ppm > 2400.0f) ppm = 2400.0f;
    return ppm;
}

/* -------------------------------- UTILIDADES ------------------------------ */

static void formatar_hora(long epoch, char *buffer, size_t tamanho) {
    time_t instante = (time_t)epoch;
    struct tm *info = localtime(&instante);
    if (info == NULL) {
        snprintf(buffer, tamanho, "--:--:--");
        return;
    }
    strftime(buffer, tamanho, "%H:%M:%S", info);
}

static double agora_segundos(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

/* --------------------------------- CONFIG --------------------------------- */

typedef struct {
    const char   *setor;
    unsigned long leituras;
    float         limiar_atencao;
    float         limiar_critico;
    unsigned      semente;
} Config;

static void imprimir_uso(const char *programa) {
    printf("Uso: %s [opcoes]\n", programa);
    printf("  --setor NOME          nome do setor agricola (padrao: Estufa Alpha)\n");
    printf("  --leituras N          quantidade de leituras simuladas (1..%lu)\n", LEITURAS_MAXIMAS);
    printf("  --limiar-atencao PPM  limiar de atencao (padrao: %.0f)\n", (double)LIMIAR_ATENCAO_PADRAO);
    printf("  --limiar-critico PPM  limiar critico  (padrao: %.0f)\n", (double)LIMIAR_CRITICO_PADRAO);
    printf("  --seed N              semente do gerador (padrao: %u)\n", SEMENTE_PADRAO);
    printf("  --benchmark           mede o custo por operacao das estruturas\n");
    printf("  --help                esta ajuda\n");
}

static int ler_argumentos(int argc, char **argv, Config *cfg, int *benchmark) {
    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];

        if (strcmp(arg, "--benchmark") == 0) {
            *benchmark = 1;
        } else if (strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0) {
            imprimir_uso(argv[0]);
            exit(EXIT_SUCCESS);
        } else if (strcmp(arg, "--setor") == 0 && i + 1 < argc) {
            cfg->setor = argv[++i];
        } else if (strcmp(arg, "--leituras") == 0 && i + 1 < argc) {
            char *fim = NULL;
            unsigned long valor = strtoul(argv[++i], &fim, 10);
            if (fim == argv[i] || *fim != '\0' || valor == 0 || valor > LEITURAS_MAXIMAS) {
                fprintf(stderr, "[ERRO] --leituras deve estar entre 1 e %lu.\n", LEITURAS_MAXIMAS);
                return 0;
            }
            cfg->leituras = valor;
        } else if (strcmp(arg, "--limiar-atencao") == 0 && i + 1 < argc) {
            cfg->limiar_atencao = (float)atof(argv[++i]);
        } else if (strcmp(arg, "--limiar-critico") == 0 && i + 1 < argc) {
            cfg->limiar_critico = (float)atof(argv[++i]);
        } else if (strcmp(arg, "--seed") == 0 && i + 1 < argc) {
            cfg->semente = (unsigned)strtoul(argv[++i], NULL, 10);
        } else {
            fprintf(stderr, "[ERRO] Argumento invalido: %s\n", arg);
            imprimir_uso(argv[0]);
            return 0;
        }
    }

    if (cfg->limiar_atencao <= 0.0f || cfg->limiar_critico <= cfg->limiar_atencao) {
        fprintf(stderr, "[ERRO] Exige-se 0 < limiar de atencao < limiar critico.\n");
        return 0;
    }
    return 1;
}

/* -------------------------------- METRICAS -------------------------------- */

typedef struct {
    unsigned long leituras;
    unsigned long normais;
    unsigned long atencoes;
    unsigned long criticos;
    float         ppm_min;
    float         ppm_max;
    double        soma;
    int           duty;             /* estado atual do exaustor (%)          */
    unsigned long acionamentos;
    unsigned long rejeitadas_arvore;
} Metricas;

static void metricas_iniciar(Metricas *m) {
    memset(m, 0, sizeof *m);
    m->ppm_min = 1.0e9f;
    m->ppm_max = -1.0e9f;
}

static void metricas_registrar(Metricas *m, const Leitura *l) {
    m->leituras++;
    m->soma += (double)l->ppm;
    if (l->ppm < m->ppm_min) m->ppm_min = l->ppm;
    if (l->ppm > m->ppm_max) m->ppm_max = l->ppm;

    switch (l->nivel) {
        case NIVEL_CRITICO: m->criticos++; break;
        case NIVEL_ATENCAO: m->atencoes++; break;
        default:            m->normais++;  break;
    }
}

/* ------------------------------- BENCHMARK -------------------------------- */

static void benchmark_fila(size_t volume) {
    Fila fila;
    Leitura amostra;
    memset(&amostra, 0, sizeof amostra);
    amostra.ppm = 900.0f;
    fila_iniciar(&fila);

    double inicio = agora_segundos();
    for (size_t i = 0; i < volume; i++) {
        fila_enfileirar(&fila, amostra);
    }
    for (size_t i = 0; i < volume; i++) {
        fila_desenfileirar(&fila, &amostra);
    }
    double fim = agora_segundos();

    printf("%-14s %12zu %14.1f\n", "Fila FIFO", volume * 2,
           (fim - inicio) * 1e9 / (double)(volume * 2));
    fila_destruir(&fila);
}

static void benchmark_pilha(size_t volume) {
    Pilha pilha;
    Comando amostra;
    memset(&amostra, 0, sizeof amostra);
    amostra.ppm = 900.0f;
    pilha_iniciar(&pilha);

    double inicio = agora_segundos();
    for (size_t i = 0; i < volume; i++) {
        pilha_empilhar(&pilha, amostra);
    }
    for (size_t i = 0; i < volume; i++) {
        pilha_desempilhar(&pilha, &amostra);
    }
    double fim = agora_segundos();

    printf("%-14s %12zu %14.1f\n", "Pilha LIFO", volume * 2,
           (fim - inicio) * 1e9 / (double)(volume * 2));
    pilha_destruir(&pilha);
}

static void benchmark_arvore(size_t volume) {
    Arvore arvore;
    Leitura amostra;
    arvore_iniciar(&arvore);
    srand(12345);

    double inicio = agora_segundos();
    for (size_t i = 0; i < volume; i++) {
        memset(&amostra, 0, sizeof amostra);
        amostra.ppm = 300.0f + (float)(rand() % 2000);
        arvore_inserir(&arvore, amostra);
    }
    double fim = agora_segundos();

    printf("%-14s %12zu %14.1f\n", "Arvore ABB", volume,
           (fim - inicio) * 1e9 / (double)volume);
    arvore_destruir(&arvore);
}

static void executar_benchmark(void) {
    const size_t volumes[] = { 10000, 100000, 1000000 };
    const size_t volumes_arvore[] = { 1000, 10000, 100000 };

    printf("=== BENCHMARK: custo por operacao (media de execucao) ===\n");
    printf("%-14s %12s %14s\n", "estrutura", "operacoes", "ns/op");

    for (size_t i = 0; i < sizeof volumes / sizeof volumes[0]; i++) {
        benchmark_fila(volumes[i]);
    }
    for (size_t i = 0; i < sizeof volumes / sizeof volumes[0]; i++) {
        benchmark_pilha(volumes[i]);
    }
    for (size_t i = 0; i < sizeof volumes_arvore / sizeof volumes_arvore[0]; i++) {
        benchmark_arvore(volumes_arvore[i]);
    }

    printf("\nLeitura: tempo por operacao estavel em FIFO/LIFO => O(1).\n");
    printf("Na ABB o custo cresce de forma logaritmica => O(log n) medio.\n");
}

/* -------------------------- RELATORIO DE MEMORIA -------------------------- */

static int relatorio_memoria(void) {
    long saldo = (long)g_alocacoes - (long)g_liberacoes;
    printf("\n=== AUDITORIA DE MEMORIA ===\n");
    printf("Alocacoes (malloc) : %zu\n", g_alocacoes);
    printf("Liberacoes (free)  : %zu\n", g_liberacoes);
    printf("Saldo              : %ld\n", saldo);
    if (saldo == 0) {
        printf("[OK] Nenhum memory leak: todo no alocado foi liberado.\n");
        return EXIT_SUCCESS;
    }
    printf("[FALHA] Existem %ld blocos sem free.\n", saldo);
    return EXIT_FAILURE;
}

/* ---------------------------------- MAIN ---------------------------------- */

int main(int argc, char **argv) {
    Config cfg = {
        .setor           = "Estufa Alpha",
        .leituras        = LEITURAS_PADRAO,
        .limiar_atencao  = LIMIAR_ATENCAO_PADRAO,
        .limiar_critico  = LIMIAR_CRITICO_PADRAO,
        .semente         = SEMENTE_PADRAO
    };
    int modo_benchmark = 0;

    if (!ler_argumentos(argc, argv, &cfg, &modo_benchmark)) {
        return EXIT_FAILURE;
    }

    if (modo_benchmark) {
        executar_benchmark();
        return relatorio_memoria();
    }

    Fila   fila;
    Pilha  pilha;
    Arvore arvore;
    Metricas m;

    fila_iniciar(&fila);
    pilha_iniciar(&pilha);
    arvore_iniciar(&arvore);
    metricas_iniciar(&m);

    unsigned estado = (cfg.semente != 0U) ? cfg.semente : 1U;
    long agora = (long)time(NULL);

    /* ---------------- ETAPA 1: ingestao (enfileiramento) ---------------- */
    for (unsigned long i = 0; i < cfg.leituras; i++) {
        Leitura l;
        memset(&l, 0, sizeof l);
        l.id    = i + 1;
        snprintf(l.setor, sizeof l.setor, "%s", cfg.setor);
        l.ppm   = simular_ppm(&estado);
        l.epoch = agora + (long)(i * 5UL);
        l.nivel = classificar(l.ppm, cfg.limiar_atencao, cfg.limiar_critico);
        fila_enfileirar(&fila, l);
    }

    printf("=== ECOTECH MONITOR CO2 - ESTEIRA LIFO/FIFO ===\n");
    printf("Setor..............: %s\n", cfg.setor);
    printf("Leituras simuladas.: %lu\n", cfg.leituras);
    printf("Limiar de atencao..: %.0f ppm\n", (double)cfg.limiar_atencao);
    printf("Limiar critico.....: %.0f ppm\n", (double)cfg.limiar_critico);
    printf("Fila apos ingestao.: %zu no(s) na memoria\n", fila.tamanho);

    /* ---------------- ETAPA 2: processamento FIFO ---------------- */
    printf("\n=== ETAPA 2: PROCESSAMENTO FIFO (ordem de chegada) ===\n");
    printf("%-5s %-9s %-17s %8s %-8s %7s\n",
           "ID", "HORA", "SETOR", "PPM", "NIVEL", "EXAUST");

    Leitura atual;
    while (fila_desenfileirar(&fila, &atual)) {
        int duty = calcular_duty(atual.ppm, cfg.limiar_atencao, cfg.limiar_critico);
        int duty_anterior = m.duty;
        int mudou = (duty != duty_anterior);
        char hora[16];

        formatar_hora(atual.epoch, hora, sizeof hora);
        metricas_registrar(&m, &atual);

        if (!arvore_inserir(&arvore, atual)) {
            m.rejeitadas_arvore++;    /* indice recusou: limite de profundidade */
        }

        if (mudou) {
            Comando c;
            memset(&c, 0, sizeof c);
            c.id_leitura    = atual.id;
            c.ppm           = atual.ppm;
            c.nivel         = atual.nivel;
            c.epoch         = atual.epoch;
            c.duty_anterior = duty_anterior;
            c.duty_novo     = duty;
            snprintf(c.setor, sizeof c.setor, "%s", atual.setor);
            pilha_empilhar(&pilha, c);
            m.acionamentos++;
        }
        m.duty = duty;

        printf("%-5lu %-9s %-17s %8.1f %-8s %3d%%%s\n",
               atual.id, hora, atual.setor, (double)atual.ppm,
               rotulo_nivel(atual.nivel), duty, mudou ? " <- mudou" : "");
    }

    printf("Fila apos processamento: %zu no(s) (deve ser 0)\n", fila.tamanho);
    printf("Comandos empilhados...: %zu\n", pilha.tamanho);

    /* ---------------- ETAPA 3: pilha LIFO / rollback ---------------- */
    printf("\n=== ETAPA 3: PILHA LIFO (comandos do atuador) ===\n");

    const Comando *ultimo = pilha_topo(&pilha);
    if (ultimo == NULL) {
        printf("Nenhum comando de atuador foi necessario.\n");
    } else {
        char hora[16];
        formatar_hora(ultimo->epoch, hora, sizeof hora);
        printf("Topo da pilha (ultimo comando): leitura %04lu em %s, "
               "%.1f ppm -> exaustor %d%%\n",
               ultimo->id_leitura, hora, (double)ultimo->ppm, ultimo->duty_novo);

        /* Rollback: leitura defeituosa desfaz o ultimo comando fisico. */
        Comando desfeito;
        if (pilha_desempilhar(&pilha, &desfeito)) {
            m.duty = desfeito.duty_anterior;
            printf("[ROLLBACK] Comando da leitura %04lu desfeito: exaustor "
                   "%d%% -> %d%%\n",
                   desfeito.id_leitura, desfeito.duty_novo, desfeito.duty_anterior);
        }

        printf("Historico reverso (mais recente -> mais antigo):\n");
        printf("  %-6s %-17s %8s %8s %8s\n", "LEIT", "SETOR", "PPM", "DE", "PARA");
        while (pilha_desempilhar(&pilha, &desfeito)) {
            printf("  %-6lu %-17s %8.1f %7d%% %7d%%\n",
                   desfeito.id_leitura, desfeito.setor,
                   (double)desfeito.ppm,
                   desfeito.duty_anterior, desfeito.duty_novo);
        }
    }

    /* ---------------- ETAPA 4: indice nao linear (ABB) ---------------- */
    printf("\n=== ETAPA 4: ARVORE BINARIA DE BUSCA (indice por PPM) ===\n");
    printf("Nos indexados......: %zu\n", arvore.tamanho);
    printf("Altura da arvore...: %d (limite de seguranca: %d)\n",
           arvore_altura(arvore.raiz), PROFUNDIDADE_MAX_ARVORE);
    printf("Rejeitadas (limite).: %lu\n", m.rejeitadas_arvore);
    printf("Leituras >= %.0f ppm: %zu  (consulta em O(h))\n",
           (double)cfg.limiar_critico,
           arvore_contar_acima(arvore.raiz, cfg.limiar_critico));
    printf("Leituras >= %.0f ppm: %zu  (consulta em O(h))\n",
           (double)cfg.limiar_atencao,
           arvore_contar_acima(arvore.raiz, cfg.limiar_atencao));

    printf("\nMenores concentracoes (caminhamento em ordem = PPM crescente):\n");
    arvore_listar_em_ordem(&arvore, 10);

    /* ---------------- ETAPA 5: consolidado ---------------- */
    printf("\n=== ETAPA 5: CONSOLIDADO DO CICLO ===\n");
    printf("Leituras processadas: %lu\n", m.leituras);
    printf("  normais...........: %lu\n", m.normais);
    printf("  atencao...........: %lu\n", m.atencoes);
    printf("  criticos..........: %lu\n", m.criticos);
    printf("PPM minimo..........: %.1f\n", (double)m.ppm_min);
    printf("PPM maximo..........: %.1f\n", (double)m.ppm_max);
    printf("PPM medio...........: %.1f\n", m.leituras ? m.soma / (double)m.leituras : 0.0);
    printf("Acionamentos........: %lu\n", m.acionamentos);
    printf("Exaustor (final)....: %d%%\n", m.duty);

    /* ---------------- Encerramento e auditoria ---------------- */
    fila_destruir(&fila);
    pilha_destruir(&pilha);
    arvore_destruir(&arvore);

    return relatorio_memoria();
}
