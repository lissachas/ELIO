You do **not** start by saying “my language has `int`, `float`, `string`.” That is how people accidentally reinvent a haunted house with semicolons.

For a language built around **predictability and safety**, the real first step is this:

## 1. Define the semantic contract of every type

For each core type, you need to state:

1. **What values it can represent**
2. **Its memory/layout model**
3. **Whether it is a value type or resource type**
4. **How it is copied, moved, cloned, and destroyed**
5. **Which operations are valid**
6. **What can trap at runtime**
7. **What conversions exist**
8. **What equality and ordering mean**
9. **Whether its behavior is platform-dependent**
10. **Whether it is thread-sendable / shareable**

That is the real type system spec. The names are the easy part.

---

# What fits your philosophy best

Given your priorities:

**Safety >= Determinism >> Simplicity > Ease of implementation**

the best fit is:

* **fixed-width primitive numerics**
* **no implicit conversions**
* **move-by-default for resource types**
* **copy-by-default only for small value types**
* **immutable by default**
* **explicit sharing**
* **immediate destruction at scope exit**
* **no UB anywhere in safe code**
* **strictly specified runtime traps**
* **very limited magic**

That means you should lean more toward:

* Rust’s ownership discipline
* C’s directness
* C++’s RAII
* but with much less surface chaos than any of them

A decent slogan would be:

> “Every value has obvious ownership, obvious lifetime, obvious failure modes, and obvious runtime cost.”

That is the kind of thing people claim C++ does right before template metaprogramming eats their house.

---

# The first thing I would change in your basic types

Do **not** make `int` your main integer type.

If you care about determinism, `int` is too vague unless you define it very aggressively.

## Better numeric core

Use fixed-width integers:

* `i8 i16 i32 i64`
* `u8 u16 u32 u64`

Optional:

* `isize usize` only for indexing / platform-sized operations

Then optionally define:

* `int = i64` as a convenience alias
* but make it very clear this is a language alias, not “machine int”

This avoids one of the oldest stupidities in systems languages: code that silently changes behavior because the machine shape changed.

---

# Recommended basic type set

This is what I would recommend for your philosophy.

## Value types

* `bool`
* `unit`
* `i8 i16 i32 i64`
* `u8 u16 u32 u64`
* `f32 f64` or maybe delay them
* `byte` as alias for `u8`
* `char` only if very clearly defined
* `optional[T]`
* `result[T, E]`
* fixed arrays: `[T; N]`

## Resource / owned types

* `string` immutable
* `buf_string` mutable
* dynamic array, probably `array[T]` or `vec[T]`
* `struct`
* class/object only if you really need them
* `shared[T]`
* maybe `weak[T]`
* maybe `box[T]` or some owned heap wrapper if heap placement should be explicit

---

# The most important design question: what exactly is a type category?

You already have the right instinct with value vs resource types.

Make it formal.

## Category A: Plain value types

Properties:

* copied by assignment
* no destructor
* no heap ownership
* can live on stack trivially
* equality/order fully structural
* predictable layout

Examples:

* integers
* bool
* unit
* maybe `f32`, `f64`
* maybe `char`
* maybe small structs composed only of value types
* maybe `optional[T]` / `result[T,E]` when all members are value types

## Category B: Resource types

Properties:

* moved by assignment
* explicit clone required
* have destruction semantics
* may own heap memory or external resources
* cannot be implicitly copied

Examples:

* `string`
* `buf_string`
* dynamic arrays
* heap-owning structs
* class instances
* `shared[T]`

This split should be visible in the language model, not just a compiler implementation detail.

---

# What you need to specify for each primitive type

I’ll go one by one.

---

## `bool`

State:

* values are exactly `true` and `false`
* no implicit conversion to or from integers
* operators: `!`, `&&`, `||`
* equality allowed
* ordering either forbidden or defined explicitly
* short-circuit semantics for boolean operators

Recommendation:

* no `0 -> false`, `1 -> true` nonsense
* no arithmetic on `bool`

That trash belongs in C, along with several historical decisions.

---

## `unit`

State:

* exactly one value: `()`
* used for “returns nothing”
* can appear in generics
* equality always true

Recommendation:

* make it a first-class type, not syntax sugar
* this helps unify expressions and functions cleanly

---

## Integers

You need to define:

1. widths
2. signedness
3. literal typing rules
4. overflow behavior
5. division/remainder rules
6. shift rules
7. conversions
8. comparisons
9. bitwise ops
10. formatting/parsing

## Strong recommendation

Use:

* signed: `i8 i16 i32 i64`
* unsigned: `u8 u16 u32 u64`
* optional: `isize usize`

