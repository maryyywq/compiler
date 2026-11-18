#pragma once

#include "defs.h"
#include <string>
#include <vector>

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

// объект класса, созданный интерпретатором
struct TObject;

// значение данных, вычисленное интерпретатором
struct Value {
    DATA_TYPE type;   // тип значения
    bool      has;    // значение вычислено
    union {
        int      i;   // значение для TYPE_INT
        float    f;   // значение для TYPE_FLOAT
        bool     b;   // значение для TYPE_BOOL
        TObject *obj; // значение для TYPE_OBJECT
    };

    Value() : type(TYPE_VOID), has(false), i(0) {}

    static Value Int(int v)     { Value r; r.type = TYPE_INT;   r.has = true; r.i = v; return r; }
    static Value Float(float v) { Value r; r.type = TYPE_FLOAT; r.has = true; r.f = v; return r; }
    static Value Bool(bool v)   { Value r; r.type = TYPE_BOOL;  r.has = true; r.b = v; return r; }
    static Value Object(TObject * o) { Value r; r.type = TYPE_OBJECT; r.has = true; r.obj = o; return r; }
};

// текстовое представление значения для вывода и тестов: 5, 2.5, true
std::string ValueToString(const Value & v);

struct SemNode {
    std::string id;       // имя идентификатора ("" для области видимости)
    DATA_TYPE   type;     // тип объекта
    bool        isPrivate;// признак закрытого члена класса (private:)
    int         line;     // строка объявления (для сообщений об ошибках)
    int         col;      // позиция в строке (для сообщений об ошибках)
    SemNode    *ref;      // ссылка на запись класса 
    Tree       *self;     // вершина дерева, в которой находится запись
    Value       val;      // значение, вычисленное интерпретатором
    int         bodyPos;  // позиция '{' тела метода в исходном модуле
    int         bodyLine; // строка начала тела метода

    SemNode(const std::string & a, DATA_TYPE t, int l, int c,
            SemNode * r = NULL)
        : id(a), type(t), isPrivate(false), line(l), col(c),
          ref(r), self(NULL), bodyPos(-1), bodyLine(0) {}
};

// Объект класса: собственные значения полей каждого экземпляра
struct TObject {
    std::string clsName;   // имя класса
    std::string name;      // имя переменной или путь к полю: g1, o.b
    std::string scopeName; // подпись для трассировки: G g1
    Tree       *clsTree;   // вершина класса в семантическом дереве
    std::vector<std::pair<SemNode *, Value> > fields; // поле -> значение

    TObject() : clsTree(NULL) {}

    // значение поля объекта по записи поля класса
    Value * Find(SemNode * field);
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
    void   SemExitBlock(bool freeBlock = false);

    // Класс, в теле которого находится текущая область видимости, или NULL вне методов
    SemNode * EnclosingClass();

    // Печать дерева (по умолчанию - в стандартный поток вывода)
    void Print(FILE * out = stdout);
    void PrintRec(int level, FILE * out);

    // Печать дерева со значениями переменных и пометкой текущей области
    void PrintTrace(FILE * out = stdout);
    void PrintTraceRec(int level, FILE * out);
};

// Текстовое имя типа (для сообщений об ошибках и печати дерева)
const char * TypeName(DATA_TYPE t);

// Текстовое имя типа объекта с учётом ссылки на класс
std::string TypeNameOf(const SemNode * n);

// Совместимы ли типы при присваивании: int и float приводятся друг к другу, bool только к bool, объекты - того же класса
bool TypesCompatible(DATA_TYPE dst, SemNode * dstCls,
                     DATA_TYPE src, SemNode * srcCls);
