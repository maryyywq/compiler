#ifndef __DEFS
#define __DEFS

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

// максимальные размеры буферов
#define MAX_TEXT    100000   
#define MAX_LEX     100      
#define MAX_KEYW    10      

// ограничения на длину лексем
#define MAX_IDENT   30       
#define MAX_CONST   30       
#define MAX_COMMENT 150      
typedef char LEX[MAX_LEX];
typedef char TypeMod[MAX_TEXT];

// коды типов лексем
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

    T_END       = 100,
    T_ERR       = 200
};

#endif