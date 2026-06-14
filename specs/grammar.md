# Elio Grammar

Notation: EBNF.
`{ x }` = zero or more repetitions of x.
`[ x ]` = optional x.
`x | y` = alternation.
Terminal strings are quoted. Token names in UPPER_CASE.

---

## Top level

```ebnf
program     = { item } EOF ;

item        = fn_decl
            | struct_decl
            | enum_decl
            | type_alias
            | namespace_decl
            | let_decl
            | const_decl ;

fn_decl     = "fn" IDENT "(" [ param { "," param } ] ")" "->" type
              ( block | ";" ) ;

param       = IDENT ":" type ;

struct_decl = "struct" IDENT "{" [ field_decl { "," field_decl } ] "}" ;
field_decl  = IDENT ":" type ;

enum_decl   = "enum" IDENT "{" [ enum_variant { "," enum_variant } ] "}" ;
enum_variant = IDENT [ "(" type { "," type } ")" ] ;

type_alias  = "type" IDENT "=" type ";" ;

namespace_decl = "namespace" IDENT "{" { item } "}" ;

let_decl    = "let" pattern ( ":=" expr | ":" type "=" expr ) ";" ;
const_decl  = "const" IDENT ":" type "=" expr ";" ;
```

---

## Types

```ebnf
type        = "&" [ "mut" ] type               (* borrow reference / mutable reference *)
            | "[" type ";" INT_LIT "]"         (* fixed-size array *)
            | "optional" "[" type "]"
            | "result"   "[" type "," type "]"
            | "shared"   "[" type "]"          (* ARC shared pointer *)
            | "weak"     "[" type "]"          (* ARC weak pointer *)
            | primitive_type
            | IDENT ;                          (* named / alias type *)

primitive_type
            = "bool" | "unit" | "char" | "string" | "string_view" | "buf_string"
            | "int8"  | "int16"  | "int32"  | "int64"
            | "uint8" | "uint16" | "uint32" | "uint64"
            | "flo32" | "flo64" ;
```

---

## Statements

```ebnf
block       = "{" { statement } "}" ;

statement   = let_decl
            | const_decl
            | if_stmt
            | while_stmt
            | for_stmt
            | loop_stmt
            | match_stmt
            | return_stmt
            | break_stmt
            | continue_stmt
            | block
            | tagged_stmt
            | expr_stmt ;

tagged_stmt = IDENT ":" ( while_stmt | for_stmt | loop_stmt ) ;

if_stmt     = "if" expr block [ "else" ( if_stmt | block ) ] ;

while_stmt  = "while" expr block ;

for_stmt    = "for" pattern "in" expr block ;

loop_stmt   = "loop" block ;

match_stmt  = "match" expr "{" { match_arm } "}" ;

return_stmt = "return" [ expr ] ";" ;

break_stmt  = "break" [ "with" IDENT [ expr ] | expr ] ";" ;

continue_stmt = "continue" [ IDENT ] ";" ;

expr_stmt   = expr ";" ;
```

---

## Match arms and patterns

```ebnf
match_arm   = pattern [ "if" expr ] "=>" ( block | expr "," ) ;

pattern     = "_"                                        (* wildcard *)
            | "true" | "false"                           (* bool literals *)
            | "none"                                     (* none literal *)
            | INT_LIT | FLOAT_LIT | CHAR_LIT | STR_LIT  (* literal pattern *)
            | "some" "(" pattern ")"
            | "ok"   "(" pattern ")"
            | "err"  "(" pattern ")"
            | "(" { pattern "," } ")"                   (* tuple pattern *)
            | IDENT "{" IDENT [ ":" pattern ]
                    { "," IDENT [ ":" pattern ] } "}"   (* struct pattern *)
            | IDENT "(" { pattern { "," pattern } } ")" (* enum variant with payload *)
            | IDENT ;                                   (* binding or zero-payload variant *)
```

---

## Expressions

Precedence, lowest to highest:

