module;

#include <string>
#include <unordered_map>
#include <string_view>
#include <vector>
#include <variant>
#include <concepts>
#include <stdexcept>

export module resolve;
import lexer;
import tokens;
import error;
import parser;
import symbol;
import expr;

// NAME RESOLUTION
// 1. Scope + Symbol table
// Name resolution
// Function signature registration
// Pattern exhaustiveness
// Control flow validation
// Immutability check
// Definite initialization check
// Type inference
// Return type checking

export class Resolver {
    public:
        // Entry point from main
        void resolve(Node* program);
    

    private:
        Error err;
        Scope* current = nullptr;
        FunctionDecl* current_fn = nullptr;
        int loop_depth = 0; 

        std::vector<std::string_view> active_tags;


        void push_scope() {
            current = new Scope(current);
        }
        void pop_scope() {
            Scope* old = current;
            current = current->parent;
            delete old;
        }
        Node* lookup_any(Token name) {
            std::string_view n = name.get_value();
            if (auto* v = current->lookup_var(n))   return v->decl;
            if (auto* f = current->lookup_fn(n))    return f->decl;
            if (auto* t = current->lookup_type(n))  return t->decl;
            err.error(name.get_line(), std::string(n), "Undefined identifier");
            return nullptr;
        }
        bool lookup_type_exists(Token name) {
            if (current->lookup_type(name.get_value())) return true;
            err.error(name.get_line(), std::string(name.get_value()), "Unknown type");
            return false;
        }
        bool lookup_var_is_mutable(Token name) {
            auto* v = current->lookup_var(name.get_value());
            return v && v->is_mutable;
        }
        bool check_tag(std::string_view tag) {
            for (auto& t: active_tags) 
                if (t == tag) return true;
            return false;
        }
        void push_tag(std::string_view tag) {
            active_tags.push_back(tag);
        }
        void pop_tag() {
            active_tags.pop_back();
        }
    



        void resolve_node(Node*);

        void resolve_const(ConstDecl*);
        void resolve_let(LetDecl*);
        void resolve_param(Param*);
        void resolve_field_decl(FieldDecl*);
        void resolve_struct(StructDecl*);
        void resolve_function(FunctionDecl*);
        
        void resolve_block(BlockExpr*);
        void resolve_expr_stmt(ExprStmt*);
        void resolve_if_stmt(IfStmt*);  
        void resolve_while(WhileStmt*);      
        void resolve_for(ForStmt*);     
        void resolve_loop(LoopStmt*);        
        void resolve_match_stmt(MatchStmt*); 
        void resolve_return(ReturnStmt*);
        void resolve_break(BreakStmt*);   
        void resolve_continue(ContinueStmt*); 

        void resolve_identifier(Identifier*);
        void resolve_literal(Literal*);
        void resolve_binary(BinaryExpr*);
        void resolve_unary(UnaryExpr*);
        void resolve_call(CallExpr*);
        void resolve_index(IndexExpr*);
        void resolve_field(FieldExpr*);
        void resolve_assign(AssignExpr*);
        void resolve_if_expr(IfExpr*);
        void resolve_match_expr(MatchExpr*);
        void resolve_lambda(LambdaExpr*);
        void resolve_block_expr(BlockExpr*);
        void resolve_struct_init(StructInit*);
        void resolve_builtin(BuiltinCast*);

        void resolve_pattern(Pattern* pat, Node* decl, bool is_mutable);
        void resolve_match_arm(MatchArm*);
};

void Resolver::resolve(Node* program) {
        // Start new global scope
        push_scope();

        // Pre-pass
        // Start registering functions
        auto* root = static_cast<BlockExpr*>(program);


        for (Node* node : root->opt) {
            if (node->type == NodeType::FunctionDecl) {
                auto* fn = static_cast<FunctionDecl*>(node);
                FnSymbol s { fn->name, fn, fn->ret_type };
                current->define_fn(fn->name.get_value(), s);
            }
            if (node->type == NodeType::StructDecl) {
                auto* st = static_cast<StructDecl*>(node);
                StructSymbol s { st->tag, st };
                current->define_type(st->tag.get_value(), s);
            }
            if (node->type == NodeType::TypeAliasDecl) {
                auto* ta = static_cast<TypeAliasDecl*>(node);
                TypeAliasSymbol s { ta->name, ta->target };
                current->define_alias(ta->name.get_value(), s);
            }
        }

        // Actual pass lol
        for (Node* node: root->opt) {
            resolve_node(node);
        }
        pop_scope();
    }


// =======================================================

// M A I N

// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>


