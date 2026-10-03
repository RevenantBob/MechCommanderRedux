#pragma once

/// <summary>A shader the Vulkan renderer uses, and the resources its bindings declare.</summary>
struct MCShaderInfo
{
    /// <summary>The shader's name: its source is <c>shaders/&lt;Name&gt;.vshader</c> or <c>.pshader</c>, and a
    /// replacement is <c>&lt;Name&gt;.spv</c>.</summary>
    const char* Name;
    SDL_GPUShaderStage Stage;
    uint32_t Samplers;
    uint32_t UniformBuffers;
    /// <summary>The built-in SPIR-V (compiled from the source by dxc at build time).</summary>
    const unsigned char* Code;
    size_t CodeSize;
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

    /// <summary>The folders searched for replacements, in order.</summary>
    std::vector<std::filesystem::path> SearchFolders();

    /// <summary>Creates <paramref name="info"/>'s shader on <paramref name="device"/>, from a replacement if there is one.</summary>
    std::expected<SDL_GPUShader*, std::string> Load(SDL_GPUDevice* device, const MCShaderInfo& info);
}
