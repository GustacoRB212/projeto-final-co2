# EcoTech Monitor CO₂ — Esteira de Decisão LIFO/FIFO

**Documentação Técnica — Padrão README Corporativo**
Disciplina: Algoritmo e Estrutura de Dados · Professor Me. Valdir Prado
Entrega: Projeto Final (Inovatech + A2) · Prazo: 01/12/2026 às 23:59

---

## I. Cabeçalho e Identificação

| Campo | Conteúdo |
|---|---|
| **Nome do sistema** | EcoTech Monitor CO₂ — Esteira de Decisão LIFO/FIFO |
| **Nicho temático** | Agronegócio / agricultura de precisão em ambiente protegido (estufas) |
| **Problema físico** | Concentração de CO₂ (ppm) no ar interno |
| **Versão** | 2.0.0 |
| **Linguagem** | C (padrão C11) |
| **Licença** | MIT (repositório público) |
| **Repositório** | `<inserir link público do GitHub/GitLab>` |

**Integrantes do grupo**

| Nome completo | RA | Responsabilidade principal |
|---|---|---|
| `<Nome 1>` | `<RA>` | Arquitetura de dados (FIFO/LIFO) |
| `<Nome 2>` | `<RA>` | Regra de negócio e atuador |
| `<Nome 3>` | `<RA>` | Documentação e testes de memória |
| `<Nome 4>` | `<RA>` | Seminário e integração |

> **Preencher os nomes antes de submeter no LXP.** Apenas um integrante envia o arquivo.

---

## II. Escopo Arquitetural

### II.1 Problema físico mapeado

Em estufas e galpões agrícolas fechados, o CO₂ é consumido pela fotossíntese durante o dia e produzido pela respiração das plantas e do solo à noite. Fora da faixa ideal (≈ 400–800 ppm) a produtividade cai: abaixo de 350 ppm há déficit de assimilação; acima de 1.000 ppm o ambiente passa a exigir renovação de ar por segurança e conforto das culturas e dos trabalhadores.

O sistema lê a concentração de CO₂ (ppm), classifica cada leitura e aciona um **exaustor** com velocidade proporcional ao risco.

### II.2 Métrica de acionamento lógico

| Faixa de CO₂ | Classificação | Ação do atuador (exaustor) |
|---|---|---|
| `ppm < 800` | NORMAL | 0 % (desligado) |
| `800 ≤ ppm < 1000` | ATENÇÃO | 50 % (ventilação preventiva) |
| `ppm ≥ 1000` | CRÍTICO | 100 % (exaustão máxima) |

A função que implementa a regra é `calcular_duty()` (arquivo `src/main.c`). Ela é **pura**: recebe a leitura e os limiares, devolve o percentual, sem efeito colateral — o que a torna trivialmente testável e auditável.

### II.3 Decisões arquiteturais e justificativa

| Necessidade do domínio | Estrutura escolhida | Justificativa |
|---|---|---|
| Preservar a ordem cronológica das leituras (nenhuma leitura "fura fila") | **Fila FIFO** encadeada | `enqueue`/`dequeue` em **O(1)**; sem limite de capacidade; sem *starvation* |
| Reverter o último comando físico quando uma leitura é diagnosticada como defeituosa | **Pilha LIFO** encadeada | O comando mais recente é exatamente o que está ativo no atuador → rollback correto em **O(1)** |
| Consultar rapidamente "quantas leituras estão acima do limiar" e listar por ordem de ppm | **Árvore Binária de Busca (ABB)** | Estrutura **não linear**: consulta em **O(h)**, caminhamento em ordem devolve ppm crescente sem ordenar de novo |

> **Por que não só vetores?** Vetor exigiria capacidade fixa (ou `realloc`) e o `dequeue` por deslocamento custaria **O(n)**. A lista encadeada elimina o deslocamento e cresce sob demanda. O vetor continua sendo usado apenas onde é a melhor escolha: a pilha local de 64 ponteiros do caminhamento iterativo da árvore (memória contígua, sem heap).

---

## III. Lista de Materiais (BOM)

### III.1 Entrega atual — projeto 100 % simulado

Conforme orientação do professor, **não há integração física com sensores nesta entrega**. O sensor é substituído por um gerador determinístico (`simular_ppm()`), o que permite demonstrar a esteira de dados de forma reprodutível.

| Item | Especificação | Quantidade | Situação |
|---|---|---|---|
| Computador com compilador C11 (GCC ≥ 9 ou Clang ≥ 10) | — | 1 | Necessário |
| Biblioteca padrão C (`libc`) | `stdio`, `stdlib`, `string`, `time` | 1 | Necessário |
| Sensor físico de CO₂ | — | 0 | **Não aplicável nesta entrega** |
| Atuador físico | — | 0 | **Não aplicável nesta entrega** |

