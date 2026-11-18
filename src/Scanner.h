#pragma once

#include "defs.h"

// лексический анализатор: читает исходный модуль и выдаёт очередные лексемы
class TScanner {
private:
    TypeMod text;    // текст исходного модуля целиком
    std::string fileName; // имя исходного модуля (для повторного разбора)
    int     pos;     // текущая позиция в тексте
    int     line;    // текущая строка исходного модуля
    int     tokPos;  // позиция начала текущей (последней прочитанной) лексемы
    int     tokLine; // строка начала текущей лексемы
public:
    TScanner(const char * FileName);
    ~TScanner() {}

    // текущая позиция и строка в тексте, используются при повторном разборе
    void setPos(int i)  { pos = i; }
    int  getPos()       { return pos; }
    void setLine(int i) { line = i; }
    int  getLine()      { return line; }

    // позиция начала текущей лексемы, нужна для сообщений об ошибках
    int  getTokPos()  { return tokPos; }
    int  getTokLine() { return tokLine; }
    int  getCol()     { return ColOf(tokPos); }
    void setTokPos(int i)  { tokPos  = i; }
    void setTokLine(int i) { tokLine = i; }

    // имя исходного модуля
    const char * GetFileName() { return fileName.c_str(); }

    // номер позиции в строке (с 1) для символа с индексом at
    int  ColOf(int at);

    // выбросить лексическую ошибку, функция не возвращает управление
    [[noreturn]] void printError(const char * err, const char * lex);

    // прочитать очередную лексему, вернуть её тип, изображение записать в l
    int Scanner(LEX l);

    // прочитать исходный модуль из файла в буфер text
    void getData(const char * FileName);
};
