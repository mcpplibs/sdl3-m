"""What the module does with each part of SDL's headers that is not a plain
declaration: shared by tools/gen_exports.py and tools/check_exports.py.

Every declaration of the wrapped headers is exported by `export using`
(src/sdl3.cppm) unless it is listed in EXCLUDED_DECLS, and every
`SDL_FORCE_INLINE` function is adapted (ADAPTED) unless listed there too.
Every macro is either

  - mapped in mcpplibs::mwrap::sdl3::macros (src/macros.cppm): object-like
    macros with a value as `inline constexpr` constants of the macro's own
    type (strings as `const char[]`), function-like macros listed in
    FUNCTION_MACROS as functions;
  - kept a macro (SIDE_HEADER, SIDE_HEADER_FILES): <mwrap/sdl3/macros.hpp>,
    which is <SDL3/SDL.h>, has them;
  - or excluded with a reason (EXCLUDED_MACROS).
"""

# ── declarations ────────────────────────────────────────────────────────────

EXCLUDED_DECLS = {
    # SDL_stdinc.h, inside `#ifndef DOXYGEN_SHOULD_IGNORE_THIS`: compile-time
    # checks of the platform's types, not API.
    'SDL_alignment_test': "SDL_stdinc.h's compile-time check of struct alignment (inside DOXYGEN_SHOULD_IGNORE_THIS)",
    'SDL_DUMMY_ENUM': "SDL_stdinc.h's compile-time check that an enum is an int (inside DOXYGEN_SHOULD_IGNORE_THIS)",
    'DUMMY_ENUM_VALUE': 'the enumerator of SDL_DUMMY_ENUM',
    # For pre-2010 MSVC without <stdint.h> only; everywhere else these are the C library's.
    **{t: "SDL_stdinc.h's fallback typedef for MSVC before 2010 (no <stdint.h>); the C library's otherwise"
       for t in ('int8_t', 'uint8_t', 'int16_t', 'uint16_t', 'int32_t', 'uint32_t', 'int64_t', 'uint64_t',
                 'intptr_t', 'uintptr_t')},
    # The incomplete structs behind handles: the handles (typedefs) are exported.
    'VkInstance_T': 'the incomplete struct behind VkInstance (exported)',
    'VkPhysicalDevice_T': 'the incomplete struct behind VkPhysicalDevice (exported)',
    'VkSurfaceKHR_T': 'the incomplete struct behind VkSurfaceKHR (exported)',
    'tagMSG': 'the struct tag of MSG (exported on Windows)',
    '_XEvent': 'the union tag of XEvent (exported)',
    'XTaskQueueObject': 'the struct tag of XTaskQueueHandle (exported on GDK)',
    'XUser': 'the struct tag of XUserHandle (exported on GDK)',
    'SDL_GLContextState': 'the struct tag of SDL_GLContext (exported)',
    'SDL_iconv_data_t': 'the struct tag of SDL_iconv_t (exported)',
    # Implementation choices of the inline API.
    'SDL_size_mul_check_overflow_builtin': 'what SDL_size_mul_check_overflow (adapted) uses when the compiler has '
                                           '__builtin_mul_overflow',
    'SDL_size_add_check_overflow_builtin': 'what SDL_size_add_check_overflow (adapted) uses when the compiler has '
                                           '__builtin_add_overflow',
    '_SDL_bsr_watcom': "Open Watcom's helper for SDL_MostSignificantBitIndex32 (adapted)",
    'SDL_CompilerBarrier': "Open Watcom's inline variant; elsewhere a macro, mapped in the macros module",
    'SDL_CPUPauseInstruction': "Open Watcom's inline variant; elsewhere a macro, mapped in the macros module",
    '_m_prefetch': "a prototype SDL_endian.h supplies for clang-cl's <intrin.h>; the compiler's, not SDL's",
    # Declared only for the application to define (with SDL_MAIN_USE_CALLBACKS).
    'SDL_AppInit': 'declared by <SDL3/SDL_main.h> under SDL_MAIN_USE_CALLBACKS, for the application to define',
    'SDL_AppIterate': 'declared by <SDL3/SDL_main.h> under SDL_MAIN_USE_CALLBACKS, for the application to define',
    'SDL_AppEvent': 'declared by <SDL3/SDL_main.h> under SDL_MAIN_USE_CALLBACKS, for the application to define',
    'SDL_AppQuit': 'declared by <SDL3/SDL_main.h> under SDL_MAIN_USE_CALLBACKS, for the application to define',
}

