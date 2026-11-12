// Автотесты интерпретатора: значения переменных, ошибки выполнения и трассировка присваиваний
#include "defs.h"
#include "Scanner.h"
#include "Parser.h"
#include "SemTree.h"
#include "Interp.h"
#include <string>
#include <vector>
#include <fstream>
#include <cstdio>
#include <cstdlib>

// вид ожидаемой ошибки
enum ErrKind { EK_ANY = 0, EK_LEX, EK_SYN, EK_SEM, EK_RUN };

// один тест: программа и ожидаемый результат
struct InterpCase {
    std::string name;
    std::string src;
    bool        valid;                  // true - программа выполняется без ошибок
    ErrKind     kind;
    int         line, col;
    std::string part;                   // часть сообщения об ошибке
    std::vector<std::pair<std::string, std::string> > values; // имя=значение

    InterpCase() : valid(true), kind(EK_ANY), line(0), col(0) {}
};

static int testsPassed = 0;
static int testsFailed = 0;
static int testsTotal  = 0;

static void Ok(const std::string & name) {
    testsTotal++;
    testsPassed++;
    printf("[OK]   %s\n", name.c_str());
}

static void Fail(const std::string & name) {
    testsTotal++;
    testsFailed++;
    printf("[FAIL] %s\n", name.c_str());
}

static const char * TmpFile() {
    const char * dir = getenv("TMPDIR");
    static std::string path =
        std::string(dir ? dir : "/tmp") + "/interp_test_input.txt";
    return path.c_str();
}

static const char * TraceFile() {
    const char * dir = getenv("TMPDIR");
    static std::string path =
        std::string(dir ? dir : "/tmp") + "/interp_test_trace.txt";
    return path.c_str();
}

static std::string ReadFile(const char * path) {
    std::ifstream in(path, std::ios::binary);
    std::string text, line;
    while (in && std::getline(in, line))
        text += line + "\n";
    return text;
}

static std::string Trim(const std::string & s) {
    size_t b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos)
        return "";
    size_t e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

static void Split(const std::string & s, char sep,
                  std::vector<std::string> & out) {
    out.clear();
    size_t start = 0;
    while (true) {
        size_t p = s.find(sep, start);
        if (p == std::string::npos) {
            out.push_back(Trim(s.substr(start)));
            break;
        }
        out.push_back(Trim(s.substr(start, p - start)));
        start = p + 1;
    }
}

// собрать значения переменных из трассировки присваиваний (последнее значение)
static void CollectFromTrace(std::vector<std::pair<std::string, std::string> > & out) {
    std::string text = ReadFile(TraceFile());
    const std::string key = "Присваивание: ";
    size_t pos = 0;
    while ((pos = text.find(key, pos)) != std::string::npos) {
        pos += key.size();
        size_t end = text.find('\n', pos);
        if (end == std::string::npos)
            end = text.size();
        std::string line = text.substr(pos, end - pos);
        size_t eq = line.find(" = ");
        size_t sp = line.rfind(" (");
        if (eq != std::string::npos && sp != std::string::npos && sp > eq + 3) {
            std::string name = line.substr(0, eq);
            std::string val  = line.substr(eq + 3, sp - (eq + 3));
            bool found = false;
            for (size_t i = 0; i < out.size(); i++) {
                if (out[i].first == name) {
                    out[i].second = val;
                    found = true;
                }
            }
            if (!found)
                out.push_back(std::make_pair(name, val));
        }
        pos = end;
    }
}

// выполнить программу: значения, ошибка и её вид
static bool Run(const std::string & src,
                std::vector<std::pair<std::string, std::string> > & values,
                TError & err, ErrKind & kind) {
    std::ofstream out(TmpFile(), std::ios::binary | std::ios::trunc);
    out << src;
    out.close();

    TScanner * sc = new TScanner(TmpFile());
    TParser  * p  = new TParser(sc, true, true);

    // трассировка присваиваний направляется в файл, чтобы не засорять протокол
    FILE * tr = fopen(TraceFile(), "w");
    TInterpreter::SetOutput(tr);

    bool ok = true;
    try {
        p->Program();
    }
    catch (TLexError & e) { err = e; kind = EK_LEX; ok = false; }
    catch (TSynError & e) { err = e; kind = EK_SYN; ok = false; }
    catch (TSemError & e) { err = e; kind = EK_SEM; ok = false; }
    catch (TRunError & e) { err = e; kind = EK_RUN; ok = false; }
    catch (TError & e)    { err = e; kind = EK_ANY; ok = false; }

    if (tr != NULL)
        fclose(tr);
    TInterpreter::SetOutput(stdout);

    if (ok)
        CollectFromTrace(values);

    delete p;
    delete sc;
    return ok;
}