void Resolver::resolve_node(Node* node) {
    if (!node) return;

    switch (node->type) {
        // declarations
        case NodeType::LetDecl: 
            resolve_let(static_cast<LetDecl*>(node));
            break;
        case NodeType::FunctionDecl: 
            resolve_function(static_cast<FunctionDecl*>(node));
            break;
        case NodeType::ConstDecl: 
            resolve_const(static_cast<ConstDecl*>(node));
            break;
        case NodeType::StructDecl: 
            resolve_struct(static_cast<StructDecl*>(node));
            break;
        case NodeType::TypeAliasDecl: {
            auto* ta = static_cast<TypeAliasDecl*>(node);
            TypeAliasSymbol sym { ta->name, ta->target };
            if (!current->define_alias(ta->name.get_value(), sym))
                err.error(ta->name.get_line(),
                        std::string(ta->name.get_value()),
                        "Type alias already defined");
            break;
        }
        
        // statements
        case NodeType::BlockExpr:
            resolve_block(static_cast<BlockExpr*>(node)); break;
        case NodeType::ExprStmt:
            resolve_expr_stmt(static_cast<ExprStmt*>(node)); break;
        case NodeType::IfStmt:
            resolve_if_stmt(static_cast<IfStmt*>(node)); break;
        case NodeType::WhileStmt:
            resolve_while(static_cast<WhileStmt*>(node)); break;
        case NodeType::ForStmt:
            resolve_for(static_cast<ForStmt*>(node)); break;
        case NodeType::LoopStmt:
            resolve_loop(static_cast<LoopStmt*>(node)); break;
        case NodeType::ReturnStmt:
            resolve_return(static_cast<ReturnStmt*>(node)); break;
        case NodeType::BreakStmt:
            resolve_break(static_cast<BreakStmt*>(node)); break;
        case NodeType::ContinueStmt:
            resolve_continue(static_cast<ContinueStmt*>(node)); break;
        case NodeType::MatchStmt:
            resolve_match_stmt(static_cast<MatchStmt*>(node)); break;

        // expressions
        case NodeType::Identifier:
            resolve_identifier(static_cast<Identifier*>(node)); break;
        case NodeType::Literal: break;
        case NodeType::BinaryExpr:
            resolve_binary(static_cast<BinaryExpr*>(node)); break;
        case NodeType::UnaryExpr:
            resolve_unary(static_cast<UnaryExpr*>(node)); break;
        case NodeType::CallExpr:
            resolve_call(static_cast<CallExpr*>(node)); break;
        case NodeType::IndexExpr:
            resolve_index(static_cast<IndexExpr*>(node)); break;
        case NodeType::FieldExpr:
            resolve_field(static_cast<FieldExpr*>(node)); break;
        case NodeType::AssignExpr:
            resolve_assign(static_cast<AssignExpr*>(node)); break;
        case NodeType::IfExpr:
            resolve_if_expr(static_cast<IfExpr*>(node)); break;
        case NodeType::MatchExpr:
            resolve_match_expr(static_cast<MatchExpr*>(node)); break;
        case NodeType::LambdaExpr:
            resolve_lambda(static_cast<LambdaExpr*>(node)); break;
        case NodeType::StructInit:
            resolve_struct_init(static_cast<StructInit*>(node)); break;
        case NodeType::BuiltInCast:
            resolve_builtin(static_cast<BuiltinCast*>(node)); break;

        // not visited directly via resolve_node
        case NodeType::TypeNode:
        case NodeType::Pattern:
        case NodeType::MatchArm:
        case NodeType::Param:
        case NodeType::FieldDecl:
        case NodeType::FieldInit:
        default:
            break;
    }

}

// +++++++++++++++++++++++++++++++++++++++++++++++++++++++

// D E C L A R A T I O N S

// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++

void Resolver::resolve_let(LetDecl* node) {
    // Resolve Right Hand Side before defining name
    resolve_node(node->init);

    // Extract name from pattern (SIMPLIFIED)
    auto* pat = static_cast<Pattern*>(node->pattern);
    resolve_pattern(pat, node, true); 
}

void Resolver::resolve_const(ConstDecl* node) {
    resolve_node(node->init);

    VarSymbol sym {
        node->ident,
        node,
        node->type_ann,
        false
    };
    if (!current->define_var(node->ident.get_value(), sym))
        err.error(node->ident.get_line(),
                  std::string(node->ident.get_value()),
                  "Name already defined in this scope");
}

