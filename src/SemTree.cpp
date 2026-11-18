#include "SemTree.h"
#include <stdio.h>

// Текущая область видимости (текущая вершина семантического дерева)
Tree * Tree::Cur = NULL;

// Значение поля объекта по записи поля класса
Value * TObject::Find(SemNode * field) {
    for (size_t i = 0; i < fields.size(); i++)
        if (fields[i].first == field)
            return &fields[i].second;
    return NULL;
}

// Текстовое представление значения для вывода и тестов
std::string ValueToString(const Value & v) {
    char buf[64];
    switch (v.type) {
        case TYPE_INT:   snprintf(buf, sizeof(buf), "%d", v.i); return buf;
        case TYPE_FLOAT: snprintf(buf, sizeof(buf), "%g", v.f); return buf;
        case TYPE_BOOL:  return v.b ? "true" : "false";
        default:         return "?";
    }
}

// Текстовое имя типа значения
const char * TypeName(DATA_TYPE t) {
    switch (t) {
        case TYPE_INT:    return "int";
        case TYPE_FLOAT:  return "float";
        case TYPE_BOOL:   return "bool";
        case TYPE_VOID:   return "void";
        case TYPE_CLASS:  return "класс";
        case TYPE_OBJECT: return "объект класса";
        case TYPE_FUNCT:  return "функция";
        case TYPE_SCOPE:  return "область видимости";
        default:          return "неизвестный тип";
    }
}

// Текстовое имя типа объекта с учётом ссылки на класс
std::string TypeNameOf(const SemNode * n) {
    if (n == NULL)
        return "нет";
    if (n->type == TYPE_OBJECT)
        return std::string("объект класса ") + (n->ref ? n->ref->id : "?");
    if (n->type == TYPE_FUNCT)
        return "void (метод)";
    if (n->type == TYPE_CLASS)
        return std::string("класс ") + n->id;
    return TypeName(n->type);
}

// Совместимость типов при присваивании: int и float приводятся друг к другу,
// bool только к bool, объекты - того же класса
bool TypesCompatible(DATA_TYPE dst, SemNode * dstCls,
                     DATA_TYPE src, SemNode * srcCls) {
    if (dst == src) {
        if (dst == TYPE_OBJECT)
            return dstCls == srcCls;
        return true;
    }
    bool dstNum = (dst == TYPE_INT || dst == TYPE_FLOAT);
    bool srcNum = (src == TYPE_INT || src == TYPE_FLOAT);
    return dstNum && srcNum;
}

// Конструктор вершины дерева
Tree::Tree(Tree * l, Tree * r, Tree * u, SemNode * d)
    : n(d), Up(u), Left(l), Right(r) {
    if (d != NULL)
        d->self = this;
}

// Деструктор: удаляет запись узла, затем рекурсивно потомков и соседей
Tree::~Tree() {
    if (Up == NULL)
        Cur = NULL;      // удаляется корень - текущая позиция недействительна
    delete n;            // запись таблицы принадлежит вершине дерева
    delete Left;
    delete Right;
}

// Создать первого потомка текущей области видимости
Tree * Tree::SetLeft(SemNode * d) {
    Left = new Tree(NULL, NULL, this, d);
    return Left;
}

// Добавить следующий элемент того же уровня (в конец цепочки потомков)
Tree * Tree::SetRight(SemNode * d) {
    if (Left == NULL)
        return SetLeft(d);
    Tree * p = Left;
    while (p->Right != NULL)
        p = p->Right;
    p->Right = new Tree(NULL, NULL, this, d);
    return p->Right;
}

// Присоединить готовую вершину к текущей (в конец списка потомков)
Tree * Tree::AddChild(Tree * child) {
    child->Up = this;
    if (Left == NULL) {
        Left = child;
        return child;
    }
    Tree * p = Left;
    while (p->Right != NULL)
        p = p->Right;
    p->Right = child;
    return child;
}

// Поиск идентификатора на одном уровне, области видимости TYPE_SCOPE пропускаются
Tree * Tree::FindUpOneLevel(Tree * from, const std::string & id) {
    if (from == NULL)
        return NULL;
    for (Tree * p = from->Left; p != NULL; p = p->Right)
        if (p->n != NULL && p->n->type != TYPE_SCOPE && p->n->id == id)
            return p;
    return NULL;
}

