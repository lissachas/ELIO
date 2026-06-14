module;
#include <set>  
#include <string>
#include <unordered_map>
#include <string_view>
#include <vector>
#include <variant>
#include <concepts>
#include <stdexcept>
#include <memory>

export module typecheck;

import lexer;
import tokens;
import error;
import parser;
import symbol;
import resolve;
import expr;

export enum class TypeKind {
    // Primitives
    INT8, INT16, INT32, INT64, UINT8, UINT16, UINT32, UINT64, FLO32, FLO64, UNIT, BOOL,
    CHAR, STRING, STRING_VIEW, BUF_STRING,

    // Compounds
    ARRAY, // inner + size
    OPTIONAL, // inner
    RESULT, // inner and inner2
    SHARED, // inner
    WEAK, // inner
    REF, // inner
    MUTREF, // inner

    // User defined
    STRUCT,
    ENUM,

    // Addon
    UNKNOWN, // Errors
    UNIT_ // Returned by statements
};

export struct Type {
    TypeKind tkind;

    // Compounds
    std::shared_ptr<Type> inner = nullptr;
    std::shared_ptr<Type> inner2 = nullptr;
    size_t array_size = 0;

    // Struct
    std::string_view struct_name;

    // Constructors
    static Type make(TypeKind k) {
        Type t{};
        t.tkind = k;
        return t;
    }
    static Type make_array(Type elem, size_t n) {
        Type t{};
        t.tkind = TypeKind::ARRAY;
        t.inner = std::make_shared<Type>(elem);
        t.array_size = n;
        return t;
    }
    static Type make_optional(Type inner) {
        Type t{};
        t.tkind = TypeKind::OPTIONAL;
        t.inner = std::make_shared<Type>(inner);
        return t;
    }
    static Type make_result(Type inner, Type inner2) {
        Type t{};
        t.tkind = TypeKind::RESULT;
        t.inner = std::make_shared<Type>(inner);
        t.inner2 = std::make_shared<Type>(inner2);
        return t;
    }
    static Type make_shared(Type inner) {
        Type t{};
        t.tkind = TypeKind::SHARED;
        t.inner = std::make_shared<Type>(inner);
        return t;
    }
    static Type make_weak(Type inner) {
        Type t{};
        t.tkind = TypeKind::WEAK;
        t.inner = std::make_shared<Type>(inner);
        return t;
    }
    static Type make_ref(Type inner) {
        Type t{};
        t.tkind = TypeKind::REF;
        t.inner = std::make_shared<Type>(inner);
        return t;
    }
    static Type make_mut_ref(Type inner) {
        Type t{};
        t.tkind = TypeKind::MUTREF;
        t.inner = std::make_shared<Type>(inner);
        return t;
    }
};

// Types checking:: comparison order -> Same type -> Struct name -> First inner compound -> Second inner compound -> Array size
bool types_equal(const Type& a, const Type& b) {
    if (a.tkind != b.tkind) return false;
    if (a.tkind == TypeKind::STRUCT) return a.struct_name == b.struct_name;
    if (a.inner && b.inner && !types_equal(*a.inner, *b.inner)) return false;
    if (a.inner2 && b.inner2 && !types_equal(*a.inner2, *b.inner2)) return false;
    if (a.tkind == TypeKind::ARRAY) return a.array_size == b.array_size;
    if (a.tkind == TypeKind::ENUM)   return a.struct_name == b.struct_name;
    return true;
}


export class TypeChecker {
    public:
    explicit TypeChecker(Diagnostics* diag) : diag{diag} {}
    void run(Node* program) { 
        check(program); 
    }
    Type query_type(Node* n) { 
        return type_cache.count(n) ? type_cache.at(n) : Type::make(TypeKind::UNKNOWN); 
    }
    Type resolve_type(Node* type_node) {
        if (!type_node) return Type::make(TypeKind::UNIT);
        return resolve_type_node(static_cast<TypeNode*>(type_node));
    }

    private:
    std::unordered_map<Node*, Type> type_cache;
    Diagnostics* diag;

    // Struct field lookup map
    std::unordered_map<std::string_view, std::unordered_map<std::string_view, Type>> struct_fields;
    std::unordered_map<std::string_view, Type> fn_return_types;
    FunctionDecl* current_fn = nullptr;
    std::unordered_map<std::string_view, std::vector<Type>> enum_variant_payloads;
    // maps enum_name -> payload Type per variant index (UNIT if unit variant)
    std::unordered_map<std::string_view,
    std::unordered_map<std::string_view, EnumVariant*>> enum_ctor_map;
    std::unordered_map<std::string_view, EnumDecl*> enum_decl_map;
    std::unordered_map<std::string_view, Type> var_types;
    std::vector<Type> current_loop_break_types;
    std::unordered_map<std::string_view, Type> alias_map;

    void set_type(Node* n, Type t) {
        type_cache[n] = t;
    }
    Type get_type(Node* n) {
        return type_cache.at(n);
    }

    Type resolve_type_node(TypeNode*);
    void type_error(Token tok, const std::string& msg,
                const Type& got, const Type& expected) {
        diag->error(ErrorStage::TypeChecker,
                tok.get_line(),
                std::string(tok.get_value()),
                msg + ": expected " + type_to_string(expected)
                    + ", got " + type_to_string(got));
    }
    std::string type_to_string(const Type& t) {
        switch (t.tkind) {
            case TypeKind::INT8:  return "int8";
            case TypeKind::INT16: return "int16";
            case TypeKind::INT32: return "int32";
            case TypeKind::INT64: return "int64";
            case TypeKind::UINT8:  return "uint8";
            case TypeKind::UINT16: return "uint16";
            case TypeKind::UINT32: return "uint32";
            case TypeKind::UINT64: return "uint64";
            case TypeKind::FLO32: return "flo32";
            case TypeKind::FLO64: return "flo64";
            case TypeKind::BOOL:  return "bool";
            case TypeKind::UNIT:  return "unit";
            case TypeKind::CHAR:  return "char";
            case TypeKind::STRING:  return "string";
            case TypeKind::STRING_VIEW:  return "string_view";
            case TypeKind::BUF_STRING:  return "buf_string";
            case TypeKind::OPTIONAL:
                return "optional[" + type_to_string(*t.inner) + "]";
            case TypeKind::RESULT:
                return "result[" + type_to_string(*t.inner) + ", " + type_to_string(*t.inner2) + "]";
            case TypeKind::SHARED:
                return "shared[" + type_to_string(*t.inner) + "]";
            case TypeKind::WEAK:
                return "weak[" + type_to_string(*t.inner) + "]";
            case TypeKind::ARRAY:
                return "[" + type_to_string(*t.inner) + "; " + std::to_string(t.array_size) + "]";
            case TypeKind::STRUCT:
                return std::string(t.struct_name);
            case TypeKind::UNIT_:  return "unit";
            case TypeKind::REF:    return "&" + type_to_string(*t.inner);
            case TypeKind::MUTREF: return "&mut " + type_to_string(*t.inner);
            case TypeKind::ENUM:
                return "enum " + std::string(t.struct_name);
            default: return "?";
        }
    }

