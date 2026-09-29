// Scanner.h - класс лексического анализатора
#ifndef __SCANNER
#define __SCANNER

#include "defs.h"

class TScanner {
private:
    TypeMod text;    // текст исходного модуля
    int     pos;     // текущая позиция в тексте
    int     line;    // текущая строка
public:
    TScanner(const char * FileName);
    ~TScanner() {}

    // управление позицией в тексте
    void setPos(int i)  { pos = i; }
    int  getPos()       { return pos; }
    void setLine(int i) { line = i; }
    int  getLine()      { return line; }

    // вывод сообщения об ошибке
    void printError(const char * err, const char * lex);

    // основная функция сканирования
    int Scanner(LEX l);

    // загрузка исходного модуля
    void getData(const char * FileName);
};

#endif