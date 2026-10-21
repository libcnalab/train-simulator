#include "SceneShared.hpp"

#include "Microsoft/Xna/Framework/Graphics/SamplerStateCollection.hpp"

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;
using CnaEngine::Graphics::Material;
using CnaEngine::Graphics::ShadingModel;

namespace TrainSim::Render {

namespace {
void applyFirstPass(Effect& effect)
{
    effect.getCurrentTechniqueProperty()->getPassesProperty()[0]->Apply();
}
}

void SceneRenderer::Binder::BeginFrame(GraphicsDevice&, const CnaEngine::Graphics::Camera&,
                                       const CnaEngine::Graphics::ForwardLighting&)
{
    vertexEffect_ = {};
    materialEffect_ = {};
}

void SceneRenderer::Binder::applyLight(BasicEffect& effect, Configured& configured,
                                       Look::Light light) const
{
    if (configured.light == light) return;
    configured.light = light;
    const FrameLighting& frame = scene_.frame_;
    effect.DirectionalLight0.setDirectionProperty(frame.daylight);
    if (light == Look::Light::Saloon)
    {
        // The passenger saloon is independently lit, so it stays comfortable and readable while
        // the scenery beyond the windows transitions through a tunnel. Broad, diffuse
        // ceiling-strip approximation: opposed longitudinal fill reveals both seat orientations
        // without specular points or a sun-facing/non-sun-facing black side. Keep upward-facing
        // light bounded: these lights supplement, not duplicate, the sun.
        effect.setAmbientLightColorProperty(Vector3{0.60f,0.61f,0.62f});
        effect.DirectionalLight0.setDiffuseColorProperty(Vector3{0.26f,0.25f,0.23f});
        effect.DirectionalLight1.setEnabledProperty(true);
        effect.DirectionalLight1.setDirectionProperty(Vector3{0.0f,-0.70710678f,-0.70710678f});
        effect.DirectionalLight1.setDiffuseColorProperty(Vector3{0.16f,0.17f,0.18f});
        effect.DirectionalLight1.setSpecularColorProperty(Vector3::Zero);
        effect.DirectionalLight2.setEnabledProperty(true);
        effect.DirectionalLight2.setDirectionProperty(Vector3{0.0f,-0.70710678f,0.70710678f});
        effect.DirectionalLight2.setDiffuseColorProperty(Vector3{0.16f,0.17f,0.18f});
        effect.DirectionalLight2.setSpecularColorProperty(Vector3::Zero);
        return;
    }
    // Platform lamps replace the sun and sky on the paving after dark; shaded facades otherwise
    // take only the cool sky ambient and lose their own hue, hence the bounce and uplight.
    const ExteriorLighting& key = light == Look::Light::Floodlit ? frame.floodlit : frame.outdoor;
    effect.setAmbientLightColorProperty(key.ambient);
    effect.DirectionalLight0.setDiffuseColorProperty(key.diffuse);
    effect.DirectionalLight1.setEnabledProperty(true);
    effect.DirectionalLight1.setDirectionProperty(frame.bounceDirection);
    effect.DirectionalLight1.setDiffuseColorProperty(frame.outdoor.bounce);
    effect.DirectionalLight1.setSpecularColorProperty(Vector3::Zero);
    effect.DirectionalLight2.setEnabledProperty(true);
    effect.DirectionalLight2.setDirectionProperty(frame.uplightDirection);
    effect.DirectionalLight2.setDiffuseColorProperty(frame.outdoor.uplight);
    effect.DirectionalLight2.setSpecularColorProperty(Vector3::Zero);
}

void SceneRenderer::Binder::applyFog(BasicEffect& effect, Configured& configured, bool enabled) const
{
    if (configured.fog == enabled) return;
    configured.fog = enabled;
    effect.setFogEnabledProperty(enabled);
    effect.setFogColorProperty(scene_.frame_.fogColour);
    effect.setFogStartProperty(scene_.frame_.fogStart);
    effect.setFogEndProperty(scene_.frame_.fogEnd);
}

bool SceneRenderer::Binder::Apply(GraphicsDevice& device, const CnaEngine::Graphics::RenderItem& item,
                                  CnaEngine::Graphics::RenderBucket, const CnaEngine::Graphics::Camera& camera,
                                  const CnaEngine::Graphics::FallbackSink&)
{
    const Material& material = *item.material;
    const Look& look = *static_cast<const Look*>(item.userData);
    const Matrix& view = camera.View();
    const Matrix& projection = camera.Projection();
    device.getSamplerStatesProperty()[0] = CnaEngine::Graphics::SamplerFor(material);
    switch (look.effect)
    {
        case Look::Effect::Vertex:
        {
            BasicEffect& effect = *scene_.effect_;
            effect.setWorldProperty(item.world);
            effect.setViewProperty(view);
            effect.setProjectionProperty(projection);
            effect.setLightingEnabledProperty(material.shading != ShadingModel::Unlit);
            effect.setDiffuseColorProperty(material.diffuseColor);
            effect.setAlphaProperty(material.alpha);
            effect.setTextureProperty(material.baseTexture);
            applyFog(effect,vertexEffect_,material.fog);
            applyLight(effect,vertexEffect_,look.light);
            applyFirstPass(effect);
            return true;
        }
        case Look::Effect::Material:
        case Look::Effect::Water:
        case Look::Effect::Facade:
        {
            // The water and facade shaders read their lighting, fog and texture from the
            // configured material effect.
            BasicEffect& effect = *scene_.materialEffect_;
            effect.setWorldProperty(item.world);
            effect.setViewProperty(view);
            effect.setProjectionProperty(projection);
            effect.setTextureProperty(material.baseTexture);
            effect.setDiffuseColorProperty(material.diffuseColor);
            effect.setSpecularColorProperty(material.specularColor);
            effect.setSpecularPowerProperty(material.specularPower);
            effect.DirectionalLight0.setSpecularColorProperty(look.keySpecular);
            applyFog(effect,materialEffect_,material.fog);
            applyLight(effect,materialEffect_,look.light);
            if (look.effect == Look::Effect::Water)
                scene_.waterEffect_->apply(device,effect,item.world,view,projection,
                                           scene_.frame_.serviceSeconds,scene_.frame_.waterSky,
                                           scene_.frame_.sunSparkle);
            else if (look.effect == Look::Effect::Facade)
                scene_.facadeEffect_->apply(device,effect,item.world,view,projection,
                                            scene_.facadeLightingMode_ == 2);
            else
                applyFirstPass(effect);
            return true;
        }
        case Look::Effect::Lettering:
        {
            BasicEffect& effect = *scene_.passengerDisplayEffect_;
            effect.setWorldProperty(item.world);
            effect.setViewProperty(view);
            effect.setProjectionProperty(projection);
            applyFirstPass(effect);
            return true;
        }
        case Look::Effect::Spray:
        {
            AlphaTestEffect& effect = *scene_.leafSprayEffect_;
            effect.setWorldProperty(item.world);
            effect.setViewProperty(view);
            effect.setProjectionProperty(projection);
            effect.setDiffuseColorProperty(material.diffuseColor);
            effect.setTextureProperty(material.baseTexture);
            effect.setFogEnabledProperty(material.fog);
            effect.setFogColorProperty(scene_.frame_.fogColour);
            effect.setFogStartProperty(scene_.frame_.fogStart);
            effect.setFogEndProperty(scene_.frame_.fogEnd);
            applyFirstPass(effect);
            return true;
        }
    }
    return false;
}

} // namespace TrainSim::Render
