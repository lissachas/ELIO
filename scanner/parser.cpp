#include "parser.hpp"

Node* Parser::parse_type() {
    
}

Node* Parser::parse_comparison() {
    Node* expr = parse_additive();

    while (match(GREATER, GREATER_EQUAL, LESS, LESS_EQUAL)) {
        Token op = previous();
        Node* right = parse_additive();
        BinaryExpr* bin = arena.alloc<BinaryExpr>();
        bin->left = expr;
        bin->op = op;
        bin->right = right;
        bin->type = NodeType::BinaryExpr;
        
        expr = bin;
    }

    return expr;
}

Node* Parser::parse_additive() {
    Node* expr = parse_multiplicative();

    while (match(MINUS, PLUS)) {
        Token op = previous();
        Node* right = parse_multiplicative();

        BinaryExpr* bin = arena.alloc<BinaryExpr>();
        bin->left = expr;
        bin->op = op;
        bin->right = right;
        bin->type = NodeType::BinaryExpr;

        expr = bin;
    }

    return expr;
}

Node* Parser::parse_multiplicative() {
    Node* expr = parse_unary();

    while (match(SLASH, STAR)) {
        Token op = previous();
        Node* right = parse_unary();

        BinaryExpr* bin = arena.alloc<BinaryExpr>();
        bin->left = expr;
        bin->op = op;
        bin->right = right;
        bin->type = NodeType::BinaryExpr;

        expr = bin;
    }

    return expr;
}

Node* Parser::parse_unary() {
    
}