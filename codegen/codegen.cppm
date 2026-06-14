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
#include <llvm/Support/TargetSelect.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/MC/TargetRegistry.h>


export module codegen;

import expr;
import typecheck;
import tokens;
import error;

export class Codegen {
    public:
    Codegen (Diagnostics* diag) : diag{diag} {
        this->context = std::make_unique<llvm::LLVMContext>();
        this->_module = std::make_unique<llvm::Module>("elio", *context);

        llvm::InitializeNativeTarget();
        llvm::InitializeNativeTargetAsmPrinter();

        this->builder = std::unique_ptr<llvm::IRBuilder<>>(new llvm::IRBuilder<>(*context));
        std::string triple = LLVM_DEFAULT_TARGET_TRIPLE;
        _module->setTargetTriple(triple);

        std::string err;
        const llvm::Target* target = llvm::TargetRegistry::lookupTarget(triple, err);
        if (target) {
            auto* tm = target->createTargetMachine(
                triple, "generic", "",
                llvm::TargetOptions{},
                llvm::Reloc::PIC_,      // position-independent code
                llvm::CodeModel::Small,
                llvm::CodeGenOptLevel::None
            );
            if (tm) _module->setDataLayout(tm->createDataLayout());
        }
    }

    void generate(Node* program);
    void emit_ir(const std::string& path, bool optimize);
    void set_type_checker(TypeChecker* tc) {
        tchecker = tc;
    }


    private:
    Diagnostics* diag;
    std::unique_ptr<llvm::LLVMContext> context; // First create unique Context object to tie whole code generation together. Use Context to get access to LLVM data structures like modules and IRBuilder objects
    std::unique_ptr<llvm::Module> _module; // Create named global variables and query them
    std::unique_ptr<llvm::IRBuilder<>> builder; // Object. Incrementally build up IR. Acts something like a current pointer
    TypeChecker* tchecker = nullptr;

    // MAPS:
    std::unordered_map<std::string_view, llvm::StructType*>  struct_type_map;
    std::unordered_map<std::string_view, llvm::Function*>  fn_map;
    std::unordered_map<FunctionDecl*, llvm::Function*> fn_decl_map; // keyed by AST
    std::unordered_map<std::string_view, llvm::Value*> var_map;
    // field_map[struct_name][field_name] = positional index inside the StructType
    std::unordered_map<std::string_view, std::unordered_map<std::string_view, unsigned>> field_map;
    std::unordered_map<std::string_view, llvm::StructType*> enum_type_map;
    std::unordered_map<std::string_view,
    std::unordered_map<std::string_view, llvm::StructType*>> variant_payload_type;
    std::unordered_map<std::string_view,
    std::unordered_map<std::string_view, unsigned>> variant_index_map;
    std::unordered_map<std::string_view, EnumDecl*> enum_decl_map;

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

    std::string tchecker_type_tag(const Type& t) {
    switch (t.tkind) {
        case TypeKind::INT8:    return "i8";
        case TypeKind::INT16:   return "i16";
        case TypeKind::INT32:   return "i32";
        case TypeKind::INT64:   return "i64";
        case TypeKind::UINT8:   return "u8";
        case TypeKind::UINT16:  return "u16";
        case TypeKind::UINT32:  return "u32";
        case TypeKind::UINT64:  return "u64";
        case TypeKind::FLO32:   return "f32";
        case TypeKind::FLO64:   return "f64";
        case TypeKind::BOOL:    return "bool";
        case TypeKind::CHAR:    return "char";
        case TypeKind::STRING:  return "str";
        case TypeKind::UNIT:    return "unit";
        case TypeKind::STRUCT:  return std::string(t.struct_name);
        case TypeKind::ARRAY:
            return "arr$" + tchecker_type_tag(*t.inner);
        case TypeKind::OPTIONAL:
            return "opt$" + tchecker_type_tag(*t.inner);
        default:                return "unk";
    }
}


    std::string mangle(FunctionDecl* fd) {
        std::string m = std::string(fd->name.get_value());
        for (Node* p : fd->params) {
            auto* param = static_cast<Param*>(p);
            Type t = tchecker->resolve_type(param->type_ann);
            m += "$" + tchecker_type_tag(t); 
        }
        return m;
    }

    // Another overload helper
    llvm::Value* coerce(llvm::Value* v, Type from, Type to) {
        if (from.tkind == to.tkind) return v;
        llvm::Type* dest = llvm_type(to);
        // Floating-Point Extend. Converts float (f32) to double (f64) without loss.
        if (is_float(from.tkind) && is_float(to.tkind))
            return builder->CreateFPExt(v, dest, "fpext");
        // Sign-Extend. Widens a signed integer to a larger integer type by copying the sign bit into the new high bits, so -1 : i8 stays -1 : i32
        if (is_signed_int(from.tkind))
            return builder->CreateSExt(v, dest, "sext");
        // Zero-Extend. Widens an unsigned integer by filling the new high bits with zeros.
        if (is_unsigned_int(from.tkind))
            return builder->CreateZExt(v, dest, "zext");
        return v;
    }

    unsigned variant_index(std::string_view ename, std::string_view vname) {
        return variant_index_map[ename][vname]; // 0 if not found, which is wrong but safe
    }

    EnumVariant* find_variant_decl(EnumDecl* en, std::string_view vname) {
        for (Node* vn : en->variants) {
            auto* v = static_cast<EnumVariant*>(vn);
            if (v->name.get_value() == vname) return v;
        }
        return nullptr;
    }

    llvm::Value* literal_constant(const Token& tok, const Type& vt) {
        llvm::Type* lt = llvm_type(vt);
        std::string s(tok.get_value());
        if (vt.tkind == TypeKind::BOOL) return llvm::ConstantInt::get(lt, s == "true" ? 1 : 0);
        if (is_float(vt.tkind))         return llvm::ConstantFP::get(lt, std::stod(s));
        if (vt.tkind == TypeKind::CHAR) return llvm::ConstantInt::get(lt, (int)s[0]); // refine escapes
        return llvm::ConstantInt::get(lt, std::stoll(s), /*signed*/true);
    }

