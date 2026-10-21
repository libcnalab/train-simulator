#include "trainsim/render/GpuMesh.hpp"

#include "Microsoft/Xna/Framework/Graphics/BufferUsage.hpp"
#include "Microsoft/Xna/Framework/Graphics/DynamicVertexBuffer.hpp"
#include "Microsoft/Xna/Framework/Graphics/DynamicIndexBuffer.hpp"
#include "Microsoft/Xna/Framework/Graphics/SetDataOptions.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp"
#include "Microsoft/Xna/Framework/Graphics/IndexBuffer.hpp"
#include "Microsoft/Xna/Framework/Graphics/IndexElementSize.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexBuffer.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexDeclaration.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexElement.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexElementFormat.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexElementUsage.hpp"

#include <cstddef>
#include <stdexcept>

using namespace Microsoft::Xna::Framework::Graphics;

namespace TrainSim::Render {

namespace {
const VertexDeclaration& litColorDeclaration()
{
    static const VertexDeclaration declaration(static_cast<int>(sizeof(LitColorVertex)), {
        VertexElement(static_cast<int>(offsetof(LitColorVertex, position)),
                      VertexElementFormat::Vector3, VertexElementUsage::Position, 0),
        VertexElement(static_cast<int>(offsetof(LitColorVertex, normal)),
                      VertexElementFormat::Vector3, VertexElementUsage::Normal, 0),
        VertexElement(static_cast<int>(offsetof(LitColorVertex, textureCoordinate)),
                      VertexElementFormat::Vector2, VertexElementUsage::TextureCoordinate, 0),
        VertexElement(static_cast<int>(offsetof(LitColorVertex, color)),
                      VertexElementFormat::Color, VertexElementUsage::Color, 0),
    });
    return declaration;
}
}

GpuMesh::GpuMesh() : streamed_(true) {}

void GpuMesh::refreshView(Segment& segment)
{
    using CnaEngine::Graphics::VertexAttribute;
    segment.view = segment.vertices && segment.indices && segment.vertexCount > 0
                       && segment.primitiveCount > 0
        ? CnaEngine::Graphics::GpuMesh::Borrow(*segment.vertices,*segment.indices,
              segment.vertexCount,segment.primitiveCount,{},
              {VertexAttribute::Position,VertexAttribute::Normal,VertexAttribute::Color,
               VertexAttribute::TexCoord0})
        : CnaEngine::Graphics::GpuMesh{};
}

GpuMesh::GpuMesh(GraphicsDevice& device, const MeshData& data, bool streamed)
    : streamed_(streamed)
{
    if (data.empty() || data.indices.size() % 3 != 0)
        throw std::invalid_argument("GpuMesh requires a non-empty triangle list");
    const auto ranges=meshSegments(data);
    segments_.resize(ranges.size());
    for (std::size_t i=0;i<ranges.size();++i) upload(device,data,ranges[i],segments_[i]);
}

int GpuMesh::upload(GraphicsDevice& device,const MeshData& data,const MeshSegment& range,
                    Segment& segment)
{
    const int vertices=static_cast<int>(range.vertexCount);
    const int primitives=static_cast<int>(range.indexCount/3);
    const LitColorVertex* vertexData=data.vertices.data()+range.firstVertex;
    // Segment indices are rebased to their own vertex buffer; the whole-mesh case needs no copy.
    std::vector<std::uint32_t> rebased;
    const std::uint32_t* indexData=data.indices.data()+range.firstIndex;
    if (range.firstVertex!=0)
    {
        rebased.assign(indexData,indexData+range.indexCount);
        for (auto& index : rebased) index-=static_cast<std::uint32_t>(range.firstVertex);
        indexData=rebased.data();
    }
    if (!streamed_)
    {
        segment.vertices=std::make_unique<VertexBuffer>(device,litColorDeclaration(),vertices,
                                                        BufferUsage::WriteOnly);
        segment.vertices->SetDataRaw(vertexData,vertices,static_cast<int>(sizeof(LitColorVertex)));
        segment.indices=std::make_unique<IndexBuffer>(device,IndexElementSize::ThirtyTwoBits,
                                                      primitives*3,BufferUsage::WriteOnly);
        segment.indices->SetData(indexData,primitives*3);
        segment.vertexCount=vertices;
        segment.primitiveCount=primitives;
        refreshView(segment);
        return 0;
    }
    const int reused=(segment.vertices && vertices == segment.vertexCount ? 1 : 0)
                    +(segment.indices && primitives == segment.primitiveCount ? 1 : 0);
    if (!segment.vertices || vertices != segment.vertexCount)
        segment.vertices=std::make_unique<DynamicVertexBuffer>(device,litColorDeclaration(),vertices,
                                                               BufferUsage::WriteOnly);
    if (!segment.indices || primitives != segment.primitiveCount)
        segment.indices=std::make_unique<DynamicIndexBuffer>(device,IndexElementSize::ThirtyTwoBits,
                                                             primitives*3,BufferUsage::WriteOnly);
    static_cast<DynamicVertexBuffer&>(*segment.vertices).SetDataRawWithOptionsEXT(
        vertexData,vertices,static_cast<int>(sizeof(LitColorVertex)),SetDataOptions::Discard);
    static_cast<DynamicIndexBuffer&>(*segment.indices).SetData(
        indexData,0,primitives*3,SetDataOptions::Discard);
    segment.vertexCount=vertices;
    segment.primitiveCount=primitives;
    refreshView(segment);
    return reused;
}

int GpuMesh::replace(GraphicsDevice& device, const MeshData& data)
{
    if (!streamed_) throw std::logic_error("Only streamed meshes support replacement");
    if (data.empty() || data.indices.size()%3 != 0)
        throw std::invalid_argument("GpuMesh requires a non-empty triangle list");
    const auto ranges=meshSegments(data);
    int reused=0;
    for (std::size_t i=0;i<ranges.size();++i) reused+=replaceSegment(device,data,ranges,i);
    return reused;
}

int GpuMesh::replaceSegment(GraphicsDevice& device,const MeshData& data,
                            std::span<const MeshSegment> segments,std::size_t index)
{
    if (!streamed_) throw std::logic_error("Only streamed meshes support replacement");
    if (index>=segments.size() || data.empty() || data.indices.size()%3 != 0)
        throw std::invalid_argument("GpuMesh segment replacement needs a valid segment");
    segments_.resize(segments.size());
    segments_[index].stagedVertices=0;
    segments_[index].stagedIndices=0;
    return upload(device,data,segments[index],segments_[index]);
}

bool GpuMesh::stageSegment(GraphicsDevice& device,const MeshData& data,
                           std::span<const MeshSegment> segments,std::size_t index,
                           std::size_t maxBytes,int& reused,std::size_t& submitted)
{
    if (!streamed_) throw std::logic_error("Only streamed meshes support staging");
    if (index>=segments.size() || data.empty() || data.indices.size()%3 != 0)
        throw std::invalid_argument("GpuMesh segment staging needs a valid segment");
    segments_.resize(segments.size());
    const MeshSegment& range=segments[index];
    Segment& segment=segments_[index];
    const int vertices=static_cast<int>(range.vertexCount);
    const int primitives=static_cast<int>(range.indexCount/3);
    if (segment.stagedVertices==0 && segment.stagedIndices==0)
    {
        // Exact-size buffers are kept when the segment shape repeats; otherwise reallocate.
        reused+=(segment.vertices && vertices==segment.vertexCount ? 1 : 0)
               +(segment.indices && primitives==segment.primitiveCount ? 1 : 0);
        if (!segment.vertices || vertices!=segment.vertexCount)
            segment.vertices=std::make_unique<DynamicVertexBuffer>(device,litColorDeclaration(),
                                                                   vertices,BufferUsage::WriteOnly);
        if (!segment.indices || primitives!=segment.primitiveCount)
            segment.indices=std::make_unique<DynamicIndexBuffer>(device,IndexElementSize::ThirtyTwoBits,
                                                                 primitives*3,BufferUsage::WriteOnly);
        segment.vertexCount=vertices;
        segment.primitiveCount=primitives;
        refreshView(segment);
    }
    std::size_t budget=std::max<std::size_t>(maxBytes,sizeof(LitColorVertex)*3);
    if (segment.stagedVertices<range.vertexCount)
    {
        const std::size_t count=std::min(range.vertexCount-segment.stagedVertices,
                                         std::max<std::size_t>(1,budget/sizeof(LitColorVertex)));
        static_cast<DynamicVertexBuffer&>(*segment.vertices).SetDataRawAtWithOptionsEXT(
            static_cast<int>(segment.stagedVertices*sizeof(LitColorVertex)),
            data.vertices.data()+range.firstVertex+segment.stagedVertices,static_cast<int>(count),
            static_cast<int>(sizeof(LitColorVertex)),
            segment.stagedVertices==0 ? SetDataOptions::Discard : SetDataOptions::None);
        segment.stagedVertices+=count;
        submitted+=count*sizeof(LitColorVertex);
        budget-=std::min(budget,count*sizeof(LitColorVertex));
        if (segment.stagedVertices<range.vertexCount || budget==0) return false;
    }
    const std::size_t count=std::min(range.indexCount-segment.stagedIndices,
                                     std::max<std::size_t>(3,budget/sizeof(std::uint32_t)/3*3));
    std::vector<std::uint32_t> rebased(data.indices.begin()+static_cast<std::ptrdiff_t>(
                                           range.firstIndex+segment.stagedIndices),
                                       data.indices.begin()+static_cast<std::ptrdiff_t>(
                                           range.firstIndex+segment.stagedIndices+count));
    if (range.firstVertex!=0)
        for (auto& value : rebased) value-=static_cast<std::uint32_t>(range.firstVertex);
    static_cast<DynamicIndexBuffer&>(*segment.indices).SetData(
        static_cast<int>(segment.stagedIndices*sizeof(std::uint32_t)),rebased.data(),0,
        static_cast<int>(count),
        segment.stagedIndices==0 ? SetDataOptions::Discard : SetDataOptions::None);
    segment.stagedIndices+=count;
    submitted+=count*sizeof(std::uint32_t);
    if (segment.stagedIndices<range.indexCount) return false;
    segment.stagedVertices=0;
    segment.stagedIndices=0;
    return true;
}

GpuMesh::~GpuMesh() = default;

int GpuMesh::triangles() const noexcept
{
    int total=0;
    for (const auto& segment : segments_) total+=segment.primitiveCount;
    return total;
}

void GpuMesh::draw(GraphicsDevice& device) const
{
    for (const auto& segment : segments_) segment.view.Draw(device);
}

} // namespace TrainSim::Render
