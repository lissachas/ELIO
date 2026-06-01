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
#include <llvm/Support/FileSystem.h>
#include <vector>
#include <memory>
#include <unordered_map>
#include <string>
#include <string_view>
#include <cstddef>
#include <cassert>
#include <llvm-19/llvm/Analysis/CGSCCPassManager.h>
#include <llvm-19/llvm/Analysis/LoopAnalysisManager.h>
#include <llvm-19/llvm/IR/PassManager.h>
#include <llvm-19/llvm/Passes/OptimizationLevel.h>
#include <system_error>
#include <llvm-19/llvm/IR/Constants.h>
#include <llvm-19/llvm/IR/Instructions.h>


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
    // field_map[struct_name][field_name] = positional index inside the StructType
    std::unordered_map<std::string_view, std::unordered_map<std::string_view, unsigned>> field_map;

    // Loop context
    llvm::BasicBlock* loop_exit_bb = nullptr;
    llvm::BasicBlock* loop_continue_bb = nullptr;

    std::unordered_map<std::string_view, llvm::Value*> save_scope() {
        return var_map;
    }

    void restore_scope(std::unordered_map<std::string_view, llvm::Value*> saved) {
        var_map = std::move(saved);
    }

    // Type helpers
    bool is_signed_int(TypeKind tkind) {
        return (tkind == TypeKind::INT8) || (tkind == TypeKind::INT16) || (tkind == TypeKind::INT32) || (tkind == TypeKind::INT64);
    }
    bool is_float(TypeKind tkind) {
        return (tkind == TypeKind::FLO32) || (tkind == TypeKind::FLO64);
    }
    bool is_unsigned_int(TypeKind tkind) {
        return (tkind == TypeKind::UINT8) || (tkind == TypeKind::UINT16) || (tkind == TypeKind::UINT32) || (tkind == TypeKind::UINT64);
    }
    bool is_integer(TypeKind tkind) {
        return is_signed_int(tkind) || is_unsigned_int(tkind) || tkind == TypeKind::BOOL || tkind == TypeKind::CHAR;
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
    llvm::Value* gen_loop(LoopStmt*);
    llvm::Value* gen_assign(AssignExpr*);
    llvm::Value* gen_struct_decl(StructDecl*);
    llvm::Value* gen_struct_init(StructInit*);
    llvm::Value* gen_field(FieldExpr*);
    llvm::Value* gen_break(BreakStmt*);
    llvm::Value* gen_continue(ContinueStmt*);

    llvm::Value* gen_lvalue(Node* node);
    llvm::Value* gen_node(Node*);
    llvm::Type* llvm_type(const Type&);
};


// -----------------------------------------------------

// M A I N D I S P A T C H

// -----------------------------------------------------

// Passess -> Structs -> Function signatures -> Bodies
void Codegen::generate(Node* program) {
    assert(tchecker && "call set_type_checker() before generate");
    auto* root = static_cast<BlockExpr*>(program);

    // Pass 1: register all struct layouts
    for (Node* n : root->opt) {
        if (n->type == NodeType::StructDecl)
            gen_struct_decl(static_cast<StructDecl*>(n));
    }

    // Pass 2: register all function signatures
    for (Node* n : root->opt) {
        if (n->type != NodeType::FunctionDecl) continue;
        auto* fd = static_cast<FunctionDecl*>(n);

        std::vector<llvm::Type*> param_types;
        for (Node* p : fd->params) {
            auto* param = static_cast<Param*>(p);
            Type t = tchecker->query_type(param->type_ann ? param->type_ann : param);
            param_types.push_back(llvm_type(t));
        }
        Type ret_t = tchecker->query_type(fd->ret_type ? fd->ret_type : fd);
        auto* fn_type = llvm::FunctionType::get(llvm_type(ret_t), param_types, false);
        // Linkage determines how symbols are visible to the linker and how they are merged during the linking process
        auto* fn = llvm::Function::Create(fn_type, llvm::Function::ExternalLinkage, std::string(fd->name.get_value()), *_module);
        fn_map[fd->name.get_value()] = fn;
    }

    // Pass 3: generate function bodies
    for (Node* n : root->opt) {
        if (n->type == NodeType::FunctionDecl)
            gen_function(static_cast<FunctionDecl*>(n));
    }
}