### III.2 Anexo — BOM prevista para fase futura de integração (não executada)

| Item | Modelo de referência | Qtd. | Função |
|---|---|---|---|
| Microcontrolador | ESP32 DevKit V1 (30 pinos) | 1 | Aquisição e controle |
| Sensor de CO₂ NDIR | MH-Z19C (UART, 0–5000 ppm) | 1 | Medição real de ppm |
| Módulo relé | 1 canal, 5 V, optoacoplado | 1 | Chaveamento do exaustor |
| Exaustor | 12 V DC, 0,3 A | 1 | Renovação de ar |
| Fonte | 12 V / 2 A + conversor buck 5 V | 1 | Alimentação |
| Infraestrutura | Protoboard, jumpers, caixa de proteção | 1 kit | Montagem |

> Esta lista é **projeção**, não foi montada nem testada. Não confundir com o escopo entregue.

---

## IV. Engenharia de Memória (Inovação)

### IV.1 Mapeamento da `struct` central

```c
typedef struct {
    unsigned long id;               /*  0  |  8 bytes */
    char          setor[48];        /*  8  | 48 bytes */
    float         ppm;              /* 56  |  4 bytes */
    long          epoch;            /* 64  |  8 bytes */
    Nivel         nivel;            /* 72  |  4 bytes */
} Leitura;                          /* total: 80 bytes (x86-64) */
```

| Campo | Offset | Tipo | Tamanho | Papel na arquitetura |
|---|---:|---|---:|---|
| `id` | 0 | `unsigned long` | 8 | Rastreabilidade: liga leitura → comando → rollback |
| `setor` | 8 | `char[48]` | 48 | Identificação textual **inline** — sem `char *`, logo sem ponteiro solto/dangling |
| `ppm` | 56 | `float` | 4 | Grandeza física medida |
| `epoch` | 64 | `long` | 8 | Ordenação temporal e formatação de hora |
| `nivel` | 72 | `Nivel` (enum) | 4 | Classificação **calculada uma única vez** na ingestão (evita recomputar em cada relatório) |

**Por que `char setor[48]` e não `char *setor`?**
Com vetor embutido, o texto viaja **por valor** junto com o registro. Ao desenfileirar, basta `*destino = alvo->dados` e depois `free(alvo)` — não existe a janela de *use-after-free* típica de liberar o nó e continuar usando um ponteiro interno. Além disso, cada registro custa **um único `malloc`**, o que simplifica a contabilidade de memória (1 alocação = 1 liberação).

### IV.2 Custos de memória

| Estrutura | `sizeof` (x86-64) | Composição |
|---|---:|---|
| `Leitura` | 80 B | dados do domínio |
| `NoFila` | 88 B | `Leitura` (80) + ponteiro `proximo` (8) |
| `NoPilha` | 88 B | `Comando` (80) + ponteiro `abaixo` (8) |
| `NoArvore` | 104 B | `Leitura` (80) + `esquerda` (8) + `direita` (8) + `subarvore` (8) |
| `Fila` (descritor) | 24 B | `inicio` + `fim` + `tamanho` |
| `Pilha` (descritor) | 16 B | `topo` + `tamanho` |
| `Arvore` (descritor) | 16 B | `raiz` + `tamanho` |

Exemplo com 24 leituras: `24×88` (fila) + `16×88` (pilha de comandos) + `24×104` (ABB) = **6.016 bytes de heap**. O consumo é linear no número de leituras, `O(n)`.

### IV.3 Motor FIFO — fila encadeada

```
inicio                          fim
  |                              |
  v                              v
+------+    +------+    +------+
| L1   |--->| L2   |--->| L3   |---> NULL
+------+    +------+    +------+
  ^ sai                     ^ entra
```

**Enfileirar (`fila_enfileirar`) — O(1)**
1. `malloc` de um `NoFila` (verificado; falha → erro fatal controlado).
2. Copia a `Leitura` para `novo->dados`.
3. Se a fila não está vazia, `f->fim->proximo = novo`; senão, `f->inicio = novo`.
4. `f->fim = novo` e `f->tamanho++`.

**Desenfileirar (`fila_desenfileirar`) — O(1)**
1. Se `f->inicio == NULL` → retorna `0` (**underflow tratado sem tocar no destino**).
2. Guarda o nó em `alvo`, avança `f->inicio = alvo->proximo`.
3. Se a fila esvaziou, zera também `f->fim` (evita ponteiro para memória liberada).
4. Copia `alvo->dados` **antes** do `free(alvo)`.
5. `f->tamanho--`.

