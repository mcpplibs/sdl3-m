# sdl3

> 通过 C++23 模块使用 SDL 3——跨平台的窗口、输入、音频、GPU 与平台层。

[English](README.md) · **简体中文** · [Upstream](https://github.com/libsdl-org/SDL) · [Releases](https://github.com/mcpplibs/sdl3-m/releases) · [mcpp](https://github.com/mcpp-community/mcpp) · [Index](https://github.com/mcpplibs/mcpp-index)

[![C++23](https://img.shields.io/badge/C%2B%2B-23-blue.svg)](https://en.cppreference.com/w/cpp/23)
[![License](https://img.shields.io/badge/license-Apache--2.0-blue.svg)](LICENSE)
[![mcpp-community](https://img.shields.io/badge/mcpp--community-mwrap-blue)](https://github.com/mcpp-community)
[![CI](https://github.com/mcpplibs/sdl3-m/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/mcpplibs/sdl3-m/actions/workflows/ci.yml)

上游 C API 保持不变：`SDL_Init`、`SDL_Window`、`SDL_Event`、`SDL_CreateGPUDevice`
就是 `<SDL3/SDL.h>`（以及 `SDL_vulkan.h`、`SDL_main.h` 的函数和 `SDL_revision.h`）
里的上游全局名字：每个函数、类型和枚举值，在声明它的平台上导出。库本身是
`compat.sdl3` 3.4.18。本包把头文件里的内联 API（`SDL_PointInRect`、`SDL_Swap32` …）
做了适配，并把宏以原名映射为常量和函数（`SDL_INIT_VIDEO`、`SDLK_A`、`SDL_HINT_*`、
`SDL_PROP_*`、`SDL_BUTTON_MASK`、`SDL_CreateThread` …）。SDL 自己的自动化测试套件
针对模块编译，在每次 CI 中全部通过。

## 模块

- `import mcpplibs.mwrap.sdl3;`：C API，内联 API 位于 `mcpplibs::mwrap::sdl3`，并重新导出下面的模块
- `import mcpplibs.mwrap.sdl3.macros;`：`SDL_INIT_VIDEO`、`SDL_WINDOW_RESIZABLE`、`SDLK_A`、`SDL_BITSPERPIXEL` …，位于 `mcpplibs::mwrap::sdl3::macros`

> 更多模块 API -> [用法与 API](docs/usage.md)

## 快速开始

```sh
mcpp new sdlinfo --template mcpplibs.mwrap.sdl3@3.4.18
cd sdlinfo
mcpp run
```

默认模板（`info`）打印 SDL 在这台机器上看到的一切：版本、CPU 特性、视频/渲染/
音频/GPU 驱动、显示器、一个 GPU 设备及其着色器格式。它不需要窗口或桌面；没有显示器时
通过 SDL 的 offscreen 驱动报告。`window` 模板是基于 SDL 应用回调的画板，需要桌面环境：
`mcpp new pad --template mcpplibs.mwrap.sdl3@3.4.18:window`。

```toml
[dependencies]
"mcpplibs.mwrap.sdl3" = "3.4.18"
```

```cpp
import std;
import mcpplibs.mwrap.sdl3;
using namespace mcpplibs::mwrap::sdl3::macros;  // SDL_INIT_VIDEO、SDL_WINDOW_RESIZABLE、SDLK_ESCAPE …

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

## 内联函数与宏

`SDL_FORCE_INLINE` 函数具有内部链接，无法导出；16 个公开的内联函数以原名适配在
`mcpplibs::mwrap::sdl3` 中，各自调用上游实现。宏被映射为与宏本身类型相同的
`inline constexpr` 常量（字符串为 `const char[]`），以及函数体就是宏本身、每个参数
只求值一次的函数；拆解数值的宏是模板，因为上游既把它们用于枚举也用于整数。断言、
`SDL_stack_alloc`、`SDL_COMPILE_TIME_ASSERT`、`SDL_PLATFORM_*` 平台判断、属性标注和
SDL 2 的旧名字仍是宏，位于 `<mwrap/sdl3/macros.hpp>`，在 import 之前包含；
`"%" SDL_PRIs64` 这样的字面量拼接也用它。每个宏的去向见 [docs/usage.md](docs/usage.md)。

## SDL_main.h 与应用回调

模块以 `SDL_MAIN_HANDLED` 包含 `<SDL3/SDL_main.h>`，因此导出 `SDL_SetMainReady`、
`SDL_RunApp`、`SDL_EnterAppMainCallbacks` 和回调类型，而不导出对 `main` 的重定义。
需要 SDL 入口点的程序在定义 `main` 或 `SDL_App*` 回调的那一个文件里自行包含
`<SDL3/SDL_main.h>`，与 import 并用（`window` 模板就是这样做的）。

## 平台

Linux、macOS 和 Windows。只属于某个平台的声明只在该平台导出（`SDL_SetLinuxThreadPriority`、
`SDL_GetDXGIOutputInfo`、`MSG` …）。`SDL_opengl*.h`、`SDL_egl.h`（SDL 附带的 Khronos API；
`SDL_GL_*` 和 `SDL_EGL_*` 已导出）、`SDL_intrin.h` 以及 `SDL_test` 头文件不做封装，
请直接包含。无界面运行使用 `SDL_VIDEO_DRIVER=offscreen` 或 `dummy`，以及
`SDL_AUDIO_DRIVER=dummy`。

## 构建与测试

Linux（GCC 16、LLVM 23）、macOS 和 Windows 的 CI 构建模块并运行：

- 测试：经由模块调用的 API（初始化、属性、hint、事件、定时器、线程、IO、文件系统、
  像素、surface、渲染器、音频、lavapipe 或 Metal 上的 GPU 设备）、宏映射、头文件与
  import 混用、`SDL_main.h` 与应用回调；
- 导出审计（[tools/check_exports.py](tools/check_exports.py)）：每一行都对头文件做预处理，
  检查那里的每个声明都已导出、每个内联函数都已适配、每个宏都已映射或列明原因；
- SDL 的 `testautomation`（26 个套件、377 个测试）：其 C 代码作为 C++ 针对模块编译，
  按上游 CTest 的方式运行，分别启用和禁用 SIMD
  （[tools/run_conformance.py](tools/run_conformance.py)）。

另有一个任务把两个模板当作已安装的包生成并构建，并运行默认模板。

## 许可

封装部分采用 [Apache-2.0](LICENSE)，由 mcpp-community 贡献（[NOTICE](NOTICE)）；
SDL 仍为 zlib 许可（[upstream/sdl3/LICENSE.txt](upstream/sdl3/LICENSE.txt)）。
