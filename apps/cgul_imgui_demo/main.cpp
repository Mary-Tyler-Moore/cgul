#include <SDL.h>

#include <imgui.h>

#include "app/DemoPersistence.hpp"
#include "app/Paths.hpp"
#include "imgui_backends/imgui_impl_sdl2.h"
#include "imgui_backends/imgui_impl_sdlrenderer2.h"
#include "render/ArtRenderer.hpp"
#include "render/CgulUiRenderer.hpp"
#include "ui/ChunkExporterPanel.hpp"
#include "world/WorldState.hpp"

#include <cmath>
#include <filesystem>
#include <iostream>
#include <string>

namespace {

struct CliOptions {
    std::filesystem::path assetsDir;
    std::filesystem::path defaultBrowseDir;
    std::filesystem::path outputRoot;
    bool assetsDirSet = false;
    bool defaultBrowseDirSet = false;
    bool outputRootSet = false;
    bool showHelp = false;
};

void PrintUsage(const char* argv0) {
    std::cout
        << "Usage: " << argv0
        << " [--assets-dir <path>] [--default-browse-dir <path>] [--output-root <path>]\n"
        << "  --assets-dir <path>          Set assets dir (default: <app_dir>/assets)\n"
        << "  --default-browse-dir <path>  Set initial browse directory (default: <assets-dir>)\n"
        << "  --output-root <path>         Set export output root (default: <assets-dir>/chunks)\n";
}

bool ParseCli(int argc, char** argv, CliOptions* outOptions) {
    if (!outOptions) {
        return false;
    }

    CliOptions options;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            options.showHelp = true;
            continue;
        }
        if (arg == "--assets-dir") {
            if (i + 1 >= argc) {
                std::cerr << "Missing value for --assets-dir\n";
                return false;
            }
            options.assetsDir = std::filesystem::path(argv[++i]);
            options.assetsDirSet = true;
            continue;
        }
        if (arg == "--default-browse-dir") {
            if (i + 1 >= argc) {
                std::cerr << "Missing value for --default-browse-dir\n";
                return false;
            }
            options.defaultBrowseDir = std::filesystem::path(argv[++i]);
            options.defaultBrowseDirSet = true;
            continue;
        }
        if (arg == "--output-root") {
            if (i + 1 >= argc) {
                std::cerr << "Missing value for --output-root\n";
                return false;
            }
            options.outputRoot = std::filesystem::path(argv[++i]);
            options.outputRootSet = true;
            continue;
        }

        std::cerr << "Unknown argument: " << arg << "\n";
        return false;
    }

    *outOptions = options;
    return true;
}

ImGuiStyle g_defaultStyle;
bool g_defaultCaptured = false;
ImFont* g_defaultFont = nullptr;
ImFont* g_cgulFont = nullptr;

void CaptureDefaultImGuiStyleOnce() {
    if (g_defaultCaptured) {
        return;
    }

    g_defaultStyle = ImGui::GetStyle();
    g_defaultFont = ImGui::GetIO().FontDefault;
    g_defaultCaptured = true;
}

void ApplyDefaultImGuiStyle() {
    CaptureDefaultImGuiStyleOnce();
    ImGui::GetStyle() = g_defaultStyle;
    ImGui::GetIO().FontDefault = g_defaultFont;
}