// разобрать строку заголовка и заполнить тест
static bool ParseHeader(const std::string & line, InterpCase & c) {
    std::vector<std::string> f;
    Split(line, '|', f);
    if (f.size() < 2)
        return false;

    c.name = Trim(f[0].substr(std::string("// ТЕСТ:").size()));
    if (f[1].rfind("значения:", 0) == 0) {
        std::string vals = Trim(f[1].substr(std::string("значения:").size()));
        std::vector<std::string> items;
        Split(vals, ',', items);
        for (size_t i = 0; i < items.size(); i++) {
            size_t eq = items[i].find('=');
            if (eq == std::string::npos)
                return false;
            c.values.push_back(std::make_pair(Trim(items[i].substr(0, eq)),
                                              Trim(items[i].substr(eq + 1))));
        }
        c.valid = true;
        return true;
    }
    if (f[1] == "ошибка" && f.size() >= 6) {
        c.valid = false;
        if (f[2] == "R")      c.kind = EK_RUN;
        else if (f[2] == "М") c.kind = EK_SEM;
        else if (f[2] == "С") c.kind = EK_SYN;
        else                  c.kind = EK_LEX;
        c.line = atoi(f[3].c_str());
        c.col  = atoi(f[4].c_str());
        c.part = f[5];
        return true;
    }
    return false;
}

