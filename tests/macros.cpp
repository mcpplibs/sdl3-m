// The macro mappings, checked at compile time: values, the macros' own
// types, strings as arrays, and the function-like macros as constant
// expressions. Nothing here includes an SDL header.
import std;
import mcpplibs.mwrap.sdl3;

namespace macros = mcpplibs::mwrap::sdl3::macros;
using namespace mcpplibs::mwrap::sdl3::macros;

template <class T, class U>
constexpr bool same = std::is_same_v<std::remove_cvref_t<T>, U>;

// Versions.
static_assert(SDL_MAJOR_VERSION == 3 && SDL_MINOR_VERSION == 4 && SDL_MICRO_VERSION == 18);
static_assert(SDL_VERSION == 3004018 && SDL_VERSIONNUM_MINOR(SDL_VERSION) == 4 && SDL_VERSIONNUM_MICRO(SDL_VERSION) == 18);
static_assert(SDL_VERSION_ATLEAST(3, 4, 18) && !SDL_VERSION_ATLEAST(3, 5, 0));

// Flags keep the macros' types: SDL_WindowFlags is Uint64, SDL_InitFlags Uint32.
static_assert(same<decltype(SDL_WINDOW_RESIZABLE), SDL_WindowFlags> && SDL_WINDOW_RESIZABLE == 0x20);
static_assert(same<decltype(SDL_WINDOW_HIGH_PIXEL_DENSITY), Uint64> && SDL_WINDOW_HIGH_PIXEL_DENSITY == 0x2000);
static_assert(same<decltype(SDL_INIT_VIDEO), unsigned int> && SDL_INIT_VIDEO == 0x20u && SDL_INIT_EVENTS == 0x4000u);
static_assert(same<decltype(SDL_INIT_VIDEO | SDL_INIT_AUDIO), SDL_InitFlags>);
static_assert(SDL_GPU_SHADERFORMAT_INVALID == 0 && SDL_GPU_SHADERFORMAT_SPIRV == (1u << 1) && SDL_GPU_SHADERFORMAT_METALLIB == (1u << 5));
static_assert(SDL_GPU_TEXTUREUSAGE_COLOR_TARGET == 2u && SDL_GPU_BUFFERUSAGE_INDEX == 2u && SDL_GPU_COLORCOMPONENT_A == 8u);

// Keys, buttons, modifiers.
static_assert(same<decltype(SDLK_A), unsigned int> && SDLK_A == 'a' && SDLK_ESCAPE == 0x1Bu);
static_assert(SDLK_F1 == (SDL_SCANCODE_F1 | SDLK_SCANCODE_MASK) && SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_F1) == SDLK_F1);
static_assert(SDL_KMOD_CTRL == (SDL_KMOD_LCTRL | SDL_KMOD_RCTRL) && SDL_KMOD_NONE == 0);
static_assert(SDL_BUTTON_LEFT == 1 && SDL_BUTTON_MASK(SDL_BUTTON_RIGHT) == 4u && SDL_BUTTON_RMASK == 4u);
static_assert(same<decltype(SDL_BUTTON_MASK(1)), unsigned int>);
static_assert(SDL_HAT_RIGHTUP == (SDL_HAT_RIGHT | SDL_HAT_UP));

// Identifiers and limits.
static_assert(SDL_TOUCH_MOUSEID == static_cast<SDL_MouseID>(-1) && same<decltype(SDL_TOUCH_MOUSEID), SDL_MouseID>);
static_assert(SDL_PEN_MOUSEID == static_cast<SDL_MouseID>(-2));
static_assert(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK == 0xFFFFFFFFu && same<decltype(SDL_AUDIO_DEVICE_DEFAULT_RECORDING), SDL_AudioDeviceID>);
static_assert(SDL_MAX_SINT32 == 2147483647 && SDL_MIN_SINT8 == -128 && SDL_MAX_UINT64 == ~Uint64{0});
static_assert(same<decltype(SDL_MAX_UINT16), Uint16> && same<decltype(SDL_MIN_SINT64), Sint64>);
static_assert(SDL_ICONV_ERROR == static_cast<std::size_t>(-1) && SDL_SIZE_MAX == static_cast<std::size_t>(-1));
static_assert(SDL_NS_PER_SECOND == 1000000000 && SDL_MS_PER_SECOND == 1000);
static_assert(SDL_PI_F > 3.14159f && SDL_PI_F < 3.1416f && same<decltype(SDL_PI_D), double>);
static_assert(SDL_STANDARD_GRAVITY == 9.80665f && SDL_ALPHA_OPAQUE == 255 && SDL_ALPHA_OPAQUE_FLOAT == 1.0f);
static_assert(SDL_WINDOWPOS_CENTERED == SDL_WINDOWPOS_CENTERED_DISPLAY(0) && SDL_WINDOWPOS_UNDEFINED == 0x1FFF0000u);
static_assert(SDL_LIL_ENDIAN == 1234 && SDL_BIG_ENDIAN == 4321 && (SDL_BYTEORDER == SDL_LIL_ENDIAN || SDL_BYTEORDER == SDL_BIG_ENDIAN));

