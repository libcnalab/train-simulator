#pragma once

#include "trainsim/sim/PassengerSystem.hpp"
#include "trainsim/sim/Route.hpp"
#include "trainsim/sim/RouteSimulation.hpp"
#include "trainsim/sim/TrainInterior.hpp"
#include "trainsim/world/OtherTrainConsist.hpp"
#include "trainsim/world/StreamingWorld.hpp"
#include "trainsim/render/Atmosphere.hpp"
#include "trainsim/render/WorldGeometry.hpp"

#include "CnaEngine/Graphics/Camera.hpp"
#include "CnaEngine/Graphics/ForwardRenderer.hpp"
#include "CnaEngine/Graphics/Material.hpp"
#include "CnaEngine/Graphics/RenderQueue.hpp"

#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/Matrix.hpp"

#include <array>
#include <cstdint>
#include <deque>
#include <memory>
#include <optional>
#include <vector>

namespace Microsoft::Xna::Framework::Graphics {
class BasicEffect;
class AlphaTestEffect;
class GraphicsDevice;
class Texture2D;
}
namespace Microsoft::Xna::Framework::GamerServices {
class AvatarAnimation;
class AvatarDescription;
class AvatarRenderer;
}

namespace TrainSim::Render {

class GpuMesh;
class MeshBuilder;
class OpenGL33FacadeEffect;
class OpenGL33WaterEffect;

struct RenderStats
{
    int facadeShaderDraws = 0;
    int waterShaderDraws = 0;
    int culledOnboardAvatars = 0;
    std::uint64_t worldPrefetchHits = 0;
    std::uint64_t worldSynchronousBuilds = 0;
    std::uint64_t worldLatePrefetches = 0;
    std::uint64_t worldRebuilds = 0;
    std::uint64_t worldBufferReuses = 0;
    std::uint64_t routePointCacheHits = 0;
    std::uint64_t routePointCacheMisses = 0;
    double worldRebuildTotalMs = 0.0;
    double worldRebuildMaxMs = 0.0;
    double worldGenerationTotalMs = 0.0;
    double worldGenerationMaxMs = 0.0;
    double worldUploadTotalMs = 0.0;
    double worldUploadMaxMs = 0.0;
    int drawCalls = 0;
    int triangles = 0;
    int visiblePassengers = 0;
    int animatedGazeAvatars = 0;
    int readingIdleAvatars = 0;
    int phoneCheckingAvatars = 0;
    int onboardSocialAvatars = 0;
    int distanceDrivenGaitAvatars = 0;
    int seatTransitionAvatars = 0;
    int playerAcknowledgingAvatars = 0;
    int conductorHandheldDevices = 0;
    int platformPhoneUsers = 0;
    int platformSocialAvatars = 0;
    int skyCloudBillboards = 0;
    int avatarVisuals = 0;
    int readyAvatarVisuals = 0;
    int civilianAvatarDescriptions = 0;
    int redSignals = 0;
    int yellowSignals = 0;
    int greenSignals = 0;
    int platformInformationBoards = 0;
    int platformSurfaceJoints = 0;
    int platformCanopyLights = 0;
    int platformCanopyDrainageAssemblies = 0;
    int platformPlanters = 0;
    int platformPosterCases = 0;
    int platformEquipmentCabinets = 0;
    int platformBenchAssemblies = 0;
    int platformWeatheringMarks = 0;
    int stationFacadeWeatheringStreaks = 0;
    int platformBoundarySections = 0;
    int tracksideDrainageSections = 0;
    int tracksideVergeTufts = 0;
    int fieldBoundarySegments = 0;
    int distantWoodlandTrees = 0;
    int distantRidgeMasses = 0;
    int treeBranchCues = 0;
    int coniferBranchTiers = 0;
    int understoreyGroups = 0;
    int distantSettlementBuildings = 0;
    int buildingEndWallWindows = 0;
    int towerEndWallWindows = 0;
    int detailedWarehouses = 0;
    int industrialCompounds = 0;
    int industrialYardVehicles = 0;
    int industrialTrailers = 0;
    int industrialUtilityGroups = 0;
    int industrialEdgePlantingGroups = 0;
    int urbanRoadEdgeMarkingSections = 0;
    int urbanRoadDrainGrates = 0;
    int urbanResidentialPlotSurfaces = 0;
    int urbanStreetPockets = 0;
    int urbanResidentialEntrancePaths = 0;
    int platformLuggageItems = 0;
    int deployedBoardingSteps = 0;
    int lakesideWaterPatches = 0;
    int lakesideBankTrees = 0;
    int hayBaleStacks = 0;
    int riverWaterSections = 0;
    int riverBankClumps = 0;
    int stationWaterfrontSections = 0;
    int tracksideKilometrePosts = 0;
    int catenaryDroppers = 0;
    int catenaryFoundations = 0;
    int catenaryMessengerSegments = 0;
    int catenaryRegistrationAssemblies = 0;
    int linesideEquipmentCabinets = 0;
    int trackFastenerAssemblies = 0;
    int turnoutPointMotors = 0;
    int turnoutBladeAssemblies = 0;
    int otherTrainCars = 0;
    int otherTrainCabEnds = 0;
    int otherTrainLocomotives = 0;
    int otherTrainFreightWagons = 0;
    int roadVehicles = 0;
    int tunnelLuminaires = 0;
    int tunnelSafetyNiches = 0;
    int tunnelLiningRings = 0;
    int tunnelMarkerPlates = 0;
    int bridgeSections = 0;
    int riverBridgeSections = 0;
    int viaductSections = 0;
    int bridgeAbutments = 0;
    int terminalBufferStops = 0;
    int terminalShedColumns = 0;
    int bridgeParapetPosts = 0;
    int fencePosts = 0;
    int cableTroughUnits = 0;
    int vergeFlowerClumps = 0;
    int palisadePosts = 0;
    int houseOutbuildings = 0;
    int houseRearWings = 0;
    int blockPenthouses = 0;
    int platformAmenities = 0;
    int platformSeatedAvatars = 0;
    int platformSuitcaseTravellers = 0;
    int blockBalconies = 0;
    int canopyValanceBoards = 0;
    int tramlineSegments = 0;
    double worldGpuMegabytes = 0;
    double largestWorldMaterialMegabytes = 0;
    double largestWorldMaterialVertices = 0;
    int largestWorldMaterial = -1;
    int worldGpuSegments = 0;
    std::uint64_t trackRebuilds = 0;
    std::uint64_t trackPrefetchHits = 0;
    std::uint64_t trackSynchronousBuilds = 0;
    std::uint64_t trackLatePrefetches = 0;
    double trackGenerationMaxMs = 0;
    double trackRebuildTotalMs = 0;
    double trackRebuildMaxMs = 0;
    int fieldBoundaryTrees = 0;
    int stationClockHands = 0;
    int levelCrossingBarriers = 0;
    int levelCrossingWarningLights = 0;
    int openGangwayDoors = 0;
    int occupiedToiletIndicators = 0;
    int paSpeakerGrilles = 0;
    int hvacVents = 0;
    int carIdentitySigns = 0;
    int carFunctionFixtureGroups = 0;
    int cabControlGroups = 0;
    int activeCabDisplays = 0;
    int vestibulePortalDetailGroups = 0;
    int overheadLuggageItems = 0;
    int floorLuggageCases = 0;
    int floorLuggageWheelPairs = 0;
    int roundedSeatCushions = 0;
    int roundedSeatBacks = 0;
    int seatPedestals = 0;
    int seatMountFasteners = 0;
    int roundedAisleArmrests = 0;
    int roundedTableTops = 0;
    int tableSupportPlates = 0;
    int windowReflectionAccents = 0;
    int interiorLightGradientQuads = 0;
    int sunlightPatches = 0;
    int aisleRunnerInlays = 0;
    int nonCarpetedFloorZones = 0;
    int interiorCovingSections = 0;
    int ceilingPanelSeams = 0;
    int vestibuleWearMarks = 0;
    int terrainMaterialTriangles = 0;
    int foliageMaterialTriangles = 0;
    int ballastMaterialTriangles = 0;
    int railHeadMaterialTriangles = 0;
    int interiorMaterialTriangles = 0;
    int interiorPanelMaterialTriangles = 0;
    int interiorResilientMaterialTriangles = 0;
    int waterMaterialTriangles = 0;
    int facadeMaterialTriangles = 0;
    int renderedFacadeMaterialTriangles = 0;
    int staticContactShadowTriangles = 0;
    int platformMaterialTriangles = 0;
    int interiorContactShadowTriangles = 0;
    // Time of day and weather (ENV-138/139).
    int clockMinutes = 0;
    float atmosphereNight = 0.0f;
    float atmosphereRain = 0.0f;
    float atmosphereOvercast = 0.0f;
    float sunElevationDegrees = 0.0f;
    int skyRebuilds = 0;
    int rainStreakDraws = 0;
    int rainDropletTriangles = 0;
    int nightGlowTriangles = 0;
    int otherTrainNightOverlays = 0;
    int cloudLayers = 0;
};

class SceneRenderer
{
public:
    explicit SceneRenderer(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device,
                           float vegetationDistanceMetres = 750.0f,
                           int graphicsQuality = 2,
                           bool passengerDoorsInitiallyOpen = false,
                           bool avatarRenderingEnabled = true);
    ~SceneRenderer();
    SceneRenderer(const SceneRenderer&) = delete;
    SceneRenderer& operator=(const SceneRenderer&) = delete;
    void setVegetationDistance(float metres);
    void setGraphicsQuality(int quality);
    void setFacadeLightingMode(int mode) { facadeLightingMode_=mode; }
    // Service-day start on the station clocks and the weather mode (ENV-138/139).
    void setEnvironment(int serviceStartMinutes, Sim::WeatherMode weather) noexcept
    {
        serviceStartMinutes_ = serviceStartMinutes;
        weatherMode_ = weather;
    }
    [[nodiscard]] bool avatarLoadingComplete() const noexcept;

