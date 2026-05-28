module;
#include <llvm-19/llvm/IR/DerivedTypes.h>
#include <llvm-19/llvm/IR/Value.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Support/raw_ostream.h>
#include <vector>
#include <memory>
#include <unordered_map>
#include <string>
#include <string_view>
#include <cstddef>

export module codegen;

import expr;
import typecheck;
import tokens;

export class Codegen {
    public:
    Codegen () {
        this->context = std::make_unique<llvm::LLVMContext>();
        this->_module = std::make_unique<llvm::Module>("elio", *context);
        this->builder = std::unique_ptr<llvm::IRBuilder<>>(new llvm::IRBuilder<>(*context));
    }

    void generate(Node* program);
    void emit_ir(const std::string& path);
    void set_type_checker(TypeChecker* tc) {
        tchecker = tc;
    }
    

    private:
    std::unique_ptr<llvm::LLVMContext> context; // First create unique Context object to tie whole code generation together. Use Context to get access to LLVM data structures like modules and IRBuilder objects
    std::unique_ptr<llvm::Module> _module; // Create named global variables and query them
    std::unique_ptr<llvm::IRBuilder<>> builder; // Object. Incrementally build up IR. Acts something like a current pointer
    TypeChecker* tchecker = nullptr;

    // MAPS:
    std::unordered_map<std::string_view, llvm::StructType*>  struct_type_map;
    std::unordered_map<std::string_view, llvm::Function*>  fn_map;
    std::unordered_map<std::string_view, llvm::Value*> var_map;
    std::unordered_map<std::string_view, std::unordered_map<std::string_view, unsigned>> field_map;

    void save_entry() {

    }

    void restore_entry() {

    }
    bool is_signed_int(TypeKind tkind) {
        return (tkind == TypeKind::INT8) || (tkind == TypeKind::INT16) || (tkind == TypeKind::INT32) || (tkind == TypeKind::INT64);
    }
    bool is_float(TypeKind tkind) {
        return (tkind == TypeKind::FLO32) || (tkind == TypeKind::FLO64);
    }
    bool is_unsigned_int(TypeKind tkind) {
        return (tkind == TypeKind::UINT8) || (tkind == TypeKind::UINT16) || (tkind == TypeKind::UINT32) || (tkind == TypeKind::UINT64);
    }

    // Main dispatch

    
    llvm::Value* gen_function(FunctionDecl*);
    llvm::Value* gen_block(BlockExpr*);
    llvm::Value* gen_let(LetDecl*);
    llvm::Value* gen_return(ReturnStmt*);
    llvm::Value* gen_binary(BinaryExpr*);
    llvm::Value* gen_literal(Literal*);
    llvm::Value* gen_identifier(Identifier*);
    llvm::Value* gen_call(CallExpr*);
    llvm::Value* gen_if(IfStmt*);
    llvm::Value* gen_while(WhileStmt*);
    llvm::Value* gen_assign(AssignExpr*);
    llvm::Value* gen_struct_decl(StructDecl*);
    llvm::Value* gen_struct_init(StructInit*);
    llvm::Value* gen_field(FieldExpr*);

    llvm::Value* gen_node(Node*);
    llvm::Type* llvm_type(const Type&);
};

llvm::Value* Codegen::gen_node(Node* node) {
    if (!node) return nullptr;
    switch (node->type) {
        case NodeType::FunctionDecl: return gen_function(static_cast<FunctionDecl*>(node));
        case NodeType::BlockExpr:    return gen_block(static_cast<BlockExpr*>(node));
        case NodeType::LetDecl:      return gen_let(static_cast<LetDecl*>(node));
        case NodeType::ReturnStmt:   return gen_return(static_cast<ReturnStmt*>(node));
        case NodeType::BinaryExpr:   return gen_binary(static_cast<BinaryExpr*>(node));
        case NodeType::Literal:      return gen_literal(static_cast<Literal*>(node));
        case NodeType::Identifier:   return gen_identifier(static_cast<Identifier*>(node));
        case NodeType::CallExpr:     return gen_call(static_cast<CallExpr*>(node));
        case NodeType::IfStmt:       return gen_if(static_cast<IfStmt*>(node));
        case NodeType::WhileStmt:    return gen_while(static_cast<WhileStmt*>(node));
        case NodeType::AssignExpr:   return gen_assign(static_cast<AssignExpr*>(node));
        case NodeType::ExprStmt: {
            auto* es = static_cast<ExprStmt*>(node);
            return gen_node(es->node);
        }
        default: return nullptr;
    }
}

