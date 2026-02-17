#include "imgui_impl_sdl2.h"

#include <SDL.h>

#include <imgui.h>

#include <array>
#include <cstring>

namespace {

SDL_Window* g_Window = nullptr;
Uint64 g_Time = 0;
SDL_Cursor* g_MouseCursors[ImGuiMouseCursor_COUNT] = {};

const char* GetClipboardText(void* userData) {
    (void)userData;
    return SDL_GetClipboardText();
}

void SetClipboardText(void* userData, const char* text) {
    (void)userData;
    SDL_SetClipboardText(text);
}

ImGuiKey KeycodeToImGuiKey(SDL_Keycode keycode) {
    switch (keycode) {
    case SDLK_TAB: return ImGuiKey_Tab;
    case SDLK_LEFT: return ImGuiKey_LeftArrow;
    case SDLK_RIGHT: return ImGuiKey_RightArrow;
    case SDLK_UP: return ImGuiKey_UpArrow;
    case SDLK_DOWN: return ImGuiKey_DownArrow;
    case SDLK_PAGEUP: return ImGuiKey_PageUp;
    case SDLK_PAGEDOWN: return ImGuiKey_PageDown;
    case SDLK_HOME: return ImGuiKey_Home;
    case SDLK_END: return ImGuiKey_End;
    case SDLK_INSERT: return ImGuiKey_Insert;
    case SDLK_DELETE: return ImGuiKey_Delete;
    case SDLK_BACKSPACE: return ImGuiKey_Backspace;
    case SDLK_SPACE: return ImGuiKey_Space;
    case SDLK_RETURN: return ImGuiKey_Enter;
    case SDLK_ESCAPE: return ImGuiKey_Escape;
    case SDLK_QUOTE: return ImGuiKey_Apostrophe;
    case SDLK_COMMA: return ImGuiKey_Comma;
    case SDLK_MINUS: return ImGuiKey_Minus;
    case SDLK_PERIOD: return ImGuiKey_Period;
    case SDLK_SLASH: return ImGuiKey_Slash;
    case SDLK_SEMICOLON: return ImGuiKey_Semicolon;
    case SDLK_EQUALS: return ImGuiKey_Equal;
    case SDLK_LEFTBRACKET: return ImGuiKey_LeftBracket;
    case SDLK_BACKSLASH: return ImGuiKey_Backslash;
    case SDLK_RIGHTBRACKET: return ImGuiKey_RightBracket;
    case SDLK_BACKQUOTE: return ImGuiKey_GraveAccent;
    case SDLK_CAPSLOCK: return ImGuiKey_CapsLock;
    case SDLK_SCROLLLOCK: return ImGuiKey_ScrollLock;
    case SDLK_NUMLOCKCLEAR: return ImGuiKey_NumLock;
    case SDLK_PRINTSCREEN: return ImGuiKey_PrintScreen;
    case SDLK_PAUSE: return ImGuiKey_Pause;
    case SDLK_KP_0: return ImGuiKey_Keypad0;
    case SDLK_KP_1: return ImGuiKey_Keypad1;
    case SDLK_KP_2: return ImGuiKey_Keypad2;
    case SDLK_KP_3: return ImGuiKey_Keypad3;
    case SDLK_KP_4: return ImGuiKey_Keypad4;
    case SDLK_KP_5: return ImGuiKey_Keypad5;
    case SDLK_KP_6: return ImGuiKey_Keypad6;
    case SDLK_KP_7: return ImGuiKey_Keypad7;
    case SDLK_KP_8: return ImGuiKey_Keypad8;
    case SDLK_KP_9: return ImGuiKey_Keypad9;
    case SDLK_KP_PERIOD: return ImGuiKey_KeypadDecimal;
    case SDLK_KP_DIVIDE: return ImGuiKey_KeypadDivide;
    case SDLK_KP_MULTIPLY: return ImGuiKey_KeypadMultiply;
    case SDLK_KP_MINUS: return ImGuiKey_KeypadSubtract;
    case SDLK_KP_PLUS: return ImGuiKey_KeypadAdd;
    case SDLK_KP_ENTER: return ImGuiKey_KeypadEnter;
    case SDLK_KP_EQUALS: return ImGuiKey_KeypadEqual;
    case SDLK_LCTRL: return ImGuiKey_LeftCtrl;
    case SDLK_LSHIFT: return ImGuiKey_LeftShift;
    case SDLK_LALT: return ImGuiKey_LeftAlt;
    case SDLK_LGUI: return ImGuiKey_LeftSuper;
    case SDLK_RCTRL: return ImGuiKey_RightCtrl;
    case SDLK_RSHIFT: return ImGuiKey_RightShift;
    case SDLK_RALT: return ImGuiKey_RightAlt;
    case SDLK_RGUI: return ImGuiKey_RightSuper;
    case SDLK_APPLICATION: return ImGuiKey_Menu;
    case SDLK_0: return ImGuiKey_0;
    case SDLK_1: return ImGuiKey_1;
    case SDLK_2: return ImGuiKey_2;
    case SDLK_3: return ImGuiKey_3;
    case SDLK_4: return ImGuiKey_4;
    case SDLK_5: return ImGuiKey_5;
    case SDLK_6: return ImGuiKey_6;
    case SDLK_7: return ImGuiKey_7;
    case SDLK_8: return ImGuiKey_8;
    case SDLK_9: return ImGuiKey_9;
    case SDLK_a: return ImGuiKey_A;
    case SDLK_b: return ImGuiKey_B;
    case SDLK_c: return ImGuiKey_C;
    case SDLK_d: return ImGuiKey_D;
    case SDLK_e: return ImGuiKey_E;
    case SDLK_f: return ImGuiKey_F;
    case SDLK_g: return ImGuiKey_G;
    case SDLK_h: return ImGuiKey_H;
    case SDLK_i: return ImGuiKey_I;
    case SDLK_j: return ImGuiKey_J;
    case SDLK_k: return ImGuiKey_K;
    case SDLK_l: return ImGuiKey_L;
    case SDLK_m: return ImGuiKey_M;
    case SDLK_n: return ImGuiKey_N;
    case SDLK_o: return ImGuiKey_O;
    case SDLK_p: return ImGuiKey_P;
    case SDLK_q: return ImGuiKey_Q;
    case SDLK_r: return ImGuiKey_R;
    case SDLK_s: return ImGuiKey_S;
    case SDLK_t: return ImGuiKey_T;
    case SDLK_u: return ImGuiKey_U;
    case SDLK_v: return ImGuiKey_V;
    case SDLK_w: return ImGuiKey_W;
    case SDLK_x: return ImGuiKey_X;
    case SDLK_y: return ImGuiKey_Y;
    case SDLK_z: return ImGuiKey_Z;
    case SDLK_F1: return ImGuiKey_F1;
    case SDLK_F2: return ImGuiKey_F2;
    case SDLK_F3: return ImGuiKey_F3;
    case SDLK_F4: return ImGuiKey_F4;
    case SDLK_F5: return ImGuiKey_F5;
    case SDLK_F6: return ImGuiKey_F6;
    case SDLK_F7: return ImGuiKey_F7;
    case SDLK_F8: return ImGuiKey_F8;
    case SDLK_F9: return ImGuiKey_F9;
    case SDLK_F10: return ImGuiKey_F10;
    case SDLK_F11: return ImGuiKey_F11;
    case SDLK_F12: return ImGuiKey_F12;
    default: return ImGuiKey_None;
    }
}

void UpdateMouseCursor() {
    ImGuiIO& io = ImGui::GetIO();
    if ((io.ConfigFlags & ImGuiConfigFlags_NoMouseCursorChange) != 0) {
        return;
    }
    ImGuiMouseCursor cursor = ImGui::GetMouseCursor();
    if (cursor == ImGuiMouseCursor_None || io.MouseDrawCursor) {
        SDL_ShowCursor(SDL_FALSE);
    } else {
        SDL_SetCursor(g_MouseCursors[cursor] ? g_MouseCursors[cursor] : g_MouseCursors[ImGuiMouseCursor_Arrow]);
        SDL_ShowCursor(SDL_TRUE);
    }
}

}  // namespace

