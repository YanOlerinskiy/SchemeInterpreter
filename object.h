#pragma once

#include "error.h"

#include <memory>
#include <vector>
#include <string>
#include <set>

#include <map>

class Object;

using Node = Object*;

//////////////////////////////////////////////////////////////////////////////////////////
// heap

class Number;
class Symbol;
class Cell;

void PrintType();

class Heap {
public:
    Heap(const Heap&) = delete;
    void operator=(const Heap&) = delete;

    template <typename T, typename... Args>
    Object* Make(Args... args) {
        storage_.push_back(std::unique_ptr<Object>(new T(args...)));
        return storage_.back().get();
    }

    void RunGC();
    void Del();

    void AddRoot(Object* root);

    void RemoveRoot(Object* root);

    bool Check(Object* root);

    static Heap& GetInstance() {
        if (!ptr) {
            ptr.reset(new Heap);
        }
        return *ptr;
    }

private:
    Heap();

    Object* lifetime_root_;
    std::vector<std::unique_ptr<Object>> storage_;

    static std::unique_ptr<Heap> ptr;
};

Heap& GetHeap();

//////////////////////////////////////////////////////////////////////////////////////////
// scope

class Scope {
public:
    Scope(std::shared_ptr<Scope> prev);
    ~Scope() {
        std::set<Object*> wow;
        for (auto [symbol, root] : buf_) {
            wow.insert(root);
        }
        for (auto ptr : wow) {
            Heap::GetInstance().RemoveRoot(ptr);
        }
    }

    Object*& ResolveSymbol(const std::string& symbol);

    template <typename T>
    void Define(const std::string& symbol, T* root) {
        if (buf_.find(symbol) != buf_.end()) {
            Heap::GetInstance().RemoveRoot(buf_[symbol]);
        }
        buf_[symbol] = root;
        Heap::GetInstance().AddRoot(buf_[symbol]);
    }
    void Set(const std::string& symbol, Node root);

    std::shared_ptr<Scope> GetPrev() {
        return prev_;
    }

private:
    // initialize all builtin functions for global scope
    void InitBuiltinFunctions();

    std::map<std::string, Object*> buf_;
    std::shared_ptr<Scope> prev_;
};

//////////////////////////////////////////////////////////////////////////////////////////
// object

class Object : public std::enable_shared_from_this<Object> {
public:
    friend class Heap;

    virtual Object* Run(std::shared_ptr<Scope>, Object*) {
        throw RuntimeError("Object not callable");
    }

    virtual Object* Clone() const {
        auto ptr = Heap::GetInstance().Make<Object>(*this);
        ptr->mark_ = mark_;
        ptr->dependants_ = dependants_;
        return ptr;
    }
    virtual void Update() {
    }
    virtual ~Object() {
        dependants_.clear();
    }

protected:
    Object() = default;

    Object(const Object& other) : mark_(other.mark_), dependants_(other.dependants_) {
    }

    void Mark();

    void AddDependant(Object* ptr) {
        if (ptr) {
            dependants_.insert(ptr);
        }
    }

    void RemoveDependant(Object* ptr) {
        if (ptr && dependants_.find(ptr) != dependants_.end()) {
            dependants_.erase(dependants_.find(ptr));
        }
    }

    bool Check(Object* ptr) {
        return dependants_.count(ptr);
    }

    bool mark_ = 0;
    std::multiset<Object*> dependants_;
};

class Number : public Object {
    friend class Heap;

public:
    int GetValue() const {
        return value_;
    }
    Object* Clone() const {
        return Heap::GetInstance().Make<Number>(*this);
    }

protected:
    Number() = default;
    Number(const Number& other) : value_(other.value_) {
    }
    Number(int value) : value_(value) {
    }

private:
    int value_;
};

class Symbol : public Object {
    friend class Heap;

public:
    const std::string& GetName() const {
        return name_;
    }

    Object* Clone() const {
        return Heap::GetInstance().Make<Symbol>(*this);
    }

protected:
    Symbol() = default;
    Symbol(const Symbol& other) : name_(other.name_) {
    }
    Symbol(const std::string& name) : name_(name) {
    }

private:
    std::string name_;
};