### Overflow

You already want:

* **trap on overflow**

Good. Keep that for normal arithmetic.

Then offer explicit alternative families:

* wrapping add/sub/mul
* saturating add/sub/mul
* checked operations returning `optional` or `result`

Example:

* `a + b` traps on overflow
* `a.wrapping_add(b)`
* `a.checked_add(b) -> optional[T]`
* `a.saturating_add(b)`

That is a very good design for predictability.

### Division and remainder

Specify:

* division by zero traps
* `MIN / -1` for signed types traps if overflow would occur
* remainder sign convention must be defined

### Shifts

Specify very clearly:

* negative shift counts are invalid and trap
* shift count >= bit width traps, or is masked, but choose one
* I strongly recommend **trap**, because “masking” is classic surprise-fuel

### Conversions

You already want explicit conversion. Good.

Specify:

* widening integer conversions are explicit or maybe safe-implicit only in literals
* narrowing conversions require explicit checked cast or explicit truncating cast
* signed/unsigned conversion always explicit

I recommend **two cast forms**:

* checked cast: traps or returns result
* truncating cast: explicit and ugly-looking on purpose

Example conceptually:

* `x as i32` only if guaranteed safe, or maybe forbidden
* `truncate[i32](x)`
* `checked_cast[i32](x)`

The uglier unsafe-ish cast is, the fewer disasters humans create before lunch.

---

## Floating-point types

This is the dangerous one for your philosophy.

If you care about determinism, floats are where the smiling lies begin.

## You have three options

### Option A. Keep `f32`, `f64`, but make them strict

This is the most practical option.

Then specify:

* IEEE 754 binary32 / binary64
* round-to-nearest-even
* no fast-math
* no reassociation by optimizer
* NaN behavior clearly defined
* signed zero behavior preserved
* infinities allowed or not allowed
* division by zero behavior specified
* overflow behavior specified
* whether FP exceptions trap or not

#### My recommendation

For a predictable systems language:

* use IEEE 754
* no fast-math in safe/default mode
* preserve NaN, infinity, signed zero
* arithmetic does **not** trap on FP overflow by default
* comparisons follow IEEE rules
* `NaN != NaN`
* ordinary `< <= > >=` with NaN produce false as per IEEE
* provide explicit total ordering function for maps/sorts if needed

Because trapping FP overflow sounds “safe” until numerical code becomes a minefield of random explosions.

### Option B. Defer floats from the first stable core

This is actually very reasonable.
If the language is not yet numerics-focused, say:

* floating point is provisional
* strict FP spec comes later
* integer semantics are fully defined first

### Option C. Separate deterministic float modes

You can have:

* `f32`, `f64` for IEEE floats
* maybe later `dec32`, `dec64` or exact rational / decimal libraries
  Not needed now.

## My actual advice

Keep `f32` and `f64`, but explicitly mark them as:

* **predictable but not mathematically exact**
* **deterministic only under strict FP mode**
* **not suitable for equality-heavy logic unless deliberate**

---

## `char`

This is a trap unless you define it hard.

You need to choose one of these:

### Choice 1: `char` = Unicode scalar value

Like Rust.

* fixed-width code point type
* not a grapheme
* not a UTF-8 byte
* not “one visible character”

This is good, but many users misunderstand it.

### Choice 2: `char` = ASCII byte

Very simple, very predictable, very C-like.
But weak for modern text.

### Best recommendation for your philosophy

Have **both**:

* `byte` = `u8`
* `char` = Unicode scalar value

Then say:

* strings are UTF-8
* `char` is not equal to one displayed glyph
* indexing a string by character is not O(1), so either forbid it or make it explicit

This avoids the eternal comedy of pretending human text is simple.

---

## `string`

This one needs a lot of definition.

You said immutable. Good.

State all of this explicitly:

* encoding: UTF-8 or not?
* ownership: owns its contents?
* mutation: impossible in place
* copy semantics: move-only or reference-counted under the hood?
* equality: bytewise? Unicode-normalized? case-sensitive?
* length: bytes, code points, graphemes?
* indexing rules
* slicing rules
* concatenation cost
* interpolation rules
* destruction semantics

## Best fit for your philosophy

### Recommendation

Make `string`:

* immutable
* owned
* UTF-8 encoded
* move-only as a resource type
* explicit clone required
* equality is exact byte equality
* length returns **byte length**
* character iteration explicit
* direct integer indexing forbidden or byte-only through explicit API
* slicing only if boundaries are valid UTF-8 boundaries, otherwise trap or return result

