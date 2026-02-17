#pragma once

#include <filesystem>
#include <string>

#include <SDL.h>

namespace tiled {

SDL_Texture* LoadAtlasTexture(SDL_Renderer* renderer, const std::filesystem::path& path,
    int* outWidth, int* outHeight, std::string* error);

}  // namespace tiled
