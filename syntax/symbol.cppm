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

export struct EnumSymbol {
    Token name;
    EnumDecl* decl;
};

export struct EnumCtorSymbol {
    Token name;           // the constructor name
    EnumDecl* parent;     // which enum it belongs to
    unsigned index;       // position in variants[]
};

export using Symbol = std::variant<VarSymbol, FnSymbol, StructSymbol, ParamSymbol, TypeAliasSymbol>;

export Token symbol_name(const Symbol& s) {
    return std::visit([](const auto& sym) { return sym.name; }, s);
}


export class Scope {
    public:
        std::unordered_map<std::string_view, VarSymbol> var_table;      // let, const, param
        std::unordered_map<std::string_view, std::vector<FnSymbol>> fn_table;       //overloading functions
        std::unordered_map<std::string_view, StructSymbol> type_table;
        std::unordered_map<std::string_view, TypeAliasSymbol> alias_table;
        std::unordered_map<std::string_view, EnumSymbol>     enum_table;
        std::unordered_map<std::string_view, EnumCtorSymbol> ctor_table;
        Scope* parent = nullptr;

        explicit Scope(Scope* parent) : parent{parent} {}

        // lookup -------------
        VarSymbol* lookup_var(std::string_view name) {
            auto it = var_table.find(name);
            if (it != var_table.end()) return &it->second;
            return parent ? parent->lookup_var(name) : nullptr;
        }
        std::vector<FnSymbol>* lookup_fn(std::string_view name) {
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

        EnumSymbol* lookup_enum(std::string_view name) {
            auto it = enum_table.find(name);
            if (it != enum_table.end()) return &it->second;
            return parent ? parent->lookup_enum(name) : nullptr;
        }

        EnumCtorSymbol* lookup_ctor(std::string_view name) {
            auto it = ctor_table.find(name);
            if (it != ctor_table.end()) return &it->second;
            return parent ? parent->lookup_ctor(name) : nullptr;
        }

        // define -----------
        bool define_var(std::string_view name, VarSymbol sym) {
            if (var_table.contains(name)) return false;
            var_table[name] = sym;
            return true;
        }
        void define_fn(std::string_view name, FnSymbol sym) {
            fn_table[name].push_back(sym);
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

        bool define_enum(std::string_view name, EnumSymbol sym) {
            if (enum_table.contains(name)) return false;
            enum_table[name] = sym;
            return true;
        }

        void define_ctor(std::string_view name, EnumCtorSymbol sym) {
            // constructors can't be overloaded, so same duplicate-guard as define_type
            ctor_table[name] = sym;
        }
        
        bool has_local_var(std::string_view name) const {
            return var_table.contains(name);
        }
};