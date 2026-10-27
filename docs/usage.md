# Using mcpplibs.mwrap.sdl3

`import mcpplibs.mwrap.sdl3;` declares SDL 3.4.18's C API exactly as `<SDL3/SDL.h>` does. The names, types, signatures and behaviour are upstream's, and they are the very same entities as the header's (a translation unit may include the header too). The library is `compat.sdl3`, built from the same release.

## Modules

| Module | What it holds |
|---|---|
| `mcpplibs.mwrap.sdl3` | every function, type, struct and enumerator of `<SDL3/SDL.h>` and its closure, `<SDL3/SDL_vulkan.h>` (with the `VkInstance`, `VkPhysicalDevice`, `VkSurfaceKHR` handles and `VkAllocationCallbacks` it declares), `<SDL3/SDL_main.h>`'s functions and `<SDL3/SDL_revision.h>`; the inline API adapted in `mcpplibs::mwrap::sdl3`; re-exports the one below |
| `mcpplibs.mwrap.sdl3.macros` | the macros, in `mcpplibs::mwrap::sdl3::macros` |

A declaration is exported on the platforms whose headers declare it: `SDL_SetLinuxThreadPriority` on Linux; `SDL_GetDXGIOutputInfo`, `SDL_GetDirect3D9AdapterIndex`, `SDL_SetWindowsMessageHook`, `MSG`, `SDL_RegisterApp` on Windows; the Android, iOS and GDK functions on those platforms. `tools/check_exports.py` checks this on every CI row, from the compiler's own preprocessing of the headers.

```cpp
import std;
import mcpplibs.mwrap.sdl3;
using namespace mcpplibs::mwrap::sdl3;          // SDL_PointInRect, SDL_Swap32 … (the adapted inline API)
using namespace mcpplibs::mwrap::sdl3::macros;  // SDL_INIT_VIDEO, SDLK_A, SDL_HINT_* …
```

Either directive may be left out when its names are not used; the C API itself needs neither.

## The inline API

`SDL_FORCE_INLINE` functions are `static`: internal linkage, not exportable. The 16 public ones are adapted in `mcpplibs::mwrap::sdl3` under the same names and signatures, each defined in `src/inline.cpp` by calling upstream's: `SDL_PointInRect`, `SDL_RectEmpty`, `SDL_RectsEqual`, `SDL_RectToFRect`, `SDL_PointInRectFloat`, `SDL_RectEmptyFloat`, `SDL_RectsEqualEpsilon`, `SDL_RectsEqualFloat`, `SDL_Swap16`, `SDL_Swap32`, `SDL_Swap64`, `SDL_SwapFloat`, `SDL_MostSignificantBitIndex32`, `SDL_HasExactlyOneBitSet32`, `SDL_size_mul_check_overflow`, `SDL_size_add_check_overflow`. On GCC and Clang `SDL_Swap16` &c. and the overflow checks are macros over compiler builtins; the adaptation calls whichever the header defines.

## Macros

A named module cannot export a macro. `mcpplibs::mwrap::sdl3::macros` has them under their original names:

- **Constants** (1060) as `inline constexpr` values of the macro's own type: `SDL_INIT_VIDEO` is `unsigned int`, `SDL_WINDOW_RESIZABLE` is `Uint64` (`SDL_WindowFlags`), `SDL_TOUCH_MOUSEID` is `SDL_MouseID`, `SDL_PI_D` is `double`. Strings are `const char[]` (`sizeof` counts the terminator): the 267 `SDL_HINT_*` names, the `SDL_PROP_*` names, `SDL_SOFTWARE_RENDERER`, `SDL_PRIs64` …. Keycodes (`SDLK_*`), modifiers (`SDL_KMOD_*`), buttons, hats, GPU usage and shader-format bits, the version, the byte order (`SDL_BYTEORDER`, `SDL_LIL_ENDIAN`) are all here.
- **Function-like macros** (87) as functions whose body is the macro itself, expanded in the module where it is defined; every argument is evaluated once (a macro that names an argument twice, like `SDL_min` or `SDL_INIT_INTERFACE`, gives the same result for arguments without side effects):
  - constant expressions: `SDL_FOURCC`, `SDL_DEFINE_PIXELFORMAT`, `SDL_BITSPERPIXEL`, `SDL_ISPIXELFORMAT_*`, `SDL_COLORSPACE*`, `SDL_AUDIO_*`, `SDL_DEFINE_AUDIO_FORMAT`, `SDL_BUTTON_MASK`, `SDL_SCANCODE_TO_KEYCODE`, `SDL_WINDOWPOS_*`, `SDL_*_TO_NS` / `SDL_NS_TO_*`, `SDL_VERSIONNUM*`, `SDL_VERSION_ATLEAST`, `SDL_MUSTLOCK`, `SDL_min`, `SDL_max`, `SDL_clamp`, `SDL_arraysize`;
  - functions: `SDL_zero`, `SDL_zerop`, `SDL_zeroa`, `SDL_copyp` (its size check a `static_assert`), `SDL_INIT_INTERFACE`, `SDL_iconv_utf8_locale` &c., `SDL_AtomicIncRef`, `SDL_AtomicDecRef`, the barriers and `SDL_CPUPauseInstruction`, `SDL_Unsupported`, `SDL_InvalidParamError`, `SDL_Swap16LE` … `SDL_SwapFloatBE`;
  - `SDL_CreateThread` and `SDL_CreateThreadWithProperties`, which are always macros upstream: they pass the C runtime's thread entry points (`_beginthreadex` on Windows).