llvm::Type* Codegen::llvm_type(const Type& t) {
    switch (t.tkind) {
        case TypeKind::INT8: return llvm::Type::getInt8Ty(*context);
        case TypeKind::INT16: return llvm::Type::getInt16Ty(*context);
        case TypeKind::INT32: return llvm::Type::getInt32Ty(*context);
        case TypeKind::INT64: return llvm::Type::getInt64Ty(*context);
        case TypeKind::UINT8: return llvm::Type::getInt8Ty(*context);
        case TypeKind::UINT16: return llvm::Type::getInt16Ty(*context);
        case TypeKind::UINT32: return llvm::Type::getInt32Ty(*context);
        case TypeKind::UINT64: return llvm::Type::getInt64Ty(*context);
        case TypeKind::FLO32: return llvm::Type::getFloatTy(*context);
        case TypeKind::FLO64: return llvm::Type::getDoubleTy(*context);
        case TypeKind::BOOL: return llvm::Type::getInt1Ty(*context);
        case TypeKind::CHAR: return llvm::Type::getInt32Ty(*context);
        case TypeKind::UNIT: return llvm::Type::getVoidTy(*context);
        case TypeKind::STRUCT: {
            auto it = struct_type_map.find(t.struct_name);
            if (it != struct_type_map.end()) return it->second;

            return llvm::StructType::create(*context, std::string(t.struct_name));
        }
        case TypeKind::STRING:
        case TypeKind::STRING_VIEW:
        case TypeKind::BUF_STRING:
            return llvm::PointerType::get(llvm::Type::getInt8Ty(*context), 0);
        default:
            return llvm::Type::getInt64Ty(*context); // fallback
    }
}

llvm::Value* Codegen::gen_function(FunctionDecl* node) {
    // Parameters
    std::vector<llvm::Type*> param_types;
    for (Node* s: node->params) {
        auto* param = static_cast<Param*>(s);
        Type t = tchecker->query_type(param->type_ann? param->type_ann : node);
        param_types.push_back(llvm_type(t));
    }

    // Return type
    Type ret_type = tchecker->query_type(node->ret_type? node->ret_type : node);
    llvm::Type* ret_llvm = llvm_type(ret_type);

    // Creating the function
    auto* fn_type = llvm::FunctionType::get(ret_llvm, param_types, false);
    // Linkage determines how symbols are visible to the linker and how they are merged during the linking process
    auto* fn = llvm::Function::Create(fn_type, llvm::Function::ExternalLinkage, std::string(node->name.get_value()), *_module);

    fn_map[node->name.get_value()] = fn;

    // Entry basic block
    auto* entry = llvm::BasicBlock::Create(*context, "entry", fn);
    builder->SetInsertPoint(entry);

    // Allocate each param, store it, register in var_map
    auto arg_it = fn->arg_begin();
    for (Node* p : node->params) {
        auto* param = static_cast<Param*>(p);
        llvm::Value* alloca = builder->CreateAlloca(arg_it->getType(), nullptr, std::string(param->name.get_value()));
        builder->CreateStore(&*arg_it, alloca);
        var_map[param->name.get_value()] = alloca;
        ++arg_it;
    }

    // Generate body
    if (node->body) gen_node(node->body);

    // Add implicit void return if needed
    if (ret_llvm->isVoidTy() && !builder->GetInsertBlock()->getTerminator())
        builder->CreateRetVoid();

    return fn;
}

