#include "chunkexporter/tiled/AtlasTexture.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "third_party/stb/stb_image.h"

#include <SDL.h>

#include <string>

namespace tiled {

SDL_Texture* LoadAtlasTexture(SDL_Renderer* renderer, const std::filesystem::path& path,
    int* outWidth, int* outHeight, std::string* error) {
    if (!renderer) {
        if (error) {
            *error = "Missing SDL renderer for atlas load.";
        }
        return nullptr;
    }

    int width = 0;
    int height = 0;
    int channels = 0;
    stbi_uc* pixels = stbi_load(path.string().c_str(), &width, &height, &channels, 4);
    if (!pixels) {
        if (error) {
            *error = "Failed to decode atlas PNG: " + path.string();
        }
        return nullptr;
    }

    size_t colorKeyToAlphaCount = 0;
    const size_t pixelCount = static_cast<size_t>(width) * static_cast<size_t>(height);
    for (size_t i = 0; i < pixelCount; ++i) {
        stbi_uc* px = pixels + (i * 4);
        const stbi_uc r = px[0];
        const stbi_uc g = px[1];
        const stbi_uc b = px[2];
        if (r == 255 && g == 0 && b == 255) {
            px[0] = 0;
            px[1] = 0;
            px[2] = 0;
            px[3] = 0;
            ++colorKeyToAlphaCount;
        }
    }
    SDL_Log("Atlas colorkey pixels -> alpha: %zu (%s)", colorKeyToAlphaCount, path.string().c_str());

    SDL_Surface* surface = SDL_CreateRGBSurfaceFrom(
        pixels, width, height, 32, width * 4, 0x000000FF, 0x0000FF00, 0x00FF0000,
        0xFF000000);
    if (!surface) {
        if (error) {
            *error = std::string("SDL_CreateRGBSurfaceFrom failed: ") + SDL_GetError();
        }
        stbi_image_free(pixels);
        return nullptr;
    }

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (!texture) {
        if (error) {
            *error = std::string("SDL_CreateTextureFromSurface failed: ") + SDL_GetError();
        }
        SDL_FreeSurface(surface);
        stbi_image_free(pixels);
        return nullptr;
    }

    SDL_FreeSurface(surface);
    stbi_image_free(pixels);
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
#if SDL_VERSION_ATLEAST(2, 0, 12)
    SDL_SetTextureScaleMode(texture, SDL_ScaleModeNearest);
#endif

    if (outWidth) {
        *outWidth = width;
    }
    if (outHeight) {
        *outHeight = height;
    }

    return texture;
}

}  // namespace tiled
