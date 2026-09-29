// Scanner.cpp - реализация класса сканера
#include "defs.h"
#include "Scanner.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

// таблица ключевых слов
static LEX Keywords[MAX_KEYW] = {
    "int", "float", "bool", "void", "class",
    "public", "private", "if", "else", "main"
};

// коды типов, соответствующие ключевым словам
static int KeywordTypes[MAX_KEYW] = {
    KW_INT, KW_FLOAT, KW_BOOL, KW_VOID, KW_CLASS,
    KW_PUBLIC, KW_PRIVATE, KW_IF, KW_ELSE, KW_MAIN
};

// конструктор: загружает текст и ставит указатель в начало
TScanner::TScanner(const char * FileName) {
    getData(FileName);
    pos  = 0;
    line = 1;
}

// вывод сообщения об ошибке и завершение работы
void TScanner::printError(const char * err, const char * lex) {
    if (lex == NULL || lex[0] == '\0')
        printf("Ошибка (строка %d): %s\n", line, err);
    else
        printf("Ошибка (строка %d): %s. Неверный фрагмент: \"%s\"\n",
               line, err, lex);
    exit(0);
}

// загрузка всего исходного модуля в буфер text
void TScanner::getData(const char * FileName) {
    FILE * in = fopen(FileName, "r");
    if (in == NULL) {
        printf("Отсутствует входной файл %s\n", FileName);
        exit(-1);
    }
    int i = 0;
    char c;
    while (fscanf(in, "%c", &c) != EOF) {
        if (i >= MAX_TEXT - 1) {
            printf("Слишком большой размер исходного модуля\n");
            break;
        }
        text[i++] = c;
    }
    text[i] = '\0'; // маркер конца текста
    fclose(in);
}