llvm::Value* Codegen::gen_block(BlockExpr* node) {
    llvm::Value* return_last = nullptr;
    for (Node* o : node->opt) {
        return_last = gen_node(o);
    }
    return return_last;
}

llvm::Value* Codegen::gen_let(LetDecl* node) {
    Type t = tchecker->query_type(node);
    llvm::Type* tllvm = llvm_type(t);
    auto* pat = static_cast<Pattern*>(node->pattern);
    llvm::Value* alloca = builder->CreateAlloca(tllvm, nullptr, pat->name.get_value());
    var_map[pat->name.get_value()] = alloca;

    llvm::Value* sllvm = gen_node(node->init);
    if (!sllvm) {
        builder->CreateStore(sllvm, alloca);
    }
    return sllvm;
}

llvm::Value* Codegen::gen_return(ReturnStmt* node) {
    llvm::Value* value = gen_node(node->value);
    if (node->has_value) {
        builder->CreateRet(value);
    } else {
        builder->CreateRetVoid();
    }

    return value;
}

llvm::Value* Codegen::gen_binary(BinaryExpr* node) {
    llvm::Value* vleft = gen_node(node->left);
    llvm::Value* vright = gen_node(node->right);


    Type ttype = tchecker->query_type(node->left);
    llvm::Type* tllvm = llvm_type(ttype);

    switch (node->op.type) {
        // Arithmetic

        case TokenType::PLUS: {
            if (is_float(ttype.tkind)) {
                return builder->CreateFAdd(vleft, vright);
            } else if (is_signed_int(ttype.tkind)) {
                return builder->CreateAdd(vleft, vright);
            } else if (is_unsigned_int(ttype.tkind)) {
                return builder->CreateAdd(vleft, vright);
            }
        }
        case TokenType::MINUS: {
            if (is_float(ttype.tkind)) {
                return builder->CreateFSub(vleft, vright);
            } else if (is_signed_int(ttype.tkind)) {
                return builder->CreateSub(vleft, vright);
            } else if (is_unsigned_int(ttype.tkind)) {
                return builder->CreateSub(vleft, vright);
            }
        }
        case TokenType::STAR: {
            if (is_float(ttype.tkind)) {
                return builder->CreateFMul(vleft, vright);
            } else if (is_signed_int(ttype.tkind)) {
                return builder->CreateMul(vleft, vright);
            } else if (is_unsigned_int(ttype.tkind)) {
                return builder->CreateMul(vleft, vright);
            }
        }
        case TokenType::SLASH: {
            if (is_float(ttype.tkind)) {
                return builder->CreateFDiv(vleft, vright);
            } else if (is_signed_int(ttype.tkind)) {
                return builder->CreateSDiv(vleft, vright);
            } else if (is_unsigned_int(ttype.tkind)) {
                return builder->CreateUDiv(vleft, vright);
            }
        }
        case TokenType::PERCENT: {
            if (is_signed_int(ttype.tkind)) {
                return builder->CreateSRem(vleft, vright);
            } else if (is_unsigned_int(ttype.tkind)) {
                return builder->CreateURem(vleft, vright);
            }
        }

        // Comparison

        case TokenType::EQUAL_EQUAL: {
            if (is_float(ttype.tkind)) {
                return builder->CreateFCmpOEQ(vleft, vright);
            } else if (is_signed_int(ttype.tkind)) {
                return builder->CreateICmpEQ(vleft, vright);
            } else if (is_unsigned_int(ttype.tkind)) {
                return builder->CreateICmpEQ(vleft, vright);
            }
        }
        case TokenType::BANG_EQUAL: {
            if (is_float(ttype.tkind)) {
                return builder->CreateFCmpONE(vleft, vright);
            } else if (is_signed_int(ttype.tkind)) {
                return builder->CreateICmpNE(vleft, vright);
            } else if (is_unsigned_int(ttype.tkind)) {
                return builder->CreateICmpNE(vleft, vright);
            }
        }
        case TokenType::LESS: {
            if (is_float(ttype.tkind)) {
                return builder->CreateFCmpOLT(vleft, vright);
            } else if (is_signed_int(ttype.tkind)) {
                return builder->CreateICmpSLT(vleft, vright);
            } else if (is_unsigned_int(ttype.tkind)) {
                return builder->CreateICmpULT(vleft, vright);
            }
        }
        case TokenType::LESS_EQUAL: {
            if (is_float(ttype.tkind)) {
                return builder->CreateFCmpOLE(vleft, vright);
            } else if (is_signed_int(ttype.tkind)) {
                return builder->CreateICmpSLE(vleft, vright);
            } else if (is_unsigned_int(ttype.tkind)) {
                return builder->CreateICmpULE(vleft, vright);
            }
        }
        case TokenType::GREATER: {
            if (is_float(ttype.tkind)) {
                return builder->CreateFCmpOGT(vleft, vright);
            } else if (is_signed_int(ttype.tkind)) {
                return builder->CreateICmpSGT(vleft, vright);
            } else if (is_unsigned_int(ttype.tkind)) {
                return builder->CreateICmpUGT(vleft, vright);
            }
        }
        case TokenType::GREATER_EQUAL: {
            if (is_float(ttype.tkind)) {
                return builder->CreateFCmpOGE(vleft, vright);
            } else if (is_signed_int(ttype.tkind)) {
                return builder->CreateICmpSGE(vleft, vright);
            } else if (is_unsigned_int(ttype.tkind)) {
                return builder->CreateICmpUGE(vleft, vright);
            }
        }
        // stub

    }


}

