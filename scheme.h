#pragma once

#include "object.h"

#include <string>

Node Evaluate(std::shared_ptr<Scope> scope, Node root);

class Interpreter {
public:
    Interpreter() : global_scope_(new Scope(nullptr)) {
    }

    ~Interpreter() {
        global_scope_.reset();
        Heap::GetInstance().Del();
    }

    std::string Run(const std::string& program);

private:
    std::shared_ptr<Scope> global_scope_;
};