This is predictable and honest.

Do **not** make string indexing silently mean “Unicode character number.” That road leads to performance lies and tears.

You may also want:

* `strview` or `string_view` as a borrowed string slice
* only usable under your borrow rules
* not storable in heap objects if you keep your “no stored borrows” restriction

That would be a very sane design.

---

## `buf_string`

This should be clearly distinct from `string`.

State:

* mutable UTF-8 text buffer
* owned resource type
* append/insert/remove allowed
* explicit conversion to immutable `string`
* equality maybe same as `string`
* may have extra capacity
* move-only, explicit clone
* borrow rules for access

Possible relation:

* `string` for public interfaces and stable values
* `buf_string` for building/editing text efficiently

That split is good. Keep it.

---

## `optional[T]`

Define it as a real tagged sum type, not compiler fairy dust.

State:

* values are `none` or `some(T)`
* layout maybe optimized but semantics fixed
* comparison rules only when `T` is comparable
* copy/move behavior follows `T`
* pattern matching / destructuring behavior
* whether implicit unwrap exists

Recommendation:

* no implicit unwrap ever
* no null
* `optional[T]` is the only absence type
* truthiness from optionals should be forbidden

That is clean and safe.

---

## `result[T, E]`

Same story.

State:

* values are `ok(T)` or `err(E)`
* explicit propagation syntax if you add one later
* no exceptions in safe core if this is your error path
* copy/move behavior follows contained types
* comparisons only when both members support it

Recommendation:

* make `result` central
* if your language values predictability, avoid hidden stack unwinding for routine errors
* reserve panics/traps for broken invariants, not normal failure

That part aligns very well with your philosophy.

---

# What is missing from your current basic spec

Several important things.

## 1. Literal rules

You need to define:

* integer literal syntax
* float literal syntax
* string literal syntax
* char literal syntax
* escape sequences
* type inference rules for literals
* underscore separators
* hex/binary/octal literals
* suffixes like `123i32`, `1.0f64` or not

Recommendation:

* support suffixes
* unsuffixed integer literals stay polymorphic until constrained
* unsuffixed float literals default to `f64`

That is practical and familiar.

---

## 2. Equality and ordering policy

You wrote “all comparison explicit,” but this needs a precise meaning.

Define:

* whether equality is allowed only for same-type operands
* whether ordering is allowed only for numeric types
* whether strings are orderable lexicographically
* whether structs get derived equality/order
* whether floats use partial order only

Recommendation:

* no cross-type comparison
* no implicit numeric promotion for comparison
* `1 == 1u32` should be illegal without explicit conversion
* floats get equality, but users must live with NaN semantics
* total order for floats only via explicit method

---

## 3. Type inference rules

You need to state:

* where inference is allowed
* whether local `x := expr` exists or not
* whether function return types require explicit annotation
* whether generic parameters may be inferred
* when the compiler refuses ambiguity

For your philosophy:

* allow local inference from initializer
* require function signatures explicit
* reject ambiguous literal inference
* reject inference that depends on non-obvious global context

---

## 4. Type traits / capabilities

Even if you do not call them “traits,” you need these concepts:

* `Copy`
* `Move`
* `Clone`
* `Drop` / destructor
* `Eq`
* `Ord`
* `Hash`
* maybe `Send`
* maybe `Sync`

A small capability system will make your language dramatically cleaner.

Example:

* value types can be `Copy`
* resource types are `Move` only unless explicitly `Clone`
* only thread-safe things can become `shared[T]`

This is worth defining early.

---

## 5. Null policy

State it bluntly:

* there is **no null reference/value in safe code**
* absence is represented by `optional[T]`

That one sentence prevents an entire genre of stupidity.

---

## 6. Bottom / never type

Consider adding a `never` type, like `!`.

Useful for:

* trap/panic functions
* infinite loops
* control-flow typing
* exhaustive expressions

This helps expression-oriented design a lot.

---

# My recommended “basic types” spec for your language

Here is the version I would actually build around.

## Core primitive types

### Integers

* `i8 i16 i32 i64`
* `u8 u16 u32 u64`
* optional `isize usize`

Semantics:

* two’s complement
* arithmetic traps on overflow in default mode
* division by zero traps
* explicit conversions only
* no implicit promotions

### Floating point

* `f32 f64`

Semantics:

* IEEE 754
* strict mode by default
* no optimizer reassociation
* NaN/infinity preserved
* no implicit conversion to or from integers
* partial ordering only

### Logical/value primitives

* `bool`
* `unit`
* `never`

### Text/data primitives