// загрузить тесты из файла
static bool LoadCases(const char * path, std::vector<InterpCase> & cases) {
    std::ifstream in(path);
    if (!in)
        return false;

    std::string line;
    InterpCase cur;
    bool active = false;
    while (std::getline(in, line)) {
        if (line.rfind("// ТЕСТ:", 0) == 0) {
            if (active)
                cases.push_back(cur);
            cur = InterpCase();
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

    // завершающий перевод строки не входит в исходный модуль
    for (size_t i = 0; i < cases.size(); i++) {
        if (!cases[i].src.empty() && cases[i].src[cases[i].src.size() - 1] == '\n')
            cases[i].src.erase(cases[i].src.size() - 1);
    }
    return true;
}

// сравнить ожидаемые значения с вычисленными
static bool SameValues(const std::vector<std::pair<std::string, std::string> > & a,
                       const std::vector<std::pair<std::string, std::string> > & b) {
    if (a.size() != b.size())
        return false;
    for (size_t i = 0; i < a.size(); i++) {
        bool found = false;
        for (size_t j = 0; j < b.size(); j++)
            if (a[i].first == b[j].first && a[i].second == b[j].second)
                found = true;
        if (!found)
            return false;
    }
    return true;
}

static void FailValues(const std::vector<std::pair<std::string, std::string> > & exp,
                       const std::vector<std::pair<std::string, std::string> > & got) {
    printf("       ожидались значения:");
    for (size_t i = 0; i < exp.size(); i++)
        printf(" %s=%s", exp[i].first.c_str(), exp[i].second.c_str());
    printf("\n       получены значения:");
    for (size_t i = 0; i < got.size(); i++)
        printf(" %s=%s", got[i].first.c_str(), got[i].second.c_str());
    printf("\n");
}

// прогнать все тесты из файла
static void RunProgramTests(const char * path) {
    std::vector<InterpCase> cases;
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
        const InterpCase & c = cases[i];
        std::vector<std::pair<std::string, std::string> > values;
        TError  err("", 0, 0);
        ErrKind kind = EK_ANY;
        bool    ok   = Run(c.src, values, err, kind);

        bool good = (ok == c.valid);
        if (good && ok)
            good = SameValues(c.values, values);
        if (good && !ok) {
            if (c.kind != EK_ANY && kind != c.kind)
                good = false;
            if (c.line > 0 && err.line != c.line)
                good = false;
            if (c.col > 0 && err.col != c.col)
                good = false;
            if (!c.part.empty() && err.message.find(c.part) == std::string::npos)
                good = false;
        }

        if (good) {
            Ok(c.name);
        }
        else {
            Fail(c.name);
            printf("       программа: <<<%s>>>\n", c.src.c_str());
            if (c.valid) {
                FailValues(c.values, values);
            }
            else {
                printf("       ожидалась ошибка (строка %d, позиция %d, \"%s\")\n",
                       c.line, c.col, c.part.c_str());
                if (ok)
                    printf("       получено: программа выполнена без ошибок\n");
                else
                    printf("       получено: ошибка (строка %d, позиция %d): %s\n",
                           err.line, err.col, err.message.c_str());
            }
        }
    }
}

// проверка трассировки присваиваний (отладочный режим интерпретатора)
static void RunTraceTests() {
    const char * src =
        "void main()\n"
        "{\n"
        "    int a = 1;\n"
        "    a = a + 1;\n"
        "    if (false)\n"
        "        a = 9;\n"
        "}\n";

    std::vector<std::pair<std::string, std::string> > values;
    TError  err("", 0, 0);
    ErrKind kind = EK_ANY;
    Run(src, values, err, kind);

    std::string text = ReadFile(TraceFile());
    int count = 0;
    size_t pos = 0;
    while ((pos = text.find("DEBUG:", pos)) != std::string::npos) {
        count++;
        pos += 6;
    }

    if (count == 2) Ok("трассировка: две строки присваиваний");
    else            Fail("трассировка: две строки присваиваний");

    if (text.find("Присваивание: a = 1 (int)") != std::string::npos)
        Ok("трассировка: первое присваивание");
    else
        Fail("трассировка: первое присваивание");

    if (text.find("Присваивание: a = 2 (int)") != std::string::npos)
        Ok("трассировка: второе присваивание");
    else
        Fail("трассировка: второе присваивание");

    if (text.find("a = 9") == std::string::npos)
        Ok("трассировка: пропущенная ветвь не выполняется");
    else
        Fail("трассировка: пропущенная ветвь не выполняется");
}

// проверка ошибок выполнения, не связанных с классами
static void RunErrorTests() {
    std::vector<std::pair<std::string, std::string> > values;
    TError  err("", 0, 0);
    ErrKind kind = EK_ANY;

    // деление на ноль
    values.clear();
    kind = EK_ANY;
    if (!Run("void main()\n{\n    int a = 1;\n    int b = a / 0;\n}\n",
             values, err, kind) &&
        kind == EK_RUN &&
        err.message.find("деление на ноль") != std::string::npos)
        Ok("ошибка выполнения: деление на ноль");
    else
        Fail("ошибка выполнения: деление на ноль");

    // остаток от деления на ноль
    values.clear();
    kind = EK_ANY;
    if (!Run("void main()\n{\n    int a = 1;\n    int b = a % 0;\n}\n",
             values, err, kind) &&
        kind == EK_RUN &&
        err.message.find("остаток от деления на ноль") != std::string::npos)
        Ok("ошибка выполнения: остаток от деления на ноль");
    else
        Fail("ошибка выполнения: остаток от деления на ноль");

    // чтение неинициализированной переменной
    values.clear();
    kind = EK_ANY;
    if (!Run("void main()\n{\n    int a;\n    int b = a + 1;\n}\n",
             values, err, kind) &&
        kind == EK_RUN &&
        err.message.find("не инициализирована") != std::string::npos)
        Ok("ошибка выполнения: неинициализированная переменная");
    else
        Fail("ошибка выполнения: неинициализированная переменная");
}

// подсчитать число вхождений подстроки
static int CountOf(const std::string & text, const std::string & what) {
    int n = 0;
    size_t pos = 0;
    while ((pos = text.find(what, pos)) != std::string::npos) {
        n++;
        pos += what.size();
    }
    return n;
}

// проверка трассировки семантического дерева при входе в блок и выходе из него
static void RunTreeTraceTests() {
    const char * src =
        "void main()\n"
        "{\n"
        "    int a = 1;\n"
        "    {\n"
        "        int b = 2;\n"
        "        a = a + b;\n"
        "    }\n"
        "    a = a + 3;\n"
        "}\n";

    std::ofstream out(TmpFile(), std::ios::binary | std::ios::trunc);
    out << src;
    out.close();

    TScanner * sc = new TScanner(TmpFile());
    TParser  * p  = new TParser(sc, true, true, true);

    FILE * tr = fopen(TraceFile(), "w");
    TInterpreter::SetOutput(tr);
    bool ok = true;
    try {
        p->Program();
    }
    catch (TError &) {
        ok = false;
    }
    if (tr != NULL)
        fclose(tr);
    TInterpreter::SetOutput(stdout);

    std::string text = ReadFile(TraceFile());
    if (ok)
        Ok("трассировка дерева: программа выполнена");
    else
        Fail("трассировка дерева: программа выполнена");

    if (CountOf(text, "До входа в блок") == 2)
        Ok("трассировка дерева: два вывода до входа");
    else
        Fail("трассировка дерева: два вывода до входа");

    if (CountOf(text, "При входе в блок") == 2)
        Ok("трассировка дерева: два вывода при входе");
    else
        Fail("трассировка дерева: два вывода при входе");

    if (CountOf(text, "Перед выходом из блока") == 2)
        Ok("трассировка дерева: два вывода перед выходом");
    else
        Fail("трассировка дерева: два вывода перед выходом");

    if (CountOf(text, "После выхода из блока") == 2)
        Ok("трассировка дерева: два вывода после выхода");
    else
        Fail("трассировка дерева: два вывода после выхода");

    // после выхода из внутреннего блока его записи удалены из дерева
    size_t last = text.rfind("После выхода из блока");
    if (last != std::string::npos) {
        std::string tail = text.substr(last);
        if (tail.find("b : int") == std::string::npos)
            Ok("трассировка дерева: записи блока удалены после выхода");
        else
            Fail("трассировка дерева: записи блока удалены после выхода");
    }
    else {
        Fail("трассировка дерева: записи блока удалены после выхода");
    }

    // до входа в главную функцию её тела в дереве ещё нет
    size_t firstEnter = text.find("При входе в блок");
    if (firstEnter != std::string::npos &&
        text.substr(0, firstEnter).find("{ блок (строка 3") == std::string::npos)
        Ok("трассировка дерева: до входа области ещё нет");
    else
        Fail("трассировка дерева: до входа области ещё нет");

    if (text.find("b : int = 2") != std::string::npos)
        Ok("трассировка дерева: значения переменных в дереве");
    else
        Fail("трассировка дерева: значения переменных в дереве");

    if (text.find("<-- текущая область") != std::string::npos)
        Ok("трассировка дерева: отмечена текущая область");
    else
        Fail("трассировка дерева: отмечена текущая область");

    delete p;
    delete sc;
}

// проверка предупреждений о неявном приведении типов
static void RunWarningTests() {
    std::vector<std::pair<std::string, std::string> > values;
    TError  err("", 0, 0);
    ErrKind kind = EK_ANY;

    // int приводится к float при инициализации
    values.clear();
    Run("void main()\n{\n    float f = 1;\n}\n", values, err, kind);
    std::string t = ReadFile(TraceFile());
    if (t.find("Предупреждение: неявное преобразование типа int к float") != std::string::npos)
        Ok("предупреждение: int приводится к float при инициализации");
    else
        Fail("предупреждение: int приводится к float при инициализации");

    if (t.find("(строка 3, позиция 11)") != std::string::npos)
        Ok("предупреждение: указана строка и позиция");
    else
        Fail("предупреждение: указана строка и позиция");

    // float обрезается при приведении к int
    values.clear();
    Run("void main()\n{\n    int i = 1.5;\n}\n", values, err, kind);
    t = ReadFile(TraceFile());
    if (t.find("Предупреждение: значение 1.5 обрезается при преобразовании к int") != std::string::npos)
        Ok("предупреждение: float обрезается при приведении к int");
    else
        Fail("предупреждение: float обрезается при приведении к int");

    // int приводится к float внутри выражения
    values.clear();
    Run("void main()\n{\n    float g = 1 + 2.5;\n}\n", values, err, kind);
    t = ReadFile(TraceFile());
    if (t.find("Предупреждение: неявное преобразование левого операнда типа int к float") != std::string::npos)
        Ok("предупреждение: int приводится к float в выражении");
    else
        Fail("предупреждение: int приводится к float в выражении");

    // int приводится к float при сравнении
    values.clear();
    Run("void main()\n{\n    bool b = 1 < 1.5;\n}\n", values, err, kind);
    t = ReadFile(TraceFile());
    if (t.find("Предупреждение: неявное преобразование левого операнда типа int к float") != std::string::npos)
        Ok("предупреждение: int приводится к float при сравнении");
    else
        Fail("предупреждение: int приводится к float при сравнении");

    // приведений нет - предупреждений быть не должно
    values.clear();
    Run("void main()\n{\n    int a = 1 + 2;\n    int b = a;\n}\n", values, err, kind);
    t = ReadFile(TraceFile());
    if (t.find("Предупреждение:") == std::string::npos)
        Ok("предупреждение: без приведений не печатается");
    else
        Fail("предупреждение: без приведений не печатается");

    // float в float - приведения нет
    values.clear();
    Run("void main()\n{\n    float f = 1.5;\n    float g = f;\n}\n", values, err, kind);
    t = ReadFile(TraceFile());
    if (t.find("Предупреждение:") == std::string::npos)
        Ok("предупреждение: float в float не требует приведения");
    else
        Fail("предупреждение: float в float не требует приведения");
}

int main(int argc, char * argv[]) {
    const char * path = (argc > 1) ? argv[1] : "tests/test_programs.txt";

    RunProgramTests(path);
    RunTraceTests();
    RunWarningTests();
    RunTreeTraceTests();
    RunErrorTests();

    printf("\nИтого: %d из %d тестов пройдено", testsPassed, testsTotal);
    if (testsFailed)
        printf(", ошибок: %d", testsFailed);
    printf("\n");

    return testsFailed == 0 ? 0 : 1;
}
