# Roteiro do Seminário — 20 min de defesa + 5 min de arguição

**Grupo:** `<nomes>` · **Tema:** CO₂ · **Sistema:** EcoTech Monitor CO₂ — Esteira LIFO/FIFO

> Regra de ouro: **ninguém lê slide**. Cada fala deve apontar para uma linha do código ou para uma saída do terminal. A banca pode chamar qualquer integrante para explicar qualquer bloco.

---

## 0. Antes de começar (checklist de 2 minutos)

```bash
cd ProjetoFinal-CO2
make clean && make          # precisa compilar sem warnings
./build/ecotech             # demo padrão
./build/ecotech --benchmark # prova de O(1)
make asan                   # prova de ausência de leak
```

Deixe 3 terminais/abas prontos:
1. Saída da execução padrão.
2. Saída do benchmark.
3. `src/main.c` aberto na função que for citada.

---

## 1. Bloco de abertura — 2 min (integrante A)

**Fala pronta:**
> "Nosso problema físico é a concentração de CO₂ em estufa. O sensor entrega ppm; a planta de execução transforma esse número em uma decisão de ventilação. O diferencial não é ler o sensor — é **como os dados atravessam a memória**: uma fila FIFO preserva a ordem de chegada, uma pilha LIFO garante rollback correto do último comando e uma árvore binária de busca responde consultas por faixa sem varrer tudo."

Mostrar: o diagrama do topo do `main.c`.

```
[SENSOR] -> [FILA FIFO] -> [CLASSIFICADOR] -> [PILHA LIFO] -> [ATUADOR]
```

**Números para citar:** 24 leituras, 3 faixas (0/50/100 %), complexidade O(1) nas duas estruturas lineares.

---

## 2. Regra de negócio — 3 min (integrante B)

Apontar `classificar()` e `calcular_duty()` no código.

- `ppm < 800` → NORMAL → 0 %
- `800 ≤ ppm < 1000` → ATENÇÃO → 50 %
- `ppm ≥ 1000` → CRÍTICO → 100 %

**Frase de efeito:**
> "A função é pura: entra leitura, sai percentual, sem efeito colateral. Isso permite testar a regra sem ligar hardware nenhum."

Mostrar na saída as linhas com `<- mudou` e explicar que **só empilhamos quando o estado do atuador muda** — economiza memória e evita comandos redundantes.

---

## 3. Motor FIFO — 4 min (integrante B ou C)

Abrir `fila_enfileirar` e `fila_desenfileirar`.

**Fala pronta:**
> "Fila é a estrutura certa porque leitura de sensor tem ordem temporal. Se eu usasse pilha, a última leitura seria processada primeiro e a primeira ficaria para o fim — isso inverteria a cronologia do ambiente."

Pontos obrigatórios:
1. `fim` (cauda) é o que dá O(1) no enfileiramento; sem ele seria O(n).
2. `dequeue` copia o dado **antes** do `free` — evita *use-after-free*.
3. Quando a fila esvazia, `fim` volta para `NULL` — evita ponteiro pendurado.
4. Underflow: `if (alvo == NULL) return 0;` — o laço de consumo termina sozinho.

Mostrar na saída:
```
Fila apos ingestao.: 24 no(s) na memoria
Fila apos processamento: 0 no(s) (deve ser 0)
```

---

## 4. Motor LIFO e rollback — 4 min (integrante C)

Abrir `pilha_empilhar`, `pilha_desempilhar` e o bloco de rollback no `main`.

**Fala pronta:**
> "O atuador tem um único estado físico: o último comando. Então, para desfazer, eu preciso do comando imediatamente anterior — e é exatamente o que o topo da pilha guarda. Cada comando carrega `duty_anterior` e `duty_novo`, o que torna o rollback exato, não uma estimativa."

Demonstrar ao vivo:
1. Rodar `--seed 2026` (saída reproduzível).
2. Mostrar `Topo da pilha` e a linha `[ROLLBACK]`.
3. Mostrar o histórico reverso descendo do mais recente ao mais antigo.

**Pergunta que a banca vai fazer:** "por que não desfazer em FIFO?" → porque restauraria um estado antigo e deixaria o exaustor incoerente com o presente.

---

## 5. Árvore (não linear) e Big-O — 3 min (integrante A ou D)

Abrir `arvore_inserir` e `arvore_contar_acima`.

**Fala pronta:**
> "A árvore não substitui a fila: ela responde outra pergunta. A fila diz *em que ordem processar*; a árvore diz *quantas leituras estão acima do limiar* e *quais são as menores concentrações*. Com o campo `subarvore`, a contagem poda metade da árvore a cada passo: O(h), não O(n)."

Mostrar:
```
Leituras >= 1000 ppm: 4  (consulta em O(h))
Altura da arvore...: 9 (limite de seguranca: 64)
```

**Big-O para decorar:**

| Operação | Complexidade |
|---|---|
| enqueue / dequeue | O(1) |
| push / pop | O(1) |
| inserir na ABB | O(h) ≈ O(log n) |
| contar acima do limiar | O(h) |
| ciclo completo | O(n log n) |

---

## 6. Memória e prova de robustez — 3 min (integrante D)

Abrir `alocar()`, `liberar()` e `relatorio_memoria()`.

**Fala pronta:**
> "Toda alocação e toda liberação passam por dois contadores. No fim do programa o saldo tem que ser zero. Se alguém remover um `free`, a auditoria acusa imediatamente."

Mostrar:
```
Alocacoes (malloc) : 64
Liberacoes (free)  : 64
Saldo              : 0
```

Rodar `./build/ecotech --benchmark` e apontar que ns/op não cresce de 20 mil para 2 milhões de operações → **prova empírica do O(1)**.

**Stack overflow:** "não usamos VLA; a árvore rejeita inserção acima de 64 níveis; o caminhamento em ordem é iterativo com 512 bytes fixos de stack."

**Underflow:** "as duas remoções testam `NULL` antes de acessar; a função devolve 0 e o laço de destruição para."

---

## 7. Fechamento — 1 min (integrante A)

> "Resumindo: FIFO garante justiça temporal, LIFO garante reversão correta, ABB garante consulta eficiente. As três convivem sobre a mesma `struct Leitura`, sem vazamento de memória e com complexidade comprovada. Obrigado."

---

## 8. Preparação para os 5 min de arguição

Regras:
- Quem for chamado **abre o arquivo na função** e explica linha a linha. Não vale ler slide.
- Se não souber, diga: "não sei de memória, mas sei onde está no código" e abra o trecho. Isso vale mais que inventar.
- Tenha decorado: **tamanho da struct (80 B)**, **complexidade das 4 operações (O(1))**, **saldo de memória (0)** e **limiar crítico (1000 ppm)**.

Ver `docs/banco-de-arguicoes.md` para as 25 perguntas prováveis.