llvm::Value* Codegen::gen_literal(Literal* node) {
    switch (node->literal) {
        case LiteralType::IntLiteral: {
            int value = std::stoi(std::string(node->token.get_value()));
            Type ttype = tchecker->query_type(node);
            llvm::Type* tllvm = llvm_type(ttype);
            return llvm::ConstantInt::get(tllvm, value, true);
        }
        case LiteralType::FloatLiteral: {
            float value = std::stof(std::string(node->token.get_value()));
            Type ttype = tchecker->query_type(node);
            llvm::Type* tllvm = llvm_type(ttype);
            return llvm::ConstantFP::get(tllvm, value);
        }
        case LiteralType::BoolLiteral: {
            Type ttype = tchecker->query_type(node);
            llvm::Type* tllvm = llvm_type(ttype);
            if (std::string(node->token.get_value()) == "true") {
                return llvm::ConstantInt::get(tllvm, 1);
            } else if (std::string(node->token.get_value()) == "false") {
                return llvm::ConstantInt::get(tllvm, 0);
            }
        }
        case LiteralType::UnitLiteral:
            return nullptr;
        case LiteralType::CharLiteral: {
            char c = node->token.get_value()[0];
            int value = int(c);
            Type ttype = tchecker->query_type(node);
            llvm::Type* tllvm = llvm_type(ttype);
            return llvm::ConstantInt::get(tllvm, value);
        }
        case LiteralType::StringLiteral: {
            // Stub
            return builder->CreateGlobalStringPtr(node->token.get_value());
        }

    }
}

llvm::Value* Codegen::gen_identifier(Identifier* node) {
    Type ttype = tchecker->query_type(node);
    llvm::Type* tllvm = llvm_type(ttype);
    auto* alloca = var_map[node->token.get_value()];
    return builder->CreateLoad(tllvm, alloca);
}

llvm::Value* Codegen::gen_call(CallExpr* node) {
    auto* tmp = static_cast<Identifier*>(node->callee);
    llvm::Function* fn = fn_map[tmp->token.get_value()];
    std::vector<llvm::Value*> args;
    for (Node* a : node->args) {
        args.push_back(gen_node(a));
    }
    return builder->CreateCall(fn, args, "calltmp");

}

