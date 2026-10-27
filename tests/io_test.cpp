// Streams, files and the C library SDL carries, through the module:
// SDL_IOStream over memory, a file and an interface of our own
// (SDL_INIT_INTERFACE), the endian readers and writers, the filesystem, and
// SDL_stdinc's strings, conversions, sorting and checksums with their
// adapted inline helpers.
#include <gtest/gtest.h>

import std;
import mcpplibs.mwrap.sdl3;

using namespace mcpplibs::mwrap::sdl3;
using namespace mcpplibs::mwrap::sdl3::macros;

TEST(IO, DynamicMemoryAndEndianness) {
    SDL_IOStream* io = SDL_IOFromDynamicMem();
    ASSERT_NE(io, nullptr);
    EXPECT_TRUE(SDL_WriteU32LE(io, 0x11223344u));
    EXPECT_TRUE(SDL_WriteU16BE(io, 0xA1B2));
    EXPECT_TRUE(SDL_WriteS64LE(io, -2));
    EXPECT_EQ(SDL_IOprintf(io, "<%s:%d>", "n", 5), 5u);
    EXPECT_EQ(SDL_GetIOSize(io), 4 + 2 + 8 + 5);
    ASSERT_EQ(SDL_SeekIO(io, 0, SDL_IO_SEEK_SET), 0);
    Uint8 first = 0;
    EXPECT_TRUE(SDL_ReadU8(io, &first));
    EXPECT_EQ(first, 0x44);  // little-endian
    ASSERT_EQ(SDL_SeekIO(io, 0, SDL_IO_SEEK_SET), 0);
    Uint32 u32 = 0;
    Uint16 u16 = 0;
    Sint64 s64 = 0;
    EXPECT_TRUE(SDL_ReadU32LE(io, &u32));
    EXPECT_TRUE(SDL_ReadU16BE(io, &u16));
    EXPECT_TRUE(SDL_ReadS64LE(io, &s64));
    EXPECT_EQ(u32, 0x11223344u);
    EXPECT_EQ(u16, 0xA1B2);
    EXPECT_EQ(s64, -2);
    char text[8] = {};
    EXPECT_EQ(SDL_ReadIO(io, text, 5), 5u);
    EXPECT_STREQ(text, "<n:5>");
    EXPECT_EQ(SDL_ReadIO(io, text, 1), 0u);
    EXPECT_EQ(SDL_GetIOStatus(io), SDL_IO_STATUS_EOF);
    EXPECT_TRUE(SDL_CloseIO(io));

    static const char data[] = "constant bytes";
    SDL_IOStream* c = SDL_IOFromConstMem(data, sizeof data - 1);
    size_t size = 0;
    char* copy = static_cast<char*>(SDL_LoadFile_IO(c, &size, true));
    ASSERT_NE(copy, nullptr);
    EXPECT_EQ(std::string_view(copy, size), "constant bytes");
    SDL_free(copy);
}

TEST(IO, CustomInterface) {
    // A stream SDL reads from an interface of ours: SDL_INIT_INTERFACE sets
    // its version, as the macro does.
    struct Source {
        std::string bytes;
        size_t at = 0;
    } source{"0123456789"};
    SDL_IOStreamInterface iface;
    SDL_INIT_INTERFACE(&iface);
    EXPECT_EQ(iface.version, sizeof iface);
    EXPECT_EQ(iface.read, nullptr);
    iface.size = [](void* u) -> Sint64 { return static_cast<Sint64>(static_cast<Source*>(u)->bytes.size()); };
    iface.seek = [](void* u, Sint64 offset, SDL_IOWhence whence) -> Sint64 {
        auto* s = static_cast<Source*>(u);
        const Sint64 base = whence == SDL_IO_SEEK_SET ? 0 : whence == SDL_IO_SEEK_CUR ? static_cast<Sint64>(s->at)
                                                                                      : static_cast<Sint64>(s->bytes.size());
        s->at = static_cast<size_t>(base + offset);
        return static_cast<Sint64>(s->at);
    };
    iface.read = [](void* u, void* ptr, size_t n, SDL_IOStatus* status) -> size_t {
        auto* s = static_cast<Source*>(u);
        n = std::min(n, s->bytes.size() - s->at);
        std::memcpy(ptr, s->bytes.data() + s->at, n);
        s->at += n;
        if (n == 0) *status = SDL_IO_STATUS_EOF;
        return n;
    };
    iface.close = [](void*) { return true; };
    SDL_IOStream* io = SDL_OpenIO(&iface, &source);
    ASSERT_NE(io, nullptr) << SDL_GetError();
    EXPECT_EQ(SDL_GetIOSize(io), 10);
    EXPECT_EQ(SDL_SeekIO(io, -3, SDL_IO_SEEK_END), 7);
    char tail[4] = {};
    EXPECT_EQ(SDL_ReadIO(io, tail, 3), 3u);
    EXPECT_STREQ(tail, "789");
    EXPECT_TRUE(SDL_CloseIO(io));
}

