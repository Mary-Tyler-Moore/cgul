#pragma once

#include <string>

#include "cgul/io/cgul_document.h"

namespace cgul {

bool Equal(const RectI& a, const RectI& b);
bool Equal(const Widget& a, const Widget& b);
bool Equal(const CgulDocument& a, const CgulDocument& b, std::string* outDiff);

}  // namespace cgul
