#pragma once
#include <string>
#include <unordered_map>
#include <string_view>
#include <vector>
#include <variant>
#include <concepts>
#include <stdexcept>

#include "tokens.hpp"
#include "error.hpp"
#include "lexer.hpp"
#include "expr.hpp"

class Parser {
    public:
    Node* parse();
    Parser(std::vector<Token> tokens, Error& err) : err{err}, tokens{tokens}  {}

    private:
    Error& err;
    Arena arena;
    std::vector<Token> tokens;
    int current = 0;

    template<typename... Args> 
    requires (std::same_as<Args, TokenType> && ...)
    bool match(Args... types) {
        // This had a problem and wouldn't compile. {types...} expands into std::initializer_list<TokenType> you can range-for over it
        for(TokenType type: {types...}) {
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
    Token expect(TokenType type, const std::string& msg) {
        if (check(type)) return advance();
        err.error(peek().get_line(), std::string(peek().get_value()), msg.c_str());
        throw ParseError{};
    }

    void synchronize() {
        advance();
        while (!is_at_end()) {
            if (previous().type == SEMI) return;
            switch (peek().type) {
                case FN: case STRUCT: case LET: case CONST:
                case IF: case WHILE: case FOR: case RETURN: case LOOP:
                    return;
                default: break;
            }
            advance();
        }
    }

    Node* parse_type();
    Node* parse_literal();
    Node* parse_pattern();
    Node* parse_match_arm();
    Node* parse_param_list();
    Node* parse_arg_list();
    Node* parse_param();
    Node* parse_field_init();
    Node* parse_field_decl();

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
    Node* parse_if_stmt();
    Node* parse_while_stmt();
    Node* parse_for_stmt();
    Node* parse_loop_stmt();
    Node* parse_match_stmt();
    Node* parse_return_stmt();
    Node* parse_break_stmt();
    Node* parse_continue_stmt();
    Node* parse_expr_stmt();

    Node* parse_function_decl();
    Node* parse_struct_decl();
    Node* parse_global_decl();
    Node* parse_let_decl();
    Node* parse_const_decl();
    Node* parse_item();

};