// verify -> optimize -> write .ll file
void Codegen::emit_ir(const std::string& path) {
    std::string err_str;
    llvm::raw_string_ostream err_os(err_str);
    if (llvm::verifyModule(*_module, &err_os)) {
        llvm::errs() << "IR verification failed:\n" << err_str << "\n";
        return;
    }

    llvm::PassBuilder pb;
    llvm::LoopAnalysisManager lam;
    llvm::FunctionAnalysisManager fam;
    llvm::CGSCCAnalysisManager cgam;
    llvm::ModuleAnalysisManager mam;
    pb.registerModuleAnalyses(mam);
    pb.registerCGSCCAnalyses(cgam);
    pb.registerFunctionAnalyses(fam);
    pb.registerLoopAnalyses(lam);
    pb.crossRegisterProxies(lam, fam, cgam, mam);
    auto mpm = pb.buildPerModuleDefaultPipeline(llvm::OptimizationLevel::O1);
    mpm.run(*_module, mam);

    std::error_code ec;
    llvm::raw_fd_ostream out(path, ec, llvm::sys::fs::OF_Text);
    if (ec) {
        llvm::errs() << "Cannot open output: " << ec.message() << "\n";
        return;
    }
   _module->print(out, nullptr);
}




// Central dispatch
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
        case NodeType::LoopStmt:     return gen_loop(static_cast<LoopStmt*>(node));
        case NodeType::AssignExpr:   return gen_assign(static_cast<AssignExpr*>(node));
        case NodeType::StructDecl:   return gen_struct_decl(static_cast<StructDecl*>(node));
        case NodeType::StructInit:   return gen_struct_init(static_cast<StructInit*>(node));
        case NodeType::FieldExpr:    return gen_field(static_cast<FieldExpr*>(node));
        case NodeType::BreakStmt:    return gen_break(static_cast<BreakStmt*>(node));
        case NodeType::ContinueStmt: return gen_continue(static_cast<ContinueStmt*>(node));
        case NodeType::ExprStmt: {
            auto* es = static_cast<ExprStmt*>(node);
            return gen_node(es->node);
        }
        default: return nullptr;
    }
}

// Direct type conversion
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
        case TypeKind::UNIT:
        case TypeKind::UNIT_: 
        return llvm::Type::getVoidTy(*context);
        case TypeKind::STRUCT: {
            auto it = struct_type_map.find(t.struct_name);
            if (it != struct_type_map.end()) return it->second;

            return llvm::StructType::create(*context, std::string(t.struct_name));
        }
        case TypeKind::REF:
        case TypeKind::MUTREF:
        case TypeKind::STRING:
        case TypeKind::STRING_VIEW:
        case TypeKind::BUF_STRING:
            // Stub: i8*
            return llvm::PointerType::get(llvm::Type::getInt8Ty(*context), 0);
        default:
            return llvm::Type::getInt64Ty(*context); // fallback
    }
}

// ----------------------------------------------------

// A L L F U N C T I O N S

// ----------------------------------------------------

// already halfway complete in generate
llvm::Value* Codegen::gen_function(FunctionDecl* node) {
    llvm::Function* fn = fn_map[node->name.get_value()];
    if (!fn || !node->body) return fn; // forward declaration or haven't yet been registered

    auto* entry = llvm::BasicBlock::Create(*context, "entry", fn);
    builder->SetInsertPoint(entry);

    // Isolate function scope?
    auto outer = save_scope();
    var_map.clear();

    auto arg_it = fn->arg_begin();
    for (Node* p : node->params) {
        auto* param = static_cast<Param*>(p);
        llvm::AllocaInst* slot = builder->CreateAlloca(arg_it->getType(), nullptr, std::string(param->name.get_value()));
        builder->CreateStore(&*arg_it, slot);
        var_map[param->name.get_value()] = slot;
        ++arg_it;
    }

    gen_node(node->body);

    llvm::BasicBlock* last = builder->GetInsertBlock();
    if (last && !last->getTerminator()) {
        llvm::Type* ret = fn->getReturnType();
        if (ret->isVoidTy())
            builder->CreateRetVoid();
        else
            builder->CreateRet(llvm::UndefValue::get(ret));
    }

    restore_scope(std::move(outer));
    return fn;
}

