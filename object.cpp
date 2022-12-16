#include "object.h"

#include "error.h"
#include "scheme.h"

#include <cmath>
#include <algorithm>

std::unique_ptr<Heap> Heap::ptr;

void Heap::AddRoot(Object* root) {
    if (!lifetime_root_) {
        return;
    }
    lifetime_root_->AddDependant(root);
}

void Heap::RemoveRoot(Object* root) {
    if (!lifetime_root_) {
        return;
    }
    lifetime_root_->RemoveDependant(root);
}

bool Heap::Check(Object* root) {
    return lifetime_root_->Check(root);
}

void ParseArguments(std::shared_ptr<Scope> scope, Node root, std::vector<Node>& args) {
    if (!root) {
        return;
    }
    if (!Is<Cell>(root) || IsNullCell(root)) {
        args.push_back(Evaluate(scope, root));
        return;
    }

    if (Is<Symbol>(GetFirst(root)) && As<Symbol>(GetFirst(root))->GetName() == "quote") {
        args.push_back(GetSecond(root));
        return;
    }

    args.push_back(Evaluate(scope, GetFirst(root)));
    ParseArguments(scope, GetSecond(root), args);
}

std::vector<Node> ParseArguments(std::shared_ptr<Scope> scope, Node root) {
    std::vector<Node> args;
    ParseArguments(scope, root, args);
    return args;
}

std::vector<Node> ParseArgumentsNoEval(Node root) {
    std::vector<Node> args;
    Node cur = root;
    while (cur) {
        args.push_back(GetFirst(cur));
        cur = GetSecond(cur);
    }
    return args;
}

bool NodeEq(Node lhs, Node rhs) {
    if (Is<Number>(lhs) && Is<Number>(rhs)) {
        return As<Number>(lhs)->GetValue() == As<Number>(rhs)->GetValue();
    }
    if (Is<Symbol>(lhs) && Is<Symbol>(rhs)) {
        return As<Symbol>(lhs)->GetName() == As<Symbol>(rhs)->GetName();
    }
    return 0;
}

template <typename Func>
Node RuntimeParse(std::shared_ptr<Scope> scope, Node root, const Func* func, const Node res,
                  Node terminal = Heap::GetInstance().Make<Object>()) {
    if (!root || NodeEq(res, terminal)) {
        return res;
    }
    if (!Is<Cell>(root)) {
        return (*func)(res, Evaluate(scope, root));
    }
    return RuntimeParse(scope, GetSecond(root), func, (*func)(res, Evaluate(scope, GetFirst(root))),
                        terminal);
}

// T has to be a descendant to Object
template <typename T>
void RequireArgType(const std::vector<Node>& args) {
    for (auto obj : args) {
        if (!Is<T>(obj)) {
            throw RuntimeError("Certain argument type required, invalid type given");
        }
    }
}

bool NonNullVal(Node root) {
    return root && !Is<Cell>(root);
}

// throws if any argument is a Cell
void RequireValues(const std::vector<Node>& args) {
    for (auto obj : args) {
        if (Is<Cell>(obj)) {
            throw RuntimeError("Cell object as an argument");
        }
    }
}

void RequireArgumentSize(const std::vector<Node>& args, size_t l, size_t r = size_t(-1)) {
    if (l > args.size() || args.size() > r) {
        throw RuntimeError("Incorrect number of arguments");
    }
}

Node Bool(std::shared_ptr<Scope> scope, bool value) {
    return scope->ResolveSymbol(value ? "#t" : "#f");
}

Scope::Scope(std::shared_ptr<Scope> prev) : prev_(prev) {
    if (prev_.get() == nullptr) {
        InitBuiltinFunctions();
    }
}

