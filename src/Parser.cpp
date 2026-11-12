#include "defs.h"
#include "Scanner.h"
#include "Parser.h"
#include "SemTree.h"
#include <stdio.h>
#include <string.h>

// Текстовые имена типов лексем (для сообщений об ошибках)
const char * TokenName(int type) {
    switch (type) {
        case KW_INT:     return "int";
        case KW_FLOAT:   return "float";
        case KW_BOOL:    return "bool";
        case KW_VOID:    return "void";
        case KW_CLASS:   return "class";
        case KW_PUBLIC:  return "public";
        case KW_PRIVATE: return "private";
        case KW_IF:      return "if";
        case KW_ELSE:    return "else";
        case KW_MAIN:    return "main";
        case IDENT:      return "идентификатор";
        case DEC_CONST:  return "целая константа";
        case REAL_CONST: return "вещественная константа";
        case BOOL_CONST: return "логическая константа";
        case SEMI:       return ";";
        case COMMA:      return ",";
        case LPAREN:     return "(";
        case RPAREN:     return ")";
        case LBRACE:     return "{";
        case RBRACE:     return "}";
        case COLON:      return ":";
        case DOT:        return ".";
        case TILDE:      return "~";
        case EQ:         return "==";
        case NEQ:        return "!=";
        case LE:         return "<=";
        case GE:         return ">=";
        case LT:         return "<";
        case GT:         return ">";
        case ASSIGN:     return "=";
        case PLUS:       return "+";
        case MINUS:      return "-";
        case MULT:       return "*";
        case DIV:        return "/";
        case MOD:        return "%";
        case AND:        return "&&";
        case OR:         return "||";
        case NOT:        return "!";
        case T_END:      return "конец модуля";
        default:         return "неизвестная лексема";
    }
}

// Является ли тип числовым (int или float)
static bool IsNum(DATA_TYPE t) {
    return t == TYPE_INT || t == TYPE_FLOAT;
}

// Конструктор привязывает сканер, создаёт корень дерева и читает первую лексему
TParser::TParser(TScanner * s, bool semantics, bool interpret, bool trace)
    : sc(s), semOn(semantics), interpOn(interpret), treeTrace(trace),
      exec(true), scope("global"), memberPrivate(false) {
    root = new Tree(NULL, NULL, NULL, new SemNode("", TYPE_SCOPE, 1, 1));
    if (semOn)
        Tree::Cur = root;
    next();
}

// Деструктор: удаляет семантическое дерево целиком
TParser::~TParser() {
    delete root;
}

// Вспомогательные функции

// Перейти к следующей лексеме
void TParser::next() {
    t = sc->Scanner(lex);
}

// Тип лексемы через k позиций вперёд, состояние сканера после просмотра восстанавливается
int TParser::lookAhead(int k) {
    int savePos  = sc->getPos();
    int saveLine = sc->getLine();
    int saveTok  = sc->getTokPos();
    int saveTokL = sc->getTokLine();

    int type = T_END;
    LEX l;
    for (int i = 0; i < k; i++) {
        type = sc->Scanner(l);
        if (type == T_END) break;
    }

    sc->setPos(savePos);
    sc->setLine(saveLine);
    sc->setTokPos(saveTok);
    sc->setTokLine(saveTokL);
    return type;
}

// Проверить тип текущей лексемы и пропустить её
void TParser::match(int expected, const char * what) {
    if (t != expected) {
        char buf[256];
        snprintf(buf, sizeof(buf), "ожидалось: %s", what);
        error(buf);
    }
    next();
}

// Сообщение о синтаксической ошибке
void TParser::error(const char * msg) {
    std::string full = std::string(msg) + ", а встретилось: " + TokenName(t);
    if (t != T_END && lex[0] != '\0')
        full += std::string(" \"") + lex + "\"";
    throw TSynError(full, sc->getTokLine(), sc->getCol());
}

// Текстовое имя типа для сообщений семантических ошибок
std::string TParser::TypeStr(const TypeInfo & ti) {
    if (ti.type == TYPE_OBJECT)
        return std::string("объект класса ") + (ti.cls ? ti.cls->id : "?");
    return TypeName(ti.type);
}

