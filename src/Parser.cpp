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
      exec(true), scope("global"), curObj(NULL), fileName(s->GetFileName()),
      memberPrivate(false) {
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

    // после завершения программы удаляются глобальные объекты (вызываются деструкторы)
    if (interpOn && exec) {
        Tree * top = Tree::Cur;
        while (top != NULL && top->Up != NULL)
            top = top->Up;
        DestroyScopeObjects(top);
    }
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
        return;
    }

    Tree * mNode = NULL;

    if (t == KW_VOID) {                 // Метод: void IDENT ( ) Block
        next();
        std::string name = lex;
        int l = sc->getTokLine();
        int c = sc->getCol();
        if (semOn) {
            mNode = Tree::Cur->SemInclude(name, TYPE_FUNCT, l, c);
            mNode->n->ref       = clsRec;
            mNode->n->isPrivate = memberPrivate;
        }
        match(IDENT,  "имя метода");
        match(LPAREN, "(");
        match(RPAREN, ")");
        int bodyPos  = sc->getTokPos();
        int bodyLine = sc->getTokLine();
        if (mNode != NULL) {
            mNode->n->bodyPos  = bodyPos;
            mNode->n->bodyLine = bodyLine;
        }
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
            mNode = Tree::Cur->SemInclude("~" + name, TYPE_FUNCT, l, c);
            mNode->n->ref       = clsRec;
            mNode->n->isPrivate = memberPrivate;
        }
        match(IDENT,  "имя деструктора");
        match(LPAREN, "(");
        match(RPAREN, ")");
        int bodyPos  = sc->getTokPos();
        int bodyLine = sc->getTokLine();
        if (mNode != NULL) {
            mNode->n->bodyPos  = bodyPos;
            mNode->n->bodyLine = bodyLine;
        }
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
                mNode = Tree::Cur->SemInclude(name, TYPE_FUNCT, l, c);
                mNode->n->ref       = clsRec;
                mNode->n->isPrivate = memberPrivate;
            }
            match(IDENT,  "имя конструктора");
            match(LPAREN, "(");
            match(RPAREN, ")");
            int bodyPos  = sc->getTokPos();
            int bodyLine = sc->getTokLine();
            if (mNode != NULL) {
                mNode->n->bodyPos  = bodyPos;
                mNode->n->bodyLine = bodyLine;
            }
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
    }

    match(RBRACE, "}");

    // объекты, объявленные в блоке, удаляются при выходе из области видимости
    if (interpOn && exec)
        DestroyScopeObjects(Tree::Cur);

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
        if (lookAhead(1) == LPAREN) {
            // вызов метода текущего объекта без указания объекта
            CallOwnMethod();
        }
        else {
            LValueInfo lv = LValue();
            StmtTail(lv);
        }
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

        if (interpOn && exec) {
            if (lv.t.type == TYPE_OBJECT) {
                // объект копируется по полям
                TObject * dst = NULL;
                if (lv.simple) {
                    if (lv.obj->val.has)
                        dst = lv.obj->val.obj;
                }
                else if (lv.robj != NULL) {
                    Value * fv = lv.robj->Find(lv.obj);
                    if (fv != NULL && fv->has)
                        dst = fv->obj;
                }
                if (rv.type != TYPE_OBJECT || rv.obj == NULL)
                    throw TRunError("объекту можно присвоить только объект",
                                    lv.line, lv.col);
                std::string srcName = rv.obj->name.empty()
                                      ? std::string("объект") : rv.obj->name;
                TInterpreter::TraceAssign(lv.path, srcName, rv.obj->clsName,
                                          lv.line, lv.col, scope);
                CopyObject(dst, rv.obj, lv.line, lv.col);
            }
            else if (!lv.simple) {
                TInterpreter::AssignField(lv.robj, lv.obj, rv,
                                          lv.line, lv.col, scope, lv.path);
            }
            else {
                TInterpreter::Assign(lv.obj, rv, lv.line, lv.col, scope);
            }
        }
        match(SEMI, ";");
    }
    else if (t == DOT) {               // Вызов метода: LValue . IDENT ( ) ;
        next();
        std::string name = lex;
        int l = sc->getTokLine();
        int c = sc->getCol();
        if (semOn) {
            Tree * m = Tree::Cur->SemFindMember(lv.obj, name, l, c);
            if (m->n->type != TYPE_FUNCT)
                throw TSemError("\"" + name + "\" - не метод класса \""
                             + lv.obj->ref->id + "\"", l, c);
            if (interpOn && exec) {
                if (lv.receiver == NULL)
                    throw TRunError("метод \"" + name + "\" вызывается у "
                                    "объекта, который не создан", l, c);
                RunMethod(m->n, lv.receiver, l, c);
            }
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

// Вызов метода текущего объекта: IDENT ( ) ;
void TParser::CallOwnMethod() {
    std::string name = lex;
    int l = sc->getTokLine();
    int c = sc->getCol();

    if (semOn) {
        SemNode * cls = Tree::Cur->EnclosingClass();
        if (cls == NULL)
            throw TSemError("вызов метода без объекта возможен только "
                            "внутри метода класса", l, c);

        SemNode * m = FindClassMember(cls, name, TYPE_FUNCT);
        if (m == NULL)
            throw TSemError("в классе \"" + cls->id + "\" нет метода \"" +
                            name + "\"", l, c);

        if (interpOn && exec) {
            if (curObj == NULL)
                throw TRunError("вызов метода без объекта вне выполняемого "
                                "метода", l, c);
            RunMethod(m, curObj, l, c);
        }
    }

    match(IDENT,  "имя метода");
    match(LPAREN, "(");
    match(RPAREN, ")");
    match(SEMI,   ";");
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
        info.path       = v->n->id;
        info.assignable = true;

        if (interpOn && curObj != NULL && v->n->self != NULL &&
            v->n->self->Up == curObj->clsTree) {
            // поле текущего объекта: значение хранится в объекте
            info.simple = false;
            info.robj   = curObj;
            if (v->n->type == TYPE_OBJECT) {
                Value * fv = curObj->Find(v->n);
                if (fv != NULL && fv->has && fv->obj != NULL)
                    info.receiver = fv->obj;
            }
        }
        else if (v->n->type == TYPE_OBJECT && v->n->val.has &&
                 v->n->val.obj != NULL) {
            info.receiver = v->n->val.obj;   // объект для вызова методов и полей
        }
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

            info.path += "." + field;

            // объект, в котором хранится поле
            TObject * container = (info.t.type == TYPE_OBJECT)
                                  ? info.receiver : info.robj;
            if (interpOn && container != NULL) {
                if (m->n->type == TYPE_OBJECT) {
                    Value * fv = container->Find(m->n);
                    if (fv == NULL || !fv->has || fv->obj == NULL)
                        throw TRunError("объект-поле \"" + field +
                                        "\" не создан", l, c);
                    info.receiver = fv->obj;
                }
                else {
                    info.receiver = NULL;
                }
                info.robj = container;
            }

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
            if (!lv.simple)
                lastValue = TInterpreter::LoadField(lv.robj, lv.obj,
                                                    lv.line, lv.col);
            else
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
        // инициализатор поля вычисляется сразу: значение хранится как значение по умолчанию
        bool evalField = classMember && interpOn && !exec;
        if (evalField)
            exec = true;
        initType  = Expr();
        initValue = lastValue;
        if (evalField)
            exec = false;
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
        }

        // поле не может быть объектом своего же класса (бесконечное вложение)
        if (classMember && ti.type == TYPE_OBJECT && ti.cls != NULL &&
            Tree::Cur->EnclosingClass() == ti.cls)
            throw TSemError("класс \"" + ti.cls->id + "\" не может содержать "
                            "поле своего типа", l, c);

        // контроль доступа к конструктору при создании объекта
        if (ti.type == TYPE_OBJECT && ti.cls != NULL) {
            SemNode * ctor = FindCtor(ti.cls);
            if (ctor != NULL && ctor->isPrivate && Tree::Cur->EnclosingClass() != ti.cls)
                throw TSemError("конструктор класса \"" + ti.cls->id +
                             "\" объявлен как private и недоступен вне класса",
                             l, c);
        }

        if (classMember) {
            // значение инициализатора поля запоминается для создания объектов
            if (hasInit && ti.type == TYPE_OBJECT)
                throw TSemError("инициализация поля-объекта не поддерживается",
                                l, c);
            if (interpOn && hasInit)
                v->n->val = TInterpreter::Convert(initValue, ti.type, l, c);
        }
        else if (interpOn && exec) {
            if (ti.type == TYPE_OBJECT) {
                TObject * o = CreateObject(ti.cls, l, c, name);
                v->n->val = Value::Object(o);
                if (hasInit) {
                    if (initValue.type == TYPE_OBJECT && initValue.obj != NULL)
                        CopyObject(o, initValue.obj, l, c);
                    else
                        throw TRunError("объект можно инициализировать только "
                                        "объектом того же класса", l, c);
                }
            }
            else if (hasInit) {
                TInterpreter::Assign(v->n, initValue, l, c, scope);
            }
        }
    }
}