// initialize all builtin functions for global scope
void Scope::InitBuiltinFunctions() {
    Define("number?", Heap::GetInstance().Make<IsNumber>());
    Define("symbol?", Heap::GetInstance().Make<IsSymbol>());
    Define("boolean?", Heap::GetInstance().Make<IsBoolean>());
    Define("null?", Heap::GetInstance().Make<IsNull>());
    Define("pair?", Heap::GetInstance().Make<IsPair>());
    Define("list?", Heap::GetInstance().Make<IsList>());
    Define("cons", Heap::GetInstance().Make<MakePair>());
    Define("list", Heap::GetInstance().Make<MakeList>());
    Define("car", Heap::GetInstance().Make<GetHead>());
    Define("cdr", Heap::GetInstance().Make<GetTail>());
    Define("list-ref", Heap::GetInstance().Make<Get>());
    Define("list-tail", Heap::GetInstance().Make<GetSuffix>());
    Define("not", Heap::GetInstance().Make<Not>());
    Define("and", Heap::GetInstance().Make<And>());
    Define("or", Heap::GetInstance().Make<Or>());
    Define("+", Heap::GetInstance().Make<Plus>());
    Define("-", Heap::GetInstance().Make<Minus>());
    Define("*", Heap::GetInstance().Make<Mult>());
    Define("/", Heap::GetInstance().Make<Div>());
    Define("=", Heap::GetInstance().Make<IsEqual>());
    Define(">", Heap::GetInstance().Make<IsGreater>());
    Define("<", Heap::GetInstance().Make<IsSmaller>());
    Define(">=", Heap::GetInstance().Make<IsGeq>());
    Define("<=", Heap::GetInstance().Make<IsLeq>());
    Define("max", Heap::GetInstance().Make<Max>());
    Define("min", Heap::GetInstance().Make<Min>());
    Define("abs", Heap::GetInstance().Make<Abs>());
    Define("define", Heap::GetInstance().Make<class Define>());
    Define("set!", Heap::GetInstance().Make<class Set>());
    Define("set-car!", Heap::GetInstance().Make<SetCar>());
    Define("set-cdr!", Heap::GetInstance().Make<SetCdr>());
    Define("if", Heap::GetInstance().Make<If>());
    Define("lambda", Heap::GetInstance().Make<ConstructLambda>());
    Define("quote", Heap::GetInstance().Make<Object>());

    Define("#t", Heap::GetInstance().Make<Symbol>("#t"));
    Define("#f", Heap::GetInstance().Make<Symbol>("#f"));
}

Node& Scope::ResolveSymbol(const std::string& symbol) {
    if (buf_.find(symbol) != buf_.end()) {
        return buf_[symbol];
    }
    if (prev_ == nullptr) {
        throw NameError("Symbol not found");
    }
    return prev_->ResolveSymbol(symbol);
}

void Scope::Set(const std::string& symbol, Node root) {
    if (buf_.find(symbol) == buf_.end()) {
        if (prev_ == nullptr) {
            throw NameError("Can't set value of undefined symbol");
        }
        prev_->Set(symbol, root);
        return;
    }
    Heap::GetInstance().RemoveRoot(buf_[symbol]);
    buf_[symbol] = root;
    Heap::GetInstance().AddRoot(buf_[symbol]);
}

Heap::Heap() {
    lifetime_root_ = Make<Object>();
}

void Heap::RunGC() {
    lifetime_root_->Mark();
    for (auto& ptr : storage_) {
        if (ptr && !ptr->mark_) {
            ptr.reset();
        }
    }
    std::vector<std::unique_ptr<Object>>::iterator object = find_if(
        storage_.begin(), storage_.end(), [&](std::unique_ptr<Object>& obj) { return !obj; });
    storage_.erase(std::remove(storage_.begin(), storage_.end(), *object));
    for (auto& ptr : storage_) {
        if (ptr) {
            ptr->mark_ = 0;
        }
    }
}