// Начинается ли лексема одним из типов int|float|bool|IDENT
bool TParser::isTypeStart() {
    return t == KW_INT || t == KW_FLOAT || t == KW_BOOL || t == IDENT;
}

// Является ли текущая позиция началом объявления переменной, внутри блока тип может быть именем класса
bool TParser::isVarDeclStart() {
    if (t == KW_INT || t == KW_FLOAT || t == KW_BOOL)
        return true;
    if (t == IDENT)
        return lookAhead(1) == IDENT;
    return false;
}

// 1) программа - аксиома грамматики: Program -> TopDecl* до конца модуля
void TParser::Program() {
    while (t != T_END)
        TopDecl();
}

// 2) Описания (на верхнем уровне) - TopDecl: TopDecl -> ClassDecl | VarDecl | MainFunc
void TParser::TopDecl() {
    if (t == KW_CLASS)
        ClassDecl();
    else if (t == KW_VOID)
        MainFunc();
    else if (isTypeStart())
        VarDecl();
    else
        error("ожидалось описание верхнего уровня (class, тип или void main)");
}

// 3) Класс - ClassDecl: ClassDecl -> class IDENT { ClassMemberList }
void TParser::ClassDecl() {
    match(KW_CLASS, "class");
    std::string clsName = lex;
    int l = sc->getTokLine();
    int c = sc->getCol();
    match(IDENT, "имя класса");

    SemNode * clsRec = NULL;
    Tree    * outer  = semOn ? Tree::Cur : NULL;
    memberPrivate    = false;      // по умолчанию члены класса открыты

    if (semOn) {
        // имя класса заносится во внешнюю область видимости
        Tree * clsTree = Tree::Cur->SemInclude(clsName, TYPE_CLASS, l, c);
        clsRec = clsTree->n;
        // члены класса образуют собственную область видимости
        Tree::Cur = clsTree;
    }

    // объявления класса не выполняются: интерпретация полей и методов отключена
    bool savedExec = exec;
    exec = false;
    match(LBRACE, "{");
    ClassMemberList(clsRec, clsName);
    match(RBRACE, "}");
    exec = savedExec;

    if (semOn)
        Tree::Cur = outer;
}

// 4) Список членов класса - ClassMemberList: ClassMemberList -> ClassMember*
void TParser::ClassMemberList(SemNode * clsRec, const std::string & clsName) {
    while (t != RBRACE && t != T_END)
        ClassMember(clsRec, clsName);
}

// 5) член класса - ClassMember: AccessSpec | VarDecl | void IDENT ( ) Block | IDENT ( ) Block | ~ IDENT ( ) Block
void TParser::ClassMember(SemNode * clsRec, const std::string & clsName) {
    if (t == KW_PUBLIC || t == KW_PRIVATE) {
        AccessSpec();
    }
    else if (t == KW_VOID) {            // Метод: void IDENT ( ) Block
        next();
        std::string name = lex;
        int l = sc->getTokLine();
        int c = sc->getCol();
        if (semOn) {
            Tree * m = Tree::Cur->SemInclude(name, TYPE_FUNCT, l, c);
            m->n->ref       = clsRec;
            m->n->isPrivate = memberPrivate;
        }
        match(IDENT,  "имя метода");
        match(LPAREN, "(");
        match(RPAREN, ")");
        Block();
    }
    else if (t == TILDE) {              // Деструктор: ~ IDENT ( ) Block
        next();
        std::string name = lex;
        int l = sc->getTokLine();
        int c = sc->getCol();
        if (semOn) {
            if (name != clsName) {
                throw TSemError("имя деструктора \"" + name +
                             "\" не совпадает с именем класса \"" +
                             clsName + "\"", l, c);
            }
            Tree * m = Tree::Cur->SemInclude("~" + name, TYPE_FUNCT, l, c);
            m->n->ref       = clsRec;
            m->n->isPrivate = memberPrivate;
        }
        match(IDENT,  "имя деструктора");
        match(LPAREN, "(");
        match(RPAREN, ")");
        Block();
    }
    else if (t == KW_INT || t == KW_FLOAT || t == KW_BOOL) {
        VarDecl(true);                  // Поле - объявление переменной
    }
    else if (t == IDENT) {
        if (lookAhead(1) == LPAREN) {   // Конструктор: IDENT ( ) Block
            std::string name = lex;
            int l = sc->getTokLine();
            int c = sc->getCol();
            if (semOn) {
                if (name != clsName) {
                    throw TSemError("имя конструктора \"" + name +
                                 "\" не совпадает с именем класса \"" +
                                 clsName + "\"", l, c);
                }
                Tree * m = Tree::Cur->SemInclude(name, TYPE_FUNCT, l, c);
                m->n->ref       = clsRec;
                m->n->isPrivate = memberPrivate;
            }
            match(IDENT,  "имя конструктора");
            match(LPAREN, "(");
            match(RPAREN, ")");
            Block();
        }
        else {
            VarDecl(true);              // Поле типа IDENT
        }
    }
    else {
        error("ожидался член класса");
    }
}

