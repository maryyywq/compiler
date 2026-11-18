#include "Interp.h"
#include <stdio.h>
#include <limits.h>

// по умолчанию трассировка присваиваний печатается в стандартный поток вывода
FILE * TInterpreter::out = stdout;

// проверить, что значение вычислено, и вернуть его
static const Value & NeedValue(const Value & v, int line, int col) {
    if (!v.has)
        throw TRunError("значение выражения не вычислено", line, col);
    return v;
}

// является ли значение числовым
static bool IsNumber(const Value & v) {
    return v.type == TYPE_INT || v.type == TYPE_FLOAT;
}

// числовое значение в виде double для смешанных операций
static double AsDouble(const Value & v) {
    return v.type == TYPE_INT ? (double)v.i : (double)v.f;
}

// напечатать предупреждение о неявном приведении типа
void TInterpreter::Warn(const std::string & msg, int line, int col) {
    if (out != NULL)
        fprintf(out, "Предупреждение: %s (строка %d, позиция %d)\n",
                msg.c_str(), line, col);
}

// привести значение к типу to: int и float приводятся друг к другу,
// bool приводится к числовым и обратно (как в C++)
Value TInterpreter::Convert(const Value & v, DATA_TYPE to, int line, int col) {
    if (!v.has)
        return v;
    if (v.type == to)
        return v;
    if (to == TYPE_FLOAT && v.type == TYPE_INT) {
        Warn("неявное преобразование типа int к float", line, col);
        return Value::Float((float)v.i);
    }
    if (to == TYPE_INT && v.type == TYPE_FLOAT) {
        Warn("значение " + ValueToString(v) +
             " обрезается при преобразовании к int", line, col);
        return Value::Int((int)v.f);
    }
    if (v.type == TYPE_BOOL && to == TYPE_INT) {
        Warn("неявное преобразование bool к int", line, col);
        return Value::Int(v.b ? 1 : 0);
    }
    if (v.type == TYPE_BOOL && to == TYPE_FLOAT) {
        Warn("неявное преобразование bool к float", line, col);
        return Value::Float(v.b ? 1.0f : 0.0f);
    }
    if (v.type == TYPE_INT && to == TYPE_BOOL) {
        Warn("неявное преобразование int к bool", line, col);
        return Value::Bool(v.i != 0);
    }
    if (v.type == TYPE_FLOAT && to == TYPE_BOOL) {
        Warn("неявное преобразование float к bool", line, col);
        return Value::Bool(v.f != 0.0f);
    }
    throw TRunError("недопустимое приведение значения при интерпретации", line, col);
}

// арифметическая операция над значениями
Value TInterpreter::Arith(const std::string & op, const Value & a,
                          const Value & b, int line, int col) {
    NeedValue(a, line, col);
    NeedValue(b, line, col);

    if (!IsNumber(a) || !IsNumber(b))
        throw TRunError("арифметическая операция над нечисловым значением", line, col);

    // операция % допустима только для целых значений
    if (op == "%") {
        if (a.type != TYPE_INT || b.type != TYPE_INT)
            throw TRunError("операция \"%\" допустима только для целых", line, col);
        if (b.i == 0)
            throw TRunError("остаток от деления на ноль", line, col);
        if (a.i == INT_MIN && b.i == -1)
            return Value::Int(0);
        return Value::Int(a.i % b.i);
    }

    // оба операнда целые - вычисляем в целых
    if (a.type == TYPE_INT && b.type == TYPE_INT) {
        // + - * выполняются в беззнаковых числах: переполнение даёт
        // «заворот» по модулю 2^32 без неопределённого поведения
        unsigned ua = (unsigned)a.i, ub = (unsigned)b.i;
        if (op == "+") return Value::Int((int)(ua + ub));
        if (op == "-") return Value::Int((int)(ua - ub));
        if (op == "*") return Value::Int((int)(ua * ub));
        if (op == "/") {
            if (b.i == 0)
                throw TRunError("деление на ноль", line, col);
            if (a.i == INT_MIN && b.i == -1)
                throw TRunError("переполнение при делении", line, col);
            return Value::Int(a.i / b.i);
        }
    }
    else {
        // хотя бы один операнд вещественный - второй неявно приводится к float
        if (a.type == TYPE_INT && b.type == TYPE_FLOAT)
            Warn("неявное преобразование левого операнда типа int к float", line, col);
        else if (a.type == TYPE_FLOAT && b.type == TYPE_INT)
            Warn("неявное преобразование правого операнда типа int к float", line, col);
        double x = AsDouble(a);
        double y = AsDouble(b);
        if (op == "+") return Value::Float((float)(x + y));
        if (op == "-") return Value::Float((float)(x - y));
        if (op == "*") return Value::Float((float)(x * y));
        if (op == "/") {
            if (y == 0.0)
                throw TRunError("деление на ноль", line, col);
            return Value::Float((float)(x / y));
        }
    }
    throw TRunError("неизвестная арифметическая операция \"" + op + "\"", line, col);
}