class Cell : public Object {
    friend class Heap;

public:
    Object*& GetFirst() {
        return first_;
    }
    Object*& GetSecond() {
        return second_;
    }

    Object* Clone() const {
        return Heap::GetInstance().Make<Cell>(*this);
    }

    void Update() {
        dependants_.clear();
        AddDependant(first_);
        AddDependant(second_);
    }

protected:
    Cell() = default;
    Cell(const Cell& other) {
        first_ = other.first_;
        second_ = other.second_;
    }
    Cell(Object* first, Object* second) : first_(first), second_(second) {
    }

private:
    Object* first_ = nullptr;
    Object* second_ = nullptr;
};

//////////////////////////////////////////////////////////////////////////////////////////
// builtin functions

//////////////////////////////////////////////////////////////////////
// checkers

// number?
class IsNumber : public Object {
    friend class Heap;

public:
    Node Run(std::shared_ptr<Scope> scope, Node root);

    Object* Clone() const {
        return Heap::GetInstance().Make<IsNumber>(*this);
    }
};

// symbol?
class IsSymbol : public Object {
    friend class Heap;

    Node Run(std::shared_ptr<Scope> scope, Node root);

    Object* Clone() const {
        return Heap::GetInstance().Make<IsSymbol>(*this);
    }
};

// boolean?
class IsBoolean : public Object {
    friend class Heap;

public:
    Node Run(std::shared_ptr<Scope> scope, Node root);

    Object* Clone() const {
        return Heap::GetInstance().Make<IsBoolean>(*this);
    }
};

// null?
class IsNull : public Object {
    friend class Heap;

public:
    Node Run(std::shared_ptr<Scope> scope, Node root);

    Object* Clone() const {
        return Heap::GetInstance().Make<IsNull>(*this);
    }
};

// pair?
class IsPair : public Object {
    friend class Heap;

public:
    Node Run(std::shared_ptr<Scope> scope, Node root);

    Object* Clone() const {
        return Heap::GetInstance().Make<IsPair>(*this);
    }
};

// list?
class IsList : public Object {
    friend class Heap;

public:
    Node Run(std::shared_ptr<Scope> scope, Node root);

    Object* Clone() const {
        return Heap::GetInstance().Make<IsList>(*this);
    }
};

//////////////////////////////////////////////////////////////////////
// constructors

// cons
class MakePair : public Object {
    friend class Heap;

public:
    Node Run(std::shared_ptr<Scope> scope, Node root);

    Object* Clone() const {
        return Heap::GetInstance().Make<MakePair>(*this);
    }
};

// list
class MakeList : public Object {
    friend class Heap;

public:
    Node Run(std::shared_ptr<Scope> scope, Node root);

    Object* Clone() const {
        return Heap::GetInstance().Make<MakeList>(*this);
    }
};

//////////////////////////////////////////////////////////////////////
// getters

// car
class GetHead : public Object {
    friend class Heap;

public:
    Node Run(std::shared_ptr<Scope> scope, Node root);

    Object* Clone() const {
        return Heap::GetInstance().Make<GetHead>(*this);
    }
};

// cdr
class GetTail : public Object {
    friend class Heap;

public:
    Node Run(std::shared_ptr<Scope> scope, Node root);

    Object* Clone() const {
        return Heap::GetInstance().Make<GetTail>(*this);
    }
};

// list-ref
class Get : public Object {
    friend class Heap;

public:
    Node Run(std::shared_ptr<Scope> scope, Node root);

    Object* Clone() const {
        return Heap::GetInstance().Make<Get>(*this);
    }
};

// list-tail
class GetSuffix : public Object {
    friend class Heap;

public:
    Node Run(std::shared_ptr<Scope> scope, Node root);

    Object* Clone() const {
        return Heap::GetInstance().Make<GetSuffix>(*this);
    }
};

//////////////////////////////////////////////////////////////////////
// basic arithmetic