// 6) спецификатор доступа - AccessSpec: public : или private :, режим действует на последующие члены
void TParser::AccessSpec() {
    if (t == KW_PUBLIC)
        memberPrivate = false;
    else if (t == KW_PRIVATE)
        memberPrivate = true;
    else
        error("ожидался спецификатор доступа public или private");
    next();
    match(COLON, ":");
}

// 7) Объявление переменной - VarDecl: VarDecl -> Type IdInitList ;
void TParser::VarDecl(bool classMember) {
    TypeInfo ti = Type();
    IdInitList(ti, classMember);
    match(SEMI, ";");
}

// 8) Тип - Type: Type -> int | float | bool | IDENT
TypeInfo TParser::Type() {
    if (t == KW_INT)   { next(); return TypeInfo(TYPE_INT);   }
    if (t == KW_FLOAT) { next(); return TypeInfo(TYPE_FLOAT); }
    if (t == KW_BOOL)  { next(); return TypeInfo(TYPE_BOOL);  }

    if (t == IDENT) {
        // тип задан именем класса: класс должен быть объявлен
        TypeInfo ti(TYPE_OBJECT);
        if (semOn)
            ti.cls = Tree::Cur->SemGetClass(lex, sc->getTokLine(),
                                            sc->getCol())->n;
        next();
        return ti;
    }

    error("ожидался тип (int, float, bool или имя класса)");
}

// 9) Главная функция - MainFunc: MainFunc -> void main ( ) Block
void TParser::MainFunc() {
    match(KW_VOID, "void");
    std::string name = lex;
    int l = sc->getTokLine();
    int c = sc->getCol();
    // повторное объявление main контролируется однократностью top-level описаний
    match(KW_MAIN, "main");
    if (semOn)
        Tree::Cur->SemInclude(name, TYPE_FUNCT, l, c);
    match(LPAREN,  "(");
    match(RPAREN,  ")");
    // тело главной функции выполняется в области main
    const char * savedScope = scope;
    scope = "main";
    Block();
    scope = savedScope;
}

