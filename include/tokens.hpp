#pragma once
#include <string>
#include <string_view>
#include <variant>

using Object = std::variant<int, std::string_view, double>;

typedef enum TokenType {
    COMMA, DOT, SEMI, TIR, HASH, LBRKT, RBRKT, LPAREN, RPAREN, 

    PLUS, MINUS, STAR, SLASH,

    EQUAL, GREATER, LESS, GREATER_EQUAL, LESS_EQUAL, NOT_EQUAL, BANG,

    STR, VAL, IDENT,

    //Keywords
    IF, ELSE, WHILE, FOR, RETURN, TRUE, FALSE,

    END

};

class Token {
    private:
    std::string_view value;
    Object literal;
    int line;

    public:
    TokenType type;
    Token(std::string_view value, Object literal, TokenType type, int line) : value{value}, literal{literal}, type{type}, line{line} {

    }

    std::string to_string() {
            return type + " " + (std::string)value;
    }
};