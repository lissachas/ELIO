#pragma once
#include <string>
#include <unordered_map>
#include <string_view>
#include <vector>
#include <variant>

#include "tokens.hpp"
#include "errors/error.hpp"
#include "parser.hpp"

using Object = std::variant<int, std::string_view, double>;


class Lexer {
    public:
        void start(std::string);

    private:
        Token lex();
        void string();
        void number();
        void identifier();
        char advance() {
            return source.at(current++);
        }
        void create_token(TokenType type) {
            create_token(type, "");
        }
        void create_token(TokenType type, Object literal) {
            std::string_view val;
            if (type == STR) val = source.substr(begin + 1, current - 1);
            else val = source.substr(begin, current);

            tokens.push_back(Token(val, literal, type, line));
        }
        bool match(char expected) {
            if (is_at_end()) return false;
            if (source.at(current) != expected) return false;

            current++;
            return true;
        }
        char peek() {
            if (is_at_end()) return '\0';
            return source.at(current);
        }
        bool is_digit(char c) {
            return c >= '0' && c <= '9';
        }
        char peek_next() {
            if (current + 1 >= source.length()) return '\0';
            return source.at(current + 1);
        }
        bool is_alpha(char c) {
            return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
        }
        bool is_alphanum(char c) {
            return is_alpha(c) || is_digit(c);
        }



        //std::unordered_map<std::string_view, TokenType> map {
        //    {"(", LPAREN}, {")", RPAREN}, {"{", LBRKT}, {"}", RBRKT}, {";", SEMI}, {"-", }
        //};

        std::unordered_map<std::string, TokenType> identmap {
            {"if", IF},
            {"else", ELSE},
            {"while", WHILE},
            {"for", FOR},
            {"return", RETURN},
            {"true", TRUE},
            {"false", FALSE},
            {"fn", FN}, {"struct", STRUCT}, {"let", LET}, {"const", CONST},
            {"loop", LOOP}, {"match", MATCH}, {"break", BREAK}, {"continue", CONTINUE},
            {"in", IN}, {"mut", MUT},
            {"some", SOME}, {"none", NONE}, {"ok", OK}, {"err", ERR},
            {"optional", OPTIONAL}, {"result", RESULT}, {"shared", SHARED}, {"weak", WEAK},
            {"sign", SIGN}, {"unsign", UNSIGN}, {"trunc_cast", TRUNC_CAST}, {"check_cast", CHECK_CAST},
            {"wrap_add", WRAP_ADD}, {"wrap_sub", WRAP_SUB}, {"wrap_mul", WRAP_MUL},

            //TYPENAMES:
            {"bool", BOOL}, {"unit", UNIT}, 
            {"int8", INT8}, {"int16", INT16}, {"int32", INT32}, {"int64", INT64}, 
            {"uint8", UINT8}, {"uint16", UINT16}, {"uint32", UINT32}, {"uint64", UINT64}, 
            {"flo32", FLO32}, {"flo64", FLO64}, 
            {"char", CHAR}, {"string", STRING}, {"string_view", STRING_VIEW}, {"buf_string", BUF_STRING}
        };

        std::vector<Token> tokens;
        std::string source;
        int begin = 0;
        int current = 0;
        int line = 0;
        Error er;
        bool is_at_end() {
            return current >= source.length();
        }
};