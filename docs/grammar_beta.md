## Basics
    ; : Delimeter
    : : Initialization
    () : Function arguments, regular expression brackets
    {} : Blocks of code

    # Keywords:
        if, else, true, false, for, while, return

## Expressions
    <type> <name> : <expr> ;


---- GRAMMAR ----

list -> list + digit | list - digit | digit
digit -> 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 

call -> id ( optparams ) ;
optparams -> params | e
params -> params, param | param

fdecl -> type id ( optparams ) ; | type id ( optparams ) { stmt } 
optparams -> params | e
params -> params, param | param
param -> type id

vardecl -> type id : stmt ; | type id : { stmt }

ifstmt -> if ( tfstmt ) { stmt } cont 
cont -> else ifstmt | else { stmt } | e


