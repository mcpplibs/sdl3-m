// SDL_GPU through the module: a device on whatever backend the machine has
// (Vulkan, Metal, Direct3D 12), a render pass that clears a texture, copy
// passes that upload a buffer and download it and the texture, a fence.
//
// Without a GPU the test is skipped, unless SDL3M_REQUIRE_GPU=1 (CI sets it,
// with lavapipe on Linux and Windows and Metal on macOS). SDL3M_GPU_VIDEO_DRIVER
// picks the video driver (macOS: cocoa, as Metal needs a real one) and
// SDL3M_VULKAN_LIBRARY the Vulkan loader.
#include <gtest/gtest.h>

import std;
import mcpplibs.mwrap.sdl3;

using namespace mcpplibs::mwrap::sdl3::macros;

namespace {

std::string env(const char* name) {
    const char* v = SDL_getenv(name);
    return v ? v : "";
}

SDL_GPUDevice* open_device(std::string& why) {
    const std::string driver = env("SDL3M_GPU_VIDEO_DRIVER");
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, driver.empty() ? "offscreen" : driver.c_str());
    if (const std::string vk = env("SDL3M_VULKAN_LIBRARY"); !vk.empty()) SDL_SetHint(SDL_HINT_VULKAN_LIBRARY, vk.c_str());
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        why = std::string("initialising video: ") + SDL_GetError();
        return nullptr;
    }
    const SDL_PropertiesID props = SDL_CreateProperties();
    SDL_SetBooleanProperty(props, SDL_PROP_GPU_DEVICE_CREATE_DEBUGMODE_BOOLEAN, false);
    SDL_SetBooleanProperty(props, SDL_PROP_GPU_DEVICE_CREATE_SHADERS_SPIRV_BOOLEAN, true);
    SDL_SetBooleanProperty(props, SDL_PROP_GPU_DEVICE_CREATE_SHADERS_MSL_BOOLEAN, true);
    SDL_SetBooleanProperty(props, SDL_PROP_GPU_DEVICE_CREATE_SHADERS_DXIL_BOOLEAN, true);
    SDL_GPUDevice* device = SDL_CreateGPUDeviceWithProperties(props);
    SDL_DestroyProperties(props);
    if (!device) why = std::string("creating a GPU device: ") + SDL_GetError();
    return device;
}

}  // namespace