    RenderStats draw(const Microsoft::Xna::Framework::Matrix& view,
                     const Microsoft::Xna::Framework::Matrix& projection,
                     const Sim::Route& route, const Sim::ServiceState& service,
                     const World::StreamingWorld& world, const Sim::PassengerSystem& passengers,
                     double seconds, double etaSeconds, double scheduleDeviationSeconds,
                     bool toiletDoorOpen, bool toiletOccupied,
                     float playerX, float playerZ, float cameraYaw);

private:
    // How one queued draw sets up the scene's effects (its RenderItem's userData); the engine
    // Material carries the rest: blend, culling, sampler, texture, colour, alpha, lighting on/off,
    // specular and fog.
    struct Look
    {
        // Vertex: vertex-coloured batches on effect_; Material: detail-textured materials on
        // materialEffect_; Lettering: the dot-matrix font; Spray: alpha-tested foliage cards;
        // Water and Facade: the OpenGL 3.3 shaders, configured from materialEffect_.
        enum class Effect : std::uint8_t { Vertex, Material, Lettering, Spray, Water, Facade };
        // Outdoor: sun, ground bounce and sky uplight; Saloon: the cars' ceiling strips;
        // Floodlit: platform lamps after dark with the outdoor bounce and uplight.
        enum class Light : std::uint8_t { Outdoor, Saloon, Floodlit };
        Effect effect = Effect::Vertex;
        Light light = Light::Outdoor;
        // Material, Water and Facade: the sun's specular colour on this material.
        Microsoft::Xna::Framework::Vector3 keySpecular{0.0f,0.0f,0.0f};
    };
    // Lighting and time shared by this frame's queued draws.
    struct FrameLighting
    {
        Microsoft::Xna::Framework::Vector3 daylight{0.0f,-1.0f,0.0f};
        Microsoft::Xna::Framework::Vector3 bounceDirection{0.0f,1.0f,0.0f};
        Microsoft::Xna::Framework::Vector3 uplightDirection{0.0f,1.0f,0.0f};
        ExteriorLighting outdoor{};
        ExteriorLighting floodlit{};
        Microsoft::Xna::Framework::Vector3 fogColour{0.0f,0.0f,0.0f};
        float fogStart = 0.0f;
        float fogEnd = 1.0f;
        Microsoft::Xna::Framework::Vector3 waterSky{0.0f,0.0f,0.0f};
        float sunSparkle = 0.0f;
        double serviceSeconds = 0.0;
    };
    class Binder;
    // Queues every segment of `mesh` in the sequenced bucket and counts one draw, as the
    // immediate drawMesh does. The material and look must live until render().
    void queue(const GpuMesh& mesh, const Microsoft::Xna::Framework::Matrix& world,
               const CnaEngine::Graphics::Material& material, const Look& look, RenderStats& stats);
    // Storage for this frame's materials and looks, kept until the next frame starts.
    CnaEngine::Graphics::Material& frameMaterial();
    const Look& frameLook(const Look& look);
    // Draws the queued items in submission order and empties the queue.
    void render();

