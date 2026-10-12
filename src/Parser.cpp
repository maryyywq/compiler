#include "defs.h"
#include "Scanner.h"
#include "Parser.h"
#include <stdio.h>
#include <string.h>

// Текстовые имена типов лексем (для сообщений об ошибках)
const char * TokenName(int type) {
    switch (type) {
        case KW_INT:     return "int";
        case KW_FLOAT:   return "float";
        case KW_BOOL:    return "bool";
        case KW_VOID:    return "void";
        case KW_CLASS:   return "class";
        case KW_PUBLIC:  return "public";
        case KW_PRIVATE: return "private";
        case KW_IF:      return "if";
        case KW_ELSE:    return "else";
        case KW_MAIN:    return "main";
        case IDENT:      return "идентификатор";
        case DEC_CONST:  return "целая константа";
        case REAL_CONST: return "вещественная константа";
        case BOOL_CONST: return "логическая константа";
        case SEMI:       return ";";
        case COMMA:      return ",";
        case LPAREN:     return "(";
        case RPAREN:     return ")";
        case LBRACE:     return "{";
        case RBRACE:     return "}";
        case COLON:      return ":";
        case DOT:        return ".";
        case TILDE:      return "~";
        case EQ:         return "==";
        case NEQ:        return "!=";
        case LE:         return "<=";
        case GE:         return ">=";
        case LT:         return "<";
        case GT:         return ">";
        case ASSIGN:     return "=";
        case PLUS:       return "+";
        case MINUS:      return "-";
        case MULT:       return "*";
        case DIV:        return "/";
        case MOD:        return "%";
        case AND:        return "&&";
        case OR:         return "||";
        case NOT:        return "!";
        case T_END:      return "конец модуля";
        case T_ERR:      return "ошибка";
        default:         return "неизвестная лексема";
    }
}

// Конструктор: привязывает сканер и читает первую лексему
TParser::TParser(TScanner * s) : sc(s) {
    next();
}

// Вспомогательные функции

// Перейти к следующей лексеме
void TParser::next() {
    t = sc->Scanner(lex);
}

// Тип лексемы, отстоящей на k позиций вперёд (текущая не считается,
// т.е. lookAhead(1) - следующая лексема). Позиция сканера не меняется
int TParser::lookAhead(int k) {
    int savePos  = sc->getPos();
    int saveLine = sc->getLine();

    int type = T_END;
    LEX l;
    for (int i = 0; i < k; i++) {
        type = sc->Scanner(l);
        if (type == T_END) break;
    }

    sc->setPos(savePos);
    sc->setLine(saveLine);
    return type;
}

// Проверить тип текущей лексемы и пропустить её
void TParser::match(int expected, const char * what) {
    if (t != expected) {
        char buf[256];
        snprintf(buf, sizeof(buf), "ожидалось: %s", what);
        error(buf);
    }
    next();
}

// Сообщение о синтаксической ошибке
void TParser::error(const char * msg) {
    std::string full = std::string(msg) + ", а встретилось: " + TokenName(t);
    if (t != T_END && t != T_ERR && lex[0] != '\0')
        full += std::string(" \"") + lex + "\"";
    throw TError(full, sc->getLine());
}

// Начинается ли лексема одним из типов int|float|bool|IDENT
bool TParser::isTypeStart() {
    return t == KW_INT || t == KW_FLOAT || t == KW_BOOL || t == IDENT;
}

// Является ли текущая позиция началом объявления переменной
// Внутри блока тип может быть задан идентификатором (имя класса),
// поэтому сначала проверяется, что далее идёт имя переменной (IDENT)
bool TParser::isVarDeclStart() {
    if (t == KW_INT || t == KW_FLOAT || t == KW_BOOL)
        return true;
    if (t == IDENT)
        return lookAhead(1) == IDENT;
    return false;
}

// 1) Программа - аксиома грамматики
//    Program -> TopDecl*   (заканчивается концом модуля)
void TParser::Program() {
    while (t != T_END)
        TopDecl();
}