// Поиск члена класса по имени и типу (поле, метод, конструктор)
SemNode * TParser::FindClassMember(SemNode * cls, const std::string & name,
                                   DATA_TYPE type) {
    if (cls == NULL || cls->self == NULL)
        return NULL;
    for (Tree * p = cls->self->Left; p != NULL; p = p->Right)
        if (p->n != NULL && p->n->type == type && p->n->id == name)
            return p->n;
    return NULL;
}

// Конструктор класса (имя совпадает с именем класса) или NULL
SemNode * TParser::FindCtor(SemNode * cls) {
    if (cls == NULL)
        return NULL;
    return FindClassMember(cls, cls->id, TYPE_FUNCT);
}

// Деструктор класса (~Имя) или NULL
SemNode * TParser::FindDtor(SemNode * cls) {
    if (cls == NULL)
        return NULL;
    return FindClassMember(cls, "~" + cls->id, TYPE_FUNCT);
}

// Создание объекта: поля с значениями по умолчанию, вложенные объекты, конструктор
TObject * TParser::CreateObject(SemNode * cls, int line, int col,
                                 const std::string & name) {
    if (cls == NULL || cls->self == NULL)
        throw TRunError("неизвестный класс объекта", line, col);

    TObject * o = new TObject();
    o->clsName = cls->id;
    o->name    = name;
    o->scopeName = cls->id + (name.empty() ? "" : " " + name);
    o->clsTree = cls->self;

    for (Tree * p = cls->self->Left; p != NULL; p = p->Right) {
        SemNode * f = p->n;
        if (f == NULL)
            continue;
        if (f->type != TYPE_INT && f->type != TYPE_FLOAT &&
            f->type != TYPE_BOOL && f->type != TYPE_OBJECT)
            continue;

        Value def;
        if (f->type == TYPE_OBJECT) {
            // поле-объект создаётся до вызова конструктора внешнего класса
            TObject * sub = CreateObject(f->ref, f->line, f->col,
                                         name.empty() ? f->id
                                                      : name + "." + f->id);
            def = Value::Object(sub);
        }
        else if (f->val.has) {
            def = f->val;   // значение из инициализатора поля
        }
        o->fields.push_back(std::make_pair(f, def));
    }

    SemNode * ctor = FindCtor(cls);
    if (ctor != NULL) {
        if (ctor->isPrivate && Tree::Cur->EnclosingClass() != cls) {
            delete o;
            throw TSemError("конструктор класса \"" + cls->id +
                            "\" объявлен как private и недоступен вне класса",
                            line, col);
        }
        RunMethod(ctor, o, line, col);
    }
    return o;
}