// iterate statements, return last value
llvm::Value* Codegen::gen_block(BlockExpr* node) {
    auto saved = save_scope();
    llvm::Value* return_last = nullptr;
    for (Node* o : node->opt) {
        return_last = gen_node(o);
    }
    restore_scope(std::move(saved));
    return return_last;
}

// 
llvm::Value* Codegen::gen_let(LetDecl* node) {
    Type t = tchecker->query_type(node);
    llvm::Type* tllvm = llvm_type(t);
    auto* pat = static_cast<Pattern*>(node->pattern);

    llvm::AllocaInst* alloca = builder->CreateAlloca(tllvm, nullptr, std::string(pat->name.get_value()));
    var_map[pat->name.get_value()] = alloca;

    if (node->init) {
        llvm::Value* sllvm = gen_node(node->init);

        if (sllvm) {
            builder->CreateStore(sllvm, alloca);
        }
    }
    
    return alloca;
}

llvm::Value* Codegen::gen_return(ReturnStmt* node) {
    if (!node->has_value) {
        builder->CreateRetVoid();
        return nullptr;
    } 

    // CreateRet/CreateRetVoid both terminate the current BasicBlock.
    llvm::Value* value = gen_node(node->value);
    if (value)
        builder->CreateRet(value);
    else
        builder->CreateRetVoid();

    return value;
}

llvm::Value* Codegen::gen_binary(BinaryExpr* node) {
    Type ttype = tchecker->query_type(node->left);

    // needs completion
    if (node->op.type == TokenType::AND_AND || node->op.type == TokenType::OR_OR) {
        llvm::Value* vleft = gen_node(node->left);
        llvm::Value* vright = gen_node(node->right);
        if (!vleft || !vright) return nullptr;
        return node->op.type == TokenType::AND_AND ? builder->CreateAnd(vleft, vright, "and") : builder->CreateOr(vleft, vright, "or");
    }
    
    llvm::Value* vleft = gen_node(node->left);
    llvm::Value* vright = gen_node(node->right);
    if (!vleft || !vright) return nullptr;


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
            return nullptr;
        }
        case TokenType::MINUS: {
            if (is_float(ttype.tkind)) {
                return builder->CreateFSub(vleft, vright);
            } else if (is_signed_int(ttype.tkind)) {
                return builder->CreateSub(vleft, vright);
            } else if (is_unsigned_int(ttype.tkind)) {
                return builder->CreateSub(vleft, vright);
            }
            return nullptr;
        }
        case TokenType::STAR: {
            if (is_float(ttype.tkind)) {
                return builder->CreateFMul(vleft, vright);
            } else if (is_signed_int(ttype.tkind)) {
                return builder->CreateMul(vleft, vright);
            } else if (is_unsigned_int(ttype.tkind)) {
                return builder->CreateMul(vleft, vright);
            }
            return nullptr;
        }
        case TokenType::SLASH: {
            if (is_float(ttype.tkind)) {
                return builder->CreateFDiv(vleft, vright);
            } else if (is_signed_int(ttype.tkind)) {
                return builder->CreateSDiv(vleft, vright);
            } else if (is_unsigned_int(ttype.tkind)) {
                return builder->CreateUDiv(vleft, vright);
            }
            return nullptr;
        }
        case TokenType::PERCENT: {
            if (is_signed_int(ttype.tkind)) {
                return builder->CreateSRem(vleft, vright);
            } else if (is_unsigned_int(ttype.tkind)) {
                return builder->CreateURem(vleft, vright);
            }
            return nullptr;
        }

        // Comparison

        case TokenType::EQUAL_EQUAL: {
            if (is_float(ttype.tkind)) {
                return builder->CreateFCmpOEQ(vleft, vright);
            } else if (is_integer(ttype.tkind)) {
                return builder->CreateICmpEQ(vleft, vright);
            }
            return nullptr;
        }
        case TokenType::BANG_EQUAL: 
        case TokenType::NOT_EQUAL:
        {
            if (is_float(ttype.tkind)) {
                return builder->CreateFCmpONE(vleft, vright);
            } else if (is_integer(ttype.tkind)) {
                return builder->CreateICmpNE(vleft, vright);
            }
            return nullptr;
        }
        case TokenType::LESS: {
            if (is_float(ttype.tkind)) {
                return builder->CreateFCmpOLT(vleft, vright);
            } else if (is_signed_int(ttype.tkind)) {
                return builder->CreateICmpSLT(vleft, vright);
            } else if (is_unsigned_int(ttype.tkind)) {
                return builder->CreateICmpULT(vleft, vright);
            }
            return nullptr;
        }
        case TokenType::LESS_EQUAL: {
            if (is_float(ttype.tkind)) {
                return builder->CreateFCmpOLE(vleft, vright);
            } else if (is_signed_int(ttype.tkind)) {
                return builder->CreateICmpSLE(vleft, vright);
            } else if (is_unsigned_int(ttype.tkind)) {
                return builder->CreateICmpULE(vleft, vright);
            }
            return nullptr;
        }
        case TokenType::GREATER: {
            if (is_float(ttype.tkind)) {
                return builder->CreateFCmpOGT(vleft, vright);
            } else if (is_signed_int(ttype.tkind)) {
                return builder->CreateICmpSGT(vleft, vright);
            } else if (is_unsigned_int(ttype.tkind)) {
                return builder->CreateICmpUGT(vleft, vright);
            }
            return nullptr;
        }
        case TokenType::GREATER_EQUAL: {
            if (is_float(ttype.tkind)) {
                return builder->CreateFCmpOGE(vleft, vright);
            } else if (is_signed_int(ttype.tkind)) {
                return builder->CreateICmpSGE(vleft, vright);
            } else if (is_unsigned_int(ttype.tkind)) {
                return builder->CreateICmpUGE(vleft, vright);
            }
            return nullptr;
        }
        // stub
        default: return nullptr;
    }


}