// 2) Описания (на верхнем уровне) - TopDecl
//    TopDecl -> ClassDecl | VarDecl | MainFunc
void TParser::TopDecl() {
    if (t == KW_CLASS)
        ClassDecl();
    else if (t == KW_VOID)
        MainFunc();
    else if (isTypeStart())
        VarDecl();
    else
        error("ожидалось описание верхнего уровня (class, тип или void main)");
}

// 3) Класс - ClassDecl
//    ClassDecl -> class IDENT { ClassMemberList }
void TParser::ClassDecl() {
    match(KW_CLASS, "class");
    match(IDENT,    "имя класса");
    match(LBRACE,   "{");
    ClassMemberList();
    match(RBRACE,   "}");
}

// 4) Список членов класса - ClassMemberList
//    ClassMemberList -> ClassMember*
void TParser::ClassMemberList() {
    while (t != RBRACE && t != T_END)
        ClassMember();
}

// 5) Член класса - ClassMember
//    ClassMember -> AccessSpec | VarDecl
//                 | void IDENT ( ) Block
//                 | IDENT ( ) Block
//                 | ~ IDENT ( ) Block
void TParser::ClassMember() {
    if (t == KW_PUBLIC || t == KW_PRIVATE) {
        AccessSpec();
    }
    else if (t == KW_VOID) {            // Метод: void IDENT ( ) Block
        next();
        match(IDENT,  "имя метода");
        match(LPAREN, "(");
        match(RPAREN, ")");
        Block();
    }
    else if (t == TILDE) {              // Деструктор: ~ IDENT ( ) Block
        next();
        match(IDENT,  "имя деструктора");
        match(LPAREN, "(");
        match(RPAREN, ")");
        Block();
    }
    else if (t == KW_INT || t == KW_FLOAT || t == KW_BOOL) {
        VarDecl();                      // Поле - объявление переменной
    }
    else if (t == IDENT) {
        if (lookAhead(1) == LPAREN) {   // Конструктор: IDENT ( ) Block
            match(IDENT,  "имя конструктора");
            match(LPAREN, "(");
            match(RPAREN, ")");
            Block();
        }
        else {
            VarDecl();                  // Поле типа IDENT
        }
    }
    else {
        error("ожидался член класса");
    }
}

// 6) Спецификатор доступа - AccessSpec
//    AccessSpec -> public : | private :
void TParser::AccessSpec() {
    if (t == KW_PUBLIC || t == KW_PRIVATE)
        next();
    else
        error("ожидался спецификатор доступа public или private");
    match(COLON, ":");
}

// 7) Объявление переменной - VarDecl
//    VarDecl -> Type IdInitList ;
void TParser::VarDecl() {
    Type();
    IdInitList();
    match(SEMI, ";");
}

// 8) Тип - Type
//    Type -> int | float | bool | IDENT
void TParser::Type() {
    if (isTypeStart())
        next();
    else
        error("ожидался тип (int, float, bool или имя класса)");
}

// 9) Главная функция - MainFunc
//    MainFunc -> void main ( ) Block
void TParser::MainFunc() {
    match(KW_VOID, "void");
    match(KW_MAIN, "main");
    match(LPAREN,  "(");
    match(RPAREN,  ")");
    Block();
}

// 10) Блок - Block
//     Block -> { (Stmt | VarDecl)* }
void TParser::Block() {
    match(LBRACE, "{");
    while (t != RBRACE && t != T_END) {
        if (isVarDeclStart())
            VarDecl();
        else
            Stmt();
    }
    match(RBRACE, "}");
}

// 11) Оператор - Stmt
//     Stmt -> ; | Block | if ( Expr ) Stmt ElseOpt | LValue StmtTail
void TParser::Stmt() {
    if (t == SEMI) {
        next();                        // Пустой оператор
    }
    else if (t == LBRACE) {
        Block();                       // Составной оператор
    }
    else if (t == KW_IF) {
        next();
        match(LPAREN, "(");
        Expr();
        match(RPAREN, ")");
        Stmt();
        ElseOpt();
    }
    else if (t == IDENT) {
        LValue();
        StmtTail();
    }
    else {
        error("ожидался оператор");
    }
}