// Удаление объекта: деструктор, затем поля-объекты в обратном порядке
void TParser::DestroyObject(TObject * obj) {
    if (obj == NULL)
        return;

    SemNode * dtor = FindDtor(obj->clsTree->n);
    if (dtor != NULL) {
        if (dtor->isPrivate && Tree::Cur->EnclosingClass() != obj->clsTree->n)
            throw TSemError("деструктор класса \"" + obj->clsName +
                            "\" объявлен как private и недоступен вне класса",
                            dtor->line, dtor->col);
        RunMethod(dtor, obj, dtor->line, dtor->col);
    }

    for (int i = (int)obj->fields.size() - 1; i >= 0; i--) {
        Value & v = obj->fields[i].second;
        if (v.type == TYPE_OBJECT && v.has && v.obj != NULL) {
            DestroyObject(v.obj);
            v.obj = NULL;
            v.has = false;
        }
    }
    delete obj;
}

// Копирование объекта по полям
void TParser::CopyObject(TObject * dst, TObject * src, int line, int col) {
    if (dst == NULL || src == NULL)
        throw TRunError("присваивание объекта, который не создан", line, col);
    if (dst->clsName != src->clsName)
        throw TRunError("нельзя присвоить объект класса \"" + src->clsName +
                        "\" объекту класса \"" + dst->clsName + "\"",
                        line, col);

    for (size_t i = 0; i < dst->fields.size(); i++) {
        SemNode * f = dst->fields[i].first;
        Value * sv = src->Find(f);
        if (sv == NULL || !sv->has)
            continue;

        std::string dstPath = dst->name.empty() ? f->id
                                                : dst->name + "." + f->id;
        std::string srcPath = src->name.empty() ? f->id
                                                : src->name + "." + f->id;
        if (f->type == TYPE_OBJECT) {
            if (sv->obj != NULL) {
                TInterpreter::TraceCopy(dstPath, srcPath,
                                        std::string("объект ") + f->ref->id,
                                        "объект", line, col, scope);
                CopyObject(dst->fields[i].second.obj, sv->obj, line, col);
            }
        }
        else {
            dst->fields[i].second = *sv;
            TInterpreter::TraceCopy(dstPath, srcPath, ValueToString(*sv),
                                    TypeName(f->type), line, col, scope);
        }
    }
}

