#include "cgul/core/equality.h"
#include "cgul/io/cgul_document.h"
#include "cgul/validate/validate.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace {

namespace fs = std::filesystem;

void PrintFailure(const std::string& message) {
  std::cerr << message << '\n';
}

bool HasCgulExtension(const fs::path& path) {
  return path.extension() == ".cgul";
}

int RunSmoke() {
  const fs::path examplesDir = fs::path("schemas") / "examples";

  std::error_code ec;
  if (!fs::exists(examplesDir, ec) || ec) {
    PrintFailure("FAIL discover " + examplesDir.string() + ": directory does not exist");
    return 1;
  }
  if (!fs::is_directory(examplesDir, ec) || ec) {
    PrintFailure("FAIL discover " + examplesDir.string() + ": not a directory");
    return 1;
  }

  std::vector<fs::path> exampleFiles;
  for (fs::directory_iterator it(examplesDir, ec); !ec && it != fs::directory_iterator(); ++it) {
    if (!it->is_regular_file()) {
      continue;
    }
    const fs::path candidate = it->path();
    if (HasCgulExtension(candidate)) {
      exampleFiles.push_back(candidate);
    }
  }
  if (ec) {
    PrintFailure("FAIL discover " + examplesDir.string() + ": " + ec.message());
    return 1;
  }

  std::sort(exampleFiles.begin(), exampleFiles.end(),
            [](const fs::path& a, const fs::path& b) { return a.filename().string() < b.filename().string(); });

  if (exampleFiles.empty()) {
    PrintFailure("FAIL discover " + examplesDir.string() + ": no .cgul files found");
    return 1;
  }

  fs::path tempDir = fs::temp_directory_path(ec);
  if (ec) {
    PrintFailure("FAIL tempdir: " + ec.message());
    return 1;
  }

  const auto nowTicks = std::chrono::steady_clock::now().time_since_epoch().count();

  for (size_t i = 0; i < exampleFiles.size(); ++i) {
    const fs::path& sourcePath = exampleFiles[i];

    cgul::CgulDocument doc;
    std::string error;
    if (!cgul::LoadCgulFile(sourcePath.string(), &doc, &error)) {
      PrintFailure("FAIL load " + sourcePath.string() + ": " + error);
      return 1;
    }
    if (!cgul::Validate(doc, &error)) {
      PrintFailure("FAIL validate " + sourcePath.string() + ": " + error);
      return 1;
    }

    const std::string tempFileName =
        "cgul_roundtrip_" + sourcePath.stem().string() + "_" + std::to_string(i) + "_" +
        std::to_string(static_cast<long long>(nowTicks)) + ".cgul";
    const fs::path tempPath = tempDir / tempFileName;

    if (!cgul::SaveCgulFile(tempPath.string(), doc, &error)) {
      PrintFailure("FAIL save " + tempPath.string() + ": " + error);
      return 1;
    }

    cgul::CgulDocument reloaded;
    if (!cgul::LoadCgulFile(tempPath.string(), &reloaded, &error)) {
      PrintFailure("FAIL reload " + tempPath.string() + ": " + error);
      return 1;
    }
    if (!cgul::Validate(reloaded, &error)) {
      PrintFailure("FAIL validate(reloaded) " + tempPath.string() + ": " + error);
      return 1;
    }

    std::string diff;
    if (!cgul::Equal(doc, reloaded, &diff)) {
      PrintFailure("FAIL equal " + sourcePath.string() + ": " + diff);
      return 1;
    }

    fs::remove(tempPath, ec);
    ec.clear();
  }

  std::cout << "PASS cgul_smoke: " << exampleFiles.size() << " files ok\n";
  return 0;
}

}  // namespace

int main() {
  return RunSmoke();
}