O ponteiro de cauda (`fim`) é o que garante o **O(1)**: sem ele, enfileirar exigiria percorrer a lista inteira → O(n).

### IV.4 Motor LIFO — pilha encadeada

```
topo
  |
  v
+------+    +------+    +------+
| C3   |--->| C2   |--->| C1   |---> NULL
+------+    +------+    +------+
  ^ entra/sai
```

**Empilhar (`pilha_empilhar`) — O(1)**
1. `malloc` do `NoPilha`.
2. `novo->abaixo = p->topo`.
3. `p->topo = novo` e `p->tamanho++`.

**Desempilhar (`pilha_desempilhar`) — O(1)**
1. `alvo = p->topo`; se `NULL` → retorna `0` (**underflow tratado**).
2. `p->topo = alvo->abaixo`.
3. Copia o `Comando` antes de `free(alvo)`.
4. `p->tamanho--`.

**Por que o rollback precisa ser LIFO e não FIFO?** O atuador tem um único estado físico (o último comando aplicado). Para desfazê-lo corretamente é preciso conhecer o estado imediatamente anterior — que está no topo da pilha. Desfazer em ordem de chegada (FIFO) restauraria um estado antigo e deixaria o exaustor em uma condição incoerente. Cada `Comando` guarda `duty_anterior` e `duty_novo`, permitindo reversão exata.

### IV.5 Índice não linear — ABB por ppm

- **Inserção iterativa** (`arvore_inserir`): desce a árvore por comparação de `ppm`, sem recursão. Custo **O(h)**.
- **Consulta de faixa** (`arvore_contar_acima`): se o nó já é `≥ limiar`, **toda a subárvore direita também é**; soma-se `1 + subarvore(direita)` e desce-se apenas à esquerda. Custo **O(h)**, não O(n).
- **Caminhamento em ordem** (`arvore_listar_em_ordem`): iterativo, com pilha local de 64 ponteiros (512 B de stack), devolve ppm em ordem crescente.
- **Campo `subarvore`**: tamanho da subárvore, atualizado no caminho raiz→nó durante a inserção, é o que permite a contagem em O(h).

### IV.6 Mitigação de erros físicos: Stack Overflow e Underflow

| Risco | Como o código previne | Evidência |
|---|---|---|
| **Stack overflow** por recursão profunda ou VLA | Nenhum VLA é usado. A ABB rejeita inserções que ultrapassariam **64 níveis** (`PROFUNDIDADE_MAX_ARVORE`), então `arvore_altura`, `arvore_contar_acima` e `arvore_destruir_rec` têm profundidade máxima garantida de 64 frames. O caminhamento em ordem é iterativo. | Campo "Altura da árvore" na saída (ex.: 9) e "Rejeitadas (limite)" |
| **Stack overflow** por estrutura de dados na pilha do processo | Fila e pilha vivem no **heap**; os descritores têm 24 B e 16 B. O número de leituras não consome stack. | `sizeof(Fila)`/`sizeof(Pilha)` no item IV.2 |
| **Underflow** (remover de estrutura vazia) | `fila_desenfileirar` e `pilha_desempilhar` testam `NULL` **antes** de qualquer acesso e retornam `0` sem escrever no destino. `fila_destruir`/`pilha_destruir` dependem justamente desse retorno para parar. | Laço `while (fila_desenfileirar(...))` encerra com contador em 0 |
| **Overflow de fila** | Estrutura encadeada não tem capacidade fixa: cresce até o limite do heap. Não existe índice circular para "estourar". | `Fila apos ingestao: 24 nós` |
| **Memory leak** | Toda alocação passa por `alocar()` e toda liberação por `liberar()`, ambos contabilizados. As funções `*_destruir` percorrem a estrutura até o fim. | Saldo `malloc − free = 0` ao final |
| **Dangling pointer / double free** | O payload é copiado **antes** do `free`; o ponteiro `fim` é zerado quando a fila esvazia; `liberar(NULL)` é seguro. | Auditoria com AddressSanitizer (item VI.5) |
| **Falha de `malloc`** | Verificada em cada chamada; o programa encerra com mensagem explícita em vez de continuar com ponteiro inválido (fail-fast). | Revisão de código + `-Werror`-level warnings limpos |

### IV.7 Complexidade — tabela Big-O