    // entry
    void check(Node* program);
    Type check_node(Node*);

    // declarations
    void check_function(FunctionDecl*);
    void check_struct(StructDecl*);
    void check_let(LetDecl*);
    void check_const(ConstDecl*);

    // statements
    Type check_block(BlockExpr*);
    void check_expr_stmt(ExprStmt*);
    void check_if_stmt(IfStmt*);
    void check_while(WhileStmt*);
    void check_for(ForStmt*);
    Type check_loop(LoopStmt*);
    void check_return(ReturnStmt*);
    void check_break(BreakStmt*);
    void check_continue(ContinueStmt*);
    void check_match_stmt(MatchStmt*);
    void check_exhaustive(std::vector<Node*>&, Type);

    // expressions
    Type check_literal(Literal*);
    Type check_identifier(Identifier*);
    Type check_binary(BinaryExpr*);
    Type check_logical(BinaryExpr*, Type, Type);
    Type check_equality(BinaryExpr*, Type, Type);
    Type check_comparison(BinaryExpr*, Type, Type);
    Type check_arithmetic(BinaryExpr*, Type, Type);
    Type check_unary(UnaryExpr*);
    Type check_call(CallExpr*);
    Type check_index(IndexExpr*);
    Type check_field(FieldExpr*);
    Type check_assign(AssignExpr*);
    Type check_if_expr(IfExpr*);
    Type check_match_expr(MatchExpr*);
    Type check_lambda(LambdaExpr*);
    Type check_block_expr(BlockExpr*);
    Type check_struct_init(StructInit*);
    Type check_builtin(BuiltinCast*);
    void check_enum_decl(EnumDecl*);
    Type check_enum(CallExpr*);
    // helpers
    Type check_builtin_call(CallExpr*, std::string_view name);
    Type check_match_arm(MatchArm*, Type subject_type);
    void check_pattern(Pattern*, Type subject_type);
    Type check_of_node(Node*); // read back
    // All numeric types, also string, bool and char
    bool is_comparable(TypeKind tk) {
        if (tk == TypeKind::BOOL || tk == TypeKind::CHAR || tk == TypeKind::STRING || is_numeric(tk)) return true;

        return false;
    }
    // All numeric types, char and string
    bool is_ordered(TypeKind tk) {
        if (tk == TypeKind::CHAR || tk == TypeKind::STRING || is_numeric(tk)) return true;

        return false;
    }
    // Int, Unit, Float
    bool is_numeric(TypeKind tk) {
        if (tk == TypeKind::INT8 || tk == TypeKind::INT16 || tk == TypeKind::INT32 || tk == TypeKind::INT64 || tk == TypeKind::UINT8 || tk == TypeKind::UINT16 || tk == TypeKind::UINT32 || tk == TypeKind::UINT64 || tk == TypeKind::FLO32 || tk == TypeKind::FLO64) return true;

        return false;
    }

    bool is_integer(TypeKind tk) {
        switch (tk) {
            case TypeKind::INT8:  case TypeKind::INT16:
            case TypeKind::INT32: case TypeKind::INT64:
            case TypeKind::UINT8: case TypeKind::UINT16:
            case TypeKind::UINT32: case TypeKind::UINT64:
                return true;
            default: return false;
        }
}
    // All primitives plus compounds with copyable types inside. Not string, buf_string, shared or weak
    bool is_copyable(TypeKind tk) {
        if (is_numeric(tk) || tk == TypeKind::BOOL || tk == TypeKind::UNIT || tk == TypeKind::CHAR) return true;

        return true;
    }
    // Move semantics is for codegen lol
    bool is_movable(TypeKind);
    // All numerics and maybe string concatenation? (add later)
    bool is_addable(TypeKind);
    Type unknown(Node* node) {
        Type t = Type::make(TypeKind::UNKNOWN);
        set_type(node, t);
        return t;
    }


    // Overload helpers
    int int_rank(TypeKind tk) {
        switch (tk) {
            case TypeKind::INT8: case TypeKind::UINT8: return 1;
            case TypeKind::INT16: case TypeKind::UINT16: return 2;
            case TypeKind::INT32: case TypeKind::UINT32: return 3;
            case TypeKind::INT64: case TypeKind::UINT64: return 4;
            default: return 0;
        }
    }

    bool same_signedness(TypeKind a, TypeKind b) {
        bool a_signed = is_integer(a) &&
            (a==TypeKind::INT8||a==TypeKind::INT16||a==TypeKind::INT32||a==TypeKind::INT64);
        bool b_signed = is_integer(b) &&
            (b==TypeKind::INT8||b==TypeKind::INT16||b==TypeKind::INT32||b==TypeKind::INT64);
        return a_signed == b_signed;
    }

    // Disallowed:
    // int to float, narrowing and signed to unsigned
    bool can_promote(TypeKind from, TypeKind to) {
        if (from == to) return true;
        if (is_integer(from) && is_integer(to))
            return same_signedness(from, to) && int_rank(from) < int_rank(to);
        if (from == TypeKind::FLO32 && to == TypeKind::FLO64) return true;
        return false; 
    }

    EnumVariant* lookup_ctor_in_enum(std::string_view enum_name, std::string_view ctor_name) {
        auto eit = enum_ctor_map.find(enum_name);
        if (eit == enum_ctor_map.end()) return nullptr;
        auto cit = eit->second.find(ctor_name);
        if (cit == eit->second.end()) return nullptr;
        return cit->second;
    }

    EnumDecl* lookup_enum(std::string_view name) {
        auto it = enum_decl_map.find(name);
        return it != enum_decl_map.end() ? it->second : nullptr;
    }

    EnumVariant* find_variant(EnumDecl* en, std::string_view name) {
        for (Node* vn : en->variants) {
            auto* v = static_cast<EnumVariant*>(vn);
            if (v->name.get_value() == name) return v;
        }
        return nullptr;
    }

    Type literal_token_type(const Token& tok) {
        if (tok.type == TokenType::STR)
            return Type::make(TypeKind::STRING);
        if (tok.type == TokenType::TRUE || tok.type == TokenType::FALSE)
            return Type::make(TypeKind::BOOL);
        // VAL - check source string for '.' to distinguish int from float
        auto sv = tok.get_value();
        if (sv.find('.') != std::string_view::npos)
            return Type::make(TypeKind::FLO64); // default float literal type
        return Type::make(TypeKind::INT32);     // default int literal type
    }

};

