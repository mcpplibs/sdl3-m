// Pixels through the module: pixel formats and their macros, surfaces
// (fill, blit, convert, BMP round trip), the inline rectangle API, a window
// and the software renderer on SDL's offscreen video driver, keys and
// scancodes, the clipboard.
#include <gtest/gtest.h>

import std;
import mcpplibs.mwrap.sdl3;

using namespace mcpplibs::mwrap::sdl3;          // the adapted inline API: SDL_PointInRect ...
using namespace mcpplibs::mwrap::sdl3::macros;  // SDL_BITSPERPIXEL, SDL_WINDOW_HIDDEN ...

namespace {

class Video : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "offscreen");
        ASSERT_TRUE(SDL_Init(SDL_INIT_VIDEO)) << SDL_GetError();
    }
    static void TearDownTestSuite() { SDL_Quit(); }
};

struct SurfaceDeleter {
    void operator()(SDL_Surface* s) const { SDL_DestroySurface(s); }
};
using Surface = std::unique_ptr<SDL_Surface, SurfaceDeleter>;

}  // namespace

TEST_F(Video, PixelFormats) {
    static_assert(SDL_BITSPERPIXEL(SDL_PIXELFORMAT_RGB565) == 16);
    static_assert(SDL_BYTESPERPIXEL(SDL_PIXELFORMAT_ARGB8888) == 4);
    static_assert(SDL_ISPIXELFORMAT_ALPHA(SDL_PIXELFORMAT_RGBA8888) && !SDL_ISPIXELFORMAT_ALPHA(SDL_PIXELFORMAT_XRGB8888));
    static_assert(SDL_ISPIXELFORMAT_FOURCC(SDL_PIXELFORMAT_YV12) && SDL_PIXELFORMAT_YV12 == SDL_FOURCC('Y', 'V', '1', '2'));
    static_assert(SDL_PIXELTYPE(SDL_PIXELFORMAT_RGBA64_FLOAT) == SDL_PIXELTYPE_ARRAYF16 && SDL_ISPIXELFORMAT_FLOAT(SDL_PIXELFORMAT_RGBA64_FLOAT));
    static_assert(SDL_PIXELFORMAT_ARGB8888 ==
                  SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_PACKED32, SDL_PACKEDORDER_ARGB, SDL_PACKEDLAYOUT_8888, 32, 4));
    static_assert(SDL_ISCOLORSPACE_FULL_RANGE(SDL_COLORSPACE_SRGB) && SDL_ISCOLORSPACE_LIMITED_RANGE(SDL_COLORSPACE_BT709_LIMITED));
    static_assert(SDL_COLORSPACEPRIMARIES(SDL_COLORSPACE_BT2020_FULL) == SDL_COLOR_PRIMARIES_BT2020);

    EXPECT_STREQ(SDL_GetPixelFormatName(SDL_PIXELFORMAT_RGBA32),
                 SDL_BYTEORDER == SDL_LIL_ENDIAN ? "SDL_PIXELFORMAT_ABGR8888" : "SDL_PIXELFORMAT_RGBA8888");
    int bpp = 0;
    Uint32 r = 0, g = 0, b = 0, a = 0;
    ASSERT_TRUE(SDL_GetMasksForPixelFormat(SDL_PIXELFORMAT_ARGB8888, &bpp, &r, &g, &b, &a));
    EXPECT_EQ(bpp, 32);
    EXPECT_EQ(a, 0xFF000000u);
    EXPECT_EQ(SDL_GetPixelFormatForMasks(bpp, r, g, b, a), SDL_PIXELFORMAT_ARGB8888);
    const SDL_PixelFormatDetails* details = SDL_GetPixelFormatDetails(SDL_PIXELFORMAT_ARGB8888);
    ASSERT_NE(details, nullptr);
    EXPECT_EQ(SDL_MapRGBA(details, nullptr, 0x11, 0x22, 0x33, 0x44), 0x44112233u);
    Uint8 cr, cg, cb, ca;
    SDL_GetRGBA(0x44112233u, details, nullptr, &cr, &cg, &cb, &ca);
    EXPECT_EQ((std::array{cr, cg, cb, ca}), (std::array<Uint8, 4>{0x11, 0x22, 0x33, 0x44}));
}

