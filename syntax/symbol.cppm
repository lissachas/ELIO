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

export struct VarSymbol {
    Token name;
    Node* decl; // LetDecl or ConstDecl
    Node* type_ann; // may be null
    bool is_mutable; // true, false
};

export struct FnSymbol {
    Token name;
    FunctionDecl* decl;
    Node* ret_type; // TypeNode*
};

export struct StructSymbol {
    Token name;
    StructDecl* decl;
};

export struct ParamSymbol {
    Token name;
    Param* decl;
    Node* type_ann;
};

export struct TypeAliasSymbol {
    Token name;
    Node* target; // TypeNode* of the aliased type
};

export using Symbol = std::variant<VarSymbol, FnSymbol, StructSymbol, ParamSymbol, TypeAliasSymbol>;

export Token symbol_name(const Symbol& s) {
    return std::visit([](const auto& sym) { return sym.name; }, s);
}


export class Scope {
    public:
        std::unordered_map<std::string_view, VarSymbol> var_table;      // let, const, param
        std::unordered_map<std::string_view, FnSymbol> fn_table;       // functions
        std::unordered_map<std::string_view, StructSymbol> type_table;
        std::unordered_map<std::string_view, TypeAliasSymbol> alias_table;
        Scope* parent = nullptr;

        explicit Scope(Scope* parent) : parent{parent} {}

        // lookup -------------
        VarSymbol* lookup_var(std::string_view name) {
            auto it = var_table.find(name);
            if (it != var_table.end()) return &it->second;
            return parent ? parent->lookup_var(name) : nullptr;
        }
        FnSymbol* lookup_fn(std::string_view name) {
            auto it = fn_table.find(name);
            if (it != fn_table.end()) return &it->second;
            return parent ? parent->lookup_fn(name) : nullptr;
        }
        StructSymbol* lookup_type(std::string_view name) {
            auto it = type_table.find(name);
            if (it != type_table.end()) return &it->second;
            return parent ? parent->lookup_type(name) : nullptr;
        }
        TypeAliasSymbol* lookup_alias(std::string_view name) {
            auto it = alias_table.find(name);
            if (it != alias_table.end()) return &it->second;
            return parent ? parent->lookup_alias(name) : nullptr;
        }

        // define -----------
        bool define_var(std::string_view name, VarSymbol sym) {
            if (var_table.contains(name)) return false;
            var_table[name] = sym;
            return true;
        }
        bool define_fn(std::string_view name, FnSymbol sym) {
            if (fn_table.contains(name)) return false;
            fn_table[name] = sym;
            return true;
        }
        bool define_type(std::string_view name, StructSymbol sym) {
            if (type_table.contains(name)) return false;
            type_table[name] = sym;
            return true;
        }
        bool define_alias(std::string_view name, TypeAliasSymbol sym) {
            if (alias_table.contains(name)) return false;
            alias_table[name] = sym;
            return true;
        }
        
        bool has_local_var(std::string_view name) const {
            return var_table.contains(name);
        }
};