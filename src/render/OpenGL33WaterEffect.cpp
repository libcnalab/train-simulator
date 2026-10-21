#include "trainsim/render/OpenGL33WaterEffect.hpp"
#include "trainsim/render/WaterSurface.hpp"
#include "Microsoft/Xna/Framework/Graphics/ShaderEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/BasicEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp"
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
layout(location=2) in vec2 uv;
layout(location=3) in vec4 colour;
uniform mat4 World,View,waterWVP;
uniform vec2 pixelCentre;
out vec3 worldPosition;
out vec2 texcoord;
out vec3 tint;
out float depth;
void main() {
    vec4 p=World*vec4(position,1.0);
    gl_Position=waterWVP*vec4(position,1.0);
    // Match CNA's stock XNA depth/pixel-centre convention (see facade effect).
    gl_Position.z=2.0*gl_Position.z-gl_Position.w;
    gl_Position.xy+=pixelCentre*gl_Position.w;
    worldPosition=p.xyz; depth=-(View*p).z; texcoord=uv; tint=colour.rgb;
}
)";
const std::string fragment=std::string(shaderVersion)+R"(
in vec3 worldPosition;
in vec2 texcoord;
in vec3 tint;
in float depth;
uniform mat4 World;
uniform sampler2D materialTexture;
uniform vec3 ambient,diffuse,sunDirection,fogColour,eye,wavePhases,skyColour;
uniform vec2 fogRange;
uniform float sunSparkle;
out vec4 result;
float filteredWave(float phase) {
    float footprint=fwidth(phase);
    return sin(phase)*exp(-0.5*footprint*footprint);
}
void main() {
    vec2 metres=texcoord*7.5;
    float a=dot(metres,vec2(0.46,0.17))+wavePhases.x
            +0.8*sin(dot(metres,vec2(0.09,0.07)));
    float b=dot(metres,vec2(-0.23,0.82))-wavePhases.y
            +0.6*cos(dot(metres,vec2(0.11,-0.05)));
    float c=dot(metres,vec2(1.71,0.91))+wavePhases.z
            +0.7*sin(dot(metres,vec2(0.19,0.08)));
    vec2 slope=vec2(0.052,0.019)*filteredWave(a)
              +vec2(-0.018,0.065)*filteredWave(b)
              +vec2(0.026,0.014)*filteredWave(c);
    vec3 n=normalize(mat3(World)*vec3(-slope.x,1.0,-slope.y));
    vec3 v=normalize(eye-worldPosition);
    float facing=clamp(dot(n,v),0.0,1.0);
    float fresnel=0.020+0.980*pow(1.0-facing,5.0);
    vec3 reflected=reflect(-v,n);
    // Analytic sky reflection, not a fake reflection of nearby geometry. The zenith colour
    // follows time of day and weather (ENV-138/139).
    vec3 sky=mix(fogColour,skyColour,
                 smoothstep(0.0,0.65,max(reflected.y,0.0)));
    vec3 lighting=ambient+diffuse*max(dot(n,-sunDirection),0.0);
    vec3 detail=mix(vec3(0.92),texture(materialTexture,texcoord).rgb,0.20);
    vec3 body=tint*detail*lighting;
    vec3 water=mix(body,sky,0.72*fresnel);
    vec3 h=normalize(v-sunDirection);
    float nh=max(dot(n,h),0.0);
    float sparkle=0.16*pow(nh,64.0)+0.30*pow(nh,320.0);
    water+=vec3(1.0,0.96,0.83)*sparkle*max(-sunDirection.y,0.0)*sunSparkle;
    float keep=clamp((fogRange.y-depth)/max(fogRange.y-fogRange.x,0.001),0.0,1.0);
    result=vec4(mix(fogColour,water,keep),1.0);
}
)";
}
OpenGL33WaterEffect::OpenGL33WaterEffect(GraphicsDevice& device)
    : effect_(std::make_unique<ShaderEffect>(device,vertex.c_str(),fragment.c_str())) {}
OpenGL33WaterEffect::~OpenGL33WaterEffect()=default;
bool OpenGL33WaterEffect::valid() const { return effect_->IsEffectValid(); }
void OpenGL33WaterEffect::apply(GraphicsDevice& device,BasicEffect& reference,
    const Matrix& world,const Matrix& view,const Matrix& projection,double serviceSeconds,
    const Vector3& skyColour,float sunSparkle)
{
    effect_->setWorldProperty(world); effect_->setViewProperty(view); effect_->setProjectionProperty(projection);
    effect_->Apply();
    float wvp[16]; (world*view*projection).ToColumnMajor(wvp);
    effect_->SetUniformMat4("waterWVP",wvp);
    const auto vector=[&](const char* name,const Vector3& v) { effect_->SetUniformVec3(name,v.X,v.Y,v.Z); };
    vector("ambient",reference.getAmbientLightColorProperty());
    vector("diffuse",reference.DirectionalLight0.getDiffuseColorProperty());
    vector("sunDirection",reference.DirectionalLight0.getDirectionProperty());
    vector("fogColour",reference.getFogColorProperty());
    vector("eye",Matrix::Invert(view).getTranslationProperty());
    vector("skyColour",skyColour);
    effect_->SetUniformFloat("sunSparkle",sunSparkle);
    const auto phases=waterWavePhases(serviceSeconds);
    effect_->SetUniformVec3("wavePhases",phases[0],phases[1],phases[2]);
    effect_->SetUniformVec2("fogRange",reference.getFogStartProperty(),reference.getFogEndProperty());
    const auto& viewport=device.getViewportProperty();
    const float shift=device.getPresentationParametersProperty().getMultiSampleCountProperty()>0 ? 0.0f : 63.0f/64.0f;
    effect_->SetUniformVec2("pixelCentre",shift/static_cast<float>(viewport.getWidthProperty()),
        -shift/static_cast<float>(viewport.getHeightProperty()));
    effect_->SetTexture(0,*reference.getTextureProperty());
    effect_->SetUniformInt("materialTexture",0);
}
}