TEST_F(Video, SurfacesFillBlitConvertBmp) {
    Surface s{SDL_CreateSurface(8, 4, SDL_PIXELFORMAT_RGBA32)};
    ASSERT_TRUE(s) << SDL_GetError();
    EXPECT_FALSE(SDL_MUSTLOCK(s.get()));
    const SDL_PixelFormatDetails* fmt = SDL_GetPixelFormatDetails(s->format);
    ASSERT_TRUE(SDL_FillSurfaceRect(s.get(), nullptr, SDL_MapRGBA(fmt, nullptr, 0, 0, 255, 255)));
    const SDL_Rect left{0, 0, 4, 4};
    ASSERT_TRUE(SDL_FillSurfaceRect(s.get(), &left, SDL_MapRGBA(fmt, nullptr, 255, 0, 0, 255)));
    Uint8 r, g, b, a;
    ASSERT_TRUE(SDL_ReadSurfacePixel(s.get(), 1, 1, &r, &g, &b, &a));
    EXPECT_EQ((std::array{r, g, b, a}), (std::array<Uint8, 4>{255, 0, 0, 255}));
    ASSERT_TRUE(SDL_ReadSurfacePixel(s.get(), 6, 2, &r, &g, &b, &a));
    EXPECT_EQ(b, 255);

    Surface converted{SDL_ConvertSurface(s.get(), SDL_PIXELFORMAT_XRGB8888)};
    ASSERT_TRUE(converted);
    EXPECT_EQ(converted->format, SDL_PIXELFORMAT_XRGB8888);
    const auto* px = static_cast<const Uint32*>(converted->pixels);
    EXPECT_EQ(px[0] & 0x00FFFFFFu, 0xFF0000u);

    Surface dst{SDL_CreateSurface(8, 4, SDL_PIXELFORMAT_ARGB8888)};
    ASSERT_TRUE(SDL_FillSurfaceRect(dst.get(), nullptr, 0));
    SDL_Rect to{4, 0, 4, 4};
    ASSERT_TRUE(SDL_BlitSurface(s.get(), &left, dst.get(), &to));
    ASSERT_TRUE(SDL_ReadSurfacePixel(dst.get(), 5, 3, &r, &g, &b, &a));
    EXPECT_EQ(r, 255);

    // BMP, through a dynamic-memory stream and back.
    SDL_IOStream* io = SDL_IOFromDynamicMem();
    ASSERT_TRUE(SDL_SaveBMP_IO(s.get(), io, false));
    EXPECT_GT(SDL_TellIO(io), 54);
    ASSERT_EQ(SDL_SeekIO(io, 0, SDL_IO_SEEK_SET), 0);
    Surface loaded{SDL_LoadBMP_IO(io, true)};
    ASSERT_TRUE(loaded) << SDL_GetError();
    EXPECT_EQ(loaded->w, 8);
    ASSERT_TRUE(SDL_ReadSurfacePixel(loaded.get(), 0, 0, &r, &g, &b, &a));
    EXPECT_EQ((std::array{r, g, b}), (std::array<Uint8, 3>{255, 0, 0}));

    Surface scaled{SDL_ScaleSurface(s.get(), 16, 8, SDL_SCALEMODE_NEAREST)};
    ASSERT_TRUE(scaled);
    EXPECT_EQ(scaled->w, 16);
}

