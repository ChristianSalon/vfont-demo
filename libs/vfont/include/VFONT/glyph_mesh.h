﻿/**
 * @file glyph_mesh.h
 * @author Christian Saloň
 */

#pragma once

#include <cstdint>
#include <vector>

#include <glm/vec2.hpp>

namespace vft {

class GlyphMesh {
protected:
    std::vector<glm::vec2> _vertices{};
    std::vector<std::vector<uint32_t>> _indices{};

public:
    GlyphMesh(std::vector<glm::vec2> vertices, std::vector<std::vector<uint32_t>> indices);
    GlyphMesh();
    ~GlyphMesh() = default;

    void addVertex(glm::vec2 vertex);

    void setVertices(std::vector<glm::vec2> vertices);
    void setIndices(unsigned int drawIndex, std::vector<uint32_t> indices);

    const std::vector<glm::vec2> &getVertices() const;
    const std::vector<uint32_t> &getIndices(unsigned int drawIndex) const;

    uint32_t getVertexCount() const;
    int getIndexCount(unsigned int drawIndex) const;
    unsigned int getDrawCount(unsigned int drawIndex) const;
};

}  // namespace vft