// операция сравнения, результат имеет тип bool
Value TInterpreter::Compare(const std::string & op, const Value & a,
                            const Value & b, int line, int col) {
    NeedValue(a, line, col);
    NeedValue(b, line, col);

    // логические значения сравниваются только друг с другом
    if (a.type == TYPE_BOOL || b.type == TYPE_BOOL) {
        if (a.type != TYPE_BOOL || b.type != TYPE_BOOL)
            throw TRunError("нельзя сравнивать логическое значение с числом", line, col);
        if (op == "==") return Value::Bool(a.b == b.b);
        if (op == "!=") return Value::Bool(a.b != b.b);
        throw TRunError("операция \"" + op + "\" недопустима для bool", line, col);
    }
    if (a.type == TYPE_OBJECT || b.type == TYPE_OBJECT)
        throw TRunError("сравнение объектов не поддерживается интерпретатором", line, col);
    if (!IsNumber(a) || !IsNumber(b))
        throw TRunError("сравнение нечисловых значений", line, col);

    // при сравнении int и float целое значение неявно приводится к float
    if (a.type == TYPE_INT && b.type == TYPE_FLOAT)
        Warn("неявное преобразование левого операнда типа int к float", line, col);
    else if (a.type == TYPE_FLOAT && b.type == TYPE_INT)
        Warn("неявное преобразование правого операнда типа int к float", line, col);

    double x = AsDouble(a);
    double y = AsDouble(b);
    if (op == "<")  return Value::Bool(x <  y);
    if (op == "<=") return Value::Bool(x <= y);
    if (op == ">")  return Value::Bool(x >  y);
    if (op == ">=") return Value::Bool(x >= y);
    if (op == "==") return Value::Bool(x == y);
    if (op == "!=") return Value::Bool(x != y);
    throw TRunError("неизвестная операция сравнения \"" + op + "\"", line, col);
}

// логическая операция над значениями типа bool
Value TInterpreter::Logic(const std::string & op, const Value & a,
                          const Value & b, int line, int col) {
    NeedValue(a, line, col);
    NeedValue(b, line, col);
    if (a.type != TYPE_BOOL || b.type != TYPE_BOOL)
        throw TRunError("логическая операция над небулевым значением", line, col);
    if (op == "&&") return Value::Bool(a.b && b.b);
    if (op == "||") return Value::Bool(a.b || b.b);
    throw TRunError("неизвестная логическая операция \"" + op + "\"", line, col);
}

// унарная операция над значением
Value TInterpreter::Unary(const std::string & op, const Value & a,
                          int line, int col) {
    NeedValue(a, line, col);
    if (op == "!") {
        if (a.type != TYPE_BOOL)
            throw TRunError("логическое отрицание допустимо только для bool", line, col);
        return Value::Bool(!a.b);
    }
    if (!IsNumber(a))
        throw TRunError("унарная операция над нечисловым значением", line, col);
    if (op == "+")
        return a;
    if (op == "-")
        return a.type == TYPE_INT ? Value::Int((int)(0u - (unsigned)a.i)) : Value::Float(-a.f);
    throw TRunError("неизвестная унарная операция \"" + op + "\"", line, col);
}

// напечатать строку трассировки присваивания
void TInterpreter::TraceAssign(const std::string & name, const std::string & value,
                               const std::string & type, int line, int col,
                               const char * scope) {
    if (out != NULL)
        fprintf(out, "DEBUG: [%s] (строка %d:%d) Присваивание: %s = %s (%s)\n",
                scope ? scope : "?", line, col, name.c_str(), value.c_str(),
                type.c_str());
}

