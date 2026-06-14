module;

#include <string>
#include <unordered_map>
#include <string_view>
#include <vector>
#include <variant>
#include <concepts>
#include <stdexcept>
#include <optional>

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
        explicit Resolver(Diagnostics* diag) : diag{diag} {}
    

    private:
        Diagnostics* diag;
        Scope* current = nullptr;
        FunctionDecl* current_fn = nullptr;
        int loop_depth = 0; 

        std::vector<std::string_view> active_tags;

        std::optional<FunctionDecl> builtin_print;
        std::optional<FunctionDecl> builtin_input;
        std::optional<FunctionDecl> builtin_exit;
        std::optional<FunctionDecl> builtin_panic;
        std::optional<FunctionDecl> builtin_assert;


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
            if (auto* f = current->lookup_fn(n))    return f->front().decl;
            if (auto* t = current->lookup_type(n))  return t->decl;
            diag->error(ErrorStage::Resolver, name.get_line(), std::string(n), "Undefined identifier");
            return nullptr;
        }
        bool lookup_type_exists(Token name) {
            if (current->lookup_type(name.get_value())) return true;
            diag->error(ErrorStage::Resolver, name.get_line(), std::string(name.get_value()), "Unknown type");
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
        void inject_builtins();
    



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
        inject_builtins(); // handle builtin fn like print

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
                if (!current->define_alias(ta->name.get_value(), s))
                diag->error(ErrorStage::Resolver,
                            ta->name.get_line(),
                            std::string(ta->name.get_value()),
                            "Type alias already defined");
            }
            if (node->type == NodeType::EnumDecl) {
                auto* en = static_cast<EnumDecl*>(node);
                current->define_enum(en->name.get_value(), EnumSymbol{ en->name, en });
                for (unsigned i = 0; i < en->variants.size(); ++i) {
                    auto* v = static_cast<EnumVariant*>(en->variants[i]);
                    current->define_ctor(v->name.get_value(), EnumCtorSymbol{ v->name, en, i });
                }
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
            // pass
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
        diag->error(ErrorStage::Resolver, node->ident.get_line(),
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
            diag->error(ErrorStage::Resolver, param->name.get_line(),
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
        diag->error(ErrorStage::Resolver, 0,
                      "break",
                      "Break outside of loop");
    
    switch (node->break_type) {
        case BreakType::Plain:
            break;
        case BreakType::WithValue:
            if (node->value) resolve_node(node->value);
            break;
        case BreakType::WithTag:
            if (!check_tag(node->tag.get_value()))
                diag->error(ErrorStage::Resolver, node->tag.get_line(),
                          std::string(node->tag.get_value()),
                          "Break targets unknown loop tag");
        case BreakType::WithTagValue:
            if (!check_tag(node->tag.get_value()))
                diag->error(ErrorStage::Resolver, node->tag.get_line(),
                          std::string(node->tag.get_value()),
                          "Break targets unknown loop tag");
            if (node->value) resolve_node(node->value);
            break;
    }
}

void Resolver::resolve_continue(ContinueStmt* node) {
    if (loop_depth == 0) 
        diag->error(ErrorStage::Resolver, 0, "continue", "Continue outside of loop");
    if (node->has_tag) {
        if(!check_tag(node->tag.get_value()))
        diag->error(ErrorStage::Resolver, 0, "continue", "Continue outside of loop");

    }
    
}

void Resolver::resolve_return(ReturnStmt* node) {
    if (!current_fn)
        diag->error(ErrorStage::Resolver, 0, "return", "Return outside of function");
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
        node->resolved = f->front().decl;
    }
}

void Resolver::resolve_binary(BinaryExpr* node) {
    resolve_node(node->left);
    resolve_node(node->right);
}

void Resolver::resolve_unary(UnaryExpr* node) {
    resolve_node(node->operand);

}

void Resolver::resolve_call(CallExpr* node) {
    for (Node* arg: node->args) {
        resolve_node(arg);
    }

    if (node->callee->type == NodeType::Identifier) {
        auto* id = static_cast<Identifier*>(node->callee);
        std::string_view name = id->token.get_value();

        // check enum constructors first
        if (auto* ctor = current->lookup_ctor(name)) {
            node->ctor_enum  = ctor->parent;
            node->ctor_index = static_cast<int>(ctor->index);
            id->resolved = ctor->parent; // point at EnumDecl as a Node*
            return;
        }

        if (auto* set = current->lookup_fn(id->token.get_value())) {
            for (auto& fs : *set) node->candidates.push_back(fs.decl);
            if (!set->empty()) id->resolved = (*set)[0].decl;
            return;
        }

        resolve_node(node->callee);
    } else {
        resolve_node(node->callee);
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
            diag->error(ErrorStage::Resolver, id->token.get_line(),
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
            diag->error(ErrorStage::Resolver, param->name.get_line(),
                      std::string(param->name.get_value()),
                      "Duplicate parameter in lambda");
    }
    resolve_node(node->node);

    pop_scope();
}

void Resolver::resolve_block_expr(BlockExpr* node) {
    resolve_block(node);
}

void Resolver::resolve_struct_init(StructInit* node) {
    // verify struct exist
    if (!current->lookup_type(node->name.get_value()))
        diag->error(ErrorStage::Resolver, node->name.get_line(),
                  std::string(node->name.get_value()),
                  "Unknown struct type");

    // resolve each field
    for (Node* s: node->opt) {
        auto* field = static_cast<FieldInit*>(s);
        if (!field->shorthand) {
            resolve_node(field->value);
        } else {
            if (!current->lookup_var(field->name.get_value()))
                diag->error(ErrorStage::Resolver, field->name.get_line(),
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
            if (current->lookup_ctor(pat->name.get_value())) {
                pat->pat_type = PatternType::Variant;   // zero-arg variant, not a binding
                return;
            }
            if (pat->name.get_value() != "_") {
                VarSymbol sym { pat->name, decl, nullptr, is_mutable };
                if (!current->define_var(pat->name.get_value(), sym))
                    diag->error(ErrorStage::Resolver, pat->name.get_line(),
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
        case PatternType::Variant:
            if (!current->lookup_ctor(pat->name.get_value()))
                diag->error(ErrorStage::Resolver, pat->name.get_line(),
                    std::string(pat->name.get_value()), "Unknown variant in pattern");
            for (Node* f : pat->fields)
                resolve_pattern(static_cast<Pattern*>(f), decl, is_mutable);
            break;
    }
}

void Resolver::resolve_match_arm(MatchArm* arm) {
    // each arm gets its own scope so pattern bindings don't leak
    push_scope();
    auto* pat = static_cast<Pattern*>(arm->pattern);
    resolve_pattern(pat, arm, false);
    if (arm->guard) resolve_node(arm->guard);
    resolve_node(arm->body);
    pop_scope();
}

void Resolver::inject_builtins() {
    
    auto make_builtin = [](std::optional<FunctionDecl>& slot, std::string_view name_sv) -> FunctionDecl* {
        slot.emplace();
        slot->type = NodeType::FunctionDecl;
        slot->is_builtin = true;
        slot->name = Token(name_sv, std::string_view{""}, IDENT, 0);
        slot->ret_type = nullptr;  
        slot->body = nullptr;
        return &slot.value();
    };

    auto register_builtin = [&](std::optional<FunctionDecl>& slot, std::string_view name_sv) {
        FunctionDecl* decl = make_builtin(slot, name_sv);
        FnSymbol sym { decl->name, decl, nullptr };
        current->define_fn(name_sv, sym);
    };

    register_builtin(builtin_print,  "print");
    register_builtin(builtin_input,  "input");
    register_builtin(builtin_exit,   "exit");
    register_builtin(builtin_panic,  "panic");
    register_builtin(builtin_assert, "assert");
}