bool ImGui_ImplSDL2_InitForSDLRenderer(SDL_Window* window, SDL_Renderer* renderer) {
    (void)renderer;
    g_Window = window;
    g_Time = 0;

    ImGuiIO& io = ImGui::GetIO();
    io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;
    io.BackendFlags |= ImGuiBackendFlags_HasSetMousePos;
    io.BackendPlatformName = "imgui_impl_sdl2_custom";

    io.SetClipboardTextFn = SetClipboardText;
    io.GetClipboardTextFn = GetClipboardText;
    io.ClipboardUserData = nullptr;

    g_MouseCursors[ImGuiMouseCursor_Arrow] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_ARROW);
    g_MouseCursors[ImGuiMouseCursor_TextInput] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_IBEAM);
    g_MouseCursors[ImGuiMouseCursor_ResizeAll] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZEALL);
    g_MouseCursors[ImGuiMouseCursor_ResizeNS] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZENS);
    g_MouseCursors[ImGuiMouseCursor_ResizeEW] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZEWE);
    g_MouseCursors[ImGuiMouseCursor_ResizeNESW] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZENESW);
    g_MouseCursors[ImGuiMouseCursor_ResizeNWSE] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZENWSE);
    g_MouseCursors[ImGuiMouseCursor_Hand] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_HAND);
    g_MouseCursors[ImGuiMouseCursor_NotAllowed] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_NO);

    return true;
}

