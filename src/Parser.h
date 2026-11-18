#pragma once

#include "defs.h"
#include "Scanner.h"
#include "SemTree.h"
#include "Interp.h"

// тип значения, вычисленный при разборе, вместе со ссылкой на запись класса
struct TypeInfo {
    DATA_TYPE type;
    SemNode * cls;
    bool      fromComparison;  // true - результат операции сравнения (bool, приводимый к числу)

    TypeInfo(DATA_TYPE t = TYPE_VOID, SemNode * c = NULL, bool fc = false)
        : type(t), cls(c), fromComparison(fc) {}
};

// сведения о левом выражении: тип, объект для вызова метода, допустимость присваивания и позиция
struct LValueInfo {
    TypeInfo    t;           // тип значения левого выражения
    SemNode   * obj;         // запись переменной или последнего поля цепочки
    TObject   * robj;        // объект, в котором хранится поле (NULL для простой переменной)
    TObject   * receiver;    // объект-значение левого выражения (для вызова метода)
    bool        assignable;  // можно ли присваивать значение (переменная или поле)
    bool        simple;      // true - простая переменная без обращений к полям
    std::string path;        // путь для трассировки: p.x
    int         line, col;   // позиция начала левого выражения

    LValueInfo() : obj(NULL), robj(NULL), receiver(NULL),
                   assignable(false), simple(true), line(0), col(0) {}
};

// синтаксический и семантический анализатор, построенный методом рекурсивного спуска
class TParser {
private:
    TScanner * sc;       // лексический анализатор
    Tree     * root;     // корень семантического дерева (глобальная область)
    bool       semOn;    // выполнять ли семантический контроль
    bool       interpOn; // выполнять ли интерпретацию (вычисление значений)
    bool       treeTrace;// печатать ли дерево при входе в блок и выходе из него
    bool       exec;     // флаг интерпретации: выполняется ли текущий фрагмент
    const char * scope;  // имя текущей области для трассировки: global, main или класс
    Value      lastValue;// значение последнего разобранного выражения
    TObject   * curObj;  // объект, чей метод выполняется (NULL вне методов)
    std::string fileName;// исходный модуль для повторного разбора тел методов
    int        t;        // тип текущей лексемы
    LEX        lex;      // изображение текущей лексемы

    // переход к следующей лексеме и проверка текущей
    void next();
    int  lookAhead(int k);
    void match(int expected, const char * what);
    [[noreturn]] void error(const char * msg);

    // проверки, начинается ли текущая позиция типом или объявлением переменной
    bool isTypeStart();
    bool isVarDeclStart();

    // текущий режим доступа к членам класса: false - public, true - private
    bool memberPrivate;

    // функции синтаксических диаграмм, в них же выполняются семантические действия
    void TopDecl();          // 2) описания на верхнем уровне
    void ClassDecl();        // 3) класс
    void ClassMemberList(SemNode * clsRec, const std::string & clsName);
    void ClassMember(SemNode * clsRec, const std::string & clsName);
    void AccessSpec();       // 6) спецификатор доступа
    void VarDecl(bool classMember = false); // 7) объявление переменной
    TypeInfo Type();         // 8) тип
    void MainFunc();         // 9) главная функция
    void Block();            // 10) блок
    void Stmt();             // 11) оператор
    void StmtTail(const LValueInfo & lv); // 12) окончание оператора
    void CallOwnMethod();    // вызов метода текущего объекта: IDENT ( ) ;
    void ElseOpt();          // 13) ветвь else
    LValueInfo LValue();     // 14) левое выражение
    TypeInfo Expr();         // 15) выражение
    TypeInfo LOr();          // 16) логическое ИЛИ
    TypeInfo LAnd();         // 17) логическое И
    TypeInfo Rel();          // 18) отношения
    TypeInfo Eq();           // 19) равенство
    TypeInfo Add();          // 20) аддитивные
    TypeInfo Mul();          // 21) унарные
    TypeInfo Prim();         // 22) элементарное выражение
    TypeInfo Const();        // 23) константы

    // список объявляемых переменных и одна переменная с необязательной инициализацией
    void IdInitList(const TypeInfo & ti, bool classMember);
    void IdInit(const TypeInfo & ti, bool classMember);

    // работа с объектами классов
    TObject * CreateObject(SemNode * cls, int line, int col,
                           const std::string & name);
    void      DestroyObject(TObject * obj);
    void      CopyObject(TObject * dst, TObject * src, int line, int col);
    void      DestroyScopeObjects(Tree * scope);
    void      RunMethod(SemNode * m, TObject * obj, int line, int col);
    SemNode * FindClassMember(SemNode * cls, const std::string & name,
                              DATA_TYPE type);
    SemNode * FindCtor(SemNode * cls);
    SemNode * FindDtor(SemNode * cls);

    // семантические подпрограммы контроля типов
    TypeInfo BinaryNum(const TypeInfo & a, const TypeInfo & b,
                       int line, int col, const std::string & op);
    TypeInfo BinaryBool(const TypeInfo & a, const TypeInfo & b,
                        int line, int col, const std::string & op);
    void CheckAssign(const TypeInfo & dst, const TypeInfo & src,
                     int line, int col, const std::string & ctx);
    std::string TypeStr(const TypeInfo & ti);

public:
    TParser(TScanner * s, bool semantics = true, bool interpret = false,
            bool treeTrace = false);
    ~TParser();

    // 1) программа - аксиома грамматики, разбирается до конца модуля
    void Program();

    // корень построенного семантического дерева
    Tree * SemTreeRoot() { return root; }
};

// текстовое имя типа лексемы для сообщений об ошибках
const char * TokenName(int type);