// 12) Окончание оператора - StmtTail
//     StmtTail -> = Expr ; | . IDENT ( ) ;
void TParser::StmtTail() {
    if (t == ASSIGN) {                 // Присваивание
        next();
        Expr();
        match(SEMI, ";");
    }
    else if (t == DOT) {               // Вызов метода: LValue . IDENT ( ) ;
        next();
        match(IDENT,  "имя метода");
        match(LPAREN, "(");
        match(RPAREN, ")");
        match(SEMI,   ";");
    }
    else {
        error("ожидался знак '=' или вызов метода");
    }
}

// 13) Ветвь else - ElseOpt
//     ElseOpt -> else Stmt | eps
void TParser::ElseOpt() {
    if (t == KW_ELSE) {
        next();
        Stmt();
    }
}

// 14) Левое выражение - LValue
//     LValue -> IDENT ( . IDENT )*
//     Цепочка ". IDENT", за которой следует "(", не читается:
//     она относится к вызову метода (диаграмма StmtTail)
void TParser::LValue() {
    match(IDENT, "идентификатор");
    while (t == DOT) {
        if (lookAhead(1) == IDENT && lookAhead(2) == LPAREN)
            break;                     // Это селектор вызова метода
        match(DOT,   ".");
        match(IDENT, "идентификатор");
    }
}

// 15) Выражение - Expr
//     Expr -> LOr ( || LOr )*
void TParser::Expr() {
    LOr();
    while (t == OR) {
        next();
        LOr();
    }
}

// 16) Логическое ИЛИ - LOr
//     LOr -> LAnd ( && LAnd )*
void TParser::LOr() {
    LAnd();
    while (t == AND) {
        next();
        LAnd();
    }
}

// 17) Логическое И - LAnd
//     LAnd -> Rel ( ( < | <= | > | >= ) Rel )*
void TParser::LAnd() {
    Rel();
    while (t == LT || t == LE || t == GT || t == GE) {
        next();
        Rel();
    }
}

// 18) Отношения - Rel
//     Rel -> Eq ( ( == | != ) Eq )*
void TParser::Rel() {
    Eq();
    while (t == EQ || t == NEQ) {
        next();
        Eq();
    }
}

// 19) Равенство - Eq
//     Eq -> Add ( ( + | - ) Add )*
void TParser::Eq() {
    Add();
    while (t == PLUS || t == MINUS) {
        next();
        Add();
    }
}

// 20) Аддитивные - Add
//     Add -> Mul ( ( * | / | % ) Mul )*
void TParser::Add() {
    Mul();
    while (t == MULT || t == DIV || t == MOD) {
        next();
        Mul();
    }
}

// 21) Унарные - Mul
//     Mul -> + Prim | - Prim | ! Prim | Prim
void TParser::Mul() {
    if (t == PLUS || t == MINUS || t == NOT) {
        next();
        Prim();
    }
    else {
        Prim();
    }
}

// 22) Элементарное выражение - Prim
//     Prim -> LValue | Const | ( Expr )
void TParser::Prim() {
    if (t == IDENT) {
        LValue();
    }
    else if (t == DEC_CONST || t == REAL_CONST || t == BOOL_CONST) {
        Const();
    }
    else if (t == LPAREN) {
        next();
        Expr();
        match(RPAREN, ")");
    }
    else {
        error("ожидалось элементарное выражение");
    }
}

// 23) Константы - Const
//     Const -> DEC_CONST | REAL_CONST | BOOL_CONST
void TParser::Const() {
    if (t == DEC_CONST || t == REAL_CONST || t == BOOL_CONST)
        next();
    else
        error("ожидалась константа");
}

// Вспомогательные правила

// IdInitList -> IdInit ( , IdInit )*
void TParser::IdInitList() {
    IdInit();
    while (t == COMMA) {
        next();
        IdInit();
    }
}

// IdInit -> IDENT InitOpt,  InitOpt -> = Expr | eps
void TParser::IdInit() {
    match(IDENT, "имя переменной");
    if (t == ASSIGN) {
        next();
        Expr();
    }
}
