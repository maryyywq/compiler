#include "defs.h"
#include "Scanner.h"
#include <stdio.h>

static const char * TypeName(int t) {
    switch (t) {
        case KW_INT:     return "KW_INT";
        case KW_FLOAT:   return "KW_FLOAT";
        case KW_BOOL:    return "KW_BOOL";
        case KW_VOID:    return "KW_VOID";
        case KW_CLASS:   return "KW_CLASS";
        case KW_PUBLIC:  return "KW_PUBLIC";
        case KW_PRIVATE: return "KW_PRIVATE";
        case KW_IF:      return "KW_IF";
        case KW_ELSE:    return "KW_ELSE";
        case KW_MAIN:    return "KW_MAIN";
        case IDENT:      return "IDENT";
        case DEC_CONST:  return "DEC_CONST";
        case REAL_CONST: return "REAL_CONST";
        case BOOL_CONST: return "BOOL_CONST";
        case SEMI:       return "SEMI";
        case COMMA:      return "COMMA";
        case LPAREN:     return "LPAREN";
        case RPAREN:     return "RPAREN";
        case LBRACE:     return "LBRACE";
        case RBRACE:     return "RBRACE";
        case COLON:      return "COLON";
        case DOT:        return "DOT";
        case TILDE:      return "TILDE";
        case EQ:         return "EQ";
        case NEQ:        return "NEQ";
        case LE:         return "LE";
        case GE:         return "GE";
        case LT:         return "LT";
        case GT:         return "GT";
        case ASSIGN:     return "ASSIGN";
        case PLUS:       return "PLUS";
        case MINUS:      return "MINUS";
        case MULT:       return "MULT";
        case DIV:        return "DIV";
        case MOD:        return "MOD";
        case AND:        return "AND";
        case OR:         return "OR";
        case NOT:        return "NOT";
        case T_END:      return "T_END";
        case T_ERR:      return "T_ERR";
        default:         return "UNKNOWN";
    }
}

int main(int argc, char * argv[]) {
    TScanner * sc;

    // файл по умолчанию или заданный в командной строке
    if (argc <= 1)
        sc = new TScanner("input.txt");   
    else
        sc = new TScanner(argv[1]);       

    int type;
    LEX lex;
    
    // сканируем, пока не дойдём до конца файла или до ошибки
    do {
        type = sc->Scanner(lex);
        printf("%-15s -> %-12s (код %d)\n", lex, TypeName(type), type);
    } while (type != T_END && type != T_ERR);

    delete sc;
    return 0;
}