    void buildInterior();
    void updateTrack(const Sim::Route& route, const Sim::ServiceState& service);
    void updateStaticWorld(const Sim::Route& route, const World::StreamingWorld& world,
                           const Sim::ServiceState& service);
    void rebuildInformationDisplays(const Sim::Route& route, const Sim::ServiceState& service,
                                    int speedKilometresPerHour, int etaMinutes,
                                    int informationPage);
    void rebuildPlatformInformation(const Sim::Route& route,
                                    const Sim::ServiceState& service,
                                    std::size_t stationIndex, int etaMinutes,
                                    double scheduleDeviationSeconds);
    // Immediate drawing on effect_ for the sky and the people, around the queued world and train.
    void drawMesh(GpuMesh& mesh, const Microsoft::Xna::Framework::Matrix& world,
                  const Microsoft::Xna::Framework::Matrix& view,
                  const Microsoft::Xna::Framework::Matrix& projection, RenderStats& stats);
    void drawBox(const Microsoft::Xna::Framework::Matrix& world,
                 const Microsoft::Xna::Framework::Matrix& view,
                 const Microsoft::Xna::Framework::Matrix& projection,
                 const Microsoft::Xna::Framework::Color& color, RenderStats& stats);
    void drawAvatars(const Microsoft::Xna::Framework::Matrix& view,
                     const Microsoft::Xna::Framework::Matrix& projection,
                     const Sim::PassengerSystem& passengers, double seconds,
                     float renderDelta,
                     float playerX, float playerZ,
                     RenderStats& stats);
    void drawPlatformPeople(const Microsoft::Xna::Framework::Matrix& view,
                            const Microsoft::Xna::Framework::Matrix& projection,
                            const Sim::Route& route, const Sim::ServiceState& service,
                            RenderStats& stats);
    void queueStationClock(const Sim::Route& route, const Sim::ServiceState& service,
                           RenderStats& stats);
    void addNextAvatarVisual();

