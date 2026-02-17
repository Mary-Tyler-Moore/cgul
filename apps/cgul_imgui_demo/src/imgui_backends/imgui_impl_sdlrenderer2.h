#pragma once

#include <SDL.h>

#include <imgui.h>

IMGUI_IMPL_API bool ImGui_ImplSDLRenderer2_Init(SDL_Renderer* renderer);
IMGUI_IMPL_API void ImGui_ImplSDLRenderer2_Shutdown();
IMGUI_IMPL_API void ImGui_ImplSDLRenderer2_NewFrame();
IMGUI_IMPL_API void ImGui_ImplSDLRenderer2_RenderDrawData(ImDrawData* draw_data);