- Where upstream applies a macro to enums and plain integers alike (`SDL_AUDIO_BITSIZE(8)`, `SDL_PIXELTYPE(format)`), the function is a template, so the result has the macro's type for either. Elsewhere parameters have the types SDL documents.

`using namespace mcpplibs::mwrap::sdl3::macros;` makes code read as it does in C.

### What stays a macro

These cannot be a constant or a function without changing what they do. They are macros in `<mwrap/sdl3/macros.hpp>`, which is `<SDL3/SDL.h>` with `SDL_vulkan.h` and `SDL_revision.h`:

| Macro | Why |
|---|---|
| `SDL_assert`, `SDL_assert_release`, `SDL_assert_paranoid`, `SDL_assert_always`, `SDL_enabled_assert`, `SDL_disabled_assert` | report the call site, evaluate the condition only when enabled, may break into the debugger there |
| `SDL_ASSERT_LEVEL` | depends on the including file's `NDEBUG` / `SDL_DEFAULT_ASSERT_LEVEL`; used in `#if` |
| `SDL_TriggerBreakpoint`, `SDL_AssertBreakpoint`, `SDL_FUNCTION`, `SDL_FILE`, `SDL_ASSERT_FILE`, `SDL_LINE`, `SDL_NULL_WHILE_LOOP_CONDITION` | the call site, or the assertions' machinery |
| `SDL_COMPILE_TIME_ASSERT`, `SDL_STRINGIFY_ARG` | a declaration with a stringified condition; stringification |
| `SDL_reinterpret_cast`, `SDL_static_cast`, `SDL_const_cast` | take a type |
| `SDL_SINT64_C`, `SDL_UINT64_C` | paste a literal suffix (also used in `#if`) |
| `SDL_stack_alloc`, `SDL_stack_free` | take a type; allocate in the caller's frame |
| `SDL_PLATFORM_*` (`SDL_platform_defines.h`), `SDL_NOLONGLONG`, `SDL_DISABLE_ALLOCA`, `SDL_MEMORY_BARRIER_USES_FUNCTION`, `NO_SDL_VULKAN_TYPEDEFS` | tests for `#if` / `#ifdef` |
| `SDL_DECLSPEC`, `SDLCALL`, `SDL_INLINE`, `SDL_FORCE_INLINE`, `SDL_NODISCARD`, `SDL_FALLTHROUGH`, `SDL_DEPRECATED` … (`SDL_begin_code.h`), the SAL and printf-format annotations, Clang's thread-safety attributes (`SDL_ACQUIRE`, `SDL_GUARDED_BY` …) | annotations of declarations |
| `SDL_BeginThreadFunction`, `SDL_EndThreadFunction` | the thread entry points `SDL_CreateThread` passes, overridable before the header |
| `SDL_ELF_NOTE_DLOPEN` and its helpers (`SDL_dlopennote.h`) | declare an ELF note at the call site |
| the names of `SDL_oldnames.h` | SDL 2's names: renames with `SDL_ENABLE_OLD_NAMES`, poison names that tell the SDL 3 name without it |
| `main` (`SDL_main.h`) | see below |

