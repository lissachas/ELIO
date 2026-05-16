#include "parser.hpp"

// TOP PARSER LINE
Node* Parser::parse() {
    auto* node = arena.alloc<BlockExpr>(); // maybe needs better root
    node->type = NodeType::BlockExpr;
    while (!is_at_end()) {
        try {
            node->opt.push_back(parse_item());
        } catch (const ParseError&) {
            synchronize();
        }
    }
    
    return node;
}

Node* Parser::parse_item() {
    if (match(FN)) return parse_function_decl();
    if (match(STRUCT)) return parse_struct_decl();
    if (check(LET) || check(CONST)) return parse_global_decl();

    throw std::runtime_error("Expected function, struct, or global declaration");
}

Node* Parser::parse_function_decl() {
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
    if (!check(RBRKT)) {
        do {
            node->opt.push_back(parse_field_decl());
        } while (match(COMMA));
    }
    expect(RBRKT, "Expected '}'");
    return node;
    
}

Node* Parser::parse_global_decl() {
    if (match(LET))   return parse_let_decl();
    if (match(CONST)) return parse_const_decl();
    throw std::runtime_error("Expected 'let' or 'const'");
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

        throw std::runtime_error("Expected while/for/loop after tag");
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
    node->has_value = false;
    node->value     = nullptr;
    if (!match(SEMI)) {
        node->has_value = true;
        node->value = parse_expr();
        expect(SEMI, "Expected ';' in break statement");
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
    if (match(IF)) {
        return parse_if_expr();
    }

    if (match(MATCH)) {
        return parse_match_expr();
    }

    if (match(LPAREN)) {
        return parse_lambda_expr();
    }

    return parse_logic_or();
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
        break;
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
    if (match(OPTIONAL)) { node->type_type = TypeType::Optional; expect(LSQUARE,"["); node->inner = parse_type(); expect(RSQUARE,"]"); return node; }
    if (match(RESULT))   { node->type_type = TypeType::Result;   expect(LSQUARE,"["); node->inner = parse_type(); expect(COMMA,","); node->inner2 = parse_type(); expect(RSQUARE,"]"); return node; }
    if (match(SHARED))   { node->type_type = TypeType::Shared;   expect(LSQUARE,"["); node->inner = parse_type(); expect(RSQUARE,"]"); return node; }
    if (match(WEAK))     { node->type_type = TypeType::Weak;     expect(LSQUARE,"["); node->inner = parse_type(); expect(RSQUARE,"]"); return node; }

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
    throw std::runtime_error("Expected type");
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

    throw std::runtime_error("Expected pattern");
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