// 10) Блок - Block: Block -> { (Stmt | VarDecl)* }
void TParser::Block() {
    int l = sc->getTokLine();
    int c = sc->getCol();

    // дерево до входа в область видимости: блока ещё нет
    if (interpOn && exec && treeTrace && TInterpreter::out != NULL) {
        fprintf(TInterpreter::out, "До входа в блок (строка %d):\n", l);
        Tree::Cur->PrintTrace(TInterpreter::out);
    }

    if (semOn)
        Tree::Cur->SemEnterBlock(l, c);

    // дерево сразу при входе в блок: появилась новая вершина-область
    if (interpOn && exec && treeTrace && TInterpreter::out != NULL) {
        fprintf(TInterpreter::out, "При входе в блок (строка %d):\n", l);
        Tree::Cur->PrintTrace(TInterpreter::out);
    }

    match(LBRACE, "{");
    while (t != RBRACE && t != T_END) {
        if (isVarDeclStart())
            VarDecl();
        else
            Stmt();
    }

    // дерево перед выходом из блока: в блоке уже есть объявленные имена
    if (interpOn && exec && treeTrace && TInterpreter::out != NULL) {
        fprintf(TInterpreter::out,
                "Перед выходом из блока (строка %d):\n",
                sc->getTokLine());
        Tree::Cur->PrintTrace(TInterpreter::out);
    }

    // в конце тела главной функции печатаются значения переменных
    if (interpOn && exec && scope != NULL && strcmp(scope, "main") == 0 &&
        Tree::Cur->Up != NULL && Tree::Cur->Up->Up == NULL &&
        TInterpreter::out != NULL) {
        Tree * root = Tree::Cur;
        while (root->Up != NULL)
            root = root->Up;
        fprintf(TInterpreter::out, "Значения переменных:\n");
        TInterpreter::PrintValues(root, TInterpreter::out);
    }

    match(RBRACE, "}");

    // при выходе из области видимости записи блока удаляются из дерева
    if (semOn)
        Tree::Cur->SemExitBlock(interpOn);

    // дерево после выхода из блока: часть дерева уже удалена
    if (interpOn && exec && treeTrace && TInterpreter::out != NULL) {
        fprintf(TInterpreter::out,
                "После выхода из блока (строка %d):\n",
                sc->getTokLine());
        Tree::Cur->PrintTrace(TInterpreter::out);
    }
}

// 11) Оператор - Stmt: Stmt -> ; | Block | if ( Expr ) Stmt ElseOpt | LValue StmtTail
void TParser::Stmt() {
    if (t == SEMI) {
        next();                        // Пустой оператор
    }
    else if (t == LBRACE) {
        Block();                       // Составной оператор
    }
    else if (t == KW_IF) {
        next();
        match(LPAREN, "(");
        int l = sc->getTokLine();
        int c = sc->getCol();
        TypeInfo cond = Expr();
        Value condValue = lastValue;
        if (semOn && cond.type != TYPE_BOOL)
            throw TSemError("условие оператора if должно иметь тип bool, "
                         "а получен " + TypeStr(cond), l, c);

        // выбираем выполняемую ветвь: другая разбирается с exec = false
        bool taken = false;
        if (interpOn && exec) {
            if (!condValue.has)
                throw TRunError("значение условия if не вычислено", l, c);
            taken = condValue.b;
        }
        bool savedExec = exec;
        exec = interpOn ? (savedExec && taken) : savedExec;

        match(RPAREN, ")");
        Stmt();

        exec = interpOn ? (savedExec && !taken) : savedExec;
        ElseOpt();

        exec = savedExec;
    }
    else if (t == IDENT) {
        LValueInfo lv = LValue();
        StmtTail(lv);
    }
    else {
        error("ожидался оператор");
    }
}

// 12) Окончание оператора - StmtTail: StmtTail -> = Expr ; | . IDENT ( ) ;
void TParser::StmtTail(const LValueInfo & lv) {
    if (t == ASSIGN) {                 // Присваивание
        if (semOn && !lv.assignable)
            throw TSemError("слева от \"=\" должна быть переменная "
                         "или поле класса", lv.line, lv.col);
        next();
        int l = sc->getTokLine();
        int c = sc->getCol();
        TypeInfo rt = Expr();
        Value rv = lastValue;
        if (semOn)
            CheckAssign(lv.t, rt, l, c, "присваивании");

        // выполнение присваивания: только для простых переменных
        if (interpOn && exec) {
            if (!lv.simple)
                throw TRunError("присваивание полям классов не поддерживается "
                                "интерпретатором", lv.line, lv.col);
            if (lv.t.type == TYPE_OBJECT)
                throw TRunError("интерпретация объектов не поддерживается",
                                lv.line, lv.col);
            TInterpreter::Assign(lv.obj, rv, lv.line, lv.col, scope);
        }
        match(SEMI, ";");
    }
    else if (t == DOT) {               // Вызов метода: LValue . IDENT ( ) ;
        next();
        std::string name = lex;
        int l = sc->getTokLine();
        int c = sc->getCol();
        // вызов методов при интерпретации не поддерживается
        if (interpOn && exec)
            throw TRunError("вызов методов не поддерживается интерпретатором",
                            l, c);
        if (semOn) {
            Tree * m = Tree::Cur->SemFindMember(lv.obj, name, l, c);
            if (m->n->type != TYPE_FUNCT)
                throw TSemError("\"" + name + "\" - не метод класса \""
                             + lv.obj->ref->id + "\"", l, c);
        }
        match(IDENT,  "имя метода");
        match(LPAREN, "(");
        match(RPAREN, ")");
        match(SEMI,   ";");
    }
    else {
        error("ожидался знак '=' или вызов метода");
    }
}