TEST(IO, FilesAndFilesystem) {
    const char* base = SDL_GetBasePath();
    ASSERT_NE(base, nullptr) << SDL_GetError();
    const std::string dir = std::string(base) + "sdl3-m-io-test";
    SDL_RemovePath((dir + "/a.txt").c_str());
    SDL_RemovePath(dir.c_str());
    ASSERT_TRUE(SDL_CreateDirectory(dir.c_str())) << SDL_GetError();
    SDL_PathInfo info{};
    ASSERT_TRUE(SDL_GetPathInfo(dir.c_str(), &info));
    EXPECT_EQ(info.type, SDL_PATHTYPE_DIRECTORY);

    const std::string file = dir + "/a.txt";
    ASSERT_TRUE(SDL_SaveFile(file.c_str(), "hello, file", 11)) << SDL_GetError();
    size_t size = 0;
    void* loaded = SDL_LoadFile(file.c_str(), &size);
    ASSERT_NE(loaded, nullptr);
    EXPECT_EQ(std::string_view(static_cast<char*>(loaded), size), "hello, file");
    SDL_free(loaded);
    SDL_IOStream* io = SDL_IOFromFile(file.c_str(), "ab");
    ASSERT_NE(io, nullptr);
    EXPECT_EQ(SDL_WriteIO(io, "!", 1), 1u);
    EXPECT_TRUE(SDL_FlushIO(io));
    EXPECT_TRUE(SDL_CloseIO(io));
    ASSERT_TRUE(SDL_GetPathInfo(file.c_str(), &info));
    EXPECT_EQ(info.size, 12u);

    int count = 0;
    char** found = SDL_GlobDirectory(dir.c_str(), "*.TXT", SDL_GLOB_CASEINSENSITIVE, &count);
    ASSERT_NE(found, nullptr);
    ASSERT_EQ(count, 1);
    EXPECT_STREQ(found[0], "a.txt");
    SDL_free(found);

    const std::string renamed = dir + "/b.txt";
    EXPECT_TRUE(SDL_RenamePath(file.c_str(), renamed.c_str()));
    EXPECT_TRUE(SDL_CopyFile(renamed.c_str(), file.c_str()));
    EXPECT_TRUE(SDL_RemovePath(renamed.c_str()));
    EXPECT_TRUE(SDL_RemovePath(file.c_str()));
    EXPECT_TRUE(SDL_RemovePath(dir.c_str()));
    EXPECT_FALSE(SDL_GetPathInfo(dir.c_str(), &info));
    char* cwd = SDL_GetCurrentDirectory();
    EXPECT_NE(cwd, nullptr);
    SDL_free(cwd);
}

TEST(Stdinc, StringsAndConversions) {
    char buf[32];
    EXPECT_EQ(SDL_snprintf(buf, sizeof buf, "%s-%04d-%.1f", "x", 42, 2.5), 10);
    EXPECT_STREQ(buf, "x-0042-2.5");
    EXPECT_EQ(SDL_strlcpy(buf, "truncate me", 5), 11u);
    EXPECT_STREQ(buf, "trun");
    EXPECT_EQ(SDL_strtol("-0x1f", nullptr, 16), -31);
    EXPECT_EQ(SDL_atoi("17"), 17);
    EXPECT_EQ(SDL_strcasecmp("SDL", "sdl"), 0);
    EXPECT_EQ(SDL_utf8strlen("h\xC3\xA9llo"), 5u);
    char* upper = SDL_strdup("mixed");
    SDL_strupr(upper);
    EXPECT_STREQ(upper, "MIXED");
    SDL_free(upper);
    const char* p = "\xE2\x82\xAC!";
    EXPECT_EQ(SDL_StepUTF8(&p, nullptr), 0x20ACu);
    EXPECT_EQ(*p, '!');

    char* latin1 = SDL_iconv_string("ISO-8859-1", "UTF-8", "caf\xC3\xA9", 6);
    ASSERT_NE(latin1, nullptr);
    EXPECT_STREQ(latin1, "caf\xE9");
    SDL_free(latin1);
    Uint32* ucs4 = SDL_iconv_utf8_ucs4("\xE2\x82\xAC");  // the macro's mapping
    ASSERT_NE(ucs4, nullptr);
    EXPECT_EQ(SDL_Swap32BE(ucs4[0]), 0x20ACu);  // SDL's "UCS-4" is big-endian
    SDL_free(ucs4);
}

