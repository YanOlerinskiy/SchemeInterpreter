#include "parser.h"

#include "error.h"

#include <sstream>

void CheckEnd(Tokenizer* tokenizer) {
    if (tokenizer->IsEnd()) {
        Heap::GetInstance().RunGC();
        throw SyntaxError("Invalid syntax");
    }
}

void Move(Tokenizer* tokenizer) {
    tokenizer->Next();
    CheckEnd(tokenizer);
}

auto GetDot(Tokenizer* tokenizer) {
    return std::get_if<DotToken>(&(tokenizer->GetToken()));
};

auto GetBracket(Tokenizer* tokenizer) {
    return std::get_if<BracketToken>(&(tokenizer->GetToken()));
};

bool IsOpen(Tokenizer* tokenizer) {
    return GetBracket(tokenizer) != nullptr && *GetBracket(tokenizer) == BracketToken::OPEN;
}

bool IsClosed(Tokenizer* tokenizer) {
    return GetBracket(tokenizer) != nullptr && *GetBracket(tokenizer) == BracketToken::CLOSE;
}

Object* Read(Tokenizer* tokenizer) {
    CheckEnd(tokenizer);
    Token token = tokenizer->GetToken();
    if (IsOpen(tokenizer)) {
        return ReadList(tokenizer);
    }
    if (auto obj = std::get_if<ConstantToken>(&token)) {
        tokenizer->Next();
        return Heap::GetInstance().Make<Number>(obj->value_);
    }
    if (auto obj = std::get_if<SymbolToken>(&token)) {
        tokenizer->Next();
        return Heap::GetInstance().Make<Symbol>(obj->name_);
    }
    if (std::get_if<QuoteToken>(&token)) {
        tokenizer->Next();
        return Heap::GetInstance().Make<Cell>(
            Heap::GetInstance().Make<Symbol>("quote"),
            Heap::GetInstance().Make<Cell>(Read(tokenizer), nullptr));
    }
    Heap::GetInstance().RunGC();
    throw SyntaxError("Invalid syntax");
}

Object* ReadList(Tokenizer* tokenizer) {
    if (tokenizer->IsEnd() || !IsOpen(tokenizer)) {
        Heap::GetInstance().RunGC();
        throw SyntaxError("Invalid syntax");
    }
    Move(tokenizer);
    if (IsClosed(tokenizer)) {
        tokenizer->Next();
        return nullptr;
    }

    Object* res = Heap::GetInstance().Make<Cell>();
    Object* cur = res;
    while (!IsClosed(tokenizer)) {
        GetFirst(cur) = Read(tokenizer);

        if (!IsClosed(tokenizer)) {
            if (GetDot(tokenizer)) {
                Move(tokenizer);
                GetSecond(cur) = Read(tokenizer);
                break;
            } else {
                GetSecond(cur) = Heap::GetInstance().Make<Cell>();
                cur = GetSecond(cur);
            }
        }
    }

    if (tokenizer->IsEnd() || !IsClosed(tokenizer)) {
        Heap::GetInstance().RunGC();
        throw SyntaxError("Invalid syntax");
    }
    tokenizer->Next();
    return res;
}

Object* ReadFullS(const std::string& str) {
    std::stringstream ss{str};
    Tokenizer tokenizer{&ss};

    Object* obj = Read(&tokenizer);
    if (!tokenizer.IsEnd()) {
        Heap::GetInstance().RunGC();
        throw SyntaxError("Single expression required");
    }
    return obj;
}
