// Автотесты семантики варианта 9: условия читаются из tests/sem_programs.txt, дерево проверяется напрямую
#include "defs.h"
#include "Scanner.h"
#include "Parser.h"
#include "SemTree.h"
#include <string>
#include <vector>
#include <fstream>
#include <cstdio>
#include <cstdlib>

// вид ошибки, ожидаемой в тесте
enum ErrorKind { KIND_ANY = 0, KIND_LEX, KIND_SYN, KIND_SEM };

// текстовое название вида ошибки для протокола
static const char * ErrorKindName(ErrorKind k) {
    switch (k) {
        case KIND_LEX: return "лексическая ошибка";
        case KIND_SYN: return "синтаксическая ошибка";
        case KIND_SEM: return "семантическая ошибка";
        default:       return "ошибка";
    }
}

// один тест из файла: программа и ожидаемый результат
struct ProgramCase {
    std::string name;   // название теста
    std::string src;    // текст программы
    bool        valid;  // true - программа должна быть принята
    ErrorKind   kind;   // ожидаемый вид ошибки
    int         line;   // ожидаемая строка ошибки, 0 - не проверять
    int         col;    // ожидаемая позиция ошибки, 0 - не проверять
    std::string part;   // ожидаемая часть сообщения об ошибке

    ProgramCase() : valid(true), kind(KIND_ANY), line(0), col(0) {}
};

// общий счётчик всех проверок
static int testsPassed = 0;
static int testsFailed = 0;
static int testsTotal  = 0;

// отметить успешную проверку
static void Ok(const std::string & name) {
    testsTotal++;
    testsPassed++;
    printf("[OK]   %s\n", name.c_str());
}

// отметить неуспешную проверку
static void Fail(const std::string & name) {
    testsTotal++;
    testsFailed++;
    printf("[FAIL] %s\n", name.c_str());
}

// путь к временному файлу, в который записывается исходный модуль теста
static const char * TmpFile() {
    const char * dir = getenv("TMPDIR");
    static std::string path =
        std::string(dir ? dir : "/tmp") + "/sem_test_input.txt";
    return path.c_str();
}

// путь к файлу, в который направляется печать дерева, чтобы не засорять протокол
static const char * OutFile() {
    const char * dir = getenv("TMPDIR");
    static std::string path =
        std::string(dir ? dir : "/tmp") + "/sem_test_tree.txt";
    return path.c_str();
}

// прочитать текстовый файл целиком
static std::string ReadFile(const char * path) {
    std::ifstream in(path, std::ios::binary);
    std::string text;
    if (in) {
        std::string line;
        while (std::getline(in, line))
            text += line + "\n";
    }
    return text;
}

// убрать пробелы в начале и в конце строки
static std::string Trim(const std::string & s) {
    size_t b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos)
        return "";
    size_t e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

// разобрать текст программы с семантическим контролем, вернуть true если принято
static bool Analyze(const std::string & src, TError & err, ErrorKind & kind) {
    std::ofstream out(TmpFile(), std::ios::binary | std::ios::trunc);
    out << src;
    out.close();

    TScanner * sc = new TScanner(TmpFile());
    TParser  * p  = new TParser(sc, true);

    bool ok = true;
    try {
        p->Program();
    }
    catch (TLexError & e) { err = e; kind = KIND_LEX; ok = false; }
    catch (TSynError & e) { err = e; kind = KIND_SYN; ok = false; }
    catch (TSemError & e) { err = e; kind = KIND_SEM; ok = false; }
    catch (TError & e)    { err = e; kind = KIND_ANY; ok = false; }

    delete p;
    delete sc;
    return ok;
}

