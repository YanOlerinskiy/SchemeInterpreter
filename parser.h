#pragma once

#include <memory>

#include "object.h"
#include "tokenizer.h"

Object* Read(Tokenizer* tokenizer);

Object* ReadList(Tokenizer* tokenizer);

Object* ReadFullS(const std::string& str);