# Declarations of the C library and compiler that SDL's headers repeat under
# some configuration (clang's static analyzer, MSVC intrinsics, alloca), and a
# symbol only Clang's thread-safety analysis sees: not SDL's API.
NOT_SDL_STATEMENTS = {
    'alloca', 'strlcpy', 'strlcat', 'wcslcpy', 'wcslcat', 'strtok_r', 'strdup', '__debugbreak',
    '_ReadWriteBarrier', 'SDL_joystick_lock',
}

# The `SDL_FORCE_INLINE` functions (internal linkage: not exportable) and the
# ones that are a macro or a static inline function depending on the
# compiler. Each is adapted in namespace mcpplibs::mwrap::sdl3 under its own
# name and signature, defined in src/inline.cpp by calling upstream's.
ADAPTED = {
    'SDL_size_mul_check_overflow': ('bool', 'size_t a, size_t b, size_t *ret'),
    'SDL_size_add_check_overflow': ('bool', 'size_t a, size_t b, size_t *ret'),
    'SDL_Swap16': ('Uint16', 'Uint16 x'),
    'SDL_Swap32': ('Uint32', 'Uint32 x'),
    'SDL_Swap64': ('Uint64', 'Uint64 x'),
    'SDL_SwapFloat': ('float', 'float x'),
    'SDL_MostSignificantBitIndex32': ('int', 'Uint32 x'),
    'SDL_HasExactlyOneBitSet32': ('bool', 'Uint32 x'),
    'SDL_RectToFRect': ('void', 'const SDL_Rect *rect, SDL_FRect *frect'),
    'SDL_PointInRect': ('bool', 'const SDL_Point *p, const SDL_Rect *r'),
    'SDL_RectEmpty': ('bool', 'const SDL_Rect *r'),
    'SDL_RectsEqual': ('bool', 'const SDL_Rect *a, const SDL_Rect *b'),
    'SDL_PointInRectFloat': ('bool', 'const SDL_FPoint *p, const SDL_FRect *r'),
    'SDL_RectEmptyFloat': ('bool', 'const SDL_FRect *r'),
    'SDL_RectsEqualEpsilon': ('bool', 'const SDL_FRect *a, const SDL_FRect *b, float epsilon'),
    'SDL_RectsEqualFloat': ('bool', 'const SDL_FRect *a, const SDL_FRect *b'),
}

# ── macros ──────────────────────────────────────────────────────────────────