// Удаление объектов, объявленных непосредственно в области видимости
void TParser::DestroyScopeObjects(Tree * scope) {
    if (scope == NULL)
        return;

    std::vector<Tree *> kids;
    for (Tree * p = scope->Left; p != NULL; p = p->Right)
        kids.push_back(p);

    for (int i = (int)kids.size() - 1; i >= 0; i--) {
        SemNode * n = kids[i]->n;
        if (n != NULL && n->type == TYPE_OBJECT && n->val.has &&
            n->val.obj != NULL) {
            DestroyObject(n->val.obj);
            n->val.obj = NULL;
            n->val.has = false;
        }
    }
}

// Выполнение тела метода, конструктора или деструктора для объекта obj
void TParser::RunMethod(SemNode * m, TObject * obj, int line, int col) {
    if (m == NULL || m->bodyPos < 0)
        throw TRunError("тело метода не найдено", line, col);

    // защита от бесконечной рекурсии методов
    static int depth = 0;
    if (depth >= 200)
        throw TRunError("превышена допустимая глубина вызовов методов "
                        "(возможна бесконечная рекурсия)", line, col);
    struct DepthGuard {
        DepthGuard()  { depth++; }
        ~DepthGuard() { depth--; }
    } guard;

    // состояние текущего разбора сохраняется до создания вложенного парсера
    TObject   * savedObj   = curObj;
    Tree      * savedCur   = Tree::Cur;
    const char* savedScope = scope;

    TScanner * ms = new TScanner(fileName.c_str());
    ms->setPos(m->bodyPos);
    ms->setLine(m->bodyLine);
    TParser * mp = new TParser(ms, true, true, false);

    mp->curObj = obj;
    mp->scope  = (obj != NULL) ? obj->scopeName.c_str() : "method";
    Tree::Cur  = (obj != NULL) ? obj->clsTree : savedCur;

    try {
        mp->Block();
    }
    catch (...) {
        delete mp;
        delete ms;
        Tree::Cur = savedCur;
        curObj    = savedObj;
        scope     = savedScope;
        throw;
    }

    // вложенный парсер удаляется до восстановления текущей области:
    // деструктор его корня не должен обнулить Tree::Cur
    delete mp;
    delete ms;

    Tree::Cur = savedCur;
    curObj    = savedObj;
    scope     = savedScope;
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