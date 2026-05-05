## Basic Specifications

The goal of the language is to be as predictable and safe as possible. This specification is a work in progress. 

---
1. Safety law
2. Determinism law
3. Simplicity law
Safety >= Determinism >> Simplicity > Ease of implementation

---
## Types

# Primitive types:
1. bool:
* Value: true, false
* Conversion: no implicit
* Equality: yes
* Ordering: no

2. unit:
* Value: none
* Equality: always true

3. Integer:
* Types: int8, int16, int32, int64
* Signed: yes
* Overflow: trap on overflow
* Wrapping: wrap+()
* Division by zero: trap
* Conversion: always explicit

* Types: uint8, uint16, uint32, uint64
* Signed: no
* Overflow: trap on overflow
* Wrapping: wrap+()
* Division by zero: trap
* Conversion: always explicit

* sign()
* unsign()
* trunc_cast[<cast_to>](cast)
* check_cast[<cast_to>](cast)

4. Floating point:
* Types: flo32, flo64
* Standard: IEEE 754
* Overflow: no trap on overflow
* 

5. Char:
* Unicode Scalar Value (Whatever that means)

# Resource Types:

1. String:
* Encoding: UTF-8
* Ownership:
* Mutation: no
* Copy:
* Equality:
* Lenght:
* Indexing:
* Slicing:
* Concatenation:
* Interpolation:
* Destruction:

2. String_view:
* Borrowed string slice

3. Buf_string:
* Encoding: UTF-8
* Ownership:
* Mutation: yes
* Copy:
* Equality:
* Lenght:
* Indexing:
* Slicing:
* Concatenation:
* Interpolation:
* Destruction:

4. Optional[T]:
* Value: Unit/T
* Comparison: On T
* Copy: On T
* Pattern Matching
* Destruction: 

5. Result[T, E]:
* Value: T/Error E
* Comparison: On T && E

6. Shared[T]:

7. Weak[T]:

# Control Flow:
* if else if else - only bool condition
* while - only bool condition. condition calculated prior to each iteration. has a tag
* for - expects value from an iterator.
* return - always exit the current function
* continue
* break - can return value, exit the current loop. can break upon a tag and exit nested loops
* match - 
* loop - infinite loop.



# Expressions
* Comments: // till end of line
* Lambda () -> { return; } or () -> ; (can be placed anywhere) (capture immutable variable by copy)
* if expression

# Arithmetic
* trap on overflow
* division by zero
* require explicit conversion
* all comparison explicit

# Declarations
* : - any declaration
# Containers:
* Structs:
** Syntax: <type> name {fields}
** 

* Arrays:

# Functions:
* Syntax: <return_type> name(input parameters);
* Overload: no (v1)
* Return: explicit
* Parameter passing:
** value types by copy
** borrowed: &T, &mut T

## Medium Specifications

# Objects
** Classes:

** Inheritance:


** auto creation and destruction
# Special expressions
* String Interpolation
# Special definitions

# Runtime traps:
1. Integer overflow
2. Division by zero
3. Out-of-bounds
4. Cast failure
5. Unwrap failure
6. 

---
## Extra Specifications

# Memory model:
* Heap:
* Stack:
* ARC, ownership, reference counting, borrow check (tracing garbage collection?)

# Stack by default
* Local values live on the stack when possible
Simple structs, numbers, bools, optionals/results of small values, etc.

# 2. Owned heap objects
* Heap objects have exactly one owner by default
* Assignment either: moves ownership, or clones explicitly

# 3. ARC for shared heap objects
* If something must be shared, wrap it in shared[T] or similar
* Shared objects use atomic reference counting
* Thread-local shared objects can use non-atomic RC if you want a nicer optimization later

# 4. Borrowed references for function calls
* Allow temporary references like:
** immutable borrow &T
** mutable borrow &mut T
* But enforce them with simple lexical rules, not full lifetime inference

# Values come in two broad categories
# A. Value types

* Examples:

** int, float, bool, char
** maybe small structs
** optional[T] and result[T, E] when T and E are value types

* These are copied normally or moved cheaply.

# B. Resource / heap types

* Examples:

** string
** buf_string
** arrays
** class instances
** large structs

* These have ownership rules.

# Thread-local by default

* Normal variables and objects belong to one thread.

# Explicitly shared objects

* Only values wrapped in shared[T] may cross thread boundaries.

* Example conceptually:
    x : shared[Buffer] = shared_new(Buffer(...));

* Internally:

** shared[T] uses atomic reference counting
** mutable shared access requires:
** locks, or message passing, or explicit synchronized container types

* when a local owned variable goes out of scope, it is destroyed immediately
* destruction order is reverse declaration order
* shared/ARC values decrement refcount at scope end
* when count reaches zero, destroy immediately

# Core memory features
stack locals
heap allocation for dynamic objects
move semantics
explicit clone
automatic destruction at scope exit
immutable and mutable borrows for function parameters
thread-local default
explicit shared[T] with ARC
maybe weak[T]
# Restrictions for sanity
no storing borrows in objects
no returning borrowed references except trivial cases
no implicit copies of heap objects
no shared mutation without mutex[T] or similar wrapper

# Notes:

* Variables thread local by default (any shares explicitly declared)
* 

## To be decided upon:

