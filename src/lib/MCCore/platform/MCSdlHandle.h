#pragma once

/// <summary>Destroys an SDL object with the call SDL gives for its kind.</summary>
struct MCSdlDeleter
{
    void operator()(SDL_Window* window) const { SDL_DestroyWindow(window); }
    void operator()(SDL_Renderer* renderer) const { SDL_DestroyRenderer(renderer); }
    void operator()(SDL_Texture* texture) const { SDL_DestroyTexture(texture); }
    void operator()(SDL_Palette* palette) const { SDL_DestroyPalette(palette); }
    void operator()(SDL_Surface* surface) const { SDL_DestroySurface(surface); }
    void operator()(SDL_Cursor* cursor) const { SDL_DestroyCursor(cursor); }
    void operator()(SDL_AudioStream* stream) const { SDL_DestroyAudioStream(stream); }
    void operator()(SDL_GPUDevice* device) const { SDL_DestroyGPUDevice(device); }
};

/// <summary>An SDL object owned alone: destroyed with the handle.</summary>
template <typename T> using MCSdlHandle = std::unique_ptr<T, MCSdlDeleter>;

/// <summary>
/// Releases a GPU object back to the device that made it. It holds the device, so a GPU object never outlives it.
/// </summary>
struct MCGpuRelease
{
    /// <summary>The device the object belongs to.</summary>
    std::shared_ptr<SDL_GPUDevice> Device;

    void operator()(SDL_GPUTexture* texture) const { SDL_ReleaseGPUTexture(Device.get(), texture); }
    void operator()(SDL_GPUBuffer* buffer) const { SDL_ReleaseGPUBuffer(Device.get(), buffer); }
    void operator()(SDL_GPUTransferBuffer* buffer) const { SDL_ReleaseGPUTransferBuffer(Device.get(), buffer); }
    void operator()(SDL_GPUShader* shader) const { SDL_ReleaseGPUShader(Device.get(), shader); }
    void operator()(SDL_GPUGraphicsPipeline* pipeline) const { SDL_ReleaseGPUGraphicsPipeline(Device.get(), pipeline); }
    void operator()(SDL_GPUSampler* sampler) const { SDL_ReleaseGPUSampler(Device.get(), sampler); }
};

/// <summary>A GPU object owned alone, released to its device with the handle.</summary>
template <typename T> using MCGpuHandle = std::unique_ptr<T, MCGpuRelease>;
