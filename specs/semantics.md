# Elio Semantics

## Evaluation strategy

Elio uses **call-by-value**: all arguments are fully evaluated before the function is
called, and the callee receives a copy of each value. Modifications inside a function
do not affect the caller's variables.

References (`&T`, `&mut T`) are an exception: passing `&x` passes a pointer to `x`,
and the callee may read (or, for `&mut T`, write) through it.

## Assignment semantics

Elio uses **value semantics**. Assigning a struct, array, or string copies the value;
the two names thereafter refer to independent copies. There is no aliasing.

## Variable binding and scope

A variable is in scope from its `let` or `const` declaration until the end of the
enclosing block. Inner blocks may shadow outer bindings; the outer binding is restored
when the inner block exits.

Variables declared in the condition or pattern of a `for`, `while`, or `match` arm
are scoped to the body of that construct.

## Mutability

`let` bindings are **immutable** by default. A `let mut` binding may be reassigned.
Attempting to assign to an immutable binding is a compile-time error.

`const` bindings are always immutable and must have a compile-time constant initialiser.

## Initialisation

Every variable must be initialised at its declaration site. Uninitialised variables
are a compile-time error. There is no default-initialisation of any type.

## Operator precedence and associativity

All binary operators are left-associative. Precedence, highest to lowest:

| Level | Operators |
|-------|-----------|
| Unary | `!` `-` `*` `~` |
| Multiplicative | `*` `/` `%` |
| Additive | `+` `-` |
| Shift | `<<` `>>` |
| Bitwise AND | `&` |
| Bitwise XOR | `^` |
| Bitwise OR | `\|` |
| Comparison | `<` `<=` `>` `>=` |
| Equality | `==` `!=` |
| Logical AND | `&&` |
| Logical OR | `\|\|` |
| Assignment | `=` `+=` `-=` etc. |

`&&` and `||` use **short-circuit evaluation**: the right operand is not evaluated if
the left operand determines the result.

Compound assignments (`a += b`) desugar to `a = a + b` at parse time. They require
the left side to be a mutable lvalue.

## Integer arithmetic

Integer overflow is a **compile error or runtime trap** depending on the operation:
- Default arithmetic operators (`+`, `-`, `*`) trap on overflow (the program exits
  with `runtime error: integer overflow at line N`).
- To opt in to wrapping behaviour, use `wrap_add(a, b)`, `wrap_sub(a, b)`,
  `wrap_mul(a, b)`.

Division by zero is always a runtime error:
```
runtime error: division by zero at line N
```

Right shift on signed integers is arithmetic (sign-extending). Right shift on unsigned
integers is logical (zero-filling). Shifting by an amount greater than or equal to the
bit width is undefined behaviour in LLVM and is not checked at runtime; document this
as a known limitation.

## Boolean operations

`&&` and `||` are defined only on `bool` operands. Using a numeric value where a bool
is expected is a type error.

## Strings

A `string` is a UTF-8 byte sequence represented internally as a `{ ptr, len }` pair.
`len` is the **byte length**, not the codepoint count. This is the documented meaning
of "string length" in Elio.

Supported string operations:
- `+`: concatenation (allocates a new buffer via `malloc`; the program does not free it)
- `==`, `!=`: byte-wise equality
- `print(s)`: writes the bytes to standard output

String literals support the escape sequences: `\n`, `\t`, `\r`, `\0`, `\\`, `\"`, `\'`.

## Arrays

An array type `[T; N]` has a fixed size N that is part of the type.
Arrays are value types: assigning one array to another copies all N elements.

Array index expressions `a[i]` are bounds-checked at runtime:
```
runtime error: index out of bounds at line N
```
if `i < 0` or `i >= N`.

Arrays of the same type and size support `==` and `!=` (elementwise comparison).

## Structs

Structs are named nominal types; two structs with the same fields but different names
are distinct types. Fields are accessed with `.`. Struct init syntax:
`Point { x: 1, y: 2 }` or shorthand `Point { x, y }` when local variables `x` and
`y` exist.

Structs are values; assigning a struct copies all fields.

## Algebraic data types (ADTs) / enums

An `enum` declares a set of named variants, each with an optional payload:

```elio
enum Shape {
    Circle(flo64),
    Rect(flo64, flo64),
    Unit,
}
```

Constructing a variant: `Circle(3.0)`. Zero-payload variants are plain names: `Unit`.

An enum value's tag and payload are read through `match`. Direct field access on
enum values is not supported.

## Pattern matching

`match` matches a scrutinee against a sequence of arms in source order. The first
arm whose pattern matches is taken; subsequent arms are not evaluated.

**Exhaustiveness**: the typechecker requires every possible value to be handled by at
least one arm. A `_` wildcard or identifier binding covers all remaining cases.
Missing variants are a compile error.

**Guards**: `arm_pattern if cond => body`. A guarded arm is not counted toward
exhaustiveness; the guard must be a `bool` expression evaluated after the pattern
matches.

**Nested patterns**: patterns may be nested arbitrarily deep in payload positions.

**Binding**: an identifier in a pattern position binds that position's value as an
immutable local in the arm body.

## Control flow

- `break;` exits the innermost loop.
- `break expr;` exits the innermost loop and the loop evaluates to `expr`.
- `break with label;` exits the loop named `label`.
- `continue;` / `continue label;` jump to the next iteration.
- `return expr;` exits the enclosing function.

Loop labels are written as `label: while cond { ... }`.

## Runtime errors

On any runtime error the program prints to stdout:
```
runtime error: <message> at line <N>
```
and exits with code 1. Guaranteed runtime checks:
- Division by zero (integer `/` and `%`)
- Array index out of bounds
- `panic(msg)` -- user-triggered abort
- `assert(cond)` -- aborts with `assertion failed` if `cond` is false

## Builtin functions

| Name | Signature | Behaviour |
|------|-----------|-----------|
| `print(x)` | any printable type | Prints `x` followed by newline |
| `input()` | `-> string` | Reads one line from stdin |
| `exit(code)` | `int32 -> unit` | Exits with the given code |
| `panic(msg)` | `string -> unit` | Runtime error with `msg` |
| `assert(cond)` | `bool -> unit` | Aborts if `cond` is false |
| `sign(x)` | integer -> signed | Reinterprets unsigned as signed of same width |
| `unsign(x)` | integer -> unsigned | Reinterprets signed as unsigned of same width |
| `trunc_cast[T](x)` | numeric -> T | Truncating cast |
| `check_cast[T](x)` | numeric -> T | Runtime-checked narrowing cast |
| `wrap_add(a,b)` | integer -> integer | Wrapping addition |
| `wrap_sub(a,b)` | integer -> integer | Wrapping subtraction |
| `wrap_mul(a,b)` | integer -> integer | Wrapping multiplication |

## Determinism principle

Elio guarantees that the meaning of every expression is uniquely determined by its
text and the declared types of its sub-expressions, with no dependence on resolution
order or preference rules. Concretely:

- Overload resolution requires an **exact match** by default (A.2.8).
- When implicit widening is allowed (A.3.1), it is permitted only if exactly one
  candidate is reachable by the lossless widening lattice. Two or more reachable
  candidates is a hard compile error, not a preference tie-break.
- There are no implicit conversions in any context other than overload sites.

## Memory management

Local variables are stack-allocated and freed when their enclosing function returns.
String concatenation allocates on the heap via `malloc`; Elio does not currently free
these allocations (arena model: the OS reclaims them on process exit). This is a
documented limitation.