TEST_F(Video, InlineRectangles) {
    const SDL_Rect a{0, 0, 10, 10}, b{5, 5, 10, 10}, empty{0, 0, 0, 3};
    const SDL_Point p{3, 3}, q{12, 1};
    EXPECT_TRUE(SDL_PointInRect(&p, &a));
    EXPECT_FALSE(SDL_PointInRect(&q, &a));
    EXPECT_TRUE(SDL_RectEmpty(&empty));
    EXPECT_TRUE(SDL_RectsEqual(&a, &a));
    EXPECT_FALSE(SDL_RectsEqual(&a, &b));
    EXPECT_TRUE(SDL_HasRectIntersection(&a, &b));
    SDL_Rect i{}, u{};
    ASSERT_TRUE(SDL_GetRectIntersection(&a, &b, &i));
    EXPECT_TRUE((i.x == 5 && i.y == 5 && i.w == 5 && i.h == 5));
    ASSERT_TRUE(SDL_GetRectUnion(&a, &b, &u));
    EXPECT_EQ(u.w, 15);
    SDL_FRect f{};
    SDL_RectToFRect(&b, &f);
    EXPECT_EQ(f.x, 5.0f);
    const SDL_FRect g{5.0f, 5.0f, 10.0f, 10.000001f};
    EXPECT_TRUE(SDL_RectsEqualEpsilon(&f, &g, 0.001f));
    EXPECT_FALSE(SDL_RectsEqualFloat(&f, &g));
    const SDL_FPoint fp{6.0f, 6.0f};
    EXPECT_TRUE(SDL_PointInRectFloat(&fp, &f));
    EXPECT_FALSE(SDL_RectEmptyFloat(&f));
    const SDL_Point points[] = {{1, 1}, {4, 9}, {-2, 3}};
    SDL_Rect enclosing{};
    ASSERT_TRUE(SDL_GetRectEnclosingPoints(points, 3, nullptr, &enclosing));
    EXPECT_TRUE((enclosing.x == -2 && enclosing.w == 7 && enclosing.h == 9));
}

TEST_F(Video, WindowAndSoftwareRenderer) {
    SDL_Window* window = SDL_CreateWindow("sdl3-m", 64, 48, SDL_WINDOW_RESIZABLE);
    ASSERT_NE(window, nullptr) << SDL_GetError();
    EXPECT_EQ(SDL_GetWindowFromID(SDL_GetWindowID(window)), window);
    EXPECT_NE(SDL_GetWindowFlags(window) & SDL_WINDOW_RESIZABLE, 0u);
    int w = 0, h = 0;
    ASSERT_TRUE(SDL_GetWindowSizeInPixels(window, &w, &h));
    EXPECT_EQ(w, 64);
    EXPECT_TRUE(SDL_SetWindowTitle(window, "renamed"));
    EXPECT_STREQ(SDL_GetWindowTitle(window), "renamed");
    EXPECT_GT(SDL_GetWindowDisplayScale(window), 0.0f);
    int displays = 0;
    SDL_DisplayID* ids = SDL_GetDisplays(&displays);
    EXPECT_GE(displays, 1);
    SDL_free(ids);
    const SDL_PropertiesID props = SDL_GetWindowProperties(window);
    EXPECT_NE(props, 0u);

    SDL_Renderer* renderer = SDL_CreateRenderer(window, SDL_SOFTWARE_RENDERER);
    ASSERT_NE(renderer, nullptr) << SDL_GetError();
    EXPECT_STREQ(SDL_GetRendererName(renderer), SDL_SOFTWARE_RENDERER);
    ASSERT_TRUE(SDL_SetRenderDrawColor(renderer, 0, 0, 0, SDL_ALPHA_OPAQUE));
    ASSERT_TRUE(SDL_RenderClear(renderer));
    ASSERT_TRUE(SDL_SetRenderDrawColor(renderer, 0, 255, 0, SDL_ALPHA_OPAQUE));
    const SDL_FRect box{8, 8, 16, 16};
    ASSERT_TRUE(SDL_RenderFillRect(renderer, &box));

    // A streaming texture, updated from memory and drawn at the right.
    SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, 4, 4);
    ASSERT_NE(texture, nullptr) << SDL_GetError();
    std::array<Uint32, 16> pixels;
    const SDL_PixelFormatDetails* fmt = SDL_GetPixelFormatDetails(SDL_PIXELFORMAT_RGBA32);
    pixels.fill(SDL_MapRGBA(fmt, nullptr, 0, 0, 255, 255));
    ASSERT_TRUE(SDL_UpdateTexture(texture, nullptr, pixels.data(), 4 * sizeof(Uint32)));
    ASSERT_TRUE(SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_NONE));
    ASSERT_TRUE(SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST));
    const SDL_FRect at{40, 8, 16, 16};
    ASSERT_TRUE(SDL_RenderTexture(renderer, texture, nullptr, &at));

    SDL_Surface* frame = SDL_RenderReadPixels(renderer, nullptr);
    ASSERT_NE(frame, nullptr) << SDL_GetError();
    Uint8 r, g, b, a;
    ASSERT_TRUE(SDL_ReadSurfacePixel(frame, 16, 16, &r, &g, &b, &a));
    EXPECT_EQ((std::array{r, g, b}), (std::array<Uint8, 3>{0, 255, 0}));
    ASSERT_TRUE(SDL_ReadSurfacePixel(frame, 48, 16, &r, &g, &b, &a));
    EXPECT_EQ((std::array{r, g, b}), (std::array<Uint8, 3>{0, 0, 255}));
    ASSERT_TRUE(SDL_ReadSurfacePixel(frame, 2, 40, &r, &g, &b, &a));
    EXPECT_EQ((std::array{r, g, b}), (std::array<Uint8, 3>{0, 0, 0}));
    SDL_DestroySurface(frame);
    EXPECT_TRUE(SDL_RenderPresent(renderer));
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
}

