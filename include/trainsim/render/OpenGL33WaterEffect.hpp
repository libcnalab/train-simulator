#pragma once
#include <memory>
#include "Microsoft/Xna/Framework/Matrix.hpp"
#include "Microsoft/Xna/Framework/Vector3.hpp"
namespace Microsoft::Xna::Framework::Graphics {
class GraphicsDevice; class BasicEffect; class ShaderEffect;
}
namespace TrainSim::Render {
// Opaque, single-submission water approximation confined to the renderer.
class OpenGL33WaterEffect
{
public:
    explicit OpenGL33WaterEffect(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device);
    ~OpenGL33WaterEffect();
    bool valid() const;
    // Sets up and applies the shader for one draw; the caller then draws the mesh.
    void apply(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device,
        Microsoft::Xna::Framework::Graphics::BasicEffect& reference,
        const Microsoft::Xna::Framework::Matrix& world,const Microsoft::Xna::Framework::Matrix& view,
        const Microsoft::Xna::Framework::Matrix& projection,double serviceSeconds,
        const Microsoft::Xna::Framework::Vector3& skyColour={0.35f,0.59f,0.76f},
        float sunSparkle=1.0f);
private:
    std::unique_ptr<Microsoft::Xna::Framework::Graphics::ShaderEffect> effect_;
};
}
