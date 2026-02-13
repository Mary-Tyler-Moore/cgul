#pragma once

#include <string>

#include "cgul/io/cgul_document.h"

namespace cgul {

bool Validate(const CgulDocument& doc, std::string* outError);

}  // namespace cgul
