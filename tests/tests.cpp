// tests.cpp - автотесты синтаксического анализатора (лабораторная №5)
// Проверяются все случаи применения грамматики варианта 9: корректные
// конструкции и граничные (ошибочные) ситуации
//
// Тесты построены так: исходный модуль записывается во временный файл,
// затем выполняется разбор. Корректный модуль должен разбираться без
// ошибок, ошибочный - приводить к исключению TError
#include "defs.h"
#include "Scanner.h"
#include "Parser.h"
#include <string>
#include <vector>
#include <fstream>
#include <cstdio>
#include <cstdlib>

// Описание одного теста
struct TestCase {
    std::string name;   // Что проверяем
    std::string src;    // Текст исходного модуля
    bool        valid;  // Признак: true - принять, false - отвергнуть
};

// Путь к временному файлу с исходным модулем
static const char * TmpFile() {
    const char * dir = getenv("TMPDIR");
    static std::string path =
        std::string(dir ? dir : "/tmp") + "/parser_syntax_test_input.txt";
    return path.c_str();
}

// Разобрать текст исходного модуля; вернуть true, если он принят
static bool Accepts(const std::string & src) {
    std::ofstream out(TmpFile(), std::ios::binary | std::ios::trunc);
    out << src;
    out.close();

    TScanner * sc = new TScanner(TmpFile());
    TParser  * p  = new TParser(sc);

    bool ok = true;
    try {
        p->Program();
    }
    catch (TError &) {
        ok = false;
    }

    delete p;
    delete sc;
    return ok;
}

// Добавить тест в список
static void Add(std::vector<TestCase> & v, const std::string & name,
                const std::string & src, bool valid) {
    TestCase c;
    c.name  = name;
    c.src   = src;
    c.valid = valid;
    v.push_back(c);
}

