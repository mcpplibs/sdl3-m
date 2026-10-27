// <mwrap/sdl3/macros.hpp> — SDL 3's macros as macros, for the ones a constant
// or a function cannot be: SDL_assert and the other assertions (they report
// the call site and evaluate their condition only when enabled),
// SDL_TriggerBreakpoint, SDL_stack_alloc (alloca in the caller's frame),
// SDL_COMPILE_TIME_ASSERT, SDL_STRINGIFY_ARG, the casts, SDL_SINT64_C /
// SDL_UINT64_C (literal pasting), the platform tests (SDL_PLATFORM_*), the
// attributes and thread-safety annotations, SDL_ELF_NOTE_DLOPEN, and SDL 2's
// names (SDL_oldnames.h). Include it before `import mcpplibs.mwrap.sdl3;`:
// it is <SDL3/SDL.h> with SDL_vulkan.h and SDL_revision.h, so every other
// macro is defined too, and while a macro is defined it expands even inside
// `mcpplibs::mwrap::sdl3::macros::NAME`.
//
// <SDL3/SDL_main.h> is not included: it renames main and, unless
// SDL_MAIN_HANDLED is defined, defines the program's entry point. Include it
// yourself, in the one file that defines main.
#pragma once
#include <SDL3/SDL.h>
#include <SDL3/SDL_revision.h>
#include <SDL3/SDL_vulkan.h>