TEST_F(Video, WindowsFromProperties) {
    const SDL_PropertiesID props = SDL_CreateProperties();
    SDL_SetStringProperty(props, SDL_PROP_WINDOW_CREATE_TITLE_STRING, "props");
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, 32);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, 24);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_FLAGS_NUMBER, static_cast<Sint64>(SDL_WINDOW_HIDDEN));
    SDL_Window* window = SDL_CreateWindowWithProperties(props);
    SDL_DestroyProperties(props);
    ASSERT_NE(window, nullptr) << SDL_GetError();
    EXPECT_STREQ(SDL_GetWindowTitle(window), "props");
    EXPECT_NE(SDL_GetWindowFlags(window) & SDL_WINDOW_HIDDEN, 0u);
    EXPECT_TRUE(SDL_WINDOWPOS_ISUNDEFINED(static_cast<int>(SDL_WINDOWPOS_UNDEFINED)));
    EXPECT_TRUE(SDL_WINDOWPOS_ISCENTERED(static_cast<int>(SDL_WINDOWPOS_CENTERED_DISPLAY(2))));
    SDL_DestroyWindow(window);
}

TEST_F(Video, KeysAndScancodes) {
    EXPECT_STREQ(SDL_GetScancodeName(SDL_SCANCODE_A), "A");
    EXPECT_EQ(SDL_GetScancodeFromName("Escape"), SDL_SCANCODE_ESCAPE);
    EXPECT_STREQ(SDL_GetKeyName(SDLK_RETURN), "Return");
    EXPECT_EQ(SDL_GetKeyFromName("Space"), SDLK_SPACE);
    EXPECT_EQ(SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_F1), SDLK_F1 | 0u);
    EXPECT_EQ(SDLK_A, static_cast<SDL_Keycode>('a'));
    EXPECT_EQ(SDL_KMOD_CTRL, SDL_KMOD_LCTRL | SDL_KMOD_RCTRL);
    SDL_Keymod mod = SDL_KMOD_NONE;
    mod |= SDL_KMOD_LSHIFT;
    EXPECT_TRUE(mod & SDL_KMOD_SHIFT);
    EXPECT_EQ(SDL_BUTTON_MASK(SDL_BUTTON_X2), SDL_BUTTON_X2MASK);
    EXPECT_EQ(SDL_BUTTON_LMASK, 1u);
}

TEST_F(Video, Clipboard) {
    if (!SDL_SetClipboardText("sdl3-m clipboard")) GTEST_SKIP() << "no clipboard here: " << SDL_GetError();
    EXPECT_TRUE(SDL_HasClipboardText());
    char* text = SDL_GetClipboardText();
    EXPECT_STREQ(text, "sdl3-m clipboard");
    SDL_free(text);
}