The printf formats (`SDL_PRIs64`, `SDL_PRIu32` …) are mapped as strings, but `"%" SDL_PRIs64` concatenation needs the macro: include the side header for that. Include it before the import. While a header's macro is defined it expands even inside a qualified name, so `macros::SDL_INIT_VIDEO` is not reachable there; use the plain `SDL_INIT_VIDEO` (see [tests/header_mix_test.cpp](../tests/header_mix_test.cpp)).

Not mapped: the include guards; `SDL_malloc` … `SDL_vsnprintf`, which only clang's static analyzer redirects to the C library; `SDL_memcpy`, `SDL_memmove`, `SDL_memset`, which the header redirects to the C library for the compiler's builtins (the module exports SDL's functions of those names, the same operation); `SDL_Swap16` &c. and the overflow checks, adapted as functions; and `alloca`, the C library's. [tools/policy.py](../tools/policy.py) lists every one with its reason and [tools/check_exports.py](../tools/check_exports.py) accounts for every macro.

## SDL_main.h and the app callbacks

The module includes `<SDL3/SDL_main.h>` with `SDL_MAIN_HANDLED`: it exports `SDL_main` (the declaration), `SDL_SetMainReady`, `SDL_RunApp`, `SDL_EnterAppMainCallbacks`, `SDL_GDKSuspendComplete` and on Windows `SDL_RegisterApp` / `SDL_UnregisterApp`; the callback types (`SDL_AppInit_func` …) come from `SDL_init.h`. It does not rename `main`, and it defines no entry point.

A program that wants SDL's entry point — `WinMain` on Windows, the app callbacks everywhere — includes `<SDL3/SDL_main.h>` itself, in the one file that defines `main` or `SDL_AppInit` / `SDL_AppIterate` / `SDL_AppEvent` / `SDL_AppQuit`, next to the import ([templates/window](../templates/window), [tests/main_test.cpp](../tests/main_test.cpp)). Without `SDL_main.h`, a program can enter the callbacks itself with `SDL_EnterAppMainCallbacks` ([tests/callbacks_test.cpp](../tests/callbacks_test.cpp)). The side header does not include `SDL_main.h`: it is included in many files, and `SDL_main.h` belongs in one.

## Headers the module does not wrap

| Header | Why |
|---|---|
| `SDL_opengl.h`, `SDL_opengl_glext.h`, `SDL_opengles.h`, `SDL_opengles2*.h`, `SDL_egl.h` | Khronos' APIs as SDL ships them, for programs that call OpenGL or EGL directly; they would collide with the system's headers. SDL's own `SDL_GL_*` and `SDL_EGL_*` functions and types are in `SDL_video.h` and exported. Include them directly. |
| `SDL_intrin.h` | which CPU intrinsics the compiler offers (`SDL_SSE2_INTRINSICS` …), for `#if`, and the compiler's intrinsic headers. Include it directly. |
| `SDL_test*.h` | the SDL3_test library, which `compat.sdl3` does not build |
| `SDL_main_impl.h` | the entry-point definitions `SDL_main.h` includes into one file |
| `SDL_copying.h` | the license text |

## Checks

- `mcpp test`: the C API through the module ([tests/](../tests)), the macro mappings at compile time, the header mixed with the import, `SDL_main.h`, the app callbacks, and a GPU device (skipped without one unless `SDL3M_REQUIRE_GPU=1`; `SDL3M_GPU_VIDEO_DRIVER` and `SDL3M_VULKAN_LIBRARY` choose the video driver and the Vulkan loader).
- `python3 tools/check_exports.py`, after `mcpp build`: the generated sources are what `tools/gen_exports.py` writes from the pinned headers; every declaration of the raw headers is exported or excluded with a reason, every macro mapped, kept or excluded with a reason; and on this target, from the compiler's preprocessing of the headers, every declaration is exported, every inline function adapted and every defined macro mapped or listed.
- `python3 tools/run_conformance.py`, after `mcpp build`: SDL's `testautomation` (26 suites) compiled as C++ against the module — its SDL includes replaced by the import and the macros the module does not map — and run as upstream's CTest runs it (dummy video and audio drivers, `SDL_ASSERT=abort`), then again with `SDL_CPU_FEATURE_MASK=-all`. [patches/testautomation-cxx.patch](../patches/testautomation-cxx.patch) makes upstream's C valid C++.
- `python3 tools/gen_exports.py`: regenerates `src/` from the headers and [tools/policy.py](../tools/policy.py).