| Operação | Estrutura | Complexidade | Observação |
|---|---|---|---|
| `fila_enfileirar` | FIFO | **O(1)** | usa ponteiro de cauda |
| `fila_desenfileirar` | FIFO | **O(1)** | remove o primeiro nó |
| `pilha_empilhar` | LIFO | **O(1)** | insere no topo |
| `pilha_desempilhar` | LIFO | **O(1)** | remove o topo |
| `pilha_topo` | LIFO | **O(1)** | apenas leitura |
| `arvore_inserir` | ABB | **O(h)** ≈ O(log n) médio | pior caso O(n) se degenerada — mitigado pelo limite de profundidade |
| `arvore_contar_acima` | ABB | **O(h)** | usa `subarvore` para podar |
| `arvore_listar_em_ordem` | ABB | **O(k)** para k nós | iterativo |
| `arvore_destruir` | ABB | **O(n)** | pós-ordem, profundidade ≤ 64 |
| Varredura completa do ciclo | FIFO + ABB | **O(n log n)** | n desenfileiramentos + n inserções |

**Prova empírica do O(1):** o modo `--benchmark` mede o tempo por operação em 10 mil, 100 mil e 1 milhão de operações. O valor em ns/op permanece estável (variação de ruído de medição), o que confirma o comportamento constante. Um `dequeue` em vetor com deslocamento cresceria linearmente com o tamanho.

Saída real desta máquina:

```
estrutura         operacoes          ns/op
Fila FIFO             20000           90.4
Fila FIFO            200000           90.5
Fila FIFO           2000000           89.4
Pilha LIFO            20000           58.6
Pilha LIFO           200000           88.3
Pilha LIFO          2000000           71.1
Arvore ABB             1000          449.6
Arvore ABB            10000          700.1
Arvore ABB           100000         5552.8
```

> Os números absolutos variam por máquina; o que importa é a **estabilidade** de FIFO/LIFO entre escalas. Na ABB o custo cresce com a altura (e inclui o `malloc` de cada nó).

---

## V. Topologia de Conexão

### V.1 Entrega atual

**Não aplicável.** Não há placa, sensor ou atuador físicos nesta entrega; toda a "pinagem" é lógica, representada pelos ponteiros `inicio`/`fim`, `topo` e `raiz`.

### V.2 Anexo — pinagem prevista (fase futura, não executada)

| Sinal | ESP32 | Sensor / Atuador | Observação |
|---|---|---|---|
| UART TX | GPIO 17 (TX2) | RX do MH-Z19C | nível 3,3 V |
| UART RX | GPIO 16 (RX2) | TX do MH-Z19C | nível 3,3 V |
| Controle do relé | GPIO 26 | IN do módulo relé | saída digital |
| Alimentação do sensor | 5 V (VIN) | VCC do MH-Z19C | — |
| Terra comum | GND | GND do sensor, relé e fonte | **obrigatório** |
| Carga | — | Exaustor 12 V via relé | fonte separada |

> Documentado apenas para evidenciar o planejamento de integração futura. **Não faz parte do artefato avaliado nesta entrega.**

---

## VI. Manual de Deploy

### VI.1 Dependências

| Dependência | Versão mínima | Observação |
|---|---|---|
| Compilador C | GCC 9 / Clang 10 (C11) | `-std=c11` |
| `libc` | qualquer POSIX.1-2001 | `clock_gettime` |
| `make` | 3.81 | opcional (há comando direto) |
| AddressSanitizer | embutido no GCC/Clang | opcional, para auditoria |

**Nenhuma biblioteca externa.** Não há `Arduino.h`, não há `Wire.h`, não há dependência de rede.

### VI.2 Compilação

```bash
# opção 1 — make
make

# opção 2 — comando direto
gcc -std=c11 -Wall -Wextra -Wpedantic -O2 src/main.c -o build/ecotech
```

### VI.3 Execução

```bash
./build/ecotech                          # demonstração padrão (24 leituras)
./build/ecotech --leituras 100           # volume maior
./build/ecotech --setor "Estufa Norte"   # nome do setor
./build/ecotech --limiar-critico 1200    # calibração do limiar
./build/ecotech --seed 7                 # reprodutibilidade
./build/ecotech --benchmark              # prova de complexidade
./build/ecotech --help                   # ajuda
```

| Parâmetro | Padrão | Faixa válida |
|---|---|---|
| `--setor` | `Estufa Alpha` | texto livre |
| `--leituras` | 24 | 1 a 5000 |
| `--limiar-atencao` | 800 ppm | `> 0` e `<` limiar crítico |
| `--limiar-critico` | 1000 ppm | `>` limiar de atenção |
| `--seed` | 2026 | inteiro sem sinal |