```ebnf
expr        = assignment ;

assignment  = "if"    if_expr
            | "match" match_expr
            | lambda_expr
            | logic_or assign_op assignment
            | logic_or ;

assign_op   = "=" | "+=" | "-=" | "*=" | "/=" | "%="
            | "&=" | "|=" | "^=" | "<<=" | ">>=" ;

if_expr     = expr block [ "else" expr ] ;

match_expr  = expr "{" { match_arm } "}" ;

lambda_expr = "(" [ param { "," param } ] ")" "->" ( block | expr ";" ) ;

logic_or    = logic_and { "||" logic_and } ;

logic_and   = equality  { "&&" equality  } ;

equality    = comparison { ( "==" | "!=" ) comparison } ;

comparison  = bitwise_or { ( "<" | "<=" | ">" | ">=" ) bitwise_or } ;

bitwise_or  = bitwise_xor { "|" bitwise_xor } ;

bitwise_xor = bitwise_and { "^" bitwise_and } ;

bitwise_and = shift      { "&" shift      } ;

shift       = additive   { ( "<<" | ">>" ) additive } ;

additive    = multiplicative { ( "+" | "-" ) multiplicative } ;

multiplicative = unary   { ( "*" | "/" | "%" ) unary } ;

unary       = ( "!" | "-" | "*" | "~" ) unary
            | postfix ;

postfix     = primary
            { "(" [ expr { "," expr } ] ")"         (* call *)
            | "[" expr "]"                           (* index *)
            | "." IDENT                              (* field access *)
            | "." IDENT "(" [ expr { "," expr } ] ")" (* method call *)
            | "{" [ field_init { "," field_init } ] "}"  (* struct init, uppercase only *)
            } ;

field_init  = IDENT [ ":" expr ] ;

primary     = literal
            | builtin
            | "[" [ expr { "," expr } ] "]"         (* array literal *)
            | IDENT
            | "(" expr ")"
            | block ;

literal     = INT_LIT | FLOAT_LIT | CHAR_LIT | STR_LIT | "true" | "false" | "none" ;

builtin     = "sign"       "(" expr ")"
            | "unsign"     "(" expr ")"
            | "trunc_cast" "[" type "]" "(" expr ")"
            | "check_cast" "[" type "]" "(" expr ")"
            | "wrap_add"   "(" expr "," expr ")"
            | "wrap_sub"   "(" expr "," expr ")"
            | "wrap_mul"   "(" expr "," expr ")" ;
```

---

## Lexical rules

```ebnf
INT_LIT     = [0-9]+
            | "0x" [0-9a-fA-F]+
            | "0b" [01]+ ;

FLOAT_LIT   = [0-9]+ "." [0-9]+
              [ ("e" | "E") [ "+" | "-" ] [0-9]+ ] ;

CHAR_LIT    = "'" ( char_body | escape ) "'" ;
char_body   = <any byte except '\'' and '\\'> ;

STR_LIT     = '"' { char_body_s | escape } '"' ;
char_body_s = <any byte except '"' and '\\'> ;

escape      = "\\" ( "n" | "t" | "r" | "0" | "\\" | '"' | "'" ) ;

IDENT       = ( LETTER | "_" ) { LETTER | DIGIT | "_" } ;
LETTER      = [a-zA-Z] ;
DIGIT       = [0-9] ;

COMMENT_LINE  = "//" { <any> } NEWLINE ;
COMMENT_BLOCK = "/*" { COMMENT_BLOCK | <any except "/*" and "*/"> } "*/" ;
```

Block comments nest: `/* outer /* inner */ still outer */` is valid.
Comments do not appear in the token stream.

---

## Keywords (reserved; cannot be used as identifiers)

```
if else while for loop return break continue match with in
fn struct enum type let const mut namespace
true false none some ok err
optional result shared weak
sign unsign trunc_cast check_cast wrap_add wrap_sub wrap_mul
bool unit char string string_view buf_string
int8 int16 int32 int64 uint8 uint16 uint32 uint64 flo32 flo64
```
