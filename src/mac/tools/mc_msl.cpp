// Translates one of the port's shaders from SPIR-V to MSL for SDL GPU's Metal driver, as a header for
// MCVulkanShaders.cpp: mc_msl <shader.spv> <shader.msl.h> <variable name>.
//
// The Mac build compiles the HLSL in src/lib/MCCore/shaders with glslang (dxc, which Windows uses, has no macOS
// build) and runs this on the result. The SPIR-V follows SDL GPU's SPIR-V resource layout and the MSL must follow its
// Metal one (SDL_CreateGPUShader):
//   SPIR-V vertex:   set 0 sampled textures, storage textures, storage buffers; set 1 uniform buffers.
//   SPIR-V fragment: set 2 sampled textures, storage textures, storage buffers; set 3 uniform buffers.
//   MSL:             [[texture(n)]] sampled then storage textures, [[sampler(n)]] beside its texture,
//                    [[buffer(n)]] uniform buffers then storage buffers.
// SPIRV-Cross numbers resources in the order it meets them unless told, so every resource is bound explicitly.

#include <spirv_cross/spirv_msl.hpp>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace
{
    /// <summary>The entry point SDL is given for every shader (MCVulkanShaders): SPIRV-Cross renames "main".</summary>
    constexpr const char* MslEntryPoint = "main0";
    /// <summary>The bytes on each line of the header.</summary>
    constexpr size_t BytesPerLine = 16;
    /// <summary>Room for one byte's text: a new line and indent, "0xff," and the terminator.</summary>
    constexpr size_t ByteTextSize = 16;
    constexpr uint32_t MslVersionMajor = 2;
    constexpr uint32_t MslVersionMinor = 1;

    std::vector<uint32_t> ReadWords(const char* path)
    {
        std::ifstream in(path, std::ios::binary);

        if (!in)
        {
            throw std::runtime_error(std::string("can't read ") + path);
        }

        const std::vector<char> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());

        if (bytes.empty() || bytes.size() % sizeof(uint32_t) != 0)
        {
            throw std::runtime_error(std::string(path) + " isn't SPIR-V (its size isn't a whole number of words)");
        }

        std::vector<uint32_t> words(bytes.size() / sizeof(uint32_t));
        std::memcpy(words.data(), bytes.data(), bytes.size());
        return words;
    }

    /// <summary>Binds every resource of <paramref name="compiler"/>'s shader where SDL GPU's Metal driver binds it.</summary>
    void BindAsSdl(spirv_cross::CompilerMSL& compiler, const char* path)
    {
        const spv::ExecutionModel stage = compiler.get_execution_model();

        if (stage != spv::ExecutionModelVertex && stage != spv::ExecutionModelFragment)
        {
            throw std::runtime_error(std::string(path) + ": only vertex and fragment shaders are supported");
        }

        const uint32_t resourceSet = stage == spv::ExecutionModelVertex ? 0 : 2;
        const uint32_t uniformSet = resourceSet + 1;
        const spirv_cross::ShaderResources resources = compiler.get_shader_resources();

        const auto binding = [&](const spirv_cross::Resource& resource, uint32_t expectedSet)
        {
            const uint32_t set = compiler.get_decoration(resource.id, spv::DecorationDescriptorSet);

            if (set != expectedSet)
            {
                throw std::runtime_error(std::string(path) + ": " + resource.name + " is in set " + std::to_string(set) +
                                         ", SDL expects set " + std::to_string(expectedSet));
            }

            return compiler.get_decoration(resource.id, spv::DecorationBinding);
        };

        // One entry per SPIR-V binding: an HLSL texture and its sampler share one (t0 and s0), and each kind of resource
        // reads its own index from the entry.
        std::map<std::pair<uint32_t, uint32_t>, spirv_cross::MSLResourceBinding> entries;

        const auto entry = [&](uint32_t set, uint32_t spirvBinding) -> spirv_cross::MSLResourceBinding&
        {
            spirv_cross::MSLResourceBinding& msl = entries[{set, spirvBinding}];
            msl.stage = stage;
            msl.desc_set = set;
            msl.binding = spirvBinding;
            return msl;
        };

        // Textures come first in the resource set, so a storage buffer's Metal index counts from past them.
        const uint32_t textures = static_cast<uint32_t>(resources.separate_images.size() +
                                                        resources.sampled_images.size() + resources.storage_images.size());
        const uint32_t uniforms = static_cast<uint32_t>(resources.uniform_buffers.size());

        for (const auto* list : {&resources.separate_images, &resources.sampled_images, &resources.storage_images})
        {
            for (const spirv_cross::Resource& image : *list)
            {
                const uint32_t index = binding(image, resourceSet);
                entry(resourceSet, index).msl_texture = index;
            }
        }

        for (const spirv_cross::Resource& sampler : resources.separate_samplers)
        {
            const uint32_t index = binding(sampler, resourceSet);
            entry(resourceSet, index).msl_sampler = index;
        }

        for (const spirv_cross::Resource& uniform : resources.uniform_buffers)
        {
            const uint32_t index = binding(uniform, uniformSet);
            entry(uniformSet, index).msl_buffer = index;
        }

        for (const spirv_cross::Resource& storage : resources.storage_buffers)
        {
            const uint32_t index = binding(storage, resourceSet);

            if (index < textures)
            {
                throw std::runtime_error(std::string(path) + ": storage buffer " + storage.name +
                                         " is bound before the textures, SDL expects it after them");
            }

            entry(resourceSet, index).msl_buffer = uniforms + (index - textures);
        }

        for (const auto& [key, msl] : entries)
        {
            compiler.add_msl_resource_binding(msl);
        }
    }

    /// <summary>
    /// The header: <paramref name="msl"/>'s bytes as <paramref name="variable"/>, like the SPIR-V headers dxc writes
    /// on Windows (SDL takes the source's length, so there is no terminating zero).
    /// </summary>
    std::string Header(const std::string& msl, const char* variable, const char* source)
    {
        std::string header = std::string("#pragma once\n// Generated by mc_msl from ") +
                             std::filesystem::path(source).filename().string() + "; don't edit.\n" +
                             "static const unsigned char " + variable + "[] = {";

        for (size_t i = 0; i < msl.size(); ++i)
        {
            char byte[ByteTextSize];
            std::snprintf(byte, sizeof(byte), "%s0x%02x,", i % BytesPerLine == 0 ? "\n    " : " ",
                          static_cast<unsigned char>(msl[i]));
            header += byte;
        }

        return header + "\n};\n";
    }
}

int main(int argc, char** argv)
{
    if (argc != 4)
    {
        std::fprintf(stderr, "usage: mc_msl <shader.spv> <shader.msl.h> <variable name>\n");
        return 2;
    }

    try
    {
        spirv_cross::CompilerMSL compiler(ReadWords(argv[1]));
        spirv_cross::CompilerMSL::Options options;
        options.platform = spirv_cross::CompilerMSL::Options::macOS;
        options.set_msl_version(MslVersionMajor, MslVersionMinor);
        compiler.set_msl_options(options);
        BindAsSdl(compiler, argv[1]);
        const std::string msl = compiler.compile();
        const std::string entry = compiler.get_cleansed_entry_point_name("main", compiler.get_execution_model());

        if (entry != MslEntryPoint)
        {
            throw std::runtime_error(std::string(argv[1]) + ": the entry point became " + entry + ", not " +
                                     MslEntryPoint);
        }

        std::ofstream out(argv[2], std::ios::binary | std::ios::trunc);
        out << Header(msl, argv[3], argv[1]);

        if (!out)
        {
            throw std::runtime_error(std::string("can't write ") + argv[2]);
        }
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "mc_msl: %s\n", error.what());
        return 1;
    }

    return 0;
}