### VI.4 Baud Rate

**Não aplicável** — não há comunicação serial física nesta entrega (o monitor é o próprio terminal).

Anexo para a fase futura: o MH-Z19C opera em **9600 bps, 8N1**; o `Serial Monitor` do ESP32 seria configurado em **115200 bps**.

### VI.5 Testes e evidências

```bash
make asan      # AddressSanitizer + UndefinedBehaviorSanitizer
make valgrind  # se o valgrind estiver instalado
make bench     # benchmark de complexidade
```

Resultado do AddressSanitizer nesta máquina: **exit code 0, nenhum erro, nenhum leak reportado**. A auditoria interna do programa também fecha com saldo zero:

```
=== AUDITORIA DE MEMORIA ===
Alocacoes (malloc) : 64
Liberacoes (free)  : 64
Saldo              : 0
[OK] Nenhum memory leak: todo no alocado foi liberado.
```

### VI.6 Saída esperada (trecho real)

```
=== ECOTECH MONITOR CO2 - ESTEIRA LIFO/FIFO ===
Setor..............: Estufa Alpha
Leituras simuladas.: 24
Limiar de atencao..: 800 ppm
Limiar critico.....: 1000 ppm
Fila apos ingestao.: 24 no(s) na memoria

=== ETAPA 2: PROCESSAMENTO FIFO (ordem de chegada) ===
ID    HORA      SETOR                  PPM NIVEL     EXAUST
1     21:06:18  Estufa Alpha        1127.0 CRITICO  100% <- mudou
2     21:06:23  Estufa Alpha         490.0 NORMAL     0% <- mudou
...
24    21:08:13  Estufa Alpha         761.0 NORMAL     0% <- mudou
Fila apos processamento: 0 no(s) (deve ser 0)
Comandos empilhados...: 16

=== ETAPA 3: PILHA LIFO (comandos do atuador) ===
Topo da pilha (ultimo comando): leitura 0024 em 21:08:13, 761.0 ppm -> exaustor 0%
[ROLLBACK] Comando da leitura 0024 desfeito: exaustor 0% -> 50%

=== ETAPA 4: ARVORE BINARIA DE BUSCA (indice por PPM) ===
Nos indexados......: 24
Altura da arvore...: 9 (limite de seguranca: 64)
Leituras >= 1000 ppm: 4  (consulta em O(h))
Leituras >= 800 ppm: 11  (consulta em O(h))
```

### VI.7 Estrutura do repositório

```
ProjetoFinal-CO2/
├── README.md                  <- este documento (entregável do LXP)
├── Makefile                   <- build, run, bench, asan, valgrind
├── src/
│   └── main.c                 <- planta de execução completa (~770 linhas)
└── docs/
    ├── roteiro-seminario.md   <- divisão dos 20 minutos + demo ao vivo
    └── banco-de-arguicoes.md  <- perguntas prováveis da banca com respostas
```

Para gerar o PDF da entrega:

```bash
pandoc README.md -o README.pdf --pdf-engine=xelatex
# ou: abrir o README.md no VS Code e usar "Markdown PDF: Export (pdf)"
```

---

## VII. Checklist de submissão (01/12/2026, 23:59)

- [ ] Preencher nomes e RAs no item I.
- [ ] Inserir o link do repositório público (ou anexar o PDF).
- [ ] Rodar `make` e confirmar que compila **sem warnings**.
- [ ] Rodar `make asan` e confirmar exit code 0.
- [ ] Ensaiar a demo de 20 min com o roteiro de `docs/roteiro-seminario.md`.
- [ ] Cada integrante saber explicar **qualquer** bloco do `main.c` (arguição ao vivo).
- [ ] Submeter **um único arquivo** (PDF ou link) no LXP até o prazo.

---

## VIII. Limitações declaradas

1. O sensor é simulado por PRNG determinístico; não há calibração real de NDIR.
2. Não há integração física, portanto BOM e pinagem são anexos de planejamento.
3. A ABB não é auto-balanceada (não é AVL/Rubro-Negra). O limite de profundidade evita degeneração catastrófica, mas um conjunto ordenado de entradas ainda produziria árvore mais alta que o ideal — trade-off aceito pela simplicidade didática.
4. O rollback LIFO é um mecanismo de **auditoria** para leitura defeituosa, não uma válvula de segurança; em produção, reverter um comando de exaustão exigiria confirmação de um segundo sensor.
5. `malloc` com falha encerra o processo (fail-fast). Em sistema embarcado real isso seria tratado com *watchdog* e reinício controlado.
