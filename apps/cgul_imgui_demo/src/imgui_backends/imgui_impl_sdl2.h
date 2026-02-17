#pragma once

#include <SDL.h>

#include <imgui.h>

IMGUI_IMPL_API bool ImGui_ImplSDL2_InitForSDLRenderer(SDL_Window* window, SDL_Renderer* renderer);
IMGUI_IMPL_API void ImGui_ImplSDL2_Shutdown();
IMGUI_IMPL_API void ImGui_ImplSDL2_NewFrame();
IMGUI_IMPL_API bool ImGui_ImplSDL2_ProcessEvent(const SDL_Event* event);