void Heap::Del() {
    lifetime_root_->Mark();
    for (auto& ptr : storage_) {
        if (ptr && ptr.get() != lifetime_root_ && ptr->mark_) {
            ptr.reset();
        }
    }
    std::vector<std::unique_ptr<Object>>::iterator object = find_if(
        storage_.begin(), storage_.end(), [&](std::unique_ptr<Object>& obj) { return !obj; });
    storage_.erase(std::remove(storage_.begin(), storage_.end(), *object));
    for (auto& ptr : storage_) {
        if (ptr) {
            ptr->mark_ = 0;
        }
    }
    lifetime_root_ = Make<Object>();
}

void Object::Mark() {
    Update();
    mark_ = 1;
    for (auto ptr : dependants_) {
        if (!(ptr->mark_)) {
            ptr->Mark();
        }
    }
}

Node IsNumber::Run(std::shared_ptr<Scope> scope, Node root) {
    auto args = ParseArguments(scope, root);
    RequireArgumentSize(args, 1, 1);
    return Bool(scope, Is<Number>(args[0]));
}

Node IsSymbol::Run(std::shared_ptr<Scope> scope, Node root) {
    auto args = ParseArguments(scope, root);
    RequireArgumentSize(args, 1, 1);
    return Bool(scope, Is<Symbol>(args[0]));
}

Node IsBoolean::Run(std::shared_ptr<Scope> scope, Node root) {
    auto args = ParseArguments(scope, root);
    RequireArgumentSize(args, 1, 1);
    if (!Is<Symbol>(args[0])) {
        return Bool(scope, 0);
    }
    const std::string& s = As<Symbol>(args[0])->GetName();
    return Bool(scope, s == "#f" || s == "#t");
}

Node IsPair::Run(std::shared_ptr<Scope> scope, Node root) {
    auto sz = [](Node lhs, Node) {
        return Heap::GetInstance().Make<Number>(As<Number>(lhs)->GetValue() + 1);
    };
    Node res = RuntimeParse(scope, Evaluate(scope, GetFirst(root)), &sz,
                            Heap::GetInstance().Make<Number>(0));
    return Bool(scope, As<Number>(res)->GetValue() == 2);
}

Node IsNull::Run(std::shared_ptr<Scope> scope, Node root) {
    auto args = ParseArguments(scope, root);
    RequireArgumentSize(args, 1, 1);
    return Bool(scope, !args[0] || IsNullCell(args[0]));
}

Node IsList::Run(std::shared_ptr<Scope> scope, Node root) {
    auto args = ParseArguments(scope, root);
    RequireArgumentSize(args, 1, 1);
    Node cur = args[0];
    while (cur && GetSecond(cur)) {
        if (!Is<Cell>(GetSecond(cur))) {
            return Bool(scope, 0);
        }
        cur = GetSecond(cur);
    }
    return Bool(scope, 1);
}

Node MakePair::Run(std::shared_ptr<Scope> scope, Node root) {
    auto args = ParseArguments(scope, root);
    RequireArgumentSize(args, 2, 2);
    return Heap::GetInstance().Make<Cell>(args[0], args[1]);
}

Node MakeList::Run(std::shared_ptr<Scope> scope, Node root) {
    auto args = ParseArguments(scope, root);
    if (args.empty()) {
        return nullptr;
    }
    args.push_back(nullptr);
    while (args.size() > 1) {
        Node b = args.back();
        args.pop_back();
        Node a = args.back();
        args.pop_back();
        args.push_back(Heap::GetInstance().Make<Cell>(a, b));
    }
    return args[0];
}

Node GetHead::Run(std::shared_ptr<Scope> scope, Node root) {
    auto args = ParseArguments(scope, root);
    RequireArgumentSize(args, 1, 1);
    if (!args[0]) {
        throw RuntimeError("Can't get head of empty list");
    }
    return GetFirst(args[0]);
}

Node GetTail::Run(std::shared_ptr<Scope> scope, Node root) {
    auto args = ParseArguments(scope, root);
    RequireArgumentSize(args, 1, 1);
    if (!args[0]) {
        throw RuntimeError("Can't get tail of empty list");
    }
    return GetSecond(args[0]);
}

