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

struct Symbol {
    Token name; //
    Node* type_node; // Typenode from declaration
    Node* decl; // Backpointer to declaration
    bool is_mutable; //
};

class Scope {
    public:
        std::unordered_map<std::string_view, Symbol> symbols;
        Scope* parent = nullptr;

        Symbol* lookup(std::string_view name) {
            if (symbols.contains(name)) return &symbols[name];
            if (parent) return parent->lookup(name);
            return nullptr;
        }

        bool define(std::string_view name, Symbol sym) {
            if (symbols.contains(name)) return false;
            symbols[name] = sym;
            return true;
        }

        

};