#include "trainsim/render/OpenGL33FacadeEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/ShaderEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/BasicEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp"
#include <stdexcept>
#include <string>
using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;
namespace TrainSim::Render {
namespace {
#ifdef __EMSCRIPTEN__
constexpr const char* shaderVersion=R"(#version 300 es
precision highp float;
)";
#else
constexpr const char* shaderVersion=R"(#version 330 core
)";
#endif
const std::string vertex=std::string(shaderVersion)+R"(
layout(location=0) in vec3 position;
layout(location=1) in vec3 normal;
layout(location=2) in vec2 uv;
layout(location=3) in vec4 colour;
uniform mat4 World,View,Projection;
uniform mat4 facadeWVP;
uniform vec2 pixelCentre;
out vec3 worldPosition,surfaceNormal;
out vec2 texcoord;
out vec4 tint;
out float depth;
void main() {
    vec4 p=World*vec4(position,1.0);
    vec4 v=View*p;
    gl_Position=facadeWVP*vec4(position,1.0);
    // XNA matrices use [0, w] depth; custom GLSL must map it to [-w, w].
    // Stock CNA effects perform this conversion in their shader wrapper.
    gl_Position.z=2.0*gl_Position.z-gl_Position.w;
    gl_Position.xy+=pixelCentre*gl_Position.w;
    worldPosition=p.xyz; surfaceNormal=mat3(World)*normal;
    depth=-v.z; texcoord=uv; tint=colour;
}
)";
const std::string fragment=std::string(shaderVersion)+R"(
in vec3 worldPosition,surfaceNormal;
in vec2 texcoord;
in vec4 tint;
in float depth;
uniform sampler2D materialTexture;
uniform vec3 ambient,diffuse,sunDirection,fogColour,eye,specular,sunSpecular,bounce,bounceDirection;
uniform vec4 settings; // fog start/end, shininess, linear-light mode
out vec4 result;
vec3 decode(vec3 c) { return mix(c/12.92,pow((c+0.055)/1.055,vec3(2.4)),step(vec3(0.04045),c)); }
vec3 encode(vec3 c) { return mix(c*12.92,1.055*pow(max(c,vec3(0)),vec3(1.0/2.4))-0.055,step(vec3(0.0031308),c)); }
void main() {
    vec3 n=normalize(surfaceNormal);
    float ndl=dot(n,-sunDirection);
    vec3 lighting=ambient+diffuse*max(ndl,0.0)+bounce*max(dot(n,-bounceDirection),0.0);
    vec3 h=normalize(normalize(eye-worldPosition)-sunDirection);
    vec3 highlight=specular*sunSpecular*pow(max(dot(h,n),0.0)*step(0.0,ndl),settings.z);
    vec3 albedo=texture(materialTexture,texcoord).rgb*tint.rgb;
    float keep=clamp((settings.y-depth)/max(settings.y-settings.x,0.001),0.0,1.0);
    vec3 legacy=mix(fogColour,albedo*lighting+highlight,keep);
    vec3 linear=encode(mix(decode(fogColour),decode(albedo)*lighting+highlight,keep));
    result=vec4(mix(legacy,linear,settings.w),1.0);
}
)";
}
OpenGL33FacadeEffect::OpenGL33FacadeEffect(GraphicsDevice& device)
    : effect_(std::make_unique<ShaderEffect>(device,vertex.c_str(),fragment.c_str()))
{
    if (!effect_->IsEffectValid()) throw std::runtime_error("OpenGL33 facade comparison shader failed to compile");
}
OpenGL33FacadeEffect::~OpenGL33FacadeEffect()=default;
void OpenGL33FacadeEffect::apply(GraphicsDevice& device,BasicEffect& reference,
    const Matrix& world,const Matrix& view,const Matrix& projection,bool linear)
{
    effect_->setWorldProperty(world); effect_->setViewProperty(view); effect_->setProjectionProperty(projection);
    effect_->Apply();
    float wvp[16];
    (world*view*projection).ToColumnMajor(wvp);
    effect_->SetUniformMat4("facadeWVP",wvp);
    const auto vector=[&](const char* name,const Vector3& v) { effect_->SetUniformVec3(name,v.X,v.Y,v.Z); };
    vector("ambient",reference.getAmbientLightColorProperty());
    vector("diffuse",reference.DirectionalLight0.getDiffuseColorProperty());
    vector("sunDirection",reference.DirectionalLight0.getDirectionProperty());
    vector("fogColour",reference.getFogColorProperty());
    vector("specular",reference.getSpecularColorProperty());
    vector("sunSpecular",reference.DirectionalLight0.getSpecularColorProperty());
    const bool bounceEnabled=reference.DirectionalLight1.getEnabledProperty();
    vector("bounce",bounceEnabled ? reference.DirectionalLight1.getDiffuseColorProperty() : Vector3::Zero);
    vector("bounceDirection",bounceEnabled ? reference.DirectionalLight1.getDirectionProperty()
                                           : Vector3{0,1,0});
    vector("eye",Matrix::Invert(view).getTranslationProperty());
    effect_->SetUniformVec4("settings",reference.getFogStartProperty(),reference.getFogEndProperty(),
        reference.getSpecularPowerProperty(),linear ? 1.0f : 0.0f);
    const auto& viewport=device.getViewportProperty();
    const float shift=device.getPresentationParametersProperty().getMultiSampleCountProperty()>0 ? 0.0f : 63.0f/64.0f;
    effect_->SetUniformVec2("pixelCentre",shift/static_cast<float>(viewport.getWidthProperty()),
        -shift/static_cast<float>(viewport.getHeightProperty()));
    effect_->SetTexture(0,*reference.getTextureProperty());
    effect_->SetUniformInt("materialTexture",0);
}
}