    void bind_variant_payload(llvm::StructType* etype, llvm::Value* subj, std::string_view ename, Pattern* pat) {
        auto pit = variant_payload_type.find(ename);

        if (pit == variant_payload_type.end()) return;

        auto sit = pit->second.find(pat->name.get_value());
        if (sit == pit->second.end()) return; // unit variant, no payload to bind

        llvm::StructType* pstruct = sit->second;
        llvm::Value* buf_ptr = builder->CreateStructGEP(etype, subj, 1, "buf");
        llvm::Value* typed   = builder->CreateBitCast(
            buf_ptr, pstruct->getPointerTo(), "payload");

        for (size_t i = 0; i < pat->fields.size(); ++i) {
            auto* inner_pat = static_cast<Pattern*>(pat->fields[i]);
            if (inner_pat->pat_type == PatternType::Identifier) {
                llvm::Value* fp  = builder->CreateStructGEP(pstruct, typed, i, "field");
                llvm::Type*  ft  = pstruct->getElementType(i);
                llvm::Value* val = builder->CreateLoad(ft, fp, inner_pat->name.get_value());
                llvm::Value* alloca = builder->CreateAlloca(ft, nullptr, inner_pat->name.get_value());
                builder->CreateStore(val, alloca);
                var_map[inner_pat->name.get_value()] = alloca;
            }
        }
    }

    // Main dispatch


    llvm::Value* gen_function(FunctionDecl*);
    llvm::Value* gen_block(BlockExpr*);
    llvm::Value* gen_let(LetDecl*);
    llvm::Value* gen_return(ReturnStmt*);
    llvm::Value* gen_unary(UnaryExpr*);
    llvm::Value* gen_binary(BinaryExpr*);
    llvm::Value* gen_literal(Literal*);
    llvm::Value* gen_identifier(Identifier*);
    llvm::Value* gen_call(CallExpr*);
    llvm::Value* gen_if(IfStmt*);
    llvm::Value* gen_while(WhileStmt*);
    llvm::Value* gen_loop(LoopStmt*);
    llvm::Value* gen_for(ForStmt*);
    llvm::Value* gen_assign(AssignExpr*);
    llvm::Value* gen_struct_decl(StructDecl*);
    llvm::Value* gen_struct_init(StructInit*);
    llvm::Value* gen_field(FieldExpr*);
    llvm::Value* gen_break(BreakStmt*);
    llvm::Value* gen_continue(ContinueStmt*);
    llvm::Value* gen_index(IndexExpr*);
    llvm::Value* gen_const(ConstDecl*);
    llvm::Value* gen_if_expr(IfExpr*);
    llvm::Value* gen_enum_decl(EnumDecl*);
    llvm::Value* gen_enum_ctor(CallExpr*);
    llvm::Value* gen_match(MatchExpr*);
    llvm::Value* gen_match_stmt(MatchStmt*);
    llvm::Value* gen_match_expr(MatchExpr*);
    void test_pattern(llvm::Value*, const Type&, Pattern*, llvm::BasicBlock*);
    llvm::Value* scrutinee_ptr(Node*, const Type&);

    llvm::Value* gen_lvalue(Node* node);
    llvm::Value* gen_node(Node*);
    llvm::Type* llvm_type(const Type&);
    llvm::Value* gen_builtin_call(CallExpr*, std::string_view name);
    llvm::Function* get_or_declare_printf();
    llvm::Function* get_or_declare_scanf();
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
            Type t = param->type_ann
            ? tchecker->resolve_type(param->type_ann)
            : Type::make(TypeKind::UNKNOWN);
            param_types.push_back(llvm_type(t));
        }
        Type ret_t = fd->ret_type
        ? tchecker->resolve_type(fd->ret_type)
        : Type::make(TypeKind::UNIT);

        auto* fn_type = llvm::FunctionType::get(llvm_type(ret_t), param_types, false);
        // Linkage determines how symbols are visible to the linker and how they are merged during the linking process
        auto* fn = llvm::Function::Create(
            fn_type, llvm::Function::ExternalLinkage, mangle(fd), *_module);
        fn_decl_map[fd] = fn;
    }

    // Pass 3: generate function bodies
    for (Node* n : root->opt) {
        if (n->type == NodeType::FunctionDecl)
            gen_function(static_cast<FunctionDecl*>(n));
    }
}


// verify -> optimize -> write .ll file
void Codegen::emit_ir(const std::string& path, bool optimize) {
    std::string err_str;
    llvm::raw_string_ostream err_os(err_str);
    if (llvm::verifyModule(*_module, &err_os)) {
        diag->error(ErrorStage::Codegen, 0, "",
                    "IR verification failed: " + err_str);
        return;
    }

    if (optimize) {
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
    }
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
        case NodeType::UnaryExpr:    return gen_unary(static_cast<UnaryExpr*>(node));
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
        case NodeType::IndexExpr: return gen_index(static_cast<IndexExpr*>(node));
        case NodeType::ConstDecl: return gen_const(static_cast<ConstDecl*>(node));
        case NodeType::IfExpr:    return gen_if_expr(static_cast<IfExpr*>(node));
        case NodeType::ForStmt: return gen_for(static_cast<ForStmt*>(node));
        case NodeType::ExprStmt: {
            auto* es = static_cast<ExprStmt*>(node);
            return gen_node(es->node);
        }
        case NodeType::MatchStmt: return gen_match_stmt(static_cast<MatchStmt*>(node));
        case NodeType::MatchExpr: return gen_match_expr(static_cast<MatchExpr*>(node));
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
        case TypeKind::ENUM: {
            auto it = enum_type_map.find(t.struct_name);
            if (it != enum_type_map.end()) return it->second;
            return llvm::StructType::create(*context, std::string(t.struct_name));
        }
        case TypeKind::OPTIONAL: {
            // { i1 present, T value }
            llvm::Type* inner = llvm_type(*t.inner);
            return llvm::StructType::get(*context, { llvm::Type::getInt1Ty(*context), inner });
        }
        case TypeKind::RESULT: {
            // { i1 is_err, T ok, E err }
            llvm::Type* ok  = llvm_type(*t.inner);
            llvm::Type* err = llvm_type(*t.inner2);
            return llvm::StructType::get(*context, { llvm::Type::getInt1Ty(*context), ok, err });
        }
        default:
            return llvm::Type::getInt64Ty(*context); // fallback
    }
}

