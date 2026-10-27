// SDL 3 through the module: initialisation, errors, properties, hints,
// events, timers, threads and atomics, logging, time, versions — every call
// spelled as SDL's documentation spells it, the macros from the mapping. The
// video subsystem runs on SDL's offscreen driver, so no display is needed.
#include <gtest/gtest.h>

import std;
import mcpplibs.mwrap.sdl3;

using namespace mcpplibs::mwrap::sdl3::macros;  // SDL_INIT_VIDEO, SDL_HINT_*, SDL_PROP_* ...

namespace {

class Sdl : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        ASSERT_TRUE(SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "offscreen"));
        ASSERT_TRUE(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) << SDL_GetError();
    }
    static void TearDownTestSuite() { SDL_Quit(); }
};

}  // namespace

TEST_F(Sdl, VersionAndInit) {
    EXPECT_EQ(SDL_GetVersion(), SDL_VERSION);
    EXPECT_EQ(SDL_VERSIONNUM_MAJOR(SDL_GetVersion()), 3);
    EXPECT_EQ(SDL_VERSION, SDL_VERSIONNUM(3, 4, 18));
    EXPECT_TRUE(SDL_VERSION_ATLEAST(3, 4, 0));
    ASSERT_NE(SDL_GetRevision(), nullptr);
    EXPECT_EQ(SDL_WasInit(SDL_INIT_VIDEO | SDL_INIT_AUDIO), SDL_INIT_VIDEO);
    EXPECT_STREQ(SDL_GetCurrentVideoDriver(), "offscreen");
    EXPECT_TRUE(SDL_InitSubSystem(SDL_INIT_EVENTS));  // counted: a second init is fine
    SDL_QuitSubSystem(SDL_INIT_EVENTS);
    EXPECT_NE(SDL_WasInit(SDL_INIT_EVENTS), 0u);
    EXPECT_GT(SDL_GetNumVideoDrivers(), 0);
    EXPECT_FALSE(std::string_view(SDL_GetPlatform()).empty());
}

TEST_F(Sdl, Errors) {
    SDL_ClearError();
    EXPECT_STREQ(SDL_GetError(), "");
    EXPECT_FALSE(SDL_SetError("code %d: %s", 42, "nope"));  // the printf-format declaration
    EXPECT_STREQ(SDL_GetError(), "code 42: nope");
    EXPECT_FALSE(SDL_Unsupported());
    EXPECT_STREQ(SDL_GetError(), "That operation is not supported");
    EXPECT_FALSE(SDL_InvalidParamError("window"));
    EXPECT_STREQ(SDL_GetError(), "Parameter 'window' is invalid");
    EXPECT_FALSE(SDL_OutOfMemory());
    EXPECT_TRUE(SDL_ClearError());
}

TEST_F(Sdl, Properties) {
    const SDL_PropertiesID props = SDL_CreateProperties();
    ASSERT_NE(props, 0u);
    EXPECT_TRUE(SDL_SetNumberProperty(props, "n", -7));
    EXPECT_TRUE(SDL_SetFloatProperty(props, "f", 0.5f));
    EXPECT_TRUE(SDL_SetBooleanProperty(props, "b", true));
    EXPECT_TRUE(SDL_SetStringProperty(props, "s", "text"));
    EXPECT_EQ(SDL_GetNumberProperty(props, "n", 0), -7);
    EXPECT_EQ(SDL_GetFloatProperty(props, "f", 0), 0.5f);
    EXPECT_TRUE(SDL_GetBooleanProperty(props, "b", false));
    EXPECT_STREQ(SDL_GetStringProperty(props, "s", ""), "text");
    EXPECT_EQ(SDL_GetPropertyType(props, "s"), SDL_PROPERTY_TYPE_STRING);
    EXPECT_EQ(SDL_GetPropertyType(props, "missing"), SDL_PROPERTY_TYPE_INVALID);
    EXPECT_EQ(SDL_GetNumberProperty(props, "missing", 99), 99);

    static int cleaned = 0;
    int payload = 5;
    EXPECT_TRUE(SDL_SetPointerPropertyWithCleanup(props, "p", &payload,
                                                  [](void* userdata, void* value) {
                                                      cleaned += *static_cast<int*>(value) * *static_cast<int*>(userdata);
                                                  },
                                                  &payload));
    EXPECT_EQ(SDL_GetPointerProperty(props, "p", nullptr), &payload);

    std::vector<std::string> names;
    EXPECT_TRUE(SDL_EnumerateProperties(
        props, [](void* userdata, SDL_PropertiesID, const char* name) { static_cast<std::vector<std::string>*>(userdata)->emplace_back(name); },
        &names));
    std::ranges::sort(names);
    EXPECT_EQ(names, (std::vector<std::string>{"b", "f", "n", "p", "s"}));

    const SDL_PropertiesID copy = SDL_CreateProperties();
    EXPECT_TRUE(SDL_CopyProperties(props, copy));
    EXPECT_STREQ(SDL_GetStringProperty(copy, "s", ""), "text");
    EXPECT_TRUE(SDL_ClearProperty(props, "s"));
    EXPECT_FALSE(SDL_HasProperty(props, "s"));
    EXPECT_TRUE(SDL_LockProperties(props));
    SDL_UnlockProperties(props);
    SDL_DestroyProperties(copy);
    SDL_DestroyProperties(props);
    EXPECT_EQ(cleaned, 25);
    EXPECT_NE(SDL_GetGlobalProperties(), 0u);
}