// Start by resolving all types
Type TypeChecker::resolve_type_node(TypeNode* tn) {
    switch (tn->type_type) {
        case TypeType::Primitive:
            switch (tn->name.type) {
                case INT8: return Type::make(TypeKind::INT8);
                case INT16: return Type::make(TypeKind::INT16);
                case INT32: return Type::make(TypeKind::INT32);
                case INT64: return Type::make(TypeKind::INT64);
                case UINT8: return Type::make(TypeKind::UINT8);
                case UINT16: return Type::make(TypeKind::UINT16);
                case UINT32: return Type::make(TypeKind::UINT32);
                case UINT64: return Type::make(TypeKind::UINT64);
                case FLO32: return Type::make(TypeKind::FLO32);
                case FLO64: return Type::make(TypeKind::FLO64);
                case BOOL: return Type::make(TypeKind::BOOL);
                case UNIT: return Type::make(TypeKind::UNIT);
                case CHAR: return Type::make(TypeKind::CHAR);
                case STRING: return Type::make(TypeKind::STRING);
                case STRING_VIEW: return Type::make(TypeKind::STRING_VIEW);
                case BUF_STRING: return Type::make(TypeKind::BUF_STRING);
                default:
                    return Type::make(TypeKind::UNKNOWN);
            }
        case TypeType::Optional: {
            Type inner = resolve_type_node(static_cast<TypeNode*>(tn->inner));
            return Type::make_optional(inner);
        }
        case TypeType::Result: {
            Type inner = resolve_type_node(static_cast<TypeNode*>(tn->inner));
            Type inner2 = resolve_type_node(static_cast<TypeNode*>(tn->inner2));
            return Type::make_result(inner, inner2);
        }
        case TypeType::Shared: {
            Type inner = resolve_type_node(static_cast<TypeNode*>(tn->inner));
            return Type::make_shared(inner);
        }
        case TypeType::Weak: {
            Type inner = resolve_type_node(static_cast<TypeNode*>(tn->inner));
            return Type::make_weak(inner);
        }
        case TypeType::Ref: {
            Type inner = resolve_type_node(static_cast<TypeNode*>(tn->inner));
            return Type::make_ref(inner);
        }
        case TypeType::RefMut: {
            Type inner = resolve_type_node(static_cast<TypeNode*>(tn->inner));
            return Type::make_mut_ref(inner);
        }
        case TypeType::Named: {
            auto it = alias_map.find(tn->name.get_value());
            if (it != alias_map.end()) {
                return it->second; // return the resolved aliased type
            }
            if (enum_decl_map.count(tn->name.get_value())) {
                Type t;
                t.tkind = TypeKind::ENUM;
                t.struct_name = tn->name.get_value();
                return t;
            }

            // Otherwise it's a struct
            Type t;
            t.tkind = TypeKind::STRUCT;
            t.struct_name = tn->name.get_value();
            return t;
        }

        case TypeType::Array: {
            Type inner = resolve_type_node(static_cast<TypeNode*>(tn->inner));
            size_t n = tn->array_size;
            return Type::make_array(inner, n);
        }

        default: return Type::make(TypeKind::UNKNOWN);
    }
}

// #################################################

// M A I N

// )))))))))))))))))))))))))))))))))))))))))))))))))

void TypeChecker::check(Node* program) {
    // pre-pass
    auto* root = static_cast<BlockExpr*>(program);

    // register struct fields
    for (Node* item: root->opt) {
        if (item->type == NodeType::StructDecl)
            check_struct(static_cast<StructDecl*>(item));
    }
    for (Node* item : root->opt) {
        if (item->type == NodeType::EnumDecl)
            check_enum_decl(static_cast<EnumDecl*>(item));
    }

    // pre-pass 2: register function signatures
    for (Node* item : root->opt) {
        if (item->type == NodeType::FunctionDecl) {
            auto* fn = static_cast<FunctionDecl*>(item);
            Type ret = fn->ret_type
                ? resolve_type_node(static_cast<TypeNode*>(fn->ret_type))
                : Type::make(TypeKind::UNIT);
            fn_return_types[fn->name.get_value()] = ret;
        }
    }

    // pre-pass 3
    for (Node* item : root->opt) {
    if (item->type == NodeType::TypeAliasDecl) {
        auto* ta = static_cast<TypeAliasDecl*>(item);
        alias_map[ta->name.get_value()] = resolve_type_node(static_cast<TypeNode*>(ta->target));
    }
}

    // Pass 4 T T (top decl)
    for (Node* item : root->opt) {
        if (item->type == NodeType::ConstDecl) {
            auto* cd = static_cast<ConstDecl*>(item);
            Type t = cd->type_ann
                ? resolve_type_node(static_cast<TypeNode*>(cd->type_ann))
                : Type::make(TypeKind::UNKNOWN); 
            var_types[cd->ident.get_value()] = t;
        }
    }

    // main pass
    for (Node* item : root->opt)
        check_node(item);
}

Type TypeChecker::check_node(Node* node) {
    if (!node) return Type::make(TypeKind::UNIT_);
    switch (node->type) {
        case NodeType::Literal: return check_literal(static_cast<Literal*>(node));
        case NodeType::Identifier: return check_identifier(static_cast<Identifier*>(node));
        case NodeType::BinaryExpr: return check_binary(static_cast<BinaryExpr*>(node));
        case NodeType::UnaryExpr: return check_unary(static_cast<UnaryExpr*>(node));
        case NodeType::BlockExpr: return check_block(static_cast<BlockExpr*>(node));
        case NodeType::LetDecl: check_let(static_cast<LetDecl*>(node));
            return Type::make(TypeKind::UNIT_);
        case NodeType::ConstDecl: check_const(static_cast<ConstDecl*>(node));
            return Type::make(TypeKind::UNIT_);
        case NodeType::FunctionDecl: check_function(static_cast<FunctionDecl*>(node));
            return Type::make(TypeKind::UNIT_);
        case NodeType::StructDecl: check_struct(static_cast<StructDecl*>(node));
            return Type::make(TypeKind::UNIT_);
        case NodeType::ExprStmt: check_expr_stmt(static_cast<ExprStmt*>(node));
            return Type::make(TypeKind::UNIT_);
        case NodeType::IfStmt: check_if_stmt(static_cast<IfStmt*>(node));
            return Type::make(TypeKind::UNIT_);
        case NodeType::WhileStmt: check_while(static_cast<WhileStmt*>(node));
            return Type::make(TypeKind::UNIT_);
        case NodeType::ForStmt: check_for(static_cast<ForStmt*>(node));
            return Type::make(TypeKind::UNIT_);
        case NodeType::LoopStmt: return check_loop(static_cast<LoopStmt*>(node));
        case NodeType::ReturnStmt: check_return(static_cast<ReturnStmt*>(node));
            return Type::make(TypeKind::UNIT_);
        case NodeType::BreakStmt: check_break(static_cast<BreakStmt*>(node));
            return Type::make(TypeKind::UNIT_);
        case NodeType::ContinueStmt: check_continue(static_cast<ContinueStmt*>(node));
            return Type::make(TypeKind::UNIT_);
        case NodeType::MatchStmt: check_match_stmt(static_cast<MatchStmt*>(node));
            return Type::make(TypeKind::UNIT_);
        case NodeType::CallExpr: return check_call(static_cast<CallExpr*>(node));
        case NodeType::IndexExpr: return check_index(static_cast<IndexExpr*>(node));
        case NodeType::FieldExpr: return check_field(static_cast<FieldExpr*>(node));
        case NodeType::AssignExpr: return check_assign(static_cast<AssignExpr*>(node));
        case NodeType::IfExpr: return check_if_expr(static_cast<IfExpr*>(node));
        case NodeType::MatchExpr: return check_match_expr(static_cast<MatchExpr*>(node));
        case NodeType::LambdaExpr: return check_lambda(static_cast<LambdaExpr*>(node));
        case NodeType::StructInit: return check_struct_init(static_cast<StructInit*>(node));
        case NodeType::BuiltInCast: return check_builtin(static_cast<BuiltinCast*>(node));
        case NodeType::EnumDecl:
        check_enum_decl(static_cast<EnumDecl*>(node)); return Type::make(TypeKind::UNIT_);
        default: return Type::make(TypeKind::UNIT_);
    }
}


