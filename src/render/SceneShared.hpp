#pragma once

// Internal to the renderer implementation files: shared includes, scene helpers and the
// private renderer state structs. Not part of the public include tree.

#include "trainsim/render/SceneRenderer.hpp"

#include "trainsim/render/AvatarMotion.hpp"
#include "trainsim/render/BridgeGeometry.hpp"
#include "trainsim/render/TunnelGeometry.hpp"
#include "trainsim/render/TrackGeometry.hpp"
#include "trainsim/render/LinesideGeometry.hpp"
#include "trainsim/render/SignalGeometry.hpp"
#include "trainsim/render/InteriorFittingsGeometry.hpp"
#include "trainsim/render/ToiletGeometry.hpp"
#include "trainsim/render/TrainExteriorGeometry.hpp"
#include "trainsim/render/TerminalGeometry.hpp"
#include "trainsim/render/OtherTrainGeometry.hpp"
#include "trainsim/render/NightLighting.hpp"
#include "trainsim/render/WeatherGeometry.hpp"
#include "trainsim/render/AvatarAppearance.hpp"
#include "trainsim/render/AvatarProportions.hpp"
#include "trainsim/render/PlatformLayout.hpp"
#include "trainsim/render/VegetationGeometry.hpp"
#include "trainsim/render/TerrainPatchGeometry.hpp"
#include "trainsim/render/SkyGeometry.hpp"
#include "trainsim/render/FacadeGeometry.hpp"
#include "trainsim/render/StreetPocketGeometry.hpp"
#include "trainsim/render/RoadVehicleGeometry.hpp"
#include "trainsim/world/StreetLayout.hpp"
#include "trainsim/render/GroundShadowGeometry.hpp"
#include "trainsim/render/StationGeometry.hpp"
#include "trainsim/render/ForecourtGeometry.hpp"
#include "trainsim/render/RoutePointCache.hpp"
#include "trainsim/render/LandscapeVariation.hpp"
#include "trainsim/render/ViewCulling.hpp"
#include "trainsim/render/InteriorGeometry.hpp"
#include "trainsim/render/GpuMesh.hpp"
#include "trainsim/render/OpenGL33FacadeEffect.hpp"
#include "trainsim/render/OpenGL33WaterEffect.hpp"
#include "trainsim/render/ShoreGeometry.hpp"
#include "trainsim/render/VergeGeometry.hpp"
#include <span>
#include <limits>
#include "trainsim/render/Daylight.hpp"
#include "trainsim/render/MeshBuilder.hpp"
#include "trainsim/render/PixelGlyphs.hpp"
#include "trainsim/render/PassengerDisplayText.hpp"
#include "trainsim/render/PassengerDoorGeometry.hpp"
#include "trainsim/render/CarRoleGeometry.hpp"
#include "trainsim/render/ProceduralMaterials.hpp"
#include "trainsim/sim/TrainInterior.hpp"

#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/Graphics/BasicEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/AlphaTestEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/BlendState.hpp"
#include "Microsoft/Xna/Framework/Graphics/DepthStencilState.hpp"
#include "Microsoft/Xna/Framework/Graphics/EffectPass.hpp"
#include "Microsoft/Xna/Framework/Graphics/EffectTechnique.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp"
#include "Microsoft/Xna/Framework/Graphics/RasterizerState.hpp"
#include "Microsoft/Xna/Framework/Graphics/SamplerState.hpp"
#include "Microsoft/Xna/Framework/Graphics/Texture2D.hpp"
#include "Microsoft/Xna/Framework/GamerServices/AvatarAnimation.hpp"
#include "Microsoft/Xna/Framework/GamerServices/AvatarAnimationPreset.hpp"
#include "Microsoft/Xna/Framework/GamerServices/AvatarBone.hpp"
#include "Microsoft/Xna/Framework/GamerServices/AvatarBodyType.hpp"
#include "Microsoft/Xna/Framework/GamerServices/AvatarDescription.hpp"
#include "Microsoft/Xna/Framework/GamerServices/AvatarRenderer.hpp"
#include "Microsoft/Xna/Framework/Vector3.hpp"
#include "System/TimeSpan.hpp"

