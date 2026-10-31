# Banco de Arguições — 28 perguntas prováveis com resposta curta

> Use como treino. A banca pode chamar **qualquer** integrante. Resposta curta + apontar a linha do código é melhor que discurso decorado.

---

### 1. O que é uma fila (FIFO) e por que ela foi usada aqui?
FIFO = *First In, First Out*: o primeiro que entra é o primeiro que sai. Foi usada porque leitura de sensor tem ordem cronológica; processar fora de ordem distorceria a evolução do CO₂ no tempo. Operações em O(1).

### 2. O que é uma pilha (LIFO) e por que ela foi usada aqui?
LIFO = *Last In, First Out*: o último que entra é o primeiro que sai. O atuador tem um único estado físico (o último comando), então o rollback precisa do comando imediatamente anterior — que está no topo. Operações em O(1).

### 3. Por que não usar um vetor para a fila?
Vetor tem capacidade fixa (ou `realloc`) e o `dequeue` por deslocamento custa **O(n)**. A lista encadeada remove em O(1) e cresce sob demanda.

### 4. Por que não uma fila circular em vetor?
Fila circular resolve o deslocamento e é O(1), mas exige capacidade fixa e tratamento de "cheia". A encadeada não tem limite artificial e não precisa do cálculo modular `(i+1) % N`. Para este volume, a lista é mais simples e igualmente O(1).

### 5. O que garante o O(1) no `enqueue`?
O ponteiro `fim` (cauda). Sem ele, seria necessário percorrer a lista até o último nó para inserir → O(n). Com ele, a inserção é `f->fim->proximo = novo; f->fim = novo;`.

### 6. O que acontece se o `malloc` retornar `NULL`?
A função `alocar()` detecta, imprime mensagem de erro e encerra o programa (fail-fast). Continuar com ponteiro inválido corromperia a estrutura silenciosamente — pior que parar.

### 7. O que é underflow e como vocês trataram?
Underflow é tentar remover de uma estrutura vazia. Em `fila_desenfileirar` e `pilha_desempilhar` testamos `NULL` **antes** de qualquer acesso e retornamos `0`, sem escrever no destino. O laço `while (fila_desenfileirar(...))` usa esse retorno para parar.

### 8. E o overflow da fila?
Não existe capacidade fixa, então não há overflow de índice. O limite é a memória do heap; se o `malloc` falhar, o fail-fast trata.

### 9. O que é stack overflow e como o código evita?
É o esgotamento da pilha de execução, típico de recursão profunda ou VLA grande. Evitamos: (a) nenhum VLA; (b) a ABB rejeita inserção acima de 64 níveis; (c) o caminhamento em ordem é iterativo. Assim, toda recursão tem profundidade máxima de 64 frames.

### 10. Onde ficam a fila, a pilha e a árvore na memória?
Os **nós** ficam no heap (`malloc`). Os **descritores** (`Fila`, `Pilha`, `Arvore`) são variáveis locais na stack do `main` e ocupam 24, 16 e 16 bytes. A pilha local do caminhamento iterativo tem 64 ponteiros = 512 bytes de stack.

### 11. Qual a diferença entre stack e heap?
Stack: alocação automática, tamanho pequeno e fixo, liberada no fim do escopo, acesso rápido. Heap: alocação manual (`malloc`/`free`), tamanho dinâmico, exige gestão explícita — e é onde moram os nós.

### 12. O que é memory leak e como provaram que não há?
É perder a referência de um bloco alocado sem liberá-lo. Todo `malloc` passa por `alocar()` e todo `free` por `liberar()`, ambos incrementando contadores. No fim, `relatorio_memoria()` compara: saldo 0. O AddressSanitizer também roda com exit code 0.

### 13. O que é dangling pointer? Onde isso poderia acontecer?
É um ponteiro que aponta para memória já liberada. Poderia acontecer se liberássemos o nó e depois lêssemos `alvo->dados`. Por isso copiamos o payload para `*destino` **antes** do `free(alvo)`, e zeramos `f->fim` quando a fila esvazia.

### 14. O que é double free e como é evitado?
Liberar o mesmo bloco duas vezes. Como cada nó é removido de uma única estrutura e o ponteiro é descartado em seguida, não há segundo `free`. `liberar(NULL)` é seguro, mas não é usado como desculpa para lógica frouxa.

### 15. Por que `char setor[48]` dentro da struct em vez de `char *`?
Porque o texto viaja por valor com o registro. Um `char *` exigiria uma segunda alocação e criaria risco de ponteiro solto após o `free` do nó. Com vetor embutido: um `malloc` por nó, uma liberação por nó.

### 16. Por que guardar `nivel` se dá para calcular de novo?
Porque a classificação é feita **uma vez** na ingestão e reaproveitada em todos os relatórios. Custa 4 bytes e evita recomputação em cada exibição. É uma desnormalização consciente.

### 17. Por que uma árvore e não uma lista ordenada?
Inserção ordenada em lista custa O(n) por causa do deslocamento de ponteiros; na ABB é O(h) ≈ O(log n). Além disso, a árvore responde "quantos acima do limiar" podando subárvores inteiras.