# Function-like macros mapped to functions: name -> (template head, return
# type, parameters, kind). The body is the macro itself, expanded in the
# module's global module fragment, so each is exactly upstream's expression
# with every argument evaluated once:
#   'constexpr' — usable in constant expressions;
#   'inline'    — an ordinary inline function;
#   'statement' — the macro is a statement (do { } while, a block); void;
#   'outofline' — defined in src/macros.cpp, as it may call a static inline
#                 function of the header (SDL_Swap16 on compilers without
#                 __builtin_bswap16, SDL_SwapFloat).
# Template parameters where the macro is generic over its arguments' types:
# SDL_min, SDL_zero, the time conversions, and the macros that take a value
# apart (SDL_AUDIO_BITSIZE, SDL_PIXELTYPE, SDL_COLORSPACERANGE ...), which
# upstream itself applies to plain integers (SDL_AUDIO_BITSIZE(8)) as well
# as to the enums: the result has the macro's type for either. Otherwise the
# parameter types SDL documents.
FUNCTION_MACROS = {
    # SDL_stdinc.h
    'SDL_arraysize': ('template <class T, size_t N>', 'size_t', 'const T (&array)[N]', 'constexpr'),
    'SDL_FOURCC': ('', 'Uint32', 'Uint8 A, Uint8 B, Uint8 C, Uint8 D', 'constexpr'),
    'SDL_INIT_INTERFACE': ('template <class T>', 'void', 'T *iface', 'statement'),
    'SDL_min': ('template <class T, class U>', 'auto', 'T x, U y', 'constexpr'),
    'SDL_max': ('template <class T, class U>', 'auto', 'T x, U y', 'constexpr'),
    'SDL_clamp': ('template <class T, class A, class B>', 'auto', 'T x, A a, B b', 'constexpr'),
    'SDL_copyp': ('template <class D, class S>', 'void', 'D *dst, S *src', 'statement'),
    'SDL_zero': ('template <class T>', 'void *', 'T &x', 'inline'),
    'SDL_zerop': ('template <class T>', 'void *', 'T *x', 'inline'),
    'SDL_zeroa': ('template <class T, size_t N>', 'void *', 'T (&x)[N]', 'inline'),
    'SDL_iconv_utf8_locale': ('', 'char *', 'const char *S', 'inline'),
    'SDL_iconv_utf8_ucs2': ('', 'Uint16 *', 'const char *S', 'inline'),
    'SDL_iconv_utf8_ucs4': ('', 'Uint32 *', 'const char *S', 'inline'),
    'SDL_iconv_wchar_utf8': ('', 'char *', 'const wchar_t *S', 'inline'),
    # SDL_atomic.h
    'SDL_CompilerBarrier': ('', 'void', '', 'statement'),
    'SDL_MemoryBarrierRelease': ('', 'void', '', 'statement'),
    'SDL_MemoryBarrierAcquire': ('', 'void', '', 'statement'),
    'SDL_CPUPauseInstruction': ('', 'void', '', 'statement'),
    'SDL_AtomicIncRef': ('', 'int', 'SDL_AtomicInt *a', 'inline'),
    'SDL_AtomicDecRef': ('', 'bool', 'SDL_AtomicInt *a', 'inline'),
    # SDL_endian.h
    'SDL_Swap16LE': ('', 'Uint16', 'Uint16 x', 'outofline'),
    'SDL_Swap32LE': ('', 'Uint32', 'Uint32 x', 'outofline'),
    'SDL_Swap64LE': ('', 'Uint64', 'Uint64 x', 'outofline'),
    'SDL_SwapFloatLE': ('', 'float', 'float x', 'outofline'),
    'SDL_Swap16BE': ('', 'Uint16', 'Uint16 x', 'outofline'),
    'SDL_Swap32BE': ('', 'Uint32', 'Uint32 x', 'outofline'),
    'SDL_Swap64BE': ('', 'Uint64', 'Uint64 x', 'outofline'),
    'SDL_SwapFloatBE': ('', 'float', 'float x', 'outofline'),
    # SDL_error.h
    'SDL_Unsupported': ('', 'bool', '', 'inline'),
    'SDL_InvalidParamError': ('', 'bool', 'const char *param', 'inline'),
    # SDL_thread.h (always macros: they pass the C runtime's thread entry points)
    'SDL_CreateThread': ('', 'SDL_Thread *', 'SDL_ThreadFunction fn, const char *name, void *data', 'inline'),
    'SDL_CreateThreadWithProperties': ('', 'SDL_Thread *', 'SDL_PropertiesID props', 'inline'),
    # SDL_audio.h
    'SDL_DEFINE_AUDIO_FORMAT': ('', 'auto', 'int sign, int bigendian, int flt, int size', 'constexpr'),
    'SDL_AUDIO_BITSIZE': ('template <class T>', 'auto', 'T x', 'constexpr'),
    'SDL_AUDIO_BYTESIZE': ('template <class T>', 'auto', 'T x', 'constexpr'),
    'SDL_AUDIO_ISFLOAT': ('template <class T>', 'auto', 'T x', 'constexpr'),
    'SDL_AUDIO_ISBIGENDIAN': ('template <class T>', 'auto', 'T x', 'constexpr'),
    'SDL_AUDIO_ISLITTLEENDIAN': ('template <class T>', 'auto', 'T x', 'constexpr'),
    'SDL_AUDIO_ISSIGNED': ('template <class T>', 'auto', 'T x', 'constexpr'),
    'SDL_AUDIO_ISINT': ('template <class T>', 'auto', 'T x', 'constexpr'),
    'SDL_AUDIO_ISUNSIGNED': ('template <class T>', 'auto', 'T x', 'constexpr'),
    'SDL_AUDIO_FRAMESIZE': ('template <class T>', 'auto', 'const T &x', 'constexpr'),
    # SDL_pixels.h
    'SDL_DEFINE_PIXELFOURCC': ('', 'Uint32', 'Uint8 A, Uint8 B, Uint8 C, Uint8 D', 'constexpr'),
    'SDL_DEFINE_PIXELFORMAT': ('', 'auto', 'int type, int order, int layout, int bits, int bytes', 'constexpr'),
    **{n: ('template <class T>', 'auto', 'T format', 'constexpr') for n in (
        'SDL_PIXELFLAG', 'SDL_PIXELTYPE', 'SDL_PIXELORDER', 'SDL_PIXELLAYOUT', 'SDL_BITSPERPIXEL',
        'SDL_BYTESPERPIXEL', 'SDL_ISPIXELFORMAT_INDEXED', 'SDL_ISPIXELFORMAT_PACKED', 'SDL_ISPIXELFORMAT_ARRAY',
        'SDL_ISPIXELFORMAT_10BIT', 'SDL_ISPIXELFORMAT_FLOAT', 'SDL_ISPIXELFORMAT_ALPHA', 'SDL_ISPIXELFORMAT_FOURCC')},
    'SDL_DEFINE_COLORSPACE': ('', 'auto', 'int type, int range, int primaries, int transfer, int matrix, int chroma',
                              'constexpr'),
    **{n: ('template <class T>', 'auto', 'T cspace', 'constexpr') for n in (
        'SDL_COLORSPACETYPE', 'SDL_COLORSPACERANGE', 'SDL_COLORSPACECHROMA', 'SDL_COLORSPACEPRIMARIES',
        'SDL_COLORSPACETRANSFER', 'SDL_COLORSPACEMATRIX', 'SDL_ISCOLORSPACE_MATRIX_BT601',
        'SDL_ISCOLORSPACE_MATRIX_BT709', 'SDL_ISCOLORSPACE_MATRIX_BT2020_NCL', 'SDL_ISCOLORSPACE_LIMITED_RANGE',
        'SDL_ISCOLORSPACE_FULL_RANGE')},
    # SDL_surface.h
    'SDL_MUSTLOCK': ('', 'bool', 'const SDL_Surface *S', 'constexpr'),
    # SDL_video.h
    'SDL_WINDOWPOS_UNDEFINED_DISPLAY': ('template <class T>', 'auto', 'T X', 'constexpr'),
    'SDL_WINDOWPOS_ISUNDEFINED': ('template <class T>', 'auto', 'T X', 'constexpr'),
    'SDL_WINDOWPOS_CENTERED_DISPLAY': ('template <class T>', 'auto', 'T X', 'constexpr'),
    'SDL_WINDOWPOS_ISCENTERED': ('template <class T>', 'auto', 'T X', 'constexpr'),
    # SDL_keycode.h, SDL_mouse.h
    'SDL_SCANCODE_TO_KEYCODE': ('template <class T>', 'auto', 'T X', 'constexpr'),
    'SDL_BUTTON_MASK': ('template <class T>', 'auto', 'T X', 'constexpr'),
    # SDL_timer.h
    'SDL_SECONDS_TO_NS': ('template <class T>', 'auto', 'T S', 'constexpr'),
    'SDL_NS_TO_SECONDS': ('template <class T>', 'auto', 'T NS', 'constexpr'),
    'SDL_MS_TO_NS': ('template <class T>', 'auto', 'T MS', 'constexpr'),
    'SDL_NS_TO_MS': ('template <class T>', 'auto', 'T NS', 'constexpr'),
    'SDL_US_TO_NS': ('template <class T>', 'auto', 'T US', 'constexpr'),
    'SDL_NS_TO_US': ('template <class T>', 'auto', 'T NS', 'constexpr'),
    # SDL_version.h
    'SDL_VERSIONNUM': ('', 'int', 'int major, int minor, int patch', 'constexpr'),
    'SDL_VERSIONNUM_MAJOR': ('', 'int', 'int version', 'constexpr'),
    'SDL_VERSIONNUM_MINOR': ('', 'int', 'int version', 'constexpr'),
    'SDL_VERSIONNUM_MICRO': ('', 'int', 'int version', 'constexpr'),
    'SDL_VERSION_ATLEAST': ('', 'bool', 'int X, int Y, int Z', 'constexpr'),
}

