#include "lexer.hpp"

void Lexer::start(std::string set) {
    while (!is_at_end()) {
        begin = current;
        lex();
    }
    tokens.push_back(Token("", "", END, line));

    printf("Finished creating tokens \n");
}

Token Lexer::lex() {
    char c = advance();
    switch (c)
    {
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

void Lexer::string() {
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

void Lexer::number() {
    while(is_digit(peek())) advance();

    if (peek() == '.' && is_digit(peek_next())) advance();

    while (is_digit(peek())) advance();

    create_token(VAL, std::stod(source.substr(begin, current - begin)));
}

void Lexer::identifier() {
    while (is_alphanum(peek())) advance();

    TokenType type = IDENT;
    std::string text = source.substr(begin, current - begin);
    auto it = identmap.find(text);
    if (it != identmap.end()) {
        type = it->second;
    }
    
    create_token(type);
}