// ----------------------------------------------------

// A L L F U N C T I O N S

// ----------------------------------------------------

// already halfway complete in generate
llvm::Value* Codegen::gen_function(FunctionDecl* node) {
    llvm::Function* fn = fn_decl_map[node];
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
    Type t = node->type_ann
        ? tchecker->resolve_type(node->type_ann)
        : tchecker->query_type(node);

    llvm::Type* tllvm = llvm_type(t);
    auto* pat = static_cast<Pattern*>(node->pattern);
    std::string vname = std::string(pat->name.get_value());

    if (node->init && node->init->type == NodeType::StructInit) {
        llvm::Value* struct_alloca = gen_node(node->init);
        if (struct_alloca) var_map[pat->name.get_value()] = struct_alloca;
        return struct_alloca;
    }


    llvm::AllocaInst* alloca = builder->CreateAlloca(tllvm, nullptr, vname);
    var_map[pat->name.get_value()] = alloca;

    if (node->init) {
        llvm::Value* val = gen_node(node->init);

        if (val) {
            if (val->getType()->isPointerTy() && !tllvm->isPointerTy())
                val = builder->CreateLoad(tllvm, val, "initval");
            builder->CreateStore(val, alloca);
        }
    }
    
    return alloca;
}

llvm::Value* Codegen::gen_const(ConstDecl* node) {
    Type t = node->type_ann
        ? tchecker->resolve_type(node->type_ann)
        : tchecker->query_type(node);
    llvm::AllocaInst* alloca = builder->CreateAlloca(
        llvm_type(t), nullptr, std::string(node->ident.get_value()));
    var_map[node->ident.get_value()] = alloca;
    if (node->init) {
        llvm::Value* val = gen_node(node->init);
        if (val) {
            if (val->getType()->isPointerTy() && !llvm_type(t)->isPointerTy())
                val = builder->CreateLoad(llvm_type(t), val, "constval");
            builder->CreateStore(val, alloca);
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
    if (!value) {
        builder->CreateRetVoid();
        return nullptr;
    }
        
    llvm::Function* fn = builder->GetInsertBlock()->getParent();
    llvm::Type* ret_type = fn->getReturnType();

    if (value->getType()->isPointerTy() && !ret_type->isPointerTy()) {
        value = builder->CreateLoad(ret_type, value, "retval");
    }

    builder->CreateRet(value);
    return value;
}

llvm::Value* Codegen::gen_binary(BinaryExpr* node) {
    Type ttype = tchecker->query_type(node->left);

    // needs completion
    if (node->op.type == TokenType::AND_AND || node->op.type == TokenType::OR_OR) {
        bool is_and = node->op.type == TokenType::AND_AND;

        llvm::Function* fn = builder->GetInsertBlock()->getParent();

        // Evaluate the left side in the current block
        llvm::Value* vleft = gen_node(node->left);
        if (!vleft) return nullptr;
        llvm::BasicBlock* lhs_end = builder->GetInsertBlock();

        // Block where we evaluate the right side (only reached if needed)
        auto* rhs_bb   = llvm::BasicBlock::Create(*context,
                            is_and ? "and.rhs" : "or.rhs", fn);
        // Block where both paths meet
        auto* merge_bb = llvm::BasicBlock::Create(*context,
                            is_and ? "and.merge" : "or.merge", fn);

        if (is_and) {
            // AND: if left is false, result is false -- skip right
            builder->CreateCondBr(vleft, rhs_bb, merge_bb);
        } else {
            // OR: if left is true, result is true -- skip right
            builder->CreateCondBr(vleft, merge_bb, rhs_bb);
        }

        // Emit the right side
        builder->SetInsertPoint(rhs_bb);
        llvm::Value* vright = gen_node(node->right);
        builder->CreateBr(merge_bb);
        llvm::BasicBlock* rhs_end = builder->GetInsertBlock();

        // PHI node: picks the result based on which predecessor we came from
        builder->SetInsertPoint(merge_bb);
        auto* phi = builder->CreatePHI(llvm::Type::getInt1Ty(*context), 2,
                        is_and ? "and.result" : "or.result");

        if (is_and) {
            // came from lhs_end = left was false, so result = false
            phi->addIncoming(llvm::ConstantInt::getFalse(*context), lhs_end);
            // came from rhs_end = right side ran, result = right
            phi->addIncoming(vright, rhs_end);
        } else {
            // came from lhs_end = left was true, so result = true
            phi->addIncoming(llvm::ConstantInt::getTrue(*context), lhs_end);
            // came from rhs_end = right side ran, result = right
            phi->addIncoming(vright, rhs_end);
        }
        return phi;
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

llvm::Value* Codegen::gen_unary(UnaryExpr* node) {
    Type operand_type = tchecker->query_type(node->operand);

    if (node->op.type == TokenType::MINUS) {
        llvm::Value* val = gen_node(node->operand);
        if (!val) return nullptr;
        // CreateFNeg: floating-point negate, emits 'fneg' IR instruction
        if (is_float(operand_type.tkind))
            return builder->CreateFNeg(val, "fnegtmp");
        // CreateNeg: integer negate, emits 'sub i32 0, val'
        return builder->CreateNeg(val, "negtmp");
    }

    if (node->op.type == TokenType::BANG) {
        llvm::Value* val = gen_node(node->operand);
        if (!val) return nullptr;
        // CreateNot on i1: flips the bit -- logical NOT
        return builder->CreateNot(val, "nottmp");
    }

    // & (take address): return the raw alloca pointer, don't load
    if (node->op.type == TokenType::AMP) {
        return gen_lvalue(node->operand);
    }

    // * (dereference): load through the pointer
    if (node->op.type == TokenType::STAR) {
        llvm::Value* ptr = gen_node(node->operand);
        if (!ptr) return nullptr;
        Type result_type = tchecker->query_type(node);
        return builder->CreateLoad(llvm_type(result_type), ptr, "dereftmp");
    }

    return nullptr;
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

    if (id->resolved && static_cast<FunctionDecl*>(id->resolved)->is_builtin) return gen_builtin_call(node, id->token.get_value());

    FunctionDecl* fd = node->chosen;          // set by typechecker
    if (!fd) return nullptr;
    llvm::Function* fn = fn_decl_map[fd];
    if (!fn) return nullptr;

    std::vector<llvm::Value*> args;
    for (size_t i = 0; i < node->args.size(); ++i) {
        llvm::Value* v = gen_node(node->args[i]);
        if (!v) continue;

        Type arg_t = tchecker->query_type(node->args[i]);
        auto* p = static_cast<Param*>(fd->params[i]);
        Type par_t = tchecker->resolve_type(p->type_ann);

        v = coerce(v, arg_t, par_t); // no-op when equal
        args.push_back(v);
    }
    return fn->getReturnType()->isVoidTy()
        ? builder->CreateCall(fn, args)
        : builder->CreateCall(fn, args, "calltmp");

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

llvm::Value* Codegen::gen_for(ForStmt* node) {
    // Evaluate the array being iterated
    llvm::Value* arr_ptr = gen_lvalue(node->node); // alloca of the array
    if (!arr_ptr) return nullptr;

    Type arr_type = tchecker->query_type(node->node);
    if (arr_type.tkind != TypeKind::ARRAY || !arr_type.inner) return nullptr;

    size_t count = arr_type.array_size;
    llvm::Type* elem_ty = llvm_type(*arr_type.inner);
    llvm::Type* arr_ty  = llvm::ArrayType::get(elem_ty, count);
    llvm::Type* i64     = llvm::Type::getInt64Ty(*context);

    llvm::Function* fn = builder->GetInsertBlock()->getParent();

    // Allocate the loop counter
    auto* counter = builder->CreateAlloca(i64, nullptr, "for.i");
    builder->CreateStore(llvm::ConstantInt::get(i64, 0), counter);

    // Allocate the loop variable slot
    auto* pat = static_cast<Pattern*>(node->pattern);
    std::string var_name = std::string(pat->name.get_value());
    auto* var_slot = builder->CreateAlloca(elem_ty, nullptr, var_name);
    var_map[pat->name.get_value()] = var_slot;

    auto* cond_bb = llvm::BasicBlock::Create(*context, "for.cond", fn);
    auto* body_bb = llvm::BasicBlock::Create(*context, "for.body", fn);
    auto* exit_bb = llvm::BasicBlock::Create(*context, "for.exit", fn);

    llvm::BasicBlock* outer_exit     = loop_exit_bb;
    llvm::BasicBlock* outer_continue = loop_continue_bb;
    loop_exit_bb     = exit_bb;
    loop_continue_bb = cond_bb;

    builder->CreateBr(cond_bb);

    // Condition: i < count
    builder->SetInsertPoint(cond_bb);
    llvm::Value* i   = builder->CreateLoad(i64, counter, "i");
    llvm::Value* lim = llvm::ConstantInt::get(i64, count);
    llvm::Value* ok  = builder->CreateICmpULT(i, lim, "for.cond");
    builder->CreateCondBr(ok, body_bb, exit_bb);

    // Body: load arr[i] into the var slot, then run the block
    builder->SetInsertPoint(body_bb);
    llvm::Value* i2   = builder->CreateLoad(i64, counter, "i2");
    auto* zero        = llvm::ConstantInt::get(i64, 0);
    // GEP into [count x elem_ty] to get pointer to arr[i]
    llvm::Value* eptr = builder->CreateGEP(arr_ty, arr_ptr, {zero, i2}, "for.eptr");
    llvm::Value* elem = builder->CreateLoad(elem_ty, eptr, "for.elem");
    builder->CreateStore(elem, var_slot);

    auto saved = save_scope();
    gen_node(node->block);
    restore_scope(std::move(saved));

    // Increment counter
    if (!builder->GetInsertBlock()->getTerminator()) {
        llvm::Value* i3  = builder->CreateLoad(i64, counter, "i3");
        llvm::Value* inc = builder->CreateAdd(i3, llvm::ConstantInt::get(i64, 1), "inc");
        builder->CreateStore(inc, counter);
        builder->CreateBr(cond_bb);
    }

    loop_exit_bb     = outer_exit;
    loop_continue_bb = outer_continue;
    builder->SetInsertPoint(exit_bb);
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
    if (node->type == NodeType::IndexExpr) {
        auto* ix = static_cast<IndexExpr*>(node);
        llvm::Value* arr_ptr = gen_lvalue(ix->node); // pointer to the array alloca
        llvm::Value* idx     = gen_node(ix->index);
        if (!arr_ptr || !idx) return nullptr;

        Type arr_type = tchecker->query_type(ix->node);
        if (arr_type.tkind != TypeKind::ARRAY || !arr_type.inner) return nullptr;

        llvm::Type* elem_ty = llvm_type(*arr_type.inner);
        // ArrayType::get(elemTy, N): the LLVM type [N x elemTy]
        llvm::Type* arr_ty  = llvm::ArrayType::get(elem_ty, arr_type.array_size);

        // Two-index GEP: [0] steps past the array pointer, [idx] selects the element
        auto* zero = llvm::ConstantInt::get(llvm::Type::getInt64Ty(*context), 0);
        return builder->CreateGEP(arr_ty, arr_ptr, {zero, idx}, "elemptr");
    }
    return nullptr;
}

llvm::Value* Codegen::gen_enum_decl(EnumDecl* node) {
    const llvm::DataLayout& dl = _module->getDataLayout();
    uint64_t max_bytes = 0;

    // payload struct type per variant, for later construction/extraction
    for (size_t vi = 0; vi < node->variants.size(); ++vi) {
        auto* v = static_cast<EnumVariant*>(node->variants[vi]);
        std::vector<llvm::Type*> slot_tys;

        for (Node* p : v->payload)
            slot_tys.push_back(llvm_type(tchecker->resolve_type(p)));

        if (!slot_tys.empty()) {
            auto* pstruct = llvm::StructType::get(*context, slot_tys);
            // llvm::DataLayout::getTypeAllocSize(Type*): returns how many bytes a value of that type occupies in memory, including alignment padding.
            max_bytes = std::max(max_bytes, dl.getTypeAllocSize(pstruct).getFixedValue());
            variant_payload_type[node->name.get_value()][v->name.get_value()] = pstruct;
            variant_index_map[node->name.get_value()][v->name.get_value()] = static_cast<unsigned>(vi);
        }
    }

    auto* i32 = llvm::Type::getInt32Ty(*context);
    // llvm::ArrayType::get(elemTy, N): the LLVM type [N x elemTy]
    auto* buf = llvm::ArrayType::get(llvm::Type::getInt8Ty(*context), std::max<uint64_t>(max_bytes, 1));

    auto* etype = llvm::StructType::create(*context, { i32, buf }, std::string(node->name.get_value()));
    enum_type_map[node->name.get_value()] = etype;
    return nullptr;
}

llvm::Value* Codegen::gen_enum_ctor(CallExpr* node) {
    auto* en = node->ctor_enum;                 // set by resolver
    unsigned idx = node->ctor_index;
    std::string_view ename = en->name.get_value();
    auto* v = static_cast<EnumVariant*>(en->variants[idx]);

    llvm::StructType* etype = enum_type_map[ename];
    llvm::Value* slot = builder->CreateAlloca(etype, nullptr, "enumtmp");

    // field 0 = tag
    llvm::Value* tag_ptr = builder->CreateStructGEP(etype, slot, 0, "tag");
    builder->CreateStore(
        llvm::ConstantInt::get(llvm::Type::getInt32Ty(*context), idx), tag_ptr);

    // field 1 = payload buffer
    if (!v->payload.empty()) {
        llvm::Value* buf_ptr = builder->CreateStructGEP(etype, slot, 1, "buf");
        llvm::StructType* pstruct = variant_payload_type[ename][v->name.get_value()];
        // CreateBitCast(ptr, destPtrTy): reinterprets a pointer as pointing to a different type without changing the bits
        llvm::Value* typed = builder->CreateBitCast(buf_ptr, pstruct->getPointerTo(), "payload");

        for (size_t i = 0; i < node->args.size(); ++i) {
            llvm::Value* fp = builder->CreateStructGEP(pstruct, typed, i);
            llvm::Value* av = gen_node(node->args[i]);
            if (av) builder->CreateStore(av, fp);
        }
    }
    return slot; // pointer to the enum value
}

llvm::Value* Codegen::gen_match(MatchExpr* node) {
    llvm::Value* subj = gen_lvalue(node->subject);  // pointer to enum value
    Type subj_t = tchecker->query_type(node->subject);
    std::string_view ename = subj_t.struct_name;
    llvm::StructType* etype = enum_type_map[ename];

    llvm::Value* tag_ptr = builder->CreateStructGEP(etype, subj, 0, "tag");
    llvm::Value* tag = builder->CreateLoad(
        llvm::Type::getInt32Ty(*context), tag_ptr, "tagval");

    llvm::Function* fn = builder->GetInsertBlock()->getParent();
    auto* merge_bb = llvm::BasicBlock::Create(*context, "match.end", fn);
    auto* default_bb = llvm::BasicBlock::Create(*context, "match.default", fn);

    // builder->CreateSwitch(value, defaultBlock, numCases): a multi-way branch on an integer
    llvm::SwitchInst* sw = builder->CreateSwitch(tag, default_bb, node->arms.size());

    for (Node* an : node->arms) {
        auto* arm = static_cast<MatchArm*>(an);
        auto* pat = static_cast<Pattern*>(arm->pattern);
        // resolve which variant index this arm matches
        unsigned vi = variant_index(ename, pat->name.get_value());

        auto* arm_bb = llvm::BasicBlock::Create(*context, "match.arm", fn);
        sw->addCase(
            llvm::ConstantInt::get(llvm::Type::getInt32Ty(*context), vi), arm_bb);

        builder->SetInsertPoint(arm_bb);
        bind_variant_payload(etype, subj, ename, pat); // bitcast + bind fields
        gen_node(arm->body);
        if (!builder->GetInsertBlock()->getTerminator())
            builder->CreateBr(merge_bb);
    }

    builder->SetInsertPoint(default_bb);
    builder->CreateBr(merge_bb); // A.2.3 allows a default; A.3.4 makes this an error path
    builder->SetInsertPoint(merge_bb);
    return nullptr;
}

llvm::Value* Codegen::gen_match_stmt(MatchStmt* node) {
    Type st = tchecker->query_type(node->subject);
    llvm::Value* sptr = scrutinee_ptr(node->subject, st);

    llvm::Function* fn = builder->GetInsertBlock()->getParent();
    auto* merge_bb = llvm::BasicBlock::Create(*context, "match.end", fn);

    for (Node* an : node->arms) {
        auto* arm = static_cast<MatchArm*>(an);
        auto* next_bb = llvm::BasicBlock::Create(*context, "match.next", fn);

        auto saved = save_scope();
        test_pattern(sptr, st, static_cast<Pattern*>(arm->pattern), next_bb);

        if (arm->guard) {                              // guard runs only after structural match
            auto* body_bb = llvm::BasicBlock::Create(*context, "arm.body", fn);
            builder->CreateCondBr(gen_node(arm->guard), body_bb, next_bb);
            builder->SetInsertPoint(body_bb);
        }
        gen_node(arm->body);
        if (!builder->GetInsertBlock()->getTerminator())
            builder->CreateBr(merge_bb);

        restore_scope(std::move(saved));
        builder->SetInsertPoint(next_bb);
    }

    builder->CreateUnreachable();                       // proven exhaustive by A.3.4
    builder->SetInsertPoint(merge_bb);
    return nullptr;
}

llvm::Value* Codegen::gen_match_expr(MatchExpr* node) {
    Type st = tchecker->query_type(node->subject);
    llvm::Value* sptr = scrutinee_ptr(node->subject, st);

    llvm::Function* fn = builder->GetInsertBlock()->getParent();
    auto* merge_bb = llvm::BasicBlock::Create(*context, "match.end", fn);
    std::vector<std::pair<llvm::Value*, llvm::BasicBlock*>> incoming;

    for (Node* an : node->arms) {
        auto* arm = static_cast<MatchArm*>(an);
        auto* next_bb = llvm::BasicBlock::Create(*context, "match.next", fn);

        auto saved = save_scope();
        test_pattern(sptr, st, static_cast<Pattern*>(arm->pattern), next_bb);

        if (arm->guard) {
            auto* body_bb = llvm::BasicBlock::Create(*context, "arm.body", fn);
            builder->CreateCondBr(gen_node(arm->guard), body_bb, next_bb);
            builder->SetInsertPoint(body_bb);
        }
        llvm::Value* bv = gen_node(arm->body);
        llvm::BasicBlock* end = builder->GetInsertBlock();
        if (!end->getTerminator()) {
            incoming.push_back({ bv, end });
            builder->CreateBr(merge_bb);
        }
        restore_scope(std::move(saved));
        builder->SetInsertPoint(next_bb);
    }
    builder->CreateUnreachable();

    builder->SetInsertPoint(merge_bb);
    if (incoming.empty()) return nullptr;
    auto* phi = builder->CreatePHI(incoming[0].first->getType(), incoming.size(), "match.result");
    for (auto& [v, bb] : incoming) phi->addIncoming(v, bb);
    return phi;
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
        Type ft = fd->type_ann
            ? tchecker->resolve_type(fd->type_ann)
            : Type::make(TypeKind::UNKNOWN);

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

llvm::Value* Codegen::gen_index(IndexExpr* node) {
    llvm::Value* elem_ptr = gen_lvalue(node); // reuse lvalue path above
    if (!elem_ptr) return nullptr;
    Type elem_type = tchecker->query_type(node);
    // CreateLoad: loads the value at the address elem_ptr
    return builder->CreateLoad(llvm_type(elem_type), elem_ptr, "idxtmp");
}

llvm::Value* Codegen::gen_if_expr(IfExpr* node) {
    llvm::Value* vcond = gen_node(node->condition);
    if (!vcond) return nullptr;

    llvm::Function* fn = builder->GetInsertBlock()->getParent();
    auto* then_bb  = llvm::BasicBlock::Create(*context, "ifexpr.then",  fn);
    auto* else_bb  = llvm::BasicBlock::Create(*context, "ifexpr.else",  fn);
    auto* merge_bb = llvm::BasicBlock::Create(*context, "ifexpr.merge", fn);

    builder->CreateCondBr(vcond, then_bb, else_bb);

    builder->SetInsertPoint(then_bb);
    llvm::Value* vthen = gen_node(node->then_block);
    if (!builder->GetInsertBlock()->getTerminator())
        builder->CreateBr(merge_bb);
    llvm::BasicBlock* then_end = builder->GetInsertBlock();

    builder->SetInsertPoint(else_bb);
    llvm::Value* velse = node->else_expr ? gen_node(node->else_expr) : nullptr;
    if (!builder->GetInsertBlock()->getTerminator())
        builder->CreateBr(merge_bb);
    llvm::BasicBlock* else_end = builder->GetInsertBlock();

    builder->SetInsertPoint(merge_bb);
    if (!vthen || !velse) return nullptr; // void branches

    // PHI: selects vthen if we came from then_end, velse if from else_end
    auto* phi = builder->CreatePHI(vthen->getType(), 2, "ifexpr.result");
    phi->addIncoming(vthen, then_end);
    phi->addIncoming(velse, else_end);
    return phi;
}

llvm::Function* Codegen::get_or_declare_printf() {
    if (auto* f = _module->getFunction("printf")) return f;

    auto* i8ptr = llvm::PointerType::get(llvm::Type::getInt8Ty(*context), 0);
    auto* ft = llvm::FunctionType::get(
        llvm::Type::getInt32Ty(*context), { i8ptr }, true);
    return llvm::Function::Create(
        ft, llvm::Function::ExternalLinkage, "printf", *_module);
}

llvm::Function* Codegen::get_or_declare_scanf() {
    if (auto* f = _module->getFunction("scanf")) return f;

    auto* i8ptr = llvm::PointerType::get(llvm::Type::getInt8Ty(*context), 0);
    auto* ft = llvm::FunctionType::get(
        llvm::Type::getInt32Ty(*context), { i8ptr }, true);
    return llvm::Function::Create(
        ft, llvm::Function::ExternalLinkage, "scanf", *_module);
}

llvm::Value* Codegen::gen_builtin_call(CallExpr* node, std::string_view name) {

    if (name == "print") {
        llvm::Function* printf_fn = get_or_declare_printf();
        llvm::Value* arg = gen_node(node->args[0]);
        if (!arg) return nullptr;

        // Pick format string
        Type arg_type = tchecker->query_type(node->args[0]);
        std::string fmt;
        switch (arg_type.tkind) {
            case TypeKind::INT8:
            case TypeKind::INT16:
            case TypeKind::INT32:  fmt = "%d\n";   break;
            case TypeKind::INT64:  fmt = "%lld\n"; break;
            case TypeKind::UINT8:
            case TypeKind::UINT16:
            case TypeKind::UINT32: fmt = "%u\n";   break;
            case TypeKind::UINT64: fmt = "%llu\n"; break;
            case TypeKind::FLO32:
            case TypeKind::FLO64: fmt = "%f\n";   break;
            case TypeKind::BOOL: {
                auto* true_str  = builder->CreateGlobalStringPtr("true\n",  "bool_true");
                auto* false_str = builder->CreateGlobalStringPtr("false\n", "bool_false");

                auto* str = builder->CreateSelect(arg, true_str, false_str, "boolstr");
                auto* fmt_str = builder->CreateGlobalStringPtr("%s", "fmt_bool");
                return builder->CreateCall(printf_fn, { fmt_str, str }, "printtmp");
            }
            case TypeKind::STRING:
            case TypeKind::STRING_VIEW:
            case TypeKind::BUF_STRING: fmt = "%s\n"; break;
            default: fmt = "%d\n"; break;
        }


        if (arg_type.tkind == TypeKind::FLO32)
            arg = builder->CreateFPExt(arg, llvm::Type::getDoubleTy(*context), "fpext");

        auto* fmt_val = builder->CreateGlobalStringPtr(fmt, "printfmt");
        return builder->CreateCall(printf_fn, { fmt_val, arg }, "printtmp");
    }

    if (name == "exit") {
        auto* exit_fn = _module->getFunction("exit");
        if (!exit_fn) {
            auto* ft = llvm::FunctionType::get(
                llvm::Type::getVoidTy(*context),
                { llvm::Type::getInt32Ty(*context) },
                false);
            exit_fn = llvm::Function::Create(
                ft, llvm::Function::ExternalLinkage, "exit", *_module);
        }
        llvm::Value* code = gen_node(node->args[0]);
        if (!code) code = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*context), 0);
        return builder->CreateCall(exit_fn, { code });
    }

    if (name == "panic") {
        auto* exit_fn = _module->getFunction("exit");
        if (!exit_fn) {
            auto* ft = llvm::FunctionType::get(
                llvm::Type::getVoidTy(*context),
                { llvm::Type::getInt32Ty(*context) }, false);
            exit_fn = llvm::Function::Create(
                ft, llvm::Function::ExternalLinkage, "exit", *_module);
        }
        
        llvm::Function* printf_fn = get_or_declare_printf();
        llvm::Value* msg = gen_node(node->args[0]);
        auto* fmt = builder->CreateGlobalStringPtr("panic: %s\n", "panicfmt");
        builder->CreateCall(printf_fn, { fmt, msg });
        auto* one = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*context), 1);
        builder->CreateCall(exit_fn, { one });
        
        builder->CreateUnreachable();
        return nullptr;
    }

    if (name == "assert") {
        
        llvm::Value* cond = gen_node(node->args[0]);
        if (!cond) return nullptr;

        llvm::Function* fn = builder->GetInsertBlock()->getParent();
        auto* fail_bb = llvm::BasicBlock::Create(*context, "assert.fail", fn);
        auto* pass_bb = llvm::BasicBlock::Create(*context, "assert.pass", fn);

        builder->CreateCondBr(cond, pass_bb, fail_bb);

        builder->SetInsertPoint(fail_bb);
        llvm::Function* printf_fn = get_or_declare_printf();
        auto* fmt = builder->CreateGlobalStringPtr("assertion failed\n", "assertfmt");
        builder->CreateCall(printf_fn, { fmt });
        auto* exit_fn = _module->getFunction("exit");
        if (!exit_fn) {
            auto* ft = llvm::FunctionType::get(
                llvm::Type::getVoidTy(*context),
                { llvm::Type::getInt32Ty(*context) }, false);
            exit_fn = llvm::Function::Create(
                ft, llvm::Function::ExternalLinkage, "exit", *_module);
        }
        builder->CreateCall(exit_fn,
            { llvm::ConstantInt::get(llvm::Type::getInt32Ty(*context), 1) });
        builder->CreateUnreachable();

        builder->SetInsertPoint(pass_bb);
        return nullptr;
    }

    if (name == "input") {
        auto* scanf_fn = get_or_declare_scanf();
        auto* buf_type = llvm::ArrayType::get(llvm::Type::getInt8Ty(*context), 256);
        auto* buf = new llvm::GlobalVariable(
            *_module, buf_type, false,
            llvm::GlobalValue::PrivateLinkage,
            llvm::ConstantAggregateZero::get(buf_type), "input_buf");
        auto* fmt = builder->CreateGlobalStringPtr("%255s", "inputfmt");
        auto* buf_ptr = builder->CreateBitCast(
            buf, llvm::PointerType::get(llvm::Type::getInt8Ty(*context), 0));
        builder->CreateCall(scanf_fn, { fmt, buf_ptr });
        return buf_ptr;
    }

    return nullptr;
}

void Codegen::test_pattern(llvm::Value* val_ptr, const Type& vt, Pattern* pat, llvm::BasicBlock* fail_bb) {
    llvm::Function* fn = builder->GetInsertBlock()->getParent();
    auto* i1  = llvm::Type::getInt1Ty(*context);
    auto* i32 = llvm::Type::getInt32Ty(*context);

    switch (pat->pat_type) {
        case PatternType::Wildcard:
            return;

        case PatternType::Identifier: {              // bind by copy into a named slot
            llvm::Type* lt = llvm_type(vt);
            auto* slot = builder->CreateAlloca(lt, nullptr, std::string(pat->name.get_value()));
            llvm::Value* loaded = builder->CreateLoad(lt, val_ptr, "bindval");
            builder->CreateStore(loaded, slot);
            var_map[pat->name.get_value()] = slot;
            return;
        }

        case PatternType::Literal: {
            llvm::Type* lt = llvm_type(vt);
            llvm::Value* loaded = builder->CreateLoad(lt, val_ptr, "litsubj");
            llvm::Value* lit = literal_constant(pat->lit, vt);
            llvm::Value* eq = is_float(vt.tkind)
                ? builder->CreateFCmpOEQ(loaded, lit, "liteq")
                : builder->CreateICmpEQ(loaded, lit, "liteq");
            auto* cont = llvm::BasicBlock::Create(*context, "lit.ok", fn);
            builder->CreateCondBr(eq, cont, fail_bb);
            builder->SetInsertPoint(cont);
            return;
        }

        case PatternType::Variant: {
            llvm::StructType* etype = enum_type_map[vt.struct_name];
            llvm::Value* tag = builder->CreateLoad(i32,
                builder->CreateStructGEP(etype, val_ptr, 0, "tag.ptr"), "tag");
            unsigned vi = variant_index(vt.struct_name, pat->name.get_value());
            llvm::Value* eq = builder->CreateICmpEQ(tag, llvm::ConstantInt::get(i32, vi), "tag.eq");
            auto* cont = llvm::BasicBlock::Create(*context, "var.ok", fn);
            builder->CreateCondBr(eq, cont, fail_bb);
            builder->SetInsertPoint(cont);

            if (!pat->fields.empty()) {
                EnumDecl* en = enum_decl_map[vt.struct_name];
                auto* v = find_variant_decl(en, pat->name.get_value());
                llvm::StructType* pstruct =
                    variant_payload_type[vt.struct_name][pat->name.get_value()];
                llvm::Value* buf = builder->CreateStructGEP(etype, val_ptr, 1, "buf.ptr");
                llvm::Value* typed = builder->CreateBitCast(buf, pstruct->getPointerTo(), "payload.ptr");
                for (size_t i = 0; i < pat->fields.size(); ++i) {
                    llvm::Value* fptr = builder->CreateStructGEP(pstruct, typed, i, "fld");
                    Type ft = tchecker->resolve_type(v->payload[i]);
                    test_pattern(fptr, ft, static_cast<Pattern*>(pat->fields[i]), fail_bb); // NESTED
                }
            }
            return;
        }

        case PatternType::None: {
            auto* ot = static_cast<llvm::StructType*>(llvm_type(vt));
            llvm::Value* present = builder->CreateLoad(i1,
                builder->CreateStructGEP(ot, val_ptr, 0, "present.ptr"), "present");
            llvm::Value* is_none = builder->CreateNot(present, "is.none");
            auto* cont = llvm::BasicBlock::Create(*context, "none.ok", fn);
            builder->CreateCondBr(is_none, cont, fail_bb);
            builder->SetInsertPoint(cont);
            return;
        }
        case PatternType::Some: {
            auto* ot = static_cast<llvm::StructType*>(llvm_type(vt));
            llvm::Value* present = builder->CreateLoad(i1,
                builder->CreateStructGEP(ot, val_ptr, 0, "present.ptr"), "present");
            auto* cont = llvm::BasicBlock::Create(*context, "some.ok", fn);
            builder->CreateCondBr(present, cont, fail_bb);
            builder->SetInsertPoint(cont);
            if (pat->inner) {
                llvm::Value* vptr = builder->CreateStructGEP(ot, val_ptr, 1, "some.val");
                test_pattern(vptr, *vt.inner, static_cast<Pattern*>(pat->inner), fail_bb);
            }
            return;
        }
        case PatternType::Ok:
        case PatternType::Err: {
            auto* rt = static_cast<llvm::StructType*>(llvm_type(vt));
            llvm::Value* is_err = builder->CreateLoad(i1,
                builder->CreateStructGEP(rt, val_ptr, 0, "iserr.ptr"), "iserr");
            bool want_err = (pat->pat_type == PatternType::Err);
            llvm::Value* cond = want_err ? is_err : builder->CreateNot(is_err, "is.ok");
            auto* cont = llvm::BasicBlock::Create(*context, want_err ? "err.ok" : "ok.ok", fn);
            builder->CreateCondBr(cond, cont, fail_bb);
            builder->SetInsertPoint(cont);
            if (pat->inner) {
                unsigned slot = want_err ? 2 : 1;
                Type inner = want_err ? *vt.inner2 : *vt.inner;
                llvm::Value* vptr = builder->CreateStructGEP(rt, val_ptr, slot, "payload");
                test_pattern(vptr, inner, static_cast<Pattern*>(pat->inner), fail_bb);
            }
            return;
        }
    }
}

llvm::Value* Codegen::scrutinee_ptr(Node* subject, const Type& st) {
    if (subject->type == NodeType::Identifier ||
        subject->type == NodeType::FieldExpr ||
        subject->type == NodeType::IndexExpr) {
        if (llvm::Value* p = gen_lvalue(subject)) return p;
    }
    llvm::Value* v = gen_node(subject);
    if (v && v->getType()->isPointerTy()) return v;     // e.g. enum ctor already returns a ptr
    llvm::Value* slot = builder->CreateAlloca(llvm_type(st), nullptr, "scrut");
    if (v) builder->CreateStore(v, slot);
    return slot;
}