llvm::Value* Codegen::gen_literal(Literal* node) {

    switch (node->literal) {

        case LiteralType::IntLiteral: {
            int value = std::stoll(std::string(node->token.get_value()));
            Type ttype = tchecker->query_type(node);
            llvm::Type* tllvm = llvm_type(ttype);
            return llvm::ConstantInt::get(tllvm, value, true);
        }
        case LiteralType::FloatLiteral: {
            float value = std::stod(std::string(node->token.get_value()));
            Type ttype = tchecker->query_type(node);
            llvm::Type* tllvm = llvm_type(ttype);
            return llvm::ConstantFP::get(tllvm, value);
        }
        case LiteralType::BoolLiteral: {
            if (node->token.get_value() == "true") {
                return llvm::ConstantInt::get(llvm::Type::getInt1Ty(*context), 1);
            } else if (std::string(node->token.get_value()) == "false") {
                return llvm::ConstantInt::get(llvm::Type::getInt1Ty(*context), 0);
            }
        }
        case LiteralType::UnitLiteral:
            return nullptr;
        // needs completion
        case LiteralType::CharLiteral: {
            char c = node->token.get_value()[0];
            int value = int(c);
            Type ttype = tchecker->query_type(node);
            llvm::Type* tllvm = llvm_type(ttype);
            return llvm::ConstantInt::get(tllvm, value);
        }
        case LiteralType::StringLiteral: {
            // Stub
            std::string raw = std::string(node->token.get_value());
            if (raw.size() >= 2 && raw.front() == '"' && raw.back() == '"')
                raw = raw.substr(1, raw.size() - 2);
            return builder->CreateGlobalStringPtr(raw, "str");
        }
    }
    return nullptr;
}

llvm::Value* Codegen::gen_identifier(Identifier* node) {
    auto it = var_map.find(node->token.get_value());
    if (it == var_map.end()) return nullptr;
    auto* slot = static_cast<llvm::AllocaInst*>(it->second);
    // getAllocatedType(): method on AllocaInst* that returns the type of what is stored at the allocation. Used as the first argument to CreateLoad.
    return builder->CreateLoad(slot->getAllocatedType(), slot, std::string(node->token.get_value()));
}