TEST_F(Sdl, Hints) {
    static std::vector<std::string> seen;
    const char* name = "SDL3M_TEST_HINT";
    EXPECT_TRUE(SDL_AddHintCallback(
        name, [](void*, const char*, const char*, const char* value) { seen.emplace_back(value ? value : "(null)"); }, nullptr));
    EXPECT_TRUE(SDL_SetHint(name, "1"));
    EXPECT_STREQ(SDL_GetHint(name), "1");
    EXPECT_TRUE(SDL_GetHintBoolean(name, false));
    EXPECT_TRUE(SDL_SetHintWithPriority(name, "0", SDL_HINT_OVERRIDE));
    EXPECT_FALSE(SDL_SetHintWithPriority(name, "1", SDL_HINT_NORMAL));  // an override wins
    EXPECT_STREQ(SDL_GetHint(name), "0");
    EXPECT_TRUE(SDL_ResetHint(name));
    SDL_RemoveHintCallback(name, nullptr, nullptr);
    EXPECT_GE(seen.size(), 3u);
    EXPECT_EQ(seen.front(), "(null)");  // the callback is called with the value when added
    EXPECT_STREQ(SDL_HINT_VIDEO_DRIVER, "SDL_VIDEO_DRIVER");
}

TEST_F(Sdl, EventsPushPollWatch) {
    const Uint32 type = SDL_RegisterEvents(2);
    ASSERT_NE(type, 0u);
    SDL_FlushEvents(SDL_EVENT_FIRST, SDL_EVENT_LAST);

    static int watched = 0;
    auto watch = [](void* userdata, SDL_Event* e) {
        if (e->type == *static_cast<Uint32*>(userdata)) ++watched;
        return true;
    };
    Uint32 watched_type = type;
    EXPECT_TRUE(SDL_AddEventWatch(watch, &watched_type));

    for (int i = 0; i < 3; ++i) {
        SDL_Event e{};
        e.type = type;
        e.user.code = i;
        e.user.data1 = &watched;
        EXPECT_TRUE(SDL_PushEvent(&e));
    }
    EXPECT_TRUE(SDL_HasEvent(type));
    SDL_Event peeked[3];
    EXPECT_EQ(SDL_PeepEvents(peeked, 3, SDL_PEEKEVENT, type, type), 3);
    EXPECT_EQ(peeked[2].user.code, 2);

    int codes = 0, polled = 0;
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == type) {
            codes += e.user.code;
            ++polled;
            EXPECT_EQ(e.user.data1, &watched);
        }
    }
    EXPECT_EQ(polled, 3);
    EXPECT_EQ(codes, 0 + 1 + 2);
    EXPECT_EQ(watched, 3);
    SDL_RemoveEventWatch(watch, &watched_type);

    // A filter drops what it returns false for, before the queue.
    SDL_SetEventFilter([](void*, SDL_Event* ev) { return ev->type != SDL_EVENT_QUIT; }, nullptr);
    SDL_Event quit{};
    quit.type = SDL_EVENT_QUIT;
    EXPECT_FALSE(SDL_PushEvent(&quit));
    EXPECT_FALSE(SDL_HasEvent(SDL_EVENT_QUIT));
    SDL_SetEventFilter(nullptr, nullptr);
    EXPECT_TRUE(SDL_PushEvent(&quit));
    EXPECT_TRUE(SDL_WaitEventTimeout(&e, 1000));
    EXPECT_EQ(e.type, SDL_EVENT_QUIT);
    EXPECT_FALSE(SDL_WaitEventTimeout(&e, 10));  // nothing left: a timeout
}