TEST(Gpu, ClearUploadDownload) {
    EXPECT_GT(SDL_GetNumGPUDrivers(), 0);
    std::string why;
    SDL_GPUDevice* device = open_device(why);
    if (!device) {
        SDL_Quit();
        if (env("SDL3M_REQUIRE_GPU") == "1") FAIL() << why;
        GTEST_SKIP() << "no GPU device here (" << why << "); SDL3M_REQUIRE_GPU=1 makes this a failure";
    }
    std::println("GPU driver: {}", SDL_GetGPUDeviceDriver(device));
    const SDL_GPUShaderFormat formats = SDL_GetGPUShaderFormats(device);
    EXPECT_NE(formats & (SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_MSL | SDL_GPU_SHADERFORMAT_DXIL), 0u);
    ASSERT_TRUE(SDL_GPUTextureSupportsFormat(device, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, SDL_GPU_TEXTURETYPE_2D,
                                             SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER));

    constexpr Uint32 W = 4, H = 4;
    SDL_GPUTextureCreateInfo tinfo{};
    tinfo.type = SDL_GPU_TEXTURETYPE_2D;
    tinfo.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    tinfo.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
    tinfo.width = W;
    tinfo.height = H;
    tinfo.layer_count_or_depth = 1;
    tinfo.num_levels = 1;
    tinfo.sample_count = SDL_GPU_SAMPLECOUNT_1;
    SDL_GPUTexture* texture = SDL_CreateGPUTexture(device, &tinfo);
    ASSERT_NE(texture, nullptr) << SDL_GetError();
    SDL_SetGPUTextureName(device, texture, "sdl3-m target");

    const std::array<Uint32, 8> words{1, 2, 3, 5, 8, 13, 21, 34};
    SDL_GPUBufferCreateInfo binfo{SDL_GPU_BUFFERUSAGE_VERTEX, sizeof words, 0};
    SDL_GPUBuffer* buffer = SDL_CreateGPUBuffer(device, &binfo);
    ASSERT_NE(buffer, nullptr) << SDL_GetError();
    const Uint32 texture_bytes = W * H * 4;
    SDL_GPUTransferBufferCreateInfo up_info{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, sizeof words, 0};
    SDL_GPUTransferBufferCreateInfo down_info{SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD, texture_bytes + sizeof words, 0};
    SDL_GPUTransferBuffer* up = SDL_CreateGPUTransferBuffer(device, &up_info);
    SDL_GPUTransferBuffer* down = SDL_CreateGPUTransferBuffer(device, &down_info);
    ASSERT_TRUE(up && down) << SDL_GetError();
    void* mapped = SDL_MapGPUTransferBuffer(device, up, false);
    ASSERT_NE(mapped, nullptr);
    std::memcpy(mapped, words.data(), sizeof words);
    SDL_UnmapGPUTransferBuffer(device, up);

    SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(device);
    ASSERT_NE(cmd, nullptr) << SDL_GetError();
    SDL_GPUColorTargetInfo target{};
    target.texture = texture;
    target.clear_color = SDL_FColor{1.0f, 0.5f, 0.0f, 1.0f};
    target.load_op = SDL_GPU_LOADOP_CLEAR;
    target.store_op = SDL_GPU_STOREOP_STORE;
    SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(cmd, &target, 1, nullptr);
    ASSERT_NE(pass, nullptr) << SDL_GetError();
    SDL_EndGPURenderPass(pass);

    SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(cmd);
    const SDL_GPUTransferBufferLocation from_up{up, 0};
    const SDL_GPUBufferRegion whole{buffer, 0, sizeof words};
    SDL_UploadToGPUBuffer(copy, &from_up, &whole, false);
    SDL_GPUTextureRegion region{};
    region.texture = texture;
    region.w = W;
    region.h = H;
    region.d = 1;
    SDL_GPUTextureTransferInfo to_down{};
    to_down.transfer_buffer = down;
    SDL_DownloadFromGPUTexture(copy, &region, &to_down);
    const SDL_GPUTransferBufferLocation buffer_down{down, texture_bytes};
    SDL_DownloadFromGPUBuffer(copy, &whole, &buffer_down);
    SDL_EndGPUCopyPass(copy);

    SDL_GPUFence* fence = SDL_SubmitGPUCommandBufferAndAcquireFence(cmd);
    ASSERT_NE(fence, nullptr) << SDL_GetError();
    ASSERT_TRUE(SDL_WaitForGPUFences(device, true, &fence, 1)) << SDL_GetError();
    EXPECT_TRUE(SDL_QueryGPUFence(device, fence));
    SDL_ReleaseGPUFence(device, fence);

    const auto* bytes = static_cast<const Uint8*>(SDL_MapGPUTransferBuffer(device, down, false));
    ASSERT_NE(bytes, nullptr);
    for (Uint32 i = 0; i < W * H; ++i) {
        EXPECT_EQ(bytes[i * 4 + 0], 255);
        EXPECT_NEAR(bytes[i * 4 + 1], 128, 1);
        EXPECT_EQ(bytes[i * 4 + 2], 0);
        EXPECT_EQ(bytes[i * 4 + 3], 255);
    }
    std::array<Uint32, 8> back{};
    std::memcpy(back.data(), bytes + texture_bytes, sizeof back);
    EXPECT_EQ(back, words);
    SDL_UnmapGPUTransferBuffer(device, down);

    EXPECT_TRUE(SDL_WaitForGPUIdle(device));
    SDL_ReleaseGPUTransferBuffer(device, up);
    SDL_ReleaseGPUTransferBuffer(device, down);
    SDL_ReleaseGPUBuffer(device, buffer);
    SDL_ReleaseGPUTexture(device, texture);
    SDL_DestroyGPUDevice(device);
    SDL_Quit();
}