// Список тестов
static std::vector<TestCase> Cases() {
    std::vector<TestCase> v;

    // Программа и описания верхнего уровня

    Add(v, "пустая программа", "", true);
    Add(v, "только пробелы и переводы строк", "  \n\t \n  ", true);
    Add(v, "только однострочный комментарий", "// комментарий\n", true);
    Add(v, "только многострочный комментарий",
        R"src(/* a
        b */)src", true);
    Add(v, "комментарии между лексемами", "void/*x*/main/**/(){}", true);

    Add(v, "главная функция с пустым телом",
        R"src(void main()
        {
        })src", true);
    Add(v, "главная функция с пробелами", "void main ( ) { }", true);
    Add(v, "объявление перед main",
        R"src(int a;
        void main()
        {
        })src", true);
    Add(v, "main перед объявлением",
        R"src(void main()
        {
        }
        int a;)src", true);
    Add(v, "класс, переменная и main",
        R"src(class A
        {
        }
        int x;
        void main()
        {
        })src", true);
    Add(v, "несколько main (грамматика не запрещает)",
        R"src(void main()
        {
        }
        void main()
        {
        })src", true);

    Add(v, "объявление int с инициализацией", "int a = 5;", true);
    Add(v, "объявление float", "float f = 3.14;", true);
    Add(v, "объявление bool true", "bool b = true;", true);
    Add(v, "объявление bool false", "bool b = false;", true);
    Add(v, "тип - имя класса", "MyClass x;", true);
    Add(v, "список переменных", "int a, b = 2, c;", true);
    Add(v, "объявление без инициализации", "int a;", true);

    // Классы

    Add(v, "пустой класс",
        R"src(class A
        {
        })src", true);
    Add(v, "класс с пробелами", "class A { }", true);
    Add(v, "класс с полем public",
        R"src(class A
        {
        public:
            int x;
        })src", true);
    Add(v, "класс с полем private",
        R"src(class A
        {
        private:
            float y;
        })src", true);
    Add(v, "класс без спецификатора доступа",
        R"src(class A
        {
            int x;
        })src", true);
    Add(v, "только спецификатор доступа",
        R"src(class A
        {
        public:
        })src", true);
    Add(v, "два спецификатора доступа",
        R"src(class A
        {
        public:
            int x;
        private:
            int y;
        })src", true);
    Add(v, "метод void",
        R"src(class A
        {
        public:
            void foo()
            {
            }
        })src", true);
    Add(v, "конструктор",
        R"src(class A
        {
        public:
            A()
            {
            }
        })src", true);
    Add(v, "деструктор",
        R"src(class A
        {
        public:
            ~A()
            {
            }
        })src", true);
    Add(v, "все члены класса",
        R"src(class A
        {
        public:
            int x;
            void m()
            {
            }
            A()
            {
            }
            ~A()
            {
            }
        private:
            bool b;
        })src", true);
    Add(v, "поле типа класса",
        R"src(class A
        {
            MyType x;
        })src", true);
    Add(v, "несколько полей",
        R"src(class A
        {
        public:
            int x, y, z;
        })src", true);
    Add(v, "поле с инициализацией",
        R"src(class A
        {
        public:
            int x = 1 + 2;
        })src", true);

    // Блоки

    Add(v, "пустой оператор",
        R"src(void main()
        {
            ;
        })src", true);
    Add(v, "несколько пустых операторов",
        R"src(void main()
        {
            ;;;;
        })src", true);
    Add(v, "вложенный блок",
        R"src(void main()
        {
            {
            }
        })src", true);
    Add(v, "соседние блоки",
        R"src(void main()
        {
            {
            }
            {
                ;
            }
        })src", true);
    Add(v, "объявление внутри блока",
        R"src(void main()
        {
            int a;
            a = 5;
        })src", true);
    Add(v, "объявление до и после оператора",
        R"src(void main()
        {
            int a;
            a = 1;
            int b;
            b = 2;
        })src", true);
    Add(v, "локальная переменная типа класса",
        R"src(void main()
        {
            A a;
            a.x = 1;
        })src", true);

    // Операторы

    Add(v, "присваивание",
        R"src(void main()
        {
            a = 1;
        })src", true);
    Add(v, "присваивание полю",
        R"src(void main()
        {
            a.b.c = 5;
        })src", true);
    Add(v, "вызов метода",
        R"src(void main()
        {
            a.foo();
        })src", true);
    Add(v, "вызов метода у поля",
        R"src(void main()
        {
            a.b.foo();
        })src", true);
    Add(v, "if без else",
        R"src(void main()
        {
            if (true)
                ;
        })src", true);
    Add(v, "if с else",
        R"src(void main()
        {
            if (true)
                ;
            else
                ;
        })src", true);
    Add(v, "if с блоками",
        R"src(void main()
        {
            if (true)
            {
            }
            else
            {
                ;
            }
        })src", true);
    Add(v, "вложенный if",
        R"src(void main()
        {
            if (true)
                if (false)
                    ;
                else
                    ;
            else
                ;
        })src", true);
    Add(v, "else if",
        R"src(void main()
        {
            if (a)
                ;
            else if (b)
                ;
        })src", true);
    Add(v, "условие с полем",
        R"src(void main()
        {
            if (a.b)
                x = 1;
        })src", true);
    Add(v, "if внутри блока с объявлением",
        R"src(void main()
        {
            if (a < 3)
            {
                int b;
                b = a;
            }
            else
                a = 0;
        })src", true);
    Add(v, "вложенные if и else",
        R"src(void main()
        {
            if (a)
                if (b)
                    c = 1;
                else
                    c = 2;
            else
                c = 3;
        })src", true);

    // Выражения

    Add(v, "арифметика", "int a = 1 + 2 * 3 - 4 / 2 % 3;", true);
    Add(v, "скобки", "int a = (1 + 2) * (3 - 4);", true);
    Add(v, "вложенные скобки", "int a = ((1));", true);
    Add(v, "унарный минус", "int a = -1;", true);
    Add(v, "унарный плюс", "int a = +5;", true);
    Add(v, "логическое не", "bool b = !true;", true);
    Add(v, "двойное отрицание не поддерживается (унарные без рекурсии)",
        "bool b = !!true;", false);
    Add(v, "логические операции", "bool b = true && false || !true;", true);
    Add(v, "сравнение", "bool b = 1 < 2;", true);
    Add(v, "цепочка сравнений", "bool b = 1 < 2 && 3 >= 4 || 5 == 6;", true);
    Add(v, "равенство и неравенство", "bool b = a == b || c != d;", true);
    Add(v, "цепочка полей в выражении", "int a = b.c.d.e;", true);
    Add(v, "унарный минус перед скобкой", "int a = -(1 + 2);", true);
    Add(v, "унарный оператор перед выражением", "int a = -(a + b);", true);
    Add(v, "вещественная константа .5", "float f = .5;", true);
    Add(v, "вещественная константа 5.", "float f = 5.;", true);
    Add(v, "вещественные константы в выражении", "float f = .5 + 5.;", true);
    Add(v, "приоритеты операций", "int a = 1 + 2 * 3 == 7 && true || false;", true);
    Add(v, "операнд-поле",
        R"src(void main()
        {
            int a;
            b.c = a + 1;
        })src", true);
    Add(v, "приоритеты сравнений",
        "bool b = 1 <= 2 && 3 < 4 || 5 > 6 && 7 <= 8;", true);

    Add(v, "нет ; в объявлении", "int a", false);
    Add(v, "нет имени переменной", "int ;", false);
    Add(v, "нет ; после типа", "int;", false);
    Add(v, "у main нет тела", "void main()", false);
    Add(v, "незакрытый блок",
        R"src(void main()
        {
        )src", false);
    Add(v, "лишняя закрывающая скобка",
        R"src(void main()
        {
        }
        })src", false);
    Add(v, "if без тела",
        R"src(void main()
        {
            if (true)
        })src", false);
    Add(v, "if без скобок",
        R"src(void main()
        {
            if true
                ;
        })src", false);
    Add(v, "if без условия",
        R"src(void main()
        {
            if ()
                ;
        })src", false);
    Add(v, "if с незакрытой скобкой",
        R"src(void main()
        {
            if (1
                ;
        })src", false);
    Add(v, "else без оператора",
        R"src(void main()
        {
            if (true)
                ;
            else
        })src", false);
    Add(v, "else без if",
        R"src(void main()
        {
            else
                ;
        })src", false);
    Add(v, "класс без имени",
        R"src(class
        {
        })src", false);
    Add(v, "класс без тела",
        R"src(class A
        {)src", false);
    Add(v, "класс без закрывающей скобки",
        R"src(class A
        {
            int x;
        )src", false);
    Add(v, "лишняя скобка после класса",
        R"src(class A
        {
        public:
            int x;
        }
        })src", false);
    Add(v, "спецификатор доступа без :",
        R"src(class A
        {
        public
            int x;
        })src", false);
    Add(v, "поле без ;",
        R"src(class A
        {
            int x
        })src", false);
    Add(v, "метод без тела",
        R"src(class A
        {
            void foo();
        })src", false);
    Add(v, "метод с параметрами не поддерживается",
        R"src(class A
        {
            void foo(int x)
            {
            }
        })src", false);
    Add(v, "деструктор на верхнем уровне",
        R"src(~A()
        {
        })src", false);
    Add(v, "выражение вместо описания на верхнем уровне", "a = 5;", false);
    Add(v, "нет ; у локального объявления",
        R"src(void main()
        {
            int x
        })src", false);
    Add(v, "выражение как оператор",
        R"src(void main()
        {
            1 + 2;
        })src", false);
    Add(v, "имя как оператор",
        R"src(void main()
        {
            a;
        })src", false);
    Add(v, "поле как оператор",
        R"src(void main()
        {
            a.b;
        })src", false);
    Add(v, "лишняя запятая в списке",
        R"src(void main()
        {
            int a = 1,;
        })src", false);
    Add(v, "нет запятой между переменными",
        R"src(void main()
        {
            int a b;
        })src", false);
    Add(v, "нет ; после присваивания",
        R"src(void main()
        {
            a = 1
        })src", false);
    Add(v, "нет выражения справа от =", "int a = ;", false);
    Add(v, "оборванное выражение", "int a = 1 + ;", false);
    Add(v, "две константы подряд", "int a = 1 2;", false);
    Add(v, "двойной унарный минус", "int a = --1;", false);
    Add(v, "минус и отрицание подряд", "bool b = -!true;", false);
    Add(v, "незакрытая скобка выражения", "int a = (1 + 2;", false);
    Add(v, "константа вместо типа", "int 123;", false);
    Add(v, "нет параметров () у main", "void main{}", false);
    Add(v, "вызов метода с аргументами",
        R"src(void main()
        {
            a.b(1);
        })src", false);
    Add(v, "вызов без ;",
        R"src(void main()
        {
            a.b()
        })src", false);
    Add(v, "вызов без имени метода",
        R"src(void main()
        {
            a.();
        })src", false);
    Add(v, "обращение после вызова",
        R"src(void main()
        {
            a.b().c();
        })src", false);
    Add(v, "незакрытая скобка условия if",
        R"src(void main()
        {
            if (a b = 1;
        })src", false);

    return v;
}

int main() {
    std::vector<TestCase> cases = Cases();

    int passed = 0;
    int failed = 0;

    for (size_t i = 0; i < cases.size(); i++) {
        bool got = Accepts(cases[i].src);
        bool ok  = (got == cases[i].valid);

        if (ok) {
            passed++;
            printf("[OK]   %s\n", cases[i].name.c_str());
        }
        else {
            failed++;
            printf("[FAIL] %s\n", cases[i].name.c_str());
            printf("       исходный модуль: <<<%s>>>\n", cases[i].src.c_str());
            printf("       ожидалось: %s, получено: %s\n",
                   cases[i].valid ? "принять" : "отвергнуть",
                   got ? "принято" : "отвергнуто");
        }
    }

    printf("\nИтого: %d из %d тестов пройдено",
           passed, (int)cases.size());
    if (failed)
        printf(", ошибок: %d", failed);
    printf("\n");

    return failed == 0 ? 0 : 1;
}