// напечатать строку трассировки копирования поля объекта
void TInterpreter::TraceCopy(const std::string & dst, const std::string & src,
                             const std::string & value, const std::string & type,
                             int line, int col, const char * scope) {
    if (out != NULL)
        fprintf(out, "DEBUG: [%s] (строка %d:%d) Копирование: %s = %s, "
                     "значение %s (%s)\n",
                scope ? scope : "?", line, col, dst.c_str(), src.c_str(),
                value.c_str(), type.c_str());
}

// записать значение в переменную: приведение к типу переменной и трассировка
void TInterpreter::Assign(SemNode * var, const Value & v, int line, int col,
                          const char * scope) {
    if (var == NULL)
        throw TRunError("нет записи переменной для присваивания", line, col);
    if (var->type != TYPE_INT && var->type != TYPE_FLOAT &&
        var->type != TYPE_BOOL && var->type != TYPE_OBJECT)
        throw TRunError("значение этого типа нельзя прочитать", line, col);

    Value cv = Convert(v, var->type, line, col);
    var->val = cv;

    // простейший отладчик интерпретатора: печатаем имя и присвоенное значение
    TraceAssign(var->id, ValueToString(cv), TypeName(cv.type), line, col, scope);
}

// прочитать значение переменной
Value TInterpreter::Load(const SemNode * var, int line, int col) {
    if (var == NULL)
        throw TRunError("нет записи переменной", line, col);
    if (var->type != TYPE_INT && var->type != TYPE_FLOAT &&
        var->type != TYPE_BOOL && var->type != TYPE_OBJECT)
        throw TRunError("значение этого типа нельзя прочитать", line, col);
    if (!var->val.has)
        throw TRunError("переменная \"" + var->id + "\" не инициализирована", line, col);
    return var->val;
}

// прочитать значение поля объекта
Value TInterpreter::LoadField(TObject * obj, const SemNode * field,
                              int line, int col) {
    if (obj == NULL)
        throw TRunError("обращение к полю несуществующего объекта", line, col);

    Value * v = obj->Find(const_cast<SemNode *>(field));
    if (v == NULL)
        throw TRunError("в объекте класса \"" + obj->clsName +
                        "\" нет поля \"" + field->id + "\"", line, col);
    if (!v->has)
        throw TRunError("поле \"" + field->id + "\" объекта класса \"" +
                        obj->clsName + "\" не инициализировано", line, col);
    return *v;
}

// записать значение в поле объекта
void TInterpreter::AssignField(TObject * obj, const SemNode * field,
                               const Value & v, int line, int col,
                               const char * scope, const std::string & name) {
    if (obj == NULL)
        throw TRunError("присваивание полю несуществующего объекта", line, col);

    Value * f = obj->Find(const_cast<SemNode *>(field));
    if (f == NULL)
        throw TRunError("в объекте класса \"" + obj->clsName +
                        "\" нет поля \"" + field->id + "\"", line, col);

    Value cv = Convert(v, field->type, line, col);
    *f = cv;

    TraceAssign(name, ValueToString(cv), TypeName(field->type),
                line, col, scope);
}

// напечатать значения всех переменных, у которых есть значение
void TInterpreter::PrintValues(Tree * node, FILE * f) {
    if (node == NULL || f == NULL)
        return;
    for (Tree * p = node->Left; p != NULL; p = p->Right) {
        // поля классов печатаются только в составе объектов
        bool isField = (p->n != NULL && p->n->self != NULL &&
                        p->n->self->Up != NULL &&
                        p->n->self->Up->n != NULL &&
                        p->n->self->Up->n->type == TYPE_CLASS);
        if (!isField && p->n != NULL && p->n->val.has &&
            (p->n->type == TYPE_INT || p->n->type == TYPE_FLOAT ||
             p->n->type == TYPE_BOOL)) {
            fprintf(f, "  %s = %s (%s)\n",
                    p->n->id.c_str(),
                    ValueToString(p->n->val).c_str(),
                    TypeName(p->n->type));
        }
        PrintValues(p, f);
    }
}