// разобрать строку заголовка теста и заполнить поля теста
static bool ParseHeader(const std::string & line, ProgramCase & c) {
    std::vector<std::string> f;
    size_t start = 0;
    while (true) {
        size_t p = line.find('|', start);
        if (p == std::string::npos) {
            f.push_back(Trim(line.substr(start)));
            break;
        }
        f.push_back(Trim(line.substr(start, p - start)));
        start = p + 1;
    }
    if (f.size() < 2)
        return false;

    c.name  = f[0].substr(std::string("// ТЕСТ:").size());
    c.name  = Trim(c.name);
    c.valid = (f[1] == "принято");
    if (c.valid)
        return true;
    if (f.size() < 6)
        return false;

    if (f[2] == "Л")      c.kind = KIND_LEX;
    else if (f[2] == "С") c.kind = KIND_SYN;
    else                  c.kind = KIND_SEM;
    c.line = atoi(f[3].c_str());
    c.col  = atoi(f[4].c_str());
    c.part = f[5];
    return true;
}

// загрузить тесты из файла: заголовок теста и следующие за ним строки программы
static bool LoadCases(const char * path, std::vector<ProgramCase> & cases) {
    std::ifstream in(path);
    if (!in)
        return false;

    std::string line;
    ProgramCase cur;
    bool active = false;
    while (std::getline(in, line)) {
        if (line.rfind("// ТЕСТ:", 0) == 0) {
            if (active)
                cases.push_back(cur);
            cur = ProgramCase();
            if (!ParseHeader(line, cur))
                return false;
            active = true;
        }
        else if (line.rfind("// КОНЕЦ ПРОГРАММЫ", 0) == 0) {
            if (active)
                cases.push_back(cur);
            active = false;
        }
        else if (active) {
            cur.src += line + "\n";
        }
    }
    if (active)
        cases.push_back(cur);
    for (size_t i = 0; i < cases.size(); i++)
        if (!cases[i].src.empty() && cases[i].src[cases[i].src.size() - 1] == '\n')
            cases[i].src.erase(cases[i].src.size() - 1);
    return true;
}

// прогнать все тесты из файла и сравнить результат с ожидаемым
static void RunProgramTests(const char * path) {
    std::vector<ProgramCase> cases;
    if (!LoadCases(path, cases)) {
        printf("не удалось прочитать файл тестов %s\n", path);
        Fail("загрузка файла тестов");
        return;
    }
    if (cases.empty()) {
        Fail("файл тестов пуст");
        return;
    }

    for (size_t i = 0; i < cases.size(); i++) {
        const ProgramCase & c = cases[i];
        TError    err("", 0, 0);
        ErrorKind kind = KIND_ANY;
        bool      got  = Analyze(c.src, err, kind);
        bool      ok   = (got == c.valid);

        if (ok && !c.valid) {
            if (c.kind != KIND_ANY && kind != c.kind)
                ok = false;
            if (c.line > 0 && err.line != c.line)
                ok = false;
            if (c.col > 0 && err.col != c.col)
                ok = false;
            if (!c.part.empty() &&
                err.message.find(c.part) == std::string::npos)
                ok = false;
        }

        if (ok) {
            Ok(c.name);
        }
        else {
            Fail(c.name);
            printf("       программа: <<<%s>>>\n", c.src.c_str());
            printf("       ожидалось: %s", c.valid ? "принять" : "отвергнуть");
            if (!c.valid)
                printf(" (%s, строка %d, позиция %d, \"%s\")",
                       ErrorKindName(c.kind), c.line, c.col, c.part.c_str());
            printf("\n");
            if (got)
                printf("       получено: принято\n");
            else
                printf("       получено: отвергнуто (%s, строка %d, позиция %d): %s\n",
                       ErrorKindName(kind), err.line, err.col,
                       err.message.c_str());
        }
    }
}

// проверка условия с выводом одной строки протокола
static void Check(bool cond, const std::string & name) {
    if (cond)
        Ok(name);
    else
        Fail(name);
}

// проверка, что вызов вызывает исключение, с сохранением места и текста
template <class F>
static bool Throws(F f, int * line = NULL, int * col = NULL,
                   std::string * msg = NULL) {
    try {
        f();
    }
    catch (TError & e) {
        if (line) *line = e.line;
        if (col)  *col  = e.col;
        if (msg)  *msg  = e.message;
        return true;
    }
    return false;
}

