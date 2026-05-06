#pragma once
#include <string>
#include <unordered_map>
#include <string_view>
#include <vector>
#include <variant>
#include <concepts>
#include <stdexcept>

#include "tokens.hpp"
#include "errors/error.hpp"
#include "lexer.hpp"
#include "expr.hpp"

class Parser {
    public:
    Node* parse();
    Parser(std::vector<Token> tokens) : tokens{tokens} {}

    private:
    Arena arena;
    std::vector<Token> tokens;
    int current = 0;

    template<typename... Args> 
    requires (std::same_as<Args, TokenType> && ...)
    bool match(Args... types) {
        for(TokenType type: types) {
            if (check(type)) {
                advance();
                return true;
            }
        }
        return false;
    }
    bool check(TokenType type) {
        if (is_at_end()) return false;
        return peek().type == type;
    }
    Token advance() {
        if (!is_at_end()) current++;
        return previous();
    }

    bool is_at_end() {
        return peek().type == END;
    }
    Token peek() {
        return tokens.at(current);
    }
    Token previous() {
        return tokens.at(current - 1);
    }
    Token peek_next() {
        if (is_at_end()) return tokens.at(current);
        return tokens.at(current + 1);
    }
    Token expect(TokenType type, const std::string msg) {
        if (check(type)) return advance();

        throw std::runtime_error(msg);
    }

    Node* parse_type();
    Node* parse_literal();
    Node* parse_pattern();

    Node* parse_struct_init();
    Node* parse_builtin();
    bool is_builtin(Token tok);
    Node* parse_primary();
    Node* parse_postfix();
    Node* parse_unary();
    Node* parse_multiplicative();
    Node* parse_additive();
    Node* parse_comparison();
    Node* parse_equality();
    Node* parse_logic_or();
    Node* parse_logic_and();
    Node* parse_if_expr();
    Node* parse_lambda_expr();
    Node* parse_match_expr();
    Node* parse_assignment();
    Node* parse_expr();

    Node* parse_block();
    Node* parse_statement();

    Node* parse_function_decl();
    Node* parse_struct_decl();

    Node* parse_program();

};