#include "parser.hpp"

Node* Parser::parse_type() {
    
}

Node* Parser::parse_expr() {

}

Node* Parser::parse_equality() {
    Node* expr = parse_comparison();

    while (match(EQUAL_EQUAL, BANG_EQUAL)) {
        Token op = previous();
        Node* right = parse_comparison();
        BinaryExpr* bin = arena.alloc<BinaryExpr>();
        bin->left = expr;
        bin->op = op;
        bin->right = right;
        bin->type = NodeType::BinaryExpr;
        
        expr = bin;
    }

    return expr;
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
    if (match(BANG, MINUS, STAR)) {
        Token op = previous();
        Node* right = parse_unary();

        UnaryExpr* bin = arena.alloc<UnaryExpr>();
        bin->op = op;
        bin->operand = right;
        bin->type = NodeType::UnaryExpr;

        right = bin;
        return right;
    }

    return parse_postfix();
}

Node* Parser::parse_postfix() {
    Node* expr = parse_primary();

    while (true) {
        if (match(LPAREN)) {
            auto* bin = arena.alloc<CallExpr>();
            bin->type = NodeType::CallExpr;
            bin->callee = expr;

            if (!check(RPAREN)) {
                do {
                    bin->args.push_back(parse_expr());
                } while (match(COMMA));
            }
            expect(RPAREN, "Expected '(' after arguments");
            expr = bin;

        } else if (match(LBRKT)) {
            auto* bin = arena.alloc<IndexExpr>();
            bin->type  = NodeType::IndexExpr;
            bin->node  = expr;
            bin->index = parse_expr();
            expect(RBRKT, "Expected ']'");
            expr = bin;
        } else if (match(DOT)) {
            expect(IDENT, "Expected field name after '.'");
            Token field = previous();

            if (match(LPAREN)) {
                auto* node = arena.alloc<CallExpr>();
                node->type = NodeType::CallExpr;
                
                auto* bin = arena.alloc<FieldExpr>();
                bin->field = field;
                bin->object = expr;
                bin->type = NodeType::FieldExpr;
                node->callee = bin;

                if (!check(RPAREN)) {
                    do {
                        node->args.push_back(parse_expr());
                    } while (match(COMMA));
                }
                expect(RPAREN, "Expected '(' after arguments");
                expr = node;
            } else {
                auto* bin = arena.alloc<FieldExpr>();
                bin->field = field;
                bin->object = expr;
                bin->type = NodeType::FieldExpr;

                expr = bin;
            }

        } else {
            break;
        }
    }
    

    return expr;
}

Node* Parser::parse_primary() {
    if (match(TRUE, FALSE, NONE, VAL, STR)) return parse_literal();

    if (check(SIGN) || check(UNSIGN) ||
        check(TRUNC_CAST) || check(CHECK_CAST) ||
        check(WRAP_ADD)   || check(WRAP_SUB) || check(WRAP_MUL)) {
        return parse_builtin();
    }

    if (check(IDENT)) {
        if (peek_next().type == LBRKT) {
            return parse_struct_init();
        } else {
            Token op = advance();
            auto* node = arena.alloc<Identifier>();
            node->token = op;
            node->type = NodeType::Identifier;
            return node;
        }
    }

    if (match(LPAREN)) {
        Node* inner = parse_expr();
        expect(RPAREN, "Expected '(' after arguments");

        return inner;
    }

    if (check(LBRKT)) {
        return parse_block();
    }

    throw std::runtime_error("Expected expression");
}

Node* Parser::parse_literal() {
    Token tok = previous();
    auto* node = arena.alloc<Literal>();
    node->type = NodeType::Literal;
    node->token = tok;

    switch (tok.type) {
    case TRUE:
    case FALSE:
        node->literal = LiteralType::BoolLiteral;
        break;
    case NONE:
        node->literal = LiteralType::UnitLiteral;
        break;
    case STR:
        node->literal = LiteralType::StringLiteral;
    case VAL:
        if (tok.get_value().find('.') != std::string_view::npos)
            node->literal = LiteralType::FloatLiteral;
        else
            node->literal = LiteralType::IntLiteral;
        break;
    default:
        throw std::runtime_error("parse_literal: unexpected token");
    }

    return node;
}

bool Parser::is_builtin(Token tok) {
    if (tok.type == SIGN || tok.type == UNSIGN || tok.type == TRUNC_CAST || tok.type == CHECK_CAST || 
    tok.type == WRAP_ADD || tok.type == WRAP_MUL || tok.type == WRAP_SUB) {
        return true;
    } else return false;
}

Node* Parser::parse_builtin() {
    Token name = advance();

    auto* node = arena.alloc<BuiltinCast>();
    node->type = NodeType::BuiltInCast;
    node->builtin = name;
    node->type_arg = nullptr;
    node->second = nullptr;

    switch (name.type) {
    case SIGN:
    case UNSIGN:
        expect(LPAREN, "Expected '(' after sign/unsign");
        node->first = parse_expr();
        expect(RPAREN, "Expected ')' after expression");
        break;
    case TRUNC_CAST:
    case CHECK_CAST:
        expect(LSQUARE, "Expected [ after cast");
        node->first = parse_type();
        expect(RSQUARE, "Expected ] after cast");
        expect(LPAREN,  "Expected '('");
        node->first = parse_expr();
        expect(RPAREN,  "Expected ')'");
        break;
    case WRAP_ADD:
    case WRAP_SUB:
    case WRAP_MUL:
        expect(LPAREN,  "Expected '(' after wrap");
        node->first  = parse_expr();
        expect(COMMA, "Expected ',' between wrap operands");
        node->second = parse_expr();
        expect(RPAREN, "Expected ')'");
        break;
    
    default:
        throw std::runtime_error("parse_builtin: unknown builtin");
    }

    return node;
}

Node* Parser::parse_struct_init() {
    Token name = advance();
    expect(LBRKT, "Expected '{' in struct init");

    auto* node = arena.alloc<StructInit>();
    node->type = NodeType::StructInit;
    node->name = name;

    if (!check(RBRKT)) {
        do {
            expect(RBRKT, "Expected field name in struct init");
            Token field_name = previous();

            auto* fi = arena.alloc<FieldInit>();
            fi->type = NodeType::FieldInit;
            fi->name = field_name;

            if (match(COLON)) {
                fi->value = parse_expr();
                fi->shorthand = false;
            } else {
                fi->value = nullptr;
                fi->shorthand = true;
            }
            node->opt.push_back(fi);

        } while (match(COMMA));
    }

    expect(RBRKT, "Expected '}' after struct fields");
    return node;
}