void Resolver::resolve_function(FunctionDecl* node) {
    // name already registered
    FunctionDecl* enclose = current_fn;
    current_fn = node;

    // resolve body
    push_scope();
    for (Node* p: node->params) {
        auto* param = static_cast<Param*>(p);
        VarSymbol sym {
            param->name,
            param,
            param->type_ann,
            false  
        };
        if (!current->define_var(param->name.get_value(), sym))
            err.error(param->name.get_line(),
                      std::string(param->name.get_value()),
                      "Duplicate parameter name");
    
    }

    if (node->body) {
        resolve_node(node->body);
    }
    pop_scope();

    current_fn = enclose;
}

void Resolver::resolve_struct(StructDecl* node) {
    /*push_scope();

    for (Node* s: node->opt) {
        auto* field = static_cast<FieldDecl*>(s);
        Symbol sym {
            SymbolType::Struct,
            field->name,
            field,
            field->type_ann,
            false
        };
        current->define(field->name.get_value(), sym);
    }
    pop_scope();*/
}

void Resolver::resolve_field_decl(FieldDecl* node) {
    
}

void Resolver::resolve_param(Param* node) {
    
}

void Resolver::resolve_block(BlockExpr* node) {
    push_scope();
    for (Node* s: node->opt) {
        resolve_node(s);
    }
    pop_scope();
}

void Resolver::resolve_expr_stmt(ExprStmt* node) {
    resolve_node(node->node); 
}

void Resolver::resolve_if_stmt(IfStmt* node) {
    // Resolve condition
    resolve_node(node->node);
    resolve_node(node->block);
    if (node->other) 
        resolve_node(node->other);
    
}

void Resolver::resolve_break(BreakStmt* node) {
    if (loop_depth == 0)
        err.error(0, "break", "Break outside of loop");
    
    switch (node->break_type) {
        case BreakType::Plain:
            break;
        case BreakType::WithValue:
            if (node->value) resolve_node(node->value);
            break;
        case BreakType::WithTag:
            if (!check_tag(node->tag.get_value()))
                err.error(node->tag.get_line(),
                          std::string(node->tag.get_value()),
                          "Break targets unknown loop tag");
            break;
        case BreakType::WithTagValue:
            if (!check_tag(node->tag.get_value()))
                err.error(node->tag.get_line(),
                          std::string(node->tag.get_value()),
                          "Break targets unknown loop tag");
            if (node->value) resolve_node(node->value);
            break;
    }
}

void Resolver::resolve_continue(ContinueStmt* node) {
    if (loop_depth == 0) 
        err.error(0, "continue", "Continue outside of loop");
    if (node->has_tag) {
        if(!check_tag(node->tag.get_value()))
            err.error(0, "continue_tag", "Continue inside untagged loop");
    }
    
}

void Resolver::resolve_return(ReturnStmt* node) {
    if (!current_fn)
        err.error(0, "return", "Return outside of function");
    if (node->has_value) 
        resolve_node(node->value);
    
}

void Resolver::resolve_while(WhileStmt* node) {
    resolve_node(node->condition);
    loop_depth++;
    if (node->has_tag) push_tag(node->tag.get_value());
    resolve_node((node->block));
    if (node->has_tag) pop_tag();
    loop_depth--;
}

void Resolver::resolve_loop(LoopStmt* node) {
    loop_depth++;
    if (node->has_tag) 
        push_tag(node->tag.get_value());
    resolve_node(node->block);
    if (node->has_tag) pop_tag();
    loop_depth--;
}

void Resolver::resolve_for(ForStmt* node) {
    // Resolve iterable
    resolve_node(node->node);

    loop_depth++;
    if (node->has_tag) push_tag(node->tag.get_value());

    // Pattern to loop body
    push_scope();
    auto* pat = static_cast<Pattern*>(node->pattern);
    resolve_pattern(pat, node, true);
    resolve_node(node->block);
    pop_scope();

    if (node->has_tag) pop_tag();
    loop_depth--;
}

void Resolver::resolve_match_stmt(MatchStmt* node) {
    resolve_node(node->subject);
    for (Node* arm: node->arms) {
        resolve_match_arm(static_cast<MatchArm*>(arm));
    }
}



// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>..

// E X P R E S S I O N S

// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>..

void Resolver::resolve_identifier(Identifier* node) {
    std::string_view name = node->token.get_value();
    if (auto* v = current->lookup_var(name)) {
        node->resolved = v->decl;
        return;
    }
    if (auto* f = current->lookup_fn(name)) {
        node->resolved = f->decl;
        return;
    }
    err.error(node->token.get_line(), std::string(name), "Undefined identifier");
}

void Resolver::resolve_literal(Literal* node) {

}

