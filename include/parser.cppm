module;
#include <string>
#include <string_view>
#include <vector>
#include <concepts>
#include <stdexcept>
#include <variant>

export module parser;
import lexer;
import tokens;
import error;
import expr;


export class Parser {
    public:
    Node* parse();
    Parser(std::vector<Token> tokens, Diagnostics* diag) : diag{diag}, tokens{tokens}  {}

    private:
    Diagnostics* diag;
    Arena arena;
    std::vector<Token> tokens;
    int current = 0;

    static constexpr TokenType reserved_builtins[] = {
        SIGN, UNSIGN, TRUNC_CAST, CHECK_CAST,
        WRAP_ADD, WRAP_SUB, WRAP_MUL
    };

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
    Token peek_at(int offset) {
        size_t idx = current + offset;
        if (idx >= tokens.size()) return tokens.at(tokens.size() - 1);
        return tokens.at(idx);
    }
    Token expect(TokenType type, const std::string& msg) {
        if (check(type)) return advance();
        diag->error(ErrorStage::Parser,
                peek().get_line(),
                std::string(peek().get_value()),
                msg);
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

    // Declarations
    Node* parse_type();
    Node* parse_literal();
    Node* parse_pattern();
    Node* parse_match_arm();
    Node* parse_param_list();
    Node* parse_arg_list();
    Node* parse_param();
    Node* parse_field_init();
    Node* parse_field_decl();
    Node* parse_enum();

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

    Node* parse_type_alias();
    Node* parse_function_decl();
    Node* parse_struct_decl();
    Node* parse_global_decl();
    Node* parse_let_decl();
    Node* parse_const_decl();
    Node* parse_item();

};

// #################################################

// I M P L E M E N T A T I O N 

// ==============================================

// TOP PARSER LINE
Node* Parser::parse() {
    auto* node = arena.alloc<BlockExpr>(); // maybe needs better root
    node->type = NodeType::BlockExpr;
    while (!is_at_end()) {
        try {
            node->opt.push_back(parse_item());
        } catch (const ParseError&) {
            // Top-level recovery: skip everything until the next top-level declaration keyword or end of file
            while (!is_at_end()) {
                TokenType t = peek().type;
                if (t == FN || t == STRUCT || t == TYPE ||
                    t == LET || t == CONST)
                    break;
                advance();
            }
        }
    }
    
    return node;
}


Node* Parser::parse_item() {
    if (match(FN)) return parse_function_decl();
    if (match(STRUCT)) return parse_struct_decl();
    if (match(TYPE))   return parse_type_alias();
    if (check(LET) || check(CONST)) return parse_global_decl();

    diag->error(ErrorStage::Parser, peek().get_line(),
            std::string(peek().get_value()), "Expected top-level declaration (fn, struct, type, let, const)");
    throw ParseError{};
}

Node* Parser::parse_function_decl() {
    for (auto rt : reserved_builtins) {
        if (check(rt)) {
            diag->error(ErrorStage::Parser, peek().get_line(),
                        std::string(peek().get_value()),
                        "Cannot use reserved builtin as function name");
            throw ParseError{};
        }
    }

    expect(IDENT, "Expected identifier in function declaration");
    auto* node = arena.alloc<FunctionDecl>();
    node->type = NodeType::FunctionDecl;
    node->name = previous();
    expect(LPAREN, "Expected '(' in function declaration");
    if (!check(RPAREN)) {
        do {
            node->params.push_back(parse_param());

        } while (match(COMMA));
    }
    expect(RPAREN, "Expected ')' in function declaration");
    expect(ARROW, "Expected '->' in function declaration");
    node->ret_type = parse_type();
    if (match(SEMI)) node->body = nullptr;
    else node->body = parse_block();
    return node;
}

Node* Parser::parse_struct_decl() {
    auto* node = arena.alloc<StructDecl>();
    node->type = NodeType::StructDecl;
    expect(IDENT, "Expected struct name");
    node->tag = previous();
    expect(LBRKT, "Expected '{'");
    while (!check(RBRKT) && !is_at_end()) {
        node->opt.push_back(parse_field_decl());
        if (!match(COMMA)) break;
    }
    expect(RBRKT, "Expected '}'");
    return node;
    
}

Node* Parser::parse_global_decl() {
    if (match(LET))   return parse_let_decl();
    if (match(CONST)) return parse_const_decl();
    diag->error(ErrorStage::Parser, peek().get_line(),
            std::string(peek().get_value()), "expected let or const");
    throw ParseError{}; 
}


// DECLARATION PARSER LINE
Node* Parser::parse_let_decl() {
    auto* node = arena.alloc<LetDecl>();
    node->type    = NodeType::LetDecl;
    node->pattern = parse_pattern();

    if (match(COLON_EQUAL)) {
        node->type_ann = nullptr;
    } else {
        expect(COLON, "Expected ':' or ':='");
        node->type_ann = parse_type();
        expect(EQUAL, "Expected '='");
    }
    node->init = parse_expr();
    expect(SEMI, "Expected ';'");
    return node;
}

Node* Parser::parse_const_decl() {
    auto* node = arena.alloc<ConstDecl>();
    node->type = NodeType::ConstDecl;
    expect(IDENT, "Expected name after 'const'");
    node->ident = previous();
    expect(COLON, "Expected ':'");
    node->type_ann = parse_type();
    expect(EQUAL, "Expected '='");
    node->init = parse_expr();
    expect(SEMI, "Expected ';'");
    return node;
}

Node* Parser::parse_type_alias() {
    auto* node = arena.alloc<TypeAliasDecl>();
    node->type = NodeType::TypeAliasDecl;
    expect(IDENT, "Expected name after 'type'");
    node->name = previous();
    expect(EQUAL, "Expected '=' in type alias");
    node->target = parse_type();
    expect(SEMI, "Expected ';' after type alias");
    return node;
}


// STATEMENT PARSER LINE
Node* Parser::parse_block() {
    expect(LBRKT, "Expected '{' in block statement");
    auto* node = arena.alloc<BlockExpr>();
    node->type = NodeType::BlockExpr;

    while (!check(RBRKT) && !is_at_end()) {
        try {
            node->opt.push_back(parse_statement());
        } catch (const ParseError&) {
            synchronize();
        }
    }
    expect(RBRKT, "Expected '}' in block statement");
    return node;
}

Node* Parser::parse_statement() {
    if (match(LET))      return parse_let_decl();
    if (match(CONST))    return parse_const_decl();
    if (match(IF))       return parse_if_stmt();
    if (match(WHILE))    return parse_while_stmt();
    if (match(FOR))      return parse_for_stmt();
    if (match(LOOP))     return parse_loop_stmt();
    if (match(MATCH))    return parse_match_stmt();
    if (match(RETURN))   return parse_return_stmt();
    if (match(BREAK))    return parse_break_stmt();
    if (match(CONTINUE)) return parse_continue_stmt();
    if (check(LBRKT))    return parse_block();

    // tagged loop
    if (check(IDENT) && peek_next().type == COLON) {
        Token tag = advance();
        advance();
        if (match(WHILE)) {
            auto* tnode = static_cast<WhileStmt*>(parse_while_stmt());
            tnode->has_tag = true;
            tnode->tag = tag;
            return tnode;
        }
        if (match(FOR)) {
            auto* tnode = static_cast<ForStmt*>(parse_for_stmt());
            tnode->has_tag = true;
            tnode->tag = tag;
            return tnode;
        }
        if (match(LOOP)) {
            auto* tnode = static_cast<LoopStmt*>(parse_loop_stmt());
            tnode->has_tag = true;
            tnode->tag = tag;
            return tnode;
        }

        diag->error(ErrorStage::Parser, peek().get_line(),
            std::string(peek().get_value()), "Expected while/for/loop after tag");
    }

    return parse_expr_stmt(); 
}

Node* Parser::parse_if_stmt() {
    auto* node = arena.alloc<IfStmt>();
    node->type = NodeType::IfStmt;
    node->node = parse_expr();
    node->block = parse_block();
    node->other = nullptr;
    if (match(ELSE)) {
        if (match(IF)) node->other = parse_if_stmt();
        else node->other = parse_block();
    }
    return node;
}

Node* Parser::parse_while_stmt() {
    auto* node = arena.alloc<WhileStmt>();
    node->type = NodeType::WhileStmt;
    node->has_tag = false;
    node->condition = parse_expr();
    node->block = parse_block();
    return node;
}

Node* Parser::parse_for_stmt() {
    auto* node = arena.alloc<ForStmt>();
    node->type    = NodeType::ForStmt;
    node->has_tag = false;
    node->pattern = parse_pattern();
    expect(IN, "Expected 'in'");
    node->node  = parse_expr();
    node->block = parse_block();
    return node;
}

Node* Parser::parse_loop_stmt() {
    auto* node = arena.alloc<LoopStmt>();
    node->type    = NodeType::LoopStmt;
    node->has_tag = false;
    node->block   = parse_block();
    return node;
}

Node* Parser::parse_match_stmt() {
    auto* node = arena.alloc<MatchStmt>();
    node->type    = NodeType::MatchStmt;
    node->subject = parse_expr();
    expect(LBRKT, "Expected '{'");
    while (!check(RBRKT) && !is_at_end()) {
        node->arms.push_back(parse_match_arm());
    }
    expect(RBRKT, "Expected '}'");
    return node;
}

Node* Parser::parse_return_stmt() {
    auto* node = arena.alloc<ReturnStmt>();
    node->type = NodeType::ReturnStmt;
    if (match(SEMI)) {
        node->has_value = false;
        node->value = nullptr;
    } else {
        node->has_value = true;
        node->value = parse_expr();
        expect(SEMI, "Expected ; in return statement");
    }
    return node;
}

Node* Parser::parse_break_stmt() {
    auto* node = arena.alloc<BreakStmt>();
    node->type      = NodeType::BreakStmt;
    node->value     = nullptr;

    if (match(WITH)) {
        expect(IDENT, "Expected loop label after 'break with'");
        node->tag = previous();

        if (match(SEMI)) {
            node->break_type = BreakType::WithTag;
        } else {
            node->break_type = BreakType::WithTagValue;
            node->value = parse_expr();
            expect(SEMI, "Expected ';' after break expression");
        }
    } else if (match(SEMI)) {
        node->break_type = BreakType::Plain;
    } else {
        node->break_type = BreakType::WithValue;
        node->value = parse_expr();
        expect(SEMI, "Expected ';' after break expression");
    }

    return node;
}

Node* Parser::parse_continue_stmt() {
    auto* node = arena.alloc<ContinueStmt>();
    node->type    = NodeType::ContinueStmt;
    node->has_tag = false;
    if (check(IDENT)) { 
        node->has_tag = true; 
        node->tag = advance(); 
    }
    expect(SEMI, "Expected ';' in continue statement");
    return node;
}

Node* Parser::parse_expr_stmt() {
    auto* node = arena.alloc<ExprStmt>();
    node->type = NodeType::ExprStmt;
    node->node = parse_expr();
    expect(SEMI, "Expected ;");
    return node;
}

// EXPRESSION PARSER LINE
Node* Parser::parse_expr() {
    return parse_assignment();
}

Node* Parser::parse_assignment() {
    if (match(IF))     return parse_if_expr();
    if (match(MATCH))  return parse_match_expr();
    if (check(LPAREN)) {
        bool is_lambda = false;
        if (peek_next().type == RPAREN) is_lambda = true;
        else if (peek_next().type == IDENT) {
            if (peek_at(2).type == COLON)
                is_lambda = true;
        }
        if (is_lambda) {
            advance();
            return parse_lambda_expr();
        }
    }

    Node* left = parse_logic_or();

    if (match(EQUAL)) {
        Node* value  = parse_assignment();
        auto* node   = arena.alloc<AssignExpr>();
        node->type   = NodeType::AssignExpr;
        node->target = left;
        node->value  = value;
        return node;
    }

    return left;
}

Node* Parser::parse_if_expr() {
    Node* condition = parse_expr();
    Node* then_block = parse_block();


    auto* node = arena.alloc<IfExpr>();
        node->condition = condition;
        node->then_block = then_block;
        node->type = NodeType::IfExpr;

    if (match(ELSE)) {
        node->else_expr = parse_expr();

        return node;
    }

    node->else_expr = nullptr;
    return node;
}

Node* Parser::parse_match_expr() {
    Node* subject = parse_expr();
    expect(LBRKT, "Expected '{'");
    auto* node = arena.alloc<MatchExpr>();
    node->type = NodeType::MatchExpr;
    node->subject = subject;
    
    while (!check(RBRKT) && !is_at_end()) {
        node->arms.push_back(parse_match_arm());
    }
    expect(RBRKT, "Expected '}'");
    return node;
}

Node* Parser::parse_lambda_expr() {
     if (match(RPAREN)) {
        expect(ARROW, "Expected '->'");
        auto* node = arena.alloc<LambdaExpr>();
        node->type = NodeType::LambdaExpr;
        if (check(LBRKT)) node->node = parse_block();
        else { 
            node->node = parse_expr(); 
            expect(SEMI, "Expected ';'"); 
        }
        return node;
    }

    if (check(IDENT) && peek_next().type == COLON) {
        auto* node = arena.alloc<LambdaExpr>();
        node->type = NodeType::LambdaExpr;
        do {
            node->param.push_back(parse_param());
        } while (match(COMMA));

        expect(RPAREN, "Expected ')'");
        expect(ARROW,  "Expected '->'");
        if (check(LBRKT)) node->node = parse_block();
        else { 
            node->node = parse_expr(); 
            expect(SEMI, "Expected ';'"); 
        }
        return node;
    }

    Node* inner = parse_expr();
    expect(RPAREN, "Expected ')'");
    return inner;
}

Node* Parser::parse_logic_or() {
    Node* expr = parse_logic_and();

    while (match(OR_OR)) {
        Token op = previous();
        Node* right = parse_logic_and();
        BinaryExpr* bin = arena.alloc<BinaryExpr>();
        bin->left = expr;
        bin->op = op;
        bin->right = right;
        bin->type = NodeType::BinaryExpr;
        
        expr = bin;
    }

    return expr;
}

Node* Parser::parse_logic_and() {
    Node* expr = parse_equality();

    while (match(AND_AND)) {
        Token op = previous();
        Node* right = parse_equality();
        BinaryExpr* bin = arena.alloc<BinaryExpr>();
        bin->left = expr;
        bin->op = op;
        bin->right = right;
        bin->type = NodeType::BinaryExpr;
        
        expr = bin;
    }

    return expr;

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

    while (match(SLASH, STAR, PERCENT)) {
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

        } else if (match(LSQUARE)) {
            auto* bin = arena.alloc<IndexExpr>();
            bin->type  = NodeType::IndexExpr;
            bin->node  = expr;
            bin->index = parse_expr();
            expect(RSQUARE, "Expected ']'");
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
        } else if (expr->type == NodeType::Identifier && check(LBRKT)) {
            auto* id = static_cast<Identifier*>(expr);
            std::string_view nm = id->token.get_value();
            char first = nm.empty() ? 0 : nm[0];
            if (first >= 'A' && first <= 'Z') {
                advance(); // consume '{'
                auto* init = arena.alloc<StructInit>();
                init->type = NodeType::StructInit;
                init->name = id->token;
                if (!check(RBRKT)) {
                    do {
                        expect(IDENT, "Expected field name in struct init");
                        Token field_name = previous();
                        auto* fi = arena.alloc<FieldInit>();
                        fi->type = NodeType::FieldInit;
                        fi->name = field_name;
                        if (match(COLON)) { fi->value = parse_expr(); fi->shorthand = false; }
                        else              { fi->value = nullptr;      fi->shorthand = true; }
                        init->opt.push_back(fi);
                    } while (match(COMMA));
                }
                expect(RBRKT, "Expected '}' after struct fields");
                expr = init;
            } else {
                break;
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
        Token op = advance();
        auto* node = arena.alloc<Identifier>();
        node->token = op;
        node->type = NodeType::Identifier;
        return node;
    }

    if (match(LPAREN)) {
        Node* inner = parse_expr();
        expect(RPAREN, "Expected ')' after arguments");

        return inner;
    }

    if (check(LBRKT)) {
        return parse_block();
    }

    diag->error(ErrorStage::Parser, peek().get_line(),
            std::string(peek().get_value()), "Expected expression");
    throw ParseError{};
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
        break;
    case VAL:
        if (tok.get_value().find('.') != std::string_view::npos)
            node->literal = LiteralType::FloatLiteral;
        else
            node->literal = LiteralType::IntLiteral;
        break;
    default:
        diag->error(ErrorStage::Parser, peek().get_line(),
            std::string(peek().get_value()), "Parse literal unexpected token");
            throw ParseError{};
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
        node->type_arg = parse_type();
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
        diag->error(ErrorStage::Parser, peek().get_line(),
            std::string(peek().get_value()), "unknown builtin");
        throw ParseError{};
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
            expect(IDENT, "Expected field name in struct init");
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

// ADDITIONAL PARSING
Node* Parser::parse_type() {
    auto* node = arena.alloc<TypeNode>();
    node->type  = NodeType::TypeNode;
    node->inner = node->inner2 = nullptr;
    node->array_size = 0;

    if (match(AMP)) {
        node->type_type = match(MUT) ? TypeType::RefMut : TypeType::Ref;
        node->inner = parse_type();
        return node;
    }
    if (match(LSQUARE)) {
        node->type_type  = TypeType::Array;
        node->inner      = parse_type();
        expect(SEMI, "Expected ';' in array type");
        expect(VAL,  "Expected size");
        node->array_size = (size_t)std::get<double>(previous().get_literal());
        expect(RSQUARE, "Expected ']'");
        return node;
    }
    if (match(OPTIONAL)) { 
        node->type_type = TypeType::Optional; 
        expect(LSQUARE,"["); 
        node->inner = parse_type(); 
        expect(RSQUARE,"]"); 
        return node; 
    }
    if (match(RESULT))   { 
        node->type_type = TypeType::Result;   
        expect(LSQUARE,"["); 
        node->inner = parse_type(); 
        expect(COMMA,","); 
        node->inner2 = parse_type(); 
        expect(RSQUARE,"]"); 
        return node; 
    }
    if (match(SHARED))   { 
        node->type_type = TypeType::Shared;   
        expect(LSQUARE,"["); 
        node->inner = parse_type(); 
        expect(RSQUARE,"]"); 
        return node; 
    }
    if (match(WEAK))     { 
        node->type_type = TypeType::Weak;     
        expect(LSQUARE,"["); 
        node->inner = parse_type(); 
        expect(RSQUARE,"]"); 
        return node; 
    }

    if (match(BOOL,UNIT,INT8,INT16,INT32,INT64,UINT8,UINT16,UINT32,UINT64,FLO32,FLO64,CHAR,STRING,STRING_VIEW,BUF_STRING)) {
        node->type_type = TypeType::Primitive;
        node->name = previous();
        return node;
    }
    if (match(IDENT)) {
        node->type_type = TypeType::Named;
        node->name = previous();
        return node;
    }
    diag->error(ErrorStage::Parser, peek().get_line(),
            std::string(peek().get_value()), "expected type");
    throw ParseError{};
}

Node* Parser::parse_pattern() {
    auto* node = arena.alloc<Pattern>();
    node->type = NodeType::Pattern;

    if (match(UNDERSCORE)) {
        node->pat_type = PatternType::Wildcard;
        return node;
    }
    if (match(TRUE)) {
        node->pat_type = PatternType::True;
        return node;
    }
    if (match(FALSE)) {
        node->pat_type = PatternType::False;
        return node;
    }
    if (match(NONE)) {
        node->pat_type = PatternType::None;
        return node;
    }

    if (match(VAL, STR)) {
        node->pat_type = PatternType::Literal;
        node->name = previous();
        return node;
    }

    if (match(SOME)) {
        node->pat_type = PatternType::Some;
        expect(LPAREN, "Expected '(' in pattern");
        node->inner = parse_pattern();
        expect(RPAREN, "Expected ')' in pattern");
        return node;
    }

    if (match(OK)) {
        node->pat_type = PatternType::Ok;
        expect(LPAREN, "Expected '(' in pattern");
        node->inner = parse_pattern();
        expect(RPAREN, "Expected ')' in pattern");
        return node;
    }

    if (match(ERR)) {
        node->pat_type = PatternType::Err;
        expect(LPAREN, "Expected '(' in pattern");
        node->inner = parse_pattern();
        expect(RPAREN, "Expected ')' in pattern");
        return node;
    }

    if (match(LPAREN)) {
        node->pat_type = PatternType::Tuple;
        if (!check(RPAREN)) {
            do {
                node->fields.push_back(parse_pattern());
            } while (match(COMMA));
        }
        return node;
    }

    if (match(IDENT)) {
        node->name = previous();
        if (match(LBRKT)) {
            node->pat_type = PatternType::Struct;
            do {
                auto* fp = arena.alloc<Pattern>();
                fp->type = NodeType::Pattern;
                fp->pat_type = PatternType::Identifier;
                expect(IDENT, "Expected field name");
                fp->name = previous();
                if (match(COLON)) fp->inner = parse_pattern();
                node->fields.push_back(fp);
            } while (match(COMMA));
        } else {
        node->pat_type = PatternType::Identifier;
        }
        return node;
    }

    diag->error(ErrorStage::Parser, peek().get_line(),
            std::string(peek().get_value()), "Expected pattern");
    throw ParseError{};
}

Node* Parser::parse_field_decl() {
    auto* f = arena.alloc<FieldDecl>();
    f->type = NodeType::FieldDecl;
    expect(IDENT, "Expected field name");
    f->name = previous();
    expect(COLON, "Expected ':'");
    f->type_ann = parse_type();

    return f;
}

Node* Parser::parse_param() {
    auto* p = arena.alloc<Param>();
    p->type = NodeType::Param;
    expect(IDENT, "Expected param name");
    p->name = previous();
    expect(COLON, "Expected ':'");
    p->type_ann = parse_type();
    return p;
}

Node* Parser::parse_match_arm() {
    auto* node = arena.alloc<MatchArm>();
    node->type    = NodeType::MatchArm;
    node->pattern = parse_pattern();
    expect(FAT_ARROW, "Expected '=>'");

    if (check(LBRKT)) {
        node->body = parse_block();
    } else {
        node->body = parse_expr();
        expect(COMMA, "Expected ',' after match arm expression");
    }
    return node;
}

Node* Parser::parse_enum() {
    expect(ENUM, "Expected enum");
    Token name = expect(IDENT, "Expected enum name");
    expect(LBRKT, "Expected '{' in enum");
    auto* decl = arena.alloc<EnumDecl>();
    decl->type = NodeType::EnumDecl;
    decl->name = name;
    while (!check(RBRKT)) {
        auto* v = arena.alloc<EnumVariant>();
        v->type = NodeType::EnumVariant;
        v->name = expect(IDENT, "Expected tag name");
        if (match(LPAREN)) {                 // payload list
            do { v->payload.push_back(parse_type()); }
            while (match(COMMA));
            expect(RPAREN, "Expected closing ')' in payload list");
        }
        decl->variants.push_back(v);
        if (!match(COMMA)) break;
    }
    expect(RBRKT, "Expected '}' in enum");
    return decl;
}