#pragma once

#include "defs.h"
#include "SemTree.h"
#include <string>

// интерпретатор: вычисление выражений, приведение типов, присваивания и их трассировка
class TInterpreter {
public:
    // поток, в который печатается трассировка присваиваний (по умолчанию stdout)
    static FILE * out;
    static void SetOutput(FILE * f) { out = f; }

    // напечатать предупреждение о неявном приведении типа
    static void Warn(const std::string & msg, int line, int col);

    // привести значение к типу to: int и float совместимы, bool только bool
    static Value Convert(const Value & v, DATA_TYPE to, int line, int col);

    // арифметическая операция: + - * / %
    static Value Arith(const std::string & op, const Value & a, const Value & b,
                       int line, int col);

    // операция сравнения: < <= > >= == !=
    static Value Compare(const std::string & op, const Value & a, const Value & b,
                         int line, int col);

    // логическая операция: && ||
    static Value Logic(const std::string & op, const Value & a, const Value & b,
                       int line, int col);

    // унарная операция: + - !
    static Value Unary(const std::string & op, const Value & a, int line, int col);

    // напечатать строку трассировки присваивания
    static void TraceAssign(const std::string & name, const std::string & value,
                            const std::string & type, int line, int col,
                            const char * scope);

    // напечатать строку трассировки копирования поля объекта
    static void TraceCopy(const std::string & dst, const std::string & src,
                          const std::string & value, const std::string & type,
                          int line, int col, const char * scope);

    // записать значение в переменную с приведением и трассировкой
    static void Assign(SemNode * var, const Value & v, int line, int col,
                       const char * scope);

    // прочитать значение переменной, ошибка если значение не задано
    static Value Load(const SemNode * var, int line, int col);

    // прочитать значение поля объекта; name - путь для сообщений об ошибках
    static Value LoadField(TObject * obj, const SemNode * field,
                           int line, int col);

    // записать значение в поле объекта с приведением и трассировкой
    static void AssignField(TObject * obj, const SemNode * field,
                            const Value & v, int line, int col,
                            const char * scope, const std::string & name);

    // напечатать значения всех инициализированных переменных дерева
    static void PrintValues(Tree * root, FILE * f);
};