// 13) Ветвь else - ElseOpt: ElseOpt -> else Stmt | eps
void TParser::ElseOpt() {
    if (t == KW_ELSE) {
        next();
        Stmt();
    }
}

// 14) левое выражение - LValue: IDENT и цепочка . IDENT, а . IDENT ( относится к вызову метода
LValueInfo TParser::LValue() {
    LValueInfo info;
    info.line = sc->getTokLine();
    info.col  = sc->getCol();

    if (semOn) {
        // переменная должна быть объявлена; имя класса или метода - ошибка
        Tree * v = Tree::Cur->SemGetVar(lex, info.line, info.col);
        info.t          = TypeInfo(v->n->type, v->n->ref);
        info.obj        = v->n;
        info.assignable = true;
    }
    else {
        info.t          = TypeInfo(TYPE_INT);
        info.assignable = true;
    }
    match(IDENT, "идентификатор");

    while (t == DOT) {
        // ". IDENT (" - это вызов метода, его обрабатывает StmtTail
        if (lookAhead(1) == IDENT && lookAhead(2) == LPAREN) {
            if (semOn && info.t.type != TYPE_OBJECT)
                throw TSemError("вызов метода возможен только у объекта "
                             "класса, а не у значения типа " + TypeStr(info.t),
                             sc->getTokLine(), sc->getCol());
            info.assignable = false;
            info.simple     = false;
            info.t          = TypeInfo(TYPE_VOID);
            break;
        }

        next();                        // '.'
        std::string field = lex;
        int l = sc->getTokLine();
        int c = sc->getCol();

        if (semOn) {
            // поле ищется в записи класса объекта
            Tree * m = Tree::Cur->SemFindMember(info.obj, field, l, c);
            if (m->n->type == TYPE_FUNCT)
                throw TSemError("\"" + field + "\" - метод класса, "
                             "для вызова нужны круглые скобки", l, c);
            info.t          = TypeInfo(m->n->type, m->n->ref);
            info.obj        = m->n;
            info.assignable = true;
            info.simple     = false;
        }
        match(IDENT, "идентификатор поля");
    }
    return info;
}

// 15) Выражение - Expr: Expr -> LOr ( || LOr )*
TypeInfo TParser::Expr() {
    TypeInfo a = LOr();
    Value va = lastValue;
    // если левый операнд уже истина, остальные операнды не вычисляются
    bool shorted = (interpOn && exec && va.has && va.type == TYPE_BOOL && va.b);
    while (t == OR) {
        int l = sc->getTokLine();
        int c = sc->getCol();
        std::string op = lex;
        next();
        bool savedExec = exec;
        if (shorted)
            exec = false;          // правая часть не вычисляется
        TypeInfo b = LOr();
        Value vb = lastValue;
        exec = savedExec;
        if (semOn)
            a = BinaryBool(a, b, l, c, op);
        else
            a = TypeInfo(TYPE_BOOL);
        if (interpOn && exec) {
            va = shorted ? Value::Bool(true)
                         : TInterpreter::Logic(op, va, vb, l, c);
            if (va.has && va.b)
                shorted = true;
        }
        else {
            va = Value();
        }
    }
    lastValue = va;
    return a;
}