TEST(Stdinc, MemorySortChecksumsAndHelpers) {
    int values[] = {5, -1, 9, 3, 0};
    SDL_qsort(values, SDL_arraysize(values), sizeof values[0], [](const void* a, const void* b) {
        return *static_cast<const int*>(a) - *static_cast<const int*>(b);
    });
    EXPECT_EQ(std::to_array(values), (std::array{-1, 0, 3, 5, 9}));
    const int key = 5;
    EXPECT_EQ(SDL_bsearch(&key, values, 5, sizeof(int),
                          [](const void* a, const void* b) { return *static_cast<const int*>(a) - *static_cast<const int*>(b); }),
              &values[3]);
    SDL_zeroa(values);
    EXPECT_EQ(values[4], 0);
    struct Pair {
        int a, b;
    } pair{1, 2}, other{3, 4};
    SDL_copyp(&pair, &other);
    EXPECT_EQ(pair.b, 4);
    SDL_zero(pair);
    EXPECT_EQ(pair.a, 0);

    EXPECT_EQ(SDL_crc32(0, "123456789", 9), 0xCBF43926u);
    EXPECT_EQ(SDL_crc16(0, "123456789", 9), 0xBB3Du);
    EXPECT_EQ(SDL_murmur3_32("", 0, 0), 0u);

    size_t out = 0;
    EXPECT_TRUE(SDL_size_mul_check_overflow(1u << 10, 1u << 10, &out));
    EXPECT_EQ(out, 1u << 20);
    EXPECT_FALSE(SDL_size_mul_check_overflow(SDL_SIZE_MAX, 2, &out));
    EXPECT_FALSE(SDL_size_add_check_overflow(SDL_SIZE_MAX, 1, &out));

    static_assert(SDL_min(3, 2.5) == 2.5 && SDL_max(-1, 4) == 4 && SDL_clamp(12, 0, 10) == 10);
    static_assert(SDL_max(-1, 4u) == 0xFFFFFFFFu);  // the usual arithmetic conversions, as the macro has them
    static_assert(SDL_FOURCC('a', 'b', 'c', 'd') == 0x64636261u);
    EXPECT_EQ(SDL_Swap16(0x1234), 0x3412);
    EXPECT_EQ(SDL_Swap32(0x11223344u), 0x44332211u);
    EXPECT_EQ(SDL_Swap64(0x0102030405060708ull), 0x0807060504030201ull);
    EXPECT_EQ(SDL_SwapFloat(SDL_SwapFloat(1.5f)), 1.5f);
    EXPECT_EQ(SDL_Swap32LE(0x11223344u), SDL_BYTEORDER == SDL_LIL_ENDIAN ? 0x11223344u : 0x44332211u);
    EXPECT_EQ(SDL_Swap16BE(0x1234), SDL_BYTEORDER == SDL_LIL_ENDIAN ? 0x3412 : 0x1234);
    EXPECT_EQ(SDL_MostSignificantBitIndex32(0x80u), 7);
    EXPECT_EQ(SDL_MostSignificantBitIndex32(0), -1);
    EXPECT_TRUE(SDL_HasExactlyOneBitSet32(64));
    EXPECT_FALSE(SDL_HasExactlyOneBitSet32(65));

    void* block = SDL_aligned_alloc(64, 128);
    ASSERT_NE(block, nullptr);
    EXPECT_EQ(reinterpret_cast<std::uintptr_t>(block) % 64, 0u);
    SDL_aligned_free(block);
    SDL_srand(1);
    const Sint32 r = SDL_rand(10);
    EXPECT_TRUE(r >= 0 && r < 10);
    EXPECT_NEAR(SDL_sqrt(2.0), 1.4142135623730951, 1e-12);
    EXPECT_NEAR(SDL_sinf(SDL_PI_F / 2), 1.0f, 1e-6f);
}
