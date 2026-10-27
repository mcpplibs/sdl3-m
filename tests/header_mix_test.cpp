// The raw header and the import in one translation unit (the side header
// first, the canonical order): the module's names are the very same
// entities, and the macros a constant or a function cannot be — the
// assertions, SDL_stack_alloc, SDL_COMPILE_TIME_ASSERT, the platform tests,
// literal pasting — work beside them. While a header's macro is defined it
// expands even inside a qualified name, so `macros::SDL_INIT_VIDEO` is only
// reachable with the macro set aside.
#include <mwrap/sdl3/macros.hpp>
#include <gtest/gtest.h>

import std;
import mcpplibs.mwrap.sdl3;

namespace {
constexpr auto raw_video = SDL_INIT_VIDEO;
constexpr auto raw_resizable = SDL_WINDOW_RESIZABLE;

SDL_COMPILE_TIME_ASSERT(uint64_literal, SDL_UINT64_C(0xFFFFFFFFFFFFFFFF) == SDL_MAX_UINT64);

int failed_assertions = 0;
SDL_AssertState SDLCALL count_and_ignore(const SDL_AssertData* data, void*) {
    ++failed_assertions;
    EXPECT_STREQ(data->condition, "1 + 1 == 3");
    return SDL_ASSERTION_IGNORE;
}
}  // namespace

#pragma push_macro("SDL_INIT_VIDEO")
#pragma push_macro("SDL_WINDOW_RESIZABLE")
#undef SDL_INIT_VIDEO
#undef SDL_WINDOW_RESIZABLE
static_assert(mcpplibs::mwrap::sdl3::macros::SDL_INIT_VIDEO == raw_video);
static_assert(mcpplibs::mwrap::sdl3::macros::SDL_WINDOW_RESIZABLE == raw_resizable);
static_assert(std::is_same_v<decltype(mcpplibs::mwrap::sdl3::macros::SDL_WINDOW_RESIZABLE), decltype(raw_resizable)>);
#pragma pop_macro("SDL_WINDOW_RESIZABLE")
#pragma pop_macro("SDL_INIT_VIDEO")

TEST(HeaderMix, SameEntities) {
    static_assert(std::is_same_v<decltype(&::SDL_Init), bool (*)(SDL_InitFlags)>);
    static_assert(std::is_same_v<SDL_Event, ::SDL_Event>);
    static_assert(SDL_EVENT_QUIT == ::SDL_EVENT_QUIT);
    // The adapted inline function and the header's are different functions
    // with the same result.
    const SDL_Rect r{0, 0, 4, 4};
    const SDL_Point p{1, 1};
    EXPECT_EQ(mcpplibs::mwrap::sdl3::SDL_PointInRect(&p, &r), ::SDL_PointInRect(&p, &r));
    // SDL_Swap16 is a function-like macro here: parentheses keep the name a name.
    EXPECT_EQ((mcpplibs::mwrap::sdl3::SDL_Swap16)(0x1234), SDL_Swap16(0x1234));
}

TEST(HeaderMix, MacrosThatStayMacros) {
#if !defined(SDL_PLATFORM_LINUX) && !defined(SDL_PLATFORM_APPLE) && !defined(SDL_PLATFORM_WINDOWS)
    FAIL() << "no SDL_PLATFORM_* test matched";
#endif
    char* scratch = SDL_stack_alloc(char, 16);  // alloca in this frame
    ASSERT_NE(scratch, nullptr);
    SDL_snprintf(scratch, 16, "%" SDL_PRIu64, SDL_UINT64_C(18446744073709551615));  // literal concatenation
    EXPECT_STREQ(scratch, "184467440737095");  // 15 characters and the terminator
    SDL_stack_free(scratch);
    EXPECT_STREQ(SDL_STRINGIFY_ARG(SDL_WINDOW_RESIZABLE), "SDL_WINDOW_RESIZABLE");
    EXPECT_EQ(SDL_static_cast(int, 2.9), 2);

    SDL_SetAssertionHandler(count_and_ignore, nullptr);
    SDL_assert_always(1 + 1 == 3);  // reported, ignored
    SDL_assert_always(1 + 1 == 2);
    SDL_SetAssertionHandler(nullptr, nullptr);
    EXPECT_EQ(failed_assertions, 1);
    const SDL_AssertData* report = SDL_GetAssertionReport();
    ASSERT_NE(report, nullptr);
    EXPECT_GT(report->linenum, 0);
    EXPECT_NE(std::string_view(report->filename).find("header_mix_test"), std::string_view::npos);
    SDL_ResetAssertionReport();
}
