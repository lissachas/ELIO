
module;


#include <string>
#include <unordered_map>
#include <string_view>
#include <vector>
#include <variant>

export module lexer;

import tokens;
import error;
import expr;


using Object = std::variant<int, std::string_view, double>;


export class Lexer {
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


// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

// I M P L E M E N T A T I O N

// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

// Stacks our tokens
void Lexer::start(std::string set) {
    while (!is_at_end()) {
        begin = current;
        lex();
    }
    tokens.push_back(Token("", "", END, line));

    printf("Finished creating tokens \n");
}

// Just a giant switch case
Token Lexer::lex() {
    char c = advance();
    switch (c) {
    case ';': create_token(SEMI); break;
    case '(': create_token(LPAREN); break;
    case ')': create_token(RPAREN); break;
    case '.': create_token(DOT); break;
    case ',': create_token(COMMA); break;
    case '{': create_token(LBRKT); break;
    case '}': create_token(RBRKT); break;
    case '#': create_token(HASH); break;
    case '[': create_token(LSQUARE); break;
    case ']': create_token(RSQUARE); break;
    case '%': create_token(PERCENT); break;
    case ':': create_token(match('=')? COLON_EQUAL : COLON); break;
    case '+': create_token(PLUS); break;
    case '-': create_token(match('>')? ARROW : MINUS); break;
    case '*': create_token(STAR); break;
    case '!': create_token(match('=')? NOT_EQUAL : BANG); break;
    case '>': create_token(match('=')? GREATER_EQUAL : GREATER); break;
    case '<': create_token(match('=')? LESS_EQUAL : LESS); break;
    case '|': create_token(match('|')? OR_OR : PIPE); break;
    case '&': create_token(match('&')? AND_AND : AMP); break;
    case '=': 
        if (match('=')) create_token(EQUAL_EQUAL);
        else if (match('>')) create_token(FAT_ARROW);
        else create_token(EQUAL);
        break;
    case '/': 
        if (match('/')) {
            while(peek() != '\n' && !is_at_end()) advance();
        } else {
            create_token(SLASH);
        }
        break;
    case ' ':
    case '\t':
    case '\r':
        break;
    case '\n':
        line++;
        break;
    case '"': string(); break;

    
    default:
        if(is_digit(c)) {
            number();
        } else if (is_alpha(c)) { 
            identifier();
        } else {
            er.error(line, source.substr(current, 1), "Unexpected token");
        }
        break;
    }
}

// Handling strings... 
void Lexer::string() {
    // Consume the entire string and check if it ends
    while (peek() != '"' && !is_at_end()) {
        if (peek() == '\n') line++;
        advance();
    }

    if (is_at_end()) {
        er.error(line, "", "Unterminated string");
    }

    advance();
    create_token(STR);
}

// This doesn't actually check if its an int of a float, so the parser does it
void Lexer::number() {
    while(is_digit(peek())) advance();

    if (peek() == '.' && is_digit(peek_next())) advance();

    while (is_digit(peek())) advance();

    create_token(VAL, std::stod(source.substr(begin, current - begin)));
}

// The last one
void Lexer::identifier() {
    while (is_alphanum(peek())) advance();

    TokenType type = IDENT;
    std::string text = source.substr(begin, current - begin);
    // Check if its one of our reserved names
    auto it = identmap.find(text);
    if (it != identmap.end()) {
        type = it->second;
    }
    
    create_token(type);
}