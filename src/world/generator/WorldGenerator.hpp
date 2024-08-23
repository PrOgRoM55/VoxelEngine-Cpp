#pragma once

#include <string>
#include <memory>

#include "typedefs.hpp"
#include "voxels/voxel.hpp"

struct voxel;
class Content;
struct GeneratorDef;

/// @brief High-level world generation controller
class WorldGenerator {
    const GeneratorDef& def;
    const Content* content;
    uint64_t seed;
public:
    /// @param def generator definition
    /// @param content world content
    /// @param seed world seed
    WorldGenerator(
        const GeneratorDef& def,
        const Content* content,
        uint64_t seed
    );
    virtual ~WorldGenerator() = default;

    /// @brief Generate complete chunk voxels
    /// @param voxels destinatiopn chunk voxels buffer
    /// @param x chunk position X divided by CHUNK_W
    /// @param z chunk position Y divided by CHUNK_D
    virtual void generate(voxel* voxels, int x, int z);

    inline static std::string DEFAULT = "core:default";
};
