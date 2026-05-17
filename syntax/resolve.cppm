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
    

    private:
    Scope* current = nullptr;
    int loop_depth = 0; // Break and Continue ?
    void push_scope() {
        current = new Scope(current);
    }
    void pop_scope() {
        Scope* old = current;
        current = current->parent;
        delete old;
    }

    void resolve(Node*);
    void resolve_node(Node*);
    

    void visit_block(BlockExpr* block);
    void visit_let_decl(LetDecl* decl);
    void visit_identifier(Identifier* id);
};



void Resolver::visit_block(BlockExpr* block) {
    push_scope();
    resolve(block);
    pop_scope();
}

void Resolver::resolve(Node* program) {
        // Start new global scope
        push_scope();

        // Pre-pass
        // Start registering functions
        auto* root = static_cast<BlockExpr*>(program);
        for (Node* node: root->opt) {
            if (node->type == NodeType::FunctionDecl) {
                auto* fn = static_cast<FunctionDecl*>(node);
                Symbol s {
                    SymbolType::Function,
                    fn->name,
                    fn,
                    fn->ret_type,
                    false
                };
                if (!current->define(fn->name.get_value(), s)) {
                    // error
                }
                
            }
        }

        // Actual pass lol
        for (Node* node: root->opt) {
            resolve_node(node);
        }
        pop_scope();
    }

void Resolver::resolve_node(Node* node) {
    if (!node) return;

    switch (node->type) {
        
    }

}