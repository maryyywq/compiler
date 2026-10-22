#pragma once

#include <string.h>
#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

// максимальный размер исходного модуля в символах
#define MAX_TEXT 100000
// максимальная длина одной лексемы вместе с завершающим нулём
#define MAX_LEX 100
// количество ключевых слов в таблице
#define MAX_KEYW 10

// максимальная длина идентификатора
#define MAX_IDENT 30
// максимальная длина числовой константы
#define MAX_CONST 30
// максимальная длина комментария
#define MAX_COMMENT 150
typedef char LEX[MAX_LEX];
typedef char TypeMod[MAX_TEXT];

// коды типов лексем, которые возвращает сканер
enum LEX_TYPE {
    KW_INT      = 1,
    KW_FLOAT    = 2,
    KW_BOOL     = 3,
    KW_VOID     = 4,
    KW_CLASS    = 5,
    KW_PUBLIC   = 6,
    KW_PRIVATE  = 7,
    KW_IF       = 8,
    KW_ELSE     = 9,
    KW_MAIN     = 10,

    IDENT       = 20,

    DEC_CONST   = 30,
    REAL_CONST  = 31,
    BOOL_CONST  = 32,

    SEMI        = 40,
    COMMA       = 41,
    LPAREN      = 42,
    RPAREN      = 43,
    LBRACE      = 44,
    RBRACE      = 45,
    COLON       = 46,
    DOT         = 47,
    TILDE       = 48,

    EQ          = 50,
    NEQ         = 51,
    LE          = 52,
    GE          = 53,
    LT          = 54,
    GT          = 55,
    ASSIGN      = 56,
    PLUS        = 57,
    MINUS       = 58,
    MULT        = 59,
    DIV         = 60,
    MOD         = 61,
    AND         = 62,
    OR          = 63,
    NOT         = 64,

    T_END       = 100
};

// базовое исключение компилятора: текст сообщения, место ошибки и вид ошибки
class TError {
public:
    std::string message;   // текст сообщения об ошибке
    int         line;      // строка исходного модуля, 0 - не применимо
    int         col;       // позиция в строке

    TError(const std::string & m, int l, int c = 0)
        : message(m), line(l), col(c) {}
    virtual ~TError() {}

    // название вида ошибки для вывода диагностики
    virtual const char * Kind() const { return "Ошибка"; }
};

// лексическая ошибка: недопустимый символ, слишком длинная лексема или незакрытый комментарий
class TLexError : public TError {
public:
    TLexError(const std::string & m, int l, int c = 0) : TError(m, l, c) {}
    const char * Kind() const { return "Лексическая ошибка"; }
};

// синтаксическая ошибка: входной текст не соответствует правилам грамматики
class TSynError : public TError {
public:
    TSynError(const std::string & m, int l, int c = 0) : TError(m, l, c) {}
    const char * Kind() const { return "Синтаксическая ошибка"; }
};

// семантическая ошибка: нарушено контекстное условие языка
class TSemError : public TError {
public:
    TSemError(const std::string & m, int l, int c = 0) : TError(m, l, c) {}
    const char * Kind() const { return "Семантическая ошибка"; }
};