// основная функция сканирования
int TScanner::Scanner(LEX l) {
    // очищаем буфер лексемы
    for (int i = 0; i < MAX_LEX; i++) l[i] = '\0';
    int i = 0;

    // пропуск незначащих символов и комментариев
    while (true) {
        if (text[pos] == '\0') break;

        // пробельные символы
        if (text[pos] == ' ' || text[pos] == '\t' || text[pos] == '\r') {
            pos++; continue;
        }
        if (text[pos] == '\n') { line++; pos++; continue; }

        // однострочный комментарий 
        if (text[pos] == '/' && text[pos+1] == '/') {
            pos += 2;
            int len = 0;
            while (text[pos] != '\n' && text[pos] != '\0') {
                if (++len > MAX_COMMENT) {
                    printError("Слишком длинный комментарий "
                               "(макс. 150 символов)", "");
                    return T_ERR;
                }
                pos++;
            }
            continue;
        }

        // многострочный комментарий 
        if (text[pos] == '/' && text[pos+1] == '*') {
            pos += 2;
            int len = 0;
            while (text[pos] != '\0' &&
                   !(text[pos] == '*' && text[pos+1] == '/')) {
                if (++len > MAX_COMMENT) {
                    printError("Слишком длинный комментарий "
                               "(макс. 150 символов)", "");
                    return T_ERR;
                }
                if (text[pos] == '\n') line++;
                pos++;
            }
            if (text[pos] == '\0') {
                printError("Незакрытый комментарий", "");
                return T_ERR;
            }
            pos += 2;
            continue;
        }
        break;
    }

    // конец исходного модуля
    if (text[pos] == '\0') {
        l[0] = '\0';
        return T_END;
    }

    // идентификатор / ключевое слово / boolean
    if (isalpha((unsigned char)text[pos]) || text[pos] == '_') {
        int len = 0;
        while (isalnum((unsigned char)text[pos]) || text[pos] == '_') {
            if (++len > MAX_IDENT) {
                printError("Слишком длинный идентификатор "
                           "(макс. 30 символов)", l);
                return T_ERR;
            }
            l[i++] = text[pos];
            pos++;
        }
        l[i] = '\0';

        // ключевое слово
        for (int j = 0; j < MAX_KEYW; j++)
            if (strcmp(l, Keywords[j]) == 0)
                return KeywordTypes[j];

        // булевы константы
        if (strcmp(l, "true") == 0 || strcmp(l, "false") == 0)
            return BOOL_CONST;

        return IDENT;
    }

    // числовые константы
    if (isdigit((unsigned char)text[pos]) ||
        (text[pos] == '.' && isdigit((unsigned char)text[pos+1]))) {

        bool isReal = false;
        int  len    = 0;

        // целая часть
        while (isdigit((unsigned char)text[pos])) {
            if (++len > MAX_CONST) {
                printError("Слишком длинная константа "
                           "(макс. 30 символов)", l);
                return T_ERR;
            }
            l[i++] = text[pos];
            pos++;
        }

        // дробная часть
        if (text[pos] == '.') {
            isReal = true;
            if (++len > MAX_CONST) {
                printError("Слишком длинная константа "
                           "(макс. 30 символов)", l);
                return T_ERR;
            }
            l[i++] = text[pos];
            pos++;
            while (isdigit((unsigned char)text[pos])) {
                if (++len > MAX_CONST) {
                    printError("Слишком длинная константа "
                               "(макс. 30 символов)", l);
                    return T_ERR;
                }
                l[i++] = text[pos];
                pos++;
            }
        }
        l[i] = '\0';
        return isReal ? REAL_CONST : DEC_CONST;
    }

    // одиночные и составные знаки
    char c = text[pos];

    switch (c) {
        case ';': l[0]=c; l[1]='\0'; pos++; return SEMI;
        case ',': l[0]=c; l[1]='\0'; pos++; return COMMA;
        case '(': l[0]=c; l[1]='\0'; pos++; return LPAREN;
        case ')': l[0]=c; l[1]='\0'; pos++; return RPAREN;
        case '{': l[0]=c; l[1]='\0'; pos++; return LBRACE;
        case '}': l[0]=c; l[1]='\0'; pos++; return RBRACE;
        case ':': l[0]=c; l[1]='\0'; pos++; return COLON;
        case '.': l[0]=c; l[1]='\0'; pos++; return DOT;
        case '~': l[0]=c; l[1]='\0'; pos++; return TILDE;
        case '+': l[0]=c; l[1]='\0'; pos++; return PLUS;
        case '-': l[0]=c; l[1]='\0'; pos++; return MINUS;
        case '*': l[0]=c; l[1]='\0'; pos++; return MULT;
        case '%': l[0]=c; l[1]='\0'; pos++; return MOD;

        case '/':
            l[0]=c; l[1]='\0'; pos++; return DIV;

        case '=':
            l[0]=c; pos++;
            if (text[pos] == '=') {
                l[1]='='; l[2]='\0'; pos++;
                return EQ;
            }
            l[1]='\0';
            return ASSIGN;

        case '!':
            l[0]=c; pos++;
            if (text[pos] == '=') {
                l[1]='='; l[2]='\0'; pos++;
                return NEQ;
            }
            l[1]='\0';
            return NOT;

        case '<':
            l[0]=c; pos++;
            if (text[pos] == '=') {
                l[1]='='; l[2]='\0'; pos++;
                return LE;
            }
            l[1]='\0';
            return LT;

        case '>':
            l[0]=c; pos++;
            if (text[pos] == '=') {
                l[1]='='; l[2]='\0'; pos++;
                return GE;
            }
            l[1]='\0';
            return GT;

        case '&':
            l[0]=c; pos++;
            if (text[pos] == '&') {
                l[1]='&'; l[2]='\0'; pos++;
                return AND;
            }
            l[1]='\0';
            printError("Ожидался символ '&' после '&'", l);
            return T_ERR;

        case '|':
            l[0]=c; pos++;
            if (text[pos] == '|') {
                l[1]='|'; l[2]='\0'; pos++;
                return OR;
            }
            l[1]='\0';
            printError("Ожидался символ '|' после '|'", l);
            return T_ERR;

        default:
            l[0] = c; l[1] = '\0';
            pos++;
            printError("Неверный символ", l);
            return T_ERR;
    }
}