Node Get::Run(std::shared_ptr<Scope> scope, Node root) {
    auto args = ParseArguments(scope, root);
    RequireArgumentSize(args, 2, 2);
    Node cur = args[0];
    size_t ind = As<Number>(args[1])->GetValue();
    for (; ind > 0 && Is<Cell>(cur); --ind) {
        cur = GetSecond(cur);
    }
    if (!Is<Cell>(cur)) {
        throw RuntimeError("List index out of bounds");
    }
    return GetFirst(cur);
}

Node GetSuffix::Run(std::shared_ptr<Scope> scope, Node root) {
    auto args = ParseArguments(scope, root);
    RequireArgumentSize(args, 2, 2);
    Node cur = args[0];
    size_t ind = As<Number>(args[1])->GetValue();
    for (; ind > 0 && Is<Cell>(cur); --ind) {
        cur = GetSecond(cur);
    }
    if (ind > 0 && !Is<Cell>(cur)) {
        throw RuntimeError("List index out of bounds");
    }
    return cur;
}

Node Not::Run(std::shared_ptr<Scope> scope, Node root) {
    if (As<Symbol>(IsBoolean().Run(scope, root))->GetName() == "#f") {
        return Bool(scope, 0);
    }
    auto args = ParseArguments(scope, root);
    RequireArgumentSize(args, 1, 1);
    return Bool(scope, !IsTrue(args[0]));
}

Node And::Run(std::shared_ptr<Scope> scope, Node root) {
    if (!root) {
        return Bool(scope, 1);
    }
    Node lst;
    auto func = [&lst, &scope](Node, Node rhs) {
        lst = rhs;
        return Bool(scope, IsTrue(rhs));
    };
    if (IsTrue(RuntimeParse(scope, root, &func, Bool(scope, 1), Bool(scope, 0)))) {
        return lst;
    }
    return Bool(scope, 0);
}

Node Or::Run(std::shared_ptr<Scope> scope, Node root) {
    if (!root) {
        return Bool(scope, 0);
    }
    Node lst;
    auto func = [&lst, &scope](Node, Node rhs) {
        lst = rhs;
        return Bool(scope, IsTrue(rhs));
    };
    if (IsTrue(RuntimeParse(scope, root, &func, Bool(scope, 0), Bool(scope, 1)))) {
        return lst;
    }
    return Bool(scope, 0);
}

template <typename Func>
Node ProxyCompare(std::shared_ptr<Scope> scope, std::vector<Node>& args, Func func) {
    RequireArgType<Number>(args);
    if (args.empty()) {
        return Bool(scope, 1);
    }
    for (size_t i = 1; i < args.size(); ++i) {
        if (!func(args[i - 1], args[i])) {
            return Bool(scope, 0);
        }
    }
    return Bool(scope, 1);
}

Node IsEqual::Run(std::shared_ptr<Scope> scope, Node root) {
    auto args = ParseArguments(scope, root);
    return ProxyCompare(scope, args, NodeEq);
}

Node IsGreater::Run(std::shared_ptr<Scope> scope, Node root) {
    auto args = ParseArguments(scope, root);
    return ProxyCompare(scope, args,
                        [](Node lhs, Node rhs) { return GetValue(lhs) > GetValue(rhs); });
}

Node IsSmaller::Run(std::shared_ptr<Scope> scope, Node root) {
    auto args = ParseArguments(scope, root);
    return ProxyCompare(scope, args,
                        [](Node lhs, Node rhs) { return GetValue(lhs) < GetValue(rhs); });
}

Node IsGeq::Run(std::shared_ptr<Scope> scope, Node root) {
    auto args = ParseArguments(scope, root);
    return ProxyCompare(scope, args,
                        [](Node lhs, Node rhs) { return GetValue(lhs) >= GetValue(rhs); });
}

Node IsLeq::Run(std::shared_ptr<Scope> scope, Node root) {
    auto args = ParseArguments(scope, root);
    return ProxyCompare(scope, args,
                        [](Node lhs, Node rhs) { return GetValue(lhs) <= GetValue(rhs); });
}