    Microsoft::Xna::Framework::Graphics::GraphicsDevice& device_;
    // Effects keep non-owning texture pointers. Declare owners first so reverse member
    // destruction releases every effect before either texture.
    std::unique_ptr<Microsoft::Xna::Framework::Graphics::Texture2D> whiteMaterialTexture_;
    std::unique_ptr<Microsoft::Xna::Framework::Graphics::Texture2D> passengerDisplayFontTexture_;
    std::unique_ptr<Microsoft::Xna::Framework::Graphics::Texture2D> terrainDetailTexture_;
    std::unique_ptr<Microsoft::Xna::Framework::Graphics::Texture2D> foliageDetailTexture_;
    std::unique_ptr<Microsoft::Xna::Framework::Graphics::Texture2D> needleDetailTexture_;
    std::unique_ptr<Microsoft::Xna::Framework::Graphics::Texture2D> leafSprayTexture_;
    std::unique_ptr<Microsoft::Xna::Framework::Graphics::Texture2D> needleSprayTexture_;
    std::unique_ptr<Microsoft::Xna::Framework::Graphics::AlphaTestEffect> leafSprayEffect_;
    std::unique_ptr<GpuMesh> leafSprays_;
    std::unique_ptr<GpuMesh> needleSprays_;
    std::unique_ptr<Microsoft::Xna::Framework::Graphics::Texture2D> roofDetailTexture_;
    std::unique_ptr<Microsoft::Xna::Framework::Graphics::Texture2D> ballastDetailTexture_;
    std::unique_ptr<Microsoft::Xna::Framework::Graphics::Texture2D> carpetDetailTexture_;
    std::unique_ptr<Microsoft::Xna::Framework::Graphics::Texture2D> upholsteryDetailTexture_;
    std::unique_ptr<Microsoft::Xna::Framework::Graphics::Texture2D> concreteDetailTexture_;
    std::unique_ptr<Microsoft::Xna::Framework::Graphics::Texture2D> interiorPanelDetailTexture_;
    std::unique_ptr<Microsoft::Xna::Framework::Graphics::Texture2D> waterDetailTexture_;
    std::unique_ptr<Microsoft::Xna::Framework::Graphics::Texture2D> resilientFloorDetailTexture_;
    std::unique_ptr<Microsoft::Xna::Framework::Graphics::Texture2D> masonryDetailTexture_;
    std::unique_ptr<Microsoft::Xna::Framework::Graphics::Texture2D> renderedFacadeDetailTexture_;
    std::unique_ptr<Microsoft::Xna::Framework::Graphics::Texture2D> cloudDetailTexture_;
    std::unique_ptr<Microsoft::Xna::Framework::Graphics::BasicEffect> effect_;
    std::unique_ptr<Microsoft::Xna::Framework::Graphics::BasicEffect> materialEffect_;
    std::unique_ptr<Microsoft::Xna::Framework::Graphics::BasicEffect> passengerDisplayEffect_;
    std::unique_ptr<OpenGL33FacadeEffect> facadeEffect_;
    std::unique_ptr<OpenGL33WaterEffect> waterEffect_;
    int facadeLightingMode_ = 0;
    std::unique_ptr<GpuMesh> interior_;
    std::unique_ptr<GpuMesh> interiorMaterial_;
    std::unique_ptr<GpuMesh> upholsteryMaterial_;
    std::unique_ptr<GpuMesh> interiorPanelMaterial_;
    std::unique_ptr<GpuMesh> interiorResilientMaterial_;
    std::unique_ptr<GpuMesh> interiorShadows_;
    std::unique_ptr<GpuMesh> interiorLightGradients_;
    std::unique_ptr<GpuMesh> interiorEmitters_;
    std::unique_ptr<GpuMesh> interiorLettering_;
    std::unique_ptr<GpuMesh> windowGlass_;
    std::array<std::unique_ptr<GpuMesh>,2> windowReflections_;
    std::unique_ptr<GpuMesh> sunlightPatches_;
    std::unique_ptr<GpuMesh> cube_;
    std::unique_ptr<GpuMesh> sky_;
    std::unique_ptr<GpuMesh> clouds_;
    std::unique_ptr<GpuMesh> sun_;
    std::vector<std::unique_ptr<GpuMesh>> roadVehicles_;
    std::array<std::unique_ptr<GpuMesh>,
               static_cast<std::size_t>(World::OtherVehicleKind::Count)> otherVehicles_;
    std::unique_ptr<GpuMesh> levelCrossingBarrier_;
    std::unique_ptr<GpuMesh> contactShadow_;
    std::unique_ptr<GpuMesh> platformLuggage_;
    std::unique_ptr<GpuMesh> passengerPhone_;
    std::unique_ptr<GpuMesh> conductorHandheld_;
    std::unique_ptr<GpuMesh> informationDisplays_;
    std::unique_ptr<GpuMesh> platformInformation_;
    std::unique_ptr<GpuMesh> platformLettering_;
    std::unique_ptr<GpuMesh> stationLettering_;
    std::unique_ptr<GpuMesh> trackDetail_;
    std::unique_ptr<GpuMesh> trackBedMaterial_;
    std::unique_ptr<GpuMesh> railHeadMaterial_;
    std::unique_ptr<GpuMesh> terrain_;
    std::unique_ptr<GpuMesh> foliageMaterial_;
    std::unique_ptr<GpuMesh> needleMaterial_;
    std::unique_ptr<GpuMesh> roofMaterial_;
    std::unique_ptr<GpuMesh> waterMaterial_;
    std::unique_ptr<GpuMesh> staticWorld_;
    std::unique_ptr<GpuMesh> facadeMaterial_;
    std::unique_ptr<GpuMesh> renderedFacadeMaterial_;
    std::unique_ptr<GpuMesh> staticWorldShadows_;
    std::unique_ptr<GpuMesh> platformMaterial_;
    std::unique_ptr<GpuMesh> worldLuminaires_;
    std::unique_ptr<GpuMesh> worldNightGlow_;
    // Time of day and weather (ENV-138/139).
    int serviceStartMinutes_ = 9*60 + 17;
    Sim::WeatherMode weatherMode_ = Sim::WeatherMode::Clear;
    Atmosphere atmosphere_;
    SkyPalette builtSkyPalette_;
    Microsoft::Xna::Framework::Vector3 builtSkySun_{0,0,0};
    int skyRebuilds_ = 0;
    // Shadow-casting sun baked into new world windows, refreshed in whole-degree steps.
    SunPosition worldSun_ = sunPosition(Sim::kDefaultServiceStartHours);
    std::array<std::unique_ptr<GpuMesh>,2> rainStreaks_;
    std::unique_ptr<GpuMesh> rainDroplets_;
    std::array<std::unique_ptr<GpuMesh>,
               static_cast<std::size_t>(World::OtherVehicleKind::Count)> otherVehicleNight_;
    std::unique_ptr<GpuMesh> passengerDoor_;
    std::unique_ptr<GpuMesh> passengerDoorButton_;
    std::unique_ptr<GpuMesh> passengerDoorGlass_;
    std::unique_ptr<MeshBuilder> passengerDoorGlassScratch_;
    float passengerDoorGlassOpenness_ = -1.0f;
    bool passengerDoorGlassNegativeX_ = false;
    std::unique_ptr<GpuMesh> boardingStep_;
    std::unique_ptr<GpuMesh> gangwayDoor_;
    std::unique_ptr<GpuMesh> cabActiveDisplay_;
    std::unique_ptr<GpuMesh> toiletDoor_;

