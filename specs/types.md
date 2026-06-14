# Elio Type System

## Overview

Elio is **statically typed** and **nominally typed**: two types are compatible only if
they have the same name (or one is explicitly cast to the other). There is no structural
subtyping.

The type system is **strict**: implicit conversions are forbidden in all contexts except
the specific overload-resolution rule described in the overloading section.

## Primitive types

### Integer types

| Type | Width | Range |
|------|-------|-------|
| `int8`   | 8-bit signed   | -128 .. 127 |
| `int16`  | 16-bit signed  | -32768 .. 32767 |
| `int32`  | 32-bit signed  | -2^31 .. 2^31-1 |
| `int64`  | 64-bit signed  | -2^63 .. 2^63-1 |
| `uint8`  | 8-bit unsigned | 0 .. 255 |
| `uint16` | 16-bit unsigned| 0 .. 65535 |
| `uint32` | 32-bit unsigned| 0 .. 2^32-1 |
| `uint64` | 64-bit unsigned| 0 .. 2^64-1 |

Integer literals without a suffix are typed by context. When no context is available
(e.g. a standalone expression), the literal defaults to `int32`.

Supported operations: `+`, `-`, `*`, `/`, `%`, `&`, `|`, `^`, `~`, `<<`, `>>`,
comparisons (`<` `<=` `>` `>=` `==` `!=`), compound assignments.

### Float types

| Type | Standard |
|------|----------|
| `flo32` | IEEE 754 single precision |
| `flo64` | IEEE 754 double precision |

Float literals default to `flo64` when unambiguous. Supported operations: `+`, `-`,
`*`, `/`, comparisons. Bitwise and shift operators are not defined on floats.

Float literals support exponential notation: `1.5e10`, `2.71828e-3`.

### Boolean

`bool` has values `true` and `false`. Supported operations: `&&`, `||`, `!`, `==`, `!=`.
Integers are **not** implicitly usable as booleans.

### Character

`char` is a 32-bit Unicode codepoint (represented as `i32` in LLVM IR). Supported
operations: `==`, `!=`, `<`, `<=`, `>`, `>=`. Character literals use single quotes
with optional escape sequences.

### String types

| Type | Description |
|------|-------------|
| `string`      | Owned UTF-8 string. Runtime-allocated fat pointer `{ i8* ptr, i64 len }`. |
| `string_view` | Borrowed view into a string; same runtime layout, no ownership. |
| `buf_string`  | Mutable string buffer (same layout; mutability tracked by borrow checker). |

All three share the same LLVM struct type `%string = { ptr, i64 }`. The distinction
is in mutability and ownership semantics, enforced by the type checker.

String length (`len`) is the **byte count** of the UTF-8 encoding, not the codepoint
count. This is the documented meaning of length in Elio.

### Unit

`unit` is the return type of functions that produce no value. It has exactly one value,
which is not representable in source code. A function declared `-> unit` with no
`return` statement implicitly returns unit.

## Compound types

### Fixed-size arrays

`[T; N]` is an array of exactly N elements of type T. N must be a compile-time integer
constant greater than zero. Arrays are value types; the size is part of the type, so
`[int32; 3]` and `[int32; 4]` are distinct types.

Supported operations: index (`a[i]`), element-wise `==` and `!=`, pass by value.

### Structs

```elio
struct Point { x: int32, y: int32 }
```

Struct types are nominal. Two structs with the same field names and types are still
distinct if their names differ. Field access: `p.x`. Struct init: `Point { x: 1, y: 2 }`.

### Algebraic data types

```elio
enum Tree { Leaf(int32), Branch(int32, int32), Empty }
```

Each variant may carry zero or more payload values. The enum type is nominal. A value
of an enum type is created by calling the variant name as a function: `Leaf(42)`,
`Empty`. The tag and payload are accessed exclusively through `match`.

### Optional

`optional[T]` is the built-in nullable type. Values: `some(x)` (a T) and `none`.
Match patterns: `some(x)` and `none`. There is no implicit null in Elio; `optional`
must be unwrapped explicitly.

### Result

`result[T, E]` carries either `ok(value: T)` or `err(error: E)`. Match patterns:
`ok(v)` and `err(e)`.

### Reference types

`&T` (borrow reference) and `&mut T` (mutable borrow reference) are pointers to an
existing allocation. They are not heap-allocated. Address-of: `&x` yields `&T`.
Dereference: `*r` yields `T`.

### Shared / weak pointers

`shared[T]` and `weak[T]` are ARC (automatic reference-counted) pointers. These are
reserved in the type grammar but full ARC semantics are not yet implemented in v1.

## Type inference

The `:=` form of `let` infers the variable's type from the initialiser:

```elio
let x := 42;          // x : int32
let s := "hello";     // s : string
```

Functions do **not** infer their return type; a return type annotation is required.

## Type aliases

```ebnf
type alias = "type" Name "=" type ";"
```

Aliases are transparent: `type MyInt = int32` makes `MyInt` interchangeable with
`int32` in all contexts. Aliases do not create new nominal types.

## Casting

Elio has no implicit casts in general code. Explicit casts:

| Builtin | Semantics |
|---------|-----------|
| `sign(x)` | Reinterpret unsigned integer as signed of same width |
| `unsign(x)` | Reinterpret signed integer as unsigned of same width |
| `trunc_cast[T](x)` | Truncate x to T (no runtime check) |
| `check_cast[T](x)` | Narrowing cast; runtime error if value does not fit |

## Overload resolution

When a function is overloaded (multiple definitions with the same name), the call site
is resolved in two phases:

1. **Exact match**: find all overloads where every argument type equals the parameter
   type exactly. If exactly one such overload exists, it wins.
2. **Widening promotion** (A.3.1): if phase 1 found nothing, find all overloads where
   every argument can be **losslessly widened** to the parameter type.

The widening lattice is closed and strict:
- Signed integers: `int8 < int16 < int32 < int64` (same signedness only)
- Unsigned integers: `uint8 < uint16 < uint32 < uint64`
- Floats: `flo32 < flo64`
- No cross-signedness promotion (e.g., `int32` does not promote to `uint64`)
- No integer-to-float promotion

If phase 2 finds exactly one overload, it wins and the arguments are widened at the
call site. If zero or more than one overload is viable after both phases, the call is
a compile error (ambiguous or no matching overload). This guarantees deterministic
resolution with no preference rules.

## Type compatibility rules

Two types T and U are **equal** (interchangeable without cast) when:
- Both are the same primitive type
- Both are `[T; N]` with equal element type and equal N
- Both are nominal types (struct, enum) with the same name
- Both are `optional[T]` with equal T (and similarly for `result`, `shared`, `weak`, `&`, `&mut`)
- One is a type alias of the other

## Mutability

Mutability is a property of **bindings**, not types. `let x: int32 = 5` is an
immutable binding. `let mut x: int32 = 5` is mutable. The type of both is `int32`.

A `&mut T` reference carries mutability through a pointer: the pointee may be
modified through the reference. A `&T` reference is read-only.
