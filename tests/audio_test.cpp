// Audio through the module, on SDL's dummy driver: format macros, a
// conversion stream (S16 to F32, resampled), and a playback device stream
// whose callback SDL calls.
#include <gtest/gtest.h>

import std;
import mcpplibs.mwrap.sdl3;

using namespace mcpplibs::mwrap::sdl3::macros;

namespace {

class Audio : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
        ASSERT_TRUE(SDL_Init(SDL_INIT_AUDIO)) << SDL_GetError();
    }
    static void TearDownTestSuite() { SDL_Quit(); }
};

}  // namespace

TEST_F(Audio, FormatMacros) {
    static_assert(SDL_AUDIO_BITSIZE(SDL_AUDIO_S16) == 16 && SDL_AUDIO_BYTESIZE(SDL_AUDIO_F32) == 4);
    static_assert(SDL_AUDIO_ISFLOAT(SDL_AUDIO_F32) && SDL_AUDIO_ISINT(SDL_AUDIO_S32));
    static_assert(SDL_AUDIO_ISSIGNED(SDL_AUDIO_S8) && SDL_AUDIO_ISUNSIGNED(SDL_AUDIO_U8));
    static_assert(SDL_AUDIO_ISBIGENDIAN(SDL_AUDIO_S16BE) && SDL_AUDIO_ISLITTLEENDIAN(SDL_AUDIO_S16LE));
    static_assert(SDL_DEFINE_AUDIO_FORMAT(1, 0, 1, 32) == SDL_AUDIO_F32LE);
    constexpr SDL_AudioSpec spec{SDL_AUDIO_S16, 2, 48000};
    static_assert(SDL_AUDIO_FRAMESIZE(spec) == 4);
    EXPECT_STREQ(SDL_GetAudioFormatName(SDL_AUDIO_F32LE), "SDL_AUDIO_F32LE");
    EXPECT_STREQ(SDL_GetCurrentAudioDriver(), "dummy");
    EXPECT_EQ(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, 0xFFFFFFFFu);
}

TEST_F(Audio, ConversionStream) {
    const SDL_AudioSpec src{SDL_AUDIO_S16, 1, 8000}, dst{SDL_AUDIO_F32, 2, 8000};
    SDL_AudioStream* stream = SDL_CreateAudioStream(&src, &dst);
    ASSERT_NE(stream, nullptr) << SDL_GetError();
    std::array<Sint16, 4> in{0, 16384, -32768, 32767};
    ASSERT_TRUE(SDL_PutAudioStreamData(stream, in.data(), static_cast<int>(sizeof in)));
    ASSERT_TRUE(SDL_FlushAudioStream(stream));
    std::array<float, 8> out{};
    const int got = SDL_GetAudioStreamData(stream, out.data(), static_cast<int>(sizeof out));
    ASSERT_EQ(got, static_cast<int>(sizeof out));
    EXPECT_FLOAT_EQ(out[2], 0.5f);  // mono to stereo: each sample twice
    EXPECT_FLOAT_EQ(out[3], 0.5f);
    EXPECT_FLOAT_EQ(out[4], -1.0f);
    EXPECT_NEAR(out[6], 1.0f, 1e-4f);

    // Resampling 8 kHz to 16 kHz doubles the frames.
    const SDL_AudioSpec hi{SDL_AUDIO_F32, 2, 16000};
    ASSERT_TRUE(SDL_SetAudioStreamFormat(stream, nullptr, &hi));
    std::vector<Sint16> second(800, 1000);
    ASSERT_TRUE(SDL_PutAudioStreamData(stream, second.data(), static_cast<int>(second.size() * sizeof(Sint16))));
    ASSERT_TRUE(SDL_FlushAudioStream(stream));
    const int bytes = SDL_GetAudioStreamAvailable(stream);
    EXPECT_NEAR(bytes / static_cast<int>(2 * sizeof(float)), 1600, 8);
    SDL_DestroyAudioStream(stream);
}

TEST_F(Audio, DevicePlaybackCallback) {
    static SDL_AtomicInt requested;
    SDL_SetAtomicInt(&requested, 0);
    const SDL_AudioSpec spec{SDL_AUDIO_F32, 2, 48000};
    SDL_AudioStream* stream = SDL_OpenAudioDeviceStream(
        SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec,
        [](void*, SDL_AudioStream* s, int additional, int) {
            std::vector<float> silence(static_cast<size_t>(additional) / sizeof(float), 0.0f);
            SDL_PutAudioStreamData(s, silence.data(), additional);
            SDL_AddAtomicInt(&requested, 1);
        },
        nullptr);
    ASSERT_NE(stream, nullptr) << SDL_GetError();
    const SDL_AudioDeviceID device = SDL_GetAudioStreamDevice(stream);
    EXPECT_NE(device, 0u);
    ASSERT_TRUE(SDL_ResumeAudioStreamDevice(stream));
    for (int i = 0; i < 200 && SDL_GetAtomicInt(&requested) < 2; ++i) SDL_Delay(10);
    EXPECT_GE(SDL_GetAtomicInt(&requested), 2);
    SDL_AudioSpec got{};
    int frames = 0;
    EXPECT_TRUE(SDL_GetAudioDeviceFormat(device, &got, &frames));
    EXPECT_GT(frames, 0);
    EXPECT_TRUE(SDL_PauseAudioStreamDevice(stream));
    SDL_DestroyAudioStream(stream);
}
