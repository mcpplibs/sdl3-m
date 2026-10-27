// SDL's application callbacks, entered from this program's own main through
// SDL_EnterAppMainCallbacks — what <SDL3/SDL_main.h> does for a program with
// SDL_MAIN_USE_CALLBACKS on a platform with a standard main. The callback
// types (SDL_AppInit_func ...) and the entry point come from the module. A
// window and a renderer on the dummy video driver, three frames, an event the
// app pushes to itself, then success.
import std;
import mcpplibs.mwrap.sdl3;

using namespace mcpplibs::mwrap::sdl3::macros;

namespace {

struct App {
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    Uint32 ping = 0;
    int frames = 0, pings = 0;
    bool quit_ok = false;
} app;

SDL_AppResult init(void** appstate, int argc, char* argv[]) {
    if (argc < 1 || !argv[0]) return SDL_APP_FAILURE;
    SDL_SetHintWithPriority(SDL_HINT_VIDEO_DRIVER, "dummy", SDL_HINT_OVERRIDE);
    if (!SDL_Init(SDL_INIT_VIDEO)) return SDL_APP_FAILURE;
    if (!SDL_CreateWindowAndRenderer("callbacks", 32, 32, 0, &app.window, &app.renderer)) {
        SDL_Log("SDL_CreateWindowAndRenderer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    app.ping = SDL_RegisterEvents(1);
    *appstate = &app;
    return SDL_APP_CONTINUE;
}

SDL_AppResult event(void* appstate, SDL_Event* e) {
    auto* a = static_cast<App*>(appstate);
    if (e->type == a->ping) ++a->pings;
    return e->type == SDL_EVENT_QUIT ? SDL_APP_SUCCESS : SDL_APP_CONTINUE;
}

SDL_AppResult iterate(void* appstate) {
    auto* a = static_cast<App*>(appstate);
    SDL_SetRenderDrawColor(a->renderer, static_cast<Uint8>(a->frames * 80), 0, 0, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(a->renderer);
    SDL_RenderPresent(a->renderer);
    SDL_Event e{};
    e.type = a->ping;
    SDL_PushEvent(&e);  // delivered to `event` before the next iteration
    return ++a->frames == 3 ? SDL_APP_SUCCESS : SDL_APP_CONTINUE;
}

void quit(void* appstate, SDL_AppResult result) {
    auto* a = static_cast<App*>(appstate);
    app.quit_ok = a == &app && result == SDL_APP_SUCCESS;
    if (a) {
        SDL_DestroyRenderer(a->renderer);
        SDL_DestroyWindow(a->window);
    }
}

static_assert(std::is_same_v<decltype(&init), SDL_AppInit_func> && std::is_same_v<decltype(&quit), SDL_AppQuit_func>);

}  // namespace

int main(int argc, char* argv[]) {
    const int status = SDL_EnterAppMainCallbacks(argc, argv, init, iterate, event, quit);
    std::println("SDL_EnterAppMainCallbacks: {} after {} frames and {} events, quit {}", status, app.frames, app.pings,
                 app.quit_ok ? "with success" : "unexpected");
    return status == 0 && app.frames == 3 && app.pings >= 2 && app.quit_ok ? 0 : 1;
}
