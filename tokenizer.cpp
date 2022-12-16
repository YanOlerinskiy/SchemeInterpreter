#include "tokenizer.h"
#include "error.h"

#include <cctype>
#include <array>
#include <algorithm>

bool SymbolToken::operator==(const SymbolToken& other) const {
    return name_ == other.name_;
}

bool QuoteToken::operator==(const QuoteToken&) const {
    return 1;
}

bool DotToken::operator==(const DotToken&) const {
    return 1;
}

bool ConstantToken::operator==(const ConstantToken& other) const {
    return value_ == other.value_;
}

bool ShouldIgnore(int c) {
    return c == ' ' || c == 10;
}

Tokenizer::Tokenizer(std::istream* in) : stream_(in) {
    while (ShouldIgnore(stream_->peek())) {
        stream_->get();
    }
    stream_->clear();
    Next();
}

bool Tokenizer::IsEnd() {
    return end_;
}

void ReadBool(std::istream* stream, std::string& state) {
    for (int i = 0; i < 2; i++) {
        char c = stream->get();
        if (c == std::istream::traits_type::eof()) {
            throw SyntaxError("Unknown sequence");
        }
        state.push_back(c);
    }
}

bool IsNum(std::istream* stream) {
    if (stream->peek() != '-' && stream->peek() != '+') {
        return 0;
    }
    stream->get();
    if (isdigit(stream->peek())) {
        stream->unget();
        return 1;
    }
    stream->unget();
    return 0;
}

void ReadNum(std::istream* stream, std::string& state) {
    state.push_back(stream->get());
    while (isdigit(stream->peek())) {
        state.push_back(stream->get());
    }
}

bool ValidFront(char c) {
    // [a-zA-Z<=>*/#]
    return isalpha(c) || c == '<' || c == '=' || c == '>' || c == '*' || c == '/' || c == '#';
}

bool ValidSymb(char c) {
    // [a-zA-Z<=>*/#0-9?!-]
    return ValidFront(c) || isdigit(c) || c == '?' || c == '!' || c == '-';
}

void ReadSequence(std::istream* stream, std::string& state) {
    if (!ValidFront(stream->peek())) {
        throw SyntaxError("Unknown sequence");
    }
    state.push_back(stream->get());
    while (ValidSymb(stream->peek())) {
        state.push_back(stream->get());
    }
}

const size_t kSYMB = 8;
const std::array<std::string, kSYMB> kSymbols = {"+", "-", ".", "'", "(", ")", "#f", "#t"};
const std::array<Token, kSYMB> kSymbolTokens = {
    SymbolToken("+"),   SymbolToken("-"),    DotToken(),        QuoteToken(),
    BracketToken::OPEN, BracketToken::CLOSE, SymbolToken("#f"), SymbolToken("#t")};

void Tokenizer::Next() {
    stream_->peek();
    if (stream_->eof()) {
        end_ = 1;
        return;
    }

    std::string state;

    auto it =
        find(kSymbols.begin(), kSymbols.end(), std::string{static_cast<char>(stream_->peek())});
    if (it != kSymbols.end() && !IsNum(stream_)) {
        state_ = kSymbolTokens[it - kSymbols.begin()];
        stream_->get();
    } else if (stream_->peek() == '-' || stream_->peek() == '+' || isdigit(stream_->peek())) {
        ReadNum(stream_, state);
        state_ = ConstantToken(std::stoi(state));
    } else {
        ReadSequence(stream_, state);
        state_ = SymbolToken(state);
    }

    while (ShouldIgnore(stream_->peek())) {
        stream_->get();
    }
    stream_->clear();
}

const Token& Tokenizer::GetToken() {
    if (IsEnd()) {
        state_ = SymbolToken("");
    }
    return state_;
}