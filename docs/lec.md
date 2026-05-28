## BNF:

- <N> ::= "abcd"

## EBNF:

- [] - opt(0, 1)
- {} - opt (0...)
- "abcd" - term
- abcd - noterm

Regular expression (regex) - a formula describing what productions it can form (R)
1. (/) - regex : L(/) = (/)
2. E - regex : L(E) = {E}
3. VAa->V - L(a) = {a}

- Детерменированный Конечный Автомат ДКА DFA (Deterministic finite automation) :arrow_forward:



## Other 

- weakly_incrementable
- incrementable
- indirectly_readable

# Sentinel

# no push front

## New lecture

- E -> E + T|T
- T -> ID |NUM| (E)
- E -> T + E|T

 НОВОЕ ПРАВИЛО
 A  -> b^1A'|...|b^nA'
 A' -> a^1A'|...|a^nA'



 // ПИШЕМ АВТОМАТНЫЙ ЛЕКСЕР (???)
 A = {q_0, q, /delta, F, /epsilon}
 q = {q_0, q_1 -> Id, q_2 -> Num, q_3 -> Dot, q_4 -> Num.D, q_5 -> Space, q_6 -> +, q_7 -> -, q_8 -> *, q_9 -> /, q_10 -> //, q_11 -> ^, q_12 -> (, q_13 -> ), q_14 -> ,, q_15 -> ., q_16 -> dead}

 /epsilon = {Alpha, Under, Digit, Dot, Plus, Minus, Star, Slash, Chref, Lparen, Rparen, Comma, Space}

| q\Cn |  \w  |   _  |  \d |   .  |   +  |  -  |  *  |   /  |   ^  |   (  |   )  |   ,  | \n   |  \s  |   0  |
|:----:|:----:|:----:|:---:|:----:|:----:|:---:|:---:|:----:|:----:|:----:|:----:|:----:|------|:----:|:----:|
|  q_0 |  q_1 |  q_1 | q_2 | q_16 |  q_6 | q_6 | q_6 | q_11 | q_11 | q_11 | q_11 | q_14 | q_5  |  q_5 | q_16 |
|  q_1 |  q_1 |  q_1 | q_1 | q_16 |      |     |     |      |      |      |      |      |      |      |      |
|  q_2 |  q_1 | q_16 | q_2 |  q_3 | q_16 |     |     |      |      |      |      |      |      |      |      |
|  q_3 | q_16 | q_16 | q_2 |  q_3 | q_16 |     |     |      |      |      |      |      |      |      |      |
|  q_4 | q_16 | q_16 | q_4 | q_16 |      |     |     |      |      |      |      |      |      |      |      |
|  q_5 | q_16 | q_16 | q_4 | q_16 |      |     |     |      |      |      |      |      |      |      |      |
|  q_6 |      |      |     |      |      |     |     |      |      |      |      |      |      |      |      |
|  ... | q_16 |      |     |      |      |     |     |      |      |      |      |      |      |      |      |
|  q_8 |      |      |     |      |      |     |     |      |      |      |      |      |      |      |      |
|  q_9 | q_16 |      |     |      |      |     |     | q_10 | q_16 |      |      |      |      |      | q_16 |
| q_10 | q_10 |      |     |      |      |     |     |      |      |      |      |      | q_16 | q_10 | q_10 |

F = {!q_0, q_3, q_15}