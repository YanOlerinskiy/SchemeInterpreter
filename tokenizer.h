#pragma once

#include <variant>
#include <optional>
#include <istream>
#include <string>

struct SymbolToken {
    std::string name_;

    SymbolToken(const std::string& name) : name_(name) {
    }

    bool operator==(const SymbolToken& other) const;
};

struct QuoteToken {
    bool operator==(const QuoteToken&) const;
};

struct DotToken {
    bool operator==(const DotToken&) const;
};

enum class BracketToken { OPEN, CLOSE };

struct ConstantToken {
    int64_t value_;

    ConstantToken(const int64_t value) : value_(value) {
    }

    bool operator==(const ConstantToken& other) const;
};

using Token = std::variant<ConstantToken, BracketToken, SymbolToken, QuoteToken, DotToken>;

class Tokenizer {
public:
    Tokenizer(std::istream* in);

    bool IsEnd();

    void Next();

    const Token& GetToken();

private:
    std::istream* stream_;
    Token state_ = DotToken();
    bool end_ = 0;
};