TEST_F(Sdl, Timers) {
    const Uint64 t0 = SDL_GetTicksNS();
    SDL_Delay(5);
    EXPECT_GE(SDL_GetTicksNS() - t0, SDL_MS_TO_NS(5));
    EXPECT_GT(SDL_GetPerformanceFrequency(), 0u);
    EXPECT_EQ(SDL_NS_TO_MS(SDL_SECONDS_TO_NS(2)), 2000u);
    EXPECT_EQ(SDL_NS_TO_US(SDL_US_TO_NS(7)), 7u);
    EXPECT_EQ(SDL_NS_TO_SECONDS(-SDL_NS_PER_SECOND * 3), -3);  // signed, as the macro is

    // A timer's callback runs on SDL's timer thread; it reports through an event.
    static Uint32 timer_event = SDL_RegisterEvents(1);
    static SDL_AtomicInt fired;
    SDL_SetAtomicInt(&fired, 0);
    const SDL_TimerID id = SDL_AddTimer(
        1,
        [](void*, SDL_TimerID, Uint32) -> Uint32 {
            if (SDL_AtomicIncRef(&fired) == 2) {  // the third call: stop, and say so
                SDL_Event e{};
                e.type = timer_event;
                SDL_PushEvent(&e);
                return 0;
            }
            return 1;
        },
        nullptr);
    ASSERT_NE(id, 0u);
    SDL_Event e{};
    bool got = false;
    for (int i = 0; i < 200 && !got; ++i) got = SDL_WaitEventTimeout(&e, 10) && e.type == timer_event;
    EXPECT_TRUE(got);
    EXPECT_EQ(SDL_GetAtomicInt(&fired), 3);

    const SDL_TimerID ns = SDL_AddTimerNS(SDL_MS_TO_NS(1000), [](void*, SDL_TimerID, Uint64) -> Uint64 { return 0; }, nullptr);
    EXPECT_TRUE(SDL_RemoveTimer(ns));
    EXPECT_FALSE(SDL_RemoveTimer(ns));
}

TEST_F(Sdl, ThreadsMutexesAtomics) {
    struct Shared {
        SDL_Mutex* lock = SDL_CreateMutex();
        SDL_Condition* ready = SDL_CreateCondition();
        SDL_Semaphore* done = SDL_CreateSemaphore(0);
        int counter = 0;
        SDL_AtomicInt hits{};
    } shared;
    ASSERT_NE(shared.lock, nullptr);

    auto work = [](void* data) -> int {
        auto* s = static_cast<Shared*>(data);
        for (int i = 0; i < 1000; ++i) {
            SDL_LockMutex(s->lock);
            ++s->counter;
            SDL_UnlockMutex(s->lock);
            SDL_AddAtomicInt(&s->hits, 1);
        }
        SDL_SignalSemaphore(s->done);
        return 7;
    };
    SDL_Thread* threads[4];
    for (auto& t : threads) {
        t = SDL_CreateThread(work, "sdl3-m worker", &shared);  // the macro's mapping
        ASSERT_NE(t, nullptr) << SDL_GetError();
    }
    for (int i = 0; i < 4; ++i) EXPECT_TRUE(SDL_WaitSemaphoreTimeout(shared.done, 10000));
    for (auto* t : threads) {
        int status = 0;
        EXPECT_STREQ(SDL_GetThreadName(t), "sdl3-m worker");
        SDL_WaitThread(t, &status);
        EXPECT_EQ(status, 7);
    }
    EXPECT_EQ(shared.counter, 4000);
    EXPECT_EQ(SDL_GetAtomicInt(&shared.hits), 4000);

    // SDL_CreateThreadWithProperties, also a macro over the runtime's entry points.
    const SDL_PropertiesID props = SDL_CreateProperties();
    SDL_SetPointerProperty(props, SDL_PROP_THREAD_CREATE_ENTRY_FUNCTION_POINTER, reinterpret_cast<void*>(+work));
    SDL_SetStringProperty(props, SDL_PROP_THREAD_CREATE_NAME_STRING, "with properties");
    SDL_SetPointerProperty(props, SDL_PROP_THREAD_CREATE_USERDATA_POINTER, &shared);
    SDL_Thread* t = SDL_CreateThreadWithProperties(props);
    SDL_DestroyProperties(props);
    ASSERT_NE(t, nullptr) << SDL_GetError();
    int status = 0;
    SDL_WaitThread(t, &status);
    EXPECT_EQ(status, 7);
    EXPECT_EQ(shared.counter, 5000);

    SDL_AtomicInt a;
    SDL_SetAtomicInt(&a, 1);
    EXPECT_FALSE(SDL_AtomicDecRef(&a) == false);  // 1 -> 0: the last reference
    EXPECT_TRUE(SDL_CompareAndSwapAtomicInt(&a, 0, 5));
    EXPECT_EQ(SDL_GetAtomicInt(&a), 5);
    SDL_MemoryBarrierRelease();
    SDL_MemoryBarrierAcquire();
    SDL_CompilerBarrier();
    SDL_CPUPauseInstruction();
    SDL_SpinLock spin = 0;
    EXPECT_TRUE(SDL_TryLockSpinlock(&spin));
    EXPECT_FALSE(SDL_TryLockSpinlock(&spin));
    SDL_UnlockSpinlock(&spin);

    static SDL_TLSID tls;
    int value = 3;
    EXPECT_TRUE(SDL_SetTLS(&tls, &value, nullptr));
    EXPECT_EQ(SDL_GetTLS(&tls), &value);
    EXPECT_NE(SDL_GetCurrentThreadID(), 0u);

    SDL_DestroySemaphore(shared.done);
    SDL_DestroyCondition(shared.ready);
    SDL_DestroyMutex(shared.lock);
}

