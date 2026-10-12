#pragma once

#include "defs.h"
#include "Scanner.h"

class TParser {
private:
    TScanner * sc;    // Лексический анализатор
    int        t;     // Тип текущей лексемы
    LEX        lex;   // Изображение текущей лексемы

    // Вспомогательные функции
    void next();                       // Перейти к следующей лексеме
    int  lookAhead(int k);             // Тип лексемы через k позиций (без сдвига)
    void match(int expected, const char * what); // Проверить и пропустить лексему
    void error(const char * msg);      // Сгенерировать синтаксическую ошибку

    bool isTypeStart();                // Начинается ли лексема типом
    bool isVarDeclStart();             // Начинается ли фрагмент объявлением переменной

    // Функции синтаксических диаграмм 
    void TopDecl();          // 2)  Описания (на верхнем уровне)
    void ClassDecl();        // 3)  Класс
    void ClassMemberList();  // 4)  Список членов класса
    void ClassMember();      // 5)  Член класса
    void AccessSpec();       // 6)  Спецификатор доступа
    void VarDecl();          // 7)  Объявление переменной
    void Type();             // 8)  Тип
    void MainFunc();         // 9)  Главная функция
    void Block();            // 10) Блок
    void Stmt();             // 11) Оператор
    void StmtTail();         // 12) Окончание оператора
    void ElseOpt();          // 13) Ветвь else
    void LValue();           // 14) Левое выражение
    void Expr();             // 15) Выражение
    void LOr();              // 16) Логическое ИЛИ
    void LAnd();             // 17) Логическое И
    void Rel();              // 18) Отношения
    void Eq();               // 19) Равенство
    void Add();              // 20) Аддитивные
    void Mul();              // 21) Унарные
    void Prim();             // 22) Элементарное выражение
    void Const();            // 23) Константы

    // Вспомогательные правила грамматики
    void IdInitList();       // Список объявляемых переменных
    void IdInit();           // Одна переменная с необязательным = Expr

public:
    TParser(TScanner * s);
    ~TParser() {}

    // 1) Программа - аксиома грамматики
    void Program();
};

// Текстовое имя типа лексемы (для сообщений об ошибках)
const char * TokenName(int type);
