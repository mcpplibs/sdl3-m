# sdl3

> SDL 3, the cross-platform window, input, audio, GPU and platform layer, through C++23 modules.

**English** · [简体中文](README.zh.md) · [Upstream](https://github.com/libsdl-org/SDL) · [Releases](https://github.com/mcpplibs/sdl3-m/releases) · [mcpp](https://github.com/mcpp-community/mcpp) · [Index](https://github.com/mcpplibs/mcpp-index)

[![C++23](https://img.shields.io/badge/C%2B%2B-23-blue.svg)](https://en.cppreference.com/w/cpp/23)
[![License](https://img.shields.io/badge/license-Apache--2.0-blue.svg)](LICENSE)
[![mcpp-community](https://img.shields.io/badge/mcpp--community-mwrap-blue)](https://github.com/mcpp-community)
[![CI](https://github.com/mcpplibs/sdl3-m/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/mcpplibs/sdl3-m/actions/workflows/ci.yml)

The upstream C API is unchanged: `SDL_Init`, `SDL_Window`, `SDL_Event`,
`SDL_CreateGPUDevice` are the upstream globals of `<SDL3/SDL.h>` (with
`SDL_vulkan.h`, `SDL_main.h`'s functions and `SDL_revision.h`): every function,
type and enumerator, each exported on the platforms that declare it. The
library is `compat.sdl3` 3.4.18. This package adapts the header's inline API
(`SDL_PointInRect`, `SDL_Swap32` …) and maps its macros to constants and
functions under their original names (`SDL_INIT_VIDEO`, `SDLK_A`,
`SDL_HINT_*`, `SDL_PROP_*`, `SDL_BUTTON_MASK`, `SDL_CreateThread` …). SDL's own
automated test suite passes on every CI run, compiled against the module.

## Modules

- `import mcpplibs.mwrap.sdl3;`: the C API, the inline API in `mcpplibs::mwrap::sdl3`, re-exporting the one below
- `import mcpplibs.mwrap.sdl3.macros;`: `SDL_INIT_VIDEO`, `SDL_WINDOW_RESIZABLE`, `SDLK_A`, `SDL_BITSPERPIXEL` … in `mcpplibs::mwrap::sdl3::macros`

> More Modules API -> [Usage and API](docs/usage.md)

## Getting started

```sh
mcpp new sdlinfo --template mcpplibs.mwrap.sdl3@3.4.18
cd sdlinfo
mcpp run
```

The default template (`info`) prints what SDL sees on the machine: the
version, CPU features, video, render, audio and GPU drivers, the displays, a
GPU device and its shader formats. It needs no window or desktop; without a
display it reports through SDL's offscreen driver. The `window` template is a
sketch pad on SDL's app callbacks and needs a desktop:
`mcpp new pad --template mcpplibs.mwrap.sdl3@3.4.18:window`.

```toml
[dependencies]
"mcpplibs.mwrap.sdl3" = "3.4.18"
```

```cpp
import std;
import mcpplibs.mwrap.sdl3;
using namespace mcpplibs::mwrap::sdl3::macros;  // SDL_INIT_VIDEO, SDL_WINDOW_RESIZABLE, SDLK_ESCAPE …

int main() {
    if (!SDL_Init(SDL_INIT_VIDEO)) return std::println("{}", SDL_GetError()), 1;
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    SDL_CreateWindowAndRenderer("hello", 640, 480, SDL_WINDOW_RESIZABLE, &window, &renderer);
    for (bool running = true; running;) {
        for (SDL_Event e; SDL_PollEvent(&e);)
            running = e.type != SDL_EVENT_QUIT && !(e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_ESCAPE);
        SDL_SetRenderDrawColor(renderer, 30, 60, 90, SDL_ALPHA_OPAQUE);
        SDL_RenderClear(renderer);
        SDL_RenderPresent(renderer);
    }
    SDL_Quit();
}
```

## Inline functions and macros

`SDL_FORCE_INLINE` functions have internal linkage and cannot be exported; the
16 public ones are adapted under their names in `mcpplibs::mwrap::sdl3`, each
calling upstream's. Macros become `inline constexpr` constants of the macro's
own type (strings as `const char[]`) and functions whose body is the macro
itself, every argument evaluated once; the ones that take a value apart are
templates, as upstream applies them to enums and integers alike. The
assertions, `SDL_stack_alloc`, `SDL_COMPILE_TIME_ASSERT`, the `SDL_PLATFORM_*`
tests, attributes and SDL 2's names stay macros in `<mwrap/sdl3/macros.hpp>`,
included before the import; so does `"%" SDL_PRIs64` literal concatenation.
Every macro is accounted for: [docs/usage.md](docs/usage.md).

## SDL_main.h and the app callbacks

The module includes `<SDL3/SDL_main.h>` with `SDL_MAIN_HANDLED`, so it exports
`SDL_SetMainReady`, `SDL_RunApp`, `SDL_EnterAppMainCallbacks` and the
callback types, not the `main` redefinition. A program that wants SDL's entry
point includes `<SDL3/SDL_main.h>` itself, in the one file that defines `main`
or the `SDL_App*` callbacks, beside the import (the `window` template does).

## Platforms

Linux, macOS and Windows. Declarations of one platform are exported there
only (`SDL_SetLinuxThreadPriority`, `SDL_GetDXGIOutputInfo`, `MSG` …).
`SDL_opengl*.h`, `SDL_egl.h` (Khronos' APIs as SDL ships them; `SDL_GL_*` and
`SDL_EGL_*` are exported), `SDL_intrin.h` and the `SDL_test` headers are not
wrapped; include them directly. Headless runs use `SDL_VIDEO_DRIVER=dummy` and
`SDL_AUDIO_DRIVER=dummy` (on Linux and Windows `offscreen` also offers Vulkan; on
macOS it makes every window an OpenGL one, which it cannot load).

## Build and testing

Linux (GCC 16, LLVM 23), macOS and Windows CI build the modules and run:

- the tests: the API through the module (init, properties, hints, events,
  timers, threads, IO, filesystem, pixels, surfaces, the renderer, audio, a
  GPU device on lavapipe or Metal), the macro mappings, header mixing,
  `SDL_main.h` and the app callbacks;
- the export audit ([tools/check_exports.py](tools/check_exports.py)): each row
  preprocesses the headers and checks that every declaration there is
  exported, every inline function adapted and every macro mapped or listed;
- SDL's `testautomation` (26 suites, 377 tests), its C compiled as C++
  against the module and run as upstream's CTest runs it, with and without
  SIMD ([tools/run_conformance.py](tools/run_conformance.py)).

A separate job scaffolds both templates as an installed package, and runs the
default one.

## License

[Apache-2.0](LICENSE) for the wrapper, by mcpp-community ([NOTICE](NOTICE));
SDL remains zlib-licensed ([upstream/sdl3/LICENSE.txt](upstream/sdl3/LICENSE.txt)).
