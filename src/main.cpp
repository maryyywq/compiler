#include "defs.h"
#include "Scanner.h"
#include "Parser.h"
#include "SemTree.h"
#include <stdio.h>

// имя исходного модуля берётся из командной строки, по умолчанию это input.txt
static const char * SourceFile(int argc, char * argv[]) {
    return (argc > 1) ? argv[1] : "input.txt";
}

// печать ошибки: вид ошибки, строка, позиция в строке и текст сообщения
static void ReportError(const TError & e) {
    printf("%s", e.Kind());
    if (e.line > 0 && e.col > 0)
        printf(" (строка %d, позиция %d)", e.line, e.col);
    else if (e.line > 0)
        printf(" (строка %d)", e.line);
    printf(": %s\n", e.message.c_str());
}

// лексический анализ: модуль сканируется целиком, ошибки выбрасывает сканер
static void LexicalAnalysis(TScanner * sc) {
    LEX lex;
    while (sc->Scanner(lex) != T_END)
        ;   // изображение лексемы не нужно, важно лишь дойти до конца модуля
}

// синтаксический анализ
static void SyntaxAnalysis(TScanner * sc) {
    TParser parser(sc, false);
    parser.Program();
}

// семантический анализ: тот же разбор, но с контролем контекстных условий
static void SemanticAnalysis(TScanner * sc) {
    TParser parser(sc, true);
    parser.Program();
    printf("Семантический анализ выполнен\nОшибок нет\n\n");
    parser.SemTreeRoot()->Print();
}

// вернуть сканер в начало модуля перед очередным проходом
static void Rewind(TScanner * sc) {
    sc->setPos(0);
    sc->setLine(1);
}

int main(int argc, char * argv[]) {
    const char * fileName = SourceFile(argc, argv);
    TScanner * sc = NULL;

    // чтение исходного модуля, ошибка здесь относится к этапу подготовки
    try {
        sc = new TScanner(fileName);
    }
    catch (TError & e) {
        ReportError(e);
        return 1;
    }

    // первый этап: лексический анализ
    try {
        LexicalAnalysis(sc);
        printf("Лексический анализ выполнен\nОшибок нет\n");
    }
    catch (TLexError & e) {
        ReportError(e);
        delete sc;
        return 1;
    }

    Rewind(sc);

    // второй этап: синтаксический анализ
    try {
        SyntaxAnalysis(sc);
        printf("Синтаксический анализ выполнен\nОшибок нет\n");
    }
    catch (TSynError & e) {
        ReportError(e);
        delete sc;
        return 1;
    }

    Rewind(sc);

    // третий этап: семантический анализ и печать построенного дерева
    try {
        SemanticAnalysis(sc);
    }
    catch (TSemError & e) {
        ReportError(e);
        delete sc;
        return 1;
    }

    delete sc;
    return 0;
}
