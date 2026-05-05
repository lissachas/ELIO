## Program structure:

<program> ::= <item_list>

<item_list> ::= <item>
              | <item_list> <item>

<item> ::= <function_decl>
         | <struct_decl>
         | <global_decl>
         | <comment>

<comment> ::= "//" <comment_text>

# Declarations:

<global_decl> ::= <let_decl>
                | <const_decl>

<let_decl> ::= <pattern> ":" <type> "=" <expr> ";"
             | <pattern> ":=" <expr> ";"

<const_decl> ::= "const" <identifier> ":" <type> "=" <expr> ";"

# Structs:

<struct_decl> ::= "struct" <identifier> "{" <field_decl_list_opt> "}"

<field_decl_list_opt> ::= <empty>
                        | <field_decl_list>

<field_decl_list> ::= <field_decl>
                    | <field_decl_list> "," <field_decl>

<field_decl> ::= <identifier> ":" <type>

# Functions:

<function_decl> ::= "fn" <identifier> "(" <param_list_opt> ")" "->" <type> <block>
                  | "fn" <identifier> "(" <param_list_opt> ")" "->" <type> ";"

<param_list_opt> ::= <empty>
                   | <param_list>

<param_list> ::= <param>
               | <param_list> "," <param>

<param> ::= <identifier> ":" <type>

# Types:

<type> ::= <primitive_type>
         | <named_type>
         | "&" <type>
         | "&" "mut" <type>
         | "[" <type> ";" <integer_literal> "]"
         | "optional" "[" <type> "]"
         | "result" "[" <type> "," <type> "]"
         | "shared" "[" <type> "]"
         | "weak" "[" <type> "]"

<primitive_type> ::= "bool"
                   | "unit"
                   | "int8" | "int16" | "int32" | "int64"
                   | "uint8" | "uint16" | "uint32" | "uint64"
                   | "flo32" | "flo64"
                   | "char"
                   | "string"
                   | "string_view"
                   | "buf_string"

<named_type> ::= <identifier>

# Blocks and statements:

<block> ::= "{" <stmt_list_opt> "}"

<stmt_list_opt> ::= <empty>
                  | <stmt_list>

<stmt_list> ::= <stmt>
              | <stmt_list> <stmt>

<stmt> ::= <let_decl>
         | <const_decl>
         | <expr_stmt>
         | <if_stmt>
         | <while_stmt>
         | <for_stmt>
         | <loop_stmt>
         | <match_stmt>
         | <return_stmt>
         | <break_stmt>
         | <continue_stmt>
         | <block>
         | <comment>

<expr_stmt> ::= <expr> ";"

## Control flow:

# If:

<if_stmt> ::= "if" <expr> <block> <else_clause_opt>

<else_clause_opt> ::= <empty>
                    | "else" <block>
                    | "else" <if_stmt>

# While:

<while_stmt> ::= <while_tag_opt> "while" <expr> <block>

<while_tag_opt> ::= <empty>
                  | <identifier> ":"

# Loop:

<loop_stmt> ::= <loop_tag_opt> "loop" <block>

<loop_tag_opt> ::= <empty>
                 | <identifier> ":"

# For:

<for_stmt> ::= <for_tag_opt> "for" <pattern> "in" <expr> <block>

<for_tag_opt> ::= <empty>
                | <identifier> ":"

# Return / Back / Continue:

<return_stmt> ::= "return" ";"
                | "return" <expr> ";"

<continue_stmt> ::= "continue" ";"
                  | "continue" <identifier> ";"

<break_stmt> ::= "break" ";"
               | "break" <expr> ";"
               | "break" <identifier> ";"
               | "break" <identifier> <expr> ";"

# Match

<match_stmt> ::= "match" <expr> "{" <match_arm_list_opt> "}"

<match_arm_list_opt> ::= <empty>
                       | <match_arm_list>

<match_arm_list> ::= <match_arm>
                   | <match_arm_list> <match_arm>

<match_arm> ::= <pattern> "=>" <match_arm_body>

<match_arm_body> ::= <expr> ","
                   | <block>

# Patterns:

<pattern> ::= <identifier>
            | "_"
            | "none"
            | "true"
            | "false"
            | <literal>
            | "(" <pattern_list_opt> ")"
            | <identifier> "{" <field_pattern_list_opt> "}"
            | "some" "(" <pattern> ")"
            | "ok" "(" <pattern> ")"
            | "err" "(" <pattern> ")"

<pattern_list_opt> ::= <empty>
                     | <pattern_list>

<pattern_list> ::= <pattern>
                 | <pattern_list> "," <pattern>