llvm::Value* Codegen::gen_call(CallExpr* node) {
    auto* id = static_cast<Identifier*>(node->callee);
    auto it = fn_map.find(id->token.get_value());
    if (it == fn_map.end()) return nullptr;
    llvm::Function* fn = it->second;
    
    std::vector<llvm::Value*> args;
    args.reserve(node->args.size());
    for (Node* a : node->args) {
        llvm::Value* v = gen_node(a);
        if (v) args.push_back(v);
    }

    if (!fn->getReturnType()->isVoidTy())
        return builder->CreateCall(fn, args, "calltmp");
    else 
        return builder->CreateCall(fn, args);

}

// gen condition -> term block -> set then -> ?create branch -> set else -> gen else -> ?create branch -> set merge
llvm::Value* Codegen::gen_if(IfStmt* node) {
    llvm::Value* vcond = gen_node(node->node);
    if (!vcond) return nullptr;

    llvm::Function* current_fn = builder->GetInsertBlock()->getParent();
    auto* then_block = llvm::BasicBlock::Create(*context, "name", current_fn);
    auto* else_block = llvm::BasicBlock::Create(*context, "name", current_fn);
    auto* merge_block = llvm::BasicBlock::Create(*context, "name", current_fn);

    builder->CreateCondBr(vcond, then_block, else_block); // Conditional branch to a block, terminates current block

    // Then branch
    builder->SetInsertPoint(then_block);
    gen_node(node->block);
    if (!builder->GetInsertBlock()->getTerminator()) {
        builder->CreateBr(merge_block); // Unconditional branch to a block, terminates current block
    }

    // Else branch
    builder->SetInsertPoint(else_block);
    if (node->other)
        gen_node(node->other);
    if (!builder->GetInsertBlock()->getTerminator())
        builder->CreateBr(merge_block);

    builder->SetInsertPoint(merge_block);

    return nullptr;
}

llvm::Value* Codegen::gen_while(WhileStmt* node) {
    llvm::Function* current_fn = builder->GetInsertBlock()->getParent();
    auto* cond_block = llvm::BasicBlock::Create(*context, "while.cond", current_fn);
    auto* body_block = llvm::BasicBlock::Create(*context, "while.body", current_fn);
    auto* exit_block = llvm::BasicBlock::Create(*context, "while.exit", current_fn);

    llvm::BasicBlock* outer_exit = loop_exit_bb;
    llvm::BasicBlock* outer_continue = loop_continue_bb;
    loop_exit_bb = exit_block;
    loop_continue_bb = cond_block;

    builder->CreateBr(cond_block);
    builder->SetInsertPoint(cond_block);
    llvm::Value* vcond = gen_node(node->condition);
    builder->CreateCondBr(vcond, body_block, exit_block);

    builder->SetInsertPoint(body_block);
    gen_node(node->block);
    
    if (!builder->GetInsertBlock()->getTerminator()) {
        builder->CreateBr(cond_block);
    }
    loop_exit_bb = outer_exit;
    loop_continue_bb = outer_continue;

    builder->SetInsertPoint(exit_block);
    return nullptr;
}

llvm::Value* Codegen::gen_loop(LoopStmt* node) {
    llvm::Function* fn = builder->GetInsertBlock()->getParent();
    auto* body_block = llvm::BasicBlock::Create(*context, "loop.body", fn);
    auto* exit_block = llvm::BasicBlock::Create(*context, "loop.exit", fn);

    llvm::BasicBlock* outer_exit     = loop_exit_bb;
    llvm::BasicBlock* outer_continue = loop_continue_bb;
    loop_exit_bb     = exit_block;
    loop_continue_bb = body_block; 

    builder->CreateBr(body_block);
    builder->SetInsertPoint(body_block);
    gen_node(node->block);
    if (!builder->GetInsertBlock()->getTerminator())
        builder->CreateBr(body_block);

    loop_exit_bb     = outer_exit;
    loop_continue_bb = outer_continue;

    builder->SetInsertPoint(exit_block);
    return nullptr;
}

