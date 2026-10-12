#include "defs.h"
#include "Scanner.h"
#include "Parser.h"
#include <stdio.h>

// Имя исходного модуля: аргумент командной строки или input.txt
static const char * SourceFile(int argc, char * argv[]) {
    return (argc > 1) ? argv[1] : "input.txt";
}

// Печать сообщения об ошибке
static void ReportError(const char * kind, const TError & e) {
    if (e.line > 0)
        printf("%s ошибка (строка %d): %s\n", kind, e.line, e.message.c_str());
    else
        printf("%s ошибка: %s\n", kind, e.message.c_str());
}

// Первый этап - лексический анализ: модуль сканируется целиком,
// полученные лексемы выводятся на экран
static void LexicalAnalysis(TScanner * sc) {
    printf("Лексический анализ:\n");

    int type;
    LEX lex;
    do {
        type = sc->Scanner(lex);
        //printf("  %-15s -> %-12s (код %d)\n", lex, TokenName(type), type);
    } while (type != T_END && type != T_ERR);
}

// Второй этап - синтаксический анализ
static void SyntaxAnalysis(TScanner * sc) {
    TParser parser(sc);
    parser.Program();
}

int main(int argc, char * argv[]) {
    const char * fileName = SourceFile(argc, argv);
    TScanner * sc = NULL;

    // Чтение исходного модуля
    try {
        sc = new TScanner(fileName);
    }
    catch (TError & e) {
        printf("Ошибка: %s\n", e.message.c_str());
        return 1;
    }

    // Этап 1: лексический анализ
    try {
        LexicalAnalysis(sc);
    }
    catch (TError & e) {
        ReportError("Лексическая", e);
        delete sc;
        return 1;
    }

    // Возвращаемся в начало текста для грамматического разбора
    sc->setPos(0);
    sc->setLine(1);

    // Этап 2: синтаксический анализ
    printf("\nСинтаксический анализ:\n");
    try {
        SyntaxAnalysis(sc);
    }
    catch (TError & e) {
        ReportError("Синтаксическая", e);
        delete sc;
        return 1;
    }

    printf("Успешно: ошибок нет\n");

    delete sc;
    return 0;
}