void ImGui_ImplSDL2_Shutdown() {
    for (SDL_Cursor*& cursor : g_MouseCursors) {
        if (cursor) {
            SDL_FreeCursor(cursor);
            cursor = nullptr;
        }
    }
    g_Window = nullptr;
}

void ImGui_ImplSDL2_NewFrame() {
    ImGuiIO& io = ImGui::GetIO();
    if (!g_Window) {
        return;
    }

    int w = 0;
    int h = 0;
    SDL_GetWindowSize(g_Window, &w, &h);
    io.DisplaySize = ImVec2(static_cast<float>(w), static_cast<float>(h));

    Uint64 currentTime = SDL_GetPerformanceCounter();
    double deltaTime = 0.0;
    if (g_Time != 0) {
        deltaTime = static_cast<double>(currentTime - g_Time) /
            static_cast<double>(SDL_GetPerformanceFrequency());
    }
    g_Time = currentTime;
    io.DeltaTime = deltaTime > 0.0 ? static_cast<float>(deltaTime) : (1.0f / 60.0f);

    int mouseX = 0;
    int mouseY = 0;
    const Uint32 mouseButtons = SDL_GetMouseState(&mouseX, &mouseY);
    io.AddMousePosEvent(static_cast<float>(mouseX), static_cast<float>(mouseY));
    io.AddMouseButtonEvent(0, (mouseButtons & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0);
    io.AddMouseButtonEvent(1, (mouseButtons & SDL_BUTTON(SDL_BUTTON_RIGHT)) != 0);
    io.AddMouseButtonEvent(2, (mouseButtons & SDL_BUTTON(SDL_BUTTON_MIDDLE)) != 0);

    UpdateMouseCursor();
}

bool ImGui_ImplSDL2_ProcessEvent(const SDL_Event* event) {
    if (!event) {
        return false;
    }

    ImGuiIO& io = ImGui::GetIO();

    switch (event->type) {
    case SDL_MOUSEWHEEL:
        io.AddMouseWheelEvent(static_cast<float>(event->wheel.x), static_cast<float>(event->wheel.y));
        return true;
    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEBUTTONUP: {
        const bool pressed = (event->type == SDL_MOUSEBUTTONDOWN);
        if (event->button.button == SDL_BUTTON_LEFT) {
            io.AddMouseButtonEvent(0, pressed);
        } else if (event->button.button == SDL_BUTTON_RIGHT) {
            io.AddMouseButtonEvent(1, pressed);
        } else if (event->button.button == SDL_BUTTON_MIDDLE) {
            io.AddMouseButtonEvent(2, pressed);
        }
        return true;
    }
    case SDL_MOUSEMOTION:
        io.AddMousePosEvent(static_cast<float>(event->motion.x),
            static_cast<float>(event->motion.y));
        return true;
    case SDL_TEXTINPUT:
        io.AddInputCharactersUTF8(event->text.text);
        return true;
    case SDL_KEYDOWN:
    case SDL_KEYUP: {
        const bool pressed = (event->type == SDL_KEYDOWN);
        const ImGuiKey key = KeycodeToImGuiKey(event->key.keysym.sym);
        if (key != ImGuiKey_None) {
            io.AddKeyEvent(key, pressed);
            io.SetKeyEventNativeData(key, event->key.keysym.sym,
                event->key.keysym.scancode, event->key.keysym.scancode);
        }
        const SDL_Keymod mods = SDL_GetModState();
        io.AddKeyEvent(ImGuiKey_LeftCtrl, (mods & KMOD_CTRL) != 0);
        io.AddKeyEvent(ImGuiKey_LeftShift, (mods & KMOD_SHIFT) != 0);
        io.AddKeyEvent(ImGuiKey_LeftAlt, (mods & KMOD_ALT) != 0);
        io.AddKeyEvent(ImGuiKey_LeftSuper, (mods & KMOD_GUI) != 0);
        return true;
    }
    case SDL_WINDOWEVENT:
        if (event->window.event == SDL_WINDOWEVENT_FOCUS_GAINED) {
            io.AddFocusEvent(true);
        } else if (event->window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
            io.AddFocusEvent(false);
        }
        return true;
    default:
        return false;
    }
}