// Strings are arrays (sizeof counts the terminator).
static_assert(same<decltype(SDL_HINT_VIDEO_DRIVER), char[17]>);
static_assert(std::string_view(SDL_HINT_VIDEO_DRIVER) == "SDL_VIDEO_DRIVER" && sizeof SDL_HINT_RENDER_VSYNC == sizeof "SDL_RENDER_VSYNC");
static_assert(std::string_view(SDL_PROP_WINDOW_CREATE_TITLE_STRING) == "SDL.window.create.title");
static_assert(std::string_view(SDL_PROP_GPU_DEVICE_CREATE_SHADERS_SPIRV_BOOLEAN) == "SDL.gpu.device.create.shaders.spirv");
static_assert(std::string_view(SDL_SOFTWARE_RENDERER) == "software");
static_assert(std::string_view(SDL_PRIu32) == "u" && std::string_view(SDL_PRILL_PREFIX).size() >= 2);

// Computation macros are constant expressions.
static_assert(SDL_FOURCC('Y', 'V', '1', '2') == SDL_PIXELFORMAT_YV12 && SDL_DEFINE_PIXELFOURCC('N', 'V', '1', '2') == SDL_PIXELFORMAT_NV12);
static_assert(SDL_BITSPERPIXEL(SDL_PIXELFORMAT_ARGB8888) == 32 && SDL_BYTESPERPIXEL(SDL_PIXELFORMAT_RGB24) == 3);
static_assert(SDL_ISPIXELFORMAT_INDEXED(SDL_PIXELFORMAT_INDEX8) && SDL_ISPIXELFORMAT_PACKED(SDL_PIXELFORMAT_RGB565));
static_assert(SDL_ISPIXELFORMAT_ARRAY(SDL_PIXELFORMAT_RGB24) && SDL_ISPIXELFORMAT_10BIT(SDL_PIXELFORMAT_ARGB2101010));
static_assert(SDL_COLORSPACETYPE(SDL_COLORSPACE_SRGB) == SDL_COLOR_TYPE_RGB && SDL_COLORSPACEMATRIX(SDL_COLORSPACE_BT601_LIMITED) == SDL_MATRIX_COEFFICIENTS_BT601);
static_assert(SDL_COLORSPACE_SRGB == SDL_DEFINE_COLORSPACE(SDL_COLOR_TYPE_RGB, SDL_COLOR_RANGE_FULL, SDL_COLOR_PRIMARIES_BT709,
                                                           SDL_TRANSFER_CHARACTERISTICS_SRGB, SDL_MATRIX_COEFFICIENTS_IDENTITY,
                                                           SDL_CHROMA_LOCATION_NONE));
static_assert(SDL_AUDIO_BITSIZE(SDL_AUDIO_S32) == 32 && SDL_AUDIO_ISFLOAT(SDL_AUDIO_F32BE) && SDL_AUDIO_ISBIGENDIAN(SDL_AUDIO_F32BE));
static_assert(SDL_MS_TO_NS(3) == 3000000u && SDL_NS_TO_MS(SDL_SECONDS_TO_NS(1)) == 1000u && SDL_NS_TO_US(-2000) == -2);
static_assert(SDL_min(1, 2) == 1 && SDL_max(1.5, 1) == 1.5 && SDL_clamp(-3, 0, 5) == 0);
constexpr int table[7] = {};
static_assert(SDL_arraysize(table) == 7 && same<decltype(SDL_arraysize(table)), std::size_t>);

// The adapted inline API is the module's, by name.
static_assert(std::is_same_v<decltype(&mcpplibs::mwrap::sdl3::SDL_PointInRect), bool (*)(const SDL_Point*, const SDL_Rect*)>);
static_assert(std::is_same_v<decltype(&mcpplibs::mwrap::sdl3::SDL_Swap32), Uint32 (*)(Uint32)>);

int main() {
    // The qualified spelling is the same entity as the unqualified one.
    return &macros::SDL_HINT_VIDEO_DRIVER == &SDL_HINT_VIDEO_DRIVER && macros::SDL_INIT_AUDIO == SDL_INIT_AUDIO ? 0 : 1;
}
