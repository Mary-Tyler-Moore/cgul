#pragma once

#include <filesystem>

namespace cgul_demo {
namespace paths {

std::filesystem::path ResolveExecutableDir(const char* argv0);
std::filesystem::path ResolveAppDir(const char* argv0);

}  // namespace paths
}  // namespace cgul_demo