void Resolver::resolve_binary(BinaryExpr* node) {
    resolve_node(node->left);
    resolve_node(node->right);
}

void Resolver::resolve_unary(UnaryExpr* node) {
    resolve_node(node->operand);

}

void Resolver::resolve_call(CallExpr* node) {
    resolve_node(node->callee);
    for (Node* arg: node->args) {
        resolve_node(arg);
    }
}

void Resolver::resolve_index(IndexExpr* node) {
    resolve_node(node->node);
    resolve_node(node->index);
}

void Resolver::resolve_field(FieldExpr* node) {
    resolve_node(node->object);
}

void Resolver::resolve_assign(AssignExpr* node) {
    // Value first, target second
    resolve_node(node->value);
    resolve_node(node->target);

    // Mutability check
    if (node->target->type == NodeType::Identifier) {
        auto* id = static_cast<Identifier*>(node->target);
        auto* sym = current->lookup_var(id->token.get_value());
        if (sym && id->resolved && !sym->is_mutable)
            err.error(id->token.get_line(),
                      std::string(id->token.get_value()),
                      "Cannot assign to immutable variable");
    }
}

void Resolver::resolve_if_expr(IfExpr* node) {
    resolve_node(node->condition);
    resolve_node(node->then_block);
    if (node->else_expr)
        resolve_node(node->else_expr);
}

void Resolver::resolve_match_expr(MatchExpr* node) {
    resolve_node(node->subject);
    for (Node* arm: node->arms) 
        resolve_match_arm(static_cast<MatchArm*>(arm));
}

void Resolver::resolve_lambda(LambdaExpr* node) {
    push_scope();

    for (Node* p: node->param) {
        auto* param = static_cast<Param*>(p);
        VarSymbol sym {
            param->name,
            param,
            param->type_ann,
            false  
        };
        if (!current->define_var(param->name.get_value(), sym))
            err.error(param->name.get_line(),
                      std::string(param->name.get_value()),
                      "Duplicate parameter in lambda");
    }
    resolve_node(node->node);

    pop_scope();
}

void Resolver::resolve_block_expr(BlockExpr* node) {

}

void Resolver::resolve_struct_init(StructInit* node) {
    // verify struct exist
    if (!current->lookup_type(node->name.get_value()))
        err.error(node->name.get_line(),
                  std::string(node->name.get_value()),
                  "Unknown struct type");

    // resolve each field
    for (Node* s: node->opt) {
        auto* field = static_cast<FieldInit*>(s);
        if (!field->shorthand) {
            resolve_node(field->value);
        } else {
            if (!current->lookup_var(field->name.get_value()))
                err.error(field->name.get_line(),
                          std::string(field->name.get_value()),
                          "Undefined variable in struct shorthand init");
        }
    }
}


void Resolver::resolve_builtin(BuiltinCast* node) {
    resolve_node(node->first);
    if (node->second) 
        resolve_node(node->second);
    // No scope lookup is needed in types
}



// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>..

// H E L P E R S

// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>..

void Resolver::resolve_pattern(Pattern* pat, Node* decl, bool is_mutable) {
    // Define names into current scope
    switch (pat->pat_type) {
        case PatternType::Identifier:
            if (pat->name.get_value() != "_") {
                VarSymbol sym { pat->name, decl, nullptr, is_mutable };
                if (!current->define_var(pat->name.get_value(), sym))
                    err.error(pat->name.get_line(),
                            std::string(pat->name.get_value()),
                            "Name already defined in this scope");
            }
            break;
        case PatternType::Wildcard:
        case PatternType::None:
        case PatternType::True:
        case PatternType::False:
        case PatternType::Literal:
            // no names introduced
            break;
        
        case PatternType::Some:
        case PatternType::Ok:
        case PatternType::Err:
            if (pat->inner)
                resolve_pattern(static_cast<Pattern*>(pat->inner), decl, is_mutable);
            break;
        
        case PatternType::Tuple:
            for (Node* f : pat->fields)
                resolve_pattern(static_cast<Pattern*>(f), decl, is_mutable);
            break;

        case PatternType::Struct:
            for (Node* f: pat->fields)
                resolve_pattern(static_cast<Pattern*>(f), decl, is_mutable);
            break;
    }
}

void Resolver::resolve_match_arm(MatchArm* arm) {
    // each arm gets its own scope so pattern bindings don't leak
    push_scope();
    auto* pat = static_cast<Pattern*>(arm->pattern);
    resolve_pattern(pat, arm, false);
    resolve_node(arm->body);
    pop_scope();
}