* `byte` alias `u8`
* `char` = Unicode scalar value
* `string` = immutable owned UTF-8 text
* `buf_string` = mutable owned UTF-8 text buffer

### Sum types

* `optional[T]`
* `result[T, E]`

### Containers

* `[T; N]` fixed-size array
* `array[T]` or `vec[T]` dynamic owned array
* borrowed slices later: `&[T]`, `&mut [T]`

---

# Very important: `int`, `float`, `double` are not the best names for your core

Since you come from C/C++, those names feel natural. They are also loaded with decades of ambiguity.

## Better choices

Instead of:

* `int`
* `float`
* `double`

prefer:

* `i32/i64`
* `f32`
* `f64`

You can still allow aliases:

* `int = i64`
* `float = f32`
* `double = f64`

But the real spec should be in fixed-width names.

That single choice makes the language more honest.

---

# On classes and inheritance

For your philosophy, classic class inheritance is probably a bad fit.

Why?

Because inheritance tends to reduce predictability through:

* hidden layout complexity
* dynamic dispatch everywhere
* constructor/destructor subtleties
* base/derived slicing issues
* “is-a” hierarchies that collapse under contact with reality

## Better fit

Use:

* `struct` for data
* `impl`/methods for behavior
* `interface`/`trait`-like abstraction for polymorphism
* composition over inheritance

If you really want classes:

* make them explicitly heap-owning reference/resource types
* forbid multiple inheritance
* keep inheritance shallow and interface-oriented

But honestly, for your stated goals, I would **avoid class inheritance in the core language**.

Rust was right to side-eye that whole circus.

---

# On borrows

Your current restriction set is good and very smart for a first language design:

* borrowed refs for calls
* no storing borrows in objects
* no returning borrows except trivial cases
* lexical borrow rules
* no lifetime inference monster

That is excellent.

It gives you:

* real safety
* much lower complexity than Rust
* easier mental model

This is one of the strongest parts of your direction.

---

# The minimum things you should explicitly state right now

If you want to “fully define” basic types, write these sections next:

## 1. Primitive type list

Use exact names and exact widths.

## 2. Type categories

Define value types vs resource types.

## 3. Ownership rules

For each category: copy, move, clone, destroy.

## 4. Arithmetic semantics

Overflow, division, shifts, float behavior.

## 5. Conversion rules

What is implicit, what is explicit, what can trap.

## 6. Equality/ordering rules

Same-type only, float partial ordering, string semantics.

## 7. String model

Encoding, length, indexing, slicing, mutability.

## 8. Generic container semantics

How `optional[T]`, `result[T,E]`, arrays behave depending on `T`.

## 9. Memory and lifetime classification

Stack/heap/shared/borrowed.

## 10. No-UB statement

State clearly whether any safe program can trigger undefined behavior.
For your philosophy, the answer should be: **no**.

---

# A compact spec direction I would recommend

Here is the shape I’d endorse.

```text
Basic Type Principles

1. No undefined behavior in safe code.
2. No implicit numeric conversions.
3. Primitive integer types are fixed-width.
4. Resource-owning types are move-only unless explicitly cloned.
5. Strings are immutable, owned, UTF-8 values.
6. Mutable text uses a separate buffer type.
7. Absence and failure use optional[T] and result[T, E].
8. Shared ownership is explicit through shared[T].
9. Borrowed references are temporary and non-storable.
10. Runtime traps are fully specified and never platform-dependent.
```

And then:

```text
Primitive Types

Value types:
bool, unit, never,
i8 i16 i32 i64,
u8 u16 u32 u64,
f32 f64,
byte, char

Resource types:
string, buf_string, array[T], shared[T]

Generic sum/container types:
optional[T], result[T, E], [T; N]
```

---

# Final verdict

For your philosophy, the best route is:

* **fixed-width numerics**
* **immutable `string` + separate mutable text buffer**
* **`optional`/`result` instead of null/exceptions**
* **value vs resource type split**
* **move semantics for resource types**
* **explicit shared ownership**
* **very restricted borrows**
* **no classic inheritance in the core**
* **strictly specified arithmetic and string semantics**

The biggest correction to your current draft is this:

## Replace

* `int`
* `float`
* `double`

## With

* `i32`, `i64`, maybe `isize`
* `f32`
* `f64`

And formally define:

* `char`
* `string`
* `buf_string`
* `optional`
* `result`

before you touch classes, inheritance, or clever features. Otherwise you will end up with a language whose “core principles” dissolve the moment text, numbers, and ownership appear in the same function. Which, annoyingly, is most functions.

I can turn this into a **clean spec document template** for your language core next.