// Поиск идентификатора с учётом блочной видимости: текущий уровень, затем подъём до корня
Tree * Tree::FindUp(Tree * from, const std::string & id) {
    for (Tree * s = from; s != NULL; s = s->Up) {
        Tree * r = FindUpOneLevel(s, id);
        if (r != NULL)
            return r;
    }
    return NULL;
}

// Проверка повторного объявления в текущей области видимости
int Tree::DupControl(Tree * addr, const std::string & a) {
    return FindUpOneLevel(addr, a) != NULL;
}

// Занести идентификатор в текущую область видимости с контролем дублирования
Tree * Tree::SemInclude(const std::string & a, DATA_TYPE t, int line, int col) {
    if (DupControl(Cur, a)) {
        Tree * old = FindUpOneLevel(Cur, a);
        char buf[512];
        snprintf(buf, sizeof(buf),
                 "повторное объявление идентификатора \"%s\" "
                 "(ранее объявлен в строке %d, позиция %d)",
                 a.c_str(), old->n->line, old->n->col);
        throw TSemError(buf, line, col);
    }

    SemNode * d = new SemNode(a, t, line, col);
    if (Cur == NULL) {               // дерево ещё не создано - создаём корень
        Cur = new Tree(NULL, NULL, NULL, d);
        return Cur;
    }
    return Cur->SetRight(d);
}

// Найти переменную в видимых областях, имя функции или класса переменной не является
Tree * Tree::SemGetVar(const std::string & a, int line, int col) {
    Tree * v = FindUp(Cur, a);
    if (v == NULL) {
        char buf[512];
        snprintf(buf, sizeof(buf),
                 "идентификатор \"%s\" не объявлен", a.c_str());
        throw TSemError(buf, line, col);
    }
    if (v->n->type == TYPE_FUNCT) {
        char buf[512];
        snprintf(buf, sizeof(buf),
                 "\"%s\" - функция (метод), а не переменная", a.c_str());
        throw TSemError(buf, line, col);
    }
    if (v->n->type == TYPE_CLASS) {
        char buf[512];
        snprintf(buf, sizeof(buf),
                 "\"%s\" - имя класса, а не переменная", a.c_str());
        throw TSemError(buf, line, col);
    }
    return v;
}

// Найти запись класса в видимых областях
Tree * Tree::SemGetClass(const std::string & a, int line, int col) {
    Tree * v = FindUp(Cur, a);
    if (v == NULL || v->n->type != TYPE_CLASS) {
        char buf[512];
        snprintf(buf, sizeof(buf),
                 "класс \"%s\" не объявлен", a.c_str());
        throw TSemError(buf, line, col);
    }
    return v;
}

// Найти член класса по записи объекта, закрытый член виден только методам своего класса
Tree * Tree::SemFindMember(SemNode * obj, const std::string & a,
                           int line, int col) {
    if (obj == NULL || obj->ref == NULL) {
        char buf[512];
        snprintf(buf, sizeof(buf),
                 "обращение к члену \"%s\" возможно только у объекта класса",
                 a.c_str());
        throw TSemError(buf, line, col);
    }
    for (Tree * p = obj->ref->self->Left; p != NULL; p = p->Right) {
        if (p->n == NULL || p->n->type == TYPE_SCOPE || p->n->id != a)
            continue;

        // контроль прав доступа: private-член виден только методам своего класса
        if (p->n->isPrivate && EnclosingClass() != obj->ref) {
            char buf[512];
            snprintf(buf, sizeof(buf),
                     "член \"%s\" класса \"%s\" объявлен как private "
                     "и недоступен вне класса",
                     a.c_str(), obj->ref->id.c_str());
            throw TSemError(buf, line, col);
        }
        return p;
    }

    char buf[512];
    snprintf(buf, sizeof(buf),
             "в классе \"%s\" нет члена \"%s\"",
             obj->ref->id.c_str(), a.c_str());
    throw TSemError(buf, line, col);
}

// Класс, в теле которого находится текущая область видимости, нужен для контроля доступа
SemNode * Tree::EnclosingClass() {
    for (Tree * s = Cur; s != NULL; s = s->Up)
        if (s->n != NULL && s->n->type == TYPE_CLASS)
            return s->n;
    return NULL;
}