_ANNOTATION = 'an annotation of declarations (attribute, calling convention or analysis hint)'
_THREAD = "Clang's thread-safety analysis attribute"
_COND = 'a test for #if / #ifdef'

# Macros that stay macros, with the reason a constant or a function cannot be them.
SIDE_HEADER = {
    # SDL_stdinc.h
    'SDL_COMPILE_TIME_ASSERT': 'a declaration (static_assert) with a stringified condition',
    'SDL_STRINGIFY_ARG': 'stringification',
    'SDL_reinterpret_cast': 'takes a type', 'SDL_static_cast': 'takes a type', 'SDL_const_cast': 'takes a type',
    'SDL_SINT64_C': 'pastes a literal suffix (also used in #if)',
    'SDL_UINT64_C': 'pastes a literal suffix (also used in #if)',
    'SDL_stack_alloc': 'takes a type, and allocates in the caller\'s stack frame (alloca)',
    'SDL_stack_free': "pairs with SDL_stack_alloc (a no-op or SDL_free, as SDL_stack_alloc chose)",
    'SDL_NOLONGLONG': _COND,
    'SDL_DISABLE_ALLOCA': _COND,
    **{n: _ANNOTATION for n in (
        'SDL_IN_BYTECAP', 'SDL_INOUT_Z_CAP', 'SDL_OUT_Z_CAP', 'SDL_OUT_CAP', 'SDL_OUT_BYTECAP', 'SDL_OUT_Z_BYTECAP',
        'SDL_PRINTF_FORMAT_STRING', 'SDL_SCANF_FORMAT_STRING', 'SDL_PRINTF_VARARG_FUNC', 'SDL_PRINTF_VARARG_FUNCV',
        'SDL_SCANF_VARARG_FUNC', 'SDL_SCANF_VARARG_FUNCV', 'SDL_WPRINTF_VARARG_FUNC', 'SDL_WPRINTF_VARARG_FUNCV')},
    # SDL_assert.h
    'SDL_ASSERT_LEVEL': "depends on the including file's NDEBUG / SDL_DEFAULT_ASSERT_LEVEL; used in #if",
    'SDL_TriggerBreakpoint': 'breaks into the debugger at the call site',
    'SDL_FUNCTION': "the call site's function name (__func__)",
    'SDL_FILE': "the call site's file name (__FILE__)",
    'SDL_ASSERT_FILE': "the call site's file name, as assertions report it",
    'SDL_LINE': "the call site's line (__LINE__)",
    'SDL_NULL_WHILE_LOOP_CONDITION': 'the condition of the assertion macros\' do-while',
    'SDL_disabled_assert': 'does not evaluate its condition',
    'SDL_enabled_assert': 'evaluates its condition, reports the call site and may break into the debugger there',
    'SDL_AssertBreakpoint': 'breaks into the debugger at the call site',
    'SDL_assert': 'an assertion (enabled by SDL_ASSERT_LEVEL; reports the call site)',
    'SDL_assert_release': 'an assertion (enabled by SDL_ASSERT_LEVEL; reports the call site)',
    'SDL_assert_paranoid': 'an assertion (enabled by SDL_ASSERT_LEVEL; reports the call site)',
    'SDL_assert_always': 'an assertion (reports the call site)',
    # SDL_atomic.h
    'SDL_MEMORY_BARRIER_USES_FUNCTION': _COND,
    # SDL_thread.h
    'SDL_BeginThreadFunction': "the C runtime's thread entry SDL_CreateThread passes; overridable before the header",
    'SDL_EndThreadFunction': "the C runtime's thread exit SDL_CreateThread passes; overridable before the header",
    # SDL_mutex.h
    **{n: _THREAD for n in (
        'SDL_THREAD_ANNOTATION_ATTRIBUTE__', 'SDL_CAPABILITY', 'SDL_SCOPED_CAPABILITY', 'SDL_GUARDED_BY',
        'SDL_PT_GUARDED_BY', 'SDL_ACQUIRED_BEFORE', 'SDL_ACQUIRED_AFTER', 'SDL_REQUIRES', 'SDL_REQUIRES_SHARED',
        'SDL_ACQUIRE', 'SDL_ACQUIRE_SHARED', 'SDL_RELEASE', 'SDL_RELEASE_SHARED', 'SDL_RELEASE_GENERIC',
        'SDL_TRY_ACQUIRE', 'SDL_TRY_ACQUIRE_SHARED', 'SDL_EXCLUDES', 'SDL_ASSERT_CAPABILITY',
        'SDL_ASSERT_SHARED_CAPABILITY', 'SDL_RETURN_CAPABILITY', 'SDL_NO_THREAD_SAFETY_ANALYSIS')},
    # SDL_vulkan.h
    'NO_SDL_VULKAN_TYPEDEFS': _COND + ' (set when <vulkan/vulkan_core.h> came first)',
    # SDL_main.h
    'main': "<SDL3/SDL_main.h>'s renaming of main to SDL_main; include that header in the file defining main",
    'SDLMAIN_DECLSPEC': _ANNOTATION,
    'SDL_PLATFORM_PRIVATE_MAIN': _COND,
}

