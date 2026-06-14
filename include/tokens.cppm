module;

#include <string>
#include <string_view>
#include <variant>

export module tokens;
using Object = std::variant<int, std::string_view, double>;

export enum TokenType {
    COMMA, //,
    DOT, //.
    SEMI, //;
    COLON, //:
    HASH, //#
    LBRKT, //{
    RBRKT, //}
    LPAREN, //(
    RPAREN, //)
    COLON_EQUAL, //:=
    ARROW, //->
    FAT_ARROW, //=>
    AMP, //&
    PIPE, //|
    LSQUARE, //[
    RSQUARE, //]
    AND_AND, //&&
    OR_OR, //||
    PERCENT, //%
    UNDERSCORE, //_

    PLUS, MINUS, STAR, SLASH,

    EQUAL, GREATER, LESS, GREATER_EQUAL, LESS_EQUAL, NOT_EQUAL, BANG, EQUAL_EQUAL, BANG_EQUAL,

    STR, VAL, IDENT,

    //Keywords
    IF, ELSE, WHILE, FOR, RETURN, TRUE, FALSE, NONE,
    FN, STRUCT, LET, CONST, LOOP, MATCH, BREAK, CONTINUE, WITH,
    IN, MUT, SOME, OK, ERR, OPTIONAL, RESULT, SHARED, WEAK,
    SIGN, UNSIGN, TRUNC_CAST, CHECK_CAST, 
    WRAP_ADD, WRAP_SUB, WRAP_MUL, TYPE, ENUM,

    //Primitive types
    BOOL, UNIT, INT8, INT16, INT32, INT64,
    UINT8, UINT16, UINT32, UINT64,
    FLO32, FLO64, CHAR, STRING, STRING_VIEW, BUF_STRING,

    END

};

export class Token {
    private:
    Object literal;
    int line;
    std::string_view value;

    public:
    TokenType type;
    Token() : literal{0}, line{0}, value{""}, type{END} {}
    Token(std::string_view value, Object literal, TokenType type, int line) : literal{literal},  line{line}, value{value}, type{type} {

    }
    char* token_type_name(TokenType);
    std::string_view get_value() const { return value; }
    Object get_literal()         const { return literal; }
    int get_line() const { return line; }

    std::string to_string() {
        return std::to_string(type) + " " + std::string(value);
    }
};

export const char* token_type_name(TokenType t) {
    switch (t) {
        case COMMA:         return "COMMA";
        case DOT:           return "DOT";
        case SEMI:          return "SEMI";
        case COLON:         return "COLON";
        case HASH:          return "HASH";
        case LBRKT:         return "LBRKT";
        case RBRKT:         return "RBRKT";
        case LPAREN:        return "LPAREN";
        case RPAREN:        return "RPAREN";
        case COLON_EQUAL:   return "COLON_EQUAL";
        case ARROW:         return "ARROW";
        case FAT_ARROW:     return "FAT_ARROW";
        case AMP:           return "AMP";
        case PIPE:          return "PIPE";
        case LSQUARE:       return "LSQUARE";
        case RSQUARE:       return "RSQUARE";
        case AND_AND:       return "AND_AND";
        case OR_OR:         return "OR_OR";
        case PERCENT:       return "PERCENT";
        case UNDERSCORE:    return "UNDERSCORE";
        case PLUS:          return "PLUS";
        case MINUS:         return "MINUS";
        case STAR:          return "STAR";
        case SLASH:         return "SLASH";
        case EQUAL:         return "EQUAL";
        case GREATER:       return "GREATER";
        case LESS:          return "LESS";
        case GREATER_EQUAL: return "GREATER_EQUAL";
        case LESS_EQUAL:    return "LESS_EQUAL";
        case NOT_EQUAL:     return "NOT_EQUAL";
        case BANG:          return "BANG";
        case EQUAL_EQUAL:   return "EQUAL_EQUAL";
        case BANG_EQUAL:    return "BANG_EQUAL";
        case STR:           return "STR";
        case VAL:           return "VAL";
        case IDENT:         return "IDENT";
        case IF:            return "IF";
        case ELSE:          return "ELSE";
        case WHILE:         return "WHILE";
        case FOR:           return "FOR";
        case RETURN:        return "RETURN";
        case TRUE:          return "TRUE";
        case FALSE:         return "FALSE";
        case NONE:          return "NONE";
        case FN:            return "FN";
        case STRUCT:        return "STRUCT";
        case LET:           return "LET";
        case CONST:         return "CONST";
        case LOOP:          return "LOOP";
        case MATCH:         return "MATCH";
        case BREAK:         return "BREAK";
        case CONTINUE:      return "CONTINUE";
        case WITH:          return "WITH";
        case IN:            return "IN";
        case MUT:           return "MUT";
        case SOME:          return "SOME";
        case OK:            return "OK";
        case ERR:           return "ERR";
        case OPTIONAL:      return "OPTIONAL";
        case RESULT:        return "RESULT";
        case SHARED:        return "SHARED";
        case WEAK:          return "WEAK";
        case SIGN:          return "SIGN";
        case UNSIGN:        return "UNSIGN";
        case TRUNC_CAST:    return "TRUNC_CAST";
        case CHECK_CAST:    return "CHECK_CAST";
        case WRAP_ADD:      return "WRAP_ADD";
        case WRAP_SUB:      return "WRAP_SUB";
        case WRAP_MUL:      return "WRAP_MUL";
        case TYPE:          return "TYPE";
        case BOOL:          return "BOOL";
        case UNIT:          return "UNIT";
        case INT8:          return "INT8";
        case INT16:         return "INT16";
        case INT32:         return "INT32";
        case INT64:         return "INT64";
        case UINT8:         return "UINT8";
        case UINT16:        return "UINT16";
        case UINT32:        return "UINT32";
        case UINT64:        return "UINT64";
        case FLO32:         return "FLO32";
        case FLO64:         return "FLO64";
        case CHAR:          return "CHAR";
        case STRING:        return "STRING";
        case STRING_VIEW:   return "STRING_VIEW";
        case BUF_STRING:    return "BUF_STRING";
        case ENUM:          return "ENUM";
        case END:           return "END";
        default:            return "UNKNOWN";
    }
}