    // The world and the train are queued in their hand-ordered sequence and drawn by
    // cna-engine's renderer through Binder, which configures the effects above.
    std::unique_ptr<CnaEngine::Graphics::ForwardRenderer> renderer_;
    std::shared_ptr<Binder> binder_;
    CnaEngine::Graphics::RenderQueue queue_;
    CnaEngine::Graphics::Camera camera_;
    FrameLighting frame_;
    std::deque<CnaEngine::Graphics::Material> frameMaterials_;
    std::deque<Look> frameLooks_;

    struct AvatarVisual;
    struct InformationDisplayState
    {
        std::size_t nextStation = 0;
        int direction = 1;
        bool dwelling = false;
        int page = 0;
        int speedKilometresPerHour = 0;
        int etaMinutes = 0;
        bool operator==(const InformationDisplayState&) const = default;
    };
    struct PlatformInformationState
    {
        std::size_t station = 0;
        int direction = 1;
        int phase = 0;
        int etaMinutes = 0;
        int deviationMinutes = 0;
        bool operator==(const PlatformInformationState&) const = default;
    };
    struct AvatarAssignment
    {
        std::size_t passengerId = 0;
        std::uint32_t appearanceSeed = 0;
    };
    std::vector<std::unique_ptr<AvatarVisual>> avatars_;
    std::vector<std::optional<AvatarAssignment>> onboardAvatarAssignments_;
    std::vector<std::size_t> readyAvatarScratch_;
    std::vector<const Sim::Passenger*> visiblePassengerScratch_;
    int avatarBuildCooldownFrames_ = 0;
    int civilianAvatarDescriptionCount_ = 0;
    double previousRenderSeconds_ = -1.0;
    float doorOpenAmount_ = 0.0f;
    std::array<float, Sim::TrainInterior::PassengerCars - 1> gangwayDoorOpenAmounts_{};
    float toiletDoorOpenAmount_ = 0.0f;
    float tunnelLightingMix_ = 0.0f;
    double trackAnchorDistance_ = -1.0;
    std::uint64_t trackRebuilds_ = 0;
    double trackRebuildTotalMs_ = 0.0;
    double trackRebuildMaxMs_ = 0.0;
    double trackGenerationMaxMs_ = 0.0;
    double streamingMetresPerFrame_ = 0.0;
    double lastStreamingDistance_ = -1.0;
    double worldAnchorDistance_ = -1.0;
    std::int64_t worldAnchorChunk_ = -1;
    struct WorldStreamingState;
    std::unique_ptr<WorldStreamingState> worldStreaming_;
    struct TrackStreamingState;
    std::unique_ptr<TrackStreamingState> trackStreaming_;
    WorldGeometryCounters worldCounters_;
    int tracksideKilometrePosts_ = 0;
    int trackFastenerAssemblies_ = 0;
    int trackTurnoutPointMotors_ = 0;
    int trackTurnoutBladeAssemblies_ = 0;
    int skyCloudBillboardCount_ = 0;
    int paSpeakerGrilleCount_ = 0;
    int hvacVentCount_ = 0;
    int carIdentitySignCount_ = 0;
    int carFunctionFixtureGroupCount_ = 0;
    int cabControlGroupCount_ = 0;
    int vestibulePortalDetailGroupCount_ = 0;
    int overheadLuggageItemCount_ = 0;
    int floorLuggageCaseCount_ = 0;
    int floorLuggageWheelPairCount_ = 0;
    int roundedSeatCushionCount_ = 0;
    int roundedSeatBackCount_ = 0;
    int seatPedestalCount_ = 0;
    int seatMountFastenerCount_ = 0;
    int roundedAisleArmrestCount_ = 0;
    int roundedTableTopCount_ = 0;
    int tableSupportPlateCount_ = 0;
    int windowReflectionAccentCount_ = 0;
    int interiorLightGradientQuadCount_ = 0;
    int sunlightPatchCount_ = 0;
    int aisleRunnerInlayCount_ = 0;
    int nonCarpetedFloorZoneCount_ = 0;
    int interiorCovingSectionCount_ = 0;
    int ceilingPanelSeamCount_ = 0;
    int vestibuleWearMarkCount_ = 0;
    std::optional<InformationDisplayState> informationDisplayState_;
    std::optional<PlatformInformationState> platformInformationState_;
    float vegetationDistanceMetres_ = 750.0f;
    int graphicsQuality_ = 2;
    std::uint64_t worldRebuilds_ = 0;
    std::uint64_t worldBufferReuses_ = 0;
    std::uint64_t routePointCacheHits_ = 0;
    std::uint64_t routePointCacheMisses_ = 0;
    double worldRebuildTotalMs_ = 0.0;
    double worldRebuildMaxMs_ = 0.0;
    double worldGenerationTotalMs_ = 0.0;
    double worldGenerationMaxMs_ = 0.0;
    double worldUploadTotalMs_ = 0.0;
    double worldUploadMaxMs_ = 0.0;
};

} // namespace TrainSim::Render