// 11111111111111111111111111111111111111111111111111

// D E C L A R A T I O N S

// 22222222222222222222222222222222222222222222222222

void TypeChecker::check_function(FunctionDecl* node) {
    FunctionDecl* enclosing = current_fn;
    current_fn = node;
    
    // Register params so identifiers in the body resolve correctly
    for (Node* p : node->params) {
        auto* param = static_cast<Param*>(p);
        if (param->type_ann)
            var_types[param->name.get_value()] =
                resolve_type_node(static_cast<TypeNode*>(param->type_ann));
    }
    

    if (node->body) 
        check_node(node->body);

    // Remove params
    for (Node* p : node->params) {
        auto* param = static_cast<Param*>(p);
        var_types.erase(param->name.get_value());
    }
    
    current_fn = enclosing;
}

void TypeChecker::check_struct(StructDecl* node) {
    std::unordered_map<std::string_view, Type> fields;
    for (Node* s: node->opt) {
        auto* fd = static_cast<FieldDecl*>(s);
        fields[fd->name.get_value()] = resolve_type_node(static_cast<TypeNode*>(fd->type_ann));
    }
    struct_fields[node->tag.get_value()] = std::move(fields);
}

void TypeChecker::check_let(LetDecl* node) {
    Type init_type = check_node(node->init);

    Type final_type = init_type;
    if (node->type_ann) {
        Type declared = resolve_type_node(static_cast<TypeNode*>(node->type_ann));
        if (init_type.tkind != TypeKind::UNKNOWN &&  !types_equal(init_type, declared))
            diag->error(ErrorStage::TypeChecker, 0, "let", "Initializer type does not match declared type" + type_to_string(declared) + ", got " + type_to_string(init_type));
        final_type = declared;
    }

    auto* pat = static_cast<Pattern*>(node->pattern);
    if (pat->pat_type == PatternType::Identifier)
        var_types[pat->name.get_value()] = node->type_ann ? resolve_type_node(static_cast<TypeNode*>(node->type_ann)) : final_type;

    set_type(node, final_type);
}

void TypeChecker::check_const(ConstDecl* node) {
    Type init_type = check_node(node->init);
    Type final_type = init_type;

    if (node->type_ann) {
        Type declared = resolve_type_node(static_cast<TypeNode*>(node->type_ann));
        if (init_type.tkind != TypeKind::UNKNOWN &&  !types_equal(init_type, declared))
            diag->error(ErrorStage::TypeChecker, 0, "const", "Initializer type does not match declared type");
        final_type = declared;
    }

    var_types[node->ident.get_value()] = final_type;
    set_type(node, final_type);
}

// ======================================================

// S T A T E M E N T S

// =====================================================

Type TypeChecker::check_block(BlockExpr* node) {
    auto saved = var_types;          
    Type last = Type::make(TypeKind::UNIT_);
    for (Node* s : node->opt)
        last = check_node(s);
    var_types = std::move(saved);
    set_type(node, last);
    return last;
}


void TypeChecker::check_expr_stmt(ExprStmt* node) {
    check_node(node->node);
}

void TypeChecker::check_if_stmt(IfStmt* node) {
    Type condition = check_node(node->node);
    if (condition.tkind != TypeKind::BOOL && condition.tkind != TypeKind::UNKNOWN)
        diag->error(ErrorStage::TypeChecker, 0, "if", "Condition must be bool");
    check_node(node->block);
    if (node->other) check_node(node->other);
}

void TypeChecker::check_while(WhileStmt* node) {
    Type condition = check_node(node->condition);
    if (condition.tkind != TypeKind::BOOL && condition.tkind != TypeKind::UNKNOWN)
        diag->error(ErrorStage::TypeChecker, 0, "while", "Condition must be bool");
    check_node(node->block);
}

void TypeChecker::check_for(ForStmt* node) {
    Type iter = check_node(node->node);
    if (iter.tkind != TypeKind::ARRAY && iter.tkind != TypeKind::UNKNOWN)
        diag->error(ErrorStage::TypeChecker, 0, "for", "for-in requires array type");

    if (iter.tkind == TypeKind::ARRAY && iter.inner) {
        auto* pat = static_cast<Pattern*>(node->pattern);
        if (pat->pat_type == PatternType::Identifier)
            var_types[pat->name.get_value()] = *iter.inner;
    }

    check_node(node->block);
}

Type TypeChecker::check_loop(LoopStmt* node) {
    auto saved = current_loop_break_types;
    current_loop_break_types.clear();

    check_node(node->block);

    Type result = Type::make(TypeKind::UNIT);
    for (auto& bt : current_loop_break_types) {
        if (result.tkind == TypeKind::UNIT) {
            result = bt;
            continue;
        }
        if (!types_equal(result, bt))
            diag->error(ErrorStage::TypeChecker, 0, "loop", "break values have inconsistent types");
    }

    current_loop_break_types = saved;
    set_type(node, result);
    return result;
}

void TypeChecker::check_return(ReturnStmt* node) {
    if (!current_fn) return;

    // Guard null ret_type
    Type declared = current_fn->ret_type
        ? resolve_type_node(static_cast<TypeNode*>(current_fn->ret_type))
        : Type::make(TypeKind::UNIT);

    if (!node->has_value) {
        if (declared.tkind != TypeKind::UNIT && declared.tkind != TypeKind::UNIT_)
            diag->error(ErrorStage::TypeChecker, 0, "return", "Missing return value");
        return;
    }

    Type actual = check_node(node->value);
    if (actual.tkind == TypeKind::UNKNOWN) return;
    if (!types_equal(actual, declared))
        diag->error(ErrorStage::TypeChecker, 0, "return", "Return type mismatch: expected " +
                  type_to_string(declared) + ", got " + type_to_string(actual));
}

