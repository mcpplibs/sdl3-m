// <SDL3/SDL_main.h> beside the import, as a program uses it: included in the
// one file that defines main. Where SDL supplies the entry point (Windows:
// WinMain and main, which call SDL_RunApp), SDL_main.h renames this main to
// SDL_main; elsewhere main stays main. Either way SDL_main's declaration is
// the module's SDL_main.
#include <SDL3/SDL_main.h>

import std;
import mcpplibs.mwrap.sdl3;

using namespace mcpplibs::mwrap::sdl3::macros;

int main(int argc, char* argv[]) {
    static_assert(std::is_same_v<decltype(&::SDL_main), SDL_main_func>);
    if (argc < 1) return 1;
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "offscreen");
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        std::println("SDL_Init: {}", SDL_GetError());
        return 1;
    }
    SDL_Event e{};
    e.type = SDL_EVENT_QUIT;
    const bool ok = SDL_PushEvent(&e) && SDL_PollEvent(&e) && e.type == SDL_EVENT_QUIT;
    SDL_Quit();
    std::println("main through SDL_main.h: {}", ok ? "ok" : "failed");
    return ok ? 0 : 1;
}
