# Базовые спецификации

**Цель языка:** быть максимально предсказуемым и безопасным.
Спецификация находится в процессе разработки.

### Основные принципы:

1. **Закон безопасности (Safety law)**
2. **Закон детерминизма (Determinism law)** 
3. **Закон простоты (Simplicity law)** 

---

# Типы данных

### Числовые типы:

* `int` — целые числа
* `double` — двойная точность
* `float` — одинарная точность

### Нечисловые типы:

* `char` — символ
* `string` — неизменяемая строка
* `buf_string` — изменяемая строка
* `unit` — отсутствие значения (аналог `void`)
* `bool` — логический тип
* `optional[T]` — значение может отсутствовать
* `result[T, E]` — результат с возможной ошибкой

---

# Управляющие конструкции (циклы и ветвления)

* `if (...) else if (...) else (...)`
* `while (...)`
* `for (...)`


---

# Выражения

* **Комментарии:** `// до конца строки`

* **Лямбда-функции:**

  * Полная форма: `() -> { return; }`
  * Короткая форма: `() -> ;`
  * Можно использовать в любом месте
  * Захват переменных — **только неизменяемые, по значению**

* **if как выражение** (возвращает значение)

---

# Арифметика и операции

* Переполнение вызывает **trap (ошибку выполнения)**
* Деление на ноль — ошибка
* Все приведения типов — **явные**
* Все сравнения — **явные**


---

# Объявления

* `:` используется для любых объявлений

### Контейнеры:

* Структуры (`Structures`)
* Массивы (`Arrays`)

### Функции:

* 

---

# Средние спецификации

### Объекты:

* Классы (`Classes`)
* Наследование (`Inheritance`)
* Автоматическое создание и уничтожение объектов

### Специальные выражения:

* Интерполяция строк


---

# Дополнительные спецификации

* Сборка мусора (Garbage Collection)
* Среда выполнения (Runtime)
* Генерация кода
* Оптимизация
* Ввод/вывод файлов

---

# Теперь самое полезное — советы

### 1. У тебя уже есть идея философии — это редкость

Три закона (безопасность, детерминизм, простота) — это сильная база.
Но сейчас они декларативные. Нужно:

👉 Прописать **конкретные правила**, например:

* “Нет неинициализированных переменных”
* “Все функции чистые по умолчанию”
* “Нет глобального состояния”

---

### 2. Сейчас структура уровней странная

“Basic / Medium / Extra” звучит как DLC к языку.

Сделай нормальную иерархию:

* Core (типовая система, выражения)
* Control flow
* Memory model
* Runtime
* Advanced features

---

### 3. Самое сильное место — безопасность. Дожми его

Ты уже начал:

* trap on overflow
* explicit conversion

Добавь:

* отсутствие `null` (заменить на `optional`)
* неизменяемость по умолчанию
* контроль владения (если хочешь уровень Rust)

---

### 4. Лямбды — недоопределены

Сейчас:

> “can be placed anywhere”

Это опасно. Нужно уточнить:

* есть ли типы функций?
* поддерживается ли каррирование?
* можно ли возвращать функции?

---

### 5. “Same type argument declaration (no comma)” — выглядит как потенциальная боль

Пример:

```
func(a int b int)
```

Это читается хуже, чем с запятой.

👉 Либо:

* объясни, зачем это
* либо убери, пока не стало религией

---

### 6. Где система типов уровня “строгости”?

Сейчас есть:

* optional
* result

Но нет:

* pattern matching
* exhaustiveness checking

А без этого `result` — просто декоративный.

---

### 7. Нет модели памяти

Ты упомянул:

* GC
* auto destruction

Но это взаимоисключающие подходы.

👉 Нужно выбрать:

* GC (как Go, Java)
* Ownership (как Rust)
* RAII (как C++)

Иначе получится Frankenstein.

---

### 8. Runtime и codegen — пока просто слова

Это не спецификация, это TODO-лист.

Добавь хотя бы:

* компилируемый или интерпретируемый язык
* есть ли JIT
* таргет (LLVM? bytecode?)

# Memory model proposal

1. Storage classes
* Stack storage for local value types and compiler-known fixed-size data
* Heap storage for dynamically sized or shared objects
2. Ownership
* Every heap object has a single owner by default
* Assignment of owned objects transfers ownership
* Copying heap objects requires explicit clone
3. Borrowing
* Functions may take temporary immutable or mutable borrows
* At most one mutable borrow, or any number of immutable borrows, within a lexical scope
* Borrowed references cannot be stored or returned in v1 except in trivial compiler-verified cases
4. Deterministic destruction
* Owned values are destroyed automatically at end of scope
* Destruction order is deterministic
5. Shared state
* Variables are thread-local by default
* Inter-thread sharing requires explicit shared[T]
** shared[T] uses atomic reference counting
* Shared mutation requires synchronization primitives
6. Cycle handling
* Reference cycles in shared graphs must use weak references for back-links
* Cycles formed only from strong shared references are considered programmer error or rejected where detectable