void TypeChecker::check_break(BreakStmt* node) {
    switch (node->break_type) {
        case BreakType::Plain:
        case BreakType::WithTag:
            break;
        case BreakType::WithValue:
        case BreakType::WithTagValue:
            if (node->value) {
                Type t = check_node(node->value);
                current_loop_break_types.push_back(t);
            }
            break;
    }
}

void TypeChecker::check_continue(ContinueStmt* node) {
    // pass
}

void TypeChecker::check_match_stmt(MatchStmt* node) {
    Type subject = check_node(node->subject);
    for (Node* arm : node->arms) 
        check_match_arm(static_cast<MatchArm*>(arm), subject);
}

// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

// E X P R E S S I O N S

// ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Type TypeChecker::check_literal(Literal* node) {
    Type t;
    switch (node->literal) {
        case LiteralType::BoolLiteral: t = Type::make(TypeKind::BOOL); break;
        case LiteralType::UnitLiteral: t = Type::make(TypeKind::UNIT); break;
        case LiteralType::IntLiteral: t = Type::make(TypeKind::INT32); break;
        case LiteralType::FloatLiteral: t = Type::make(TypeKind::FLO64); break;
        case LiteralType::StringLiteral: t = Type::make(TypeKind::STRING); break;
        case LiteralType::CharLiteral: t = Type::make(TypeKind::CHAR); break;
    }
    set_type(node, t);
    return t;
}

Type TypeChecker::check_identifier(Identifier* node) {
    auto it = var_types.find(node->token.get_value());
    if (it == var_types.end()) {
        return unknown(node);
    }
    set_type(node, it->second);
    return it->second;
}

Type TypeChecker::check_binary(BinaryExpr* node) {
    Type left  = check_node(node->left);
    Type right = check_node(node->right);
    TokenType op = node->op.type;

    // Suppress cascading errors
    if (left.tkind == TypeKind::UNKNOWN || right.tkind == TypeKind::UNKNOWN)
        return unknown(node);

    if (op == AND_AND || op == OR_OR) {
        return check_logical(node, left, right);
    }

    if (op == EQUAL_EQUAL || op == NOT_EQUAL || op == BANG_EQUAL) {
        return check_equality(node, left, right);
    }

    if (op == LESS || op == LESS_EQUAL || op == GREATER || op == GREATER_EQUAL)
        return check_comparison(node, left, right);

    if (op == PLUS || op == MINUS || op == STAR || op == SLASH || op == PERCENT)
        return check_arithmetic(node, left, right);

    
        
    // unknown op
    set_type(node, Type::make(TypeKind::UNKNOWN));
    return Type::make(TypeKind::UNKNOWN);
}

Type TypeChecker::check_logical(BinaryExpr* node, Type left, Type right) {

    if (left.tkind  != TypeKind::BOOL ||
        right.tkind != TypeKind::BOOL)
        diag->error(ErrorStage::TypeChecker,
                    node->op.get_line(),
                    std::string(node->op.get_value()),
                    "Logical operators require bool operands");

    Type result = Type::make(TypeKind::BOOL);
    set_type(node, result);
    return result;
}

Type TypeChecker::check_equality(BinaryExpr* node, Type left, Type right) {

    if (!types_equal(left, right)) {
        type_error(node->op, "Equality operands must have the same type", left, right);
        set_type(node, Type::make(TypeKind::UNKNOWN));
        return Type::make(TypeKind::UNKNOWN);
    }

    Type result = Type::make(TypeKind::BOOL);
    set_type(node, result);
    return result;
}

Type TypeChecker::check_comparison(BinaryExpr* node, Type left, Type right) {

    if (!types_equal(left, right)) {
        type_error(node->op, "Comparison operands must have the same type", left, right);
        set_type(node, Type::make(TypeKind::UNKNOWN));
        return Type::make(TypeKind::UNKNOWN);
    }

    if (!is_comparable(left.tkind)) {
        diag->error(ErrorStage::TypeChecker,
                    node->op.get_line(),
                    std::string(node->op.get_value()),
                    "This type does not support comparison");
        set_type(node, Type::make(TypeKind::UNKNOWN));
        return Type::make(TypeKind::UNKNOWN);
    }

    Type result = Type::make(TypeKind::BOOL);
    set_type(node, result);
    return result;
}

Type TypeChecker::check_arithmetic(BinaryExpr* node, Type left, Type right) {

    if (!types_equal(left, right)) {
        type_error(node->op, "Arithmetic operands must have the same type", left, right);
        set_type(node, Type::make(TypeKind::UNKNOWN));
        return Type::make(TypeKind::UNKNOWN);
    }

    if (!is_numeric(left.tkind)) {
        diag->error(ErrorStage::TypeChecker,
                    node->op.get_line(),
                    std::string(node->op.get_value()),
                    "Arithmetic requires numeric types");
        set_type(node, Type::make(TypeKind::UNKNOWN));
        return Type::make(TypeKind::UNKNOWN);
    }

    set_type(node, left);
    return left;
}

Type TypeChecker::check_if_expr(IfExpr* node) {
    Type condition = check_node(node->condition);
    if (condition.tkind == TypeKind::UNKNOWN)
        return unknown(node);

    if (condition.tkind != TypeKind::BOOL) 
        diag->error(ErrorStage::TypeChecker, 0, "if", "Condition must be bool");

    Type then_t = check_node(node->then_block);

    if (!node->else_expr) {
        set_type(node, Type::make(TypeKind::UNIT));
        return Type::make(TypeKind::UNIT);
    }

    Type else_t = check_node(node->else_expr);

    if (!types_equal(then_t, else_t)) {
        diag->error(ErrorStage::TypeChecker, 0, "if", "if expression branches must have same type: got " +
                type_to_string(then_t) + " and " + type_to_string(else_t));
        return unknown(node);
    }

    set_type(node, then_t);
    return then_t;
}

Type TypeChecker::check_unary(UnaryExpr* node) {
    Type operand = check_node(node->operand);
    if (operand.tkind == TypeKind::UNKNOWN) return unknown(node);

    Type result;
    TokenType op = node->op.type;
    if (op == BANG) {
        if (operand.tkind != TypeKind::BOOL) {
            diag->error(ErrorStage::TypeChecker, node->op.get_line(), "!", "Logical not requires bool");
        } else {
            result = Type::make(TypeKind::BOOL);
        }
    } else if (op == MINUS) {
        if (!is_numeric(operand.tkind)) {
            diag->error(ErrorStage::TypeChecker, node->op.get_line(), "-", "Unary minus requires numeric type");
        } else {
            result = operand;
        }
    } else if (op == AMP) {
        result = Type::make_ref(operand);
    } else if (op == STAR) {
        if (operand.tkind != TypeKind::REF && operand.tkind != TypeKind::MUTREF) {
            diag->error(ErrorStage::TypeChecker, node->op.get_line(), "*", "Dereference requires reference type");
            return unknown(node);
        }
        else result = *operand.inner;
    } else return unknown(node);

    set_type(node, result);
    return result;
}