TEST_F(Sdl, LogOutput) {
    static std::vector<std::pair<SDL_LogPriority, std::string>> logged;
    SDL_LogOutputFunction previous = nullptr;
    void* previous_data = nullptr;
    SDL_GetLogOutputFunction(&previous, &previous_data);
    SDL_SetLogOutputFunction([](void*, int, SDL_LogPriority priority, const char* message) { logged.emplace_back(priority, message); },
                             nullptr);
    SDL_SetLogPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);
    SDL_Log("value %d", 3);
    SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "below the priority");
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s", "an error");
    SDL_SetLogOutputFunction(previous, previous_data);
    ASSERT_EQ(logged.size(), 2u);
    EXPECT_EQ(logged[0], std::pair(SDL_LOG_PRIORITY_INFO, std::string("value 3")));
    EXPECT_EQ(logged[1].first, SDL_LOG_PRIORITY_ERROR);
    SDL_ResetLogPriorities();
}

TEST_F(Sdl, TimeAndDates) {
    SDL_Time now = 0;
    ASSERT_TRUE(SDL_GetCurrentTime(&now));
    SDL_DateTime dt{};
    ASSERT_TRUE(SDL_TimeToDateTime(now, &dt, false));
    EXPECT_GE(dt.year, 2024);
    SDL_Time back = 0;
    ASSERT_TRUE(SDL_DateTimeToTime(&dt, &back));
    EXPECT_EQ(back / SDL_NS_PER_SECOND, now / SDL_NS_PER_SECOND);
    EXPECT_EQ(SDL_GetDaysInMonth(2024, 2), 29);
    EXPECT_EQ(SDL_GetDayOfWeek(2026, 10, 28), 3);  // a Wednesday
}

TEST_F(Sdl, CpuPowerLocaleGuid) {
    EXPECT_GE(SDL_GetNumLogicalCPUCores(), 1);
    EXPECT_GT(SDL_GetSystemRAM(), 0);
    EXPECT_GE(SDL_GetSIMDAlignment(), sizeof(void*));
    int seconds = 0, percent = 0;
    EXPECT_NE(SDL_GetPowerInfo(&seconds, &percent), SDL_POWERSTATE_ERROR);
    int count = 0;
    SDL_Locale** locales = SDL_GetPreferredLocales(&count);
    SDL_free(locales);  // may be none on a CI machine

    const SDL_GUID guid = SDL_StringToGUID("030000005e0400008e02000014010000");
    char text[33];
    SDL_GUIDToString(guid, text, sizeof text);
    EXPECT_STREQ(text, "030000005e0400008e02000014010000");
}

TEST_F(Sdl, PlatformDeclarations) {
    // Declared by SDL_system.h for one platform, exported only there.
#if defined(__linux__) && !defined(__ANDROID__)
    EXPECT_TRUE((std::is_same_v<decltype(&SDL_SetLinuxThreadPriority), bool (*)(Sint64, int)>));
#elif defined(_WIN32)
    EXPECT_TRUE((std::is_same_v<decltype(&SDL_GetDXGIOutputInfo), bool (*)(SDL_DisplayID, int*, int*)>));
    EXPECT_TRUE((std::is_same_v<decltype(&SDL_SetWindowsMessageHook), void (*)(SDL_WindowsMessageHook, void*)>));
#endif
    EXPECT_TRUE((std::is_same_v<decltype(&SDL_SetMainReady), void (*)()>));  // SDL_main.h's functions
    SDL_SetMainReady();
}
