#include "stdafx.h"
#include "platform/MCVulkanShaders.h"
#include "platform/MCFileSystem.h"
#include "shaders/composite.spv.h"
#include "shaders/draw.spv.h"
#include "shaders/instance.spv.h"
#include "shaders/quad.spv.h"
#include "shaders/terrain.spv.h"

namespace MCVulkanShaders
{
    const MCShaderInfo Quad{"quad", SDL_GPU_SHADERSTAGE_VERTEX, 0, 1, MC_quad_spirv, sizeof(MC_quad_spirv)};
    const MCShaderInfo Composite{"composite", SDL_GPU_SHADERSTAGE_FRAGMENT, 5,
                                 1,           MC_composite_spirv,           sizeof(MC_composite_spirv)};
    const MCShaderInfo DrawVertex{"instance",        SDL_GPU_SHADERSTAGE_VERTEX, 0, 1,
                                  MC_instance_spirv, sizeof(MC_instance_spirv),  1};
    const MCShaderInfo DrawFragment{"draw", SDL_GPU_SHADERSTAGE_FRAGMENT, 5, 1, MC_draw_spirv, sizeof(MC_draw_spirv)};
    const MCShaderInfo TerrainVertex{"terrain",        SDL_GPU_SHADERSTAGE_VERTEX, 1, 1,
                                     MC_terrain_spirv, sizeof(MC_terrain_spirv),   2};

    std::vector<std::filesystem::path> SearchFolders()
    {
        std::vector<std::filesystem::path> folders;

        if (const char* base = SDL_GetBasePath(); base != nullptr)
        {
            folders.push_back(std::filesystem::path(base) / "shaders");
        }

        if (const std::filesystem::path& data = MCFileSystem::GameRoot(); !data.empty())
        {
            folders.push_back(data / "shaders");
        }

        return folders;
    }

    namespace
    {
        /// <summary>The first replacement for <paramref name="name"/>, or empty.</summary>
        std::filesystem::path FindReplacement(const char* name)
        {
            const std::string file = std::format("{}.spv", name);

            for (const std::filesystem::path& folder : SearchFolders())
            {
                std::error_code error;
                const std::filesystem::path path = folder / file;

                if (std::filesystem::is_regular_file(path, error))
                {
                    return path;
                }
            }

            return {};
        }

        std::vector<unsigned char> ReadFile(const std::filesystem::path& path)
        {
            std::ifstream stream(path, std::ios::binary);
            return std::vector<unsigned char>(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
        }
    }

    std::expected<SDL_GPUShader*, std::string> Load(SDL_GPUDevice* device, const MCShaderInfo& info)
    {
        SDL_GPUShaderCreateInfo create{};
        create.code = info.Code;
        create.code_size = info.CodeSize;
        create.entrypoint = "main";
        create.format = SDL_GPU_SHADERFORMAT_SPIRV;
        create.stage = info.Stage;
        create.num_samplers = info.Samplers;
        create.num_uniform_buffers = info.UniformBuffers;
        create.num_storage_buffers = info.StorageBuffers;
        std::vector<unsigned char> replacement;

        if (const std::filesystem::path path = FindReplacement(info.Name); !path.empty())
        {
            replacement = ReadFile(path);

            if (replacement.empty() || replacement.size() % 4 != 0)
            {
                SDL_Log("MCVulkanShaders: %s isn't SPIR-V; using the built-in %s", path.string().c_str(), info.Name);
            }
            else
            {
                create.code = replacement.data();
                create.code_size = replacement.size();
                SDL_Log("MCVulkanShaders: %s replaced by %s", info.Name, path.string().c_str());
            }
        }

        SDL_GPUShader* shader = SDL_CreateGPUShader(device, &create);

        if (shader == nullptr && create.code != info.Code)
        {
            SDL_Log("MCVulkanShaders: the replacement for %s failed (%s); using the built-in", info.Name,
                    SDL_GetError());
            create.code = info.Code;
            create.code_size = info.CodeSize;
            shader = SDL_CreateGPUShader(device, &create);
        }

        if (shader == nullptr)
        {
            return std::unexpected(std::format("SDL_CreateGPUShader({}): {}", info.Name, SDL_GetError()));
        }

        return shader;
    }
}