Type TypeChecker::check_call(CallExpr* node) {
    if (node->ctor_index >= 0)
        return check_enum(node);

    if (node->callee->type != NodeType::Identifier)
        return unknown(node);

    auto* id = static_cast<Identifier*>(node->callee);

    // builtins keep their old path
    if (!node->candidates.empty() && node->candidates[0]->is_builtin) return check_builtin_call(node, id->token.get_value());

    // type each argument once 
    std::vector<Type> arg_types;
    for (Node* a : node->args) arg_types.push_back(check_node(a));

    // viable exact-match candidates
    std::vector<FunctionDecl*> viable;
    for (FunctionDecl* cand : node->candidates) {
        if (cand->params.size() != arg_types.size()) continue;
        bool ok = true;
        for (size_t i = 0; i < arg_types.size(); ++i) {
            auto* p = static_cast<Param*>(cand->params[i]);
            Type pt = resolve_type_node(static_cast<TypeNode*>(p->type_ann));
            if (!types_equal(pt, arg_types[i])) {ok = false; break;}
        }
        if (ok) viable.push_back(cand);
    }

    if (viable.empty()) {
        std::vector<FunctionDecl*> promotable;
        for (FunctionDecl* cand : node->candidates) {
            if (cand->params.size() != arg_types.size()) continue;
            bool ok = true;
            for (size_t i = 0; i < arg_types.size(); ++i) {
                auto* p = static_cast<Param*>(cand->params[i]);
                Type pt = resolve_type_node(static_cast<TypeNode*>(p->type_ann));
                if (!can_promote(arg_types[i].tkind, pt.tkind)) { ok = false; break; }
            }
            if (ok) promotable.push_back(cand);
        }
        if (promotable.size() == 1) { viable = promotable; }
        else if (promotable.size() > 1) {
            diag->error(ErrorStage::TypeChecker, id->token.get_line(),
                std::string(id->token.get_value()),
                "Ambiguous overload under implicit promotion");
            return unknown(node);
        }
    }


    if (viable.empty()) {
        diag->error(ErrorStage::TypeChecker, id->token.get_line(),
            std::string(id->token.get_value()),
            "No overload of '" + std::string(id->token.get_value()) +
            "' matches the argument types");
        return unknown(node);
    }


    if (viable.size() > 1) {
        diag->error(ErrorStage::TypeChecker, id->token.get_line(),
            std::string(id->token.get_value()),
            "Ambiguous call: multiple overloads match");
        return unknown(node);
    }

    FunctionDecl* chosen = viable.front();
    node->chosen = chosen;      // codegen
    id->resolved = chosen;

    Type ret = chosen->ret_type
        ? resolve_type_node(static_cast<TypeNode*>(chosen->ret_type))
        : Type::make(TypeKind::UNIT);
    set_type(node, ret);
    return ret;

}

Type TypeChecker::check_index(IndexExpr* node) {
    Type arr = check_node(node->node);
    Type idx = check_node(node->index);
    if (arr.tkind == TypeKind::UNKNOWN) return unknown(node);

    if (arr.tkind != TypeKind::ARRAY) {
        diag->error(ErrorStage::TypeChecker, 0, "[]", "Index requires array type, got " + type_to_string(arr));
        return unknown(node);
    }

    if (!is_integer(idx.tkind) && idx.tkind != TypeKind::UNKNOWN)
        diag->error(ErrorStage::TypeChecker, 0, "[]", "Index must be integer type, got " + type_to_string(idx));

    Type elem = *arr.inner;
    set_type(node, elem);
    return elem;
}

Type TypeChecker::check_field(FieldExpr* node) {
    Type object = check_node(node->object);
    if (object.tkind == TypeKind::UNKNOWN) 
        return unknown(node);

    if (object.tkind != TypeKind::STRUCT) {
        diag->error(ErrorStage::TypeChecker, node->field.get_line(), std::string(node->field.get_value()),
                  "Field access on non-struct type " + type_to_string(object));
        return unknown(node);
    }

    auto sit = struct_fields.find(object.struct_name);
    if (sit == struct_fields.end()) return unknown(node);

    auto fit = sit->second.find(node->field.get_value());
    if (fit == sit->second.end()) {
        diag->error(ErrorStage::TypeChecker, node->field.get_line(), std::string(node->field.get_value()),
                  "No field '" + std::string(node->field.get_value()) +
                  "' on struct " + std::string(object.struct_name));
        return unknown(node);
    }

    set_type(node, fit->second);
    return fit->second;
}

Type TypeChecker::check_assign(AssignExpr* node) {
    Type target = check_node(node->target);
    Type value  = check_node(node->value);
    if (target.tkind == TypeKind::UNKNOWN || value.tkind == TypeKind::UNKNOWN)
        return unknown(node);

    if (!types_equal(target, value))
        diag->error(ErrorStage::TypeChecker, 0, "=", "Cannot assign " + type_to_string(value) +
                  " to " + type_to_string(target));

    Type result = Type::make(TypeKind::UNIT);
    set_type(node, result);
    return result;
}

Type TypeChecker::check_match_expr(MatchExpr* node) {
    Type subject = check_node(node->subject);
    Type result = Type::make(TypeKind::UNKNOWN);

    for (Node* arm : node->arms) {
        Type arm_t = check_match_arm(static_cast<MatchArm*>(arm), subject);
        if (arm_t.tkind == TypeKind::UNKNOWN) continue;
        if (result.tkind == TypeKind::UNKNOWN) { 
            result = arm_t; 
            continue; 
        }
        if (!types_equal(result, arm_t))
            diag->error(ErrorStage::TypeChecker, 0, "match", "match arms have inconsistent types");
    }

    check_exhaustive(node->arms, subject);
    set_type(node, result);
    return result;
}