#include <algorithm>
#include <cctype>
#include <array>
#include <cstddef>
#include <cmath>
#include <chrono>
#include <optional>
#include <future>
#include <string_view>
#include <utility>


namespace TrainSim::Render {

namespace scene_detail {
using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;
using namespace Microsoft::Xna::Framework::GamerServices;
constexpr float kCarLength = 21.0f;
constexpr float kFirstCarCentre = -52.5f;
constexpr float kPi = 3.14159265358979323846f;
constexpr float kTrainFloorElevation =
    static_cast<float>(Sim::TrainInterior::FloorElevationAboveRail);
constexpr float kPlatformSurfaceElevation =
    static_cast<float>(Sim::TrainInterior::PlatformSurfaceAboveRail);
constexpr std::size_t kOnboardAvatarVisuals = 12;
constexpr std::size_t kPlatformAvatarVisuals = 12;
// Onboard seated passengers sit with the avatar root this far below the cushion top.
constexpr float kPlatformSeatedRootDrop = 0.52f;
constexpr std::size_t kTotalAvatarVisuals = kOnboardAvatarVisuals + kPlatformAvatarVisuals;
// CNA reports Ready before the first Draw() performs EasyGL resource upload. Leaving several
// complete presentation intervals between that upload and the next background model build avoids
// driver/dependency work leaking across the boundary on slower or heavily loaded desktops.
constexpr int kAvatarUploadQuiescenceFrames = 8;

inline std::size_t onboardAvatarBudget(int graphicsQuality) noexcept
{
    return graphicsQuality <= 0 ? 6U : graphicsQuality == 1 ? 9U : kOnboardAvatarVisuals;
}

inline std::size_t platformAvatarBudget(int graphicsQuality) noexcept
{
    return graphicsQuality <= 0 ? 2U : graphicsQuality == 1 ? 6U : kPlatformAvatarVisuals;
}

inline std::size_t totalAvatarBudget(int graphicsQuality) noexcept
{
    return onboardAvatarBudget(graphicsQuality) + platformAvatarBudget(graphicsQuality);
}

inline bool avatarUsesNativeCap(const AvatarDescription& description)
{
    // CNA descriptions are opaque to games, but CNA's own documented format keeps the optional
    // little-endian hat item at bytes 41-42; catalog v3 assigns 100 to its covered-hair cap. We
    // only inspect public CreateRandom() results here;
    // the description is never modified or reconstructed, so AvatarRenderer remains authoritative.
    const auto bytes = description.getDescriptionProperty();
    return bytes.size() > 42U && bytes[41] == 100 && bytes[42] == 0;
}

inline int civilianAppearanceScore(const AvatarDescription& description)
{
    return civilianAppearanceScoreBytes(description.getDescriptionProperty());
}

inline int staffAppearanceScore(const AvatarDescription& description)
{
    const auto bytes = description.getDescriptionProperty();
    if (bytes.size() <= 42U) return 0;
    const bool headwear = avatarUsesNativeCap(description);
    const auto isServiceBlue = [&bytes](std::size_t offset) {
        return bytes[offset] == 60 && bytes[offset+1] == 80 && bytes[offset+2] == 180;
    };
    return 4 * static_cast<int>(headwear)
         + static_cast<int>(isServiceBlue(19U))
         + static_cast<int>(isServiceBlue(28U));
}

inline Vector3 rgb(const Color& color)
{
    return Vector3(static_cast<float>(color.getRProperty()) / 255.0f,
                   static_cast<float>(color.getGProperty()) / 255.0f,
                   static_cast<float>(color.getBProperty()) / 255.0f);
}

inline Matrix boxMatrix(float x, float y, float z, float sx, float sy, float sz)
{
    return Matrix::CreateScale(sx, sy, sz) * Matrix::CreateTranslation(x, y, z);
}

inline Matrix orientedBoxMatrix(float x, float y, float z, float sx, float sy, float sz, float yaw)
{
    return Matrix::CreateScale(sx, sy, sz)
         * Matrix::CreateRotationY(yaw)
         * Matrix::CreateTranslation(x, y, z);
}

inline LocalRoutePoint localRoutePoint(const Sim::Route& route, double referenceDistance,
                                double sampleDistance, double lateral = 0.0)
{
    const Sim::RelativeRoutePose pose = route.relativePose(referenceDistance, sampleDistance);
    const double cosine = std::cos(pose.headingRadians);
    const double sine = std::sin(pose.headingRadians);
    return LocalRoutePoint{
        static_cast<float>(pose.x + lateral * cosine),
        static_cast<float>(pose.elevation),
        static_cast<float>(pose.z - lateral * sine),
        static_cast<float>(pose.headingRadians),
    };
}

inline std::optional<double> stationSidingLateral(const Sim::Route& route, double distance)
{
    for (const Sim::Station& station : route.stations())
    {
        if (!station.major) continue;
        const double separation = std::abs(distance - station.distanceMetres);
        if (separation >= 650.0) continue;
        const double blend = std::clamp((650.0 - separation) / 250.0, 0.0, 1.0);
        const double eased = blend * blend * (3.0 - 2.0 * blend);
        const double target = station.platformOnLeft ? 6.4 : -13.5;
        return target * eased;
    }
    return std::nullopt;
}

inline void addOrientedBox(MeshBuilder& mesh, float x, float y, float z,
                    float sx, float sy, float sz, float yaw, const Color& color)
{
    mesh.addBox(Vector3{-0.5f,-0.5f,-0.5f}, Vector3{0.5f,0.5f,0.5f}, color,
                orientedBoxMatrix(x, y, z, sx, sy, sz, yaw));
}

inline int addLayeredConifer(MeshBuilder& mesh, const Vector3& ground, float scale, int sides,
                      std::uint32_t variant, const Color& foliage,
                      const Color& foliageLight, bool closeDetail,MeshBuilder* sprays=nullptr)
{
    return addConiferCrown(mesh,ground,scale,sides,variant,foliage,foliageLight,closeDetail,sprays);
}

inline void addOrientedGroundQuad(MeshBuilder& mesh, float x, float y, float z,
                           float width, float length, float yaw, const Color& color)
{
    const Matrix transform = Matrix::CreateRotationY(yaw)
                           * Matrix::CreateTranslation(x,y,z);
    const float halfWidth = width*0.5f;
    const float halfLength = length*0.5f;
    mesh.addQuad(
        Vector3::Transform(Vector3{-halfWidth,0.0f,-halfLength},transform),
        Vector3::Transform(Vector3{-halfWidth,0.0f, halfLength},transform),
        Vector3::Transform(Vector3{ halfWidth,0.0f, halfLength},transform),
        Vector3::Transform(Vector3{ halfWidth,0.0f,-halfLength},transform),color);
}

inline void addOrientedTexturedGroundQuad(MeshBuilder& mesh, float x, float y, float z,
                                   float width, float length, float yaw, float tileMetres,
                                   const Color& color)
{
    const Matrix transform = Matrix::CreateRotationY(yaw)
                           * Matrix::CreateTranslation(x,y,z);
    const float halfWidth = width*0.5f;
    const float halfLength = length*0.5f;
    mesh.addTexturedQuad(
        Vector3::Transform(Vector3{-halfWidth,0.0f,-halfLength},transform),
        Vector3::Transform(Vector3{-halfWidth,0.0f, halfLength},transform),
        Vector3::Transform(Vector3{ halfWidth,0.0f, halfLength},transform),
        Vector3::Transform(Vector3{ halfWidth,0.0f,-halfLength},transform),color,
        Vector2{0.0f,0.0f},Vector2{0.0f,length/tileMetres},
        Vector2{width/tileMetres,length/tileMetres},Vector2{width/tileMetres,0.0f});
}

inline float terrainOffset(const Sim::RoutePose& pose, double distance, double lateral)
{
    const double corridorBlend = std::clamp((std::abs(lateral) - 12.0) / 150.0, 0.0, 1.0);
    if (pose.waterCrossing)
    {
        // The estuary bed remains safely below the water near the railway. Beyond roughly 100 m,
        // a smooth deterministic bank rises above the water plane instead of leaving an endless
        // blue horizon. Existing wide terrain bands interpolate the transition cheaply.
        const double shore = std::clamp((riverShoreCoordinate(distance,lateral) - 90.0) / 140.0, 0.0, 1.0);
        const double easedShore = shore * shore * (3.0 - 2.0 * shore);
        const double distantRelief = 2.2 * std::sin(distance / 1'350.0 + lateral / 210.0);
        return static_cast<float>(-11.0 + easedShore * (14.0 + distantRelief));
    }
    const double relief = 7.0
                        + 8.5 * std::sin(distance / 1'900.0 + lateral / 83.0)
                        + 5.0 * std::sin(distance / 720.0 - lateral / 145.0);
    const double ridgeBlend = std::clamp((std::abs(lateral) - 230.0) / 760.0, 0.0, 1.0);
    const double ridge = ridgeBlend
        * (26.0 + 18.0 * std::sin(distance / 4'200.0 + lateral / 390.0));
    const double urbanScale = 1.0 - pose.urbanity * 0.72;
    double naturalTerrain = -0.34 + corridorBlend * (relief + ridge) * urbanScale;
    // Urban streets and both pavements occupy the 12--24.7 m lateral band. Keep their
    // formation below the road surface, then ease back into the surrounding terrain.
    // Explicit mesh samples at 26/40 m below preserve this flat corridor after tessellation.
    if (pose.urbanity > 0.28 && !pose.tunnel)
    {
        const double blend = std::clamp((std::abs(lateral)-26.0)/14.0,0.0,1.0);
        const double eased = blend*blend*(3.0-2.0*blend);
        naturalTerrain = -0.34+(naturalTerrain+0.34)*eased;
    }
    if (!pose.tunnel && pose.urbanity < 0.55 && pose.adjacentWaterSide == 0)
    {
        // A shallow drain begins outside the railway fence rather than cutting into either
        // ballast shoulder. Explicit terrain-band samples below give this depression actual
        // banks; it is not a dark stripe painted over an otherwise flat field.
        const double distanceFromCentre = std::abs(lateral);
        if (distanceFromCentre > 8.8 && distanceFromCentre < 12.0)
        {
            const double acrossDrain = (distanceFromCentre-8.8)/3.2;
            const double bowl = std::sin(acrossDrain*3.14159265358979323846);
            naturalTerrain -= 0.34*bowl*bowl;
        }
    }
    if (pose.adjacentWaterSide != 0)
    {
        const double waterwardLateral = lateral
            * static_cast<double>(pose.adjacentWaterSide);
        return static_cast<float>(lakeShoreTerrainOffset(
            naturalTerrain+lakeFarBankRelief(distance,waterwardLateral),
            lakeShoreCoordinate(distance,waterwardLateral),
            pose.adjacentWaterSurfaceElevation-pose.elevation));
    }
    // The second authored crossing is a dry viaduct over a valley running across the line.
    return static_cast<float>(viaductValleyTerrain(distance,lateral,naturalTerrain));
}

inline Color roadVehicleColor(std::uint32_t variant)
{
    static const std::array<Color, 6> colors = {
        Color(186,51,45,255), Color(44,92,142,255), Color(225,220,204,255),
        Color(57,61,63,255), Color(205,145,45,255), Color(79,126,80,255)};
    return colors[variant % colors.size()];
}

inline AvatarAnimationPreset avatarPreset(std::size_t index)
{
    constexpr std::array<AvatarAnimationPreset, 8> presets = {
        AvatarAnimationPreset::Stand0, AvatarAnimationPreset::Stand1,
        AvatarAnimationPreset::FemaleIdleLookAround, AvatarAnimationPreset::MaleIdleLookAround,
        AvatarAnimationPreset::Stand2, AvatarAnimationPreset::FemaleIdleShiftWeight,
        AvatarAnimationPreset::MaleIdleShiftWeight, AvatarAnimationPreset::Stand5,
    };
    return presets[index % presets.size()];
}

inline Matrix rotatedBone(const Matrix& local, const Matrix& rotation)
{
    return rotation * Matrix::CreateTranslation(local.getTranslationProperty());
}

inline void addRouteSignText(MeshBuilder& mesh, const Sim::Route& route, double anchorDistance,
                      std::string_view text, double centreDistance, double lateral,
                      float baselineY, const Color& color, float maximumWidth = 3.2f)
{
    if (text.empty()) return;
    const float pixel = std::min(0.034f,
        maximumWidth / (static_cast<float>(text.size()) * 6.0f));
    const double width = static_cast<double>(text.size()) * 6.0 * pixel;
    for (std::size_t character = 0; character < text.size(); ++character)
    {
        const PixelGlyphRows rows = pixelGlyphRows(text[character]);
        for (int row = 0; row < 7; ++row)
            for (int column = 0; column < 5; ++column)
            {
                if ((rows[static_cast<std::size_t>(row)] & (0x10U >> column)) == 0) continue;
                const double unmirroredAlong = -width * 0.5
                    + (static_cast<double>(character) * 6.0 + column) * pixel;
                // Every platform face points inward toward the train. Reversing the along-track
                // glyph order on the negative-lateral side keeps station names and live boards
                // readable from either side instead of exposing mirrored text through the window.
                const double along = lateral < 0.0
                    ? -unmirroredAlong - pixel
                    : unmirroredAlong;
                const LocalRoutePoint point = localRoutePoint(
                    route, anchorDistance, centreDistance + along, lateral);
                addOrientedBox(mesh,point.x,
                               point.y+baselineY-static_cast<float>(row)*pixel,
                               point.z,0.045f,pixel*0.82f,pixel*0.82f,point.yaw,color);
            }
    }
}

inline void addRoutePrintedText(MeshBuilder& mesh,const Sim::Route& route,double anchorDistance,
    std::string_view text,double distance,double lateral,float baselineY,
    const Color& colour,float maximumWidth=3.2f)
{
    const auto point=localRoutePoint(route,anchorDistance,distance,lateral);
    const float side=lateral<0 ? -1.0f : 1.0f;
    addPrintedSignText(mesh,text,maximumWidth,
        Matrix::CreateRotationY(point.yaw-side*1.570796327f)
            *Matrix::CreateTranslation(point.x,point.y+baselineY,point.z),colour);
}

inline void addTrackFacingText(MeshBuilder& mesh, const Sim::Route& route, double anchorDistance,
                        std::string_view text, double centreDistance, double lateral,
                        float baselineY, int direction, const Color& color,
                        float pixel = 0.065f)
{
    const double width = static_cast<double>(text.size()) * 6.0 * pixel;
    for (std::size_t character = 0; character < text.size(); ++character)
    {
        const PixelGlyphRows rows = pixelGlyphRows(text[character]);
        for (int row = 0; row < 7; ++row)
            for (int column = 0; column < 5; ++column)
            {
                if ((rows[static_cast<std::size_t>(row)] & (0x10U >> column)) == 0) continue;
                const double horizontal = -width * 0.5
                    + (static_cast<double>(character) * 6.0 + column) * pixel;
                const LocalRoutePoint point = localRoutePoint(
                    route, anchorDistance, centreDistance,
                    lateral + horizontal * static_cast<double>(direction));
                addOrientedBox(mesh,point.x,
                               point.y+baselineY-static_cast<float>(row)*pixel,
                               point.z,pixel*0.82f,pixel*0.82f,0.045f,
                               point.yaw,color);
            }
    }
}
} // namespace scene_detail
using namespace scene_detail;

struct SceneRenderer::AvatarVisual
{
    std::unique_ptr<AvatarDescription> description;
    std::unique_ptr<AvatarRenderer> renderer;
    std::unique_ptr<AvatarAnimation> animation;
    std::vector<Matrix> boneScratch;
    std::array<int,71> parentBones{};
    float seatedPoseBlend = 0.0f;
    float walkingPoseBlend = 0.0f;
    float facingYaw = 0.0f;
    float gazeYaw = 0.0f;
    float gazePitch = 0.0f;
    float gaitPhaseRadians = 0.0f;
    double previousPassengerX = 0.0;
    double previousPassengerZ = 0.0;
    bool motionPositionInitialized = false;
    bool facingInitialized = false;
    bool gpuPrimed = false;
};

struct SceneRenderer::WorldStreamingState
{
    // One CPU window and two GPU windows, regardless of journey length. A future owns
    // the CPU window during generation; only the render thread ever owns GPU objects.
    std::unique_ptr<WorldGeometry> geometry = std::make_unique<WorldGeometry>();
    std::future<std::unique_ptr<WorldGeometry>> worker;
    std::array<std::unique_ptr<GpuMesh>,kWorldMaterialCount> staged;
    std::int64_t target = -1;
    double anchor = 0;
    int quality = -1;
    float vegetation = 0;
    std::size_t uploaded = 0;
    std::size_t uploadedSegment = 0;
    bool generationRecorded = false;
    std::uint64_t prefetchHits = 0;
    std::uint64_t synchronousBuilds = 0;
    std::uint64_t latePrefetches = 0;
};

struct SceneRenderer::TrackStreamingState
{
    // One CPU batch and two GPU batches, mirroring the world window: a future owns the CPU
    // batch while it is generated; only the render thread creates or swaps GPU buffers.
    std::unique_ptr<TrackGeometry> geometry = std::make_unique<TrackGeometry>();
    std::future<std::unique_ptr<TrackGeometry>> worker;
    std::array<std::unique_ptr<GpuMesh>,kTrackMaterialCount> staged;
    double target = -1;
    int quality = -1;
    std::size_t uploaded = 0;
    std::size_t uploadedSegment = 0;
    std::uint64_t prefetchHits = 0;
    std::uint64_t synchronousBuilds = 0;
    std::uint64_t latePrefetches = 0;
};

// cna-engine's renderer owns the order and the blend, depth, culling and sampler states of the
// queued draws; this binder configures the scene's own effects for each of them, as the
// hand-written passes did (SceneBinder.cpp).
class SceneRenderer::Binder final : public CnaEngine::Graphics::IMaterialBinder
{
public:
    explicit Binder(SceneRenderer& scene) : scene_(scene) {}
    void BeginFrame(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device,
                    const CnaEngine::Graphics::Camera& camera,
                    const CnaEngine::Graphics::ForwardLighting& lighting) override;
    bool Apply(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device,
               const CnaEngine::Graphics::RenderItem& item, CnaEngine::Graphics::RenderBucket bucket,
               const CnaEngine::Graphics::Camera& camera,
               const CnaEngine::Graphics::FallbackSink& fallback) override;

private:
    // The light and fog are the frame's and change only with the light, so each of the two
    // lit effects is set up again only when its light changes within one Render(); other code
    // (the sky, the people) changes them between Render() calls.
    struct Configured
    {
        std::optional<Look::Light> light;
        std::optional<bool> fog;
    };
    void applyLight(Microsoft::Xna::Framework::Graphics::BasicEffect& effect, Configured& configured,
                    Look::Light light) const;
    void applyFog(Microsoft::Xna::Framework::Graphics::BasicEffect& effect, Configured& configured,
                  bool enabled) const;
    SceneRenderer& scene_;
    Configured vertexEffect_;
    Configured materialEffect_;
};

} // namespace TrainSim::Render