// 16) Логическое ИЛИ - LOr: LOr -> LAnd ( && LAnd )*
TypeInfo TParser::LOr() {
    TypeInfo a = LAnd();
    Value va = lastValue;
    // если левый операнд уже ложь, остальные операнды не вычисляются
    bool shorted = (interpOn && exec && va.has && va.type == TYPE_BOOL && !va.b);
    while (t == AND) {
        int l = sc->getTokLine();
        int c = sc->getCol();
        std::string op = lex;
        next();
        bool savedExec = exec;
        if (shorted)
            exec = false;
        TypeInfo b = LAnd();
        Value vb = lastValue;
        exec = savedExec;
        if (semOn)
            a = BinaryBool(a, b, l, c, op);
        else
            a = TypeInfo(TYPE_BOOL);
        if (interpOn && exec) {
            va = shorted ? Value::Bool(false)
                         : TInterpreter::Logic(op, va, vb, l, c);
            if (va.has && !va.b)
                shorted = true;
        }
        else {
            va = Value();
        }
    }
    lastValue = va;
    return a;
}

// 17) Логическое И - LAnd: LAnd -> Rel ( ( < | <= | > | >= ) Rel )*
TypeInfo TParser::LAnd() {
    TypeInfo a = Rel();
    Value va = lastValue;
    while (t == LT || t == LE || t == GT || t == GE) {
        int l = sc->getTokLine();
        int c = sc->getCol();
        std::string op = lex;
        next();
        TypeInfo b = Rel();
        Value vb = lastValue;
        if (semOn && (!IsNum(a.type) || !IsNum(b.type)))
            throw TSemError("операция сравнения \"" + op +
                         "\" недопустима для типов " + TypeStr(a) +
                         " и " + TypeStr(b), l, c);
        a = TypeInfo(TYPE_BOOL, NULL, true);   // результат сравнения
        va = (interpOn && exec) ? TInterpreter::Compare(op, va, vb, l, c) : Value();
    }
    lastValue = va;
    return a;
}

// 18) Отношения - Rel: Rel -> Eq ( ( == | != ) Eq )*
TypeInfo TParser::Rel() {
    TypeInfo a = Eq();
    Value va = lastValue;
    while (t == EQ || t == NEQ) {
        int l = sc->getTokLine();
        int c = sc->getCol();
        std::string op = lex;
        next();
        TypeInfo b = Eq();
        Value vb = lastValue;
        if (semOn) {
            bool ok;
            if (a.type == TYPE_OBJECT || b.type == TYPE_OBJECT)
                ok = (a.type == TYPE_OBJECT && b.type == TYPE_OBJECT &&
                      a.cls == b.cls);
            else if (a.type == TYPE_BOOL || b.type == TYPE_BOOL)
                ok = (a.type == TYPE_BOOL && b.type == TYPE_BOOL);
            else
                ok = IsNum(a.type) && IsNum(b.type);

            if (!ok)
                throw TSemError("операция \"" + op + "\" недопустима для "
                             "типов " + TypeStr(a) + " и " + TypeStr(b), l, c);
        }
        a = TypeInfo(TYPE_BOOL, NULL, true);   // результат сравнения
        va = (interpOn && exec) ? TInterpreter::Compare(op, va, vb, l, c) : Value();
    }
    lastValue = va;
    return a;
}

// 19) Равенство - Eq: Eq -> Add ( ( + | - ) Add )*
TypeInfo TParser::Eq() {
    TypeInfo a = Add();
    Value va = lastValue;
    while (t == PLUS || t == MINUS) {
        int l = sc->getTokLine();
        int c = sc->getCol();
        std::string op = lex;
        next();
        TypeInfo b = Add();
        Value vb = lastValue;
        if (semOn)
            a = BinaryNum(a, b, l, c, op);
        else
            a = TypeInfo((a.type == TYPE_FLOAT || b.type == TYPE_FLOAT)
                         ? TYPE_FLOAT : TYPE_INT);
        va = (interpOn && exec) ? TInterpreter::Arith(op, va, vb, l, c) : Value();
    }
    lastValue = va;
    return a;
}