// not
class Not : public Object {
    friend class Heap;

public:
    Node Run(std::shared_ptr<Scope> scope, Node root);

    Object* Clone() const {
        return Heap::GetInstance().Make<Not>(*this);
    }
};

// and
class And : public Object {
    friend class Heap;

public:
    Node Run(std::shared_ptr<Scope> scope, Node root);

    Object* Clone() const {
        return Heap::GetInstance().Make<And>(*this);
    }
};

// or
class Or : public Object {
    friend class Heap;

public:
    Node Run(std::shared_ptr<Scope> scope, Node root);

    Object* Clone() const {
        return Heap::GetInstance().Make<Or>(*this);
    }
};

// +
class Plus : public Object {
    friend class Heap;

public:
    Node Run(std::shared_ptr<Scope> scope, Node root);

    Object* Clone() const {
        return Heap::GetInstance().Make<Plus>(*this);
    }
};

// -
class Minus : public Object {
    friend class Heap;

public:
    Node Run(std::shared_ptr<Scope> scope, Node root);

    Object* Clone() const {
        return Heap::GetInstance().Make<Minus>(*this);
    }
};

// *
class Mult : public Object {
    friend class Heap;

public:
    Node Run(std::shared_ptr<Scope> scope, Node root);

    Object* Clone() const {
        return Heap::GetInstance().Make<Mult>(*this);
    }
};

// /
class Div : public Object {
    friend class Heap;

public:
    Node Run(std::shared_ptr<Scope> scope, Node root);

    Object* Clone() const {
        return Heap::GetInstance().Make<Div>(*this);
    }
};

//////////////////////////////////////////////////////////////////////
// simple integer operations

// =
class IsEqual : public Object {
    friend class Heap;

public:
    Node Run(std::shared_ptr<Scope> scope, Node root);

    Object* Clone() const {
        return Heap::GetInstance().Make<IsEqual>(*this);
    }
};

// >
class IsGreater : public Object {
    friend class Heap;

public:
    Node Run(std::shared_ptr<Scope> scope, Node root);

    Object* Clone() const {
        return Heap::GetInstance().Make<IsGreater>(*this);
    }
};

// <
class IsSmaller : public Object {
    friend class Heap;

public:
    Node Run(std::shared_ptr<Scope> scope, Node root);

    Object* Clone() const {
        return Heap::GetInstance().Make<IsSmaller>(*this);
    }
};

// >=
class IsGeq : public Object {
    friend class Heap;

public:
    Node Run(std::shared_ptr<Scope> scope, Node root);

    Object* Clone() const {
        return Heap::GetInstance().Make<IsGeq>(*this);
    }
};

// <=
class IsLeq : public Object {
    friend class Heap;

public:
    Node Run(std::shared_ptr<Scope> scope, Node root);

    Object* Clone() const {
        return Heap::GetInstance().Make<IsLeq>(*this);
    }
};

// max
class Max : public Object {
    friend class Heap;

public:
    Node Run(std::shared_ptr<Scope> scope, Node root);

    Object* Clone() const {
        return Heap::GetInstance().Make<Max>(*this);
    }
};

// min
class Min : public Object {
    friend class Heap;

public:
    Node Run(std::shared_ptr<Scope> scope, Node root);

    Object* Clone() const {
        return Heap::GetInstance().Make<Min>(*this);
    }
};

// abs
class Abs : public Object {
    friend class Heap;

public:
    Node Run(std::shared_ptr<Scope> scope, Node root);

    Object* Clone() const {
        return Heap::GetInstance().Make<Abs>(*this);
    }
};

//////////////////////////////////////////////////////////////////////
// variable manipulation

// define
class Define : public Object {
    friend class Heap;

public:
    Node Run(std::shared_ptr<Scope> scope, Node root);

    Object* Clone() const {
        return Heap::GetInstance().Make<Define>(*this);
    }
};

// set!
class Set : public Object {
    friend class Heap;

public:
    Node Run(std::shared_ptr<Scope> scope, Node root);

