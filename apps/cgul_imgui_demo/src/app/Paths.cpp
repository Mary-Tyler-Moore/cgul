#include "app/Paths.hpp"

#include <system_error>

namespace cgul_demo {
namespace paths {
namespace {

std::filesystem::path FallbackCurrentPath() {
    std::error_code ec;
    const std::filesystem::path cwd = std::filesystem::current_path(ec);
    if (!ec && !cwd.empty()) {
        return cwd;
    }
    return std::filesystem::path(".");
}

}  // namespace

std::filesystem::path ResolveExecutableDir(const char* argv0) {
    if (argv0 != nullptr && argv0[0] != '\0') {
        const std::filesystem::path argv0Path(argv0);
        if (argv0Path.has_parent_path()) {
            std::error_code ec;
            const std::filesystem::path absolutePath = std::filesystem::absolute(argv0Path, ec);
            if (!ec && !absolutePath.empty() && absolutePath.has_parent_path()) {
                return absolutePath.parent_path();
            }
        }
    }
    return FallbackCurrentPath();
}

std::filesystem::path ResolveAppDir(const char* argv0) {
    std::error_code ec;
    const std::filesystem::path executableDir = ResolveExecutableDir(argv0);

    if (std::filesystem::is_directory(executableDir / "assets", ec)) {
        return executableDir;
    }

    ec.clear();
    std::filesystem::path current = executableDir;
    while (!current.empty()) {
        const std::filesystem::path candidate = current / "apps" / "cgul_imgui_demo";
        if (std::filesystem::is_directory(candidate / "assets", ec)) {
            return candidate;
        }
        ec.clear();

        const std::filesystem::path parent = current.parent_path();
        if (parent.empty() || parent == current) {
            break;
        }
        current = parent;
    }

    return executableDir;
}

}  // namespace paths
}  // namespace cgul_demo
