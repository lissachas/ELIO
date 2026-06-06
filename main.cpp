
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

import tokens;
import expr;
import error;
import lexer;
import parser;
import symbol;
import resolve;
import typecheck;
import codegen;

static std::string read_file(const std::string& path) {
    std::ifstream f(path);
    if (!f) {
        std::cerr << "elio: cannot open file '" << path << "'\n";
        std::exit(1);
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static void dump_tokens(const std::vector<Token>& tokens) {
    for (Token tok : tokens) {
        std::printf("%4d  %-16s  %s\n",
                    tok.get_line(),
                    token_type_name(tok.type),
                    std::string(tok.get_value()).c_str());
    }
}

static void dump_ast(Node* node, int depth = 0) {
    if (!node) return;
    std::string pad(depth * 2, ' ');

    switch (node->type) {
        case NodeType::FunctionDecl: {
            auto* fn = static_cast<FunctionDecl*>(node);
            std::cout << pad << "FunctionDecl(" << fn->name.get_value() << ")\n";
            for (Node* p : fn->params) dump_ast(p, depth + 1);
            dump_ast(fn->body, depth + 1);
            break;
        }
        case NodeType::Param: {
            auto* p = static_cast<Param*>(node);
            std::cout << pad << "Param(" << p->name.get_value() << ")\n";
            break;
        }
        case NodeType::StructDecl: {
            auto* s = static_cast<StructDecl*>(node);
            std::cout << pad << "StructDecl(" << s->tag.get_value() << ")\n";
            for (Node* f : s->opt) dump_ast(f, depth + 1);
            break;
        }
        case NodeType::FieldDecl: {
            auto* f = static_cast<FieldDecl*>(node);
            std::cout << pad << "FieldDecl(" << f->name.get_value() << ")\n";
            break;
        }
        case NodeType::TypeAliasDecl: {
            auto* ta = static_cast<TypeAliasDecl*>(node);
            std::cout << pad << "TypeAlias(" << ta->name.get_value() << ")\n";
            break;
        }
        case NodeType::LetDecl: {
            auto* ld = static_cast<LetDecl*>(node);
            std::cout << pad << "LetDecl\n";
            dump_ast(ld->pattern, depth + 1);
            dump_ast(ld->init,    depth + 1);
            break;
        }
        case NodeType::ConstDecl: {
            auto* cd = static_cast<ConstDecl*>(node);
            std::cout << pad << "ConstDecl(" << cd->ident.get_value() << ")\n";
            dump_ast(cd->init, depth + 1);
            break;
        }
        case NodeType::BlockExpr: {
            auto* bl = static_cast<BlockExpr*>(node);
            std::cout << pad << "Block\n";
            for (Node* s : bl->opt) dump_ast(s, depth + 1);
            break;
        }
        case NodeType::IfStmt: {
            auto* i = static_cast<IfStmt*>(node);
            std::cout << pad << "IfStmt\n";
            dump_ast(i->node,  depth + 1);
            dump_ast(i->block, depth + 1);
            if (i->other) dump_ast(i->other, depth + 1);
            break;
        }
        case NodeType::WhileStmt: {
            auto* w = static_cast<WhileStmt*>(node);
            std::cout << pad << "WhileStmt"
                      << (w->has_tag ? std::string(" tag=") + std::string(w->tag.get_value()) : "") << "\n";
            dump_ast(w->condition, depth + 1);
            dump_ast(w->block,     depth + 1);
            break;
        }
        case NodeType::ForStmt: {
            auto* f = static_cast<ForStmt*>(node);
            std::cout << pad << "ForStmt\n";
            dump_ast(f->pattern, depth + 1);
            dump_ast(f->node,    depth + 1);
            dump_ast(f->block,   depth + 1);
            break;
        }
        case NodeType::LoopStmt: {
            auto* l = static_cast<LoopStmt*>(node);
            std::cout << pad << "LoopStmt\n";
            dump_ast(l->block, depth + 1);
            break;
        }
        case NodeType::ReturnStmt: {
            auto* r = static_cast<ReturnStmt*>(node);
            std::cout << pad << "ReturnStmt\n";
            if (r->has_value) dump_ast(r->value, depth + 1);
            break;
        }
        case NodeType::BreakStmt: {
            auto* b = static_cast<BreakStmt*>(node);
            const char* kind = "Plain";
            switch (b->break_type) {
                case BreakType::WithValue:    kind = "WithValue";    break;
                case BreakType::WithTag:      kind = "WithTag";      break;
                case BreakType::WithTagValue: kind = "WithTagValue"; break;
                default: break;
            }
            std::cout << pad << "BreakStmt(" << kind << ")\n";
            if (b->value) dump_ast(b->value, depth + 1);
            break;
        }
        case NodeType::ContinueStmt: {
            auto* c = static_cast<ContinueStmt*>(node);
            std::cout << pad << "ContinueStmt"
                      << (c->has_tag ? std::string(" tag=") + std::string(c->tag.get_value()) : "") << "\n";
            break;
        }
        case NodeType::ExprStmt: {
            std::cout << pad << "ExprStmt\n";
            dump_ast(static_cast<ExprStmt*>(node)->node, depth + 1);
            break;
        }
        case NodeType::MatchStmt: {
            auto* m = static_cast<MatchStmt*>(node);
            std::cout << pad << "MatchStmt\n";
            dump_ast(m->subject, depth + 1);
            for (Node* arm : m->arms) dump_ast(arm, depth + 1);
            break;
        }
        case NodeType::MatchArm: {
            auto* arm = static_cast<MatchArm*>(node);
            std::cout << pad << "MatchArm\n";
            dump_ast(arm->pattern, depth + 1);
            dump_ast(arm->body,    depth + 1);
            break;
        }
        case NodeType::BinaryExpr: {
            auto* b = static_cast<BinaryExpr*>(node);
            std::cout << pad << "Binary(" << b->op.get_value() << ")\n";
            dump_ast(b->left,  depth + 1);
            dump_ast(b->right, depth + 1);
            break;
        }
        case NodeType::UnaryExpr: {
            auto* u = static_cast<UnaryExpr*>(node);
            std::cout << pad << "Unary(" << u->op.get_value() << ")\n";
            dump_ast(u->operand, depth + 1);
            break;
        }
        case NodeType::CallExpr: {
            auto* c = static_cast<CallExpr*>(node);
            std::cout << pad << "CallExpr\n";
            dump_ast(c->callee, depth + 1);
            for (Node* a : c->args) dump_ast(a, depth + 1);
            break;
        }
        case NodeType::IndexExpr: {
            auto* i = static_cast<IndexExpr*>(node);
            std::cout << pad << "IndexExpr\n";
            dump_ast(i->node,  depth + 1);
            dump_ast(i->index, depth + 1);
            break;
        }
        case NodeType::FieldExpr: {
            auto* f = static_cast<FieldExpr*>(node);
            std::cout << pad << "FieldExpr(." << f->field.get_value() << ")\n";
            dump_ast(f->object, depth + 1);
            break;
        }
        case NodeType::AssignExpr: {
            auto* a = static_cast<AssignExpr*>(node);
            std::cout << pad << "AssignExpr\n";
            dump_ast(a->target, depth + 1);
            dump_ast(a->value,  depth + 1);
            break;
        }
        case NodeType::IfExpr: {
            auto* i = static_cast<IfExpr*>(node);
            std::cout << pad << "IfExpr\n";
            dump_ast(i->condition,  depth + 1);
            dump_ast(i->then_block, depth + 1);
            if (i->else_expr) dump_ast(i->else_expr, depth + 1);
            break;
        }
        case NodeType::MatchExpr: {
            auto* m = static_cast<MatchExpr*>(node);
            std::cout << pad << "MatchExpr\n";
            dump_ast(m->subject, depth + 1);
            for (Node* arm : m->arms) dump_ast(arm, depth + 1);
            break;
        }
        case NodeType::LambdaExpr: {
            std::cout << pad << "LambdaExpr\n";
            auto* l = static_cast<LambdaExpr*>(node);
            for (Node* p : l->param) dump_ast(p, depth + 1);
            dump_ast(l->node, depth + 1);
            break;
        }
        case NodeType::StructInit: {
            auto* s = static_cast<StructInit*>(node);
            std::cout << pad << "StructInit(" << s->name.get_value() << ")\n";
            for (Node* f : s->opt) dump_ast(f, depth + 1);
            break;
        }
        case NodeType::FieldInit: {
            auto* f = static_cast<FieldInit*>(node);
            std::cout << pad << "FieldInit(" << f->name.get_value() << ")\n";
            if (!f->shorthand) dump_ast(f->value, depth + 1);
            break;
        }
        case NodeType::Identifier: {
            auto* id = static_cast<Identifier*>(node);
            std::cout << pad << "Identifier(" << id->token.get_value() << ")\n";
            break;
        }
        case NodeType::Literal: {
            auto* lit = static_cast<Literal*>(node);
            std::cout << pad << "Literal(" << lit->token.get_value() << ")\n";
            break;
        }
        case NodeType::Pattern: {
            auto* p = static_cast<Pattern*>(node);
            std::cout << pad << "Pattern(" << (int)p->pat_type << ")\n";
            if (p->inner) dump_ast(p->inner, depth + 1);
            for (Node* f : p->fields) dump_ast(f, depth + 1);
            break;
        }
        case NodeType::BuiltInCast: {
            auto* b = static_cast<BuiltinCast*>(node);
            std::cout << pad << "BuiltinCast(" << b->builtin.get_value() << ")\n";
            dump_ast(b->first,  depth + 1);
            if (b->second) dump_ast(b->second, depth + 1);
            break;
        }
        default:
            std::cout << pad << "Node(kind=" << (int)node->type << ")\n";
            break;
    }
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: elio <source.el> [-o <output.ll>] [--dump-tokens] [--dump-ast]\n";
        return 1;
    }

    std::string source_file;
    std::string output_file;
    bool flag_dump_tokens = false;
    bool flag_dump_ast    = false;
    bool flag_optimize = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if      (arg == "--dump-tokens") flag_dump_tokens = true;
        else if (arg == "--dump-ast")    flag_dump_ast    = true;
        else if (arg == "-o" && i + 1 < argc) output_file = argv[++i];
        else if (arg == "--opt") flag_optimize = true;
        else    source_file = arg;
    }

    if (source_file.empty()) {
        std::fprintf(stderr, "elio: no source file\n");
        return 1;
    }

    // Derive output name from source if -o not given
    if (output_file.empty()) {
        output_file = source_file;
        auto dot = output_file.rfind('.');
        if (dot != std::string::npos) output_file = output_file.substr(0, dot);
        output_file += ".ll";
    }

    std::string source = read_file(source_file);

    Diagnostics diag;

    std::cout << "Started making tokens \n";

    // ---- LEX ----
    Lexer lexer(&diag);
    lexer.start(std::move(source));
    if (diag.had_error_in(ErrorStage::Lexer)) {
        diag.print_all(source_file);
        return 1;
    }
    std::vector<Token> tokens = lexer.get_tokens();

    if (flag_dump_tokens) {
        dump_tokens(tokens);
        return 0;
    }

    std::cout << "Started making ast \n";

    // ---- PARSE ----
    Parser parser(std::move(tokens), &diag);
    Node* program = parser.parse();
    if (diag.had_error_in(ErrorStage::Parser)) {
        diag.print_all(source_file);
        return 1;
    }

    if (!program) {
        std::cerr << "elio: parser returned null\n";
        return 1;
    }

    if (flag_dump_ast) {
        dump_ast(program);
        return 0;
    }

    std::cout << "Finished making ast \n";
    std::cout << "Started resolving \n";

    // ---- RESOLVE ----
    Resolver resolver(&diag);
    resolver.resolve(program);
    if (diag.had_error_in(ErrorStage::Resolver)) {
        diag.print_all(source_file);
        return 1;
    }

    std::cout << "Finished resloving \n";
    std::cout << "Started checking \n";

    // Stop if resolve had errors (check Error state)
    // NOTE: requires resolver to expose its Error, or Error to be shared

    // ---- TYPECHECK ----
    TypeChecker tc(&diag);
    tc.run(program);
    if (diag.had_error_in(ErrorStage::TypeChecker)) {
        diag.print_all(source_file);
        return 1;
    }

    std::cout << "Finished checking \n";
    std::cout << "Started generating code \n";

    // ---- CODEGEN ----
    Codegen cg(&diag);
    cg.set_type_checker(&tc);
    cg.generate(program);
    cg.emit_ir(output_file, flag_optimize);
    if (diag.had_error_in(ErrorStage::Codegen)) {
        diag.print_all(source_file);
        return 1;
    }

    std::fprintf(stderr, "elio: wrote %s\n", output_file.c_str());
    return 0;
}