    Object* Clone() const {
        return Heap::GetInstance().Make<Set>(*this);
    }
};

// set-car!
class SetCar : public Object {
    friend class Heap;

public:
    Node Run(std::shared_ptr<Scope> scope, Node root);

    Object* Clone() const {
        return Heap::GetInstance().Make<SetCar>(*this);
    }
};

// set-cdr!
class SetCdr : public Object {
    friend class Heap;

public:
    Node Run(std::shared_ptr<Scope> scope, Node root);

    Object* Clone() const {
        return Heap::GetInstance().Make<SetCdr>(*this);
    }
};

//////////////////////////////////////////////////////////////////////
// control flow

// if
class If : public Object {
    friend class Heap;

public:
    Node Run(std::shared_ptr<Scope> scope, Node root);

    Object* Clone() const {
        return Heap::GetInstance().Make<If>(*this);
    }
};

//////////////////////////////////////////////////////////////////////
// lambda

// lambda
class ConstructLambda : public Object {
    friend class Heap;

public:
    Node Run(std::shared_ptr<Scope> scope, Node root);

    Object* Clone() const {
        return Heap::GetInstance().Make<ConstructLambda>(*this);
    }
};

//////////////////////////////////////////////////////////////////////////////////////////

// Runtime type checking and convertion.
// https://en.cppreference.com/w/cpp/memory/shared_ptr/pointer_cast

template <class T>
T* As(Object* obj) {
    return dynamic_cast<T*>(obj);
}

template <class T>
bool Is(Object* obj) {
    return dynamic_cast<T*>(obj) != nullptr;
}

//////////////////////////////////////////////////////////////////////////////////////////
// helper functions

inline Object*& GetFirst(Object* root) {
    return As<Cell>(root)->GetFirst();
}

inline Object*& GetSecond(Object* root) {
    return As<Cell>(root)->GetSecond();
}

inline int64_t GetValue(Object* root) {
    return As<Number>(root)->GetValue();
}

inline const std::string& GetName(Object* root) {
    return As<Symbol>(root)->GetName();
}

inline bool IsFalse(Object* root) {
    return Is<Symbol>(root) && GetName(root) == "#f";
}

inline bool IsTrue(Object* root) {
    return !IsFalse(root);
}

// root has to be a Cell
inline bool IsNullCell(Object* root) {
    return !GetFirst(root);
}

class Lambda : public Object {
    friend class Heap;

public:
    Lambda(std::shared_ptr<Scope> scope, const std::vector<Node>& args, Node calc)
        : local_scope_(scope), args_(args), calc_(calc) {
        dependants_ = std::multiset<Node>(args.begin(), args.end());
        dependants_.insert(calc_);
    }

    Node Run(std::shared_ptr<Scope> scope, Node root);

    Object* Clone() const {
        auto ptr = As<Lambda>(Heap::GetInstance().Make<Lambda>(*this));
        ptr->local_scope_ = local_scope_;
        ptr->args_ = args_;
        ptr->calc_ = calc_;
        return ptr;
    }

private:
    std::shared_ptr<Scope> local_scope_;
    std::vector<Node> args_;
    Node calc_;
};

// #include <iostream>
// using std::cout, std::endl;

// inline void PrintEulerTour(Object* root) {
//     if (!root) {
//         cout << "Null\nUp" << endl;
//         return;
//     }
//     if (Is<Number>(root)) {
//         cout << As<Number>(root)->GetValue() << endl;
//         cout << "Up" << endl;
//         return;
//     }
//     if (Is<Symbol>(root)) {
//         cout << As<Symbol>(root)->GetName() << endl;
//         cout << "Up" << endl;
//         return;
//     }
//     if (!Is<Cell>(root)) {
//         cout << "SpecialType" << endl;
//         cout << "Up" << endl;
//         return;
//     }
//     cout << "Left" << endl;
//     PrintEulerTour(GetFirst(root));
//     cout << "Right" << endl;
//     PrintEulerTour(GetSecond(root));
//     cout << "Up" << endl;
// }