// 20) Аддитивные - Add: Add -> Mul ( ( * | / | % ) Mul )*
TypeInfo TParser::Add() {
    TypeInfo a = Mul();
    Value va = lastValue;
    while (t == MULT || t == DIV || t == MOD) {
        int code = t;
        int l = sc->getTokLine();
        int c = sc->getCol();
        std::string op = lex;
        next();
        TypeInfo b = Mul();
        Value vb = lastValue;
        if (semOn) {
            a = BinaryNum(a, b, l, c, op);
            if (code == MOD && (a.type != TYPE_INT))
                throw TSemError("операция \"%\" допустима только для "
                             "целых операндов", l, c);
        }
        else {
            a = TypeInfo((a.type == TYPE_FLOAT || b.type == TYPE_FLOAT)
                         ? TYPE_FLOAT : TYPE_INT);
        }
        va = (interpOn && exec) ? TInterpreter::Arith(op, va, vb, l, c) : Value();
    }
    lastValue = va;
    return a;
}

// 21) Унарные - Mul: Mul -> + Prim | - Prim | ! Prim | Prim
TypeInfo TParser::Mul() {
    if (t == PLUS || t == MINUS) {
        int l = sc->getTokLine();
        int c = sc->getCol();
        std::string op = lex;
        next();
        TypeInfo a = Prim();
        Value va = lastValue;
        if (semOn && !IsNum(a.type))
            throw TSemError("унарная операция \"" + op + "\" недопустима "
                         "для типа " + TypeStr(a), l, c);
        lastValue = (interpOn && exec) ? TInterpreter::Unary(op, va, l, c) : Value();
        return TypeInfo(a.type == TYPE_FLOAT ? TYPE_FLOAT : TYPE_INT);
    }
    if (t == NOT) {
        int l = sc->getTokLine();
        int c = sc->getCol();
        std::string op = lex;
        next();
        TypeInfo a = Prim();
        Value va = lastValue;
        if (semOn && a.type != TYPE_BOOL)
            throw TSemError("логическое отрицание \"!\" допустимо только "
                         "для типа bool, а не для " + TypeStr(a), l, c);
        lastValue = (interpOn && exec) ? TInterpreter::Unary(op, va, l, c) : Value();
        return TypeInfo(TYPE_BOOL);
    }
    return Prim();
}

// 22) Элементарное выражение - Prim: Prim -> LValue | Const | ( Expr )
TypeInfo TParser::Prim() {
    if (t == IDENT) {
        LValueInfo lv = LValue();
        if (interpOn && exec) {
            // интерпретируются только простые переменные без полей классов
            if (!lv.simple)
                throw TRunError("обращение к полям классов не поддерживается "
                                "интерпретатором", lv.line, lv.col);
            if (lv.t.type == TYPE_OBJECT)
                throw TRunError("интерпретация объектов не поддерживается",
                                lv.line, lv.col);
            lastValue = TInterpreter::Load(lv.obj, lv.line, lv.col);
        }
        else {
            lastValue = Value();
        }
        return lv.t;
    }
    else if (t == DEC_CONST || t == REAL_CONST || t == BOOL_CONST) {
        return Const();
    }
    else if (t == LPAREN) {
        next();
        TypeInfo ti = Expr();
        match(RPAREN, ")");
        return ti;                  // значение выражения уже в lastValue
    }
    else {
        error("ожидалось элементарное выражение");
    }
}

// 23) Константы - Const: Const -> DEC_CONST | REAL_CONST | BOOL_CONST
TypeInfo TParser::Const() {
    if (t == DEC_CONST) {
        if (interpOn && exec) lastValue = Value::Int(atoi(lex));
        else                  lastValue = Value();
        next();
        return TypeInfo(TYPE_INT);
    }
    if (t == REAL_CONST) {
        if (interpOn && exec) lastValue = Value::Float((float)atof(lex));
        else                  lastValue = Value();
        next();
        return TypeInfo(TYPE_FLOAT);
    }
    if (t == BOOL_CONST) {
        if (interpOn && exec) lastValue = Value::Bool(strcmp(lex, "true") == 0);
        else                  lastValue = Value();
        next();
        return TypeInfo(TYPE_BOOL);
    }
    error("ожидалась константа");
}