// тесты самого семантического дерева, без синтаксического анализа
static void RunTreeTests() {
    // занесение идентификаторов, поиск и контроль повторного объявления
    {
        Tree root(NULL, NULL, NULL, new SemNode("", TYPE_SCOPE, 1, 1));
        Tree::Cur = &root;

        root.SemInclude("x", TYPE_INT, 1, 1);
        root.SemInclude("y", TYPE_FLOAT, 1, 5);

        Tree * x = root.FindUp(&root, "x");
        Check(x != NULL && x->n->type == TYPE_INT, "поиск объявленного имени");
        Check(root.FindUp(&root, "z") == NULL, "поиск необъявленного имени");
        Check(root.DupControl(&root, "x") == 1, "контроль дублирования");
        Check(root.DupControl(&root, "z") == 0, "дублирования нет");
        Check(x->n->line == 1 && x->n->col == 1, "сохранение позиции объявления");

        int l = 0, c = 0;
        std::string m;
        bool thrown = Throws([&]() { root.SemInclude("x", TYPE_BOOL, 3, 9); },
                             &l, &c, &m);
        Check(thrown && l == 3 && c == 9 &&
              m.find("повторное объявление") != std::string::npos,
              "повторное объявление на одном уровне");
    }

    // вход в блок, выход из блока, видимость и перекрытие имён
    {
        Tree root(NULL, NULL, NULL, new SemNode("", TYPE_SCOPE, 1, 1));
        Tree::Cur = &root;
        root.SemInclude("a", TYPE_INT, 1, 1);

        Tree * blk = root.SemEnterBlock(2, 1);
        Check(Tree::Cur == blk && blk->Up == &root, "вход в блок");
        Check(blk->FindUp(blk, "a") != NULL, "видимость внешнего имени в блоке");
        Check(blk->FindUpOneLevel(blk, "a") == NULL,
              "внешнее имя не видно на текущем уровне");

        blk->SemInclude("a", TYPE_FLOAT, 3, 9);
        Tree * inner = blk->FindUp(blk, "a");
        Check(inner != NULL && inner->n->type == TYPE_FLOAT &&
              inner->n->line == 3, "перекрытие имени во внутренней области");

        blk->SemInclude("b", TYPE_BOOL, 4, 9);

        Tree::Cur->SemExitBlock();
        Check(Tree::Cur == &root, "выход из блока");
        Tree * outer = root.FindUp(&root, "a");
        Check(outer != NULL && outer->n->type == TYPE_INT,
              "после выхода из блока снова видно внешнее имя");
        Check(root.FindUp(&root, "b") == NULL, "имя из блока не видно снаружи");
    }

    // поиск переменной и контроль того, что имя не является функцией
    {
        Tree root(NULL, NULL, NULL, new SemNode("", TYPE_SCOPE, 1, 1));
        Tree::Cur = &root;
        root.SemInclude("f", TYPE_FUNCT, 1, 1);

        int l = 0, c = 0;
        std::string m;
        bool thrown = Throws([&]() { root.SemGetVar("f", 2, 3); }, &l, &c, &m);
        Check(thrown && m.find("а не переменная") != std::string::npos,
              "SemGetVar отвергает имя метода");

        thrown = Throws([&]() { root.SemGetVar("z", 4, 7); }, &l, &c, &m);
        Check(thrown && l == 4 && c == 7 &&
              m.find("не объявлен") != std::string::npos,
              "SemGetVar: необъявленное имя с позицией");
    }

    // записи классов, поиск полей и методов, контроль прав доступа
    {
        Tree root(NULL, NULL, NULL, new SemNode("", TYPE_SCOPE, 1, 1));
        Tree::Cur = &root;

        Tree    * cls    = root.SemInclude("Point", TYPE_CLASS, 1, 7);
        SemNode * clsRec = cls->n;

        Tree * outer = Tree::Cur;
        Tree::Cur = cls;
        cls->SemInclude("x", TYPE_INT, 3, 9);
        Tree * priv = cls->SemInclude("visible", TYPE_BOOL, 4, 9);
        priv->n->isPrivate = true;
        Tree * m = cls->SemInclude("move", TYPE_FUNCT, 5, 10);
        m->n->ref = clsRec;
        Tree::Cur = outer;

        Check(root.SemGetClass("Point", 8, 5) != NULL, "поиск класса");
        int l = 0, c = 0;
        std::string msg;
        bool thrown = Throws([&]() { root.SemGetClass("Unknown", 8, 5); },
                             &l, &c, &msg);
        Check(thrown && l == 8 && c == 5, "необъявленный класс с позицией");

        Tree * obj = root.SemInclude("p", TYPE_OBJECT, 9, 5);
        obj->n->ref = clsRec;

        Tree * fx = root.SemFindMember(obj->n, "x", 10, 7);
        Check(fx != NULL && fx->n->type == TYPE_INT, "поиск поля класса");
        Tree * fm = root.SemFindMember(obj->n, "move", 10, 7);
        Check(fm != NULL && fm->n->type == TYPE_FUNCT, "поиск метода класса");

        thrown = Throws([&]() { root.SemFindMember(obj->n, "z", 11, 7); },
                        &l, &c, &msg);
        Check(thrown && l == 11 && c == 7 &&
              msg.find("нет члена") != std::string::npos,
              "отсутствующий член класса с позицией");

        thrown = Throws([&]() { root.SemFindMember(NULL, "z", 12, 3); },
                        &l, &c, &msg);
        Check(thrown && msg.find("только у объекта класса") != std::string::npos,
              "обращение к члену не у объекта класса");

        thrown = Throws([&]() { root.SemFindMember(obj->n, "visible", 13, 7); },
                        &l, &c, &msg);
        Check(thrown && l == 13 && c == 7 &&
              msg.find("private") != std::string::npos,
              "закрытый член недоступен вне класса");

        Tree::Cur  = cls;
        Tree * body = Tree::Cur->SemEnterBlock(14, 5);
        Check(Tree::Cur == body, "вход в тело метода");
        thrown = Throws([&]() { Tree::Cur->SemFindMember(obj->n, "visible", 15, 9); });
        Check(!thrown, "закрытый член доступен в методе своего класса");
        Tree::Cur->SemExitBlock();
        Tree::Cur = outer;
    }

    // совместимость типов при присваивании
    {
        SemNode * clsA = new SemNode("A", TYPE_CLASS, 1, 1);
        SemNode * clsB = new SemNode("B", TYPE_CLASS, 1, 1);

        Check(TypesCompatible(TYPE_INT, NULL, TYPE_INT, NULL), "int := int");
        Check(TypesCompatible(TYPE_FLOAT, NULL, TYPE_INT, NULL), "float := int");
        Check(TypesCompatible(TYPE_INT, NULL, TYPE_FLOAT, NULL), "int := float");
        Check(!TypesCompatible(TYPE_BOOL, NULL, TYPE_INT, NULL),
              "bool := int запрещено");
        Check(!TypesCompatible(TYPE_INT, NULL, TYPE_BOOL, NULL),
              "int := bool запрещено");
        Check(TypesCompatible(TYPE_OBJECT, clsA, TYPE_OBJECT, clsA),
              "объект := объект того же класса");
        Check(!TypesCompatible(TYPE_OBJECT, clsA, TYPE_OBJECT, clsB),
              "объект := объект другого класса запрещено");

        delete clsA;
        delete clsB;
    }

    // печать дерева в файл и корректное освобождение памяти
    {
        Tree * root = new Tree(NULL, NULL, NULL, new SemNode("", TYPE_SCOPE, 1, 1));
        Tree::Cur = root;
        root->SemInclude("a", TYPE_INT, 1, 1);
        Tree * blk = root->SemEnterBlock(2, 1);
        Tree * pb  = blk->SemInclude("b", TYPE_BOOL, 3, 9);
        pb->n->isPrivate = true;
        Tree::Cur->SemExitBlock();

        Check(Tree::Cur == root, "печать не изменяет текущую вершину");

        FILE * f = fopen(OutFile(), "w");
        bool   printed = (f != NULL);
        if (printed) {
            root->Print(f);
            fclose(f);
        }
        Check(printed, "печать дерева выполнена");

        std::string text = ReadFile(OutFile());
        Check(text.find("глобальная область") != std::string::npos,
              "печать: заголовок дерева");
        Check(text.find("a : int") != std::string::npos,
              "печать: поле глобальной области");
        Check(text.find("b : bool [private]") != std::string::npos,
              "печать: вложенное поле с пометкой private");

        delete root;
        Check(Tree::Cur == NULL, "при удалении дерева текущая вершина сброшена");
    }
}

