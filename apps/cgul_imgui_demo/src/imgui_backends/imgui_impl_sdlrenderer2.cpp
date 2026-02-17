#include "imgui_impl_sdlrenderer2.h"

#include <SDL.h>

#include <imgui.h>

#include <cstddef>

namespace {

SDL_Renderer* g_Renderer = nullptr;
SDL_Texture* g_FontTexture = nullptr;

bool CreateFontsTexture() {
    ImGuiIO& io = ImGui::GetIO();
    unsigned char* pixels = nullptr;
    int width = 0;
    int height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    if (!pixels || width <= 0 || height <= 0) {
        return false;
    }

    SDL_Texture* texture = SDL_CreateTexture(g_Renderer, SDL_PIXELFORMAT_RGBA32,
        SDL_TEXTUREACCESS_STATIC, width, height);
    if (!texture) {
        return false;
    }

    SDL_UpdateTexture(texture, nullptr, pixels, width * 4);
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    io.Fonts->SetTexID(reinterpret_cast<ImTextureID>(texture));
    g_FontTexture = texture;
    return true;
}

void SetupRenderState() {
    SDL_SetRenderDrawBlendMode(g_Renderer, SDL_BLENDMODE_BLEND);
    SDL_RenderSetClipRect(g_Renderer, nullptr);
}

}  // namespace

bool ImGui_ImplSDLRenderer2_Init(SDL_Renderer* renderer) {
    g_Renderer = renderer;
    ImGuiIO& io = ImGui::GetIO();
    io.BackendRendererName = "imgui_impl_sdlrenderer2_custom";
    return CreateFontsTexture();
}

void ImGui_ImplSDLRenderer2_Shutdown() {
    if (g_FontTexture) {
        SDL_DestroyTexture(g_FontTexture);
        g_FontTexture = nullptr;
    }
    g_Renderer = nullptr;
}

void ImGui_ImplSDLRenderer2_NewFrame() {
    if (!g_FontTexture) {
        CreateFontsTexture();
    }
}

void ImGui_ImplSDLRenderer2_RenderDrawData(ImDrawData* draw_data) {
    if (!g_Renderer || !draw_data) {
        return;
    }

    if (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f) {
        return;
    }

    SetupRenderState();

    const ImVec2 clipOffset = draw_data->DisplayPos;
    const ImVec2 clipScale = draw_data->FramebufferScale;

    for (int n = 0; n < draw_data->CmdListsCount; ++n) {
        const ImDrawList* cmdList = draw_data->CmdLists[n];
        const ImDrawVert* vtxBuffer = cmdList->VtxBuffer.Data;
        const ImDrawIdx* idxBuffer = cmdList->IdxBuffer.Data;

        for (int cmdI = 0; cmdI < cmdList->CmdBuffer.Size; ++cmdI) {
            const ImDrawCmd* cmd = &cmdList->CmdBuffer[cmdI];
            if (cmd->UserCallback) {
                if (cmd->UserCallback == ImDrawCallback_ResetRenderState) {
                    SetupRenderState();
                } else {
                    cmd->UserCallback(cmdList, cmd);
                }
                continue;
            }

            ImVec4 clipRect = cmd->ClipRect;
            clipRect.x = (clipRect.x - clipOffset.x) * clipScale.x;
            clipRect.y = (clipRect.y - clipOffset.y) * clipScale.y;
            clipRect.z = (clipRect.z - clipOffset.x) * clipScale.x;
            clipRect.w = (clipRect.w - clipOffset.y) * clipScale.y;

            if (clipRect.x >= clipRect.z || clipRect.y >= clipRect.w) {
                continue;
            }

            SDL_Rect sdlRect;
            sdlRect.x = static_cast<int>(clipRect.x);
            sdlRect.y = static_cast<int>(clipRect.y);
            sdlRect.w = static_cast<int>(clipRect.z - clipRect.x);
            sdlRect.h = static_cast<int>(clipRect.w - clipRect.y);
            SDL_RenderSetClipRect(g_Renderer, &sdlRect);

            SDL_Texture* texture = reinterpret_cast<SDL_Texture*>(cmd->GetTexID());

            SDL_RenderGeometryRaw(g_Renderer, texture,
                reinterpret_cast<const float*>(&vtxBuffer->pos), sizeof(ImDrawVert),
                reinterpret_cast<const SDL_Color*>(&vtxBuffer->col), sizeof(ImDrawVert),
                reinterpret_cast<const float*>(&vtxBuffer->uv), sizeof(ImDrawVert),
                cmdList->VtxBuffer.Size,
                idxBuffer + cmd->IdxOffset, static_cast<int>(cmd->ElemCount),
                sizeof(ImDrawIdx));
        }
    }

    SDL_RenderSetClipRect(g_Renderer, nullptr);
}
