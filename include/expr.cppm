module;

#include <string>
#include <unordered_map>
#include <string_view>
#include <vector>
#include <variant>

export module expr;

import tokens;
import error;

export {

struct Arena {
    std::vector<std::byte> buf;
    size_t offset = 0;

    template<typename T, typename... Args>
    T* alloc(Args&&... args) {
        void* pointer = buf.data() + offset;
        offset += sizeof(T);
        return new(pointer) T(std::forward<Args>(args)...);
    }
};

enum class NodeType {
    // Expressions
    Literal, Identifier, BinaryExpr, UnaryExpr, CallExpr, IndexExpr, FieldExpr, AssignExpr, IfExpr, MatchExpr, LambdaExpr, BlockExpr, StructInit, BuiltInCast,
    // Statements
    ExprStmt, IfStmt, WhileStmt, ForStmt, LoopStmt, MatchStmt, ReturnStmt, BreakStmt, ContinueStmt,
    // Declarations
    LetDecl, ConstDecl, FunctionDecl, StructDecl,
    // Other
    TypeNode, Pattern, MatchArm, Param, FieldDecl, FieldInit

};

enum class TypeType {
    Primitive, Named,
    Ref, RefMut,
    Array, Optional, Result, Shared, Weak
};

struct Node {
    NodeType type;
};


struct TypeNode : Node {
    TypeType type_type;
    Token    name;      // for Primitive and Named — the actual leaf cases
    Node*    inner  = nullptr;  // Ref, RefMut, Optional, Shared, Weak
    Node*    inner2 = nullptr;  // Result's second type (E)
    size_t   array_size = 0;    // Array [T; N]
};

enum class PatternType {
    Identifier, Wildcard, Literal,
    None, True, False,
    Tuple,
    Struct,
    Some, Ok, Err
};

struct Pattern : Node {
    PatternType pat_type;
    Token name;          // for Identifier, Struct patterns
    Node* inner = nullptr; // Some(p), Ok(p), Err(p)
    std::vector<Node*> fields;        // Struct/Tuple pattern children
};

enum class LiteralType {
    BoolLiteral, UnitLiteral, FloatLiteral, IntLiteral, StringLiteral, CharLiteral
};

struct Literal: Node {
    LiteralType literal;
    Token token;
};

struct Identifier: Node {
    Token token;
};

// EXPRESSIONS

struct BinaryExpr: Node {
    Node* left;
    Token op;
    Node* right;
};

struct UnaryExpr: Node {
    Token op;
    Node* operand;
};

struct CallExpr: Node {
    Node* callee;
    std::vector<Node*> args;
};

struct IndexExpr: Node {
    Node* node;
    Node* index;
};

struct FieldExpr: Node {
    Node* object;
    Token field;
};

struct AssignExpr: Node {
    Node* target;
    Node* value;
};

struct IfExpr: Node {
    Node* condition;
    Node* then_block;
    Node* else_expr;
};

struct MatchArm: Node {
    Node* pattern;
    Node* body;
};

struct MatchExpr: Node {
    Node* subject;
    std::vector<Node*> arms;
};

struct LambdaExpr: Node {
    std::vector<Node*> param;
    Node* node;
};

struct BlockExpr: Node {
    std::vector<Node*> opt;
};

struct StructInit: Node {
    Token name;
    std::vector<Node*> opt;
};

struct BuiltinCast: Node {
    Token builtin;
    Node* type_arg = nullptr;
    Node* first;
    Node* second = nullptr;
};

// STATEMENTS

struct ExprStmt : Node {
    Node* node;
};

struct IfStmt : Node {
    Node* node;
    Node* block;
    Node* other;
};

struct WhileStmt : Node {
    bool has_tag;
    Token tag;
    Node* condition;
    Node* block;
};

struct LoopStmt : Node {
    bool has_tag;
    Token tag;
    Node* block;
};

struct ForStmt : Node {
    bool has_tag;
    Token tag;
    Node* pattern;
    Node* node;
    Node* block;
};

struct MatchStmt : Node {
    Node* subject;
    std::vector<Node*> arms;
};

struct ReturnStmt : Node {
    bool has_value;
    Node* value;
};

struct ContinueStmt : Node {
    bool has_tag;
    Token tag;
};

struct BreakStmt : Node {
    bool has_value;
    Node* value;
};

// DECLARATIONS

struct LetDecl : Node {
    Node*  pattern;   // Pattern node
    Node*  type_ann;  // may be null if := used
    Node*  init;
};

struct ConstDecl : Node {
    Token ident;
    Node*  type_ann;  // may be null if := used
    Node*  init;
};

struct FunctionDecl : Node {
    Token              name;
    std::vector<Node*> params;   // Param nodes
    Node*              ret_type;
    Node*              body;     // Block or null (for forward decls)
};

struct StructDecl : Node {
    Token tag;
    std::vector<Node*> opt;
};


// OTHER

struct Param : Node {
    Token name;
    Node* type_ann;
};

struct FieldDecl : Node {
    Token name;
    Node* type_ann;
};

struct FieldInit : Node {
    Token  name;
    Node*  value;       // null if shorthand `{ x }` instead of `{ x: expr }`
    bool   shorthand;
};

}