// проверка структуры дерева, построенного синтаксическим анализатором
static void RunStructureTests() {
    std::string src =
        "class Point\n"
        "{\n"
        "public:\n"
        "    int x;\n"
        "    int y;\n"
        "    void move()\n"
        "    {\n"
        "        x = x + 1;\n"
        "    }\n"
        "    Point()\n"
        "    {\n"
        "        x = 0;\n"
        "    }\n"
        "}\n"
        "int limit = 10;\n"
        "void main()\n"
        "{\n"
        "    Point p;\n"
        "    p.x = 1;\n"
        "    p.move();\n"
        "}\n";

    std::ofstream out(TmpFile(), std::ios::binary | std::ios::trunc);
    out << src;
    out.close();

    TScanner * sc = new TScanner(TmpFile());
    TParser  * p  = new TParser(sc, true);

    bool ok = true;
    try {
        p->Program();
    }
    catch (TError & e) {
        ok = false;
        printf("       неожиданная ошибка: строка %d, позиция %d: %s\n",
               e.line, e.col, e.message.c_str());
    }
    Check(ok, "корректная программа разобрана без ошибок");

    if (ok) {
        Tree * root = p->SemTreeRoot();
        Check(root != NULL && root->n != NULL &&
              root->n->type == TYPE_SCOPE, "корень - глобальная область");

        Tree * point = root->FindUp(root, "Point");
        Check(point != NULL && point->n->type == TYPE_CLASS,
              "класс Point занесён в глобальную область");

        Tree * limit = root->FindUp(root, "limit");
        Check(limit != NULL && limit->n->type == TYPE_INT &&
              limit->n->line == 15, "глобальная переменная limit");

        Tree * mainF = root->FindUp(root, "main");
        Check(mainF != NULL && mainF->n->type == TYPE_FUNCT &&
              mainF->n->line == 16, "главная функция main");

        if (point != NULL) {
            bool hasX = false, hasY = false, hasMove = false, hasCtor = false;
            bool hasBody = false;
            for (Tree * q = point->Left; q != NULL; q = q->Right) {
                if (q->n == NULL)
                    continue;
                if (q->n->id == "x" && q->n->type == TYPE_INT) hasX = true;
                if (q->n->id == "y" && q->n->type == TYPE_INT) hasY = true;
                if (q->n->id == "move" && q->n->type == TYPE_FUNCT) hasMove = true;
                if (q->n->id == "Point" && q->n->type == TYPE_FUNCT) hasCtor = true;
                if (q->n->type == TYPE_SCOPE) {
                    hasBody = true;
                    Check(q->Up == point, "тело метода вложено в область класса");
                }
            }
            Check(hasX && hasY, "поля класса занесены в область класса");
            Check(hasMove, "метод класса занесён в область класса");
            Check(hasCtor, "конструктор занесён в область класса");
            Check(hasBody, "тело метода образует вложенную область");
        }
    }

    delete p;
    delete sc;
}

int main(int argc, char * argv[]) {
    const char * path = (argc > 1) ? argv[1] : "tests/sem_programs.txt";

    RunProgramTests(path);
    RunTreeTests();
    RunStructureTests();

    printf("\nИтого: %d из %d тестов пройдено", testsPassed, testsTotal);
    if (testsFailed)
        printf(", ошибок: %d", testsFailed);
    printf("\n");

    return testsFailed == 0 ? 0 : 1;
}