void TypeChecker::check_exhaustive(std::vector<Node*>& arms, Type subject) {
    bool catch_all = false;
    for (Node* a : arms) {
        auto* arm = static_cast<MatchArm*>(a);
        if (arm->guard) continue;
        auto* p = static_cast<Pattern*>(arm->pattern);
        if (p->pat_type == PatternType::Wildcard || p->pat_type == PatternType::Identifier)
            catch_all = true;
    }
    if (catch_all) return;

    auto covered_set = [&](auto pred) {
        std::set<std::string_view> seen;
        for (Node* a : arms) {
            auto* arm = static_cast<MatchArm*>(a);
            if (arm->guard) continue;
            pred(static_cast<Pattern*>(arm->pattern), seen);
        }
        return seen;
    };

    if (subject.tkind == TypeKind::ENUM) {
        EnumDecl* en = lookup_enum(subject.struct_name);
        if (!en) return;
        auto seen = covered_set([](Pattern* p, std::set<std::string_view>& s) {
            if (p->pat_type == PatternType::Variant || p->pat_type == PatternType::Identifier) s.insert(p->name.get_value());
        });
        for (Node* vn : en->variants) {
            auto* v = static_cast<EnumVariant*>(vn);
            if (!seen.count(v->name.get_value()))
                diag->error(ErrorStage::TypeChecker, 0, "match",
                    "Non-exhaustive match: variant '" +
                    std::string(v->name.get_value()) + "' not handled");
        }
    } else if (subject.tkind == TypeKind::OPTIONAL) {
        bool some=false,none=false;
        for (Node* a : arms){auto*m=static_cast<MatchArm*>(a); if(m->guard)continue;
            auto t=static_cast<Pattern*>(m->pattern)->pat_type;
            some|=t==PatternType::Some; none|=t==PatternType::None;}
        if(!some||!none) diag->error(ErrorStage::TypeChecker,0,"match","Non-exhaustive match on optional");
    } else if (subject.tkind == TypeKind::RESULT) {
        bool ok=false,err=false;
        for (Node* a : arms){auto*m=static_cast<MatchArm*>(a); if(m->guard)continue;
            auto t=static_cast<Pattern*>(m->pattern)->pat_type;
            ok|=t==PatternType::Ok; err|=t==PatternType::Err;}
        if(!ok||!err) diag->error(ErrorStage::TypeChecker,0,"match","Non-exhaustive match on result");
    }
}

Type TypeChecker::check_match_arm(MatchArm* arm, Type subject) {
    auto saved = var_types; // arm-scoped bindings
    check_pattern(static_cast<Pattern*>(arm->pattern), subject);
    if (arm->guard) {
        Type g = check_node(arm->guard);
        if (g.tkind != TypeKind::BOOL && g.tkind != TypeKind::UNKNOWN)
            diag->error(ErrorStage::TypeChecker, 0, "guard", "match guard must be bool");
    }
    Type body_t = check_node(arm->body);
    var_types = std::move(saved);
    set_type(arm, body_t);
    return body_t;
}

void TypeChecker::check_pattern(Pattern* pat, Type subject) {
    switch (pat->pat_type) {
        case PatternType::Wildcard:
        case PatternType::Identifier:
            var_types[pat->name.get_value()] = subject;     // bind type
            break;

        case PatternType::Literal: {
            Type lt = literal_token_type(pat->name);         // your token->Type helper
            if (subject.tkind != TypeKind::UNKNOWN && !types_equal(lt, subject))
                diag->error(ErrorStage::TypeChecker, pat->lit.get_line(),
                    std::string(pat->lit.get_value()),
                    "Literal pattern does not match scrutinee type " + type_to_string(subject));
            break;
        }

        case PatternType::Variant: {
            if (subject.tkind != TypeKind::ENUM) {
                diag->error(ErrorStage::TypeChecker, pat->name.get_line(),
                    std::string(pat->name.get_value()),
                    "Variant pattern on non-enum type " + type_to_string(subject));
                return;
            }
            EnumDecl* en = lookup_enum(subject.struct_name);
            EnumVariant* v = en ? find_variant(en, pat->name.get_value()) : nullptr;
            if (!v) {
                diag->error(ErrorStage::TypeChecker, pat->name.get_line(),
                    std::string(pat->name.get_value()), "Unknown variant");
                return;
            }
            if (pat->fields.size() != v->payload.size()) {
                diag->error(ErrorStage::TypeChecker, pat->name.get_line(),
                    std::string(pat->name.get_value()),
                    "Variant pattern arity mismatch: expected " +
                    std::to_string(v->payload.size()) + ", got " +
                    std::to_string(pat->fields.size()));
                return;
            }
            for (size_t i = 0; i < pat->fields.size(); ++i) {
                Type slot = resolve_type_node(static_cast<TypeNode*>(v->payload[i]));
                check_pattern(static_cast<Pattern*>(pat->fields[i]), slot);   // nested
            }
            break;
        }

        case PatternType::Some:
        if (subject.tkind != TypeKind::OPTIONAL && subject.tkind != TypeKind::UNKNOWN)
            diag->error(ErrorStage::TypeChecker, 0, "some", "some() pattern requires optional type");
        if (pat->inner && subject.inner)
            check_pattern(static_cast<Pattern*>(pat->inner), *subject.inner);
        break;
        case PatternType::Ok:
        if (subject.tkind != TypeKind::RESULT && subject.tkind != TypeKind::UNKNOWN)
            diag->error(ErrorStage::TypeChecker, 0, "ok", "ok() pattern requires result type");
        if (pat->inner && subject.inner)
            check_pattern(static_cast<Pattern*>(pat->inner), *subject.inner);
        break;
        case PatternType::Err:
        if (subject.tkind != TypeKind::RESULT && subject.tkind != TypeKind::UNKNOWN)
            diag->error(ErrorStage::TypeChecker, 0, "err", "err() pattern requires result type");
        if (pat->inner && subject.inner2)
            check_pattern(static_cast<Pattern*>(pat->inner), *subject.inner2);
        break;
        case PatternType::None:
        if (subject.tkind != TypeKind::OPTIONAL && subject.tkind != TypeKind::UNKNOWN)
            diag->error(ErrorStage::TypeChecker, 0, "none", "none pattern requires optional type");
        break;
        default: break;
    }
}

Type TypeChecker::check_struct_init(StructInit* node) {
    auto sit = struct_fields.find(node->name.get_value());
    if (sit == struct_fields.end()) {
        diag->error(ErrorStage::TypeChecker, node->name.get_line(), std::string(node->name.get_value()),
                  "Unknown struct type");
        return unknown(node);
    }

    for (Node* f : node->opt) {
        auto* fi = static_cast<FieldInit*>(f);
        auto fit = sit->second.find(fi->name.get_value());
        if (fit == sit->second.end()) {
            diag->error(ErrorStage::TypeChecker, fi->name.get_line(), std::string(fi->name.get_value()),
                      "No field '" + std::string(fi->name.get_value()) +
                      "' on struct " + std::string(node->name.get_value()));
            continue;
        }
        if (!fi->shorthand) {
            Type val_t = check_node(fi->value);
            if (val_t.tkind != TypeKind::UNKNOWN && !types_equal(val_t, fit->second))
                diag->error(ErrorStage::TypeChecker, fi->name.get_line(), std::string(fi->name.get_value()),
                          "Field type mismatch");
        } else {
            // shorthand: variable name == field name, look up var type
            auto vit = var_types.find(fi->name.get_value());
            if (vit != var_types.end() && !types_equal(vit->second, fit->second))
                diag->error(ErrorStage::TypeChecker, fi->name.get_line(), std::string(fi->name.get_value()),
                          "Shorthand field type mismatch");
        }
    }

    Type result;
    result.tkind = TypeKind::STRUCT;
    result.struct_name = node->name.get_value();
    set_type(node, result);
    return result;
}