llvm::Value* Codegen::gen_assign(AssignExpr* node) {
    llvm::Value* target_ptr = gen_lvalue(node->target);
    llvm::Value* rhs = gen_node(node->value);

    if (target_ptr && rhs)
        builder->CreateStore(rhs, target_ptr);
    return nullptr;
}

llvm::Value* Codegen::gen_lvalue(Node* node) {
    if (node->type == NodeType::Identifier) {
        auto* id = static_cast<Identifier*>(node);
        auto it = var_map.find(id->token.get_value());
        return it != var_map.end() ? it->second : nullptr;
    }
    if (node->type == NodeType::FieldExpr) {
        auto* fe = static_cast<FieldExpr*>(node);
        llvm::Value* obj_ptr = gen_lvalue(fe->object);
        if (!obj_ptr) return nullptr;

        Type obj_type = tchecker->query_type(fe->object);
        if (obj_type.tkind != TypeKind::STRUCT) return nullptr;

        auto sit = struct_type_map.find(obj_type.struct_name);
        if (sit == struct_type_map.end()) return nullptr;

        auto& fields = field_map[obj_type.struct_name];
        auto fit = fields.find(fe->field.get_value());
        if (fit == fields.end()) return nullptr;

        return builder->CreateStructGEP(sit->second, obj_ptr, fit->second, std::string(fe->field.get_value()));
    }
    return nullptr;
}

// Returns pointer to storage of assignable node
llvm::Value* Codegen::gen_struct_decl(StructDecl* node) {
    std::string_view name = node->tag.get_value();
    auto* stype = llvm::StructType::create(*context, std::string(name));
    struct_type_map[name] = stype;

    std::vector<llvm::Type*> field_types;
    unsigned idx = 0;
    for (Node* f : node->opt) {
        auto* fd = static_cast<FieldDecl*>(f);
        Type ft = tchecker->query_type(fd->type_ann ? fd->type_ann : fd);

        field_types.push_back(llvm_type(ft));
        field_map[name][fd->name.get_value()] = idx++;
    }

    stype->setBody(field_types);
    return nullptr;
}

llvm::Value* Codegen::gen_struct_init(StructInit* node) {
    std::string_view sname = node->name.get_value();
    auto sit = struct_type_map.find(sname);
    if (sit == struct_type_map.end()) return nullptr;
    llvm::StructType* stype = sit->second;

    // CreateAlloca(StructType*): allocates sizeof(struct) bytes on the stack
    llvm::AllocaInst* alloc = builder->CreateAlloca(stype, nullptr, std::string(sname));

    for (Node* f : node->opt) {
        auto* fi = static_cast<FieldInit*>(f);

        auto& fields = field_map[sname];
        auto fit = fields.find(fi->name.get_value());
        if (fit == fields.end()) continue;
        
        // CreateStructGEP(Type, Ptr, Index): pointer to a specific field
        llvm::Value* field_ptr = builder->CreateStructGEP(
            stype, alloc, fit->second, std::string(fi->name.get_value()));
        
        llvm::Value* val = nullptr;
        if (fi->shorthand) {
            auto vit = var_map.find(fi->name.get_value());
            if (vit != var_map.end()) {
                auto* vslot = static_cast<llvm::AllocaInst*>(vit->second);
                val = builder->CreateLoad(vslot->getAllocatedType(), vit->second);
            }
        } else {
            val = gen_node(fi->value);
        }
        // CreateStore: writes the field value into the field slot
        if (val) builder->CreateStore(val, field_ptr);
    }

    return alloc;
}

llvm::Value* Codegen::gen_field(FieldExpr* node) {
    llvm::Value* field_ptr = gen_lvalue(node);
    if (!field_ptr) return nullptr;
    Type ftype = tchecker->query_type(node);
    return builder->CreateLoad(llvm_type(ftype), field_ptr, std::string(node->field.get_value()));
}

llvm::Value* Codegen::gen_break(BreakStmt* node) {
    if (loop_exit_bb)
        builder->CreateBr(loop_exit_bb);
    return nullptr;
}

llvm::Value* Codegen::gen_continue(ContinueStmt* node) {
    if (loop_continue_bb)
        builder->CreateBr(loop_continue_bb);
    return nullptr;
}