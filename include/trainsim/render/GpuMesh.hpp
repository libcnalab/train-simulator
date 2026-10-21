#pragma once

#include "trainsim/render/MeshBuilder.hpp"

#include "CnaEngine/Graphics/GpuMesh.hpp"

#include <memory>
#include <span>
#include <vector>

namespace Microsoft::Xna::Framework::Graphics {
class GraphicsDevice;
class IndexBuffer;
class VertexBuffer;
}

namespace TrainSim::Render {

// One drawable mesh in one or more GPU buffer pairs. Meshes beyond kMaxMeshSegmentVertices are
// split into segments, so no single buffer reaches the graphics-profile 64 MiB limit.
class GpuMesh
{
public:
    GpuMesh(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device, const MeshData& data,
            bool streamed = false);
    // An empty streamed mesh whose segments are filled by replaceSegment.
    GpuMesh();
    // Full Discard uploads reuse exact-sized buffers; changed topology reallocates as needed.
    int replace(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device, const MeshData& data);
    // Upload one segment of `data` (streamed meshes only), so a large material can be staged
    // over several frames. The mesh keeps exactly `segments.size()` segments.
    int replaceSegment(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device,
                       const MeshData& data,std::span<const MeshSegment> segments,
                       std::size_t index);
    // Continue staging one segment with at most about `maxBytes` of vertex/index data in this
    // call (always at least one slice). Returns true once the segment is complete; `reused`
    // counts buffers kept from the previous contents and `submitted` accumulates the bytes
    // written. The mesh must not be drawn meanwhile.
    bool stageSegment(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device,
                      const MeshData& data,std::span<const MeshSegment> segments,
                      std::size_t index,std::size_t maxBytes,int& reused,std::size_t& submitted);
    // Forget partial staging progress, e.g. when a prepared window is discarded mid-upload.
    void resetStaging() noexcept
    {
        for (auto& segment : segments_) segment.stagedVertices=segment.stagedIndices=0;
    }
    ~GpuMesh();
    GpuMesh(const GpuMesh&) = delete;
    GpuMesh& operator=(const GpuMesh&) = delete;
    void draw(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device) const;
    [[nodiscard]] int triangles() const noexcept;
    [[nodiscard]] std::size_t segmentCount() const noexcept { return segments_.size(); }
    // One segment as cna-engine draws it: its buffers, borrowed. Not drawable while empty.
    [[nodiscard]] const CnaEngine::Graphics::GpuMesh& segment(std::size_t index) const
    {
        return segments_.at(index).view;
    }

private:
    struct Segment
    {
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::VertexBuffer> vertices;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::IndexBuffer> indices;
        int vertexCount = 0;
        int primitiveCount = 0;
        std::size_t stagedVertices = 0;
        std::size_t stagedIndices = 0;
        // Refreshed whenever the buffers or counts change. Bounds are not tracked: nothing
        // culls or sorts these meshes by them.
        CnaEngine::Graphics::GpuMesh view;
    };
    static void refreshView(Segment& segment);
    int upload(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device,const MeshData& data,
               const MeshSegment& range,Segment& segment);
    std::vector<Segment> segments_;
    bool streamed_ = false;
};

} // namespace TrainSim::Render