// Вход в новую область видимости (составной оператор, тело метода)
Tree * Tree::SemEnterBlock(int line, int col) {
    SemNode * d = new SemNode("", TYPE_SCOPE, line, col);
    Tree * b = new Tree(NULL, NULL, Cur, d);
    if (Cur != NULL)
        Cur->AddChild(b);
    Cur = b;
    return b;
}

// Выход из текущей области видимости; при freeBlock записи блока удаляются
void Tree::SemExitBlock(bool freeBlock) {
    if (Cur == NULL || Cur->Up == NULL)
        return;

    Tree * block  = Cur;
    Tree * parent = block->Up;
    Cur = parent;

    if (!freeBlock)
        return;

    // отсоединить блок от родителя, чтобы не удалить соседние записи
    if (parent->Left == block) {
        parent->Left = block->Right;
    }
    else {
        Tree * prev = parent->Left;
        while (prev != NULL && prev->Right != block)
            prev = prev->Right;
        if (prev != NULL)
            prev->Right = block->Right;
    }
    block->Right = NULL;
    delete block;

    // после освобождения памяти значение Cur должно указывать на живую вершину
    Cur = parent;
}

// Печать дерева (от корня) в поток out
void Tree::Print(FILE * out) {
    Tree * root = Cur;
    while (root != NULL && root->Up != NULL)
        root = root->Up;
    if (root == NULL)
        return;
    fprintf(out, "семантическое дерево:\n");
    fprintf(out, "  { глобальная область (строка %d)\n",
            root->n ? root->n->line : 1);
    root->PrintRec(2, out);
}

// Печать всех потомков вершины с отступом level
// Печать дерева со значениями переменных и пометкой текущей области видимости
void Tree::PrintTrace(FILE * out) {
    Tree * root = Cur;
    while (root != NULL && root->Up != NULL)
        root = root->Up;
    if (root == NULL)
        return;

    fprintf(out, "семантическое дерево:\n");
    for (int i = 0; i < 1; i++)
        fprintf(out, "  ");
    fprintf(out, "{ глобальная область (строка %d)",
            root->n ? root->n->line : 1);
    if (root == Cur)
        fprintf(out, "   <-- текущая область");
    fprintf(out, "\n");
    root->PrintTraceRec(2, out);
}

// Печать потомков вершины со значениями переменных и пометкой текущей области
void Tree::PrintTraceRec(int level, FILE * out) {
    for (Tree * p = Left; p != NULL; p = p->Right) {
        for (int i = 0; i < level; i++)
            fprintf(out, "  ");

        if (p->n == NULL) {
            fprintf(out, "?\n");
        }
        else if (p->n->type == TYPE_SCOPE) {
            fprintf(out, "{ блок (строка %d, позиция %d)",
                    p->n->line, p->n->col);
        }
        else {
            fprintf(out, "%s : %s", p->n->id.c_str(), TypeNameOf(p->n).c_str());
            if (p->n->val.has)
                fprintf(out, " = %s", ValueToString(p->n->val).c_str());
            fprintf(out, " (строка %d, позиция %d)", p->n->line, p->n->col);
        }
        if (p == Cur)
            fprintf(out, "   <-- текущая область");
        fprintf(out, "\n");
        p->PrintTraceRec(level + 1, out);
    }
}

void Tree::PrintRec(int level, FILE * out) {
    for (Tree * p = Left; p != NULL; p = p->Right) {
        for (int i = 0; i < level; i++)
            fprintf(out, "  ");

        if (p->n == NULL) {
            fprintf(out, "?\n");
        }
        else if (p->n->type == TYPE_SCOPE) {
            fprintf(out, "{ блок (строка %d, позиция %d)\n",
                    p->n->line, p->n->col);
        }
        else {
            fprintf(out, "%s : %s%s (строка %d, позиция %d)\n",
                    p->n->id.c_str(), TypeNameOf(p->n).c_str(),
                    p->n->isPrivate ? " [private]" : "",
                    p->n->line, p->n->col);
        }
        p->PrintRec(level + 1, out);
    }
}