<field_pattern_list_opt> ::= <empty>
                           | <field_pattern_list>

<field_pattern_list> ::= <field_pattern>
                       | <field_pattern_list> "," <field_pattern>

<field_pattern> ::= <identifier>
                  | <identifier> ":" <pattern>

# Expressions:

<expr> ::= <assignment_expr>

# Assignment:

<assignment_expr> ::= <if_expr>
                    | <match_expr>
                    | <lambda_expr>
                    | <logic_or_expr>
                    | <unary_expr> "=" <assignment_expr>

# Lambda:

<if_expr> ::= "if" <expr> <block> "else" <expr>

<match_expr> ::= "match" <expr> "{" <match_arm_list_opt> "}"

<lambda_expr> ::= "(" <param_list_opt> ")" "->" <lambda_body>

<lambda_body> ::= <block>
                | <expr> ";"

# Operator precedence:

<logic_or_expr> ::= <logic_and_expr>
                  | <logic_or_expr> "||" <logic_and_expr>

<logic_and_expr> ::= <equality_expr>
                   | <logic_and_expr> "&&" <equality_expr>

<equality_expr> ::= <relational_expr>
                  | <equality_expr> "==" <relational_expr>
                  | <equality_expr> "!=" <relational_expr>

<relational_expr> ::= <additive_expr>
                    | <relational_expr> "<" <additive_expr>
                    | <relational_expr> "<=" <additive_expr>
                    | <relational_expr> ">" <additive_expr>
                    | <relational_expr> ">=" <additive_expr>

<additive_expr> ::= <multiplicative_expr>
                  | <additive_expr> "+" <multiplicative_expr>
                  | <additive_expr> "-" <multiplicative_expr>

<multiplicative_expr> ::= <unary_expr>
                        | <multiplicative_expr> "*" <unary_expr>
                        | <multiplicative_expr> "/" <unary_expr>
                        | <multiplicative_expr> "%" <unary_expr>

# Unary

<unary_expr> ::= <postfix_expr>
               | "-" <unary_expr>
               | "!" <unary_expr>
               | "&" <unary_expr>
               | "&" "mut" <unary_expr>
               | "*" <unary_expr>

# Postfix

<postfix_expr> ::= <primary_expr>
                 | <postfix_expr> "(" <argument_list_opt> ")"
                 | <postfix_expr> "[" <expr> "]"
                 | <postfix_expr> "." <identifier>
                 | <postfix_expr> "." <identifier> "(" <argument_list_opt> ")"

# Primary

<primary_expr> ::= <literal>
                 | <identifier>
                 | "(" <expr> ")"
                 | <block>
                 | <struct_init>
                 | <builtin_cast_expr>

# Struct construction

<struct_init> ::= <identifier> "{" <field_init_list_opt> "}"

<field_init_list_opt> ::= <empty>
                        | <field_init_list>

<field_init_list> ::= <field_init>
                    | <field_init_list> "," <field_init>

<field_init> ::= <identifier> ":" <expr>
               | <identifier>

# Builtin

<builtin_cast_expr> ::= "sign" "(" <expr> ")"
                      | "unsign" "(" <expr> ")"
                      | "trunc_cast" "[" <type> "]" "(" <expr> ")"
                      | "check_cast" "[" <type> "]" "(" <expr> ")"
                      | "wrap_add" "(" <expr> "," <expr> ")"
                      | "wrap_sub" "(" <expr> "," <expr> ")"
                      | "wrap_mul" "(" <expr> "," <expr> ")"

# Arguments

<argument_list_opt> ::= <empty>
                      | <argument_list>

<argument_list> ::= <expr>
                  | <argument_list> "," <expr>

# Literals

<literal> ::= <bool_literal>
            | <unit_literal>
            | <integer_literal>
            | <float_literal>
            | <char_literal>
            | <string_literal>

<bool_literal> ::= "true" | "false"

<unit_literal> ::= "none"

<integer_literal> ::= <digit>
                    | <integer_literal> <digit>

<float_literal> ::= <integer_literal> "." <integer_literal>

<char_literal> ::= "'" <char_content> "'"

<string_literal> ::= "\"" <string_content> "\""

# Identifiers

<identifier> ::= <letter>
               | <identifier> <letter>
               | <identifier> <digit>
               | <identifier> "_"

<letter> ::= "a" | "b" | "c" | ... | "z"
           | "A" | "B" | "C" | ... | "Z"

<digit> ::= "0" | "1" | "2" | "3" | "4" | "5" | "6" | "7" | "8" | "9"

<empty> ::=

