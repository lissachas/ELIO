module;

#include <string>
#include <unordered_map>
#include <string_view>
#include <vector>
#include <variant>
#include <concepts>
#include <stdexcept>

export module symbol;

import lexer;
import tokens;
import error;
import parser;
import expr;

export enum class SymbolType {
    Let, Const, Param, Function, Struct
};

export struct Symbol {
    SymbolType type; // Symbol type 
    Token name; // the node name
    Node* type_node; // Typenode from declaration
    Node* decl; // Backpointer to declaration
    bool is_mutable; //
};



export class Scope {
    public:
        std::unordered_map<std::string_view, Symbol> symbols;
        Scope* parent = nullptr;

        explicit Scope(Scope* parent) : parent{parent} {}

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