### 18. Por que não usar uma AVL ou rubro-negra?
Porque o objetivo didático é demonstrar a ABB e o custo de mantê-la. Uma árvore balanceada exigiria rotações, mais código e mais pontos de erro. O limite de profundidade é a mitigação proporcional ao escopo.

### 19. Qual o pior caso da ABB?
O(n), quando as inserções chegam em ordem crescente (árvore degenera em lista). No caso médio, com dados aleatórios, é O(log n). Nosso limite de 64 níveis impede a degeneração catastrófica.

### 20. Como funciona `arvore_contar_acima`?
Se o nó atual tem `ppm >= limiar`, então **toda a subárvore direita também tem** (propriedade da ABB). Somamos `1 + subarvore(direita)` e descemos apenas à esquerda. Se o nó é menor que o limiar, descemos só à direita. Custo O(h).

### 21. O que é caminhamento em ordem e o que ele devolve?
Visitar esquerda → raiz → direita. Em uma ABB isso devolve os elementos em ordem crescente. Usamos a versão iterativa com pilha local para não depender de recursão.

### 22. Por que o limite de 64 níveis?
Para garantir que qualquer recursão (altura, contagem, destruição) nunca ultrapasse 64 frames — muito abaixo do limite típico de stack (8 MB). Com 64 níveis é possível indexar até 2⁶⁴ nós em teoria; na prática o limite nunca é atingido com dados reais.

### 23. Por que a pilha guarda só mudanças de estado?
Porque comandos repetidos não alteram o atuador. Empilhar apenas transições (0→50, 50→100, etc.) reduz memória e torna o histórico reverso legível: cada linha é uma mudança física real.

### 24. O que acontece com duas leituras de ppm iguais na árvore?
Duplicados vão para a direita (`valor.ppm < atual ? esquerda : direita`). A propriedade da ABB é mantida com `>=` à direita, e a contagem por faixa continua correta.

### 25. Como isso funcionaria com um ESP32 real?
O `simular_ppm()` seria substituído pela leitura UART do MH-Z19C; `calcular_duty()` acionaria um GPIO com PWM para o relé do exaustor. **A fila, a pilha, a árvore e a auditoria de memória continuariam idênticas** — é justamente o ponto: a arquitetura de dados não depende do hardware.

### 26. Qual a diferença entre este código e o exemplo inicial do professor?
O exemplo usava vetor estático de 5 posições e um contador de alertas. Aqui há estruturas dinâmicas com semântica definida (FIFO para ordem, LIFO para rollback, ABB para consulta), regra de negócio com atuador, tratamento de underflow, limite de profundidade e auditoria de memória.

### 27. Como vocês testam se há vazamento de memória?
Três formas: (a) contadores internos `malloc` × `free`; (b) AddressSanitizer (`make asan`); (c) Valgrind (`make valgrind`, se instalado). As três devem fechar com zero.

### 28. O que vocês fariam diferente com mais tempo?
Trocar a ABB por uma árvore balanceada (AVL) para garantir O(log n) no pior caso; adicionar persistência em CSV/SD; implementar o modo hardware com dois sensores para redundância e validação cruzada antes do rollback.

---

## Perguntas de "pegadinha" — respostas de 1 linha

| Pergunta | Resposta |
|---|---|
| "Se eu remover o `free`, o programa quebra?" | Não na hora — e é justamente por isso que existe a auditoria: o saldo deixa de ser zero. |
| "A fila pode ficar cheia?" | Não tem capacidade fixa; o limite é o heap e a falha de `malloc` é tratada. |
| "O `dequeue` é O(1) mesmo com 1 milhão de elementos?" | Sim; medido no `--benchmark`, ns/op estável. |
| "Onde está o `malloc` da pilha?" | Em `pilha_empilhar`, uma alocação por comando. |
| "Quantos `free` o programa faz no total?" | Exatamente o mesmo número de `malloc`; a saída mostra os dois contadores iguais. |
| "Por que `sizeof *novo` e não `sizeof(NoFila)`?" | Mesmo resultado, mas se o tipo do ponteiro mudar o código continua correto. |
| "O que acontece se `fila_desenfileirar` for chamada com `destino == NULL`?" | Hoje não é chamada assim; o contrato exige destino válido. Melhoria futura: validar o ponteiro. |
| "Isso é O(1) ou O(1) amortizado?" | O(1) puro nas operações; o custo do `malloc` é do alocador, não da estrutura. |

---

## Frases que valem ponto na arguição

- "Fila e pilha respondem perguntas diferentes: **ordem de processamento** versus **reversão de estado**."
- "O ponteiro de cauda é a diferença entre O(1) e O(n) no enfileiramento."
- "Copiar o payload antes do `free` é o que impede o *use-after-free*."
- "A auditoria de memória transforma 'acho que não vaza' em 'saldo zero, comprovado'."
- "A árvore não substitui a fila; ela é um índice para outra pergunta."
