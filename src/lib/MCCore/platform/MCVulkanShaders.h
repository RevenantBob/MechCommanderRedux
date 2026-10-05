#pragma once

#include "platform/MCSdlHandle.h"

/// <summary>The Vulkan renderer's built-in shaders.</summary>
enum class MCShader : uint8_t
{
    Quad,
    Composite,
    Instance,
    Draw,
    Terrain
};

/// <summary>A shader the Vulkan renderer uses, and the resources its bindings declare.</summary>
struct MCShaderInfo
{
    /// <summary>Which built-in shader: its name and SPIR-V (see <see cref="MCVulkanShaders::Name"/>).</summary>
    MCShader Shader;
    SDL_GPUShaderStage Stage;
    uint32_t Samplers;
    uint32_t UniformBuffers;
    uint32_t StorageBuffers = 0;
};

/// <summary>
/// Loads the Vulkan renderer's shaders. A compiled SPIR-V file named after a built-in shader replaces it: the
/// <c>shaders</c> folder beside the executable is looked in first, then the one in the game's data folder. A
/// replacement must keep the bindings the built-in's source documents.
/// </summary>
namespace MCVulkanShaders
{
    /// <summary>The built-in shaders.</summary>
    extern const MCShaderInfo Quad;
    extern const MCShaderInfo Composite;
    extern const MCShaderInfo DrawVertex;
    extern const MCShaderInfo DrawFragment;
    extern const MCShaderInfo TerrainVertex;

    /// <summary>
    /// The shader's name: its source is <c>shaders/&lt;name&gt;.vshader</c> or <c>.pshader</c>, and a replacement is
    /// <c>&lt;name&gt;.spv</c>.
    /// </summary>
    std::string_view Name(MCShader shader);

    /// <summary>The built-in SPIR-V (compiled from the source by dxc at build time, embedded in the executable).</summary>
    std::span<const unsigned char> BuiltInCode(MCShader shader);

    /// <summary>The folders searched for replacements, in order.</summary>
    std::vector<std::filesystem::path> SearchFolders();

    /// <summary>
    /// Creates <paramref name="info"/>'s shader on <paramref name="device"/>, from a replacement if there is one; the
    /// shader holds the device.
    /// </summary>
    std::expected<MCGpuHandle<SDL_GPUShader>, std::string> Load(std::shared_ptr<SDL_GPUDevice> device,
                                                                const MCShaderInfo& info);
}