# Whole headers whose macros stay macros.
SIDE_HEADER_FILES = {
    'SDL_platform_defines.h': 'the platform tests (SDL_PLATFORM_LINUX, SDL_PLATFORM_WINDOWS ...), for #if',
    'SDL_begin_code.h': 'attributes, linkage and calling conventions (SDL_DECLSPEC, SDLCALL, SDL_INLINE, '
                        'SDL_NODISCARD, SDL_FALLTHROUGH ...)',
    'SDL_dlopennote.h': 'SDL_ELF_NOTE_DLOPEN, which declares an ELF note at the call site (token pasting, '
                        'stringification)',
    'SDL_oldnames.h': "SDL 2 names, as SDL 3's renames with SDL_ENABLE_OLD_NAMES, or as poison names that "
                      'tell the SDL 3 name without it',
}

EXCLUDED_MACROS = {
    'alloca': "the C library's alloca, defined as __builtin_alloca when no header declares it",
    '__PRFCHWINTRIN_H': "an include guard SDL_endian.h sets for clang-cl's <prfchwintrin.h>",
    'SDL_size_mul_check_overflow': 'a macro only to select the builtin implementation; adapted as the function',
    'SDL_size_add_check_overflow': 'a macro only to select the builtin implementation; adapted as the function',
    'SDL_Swap16': 'a macro or a function by compiler; adapted as the function',
    'SDL_Swap32': 'a macro or a function by compiler; adapted as the function',
    'SDL_Swap64': 'a macro or a function by compiler; adapted as the function',
    # With SDL_SLOW_MEMCPY unset these name the C library's functions, for the
    # compiler's builtins; SDL's own exported functions behave the same and
    # are what the module exports.
    'SDL_memcpy': 'redirects to memcpy for the compiler; the exported SDL_memcpy is the same operation',
    'SDL_memmove': 'redirects to memmove for the compiler; the exported SDL_memmove is the same operation',
    'SDL_memset': 'redirects to memset for the compiler; the exported SDL_memset is the same operation',
}
# Under clang's static analyzer only, the C library replaces SDL's functions.
ANALYZER_REDIRECTS = (
    'SDL_malloc', 'SDL_calloc', 'SDL_realloc', 'SDL_free', 'SDL_memcmp', 'SDL_strlcpy', 'SDL_strlcat', 'SDL_strlen',
    'SDL_wcslen', 'SDL_wcslcpy', 'SDL_wcslcat', 'SDL_strdup', 'SDL_wcsdup', 'SDL_strchr', 'SDL_strrchr',
    'SDL_strstr', 'SDL_wcsstr', 'SDL_strtok_r', 'SDL_strcmp', 'SDL_wcscmp', 'SDL_strncmp', 'SDL_wcsncmp',
    'SDL_strcasecmp', 'SDL_strncasecmp', 'SDL_strpbrk', 'SDL_sscanf', 'SDL_vsscanf', 'SDL_snprintf',
    'SDL_vsnprintf')
EXCLUDED_MACROS.update({n: "clang's static analyzer only: redirected to the C library (the function is exported)"
                        for n in ANALYZER_REDIRECTS})


def is_include_guard(macro) -> bool:
    import re
    return macro.params is None and macro.body == '' and bool(re.fullmatch(r'\w+_h_?', macro.name))

# Mapped string constants whose usual use only a macro serves: printf
# formats concatenated with literals ("%" SDL_PRIs64). The constants are in
# the macros module; for concatenation include <mwrap/sdl3/macros.hpp>
# (tools/run_conformance.py keeps them as macros for upstream's suites).
PREPROCESSOR = {
    'SDL_PRIs64', 'SDL_PRIu64', 'SDL_PRIx64', 'SDL_PRIX64', 'SDL_PRIs32', 'SDL_PRIu32', 'SDL_PRIx32', 'SDL_PRIX32',
    'SDL_PRILL_PREFIX', 'SDL_PRILLd', 'SDL_PRILLu', 'SDL_PRILLx', 'SDL_PRILLX',
}