Type TypeChecker::check_builtin(BuiltinCast* node) {
    Type first = check_node(node->first);

    switch (node->builtin.type) {
    case SIGN:
        if (first.tkind == TypeKind::UNKNOWN) return unknown(node);
        if (!is_integer(first.tkind)) {
            diag->error(ErrorStage::TypeChecker, node->builtin.get_line(),
                        "sign", "sign() requires integer type");
            return unknown(node);
        }
        {
            // Map uint -> int of same width
            TypeKind result_kind = first.tkind;
            switch (first.tkind) {
                case TypeKind::UINT8:  result_kind = TypeKind::INT8;  break;
                case TypeKind::UINT16: result_kind = TypeKind::INT16; break;
                case TypeKind::UINT32: result_kind = TypeKind::INT32; break;
                case TypeKind::UINT64: result_kind = TypeKind::INT64; break;
                default: break; // already signed, pass through
            }
            Type result = Type::make(result_kind);
            set_type(node, result);
            return result;
        }
    case TRUNC_CAST:
    case CHECK_CAST: {
        Type target = resolve_type_node(static_cast<TypeNode*>(node->type_arg));
        if (!is_numeric(first.tkind) || !is_numeric(target.tkind))
            diag->error(ErrorStage::TypeChecker, node->builtin.get_line(), "cast", "cast requires numeric types");
        set_type(node, target);
        return target;
    }
    case WRAP_ADD: case WRAP_SUB: case WRAP_MUL: {
        Type second = check_node(node->second);
        if (!types_equal(first, second))
            diag->error(ErrorStage::TypeChecker, node->builtin.get_line(), "wrap", "wrap operands must have same type");
        if (!is_integer(first.tkind))
            diag->error(ErrorStage::TypeChecker, node->builtin.get_line(), "wrap", "wrap requires integer type");
        set_type(node, first);
        return first;
    }
    default: return unknown(node);
    }
}

Type TypeChecker::check_lambda(LambdaExpr* node) {
    check_node(node->node);
    return unknown(node); 
}

Type TypeChecker::check_builtin_call(CallExpr* node, std::string_view name) {

    if (name == "print") {
        if (node->args.size() != 1)
            diag->error(ErrorStage::TypeChecker, 0, "print",
                        "print() takes exactly 1 argument");
        else
            check_node(node->args[0]);

        set_type(node, Type::make(TypeKind::UNIT));
        return Type::make(TypeKind::UNIT);
    }

    if (name == "input") {
        if (!node->args.empty())
            diag->error(ErrorStage::TypeChecker, 0, "input",
                        "input() takes no arguments");
        
        set_type(node, Type::make(TypeKind::STRING));
        return Type::make(TypeKind::STRING);
    }

    if (name == "exit") {
        if (node->args.size() != 1)
            diag->error(ErrorStage::TypeChecker, 0, "exit",
                        "exit() takes exactly 1 argument");
        else {
            Type arg = check_node(node->args[0]);
            if (arg.tkind != TypeKind::UNKNOWN && !is_integer(arg.tkind))
                diag->error(ErrorStage::TypeChecker, 0, "exit",
                            "exit() argument must be integer, got " + type_to_string(arg));
        }

        set_type(node, Type::make(TypeKind::UNIT));
        return Type::make(TypeKind::UNIT);
    }

    if (name == "panic") {

        if (node->args.size() != 1)
            diag->error(ErrorStage::TypeChecker, 0, "panic",
                        "panic() takes exactly 1 argument");
        else
            check_node(node->args[0]);

        set_type(node, Type::make(TypeKind::UNIT));
        return Type::make(TypeKind::UNIT);
    }

    if (name == "assert") {
        if (node->args.size() != 1)
            diag->error(ErrorStage::TypeChecker, 0, "assert",
                        "assert() takes exactly 1 argument");
        else {
            Type arg = check_node(node->args[0]);
            if (arg.tkind != TypeKind::UNKNOWN && arg.tkind != TypeKind::BOOL)
                diag->error(ErrorStage::TypeChecker, 0, "assert",
                            "assert() argument must be bool, got " + type_to_string(arg));
        }

        set_type(node, Type::make(TypeKind::UNIT));
        return Type::make(TypeKind::UNIT);
    }

    // Unknown builtin
    for (Node* a : node->args) check_node(a);
    return unknown(node);
}

void TypeChecker::check_enum_decl(EnumDecl* node) {
    enum_decl_map[node->name.get_value()] = node;
    std::vector<Type> payloads;
    std::unordered_map<std::string_view, EnumVariant*> ctor_map;

    for (Node* v : node->variants) {
        auto* variant = static_cast<EnumVariant*>(v);
        Type payload_t = variant->payload.empty()
            ? Type::make(TypeKind::UNIT)
            : resolve_type_node(static_cast<TypeNode*>(variant->payload[0]));

        payloads.push_back(payload_t);
        ctor_map[variant->name.get_value()] = variant;
    }

    enum_variant_payloads[node->name.get_value()] = std::move(payloads);
    enum_ctor_map[node->name.get_value()] = std::move(ctor_map);
}

Type TypeChecker::check_enum(CallExpr* node) {
    EnumDecl* decl = node->ctor_enum;
    int idx = node->ctor_index;

    auto* variant = static_cast<EnumVariant*>(decl->variants[idx]);

    if (variant->payload.empty()) {
        // unit variant - no arguments allowed
        if (!node->args.empty())
            diag->error(ErrorStage::TypeChecker,
                variant->name.get_line(),
                std::string(variant->name.get_value()),
                "Variant '" + std::string(variant->name.get_value()) +
                "' takes no arguments");
    } else {
        // payload variant - exactly 1 argument
        if (node->args.size() != 1) {
            diag->error(ErrorStage::TypeChecker,
                variant->name.get_line(),
                std::string(variant->name.get_value()),
                "Variant '" + std::string(variant->name.get_value()) +
                "' takes exactly 1 argument");
        } else {
            Type arg_t    = check_node(node->args[0]);
            Type expected = resolve_type_node(static_cast<TypeNode*>(variant->payload[0]));
            if (arg_t.tkind != TypeKind::UNKNOWN && !types_equal(arg_t, expected))
                diag->error(ErrorStage::TypeChecker,
                    variant->name.get_line(),
                    std::string(variant->name.get_value()),
                    "Payload type mismatch: expected " + type_to_string(expected) +
                    ", got " + type_to_string(arg_t));
        }
    }

    Type result;
    result.tkind       = TypeKind::ENUM;
    result.struct_name = decl->name.get_value(); // reuse struct_name - holds the enum type name
    set_type(node, result);
    return result;
}