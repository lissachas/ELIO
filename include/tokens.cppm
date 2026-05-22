module;

#include <string>
#include <string_view>
#include <variant>

export module tokens;
using Object = std::variant<int, std::string_view, double>;

export enum TokenType {
    COMMA, //,
    DOT, //.
    SEMI, //;
    COLON, //:
    HASH, //#
    LBRKT, //{
    RBRKT, //}
    LPAREN, //(
    RPAREN, //)
    COLON_EQUAL, //:=
    ARROW, //->
    FAT_ARROW, //=>
    AMP, //&
    PIPE, //|
    LSQUARE, //[
    RSQUARE, //]
    AND_AND, //&&
    OR_OR, //||
    PERCENT, //%
    UNDERSCORE, //_

    PLUS, MINUS, STAR, SLASH,

    EQUAL, GREATER, LESS, GREATER_EQUAL, LESS_EQUAL, NOT_EQUAL, BANG, EQUAL_EQUAL, BANG_EQUAL,

    STR, VAL, IDENT,

    //Keywords
    IF, ELSE, WHILE, FOR, RETURN, TRUE, FALSE, NONE,
    FN, STRUCT, LET, CONST, LOOP, MATCH, BREAK, CONTINUE, 
    IN, MUT, SOME, OK, ERR, OPTIONAL, RESULT, SHARED, WEAK,
    SIGN, UNSIGN, TRUNC_CAST, CHECK_CAST, 
    WRAP_ADD, WRAP_SUB, WRAP_MUL, 

    //Primitive types
    BOOL, UNIT, INT8, INT16, INT32, INT64,
    UINT8, UINT16, UINT32, UINT64,
    FLO32, FLO64, CHAR, STRING, STRING_VIEW, BUF_STRING,

    END

};

export class Token {
    private:
    Object literal;
    int line;
    std::string_view value;

    public:
    TokenType type;
    Token() : literal{0}, line{0}, value{""}, type{END} {}
    Token(std::string_view value, Object literal, TokenType type, int line) : literal{literal},  line{line}, value{value}, type{type} {

    }
    std::string_view get_value() const { return value; }
    Object get_literal()         const { return literal; }
    int get_line() const { return line; }

    std::string to_string() {
        return std::to_string(type) + " " + std::string(value);
    }
};