void ApplyCgulImGuiStyle() {
    CaptureDefaultImGuiStyleOnce();
    ImGuiStyle& style = ImGui::GetStyle();
    style = g_defaultStyle;

    style.WindowRounding = 0.0f;
    style.FrameRounding = 0.0f;
    style.ScrollbarRounding = 0.0f;
    style.GrabRounding = 0.0f;
    style.WindowBorderSize = 1.0f;
    style.FrameBorderSize = 1.0f;
    style.PopupBorderSize = 1.0f;
    style.WindowPadding = ImVec2(8.0f, 8.0f);
    style.FramePadding = ImVec2(6.0f, 4.0f);
    style.ItemSpacing = ImVec2(8.0f, 6.0f);

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_Text] = ImVec4(0.93f, 0.96f, 1.00f, 1.00f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.58f, 0.66f, 0.76f, 1.00f);
    colors[ImGuiCol_WindowBg] = ImVec4(0.09f, 0.13f, 0.18f, 0.92f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.11f, 0.17f, 0.23f, 0.86f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.10f, 0.15f, 0.22f, 0.94f);
    colors[ImGuiCol_Border] = ImVec4(0.43f, 0.62f, 0.82f, 0.72f);
    colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.18f, 0.28f, 0.39f, 0.74f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.25f, 0.40f, 0.56f, 0.74f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.28f, 0.46f, 0.65f, 0.78f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.15f, 0.24f, 0.34f, 0.92f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.19f, 0.32f, 0.46f, 0.95f);
    colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.12f, 0.19f, 0.28f, 0.78f);
    colors[ImGuiCol_MenuBarBg] = ImVec4(0.14f, 0.22f, 0.31f, 0.88f);
    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.08f, 0.12f, 0.17f, 0.76f);
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.31f, 0.46f, 0.61f, 0.78f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.38f, 0.56f, 0.76f, 0.82f);
    colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.43f, 0.64f, 0.85f, 0.86f);
    colors[ImGuiCol_CheckMark] = ImVec4(0.60f, 0.84f, 0.95f, 1.00f);
    colors[ImGuiCol_SliderGrab] = ImVec4(0.47f, 0.72f, 0.90f, 0.86f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.61f, 0.87f, 0.98f, 0.92f);
    colors[ImGuiCol_Button] = ImVec4(0.24f, 0.37f, 0.52f, 0.74f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.33f, 0.50f, 0.70f, 0.80f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.39f, 0.60f, 0.84f, 0.86f);
    colors[ImGuiCol_Header] = ImVec4(0.24f, 0.38f, 0.53f, 0.74f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.33f, 0.52f, 0.73f, 0.82f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.38f, 0.61f, 0.85f, 0.86f);
    colors[ImGuiCol_Separator] = ImVec4(0.41f, 0.58f, 0.76f, 0.72f);
    colors[ImGuiCol_SeparatorHovered] = ImVec4(0.51f, 0.73f, 0.94f, 0.84f);
    colors[ImGuiCol_SeparatorActive] = ImVec4(0.57f, 0.80f, 1.00f, 0.90f);
    colors[ImGuiCol_ResizeGrip] = ImVec4(0.37f, 0.55f, 0.73f, 0.48f);
    colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.47f, 0.71f, 0.92f, 0.76f);
    colors[ImGuiCol_ResizeGripActive] = ImVec4(0.56f, 0.82f, 0.99f, 0.86f);

    if (g_cgulFont) {
        ImGui::GetIO().FontDefault = g_cgulFont;
    }
}

void DrawCgulGridBackground() {
    const ImGuiIO& io = ImGui::GetIO();
    if (io.DisplaySize.x <= 0.0f || io.DisplaySize.y <= 0.0f) {
        return;
    }

    ImDrawList* drawList = ImGui::GetBackgroundDrawList();
    const float width = std::floor(io.DisplaySize.x);
    const float height = std::floor(io.DisplaySize.y);
    constexpr float fineSpacing = 16.0f;
    constexpr float coarseSpacing = 64.0f;
    const ImU32 fineColor = IM_COL32(110, 165, 220, 24);
    const ImU32 coarseColor = IM_COL32(135, 195, 245, 52);

    for (float x = 0.5f; x <= width; x += fineSpacing) {
        drawList->AddLine(ImVec2(x, 0.5f), ImVec2(x, height + 0.5f), fineColor, 1.0f);
    }
    for (float y = 0.5f; y <= height; y += fineSpacing) {
        drawList->AddLine(ImVec2(0.5f, y), ImVec2(width + 0.5f, y), fineColor, 1.0f);
    }
    for (float x = 0.5f; x <= width; x += coarseSpacing) {
        drawList->AddLine(ImVec2(x, 0.5f), ImVec2(x, height + 0.5f), coarseColor, 1.0f);
    }
    for (float y = 0.5f; y <= height; y += coarseSpacing) {
        drawList->AddLine(ImVec2(0.5f, y), ImVec2(width + 0.5f, y), coarseColor, 1.0f);
    }
}

void SyncWorldStateFromTool(const tools::ChunkExporterTool& tool, cgul_demo::WorldState* worldState) {
    if (!worldState) {
        return;
    }

    worldState->hasMap = tool.HasMap();
    if (worldState->hasMap) {
        worldState->map = tool.GetMap();
    } else {
        worldState->map = tiled::TiledMap{};
        worldState->cameraTileX = 0.0f;
        worldState->cameraTileY = 0.0f;
        worldState->zoom = 1.0f;
        worldState->ResetHover();
    }

    worldState->statusText = tool.GetStatusText();
    if (!tool.GetLoadError().empty()) {
        worldState->errorText = tool.GetLoadError();
    } else if (!tool.GetRenderError().empty()) {
        worldState->errorText = tool.GetRenderError();
    } else {
        worldState->errorText.clear();
    }

    worldState->ClampCameraToMap();
}

}  // namespace