// Вспомогательные правила

// IdInitList -> IdInit ( , IdInit )*
void TParser::IdInitList(const TypeInfo & ti, bool classMember) {
    IdInit(ti, classMember);
    while (t == COMMA) {
        next();
        IdInit(ti, classMember);
    }
}

// IdInit -> IDENT InitOpt,  InitOpt -> = Expr | eps
void TParser::IdInit(const TypeInfo & ti, bool classMember) {
    std::string name = lex;
    int l = sc->getTokLine();
    int c = sc->getCol();
    match(IDENT, "имя переменной");

    bool     hasInit = false;
    TypeInfo initType;
    Value    initValue;
    if (t == ASSIGN) {
        next();
        initType  = Expr();
        initValue = lastValue;
        hasInit   = true;
    }

    if (semOn) {
        // Имя переменной не должно совпадать с видимым классом: FindUp ищет во всех областях
        Tree * existing = Tree::Cur->FindUp(Tree::Cur, name);
        if (existing != NULL && existing->n->type == TYPE_CLASS) {
            char buf[512];
            snprintf(buf, sizeof(buf),
                     "идентификатор \"%s\" уже объявлен как класс "
                     "(строка %d, позиция %d) и не может быть использован "
                     "как имя переменной",
                     name.c_str(), existing->n->line, existing->n->col);
            throw TSemError(buf, l, c);
        }

        // идентификатор становится видимым только после своего описания
        Tree * v = Tree::Cur->SemInclude(name, ti.type, l, c);
        v->n->ref       = ti.cls;
        v->n->isPrivate = classMember && memberPrivate;
        if (hasInit) {
            CheckAssign(ti, initType, l, c,
                        std::string("инициализации переменной \"") +
                        name + "\"");
            // выполнение инициализации при интерпретации
            if (interpOn && exec) {
                if (ti.type == TYPE_OBJECT)
                    throw TRunError("интерпретация объектов не поддерживается",
                                    l, c);
                TInterpreter::Assign(v->n, initValue, l, c, scope);
            }
        }
    }
}

// Семантические подпрограммы контроля контекстных условий

// Тип результата арифметической операции: операнды числовые, результат float при хотя бы одном float
TypeInfo TParser::BinaryNum(const TypeInfo & a, const TypeInfo & b,
                            int line, int col, const std::string & op) {
    if (!IsNum(a.type) || !IsNum(b.type))
        throw TSemError("операция \"" + op + "\" недопустима для типов " +
                     TypeStr(a) + " и " + TypeStr(b), line, col);
    return TypeInfo((a.type == TYPE_FLOAT || b.type == TYPE_FLOAT)
                    ? TYPE_FLOAT : TYPE_INT);
}

// Тип результата логической операции: оба операнда должны быть bool
TypeInfo TParser::BinaryBool(const TypeInfo & a, const TypeInfo & b,
                             int line, int col, const std::string & op) {
    if (a.type != TYPE_BOOL || b.type != TYPE_BOOL)
        throw TSemError("операция \"" + op + "\" недопустима для типов " +
                     TypeStr(a) + " и " + TypeStr(b), line, col);
    return TypeInfo(TYPE_BOOL);
}

// Проверка совместимости типов в операторе присваивания (инициализации)
void TParser::CheckAssign(const TypeInfo & dst, const TypeInfo & src,
                          int line, int col, const std::string & ctx) {
    if (src.type == TYPE_VOID)
        throw TSemError("нельзя использовать значение типа void в " + ctx,
                     line, col);
    // результат сравнения (bool) можно присвоить числовой переменной:
    // это значение 0/1, как в C
    if (IsNum(dst.type) && src.type == TYPE_BOOL && src.fromComparison)
        return;
    if (!TypesCompatible(dst.type, dst.cls, src.type, src.cls))
        throw TSemError("несовместимые типы в " + ctx + ": " +
                     TypeStr(dst) + " = " + TypeStr(src), line, col);
}