llvm::Value* Codegen::gen_if(IfStmt* node) {
    llvm::Function* current_fn = builder->GetInsertBlock()->getParent();
    auto* then_block = llvm::BasicBlock::Create(*context, "name", current_fn);
    auto* else_block = llvm::BasicBlock::Create(*context, "name", current_fn);
    auto* merge_block = llvm::BasicBlock::Create(*context, "name", current_fn);

    llvm::Value* vcond = gen_node(node->node);
    builder->CreateCondBr(vcond, then_block, else_block); // Conditional branch to a block, terminates current block
    builder->SetInsertPoint(then_block);

    llvm::Value* vblock = gen_node(node->block);
    if (!builder->GetInsertBlock()->getTerminator()) {
        builder->CreateBr(merge_block); // Unconditional branch to a block, terminates current block
        builder->SetInsertPoint(else_block);
    }

    llvm::Value* vother = nullptr;
    if (node->other) {
        vother = gen_node(node->other);
    }
    if (!builder->GetInsertBlock()->getTerminator()) {
        builder->CreateBr(merge_block);
        builder->SetInsertPoint(merge_block);
    }
    
    return vother;
}  

llvm::Value* Codegen::gen_while(WhileStmt* node) {
    llvm::Function* current_fn = builder->GetInsertBlock()->getParent();
    auto* cond_block = llvm::BasicBlock::Create(*context, "name", current_fn);
    auto* body_block = llvm::BasicBlock::Create(*context, "name", current_fn);
    auto* exit_block = llvm::BasicBlock::Create(*context, "name", current_fn);

    builder->CreateBr(cond_block);
    llvm::Value* vcond = gen_node(node->condition);
    builder->CreateCondBr(vcond, body_block, exit_block);

    llvm::Value* vblock = gen_node(node->block);
    builder->CreateBr(cond_block);
    builder->SetInsertPoint(exit_block);

}

llvm::Value* Codegen::gen_assign(AssignExpr* node) {
    llvm::Value* vtarget = nullptr;
    if (node->target->type == NodeType::Identifier) {
        Identifier* tmp = static_cast<Identifier*>(node->target);
        vtarget = var_map[tmp->token.get_value()];
    } else {
        // needs completing
    }

    llvm::Value* rhvalue = gen_node(node->value);
    builder->CreateStore(rhvalue, vtarget);
    return nullptr; // needs fixing
}

llvm::Value* Codegen::gen_struct_decl(StructDecl* node) {
    
    llvm::StructType* sttllvm = llvm::StructType::create(*context, node->tag.get_value());
    std::vector<llvm::Type*> stfields;
    for (Node* f : node->opt) {
        auto* fi = static_cast<FieldDecl*>(f);
        Type ttype = tchecker->query_type(fi->type_ann);
        stfields.push_back(llvm_type(ttype));
    }
    sttllvm->setBody(stfields);
    struct_type_map[node->tag.get_value()] = sttllvm;

    for (Node* f : node->opt) {
        auto* fi = static_cast<FieldDecl*>(f);
        field_map[fi->name.get_value()][fi->name.get_value()];
    }
}

llvm::Value* Codegen::gen_struct_init(StructInit* node) {
    llvm::StructType* sttype = struct_type_map[node->name.get_value()];
    llvm::Value* strf = builder->CreateAlloca(sttype);
    for (Node* f : node->opt) {
        auto* fi = static_cast<FieldInit*>(f);
        unsigned index = field_map[fi->name.get_value()][fi->name.get_value()];
        llvm::Value* point = builder->CreateStructGEP(sttype, strf, index);
        llvm::Value* val = gen_node(fi->value);
        builder->CreateStore(val, point);
    }

    return strf;
}

llvm::Value* Codegen::gen_field(FieldExpr* node) {
    llvm::Value* object = gen_node(node->object);
    Type ttype = tchecker->query_type(node->object);
    
}