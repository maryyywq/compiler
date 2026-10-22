#pragma once

#include "defs.h"
#include <string>

// типы объектов и значений в семантическом дереве
enum DATA_TYPE {
    TYPE_INT    = 1,   // целый
    TYPE_FLOAT  = 2,   // вещественный
    TYPE_BOOL   = 3,   // логический
    TYPE_VOID   = 4,   // пустой, результат методов
    TYPE_CLASS  = 5,   // имя класса, ref == NULL
    TYPE_OBJECT = 6,   // переменная или поле класса, ref -> запись класса
    TYPE_FUNCT  = 7,   // метод класса или main, ref -> запись класса
    TYPE_SCOPE  = 8    // область видимости: блок или тело метода
};

// Запись семантической таблицы (узел семантического дерева)
class Tree;

struct SemNode {
    std::string id;       // имя идентификатора ("" для области видимости)
    DATA_TYPE   type;     // тип объекта
    bool        isPrivate;// признак закрытого члена класса (private:)
    int         line;     // строка объявления (для сообщений об ошибках)
    int         col;      // позиция в строке (для сообщений об ошибках)
    SemNode    *ref;      // ссылка на запись класса 
    Tree       *self;     // вершина дерева, в которой находится запись

    SemNode(const std::string & a, DATA_TYPE t, int l, int c,
            SemNode * r = NULL)
        : id(a), type(t), isPrivate(false), line(l), col(c),
          ref(r), self(NULL) {}
};

// Древовидная таблица символов: Left - первый внутренний элемент, Right - следующий на уровне
class Tree {
public:
    SemNode *n;      // данные узла (запись таблицы)
    Tree    *Up;     // родительская (внешняя) область видимости
    Tree    *Left;   // первый потомок - первая внутренняя запись
    Tree    *Right;  // следующий элемент на том же уровне

    static Tree *Cur;   // текущая область видимости при трансляции

    Tree(Tree * l, Tree * r, Tree * u, SemNode * d);
    ~Tree();

    // Навигация и модификация дерева
    Tree * SetLeft(SemNode * d);    // создать первого потомка текущей области
    Tree * SetRight(SemNode * d);   // добавить следующий элемент того же уровня
    Tree * AddChild(Tree * child);  // присоединить готовую вершину к текущей

    // Поиск
    Tree * FindUp(Tree * from, const std::string & id);
    Tree * FindUpOneLevel(Tree * from, const std::string & id);

    // Семантические операции (контроль контекстных условий)
    Tree * SemInclude(const std::string & a, DATA_TYPE t, int line, int col);
    Tree * SemGetVar(const std::string & a, int line, int col);
    Tree * SemGetClass(const std::string & a, int line, int col);
    Tree * SemFindMember(SemNode * obj, const std::string & a,
                         int line, int col);
    int    DupControl(Tree * addr, const std::string & a);
    Tree * SemEnterBlock(int line, int col);
    void   SemExitBlock();

    // Класс, в теле которого находится текущая область видимости, или NULL вне методов
    SemNode * EnclosingClass();

    // Печать дерева (по умолчанию - в стандартный поток вывода)
    void Print(FILE * out = stdout);
    void PrintRec(int level, FILE * out);
};

// Текстовое имя типа (для сообщений об ошибках и печати дерева)
const char * TypeName(DATA_TYPE t);

// Текстовое имя типа объекта с учётом ссылки на класс
std::string TypeNameOf(const SemNode * n);

// Совместимы ли типы при присваивании: int и float приводятся друг к другу, bool только к bool, объекты - того же класса
bool TypesCompatible(DATA_TYPE dst, SemNode * dstCls,
                     DATA_TYPE src, SemNode * srcCls);