template <typename Func>
Node ProxyArithmetic(std::vector<Node>& args, Func func, bool have_neut = 0,
                     Node neutral = nullptr) {
    RequireArgType<Number>(args);
    if (args.empty()) {
        if (have_neut) {
            return neutral;
        }
        throw RuntimeError("No neutral element for arithmetic operation");
    }
    Node res = args[0];
    for (size_t i = 1; i < args.size(); ++i) {
        res = func(res, args[i]);
    }
    return res;
}

Node Plus::Run(std::shared_ptr<Scope> scope, Node root) {
    auto args = ParseArguments(scope, root);
    auto func = [](Node lhs, Node rhs) {
        return Heap::GetInstance().Make<Number>(GetValue(lhs) + GetValue(rhs));
    };
    return ProxyArithmetic(args, func, 1, Heap::GetInstance().Make<Number>(0));
}

Node Minus::Run(std::shared_ptr<Scope> scope, Node root) {
    auto args = ParseArguments(scope, root);
    auto func = [](Node lhs, Node rhs) {
        return Heap::GetInstance().Make<Number>(GetValue(lhs) - GetValue(rhs));
    };
    return ProxyArithmetic(args, func);
}

Node Mult::Run(std::shared_ptr<Scope> scope, Node root) {
    auto args = ParseArguments(scope, root);
    auto func = [](Node lhs, Node rhs) {
        return Heap::GetInstance().Make<Number>(GetValue(lhs) * GetValue(rhs));
    };
    return ProxyArithmetic(args, func, 1, Heap::GetInstance().Make<Number>(1));
}

Node Div::Run(std::shared_ptr<Scope> scope, Node root) {
    auto args = ParseArguments(scope, root);
    auto func = [](Node lhs, Node rhs) {
        return Heap::GetInstance().Make<Number>(GetValue(lhs) / GetValue(rhs));
    };
    return ProxyArithmetic(args, func);
}

Node Max::Run(std::shared_ptr<Scope> scope, Node root) {
    auto args = ParseArguments(scope, root);
    auto func = [](Node lhs, Node rhs) {
        return Heap::GetInstance().Make<Number>(std::max(GetValue(lhs), GetValue(rhs)));
    };
    return ProxyArithmetic(args, func);
}

Node Min::Run(std::shared_ptr<Scope> scope, Node root) {
    auto args = ParseArguments(scope, root);
    auto func = [](Node lhs, Node rhs) {
        return Heap::GetInstance().Make<Number>(std::min(GetValue(lhs), GetValue(rhs)));
    };
    return ProxyArithmetic(args, func);
}

Node Abs::Run(std::shared_ptr<Scope> scope, Node root) {
    auto args = ParseArguments(scope, root);
    RequireArgType<Number>(args);
    RequireArgumentSize(args, 1, 1);
    return Heap::GetInstance().Make<Number>(std::abs(GetValue(args[0])));
}

Node ToCell(Node root) {
    if (!Is<Cell>(root)) {
        return Heap::GetInstance().Make<Cell>(root, nullptr);
    }
    return root;
}

Node Define::Run(std::shared_ptr<Scope> scope, Node root) {
    if (!Is<Cell>(root)) {
        throw SyntaxError("Define requires 2 arguments");
    }

    // lambda sugar case
    if (Is<Cell>(GetFirst(root))) {
        auto args_node = GetSecond(GetFirst(root));
        auto body = GetSecond(root);
        scope->Define(
            GetName(GetFirst(GetFirst(root))),
            ConstructLambda().Run(scope, Heap::GetInstance().Make<Cell>(args_node, ToCell(body))));
        return nullptr;
    }

    if (!Is<Symbol>(GetFirst(root))) {
        throw SyntaxError("Bad argument to define");
    }
    if (GetSecond(GetSecond(root)) != nullptr) {
        throw SyntaxError("Define requires 2 arguments");
    }
    scope->Define(GetName(GetFirst(root)), Evaluate(scope, GetFirst(GetSecond(root))));
    return nullptr;
}