Does the language have OS threads?
Does it have tasks/green threads?
Are threads in the standard library or built into the language?
Can closures run on another thread?
What exactly counts as “crossing thread boundaries”?
Are globals shared by default or thread-local too?
Is initialization of global state deterministic?

Inheritance complicates:

object layout
destruction order
method dispatch
copying/cloning
ownership of base and derived parts
subtyping rules
whether a derived object can be passed as base by value or only by reference
whether destructors are virtual
whether slicing is allowed
You must decide:
Are classes reference types only, or can they be value types?
Is inheritance implementation inheritance, interface inheritance, or both?
Are methods dynamically dispatched by default?
Are destructors polymorphic?
Can class instances live on stack, heap, or both?
Can classes be copied?
If yes, what does copying mean for owned fields?

memory management rules
You need to define what happens on:
assignment
passing argument to function
returning value
binding a variable
assigning to field
putting values into arrays
pattern matching or destructuring, if you add it later
method calls
closure capture
branching
loops

borrow model:
You need to decide:

When does a borrow begin and end exactly?
Can you borrow fields separately?
Can you take two immutable borrows at once?
Can you take mutable borrow if immutable borrow exists?
Does borrow end at last use or only end-of-scope?
Can borrowed values be returned?
Can they be captured in lambdas?
Can you borrow array elements?
Can you mutate through &mut T only?
Can methods implicitly borrow self?

You must decide:
Can user-defined destructors exist?
If yes, can destructors throw?
If yes, what happens during stack unwinding?
What happens if destructor touches already partially-destroyed object graph?
Is destruction guaranteed on all exits:
normal return
break
continue
error/trap
Does the language even have exceptions?
If a program traps on overflow/div-by-zero, do destructors run?

Missing decisions:
Is int fixed-width? 32-bit? 64-bit? platform-dependent?
Are float and double IEEE-754?
Is NaN allowed?
What does “all comparison explicit” mean for floats?
Does char mean Unicode scalar value, code point, code unit, or ASCII?
Is string UTF-8?
Is indexing strings by integer allowed?
Is buf_string a mutable UTF-8 buffer, mutable bytes, or mutable chars?
Are arrays fixed-size, dynamic, or both?
Are arrays value types or heap types?
Are structs nominal or structural?
Can generics be nested arbitrarily?

You need to define:
Does integer overflow trap in all modes, always?
What about unary minus on minimum integer?
Does float overflow trap or produce infinity?
What about NaN?
Are float comparisons allowed?
What is remainder % behavior?
Is integer division truncating toward zero or floor?
Are mixed-type arithmetic operations forbidden entirely?


Missing:
break
continue
return
block expressions?
labeled loops?
pattern matching?
short-circuit boolean ops?
what for iterates over
whether if is a statement, expression, or both
whether loop bodies create scopes
whether variables declared inside branches must be initialized on all paths

when exactly are loop-local values destroyed?
every iteration end?
after break?
after continue?

You need to decide:
Can closures capture mutable variables?
Can they capture owned heap values?
Does capture copy or move?
Are closures values with anonymous class-like environments?
Can closures escape their defining scope?
Can closures be stored in variables/fields?
Can closures be passed across threads?
Are closures always immutable?
Are they stack-only or heap-allocatable?

Questions:
Is string always heap allocated?
Is it reference-counted internally?
Is substring O(1) slice or O(n) copy?
Can string share buffers?
Is buf_string resizable?
Can buf_string be converted to string without copy?
Is buf_string UTF-8 safe under mutation?
Does indexing operate on bytes, code points, or grapheme clusters?

You need to decide:

fixed-size [T; N]?
dynamic array[T]?
both?
are they contiguous?
are they copyable if small?
can they store move-only values?
what happens on resize with non-copyable elements?
is indexing bounds-checked?
does out-of-bounds trap?
can you borrow an element mutably?
can two different elements be mutably borrowed simultaneously?

You need to decide:
Are structs value types and classes reference types?
Or are both just aggregate types with different defaults?
Do methods work on both?
Can structs have destructors?
Can classes be stack-allocated?
Can classes contain move-only fields?
Can structs inherit? probably no
Can classes be compared for equality? by identity or value?

You need to decide:

which types support ==
which types support ordering < <= > >=
can arrays/structs/classes be compared
for classes, is equality by identity or by contents
for strings, lexicographic by Unicode scalar values? by bytes?
can optional[T] compare if T compares
what about result[T,E]
float equality with NaN
whether implicit bool conversion exists in conditions

Error handling model is not defined

You have result[T, E], which suggests explicit error handling. Good.

But you need to decide:

Are there exceptions? probably should be no
Are traps recoverable? probably no
Is result the only recoverable error mechanism?
Do you have syntax sugar like ?
Can constructors fail?
Can destructors fail?
Can allocation fail?

Module / visibility / namespace system is missing

This is not glamorous, but it becomes necessary fast.

You need at least:

how files form modules
namespacing
import syntax
visibility (public/private)
whether classes/structs have member visibility rules

You need to define:

Initialization:
Must every variable be initialized before use? almost certainly yes
Can fields have default values?
Can constructors leave fields uninitialized?
Are arrays zero-initialized?
Are locals default-initialized?
Are globals lazily initialized or eagerly initialized?
Can cyclic initialization happen?