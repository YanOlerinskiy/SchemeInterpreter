#include "scheme.h"

#include "parser.h"
#include "error.h"

#include <map>
#include <vector>

Node Evaluate(std::shared_ptr<Scope> scope, Node root) {
    if (!root) {
        throw RuntimeError("Evaluating null not allowed");
    }

    if (Is<Number>(root)) {
        return root;
    }
    if (Is<Symbol>(root)) {
        return scope->ResolveSymbol(GetName(root));
    }

    if (!Is<Cell>(root)) {
        throw RuntimeError("Unknown object type to evaluate");
    }

    if (!Is<Symbol>(GetFirst(root))) {
        auto func = Evaluate(scope, GetFirst(root));
        if (Is<Lambda>(func)) {
            return func->Run(scope, GetSecond(root));
        }
        throw RuntimeError("Function name has to be a string");
    }

    const std::string& func = As<Symbol>(GetFirst(root))->GetName();

    // exceptionally special case
    if (func == "quote") {
        return GetFirst(GetSecond(root));
    }

    return scope->ResolveSymbol(func)->Run(scope, GetSecond(root));
}

std::string Convert(Node root);

void ExpandIntoList(Node root, std::vector<std::string>& ans) {
    if (!root) {
        return;
    }
    if (!Is<Cell>(root)) {
        ans.push_back(Convert(root));
        return;
    }
    if (IsNullCell(root)) {
        ans.push_back(Convert(GetFirst(root)));
        return;
    }
    ExpandIntoList(GetFirst(root), ans);
    if (GetSecond(root) && !Is<Cell>(GetSecond(root))) {
        ans.push_back(".");
    }
    ExpandIntoList(GetSecond(root), ans);
}

std::string Convert(Node root) {
    if (!root) {
        return "()";
    }
    if (Is<Number>(root)) {
        return std::to_string(GetValue(root));
    }
    if (Is<Symbol>(root)) {
        return GetName(root);
    }
    if (!Is<Cell>(root)) {
        throw SyntaxError("Invalid syntax");
    }
    std::vector<std::string> objects;
    ExpandIntoList(root, objects);
    std::string ans{'('};
    for (size_t i = 0; i < objects.size(); ++i) {
        if (i) {
            ans.push_back(' ');
        }
        ans += objects[i];
    }
    ans.push_back(')');
    return ans;
}

std::string Interpreter::Run(const std::string& program) {
    auto res = Convert(Evaluate(global_scope_, ReadFullS(program)));
    Heap::GetInstance().RunGC();
    return res;
}