Node Set::Run(std::shared_ptr<Scope> scope, Node root) {
    if (!Is<Cell>(root)) {
        throw SyntaxError("Set requires 2 arguments");
    }
    if (!Is<Symbol>(GetFirst(root))) {
        throw SyntaxError("Bad argument to set");
    }
    if (GetSecond(GetSecond(root)) != nullptr) {
        throw SyntaxError("Set requires 2 arguments");
    }
    scope->Set(GetName(GetFirst(root)), Evaluate(scope, GetFirst(GetSecond(root))));
    return nullptr;
}

Node SetCar::Run(std::shared_ptr<Scope> scope, Node root) {
    if (!Is<Cell>(root) || !Is<Cell>(GetSecond(root)) || GetSecond(GetSecond(root))) {
        throw SyntaxError("set-car! requires 2 arguments");
    }
    auto obj = Evaluate(scope, GetFirst(root));
    GetFirst(obj) = Evaluate(scope, GetFirst(GetSecond(root)));
    obj->Update();
    return nullptr;
}

Node SetCdr::Run(std::shared_ptr<Scope> scope, Node root) {
    if (!Is<Cell>(root) || !Is<Cell>(GetSecond(root)) || GetSecond(GetSecond(root))) {
        throw SyntaxError("set-cdr! requires 2 arguments");
    }
    auto obj = Evaluate(scope, GetFirst(root));
    GetSecond(obj) = Evaluate(scope, GetFirst(GetSecond(root)));
    return nullptr;
}

Node If::Run(std::shared_ptr<Scope> scope, Node root) {
    if (!Is<Cell>(root) || !GetSecond(root) || !GetFirst(GetSecond(root)) ||
        (GetSecond(GetSecond(root)) && GetSecond(GetSecond(GetSecond(root))))) {
        throw SyntaxError("if invalid number of arguments, requires 2 or 3");
    }
    auto condition = GetFirst(root);
    if (IsTrue(Evaluate(scope, condition))) {
        return Evaluate(scope, GetFirst(GetSecond(root)));
    }
    if (GetSecond(GetSecond(root))) {
        return Evaluate(scope, GetFirst(GetSecond(GetSecond(root))));
    }
    return nullptr;
}

Node ConstructLambda::Run(std::shared_ptr<Scope> scope, Node root) {
    if (!Is<Cell>(root)) {
        throw SyntaxError("Invalid number of arguments for lambda construction");
    }
    if (GetFirst(root) && !Is<Cell>(GetFirst(root))) {
        throw SyntaxError("Argument list required for lambda construction");
    }
    if (!Is<Cell>(GetSecond(root))) {
        throw SyntaxError("Can't create empty lambda");
    }
    GetFirst(root);
    std::vector<Node> args = ParseArgumentsNoEval(GetFirst(root));
    RequireArgType<Symbol>(args);
    return Heap::GetInstance().Make<Lambda>(scope, args, GetSecond(root));
}

Node Lambda::Run(std::shared_ptr<Scope> scope, Node root) {
    local_scope_ = std::shared_ptr<Scope>(new Scope(local_scope_));
    for (auto obj : args_) {
        if (!root) {
            throw RuntimeError("Incorrect number of arguments for lambda function");
        }
        local_scope_->Define(GetName(obj), Evaluate(scope, GetFirst(root)));
        root = GetSecond(root);
    }
    if (root) {
        throw RuntimeError("Incorrect number of arguments for lambda function");
    }
    Node cur = calc_;
    Node lst = nullptr;
    while (cur) {
        lst = Evaluate(local_scope_, GetFirst(cur));
        cur = GetSecond(cur);
    }
    local_scope_ = local_scope_->GetPrev();
    return lst;
}

/*

*/
