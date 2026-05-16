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

class Resolver {
    public:

    private:
    void visit_block(BlockExpr* block);
    void visit_let_decl(LetDecl* decl);
    void visit_identifier(Identifier* id);
};