int main(int argc, char** argv) {
    CliOptions options;
    if (!ParseCli(argc, argv, &options)) {
        PrintUsage(argv[0]);
        return 1;
    }
    if (options.showHelp) {
        PrintUsage(argv[0]);
        return 0;
    }

    const std::filesystem::path appDir = cgul_demo::paths::ResolveAppDir(argv[0]);
    if (!options.assetsDirSet) {
        options.assetsDir = appDir / "assets";
    }
    if (!options.defaultBrowseDirSet) {
        options.defaultBrowseDir = options.assetsDir;
    }
    if (!options.outputRootSet) {
        options.outputRoot = options.assetsDir / "chunks";
    }

    std::error_code ec;
    options.assetsDir = std::filesystem::absolute(options.assetsDir, ec);
    ec.clear();
    options.defaultBrowseDir = std::filesystem::absolute(options.defaultBrowseDir, ec);
    ec.clear();
    options.outputRoot = std::filesystem::absolute(options.outputRoot, ec);
    ec.clear();
    std::filesystem::create_directories(options.outputRoot, ec);
    if (ec) {
        std::cerr << "Warning: failed to create output root '" << options.outputRoot.string()
                  << "': " << ec.message() << "\n";
    }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << "\n";
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow("CGUL ImGui Demo", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        1280, 720, SDL_WINDOW_RESIZABLE);
    if (!window) {
        std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << "\n";
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(
        window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (!renderer) {
        std::cerr << "SDL_CreateRenderer failed: " << SDL_GetError() << "\n";
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    g_cgulFont = io.Fonts->AddFontFromFileTTF("assets/fonts/cgul_mono.ttf", 15.0f);
    if (g_cgulFont == nullptr) {
        std::cerr << "Warning: failed to load assets/fonts/cgul_mono.ttf, using default ImGui font\n";
    }

    ImGui::StyleColorsDark();

    ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer2_Init(renderer);

    cgul_demo::WorldState worldState;
    cgul_demo::ChunkExporterPanel panel(renderer, options.defaultBrowseDir, options.outputRoot);
    cgul_demo::ArtRenderer artRenderer;
    cgul_demo::CgulUiRenderer cgulUiRenderer;
    const std::filesystem::path statePath = options.assetsDir / "cgul_imgui_demo_state.cgul";

    auto saveState = [&]() {
        std::string error;
        if (!cgul_demo::SaveStateCgul(statePath, worldState, panel.Tool(), &error)) {
            SDL_Log("Save state failed: %s", error.c_str());
        }
    };

    auto loadState = [&]() {
        std::string error;
        if (!cgul_demo::LoadStateCgul(statePath, &worldState, &panel.Tool(), &error)) {
            SDL_Log("Load state failed: %s", error.c_str());
            return;
        }
        SyncWorldStateFromTool(panel.Tool(), &worldState);
    };

    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);

            if (event.type == SDL_QUIT) {
                running = false;
                continue;
            }

            if (event.type == SDL_KEYDOWN && event.key.repeat == 0) {
                const bool ctrlDown = (event.key.keysym.mod & KMOD_CTRL) != 0;
                if (ctrlDown && event.key.keysym.sym == SDLK_s) {
                    saveState();
                    continue;
                }
                if (ctrlDown && event.key.keysym.sym == SDLK_l) {
                    loadState();
                    continue;
                }

                if (event.key.keysym.sym == SDLK_TAB) {
                    worldState.calmMode = !worldState.calmMode;
                    continue;
                }

                const bool viewportWantsControls = worldState.hasMap && worldState.viewportHovered;
                const bool imGuiTextEditing =
                    io.WantTextInput || ImGui::IsAnyItemActive() || ImGui::IsAnyItemFocused();
                if (worldState.hasMap && (viewportWantsControls || !io.WantCaptureKeyboard) &&
                    !imGuiTextEditing) {
                    const float panStep = 1.0f;
                    const float zoomStep = 1.1f;
                    const float fineZoomStep = 1.03f;
                    const SDL_Keycode key = event.key.keysym.sym;
                    bool changedView = false;

                    if (key == SDLK_LEFT) {
                        worldState.cameraTileX -= panStep;
                        changedView = true;
                    } else if (key == SDLK_RIGHT) {
                        worldState.cameraTileX += panStep;
                        changedView = true;
                    } else if (key == SDLK_UP) {
                        worldState.cameraTileY -= panStep;
                        changedView = true;
                    } else if (key == SDLK_DOWN) {
                        worldState.cameraTileY += panStep;
                        changedView = true;
                    } else if (key == SDLK_EQUALS
#ifdef SDLK_PLUS
                               || key == SDLK_PLUS
#endif
                               || key == SDLK_KP_PLUS) {
                        worldState.zoom *= zoomStep;
                        changedView = true;
                    } else if (key == SDLK_MINUS
#ifdef SDLK_UNDERSCORE
                               || key == SDLK_UNDERSCORE
#endif
                               || key == SDLK_KP_MINUS) {
                        worldState.zoom /= zoomStep;
                        changedView = true;
                    } else if (key == SDLK_0 || key == SDLK_KP_0) {
                        worldState.zoom = 1.0f;
                        changedView = true;
                    } else if (key == SDLK_RIGHTBRACKET) {
                        worldState.zoom *= fineZoomStep;
                        changedView = true;
                    } else if (key == SDLK_LEFTBRACKET) {
                        worldState.zoom /= fineZoomStep;
                        changedView = true;
                    }

#ifndef NDEBUG
                    SDL_Log(
                        "Input keydown: key=%d changed=%d zoom=%.3f captureK=%d captureM=%d textInput=%d "
                        "viewportHovered=%d",
                        static_cast<int>(key), changedView ? 1 : 0, worldState.zoom,
                        io.WantCaptureKeyboard ? 1 : 0, io.WantCaptureMouse ? 1 : 0,
                        io.WantTextInput ? 1 : 0, worldState.viewportHovered ? 1 : 0);
#endif

                    if (changedView) {
                        worldState.ClampCameraToMap();
                    }
                }
            }

            if (event.type == SDL_MOUSEWHEEL && worldState.hasMap) {
                const bool viewportWantsControls = worldState.hasMap && worldState.viewportHovered;
                const bool imGuiTextEditing =
                    io.WantTextInput || ImGui::IsAnyItemActive() || ImGui::IsAnyItemFocused();
                if ((viewportWantsControls || !io.WantCaptureMouse) && !imGuiTextEditing) {
                    const float step = std::pow(1.1f, static_cast<float>(event.wheel.y));
                    worldState.zoom *= step;
#ifndef NDEBUG
                    SDL_Log(
                        "Input wheel: y=%d zoom=%.3f captureK=%d captureM=%d textInput=%d "
                        "viewportHovered=%d",
                        event.wheel.y, worldState.zoom, io.WantCaptureKeyboard ? 1 : 0,
                        io.WantCaptureMouse ? 1 : 0, io.WantTextInput ? 1 : 0,
                        worldState.viewportHovered ? 1 : 0);
#endif
                    worldState.ClampCameraToMap();
                }
            }

            if (event.type == SDL_MOUSEMOTION && worldState.hasMap && worldState.viewportHovered &&
                (event.motion.state & SDL_BUTTON_RMASK) != 0 && worldState.viewportWidthPx > 1.0f &&
                worldState.viewportHeightPx > 1.0f) {
                worldState.cameraTileX -= static_cast<float>(event.motion.xrel) *
                    (worldState.visibleTileSpanX / worldState.viewportWidthPx);
                worldState.cameraTileY -= static_cast<float>(event.motion.yrel) *
                    (worldState.visibleTileSpanY / worldState.viewportHeightPx);
                worldState.ClampCameraToMap();
            }
        }

        ImGui_ImplSDLRenderer2_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();
        CaptureDefaultImGuiStyleOnce();
        if (worldState.calmMode) {
            ApplyCgulImGuiStyle();
            DrawCgulGridBackground();
            SyncWorldStateFromTool(panel.Tool(), &worldState);
        } else {
            ApplyDefaultImGuiStyle();
        }

        if (worldState.calmMode) {
            ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize, ImGuiCond_Always);
            ImGui::Begin("CGUL_UI", nullptr,
                ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                    ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus |
                    ImGuiWindowFlags_NoNav);
            cgulUiRenderer.Draw(&worldState, &panel.Tool());
            ImGui::SetCursorPos(ImVec2(12.0f, 8.0f));
            if (ImGui::Button("Save State")) {
                saveState();
            }
            ImGui::SameLine();
            if (ImGui::Button("Load State")) {
                loadState();
            }
            ImGui::End();
        } else {
            ImGui::SetNextWindowBgAlpha(0.75f);
            ImGui::SetNextWindowPos(ImVec2(10.0f, 10.0f), ImGuiCond_Always);
            ImGui::Begin("Mode", nullptr,
                ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                    ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                    ImGuiWindowFlags_NoNav);
            ImGui::TextUnformatted("TAB: Calm Mode (Default UI)");
            ImGui::SameLine();
            ImGui::TextUnformatted("RMB drag / Arrows pan / +/- zoom / [] fine / 0 reset / Wheel zoom");
            ImGui::End();

            panel.Draw(&worldState);

            ImGui::Begin("Viewport");
            if (!worldState.errorText.empty()) {
                ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f), "%s", worldState.errorText.c_str());
            }
            artRenderer.Draw(&worldState, &panel.Tool());
            ImGui::End();
        }

        ImGui::Render();
        SDL_SetRenderDrawColor(renderer, 18, 18, 22, 255);
        SDL_RenderClear(renderer);
        ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData());
        SDL_RenderPresent(renderer);
    }

    ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
