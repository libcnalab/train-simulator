#include "SceneShared.hpp"

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;
using namespace Microsoft::Xna::Framework::GamerServices;

namespace TrainSim::Render {

SceneRenderer::SceneRenderer(GraphicsDevice& device, float vegetationDistanceMetres,
                             int graphicsQuality, bool passengerDoorsInitiallyOpen,
                             bool avatarRenderingEnabled)
    : device_(device),
      doorOpenAmount_(passengerDoorsInitiallyOpen ? 1.0f : 0.0f),
      vegetationDistanceMetres_(std::clamp(vegetationDistanceMetres, 250.0f, 1'000.0f)),
      graphicsQuality_(std::clamp(graphicsQuality, 0, 2))
{
    ProceduralImage whiteMaterialImage;
    whiteMaterialImage.width = 8;
    whiteMaterialImage.height = 8;
    whiteMaterialImage.pixels.assign(64U,Color::White);
    whiteMaterialTexture_ = uploadMipmappedTexture(device_,whiteMaterialImage);
    passengerDisplayFontTexture_ = uploadMipmappedTexture(device_,makePassengerDisplayFontImage());
    passengerDisplayEffect_ = std::make_unique<BasicEffect>(device_);
    passengerDisplayEffect_->setVertexColorEnabledProperty(true);
    passengerDisplayEffect_->setLightingEnabledProperty(false);
    passengerDisplayEffect_->setTextureEnabledProperty(true);
    passengerDisplayEffect_->setTextureProperty(passengerDisplayFontTexture_.get());
    effect_ = std::make_unique<BasicEffect>(device_);
    effect_->setVertexColorEnabledProperty(true);
    effect_->setLightingEnabledProperty(true);
    effect_->setPreferPerPixelLightingProperty(false);
    effect_->setAmbientLightColorProperty(Vector3{0.56f,0.58f,0.62f});
    effect_->setSpecularColorProperty(Vector3::Zero);
    effect_->DirectionalLight0.setEnabledProperty(true);
    effect_->DirectionalLight0.setDirectionProperty(
        daylightDirection(0.0));
    effect_->DirectionalLight0.setDiffuseColorProperty(Vector3{0.46f,0.43f,0.37f});
    effect_->DirectionalLight0.setSpecularColorProperty(Vector3::Zero);
    effect_->DirectionalLight1.setEnabledProperty(false);
    effect_->DirectionalLight2.setEnabledProperty(false);
    // A complete white mip chain is the identity material for legacy vertex-coloured batches.
    // Keeping it explicit makes sampler ownership deterministic as textured material passes are
    // introduced, rather than relying on a renderer-provided implicit fallback texture.
    effect_->setTextureProperty(whiteMaterialTexture_.get());
    effect_->setTextureEnabledProperty(true);
    effect_->setFogEnabledProperty(true);
    effect_->setFogColorProperty(rgb(daylightSkyColor(0)));
    effect_->setFogStartProperty(360.0f);
    effect_->setFogEndProperty(1'050.0f);
    terrainDetailTexture_ = uploadMipmappedTexture(device_,makeTerrainDetailImage());
    foliageDetailTexture_ = uploadMipmappedTexture(device_,makeFoliageDetailImage());
    needleDetailTexture_ = uploadMipmappedTexture(device_,makeNeedleDetailImage());
    leafSprayTexture_=uploadMipmappedTexture(device_,makeLeafSprayImage(),96);
    needleSprayTexture_=uploadMipmappedTexture(device_,makeNeedleSprayImage(),96);
    leafSprayEffect_=std::make_unique<AlphaTestEffect>(device_);
    leafSprayEffect_->setTextureProperty(leafSprayTexture_.get());
    leafSprayEffect_->setVertexColorEnabledProperty(true);
    leafSprayEffect_->setReferenceAlphaProperty(96);
    roofDetailTexture_ = uploadMipmappedTexture(device_,makeRoofDetailImage());
    ballastDetailTexture_ = uploadMipmappedTexture(device_,makeBallastDetailImage());
    carpetDetailTexture_ = uploadMipmappedTexture(device_,makeCarpetDetailImage());
    upholsteryDetailTexture_ = uploadMipmappedTexture(device_,makeUpholsteryDetailImage());
    concreteDetailTexture_ = uploadMipmappedTexture(device_,makeConcreteDetailImage());
    interiorPanelDetailTexture_ = uploadMipmappedTexture(
        device_,makeInteriorPanelDetailImage());
    waterDetailTexture_ = uploadMipmappedTexture(device_,makeWaterDetailImage());
    resilientFloorDetailTexture_ = uploadMipmappedTexture(
        device_,makeResilientFloorDetailImage());
    masonryDetailTexture_ = uploadMipmappedTexture(device_,makeMasonryDetailImage());
    renderedFacadeDetailTexture_ = uploadMipmappedTexture(
        device_,makeRenderedFacadeDetailImage());
    cloudDetailTexture_ = uploadMipmappedTexture(device_,makeCloudAtlasImage());
    // Keep textured materials on their own stock effect so their lighting, texture ownership,
    // and future specular policy remain independent from the legacy vertex-colour batches.
    materialEffect_ = std::make_unique<BasicEffect>(device_);
    materialEffect_->setVertexColorEnabledProperty(true);
    materialEffect_->setLightingEnabledProperty(true);
    materialEffect_->setPreferPerPixelLightingProperty(graphicsQuality_ >= 1);
    materialEffect_->setAmbientLightColorProperty(Vector3{0.56f,0.58f,0.62f});
    materialEffect_->setSpecularColorProperty(Vector3::Zero);
    materialEffect_->DirectionalLight0.setEnabledProperty(true);
    materialEffect_->DirectionalLight0.setDirectionProperty(
        daylightDirection(0.0));
    materialEffect_->DirectionalLight0.setDiffuseColorProperty(Vector3{0.46f,0.43f,0.37f});
    materialEffect_->DirectionalLight0.setSpecularColorProperty(Vector3::Zero);
    materialEffect_->DirectionalLight1.setEnabledProperty(false);
    materialEffect_->DirectionalLight2.setEnabledProperty(false);
    materialEffect_->setTextureProperty(terrainDetailTexture_.get());
    materialEffect_->setTextureEnabledProperty(true);
    materialEffect_->setFogEnabledProperty(true);
    materialEffect_->setFogColorProperty(rgb(daylightSkyColor(0)));
    materialEffect_->setFogStartProperty(360.0f);
    materialEffect_->setFogEndProperty(1'050.0f);
    buildInterior();

    MeshBuilder cube;
    cube.addBox(Vector3{-0.5f,-0.5f,-0.5f}, Vector3{0.5f,0.5f,0.5f}, Color::White);
    cube_ = std::make_unique<GpuMesh>(device_, cube.take());

    // Streamed: the time of day and weather rebuild the palette (ENV-138/139).
    MeshBuilder sky;
    addDaylightSky(sky,1'070.0f);
    builtSkyPalette_ = atmosphere(Sim::kDefaultServiceStartHours,{}).sky;
    sky_ = std::make_unique<GpuMesh>(device_, sky.take(), true);

    MeshBuilder clouds;
    constexpr float tau = 6.2831853071795864769f;
    struct CloudBillboard { float angle; float y; float width; float height; };
    constexpr std::array<CloudBillboard,8> cloudLayout = {{
        {tau*0.03f,155.0f,184.0f,78.0f}, {tau*0.36f,245.0f,165.0f,128.0f},
        {tau*0.53f,108.0f,325.0f,105.0f}, {tau*0.86f,172.0f,220.0f,98.0f},
        {tau*0.19f,105.0f,260.0f,85.0f}, {tau*0.70f,195.0f,118.0f,105.0f},
        {tau*0.11f, 75.0f,390.0f,90.0f}, {tau*0.62f, 95.0f,290.0f,83.0f}
    }};
    skyCloudBillboardCount_ = graphicsQuality_ == 0 ? 4 : graphicsQuality_ == 1 ? 6 : 8;
    constexpr float cloudRadius = 925.0f;
    for (int index = 0; index < skyCloudBillboardCount_; ++index)
    {
        const CloudBillboard& cloud = cloudLayout[static_cast<std::size_t>(index)];
        const Vector3 radial{std::cos(cloud.angle),0.0f,std::sin(cloud.angle)};
        const Vector3 tangent{-std::sin(cloud.angle),0.0f,std::cos(cloud.angle)};
        const Vector3 centre = radial*cloudRadius+Vector3{0.0f,cloud.y,0.0f};
        const Vector3 lowerRight = centre+tangent*(cloud.width*0.5f)
                                 - Vector3::Up*(cloud.height*0.5f);
        const Vector3 lowerLeft = centre-tangent*(cloud.width*0.5f)
                                - Vector3::Up*(cloud.height*0.5f);
        const Vector3 upperLeft = centre-tangent*(cloud.width*0.5f)
                                + Vector3::Up*(cloud.height*0.5f);
        const Vector3 upperRight = centre+tangent*(cloud.width*0.5f)
                                 + Vector3::Up*(cloud.height*0.5f);
        const bool mirror = index%2 != 0;
        const float tileX=static_cast<float>(index%2)*0.5f;
        const float tileY=static_cast<float>((index%4)/2)*0.5f;
        const float left=tileX+(mirror ? 0.5f : 0.0f);
        const float right=tileX+(mirror ? 0.0f : 0.5f);
        clouds.addTexturedQuad(lowerRight,lowerLeft,upperLeft,upperRight,Color::White,
            Vector2{left,tileY+0.5f},Vector2{right,tileY+0.5f},
            Vector2{right,tileY},Vector2{left,tileY});
    }
    clouds_ = std::make_unique<GpuMesh>(device_,clouds.take());

    MeshBuilder sun;
    sun.addDiscXY(Vector3::Zero, 1.0f, 24, Color(255,238,166,255));
    sun_ = std::make_unique<GpuMesh>(device_, sun.take());

    // One periodic rain tile (ENV-139), repeated, sheared and scrolled around the camera.
    // Separate negative- and positive-X halves let a platform canopy shelter its side.
    for (std::size_t half = 0; half < rainStreaks_.size(); ++half)
    {
        MeshBuilder rain;
        addRainStreaks(rain,graphicsQuality_ == 0 ? 190 : graphicsQuality_ == 1 ? 325 : 475,
                       0x5241494EU+static_cast<std::uint32_t>(half),half == 0 ? -1.0f : 1.0f);
        rainStreaks_[half] = std::make_unique<GpuMesh>(device_, rain.take());
    }

    // Road traffic uses six color variants of the same compact mesh. Each vehicle remains one
    // submission, but the silhouette now reads as a car rather than a moving cuboid.
    roadVehicles_.reserve(6);
    for (std::uint32_t variant = 0; variant < 6; ++variant)
    {
        MeshBuilder car;
        const Color body = roadVehicleColor(variant);
        const Color glass(42,72,84,255);
        const Color rubber(29,31,31,255);
        addRoadCarBody(car,body);
        // A tapered shell leaves the sloping glazing outside the body. A solid upright cabin
        // previously intersected both windscreens and hid their upper halves.
        car.addRoundedBoxXZ(Vector3{-0.64f,1.50f,-0.92f},
                            Vector3{0.64f,1.58f,0.96f},0.12f,2,
                            body);
        for (float side : {-1.0f, 1.0f})
        {
            const auto sidePanel = [&](float lowerY, float upperY, float lowerBack,
                                       float lowerFront, float upperBack, float upperFront,
                                       float outward, const Color& colour,bool glazing=false) {
                const auto xAt = [&](float y) {
                    return side*(0.76f-(y-1.02f)*0.12f/0.48f+outward);
                };
                const Vector3 a{xAt(lowerY),lowerY,lowerBack};
                const Vector3 b{xAt(lowerY),lowerY,lowerFront};
                const Vector3 c{xAt(upperY),upperY,upperFront};
                const Vector3 d{xAt(upperY),upperY,upperBack};
                const Color lower=glazing ? Color(32,46,51,255) : colour;
                const Color upper=glazing ? Color(91,119,133,255) : colour;
                if (side > 0.0f) car.addQuad(d,c,b,a,upper,upper,lower,lower);
                else car.addQuad(a,b,c,d,lower,lower,upper,upper);
            };
            sidePanel(1.02f,1.50f,-1.18f,1.18f,-0.92f,0.92f,0.0f,body);
            sidePanel(1.08f,1.44f,-1.08f,-0.055f,-0.88f,-0.055f,0.008f,glass,true);
            sidePanel(1.08f,1.44f,0.055f,1.08f,0.055f,0.88f,0.008f,glass,true);
            sidePanel(1.02f,1.06f,-1.16f,1.16f,-1.14f,1.14f,0.012f,Color(60,65,64,255));
            for (float doorZ : {-0.75f,0.40f})
                car.addBox({std::min(side*0.901f,side*0.923f),0.84f,doorZ},
                           {std::max(side*0.901f,side*0.923f),0.874f,doorZ+0.16f},
                           Color(174,179,176,255));
            car.addBox({std::min(side*0.901f,side*0.904f),0.37f,-0.009f},
                       {std::max(side*0.901f,side*0.904f),1.015f,0.009f},Color(48,53,51,255));
            for (float axle : {-1.30f, 1.30f})
            {
                const float wheelX = side * 0.88f;
                car.addRoundedBoxYZ(Vector3{wheelX-0.12f,0.0f,axle-0.30f},
                                    Vector3{wheelX+0.12f,0.60f,axle+0.30f},
                                    0.30f,3,rubber);
                const float hubInner = side < 0.0f ? -1.008f : 0.998f;
                const float hubOuter = side < 0.0f ? -0.998f : 1.008f;
                car.addRoundedBoxYZ(Vector3{hubInner,0.12f,axle-0.18f},
                                    Vector3{hubOuter,0.48f,axle+0.18f},
                                    0.18f,3,Color(48,54,53,255));
                addRoadWheelHub(car,side,axle);
            }
            const float mirrorInner = side < 0.0f ? -0.98f : 0.80f;
            const float mirrorOuter = side < 0.0f ? -0.80f : 0.98f;
            car.addRoundedBoxXZ(Vector3{mirrorInner,1.11f,0.55f},
                                Vector3{mirrorOuter,1.28f,0.82f},0.05f,1,
                                body);
        }
        for (float end : {-1.0f,1.0f})
        {
            const auto endPanel = [&](float lowerY, float upperY, float lowerWidth,
                                      float upperWidth, float outward, const Color& colour,bool glazing=false) {
                const auto zAt = [&](float y) {
                    return end*(1.18f-(y-1.02f)*0.26f/0.48f+outward);
                };
                const Vector3 a{-lowerWidth,lowerY,zAt(lowerY)};
                const Vector3 b{-upperWidth,upperY,zAt(upperY)};
                const Vector3 c{upperWidth,upperY,zAt(upperY)};
                const Vector3 d{lowerWidth,lowerY,zAt(lowerY)};
                const Color lower=glazing ? Color(32,46,51,255) : colour;
                const Color upper=glazing ? Color(91,119,133,255) : colour;
                if (end > 0.0f) car.addQuad(d,c,b,a,lower,upper,upper,lower);
                else car.addQuad(a,b,c,d,lower,upper,upper,lower);
            };
            endPanel(1.02f,1.50f,0.76f,0.64f,0.0f,body);
            endPanel(1.08f,1.44f,0.665f,0.575f,0.008f,glass,true);
        }
        car.addBox(Vector3{-0.72f,0.43f,2.055f}, Vector3{-0.37f,0.65f,2.085f},
                   Color(247,231,174,255));
        car.addBox(Vector3{0.37f,0.43f,2.055f}, Vector3{0.72f,0.65f,2.085f},
                   Color(247,231,174,255));
        car.addBox(Vector3{-0.72f,0.43f,-2.085f}, Vector3{-0.37f,0.65f,-2.055f},
                   Color(190,43,37,255));
        car.addBox(Vector3{0.37f,0.43f,-2.085f}, Vector3{0.72f,0.65f,-2.055f},
                   Color(190,43,37,255));
        car.addBox(Vector3{-0.94f,0.25f,-2.12f}, Vector3{0.94f,0.36f,-1.98f},
                   Color(54,58,58,255));
        car.addBox(Vector3{-0.94f,0.25f,1.98f}, Vector3{0.94f,0.36f,2.12f},
                   Color(54,58,58,255));
        car.addBox({-0.31f,0.38f,2.055f},{0.31f,0.52f,2.089f},Color(38,43,43,255));
        for (float end : {-1.0f,1.0f})
            car.addBox({-0.22f,0.56f,std::min(end*2.086f,end*2.096f)},
                       {0.22f,0.66f,std::max(end*2.086f,end*2.096f)},Color(213,214,203,255));
        roadVehicles_.push_back(std::make_unique<GpuMesh>(device_, car.take()));
    }

    // One complete mesh per oncoming/parked vehicle kind (ENV-137); see OtherTrainGeometry.hpp.
    // At night lit saloon windows and lamps glow through an additive overlay (ENV-138).
    // Driving-cab glazing (34,52,62) stays dark.
    const std::array<NightGlowMatch,9> vehicleGlow{{
        {Color(43,79,96,255),Color(214,198,156,255)},   // Regional unit saloon glazing.
        {Color(56,82,104,255),Color(220,204,160,255)},  // Intercity coach glazing.
        {Color(46,64,74,255),Color(218,206,168,255)},   // Diesel unit glazing.
        {Color(40,58,70,255),Color(200,188,150,255)},   // Plug-door windows.
        {Color(244,246,228,255),Color(255,252,236,255)},
        {Color(240,244,226,255),Color(255,252,236,255)},
        {Color(250,239,181,255),Color(255,240,190,255)},
        {Color(214,38,34,255),Color(255,44,30,255)},
        {Color(211,40,38,255),Color(255,44,30,255)}}};
    for (std::size_t kind = 0; kind < otherVehicles_.size(); ++kind)
    {
        MeshBuilder vehicle;
        addOtherTrainVehicle(vehicle,static_cast<World::OtherVehicleKind>(kind));
        const MeshData glow = nightGlowOverlay(vehicle.data(),vehicleGlow);
        if (!glow.empty()) otherVehicleNight_[kind] = std::make_unique<GpuMesh>(device_,glow);
        otherVehicles_[kind] = std::make_unique<GpuMesh>(device_,vehicle.take());
    }

    MeshBuilder crossingBarrier;
    constexpr float crossingBarrierLength = 4.8f;
    constexpr int crossingBarrierBands = 8;
    for (int band = 0; band < crossingBarrierBands; ++band)
    {
        const float begin = crossingBarrierLength
            * static_cast<float>(band)/static_cast<float>(crossingBarrierBands);
        const float end = crossingBarrierLength
            * static_cast<float>(band+1)/static_cast<float>(crossingBarrierBands);
        crossingBarrier.addBox(Vector3{-0.065f,-0.065f,begin},
                               Vector3{0.065f,0.065f,end},
                               band%2 == 0 ? Color(236,230,207,255)
                                           : Color(207,52,45,255));
    }
    crossingBarrier.addBox(Vector3{-0.13f,-0.16f,-0.14f},
                           Vector3{0.13f,0.16f,0.17f},Color(69,72,69,255));
    levelCrossingBarrier_ = std::make_unique<GpuMesh>(device_,crossingBarrier.take());

    MeshBuilder contactShadow;
    contactShadow.addDiscXZ(Vector3::Zero, 1.0f, 16, Color(8,12,14,82));
    contactShadow_ = std::make_unique<GpuMesh>(device_, contactShadow.take());

    // A compact wheeled case gives selected platform passengers a travel purpose. One shared
    // mesh keeps the detail inexpensive even at a major station; the colored tag and extended
    // handle remain readable from a window without relying on textures.
    MeshBuilder platformLuggage;
    // Two moulded shells meet at a narrow dark zipper gasket. Shallow vertical ribs break up
    // the face without the former oversized horizontal bars or sharp crate-like corners.
    platformLuggage.addRoundedBoxXY(Vector3{-0.23f,0.08f,-0.14f},
                           Vector3{0.23f,0.64f,0.14f},0.055f,3,Color(43,69,91,255));
    platformLuggage.addRoundedBoxXY(Vector3{-0.233f,0.077f,-0.008f},
                           Vector3{0.233f,0.643f,0.008f},0.055f,3,Color(27,32,37,255));
    for (float rib : {-0.14f,-0.07f,0.0f,0.07f,0.14f})
        for (float face : {-1.0f,1.0f})
            platformLuggage.addRoundedBoxXY(Vector3{rib-0.012f,0.17f,face*0.144f-0.006f},
                Vector3{rib+0.012f,0.55f,face*0.144f+0.006f},0.011f,2,Color(58,82,102,255));
    for (float x : {-0.15f,0.15f})
    {
        platformLuggage.addBox(Vector3{x-0.010f,0.61f,-0.07f},
                               Vector3{x+0.010f,0.91f,-0.05f},Color(149,158,163,255));
        for (float z : {-0.105f,0.105f})
            platformLuggage.addEllipsoid(Vector3{x,0.045f,z},
                Vector3{0.028f,0.045f,0.045f},8,4,Color(30,33,34,255),true);
    }
    platformLuggage.addRoundedBoxXY(Vector3{-0.17f,0.89f,-0.08f},
                           Vector3{0.17f,0.93f,-0.035f},0.018f,3,Color(52,57,58,255));
    platformLuggage.addBox(Vector3{0.145f,0.34f,-0.151f},
                           Vector3{0.215f,0.48f,-0.145f},Color(224,180,46,255));
    platformLuggage_ = std::make_unique<GpuMesh>(device_,platformLuggage.take());

    // A tiny shared handset supports a restrained standing phone-check idle. Its luminous screen
    // and side key remain legible at saloon distance without a texture or unique passenger mesh.
    MeshBuilder passengerPhone;
    passengerPhone.addRoundedBoxXZ(Vector3{-0.075f,-0.14f,-0.018f},
                                    Vector3{0.075f,0.14f,0.018f},0.018f,2,
                                    Color(31,37,40,255));
    passengerPhone.addBox(Vector3{-0.060f,-0.105f,0.018f},
                          Vector3{0.060f,0.097f,0.024f},Color(58,117,139,255));
    passengerPhone.addBox(Vector3{-0.048f,0.068f,0.025f},
                          Vector3{0.048f,0.082f,0.028f},Color(133,207,218,255));
    passengerPhone.addBox(Vector3{0.074f,-0.025f,-0.010f},
                          Vector3{0.082f,0.035f,0.010f},Color(87,94,95,255));
    passengerPhone_ = std::make_unique<GpuMesh>(device_,passengerPhone.take());

    // One reusable staff handheld gives the conductor a visible task during inspections and
    // station duty. Housing, inset screen, status bar, keys, and wrist loop share one small mesh.
    MeshBuilder conductorHandheld;
    conductorHandheld.addRoundedBoxXZ(Vector3{-0.105f,-0.15f,-0.034f},
                                      Vector3{0.105f,0.15f,0.034f},0.025f,2,
                                      Color(38,48,53,255));
    conductorHandheld.addBox(Vector3{-0.078f,-0.055f,0.034f},
                             Vector3{0.078f,0.105f,0.041f},Color(67,133,151,255));
    conductorHandheld.addBox(Vector3{-0.064f,0.074f,0.042f},
                             Vector3{0.064f,0.091f,0.045f},Color(123,211,222,255));
    for (float keyX : {-0.052f,0.0f,0.052f})
        conductorHandheld.addBox(Vector3{keyX-0.016f,-0.115f,0.035f},
                                 Vector3{keyX+0.016f,-0.083f,0.043f},
                                 Color(197,184,116,255));
    conductorHandheld.addBox(Vector3{0.102f,-0.19f,-0.012f},
                             Vector3{0.122f,-0.13f,0.012f},Color(30,35,37,255));
    conductorHandheld_ = std::make_unique<GpuMesh>(device_,conductorHandheld.take());

    MeshBuilder passengerDoor;
    addPassengerDoorLeaf(passengerDoor);
    passengerDoor_ = std::make_unique<GpuMesh>(device_, passengerDoor.take());
    MeshBuilder passengerDoorButton;
    addPassengerDoorButtonFaces(passengerDoorButton);
    passengerDoorButton_=std::make_unique<GpuMesh>(device_,passengerDoorButton.take());

    // One shared retractable bridge plate closes the authored 20 cm horizontal platform gap at
    // every released doorway. Its outer yellow lip and three shallow anti-slip bars remain part
    // of this tiny reusable mesh rather than becoming per-door geometry allocations.
    MeshBuilder boardingStep;
    const float stepTravel = static_cast<float>(Sim::TrainInterior::BoardingStepTravel);
    constexpr float stepHalfLength = 0.70f;
    constexpr float innerTop = 0.018f;
    constexpr float outerTop = 0.004f;
    constexpr float stepThickness = 0.028f;
    const Color stepMetal(132,137,134,255);
    const Vector3 topA{0.0f,innerTop,-stepHalfLength};
    const Vector3 topB{stepTravel,outerTop,-stepHalfLength};
    const Vector3 topC{stepTravel,outerTop,stepHalfLength};
    const Vector3 topD{0.0f,innerTop,stepHalfLength};
    const Vector3 bottomA{0.0f,innerTop-stepThickness,-stepHalfLength};
    const Vector3 bottomB{stepTravel,outerTop-stepThickness,-stepHalfLength};
    const Vector3 bottomC{stepTravel,outerTop-stepThickness,stepHalfLength};
    const Vector3 bottomD{0.0f,innerTop-stepThickness,stepHalfLength};
    boardingStep.addQuad(topA,topB,topC,topD,stepMetal);
    boardingStep.addQuad(bottomD,bottomC,bottomB,bottomA,stepMetal);
    boardingStep.addQuad(topA,bottomA,bottomB,topB,stepMetal);
    boardingStep.addQuad(topB,bottomB,bottomC,topC,stepMetal);
    boardingStep.addQuad(topC,bottomC,bottomD,topD,stepMetal);
    boardingStep.addQuad(topD,bottomD,bottomA,topA,stepMetal);
    boardingStep.addBox(Vector3{stepTravel-0.070f,outerTop+0.001f,-stepHalfLength},
                        Vector3{stepTravel,outerTop+0.014f,stepHalfLength},
                        Color(235,184,43,255));
    for (float barZ : {-0.42f,0.0f,0.42f})
        for (int segment = 0; segment < 4; ++segment)
        {
            const float x0 = 0.025f + static_cast<float>(segment) * 0.045f;
            const float x1 = x0 + 0.038f;
            const float middle = (x0+x1)*0.5f;
            const float surface = innerTop + (outerTop-innerTop) * middle / stepTravel;
            boardingStep.addBox(Vector3{x0,surface+0.001f,barZ-0.018f},
                                Vector3{x1,surface+0.006f,barZ+0.018f},
                                Color(48,54,54,255));
        }
    boardingStep_ = std::make_unique<GpuMesh>(device_, boardingStep.take());

    MeshBuilder gangwayDoor;
    const Color gangwayFrame(59,82,91,255);
    gangwayDoor.addBox(Vector3{-0.29f,-1.10f,-0.035f},
                       Vector3{-0.235f,1.10f,0.035f},gangwayFrame);
    gangwayDoor.addBox(Vector3{0.235f,-1.10f,-0.035f},
                       Vector3{0.29f,1.10f,0.035f},gangwayFrame);
    gangwayDoor.addBox(Vector3{-0.235f,-1.10f,-0.035f},
                       Vector3{0.235f,-1.035f,0.035f},gangwayFrame);
    gangwayDoor.addBox(Vector3{-0.235f,1.035f,-0.035f},
                       Vector3{0.235f,1.10f,0.035f},gangwayFrame);
    gangwayDoor.addBox(Vector3{-0.235f,-0.98f,-0.018f},
                       Vector3{0.235f,0.98f,0.018f},Color(116,184,201,34));
    // A dark inner seal, brushed kick plate and compact request sensor give the shared leaf the
    // layered construction of a real inter-car door. These pieces deliberately stay within the
    // original 58 cm leaf envelope, so animation clearance and the walkable opening are unchanged.
    const Color gangwaySeal(25,34,37,255);
    gangwayDoor.addBox(Vector3{-0.235f,-0.99f,-0.041f},
                       Vector3{-0.215f,0.99f,0.041f},gangwaySeal);
    gangwayDoor.addBox(Vector3{0.215f,-0.99f,-0.041f},
                       Vector3{0.235f,0.99f,0.041f},gangwaySeal);
    gangwayDoor.addBox(Vector3{-0.215f,-0.99f,-0.041f},
                       Vector3{0.215f,-0.965f,0.041f},gangwaySeal);
    gangwayDoor.addBox(Vector3{-0.215f,0.965f,-0.041f},
                       Vector3{0.215f,0.99f,0.041f},gangwaySeal);
    gangwayDoor.addBox(Vector3{-0.212f,-0.955f,-0.045f},
                       Vector3{0.212f,-0.705f,0.045f},Color(142,149,148,255));
    for (float ribY : {-0.90f,-0.83f,-0.76f})
        gangwayDoor.addBox(Vector3{-0.18f,ribY-0.009f,-0.049f},
                           Vector3{0.18f,ribY+0.009f,0.049f},Color(85,94,95,255));
    gangwayDoor.addBox(Vector3{-0.235f,-0.04f,-0.045f},
                       Vector3{0.235f,0.035f,0.045f},Color(211,180,67,255));
    gangwayDoor.addBox(Vector3{-0.070f,0.67f,-0.048f},
                       Vector3{0.070f,0.84f,0.048f},Color(40,57,63,255));
    gangwayDoor.addBox(Vector3{-0.035f,0.745f,-0.052f},
                       Vector3{0.035f,0.795f,0.052f},Color(78,197,126,255));
    gangwayDoor_ = std::make_unique<GpuMesh>(device_, gangwayDoor.take());

    // One direction-aware luminous instrument cluster is placed in the operating cab at draw
    // time. The inactive desk keeps its physical bezels but no bright screens, which makes a
    // terminal reversal visible through the locked passenger bulkhead for one bounded draw.
    MeshBuilder cabActiveDisplay;
    cabActiveDisplay.addRoundedBoxXY(Vector3{-0.56f,-0.15f,-0.018f},
                                      Vector3{-0.05f,0.15f,0.018f},0.030f,2,
                                      Color(46,105,126,255));
    cabActiveDisplay.addRoundedBoxXY(Vector3{0.06f,-0.15f,-0.018f},
                                      Vector3{0.52f,0.15f,0.018f},0.030f,2,
                                      Color(36,83,105,255));
    cabActiveDisplay.addBox(Vector3{-0.51f,0.092f,-0.023f},
                            Vector3{-0.10f,0.119f,0.023f},Color(112,215,224,255));
    for (int bar = 0; bar < 5; ++bar)
    {
        const float x = 0.11f+static_cast<float>(bar)*0.071f;
        const float height = 0.035f+static_cast<float>((bar*3)%5)*0.016f;
        cabActiveDisplay.addBox(Vector3{x,-0.105f,-0.023f},
                                Vector3{x+0.038f,-0.105f+height,0.023f},
                                bar < 3 ? Color(78,208,136,255) : Color(237,183,50,255));
    }
    cabActiveDisplay_ = std::make_unique<GpuMesh>(device_,cabActiveDisplay.take());

    MeshBuilder door;
    door.addBox(Vector3{-0.04f,0.0f,-0.52f}, Vector3{0.04f,2.18f,0.52f}, Color(55,73,83,255));
    toiletDoor_ = std::make_unique<GpuMesh>(device_, door.take());

    renderer_ = std::make_unique<CnaEngine::Graphics::ForwardRenderer>(device_);
    binder_ = std::make_shared<Binder>(*this);
    renderer_->SetMaterialBinder(binder_);

    if (avatarRenderingEnabled)
    {
        avatars_.reserve(kTotalAvatarVisuals);
        onboardAvatarAssignments_.resize(kOnboardAvatarVisuals);
        readyAvatarScratch_.reserve(kOnboardAvatarVisuals);
        visiblePassengerScratch_.reserve(kOnboardAvatarVisuals + 8);
        addNextAvatarVisual();
    }
}

SceneRenderer::~SceneRenderer() = default;

bool SceneRenderer::avatarLoadingComplete() const noexcept
{
    if (avatars_.empty()) return true;
    const std::size_t target = totalAvatarBudget(graphicsQuality_);
    const AvatarVisual& newest = *avatars_.back();
    const AvatarRendererState state = newest.renderer->getStateProperty();
    // Unavailable is terminal and has no loader thread or deferred upload left to drain.
    if (state == AvatarRendererState::Unavailable) return true;
    if (state == AvatarRendererState::Loading || !newest.gpuPrimed
        || avatarBuildCooldownFrames_ > 0) return false;
    return avatars_.size() >= target;
}

void SceneRenderer::setVegetationDistance(float metres)
{
    const float clamped = std::clamp(metres, 250.0f, 1'000.0f);
    if (std::abs(clamped - vegetationDistanceMetres_) < 0.5f) return;
    vegetationDistanceMetres_ = clamped;
    worldAnchorChunk_ = -1;
}

void SceneRenderer::setGraphicsQuality(int quality)
{
    const int clamped = std::clamp(quality, 0, 2);
    if (clamped == graphicsQuality_) return;
    graphicsQuality_ = clamped;
    materialEffect_->setPreferPerPixelLightingProperty(graphicsQuality_ >= 1);
    trackAnchorDistance_ = -1.0;
    worldAnchorChunk_ = -1;
}

void SceneRenderer::rebuildInformationDisplays(const Sim::Route& route,
                                               const Sim::ServiceState& service,
                                               int speedKilometresPerHour, int etaMinutes,
                                               int informationPage)
{
    const std::string next = service.phase == Sim::ServicePhase::Dwelling
        ? "At " + route.stations()[service.currentStation].name
        : "Next: " + route.stations()[service.nextStation].name;
    const std::string destination = "To " + route.stations()[
        service.direction > 0 ? route.stations().size() - 1 : 0].name;
    std::string progress;
    if (service.phase == Sim::ServicePhase::Dwelling)
    {
        progress = std::string("Doors open: ") + (service.platformOnLeft ? "left" : "right");
    }
    else
    {
        const int following = static_cast<int>(service.nextStation)
                            + service.direction * informationPage;
        if (informationPage > 0 && following >= 0
            && following < static_cast<int>(route.stations().size()))
            progress = std::string(informationPage == 1 ? "Then " : "After ")
                     + route.stations()[static_cast<std::size_t>(following)].name;
        else
            progress = std::to_string(speedKilometresPerHour)
                + " km/h   Arriving in " + std::to_string(etaMinutes) + " min";
    }
    MeshBuilder displays;
    for (int car = 0; car < 6; ++car)
    {
        const float z = kFirstCarCentre + static_cast<float>(car) * kCarLength;
        for (float face : {-1.0f, 1.0f})
        {
            const float surface = z + face * 0.102f;
            const bool mirrored = face < 0.0f;
            addPassengerScreenText(displays,next,2.652f,surface,mirrored,Color(246,232,193,255),0.0050f);
            addPassengerScreenText(displays,destination,2.523f,surface,mirrored,Color(166,211,220,255),0.0040f);
            addPassengerScreenText(displays,progress,2.405f,surface,mirrored,Color(186,204,210,255),0.0036f);
            addPassengerScreenText(displays,PassengerCarRoleLabels[static_cast<std::size_t>(car)],
                2.190f,z+face*0.064f,mirrored,Color(222,233,232,255),0.0038f,-0.85f,1.30f);
        }
    }
    informationDisplays_ = std::make_unique<GpuMesh>(device_, displays.take());
    informationDisplayState_ = InformationDisplayState{
        service.nextStation, service.direction,
        service.phase == Sim::ServicePhase::Dwelling,
        informationPage, speedKilometresPerHour, etaMinutes};
}

void SceneRenderer::rebuildPlatformInformation(const Sim::Route& route,
                                               const Sim::ServiceState& service,
                                               std::size_t stationIndex, int etaMinutes,
                                               double scheduleDeviationSeconds)
{
    const Sim::Station& station = route.stations()[stationIndex];
    const double anchor = station.distanceMetres;
    // Keep the live board readable from the fourth passenger car without placing it squarely in
    // front of the showcase seat. Real platform displays are set behind the principal waiting
    // line and are markedly smaller than an advertising hoarding; the offset also leaves a clear
    // sightline to waiting passengers and the station beyond.
    const bool terminal=stationIndex==0 || stationIndex+1==route.stations().size();
    const double centreDistance = station.distanceMetres + platformDisplayOffset(terminal,station.major);
    const double side = station.platformOnLeft ? -1.0 : 1.0;
    const double lateral = side * 6.20;
    const double faceLateral = lateral - side * 0.110;

    MeshBuilder board;
    MeshBuilder lettering;
    const LocalRoutePoint body = localRoutePoint(route, anchor, centreDistance, lateral);
    addPlatformDepartureBoard(board,Matrix::CreateRotationY(body.yaw-static_cast<float>(side)*kPi*0.5f)
        *Matrix::CreateTranslation(body.x,body.y,body.z),kPlatformSurfaceElevation);

    const std::string destination = "TO " + route.stations()[
        service.direction > 0 ? route.stations().size() - 1 : 0].name;
    const std::string platform = "P" + std::to_string(1 + stationIndex % 4);
    std::string status;
    if (stationIndex == service.currentStation
        && service.phase == Sim::ServicePhase::Dwelling)
        status = platform + "  NOW BOARDING";
    else if (stationIndex == service.currentStation
             && service.phase == Sim::ServicePhase::ClosingDoors)
        status = platform + "  DOORS CLOSING";
    else if (std::abs(scheduleDeviationSeconds) < 45.0)
        status = platform + "  ON TIME  DUE " + std::to_string(std::max(1, etaMinutes)) + " MIN";
    else
    {
        const int minutes = std::max(1, static_cast<int>(
            std::ceil(std::abs(scheduleDeviationSeconds) / 60.0)));
        status = platform + "  " + std::to_string(minutes)
               + (scheduleDeviationSeconds > 0.0 ? " MIN LATE" : " MIN EARLY");
    }
    addRoutePrintedText(lettering,route,anchor,"NORTHERN VALLEY EXPRESS",centreDistance,
                     faceLateral,platformDisplayFirstRow,Color(117,214,226,255),2.62f);
    addRoutePrintedText(lettering,route,anchor,destination,centreDistance,
                     faceLateral,platformDisplayFirstRow-0.32f,Color(242,243,235,255),2.62f);
    addRoutePrintedText(lettering,route,anchor,status,centreDistance,
                     faceLateral,platformDisplayFirstRow-0.64f,Color(244,190,47,255),2.62f);

    platformInformation_ = std::make_unique<GpuMesh>(device_, board.take());
    platformLettering_ = std::make_unique<GpuMesh>(device_,lettering.take());
    platformInformationState_ = PlatformInformationState{
        stationIndex, service.direction, static_cast<int>(service.phase), etaMinutes,
        static_cast<int>(std::round(scheduleDeviationSeconds / 60.0))};
}

void SceneRenderer::drawMesh(GpuMesh& mesh, const Matrix& world, const Matrix& view,
                             const Matrix& projection, RenderStats& stats)
{
    effect_->setWorldProperty(world);
    effect_->setViewProperty(view);
    effect_->setProjectionProperty(projection);
    for (auto& pass : effect_->getCurrentTechniqueProperty()->getPassesProperty())
    {
        pass.Apply();
        mesh.draw(device_);
        ++stats.drawCalls;
        stats.triangles += mesh.triangles();
    }
}

void SceneRenderer::drawBox(const Matrix& world, const Matrix& view, const Matrix& projection,
                            const Color& color, RenderStats& stats)
{
    // A uniform-colour primitive must not tint the next vertex-coloured mesh (for example
    // the toilet door following a door-status button, or luggage following the staff band).
    const Vector3 previousDiffuse = effect_->getDiffuseColorProperty();
    effect_->setDiffuseColorProperty(rgb(color));
    drawMesh(*cube_, world, view, projection, stats);
    effect_->setDiffuseColorProperty(previousDiffuse);
}

void SceneRenderer::queue(const GpuMesh& mesh, const Matrix& world,
                          const CnaEngine::Graphics::Material& material, const Look& look,
                          RenderStats& stats)
{
    for (std::size_t index = 0; index < mesh.segmentCount(); ++index)
    {
        const CnaEngine::Graphics::GpuMesh& segment = mesh.segment(index);
        if (!segment.IsDrawable()) continue;
        CnaEngine::Graphics::RenderItem item;
        item.mesh = &segment;
        item.material = &material;
        item.world = world;
        item.userData = &look;
        queue_.Submit(item, CnaEngine::Graphics::RenderBucket::Sequenced);
    }
    ++stats.drawCalls;
    stats.triangles += mesh.triangles();
}

CnaEngine::Graphics::Material& SceneRenderer::frameMaterial()
{
    return frameMaterials_.emplace_back();
}

const SceneRenderer::Look& SceneRenderer::frameLook(const Look& look)
{
    return frameLooks_.emplace_back(look);
}

void SceneRenderer::render()
{
    queue_.Sort(camera_.Position()); // sequenced: submission order
    renderer_->Render(queue_, camera_);
    queue_.Clear();
    // The sky and the people draw on effect_ directly and expect it as the passes left it.
    effect_->setTextureProperty(whiteMaterialTexture_.get());
    effect_->setLightingEnabledProperty(true);
    effect_->setFogEnabledProperty(true);
    effect_->setDiffuseColorProperty(Vector3::One);
    effect_->setAlphaProperty(1.0f);
}

void SceneRenderer::queueStationClock(const Sim::Route& route, const Sim::ServiceState& service,
                                      RenderStats& stats)
{
    const std::size_t stationIndex = route.nearestStation(service.routeDistanceMetres);
    const Sim::Station& station = route.stations()[stationIndex];
    if ((!station.major && !hasMarketTownTurret(stationIndex))
        || std::abs(station.distanceMetres-service.routeDistanceMetres) > 260.0) return;

    // The Northern Valley service day begins at 09:17. Service time, rather than wall time,
    // keeps station clocks coherent with accelerated QA, a frozen train, and indefinite return
    // workings. The hour hand follows the minute hand continuously instead of jumping each hour.
    constexpr double startOfServiceSeconds = 9.0*3'600.0 + 17.0*60.0;
    constexpr double secondsPerDay = 24.0*3'600.0;
    constexpr float tau = 6.2831853071795864769f;
    const double timeOfDay = std::fmod(
        startOfServiceSeconds+std::max(0.0,service.serviceElapsedSeconds),secondsPerDay);
    const float minuteAngle = static_cast<float>(
        std::fmod(timeOfDay/60.0,60.0)/60.0) * tau;
    const float hourAngle = static_cast<float>(
        std::fmod(timeOfDay/3'600.0,12.0)/12.0) * tau;

    const float side = station.platformOnLeft ? -1.0f : 1.0f;
    const auto drawHand = [&](const LocalRoutePoint& pivot,float height,
                              float angle, float length, float width) {
        // Both clock faces point toward their running line. Mirroring the rotation for the
        // opposite platform side preserves clockwise motion as viewed from the train.
        const Matrix handWorld = Matrix::CreateScale(0.065f,length,width)
            * Matrix::CreateTranslation(0.0f,length*0.5f,0.0f)
            * Matrix::CreateRotationX(angle*side)
            * Matrix::CreateRotationY(pivot.yaw)
            * Matrix::CreateTranslation(pivot.x,pivot.y+height,pivot.z);
        CnaEngine::Graphics::Material& hand = frameMaterial();
        hand.vertexColors = true;
        hand.baseTexture = whiteMaterialTexture_.get();
        hand.sampler = &SamplerState::LinearClamp;
        hand.diffuseColor = rgb(Color(38,42,41,255));
        queue(*cube_,handWorld,hand,frameLook({}),stats);
        ++stats.stationClockHands;
    };
    if (station.major)
    {
        const auto pivot=localRoutePoint(route,service.routeDistanceMetres,
            station.distanceMetres,static_cast<double>(side)*4.92);
        drawHand(pivot,5.75f,hourAngle,0.34f,0.075f);
        drawHand(pivot,5.75f,minuteAngle,0.49f,0.052f);
    }
    if (hasMarketTownTurret(stationIndex))
    {
        const auto pivot=localRoutePoint(route,service.routeDistanceMetres,
            station.distanceMetres+24,static_cast<double>(side)*(12-marketTowerClockFaceDepth-0.05f));
        drawHand(pivot,marketTowerClockHeight,hourAngle,0.52f,0.08f);
        drawHand(pivot,marketTowerClockHeight,minuteAngle,0.73f,0.055f);
    }
}

RenderStats SceneRenderer::draw(const Matrix& view, const Matrix& projection, const Sim::Route& route,
                                const Sim::ServiceState& service, const World::StreamingWorld& world,
                                const Sim::PassengerSystem& passengers, double seconds,
                                double etaSeconds, double scheduleDeviationSeconds,
                                bool toiletDoorOpen, bool toiletOccupied,
                                float playerX, float playerZ, float cameraYaw)
{
    RenderStats stats;
    // Time of day and weather (ENV-138/139) follow simulated service time. At the default 09:17
    // clear start the key light, palette and fog equal the earlier fixed daylight exactly.
    const double clockHours = Sim::serviceClockHours(serviceStartMinutes_/60.0,
                                                     service.serviceElapsedSeconds);
    atmosphere_ = atmosphere(clockHours,Sim::weatherConditions(
        weatherMode_,service.serviceElapsedSeconds));
    const double routeHeading = route.sample(service.routeDistanceMetres).headingRadians;
    const Vector3 daylight = sunRayDirection(routeHeading,atmosphere_.keyLight);
    const Vector3 sunRays = sunRayDirection(routeHeading,atmosphere_.sun);
    stats.clockMinutes = static_cast<int>(clockHours*60.0) % (24*60);
    stats.atmosphereNight = atmosphere_.night;
    stats.atmosphereRain = atmosphere_.rain;
    stats.atmosphereOvercast = atmosphere_.overcast;
    stats.sunElevationDegrees = static_cast<float>(atmosphere_.sun.elevation*180.0/kPi);
    // New world windows bake their projected shadows from this light; whole-degree steps keep
    // prepared windows consistent without rebuilding anything already streamed.
    const SunPosition shadowSun = shadowCastingPosition(atmosphere_.sun);
    if (std::abs(shadowSun.elevation-worldSun_.elevation) > kPi/180.0
        || std::abs(shadowSun.azimuthOffset-worldSun_.azimuthOffset) > kPi/180.0)
        worldSun_ = shadowSun;
    // The world and the train are queued below in their drawing order; the binder applies this
    // frame's lights and fog to each draw.
    camera_.SetMatrices(view,projection);
    frameMaterials_.clear();
    frameLooks_.clear();
    frame_.daylight = daylight;
    using CnaEngine::Graphics::BlendMode;
    using CnaEngine::Graphics::Material;
    using CnaEngine::Graphics::ShadingModel;
    using Light = Look::Light;
    // A vertex-coloured batch on effect_ over the white material texture.
    struct Surface
    {
        bool lit = true;
        Vector3 diffuse = Vector3::One;
        float alpha = 1.0f;
        // AlphaBlend blends straight (non-premultiplied) colours; DepthRead for every blend.
        BlendMode blend = BlendMode::Opaque;
        bool doubleSided = false;
    };
    const auto queueVertex = [&](const GpuMesh& mesh, const Matrix& placement, Light light,
                                 const Surface& surface = {}) {
        Material& material = frameMaterial();
        material.shading = surface.lit ? ShadingModel::Lit : ShadingModel::Unlit;
        material.vertexColors = true;
        material.baseTexture = whiteMaterialTexture_.get();
        material.sampler = &SamplerState::LinearClamp;
        material.diffuseColor = surface.diffuse;
        material.alpha = surface.alpha;
        material.blend = surface.blend;
        material.straightAlpha = surface.blend == BlendMode::AlphaBlend;
        material.doubleSided = surface.doubleSided;
        queue(mesh,placement,material,frameLook({Look::Effect::Vertex,light}),stats);
    };
    const auto queueBox = [&](const Matrix& placement, const Color& color, Light light, bool lit = true) {
        queueVertex(*cube_,placement,light,{.lit = lit, .diffuse = rgb(color)});
    };
    // A detail-textured material on materialEffect_ (or a shader configured from it). Low
    // quality has no specular highlights.
    struct Detail
    {
        Texture2D* texture = nullptr;
        const SamplerState* sampler = &SamplerState::LinearClamp;
        Vector3 specular = Vector3::Zero;
        float power = 1.0f;
        Vector3 keySpecular = Vector3::Zero;
    };
    const auto queueMaterial = [&](const GpuMesh& mesh, const Matrix& placement, Light light,
                                   const Detail& detail,
                                   Look::Effect effect = Look::Effect::Material) {
        const bool specular = graphicsQuality_ != 0;
        Material& material = frameMaterial();
        material.vertexColors = true;
        material.baseTexture = detail.texture;
        material.sampler = detail.sampler;
        material.specularColor = specular ? detail.specular : Vector3::Zero;
        material.specularPower = specular ? detail.power : 1.0f;
        queue(mesh,placement,material,
              frameLook({effect,light,specular ? detail.keySpecular : Vector3::Zero}),stats);
    };
    // Terrain-scale detail wraps; High adds anisotropy.
    const SamplerState* const detailSampler = graphicsQuality_ >= 2
        ? &SamplerState::AnisotropicWrap : &SamplerState::LinearWrap;

    updateTrack(route,service);
    stats.trackRebuilds = trackRebuilds_;
    stats.trackPrefetchHits = trackStreaming_->prefetchHits;
    stats.trackSynchronousBuilds = trackStreaming_->synchronousBuilds;
    stats.trackLatePrefetches = trackStreaming_->latePrefetches;
    stats.trackGenerationMaxMs = trackGenerationMaxMs_;
    stats.trackRebuildTotalMs = trackRebuildTotalMs_;
    stats.trackRebuildMaxMs = trackRebuildMaxMs_;
    stats.tracksideKilometrePosts = tracksideKilometrePosts_;
    stats.trackFastenerAssemblies = trackFastenerAssemblies_;
    stats.turnoutPointMotors = trackTurnoutPointMotors_;
    stats.turnoutBladeAssemblies = trackTurnoutBladeAssemblies_;
    const double renderDelta = previousRenderSeconds_ < 0.0
        ? 0.0 : std::clamp(seconds - previousRenderSeconds_, 0.0, 0.1);
    previousRenderSeconds_ = seconds;
    const float tunnelTarget = route.sample(service.routeDistanceMetres).tunnel ? 1.0f : 0.0f;
    const float tunnelLightingStep = static_cast<float>(renderDelta) * 1.35f;
    tunnelLightingMix_ += std::clamp(tunnelTarget - tunnelLightingMix_,
                                     -tunnelLightingStep, tunnelLightingStep);
    const float doorTarget = service.doorsOpen ? 1.0f : 0.0f;
    const float doorStep = static_cast<float>(renderDelta)
                         / static_cast<float>(Sim::TrainInterior::PassengerDoorAnimationSeconds);
    doorOpenAmount_ += std::clamp(doorTarget - doorOpenAmount_, -doorStep, doorStep);
    // The four-second service interlock comfortably outlasts the 1.35-second animation at the
    // production time scale. Developer acceleration can traverse those simulated seconds in only
    // a few rendered frames, however. Never carry a partially open leaf or deployed bridge plate
    // into a moving phase; an accelerated QA run may snap the final fraction closed instead.
    if (service.phase == Sim::ServicePhase::Accelerating
        || service.phase == Sim::ServicePhase::Cruising
        || service.phase == Sim::ServicePhase::Coasting
        || service.phase == Sim::ServicePhase::Braking)
        doorOpenAmount_ = 0.0f;
    const bool glassPlatformNegativeX=Sim::platformOnNegativeTrainX(service);
    if (!passengerDoorGlass_ || passengerDoorGlassOpenness_!=doorOpenAmount_
        || passengerDoorGlassNegativeX_!=glassPlatformNegativeX)
    {
        if (!passengerDoorGlassScratch_)
        {
            passengerDoorGlassScratch_=std::make_unique<MeshBuilder>();
            passengerDoorGlassScratch_->reserve(2880,2880);
        }
        auto& panes=*passengerDoorGlassScratch_;
        panes.clear();
        for (int car=0;car<Sim::TrainInterior::PassengerCars;++car)
            for (float side : {-1.0f,1.0f})
                for (float panel : {-1.0f,1.0f})
                {
                    const bool platformSide=(side<0)==glassPlatformNegativeX;
                    const float openness=platformSide ? doorOpenAmount_ : 0.0f;
                    addPassengerDoorPane(panes,side*1.455f,1.17f,
                        static_cast<float>(Sim::TrainInterior::passengerDoorZ(car))
                            +panel*(0.33f+openness*0.63f));
                }
        if (passengerDoorGlass_) passengerDoorGlass_->replace(device_,panes.data());
        else passengerDoorGlass_=std::make_unique<GpuMesh>(device_,panes.data(),true);
        passengerDoorGlassOpenness_=doorOpenAmount_;
        passengerDoorGlassNegativeX_=glassPlatformNegativeX;
    }
    const float toiletDoorTarget = toiletDoorOpen ? 1.0f : 0.0f;
    const float toiletDoorStep = static_cast<float>(renderDelta) / 0.75f;
    toiletDoorOpenAmount_ += std::clamp(toiletDoorTarget - toiletDoorOpenAmount_,
                                        -toiletDoorStep, toiletDoorStep);
    const float gangwayDoorStep = static_cast<float>(renderDelta) / 0.55f;
    for (int boundary = 0; boundary < Sim::TrainInterior::GangwayCount; ++boundary)
    {
        bool sensorOccupied = Sim::TrainInterior::gangwaySensorContains(boundary, playerZ);
        if (!sensorOccupied)
            sensorOccupied = std::any_of(
                passengers.passengers().begin(),passengers.passengers().end(),
                [boundary](const Sim::Passenger& passenger) {
                    return passenger.role != Sim::PassengerRole::Driver
                        && !passenger.seat.has_value()
                        && Sim::TrainInterior::gangwaySensorContains(boundary, passenger.z);
                });
        const float target = sensorOccupied ? 1.0f : 0.0f;
        float& openness = gangwayDoorOpenAmounts_[static_cast<std::size_t>(boundary)];
        openness += std::clamp(target - openness,-gangwayDoorStep,gangwayDoorStep);
        if (openness > 0.85f) ++stats.openGangwayDoors;
    }

    // Ignore camera translation for distant atmosphere: walking through the consist must not
    // move clouds or push the sky beyond the far clip plane. World geometry still occludes it.
    Matrix atmosphereView=view;
    atmosphereView.setTranslationProperty(Vector3::Zero);
    effect_->setFogEnabledProperty(false);
    effect_->setLightingEnabledProperty(false);
    effect_->setDiffuseColorProperty(Vector3::One);
    device_.setRasterizerStateProperty(RasterizerState::CullNone);
    // The palette drifts with the clock and weather; a twilight glow also follows the sun's
    // azimuth as the route curves. Rebuilding the small sky batch is cheap and rare.
    const Vector3 towardSun{-sunRays.X,0.0f,-sunRays.Z};
    if (skyPaletteChanged(atmosphere_.sky,builtSkyPalette_)
        || (atmosphere_.sky.glow.X > 0.0f
            && Vector3::Dot(Vector3::Normalize(towardSun),
                            Vector3::Normalize(builtSkySun_+Vector3{1e-6f,0,0})) < 0.9994f))
    {
        MeshBuilder sky;
        addAtmosphereSky(sky,1'070.0f,atmosphere_.sky,towardSun);
        sky_->replace(device_,sky.data());
        builtSkyPalette_ = atmosphere_.sky;
        builtSkySun_ = towardSun;
        ++skyRebuilds_;
    }
    stats.skyRebuilds = skyRebuilds_;
    drawMesh(*sky_, Matrix::getIdentityProperty(), atmosphereView, projection, stats);
    const auto drawDisc = [&](const Vector3& rays, float scale, float visibility,
                              const Vector3& tint) {
        if (visibility <= 0.01f || rays.Y >= 0.02f) return;
        effect_->setDiffuseColorProperty(tint);
        const bool blended = visibility < 1.0f;
        if (blended)
        {
            effect_->setAlphaProperty(visibility);
            device_.setBlendStateProperty(BlendState::NonPremultiplied);
        }
        drawMesh(*sun_, Matrix::CreateScale(scale)
                        * Matrix::CreateRotationX(-std::asin(-rays.Y))
                        * Matrix::CreateRotationY(std::atan2(-rays.X,-rays.Z))
                        * Matrix::CreateTranslation(rays*-1000.0f),
                 atmosphereView, projection, stats);
        if (blended)
        {
            effect_->setAlphaProperty(1.0f);
            device_.setBlendStateProperty(BlendState::Opaque);
        }
        effect_->setDiffuseColorProperty(Vector3::One);
    };
    drawDisc(sunRays,8.0f,atmosphere_.sunDisc,atmosphere_.sunTint);
    drawDisc(sunRayDirection(routeHeading,moonPosition(atmosphere_.sun)),5.0f,
             atmosphere_.moonDisc,Vector3{0.80f,0.83f,0.90f});
    effect_->setTextureProperty(cloudDetailTexture_.get());
    device_.getSamplerStatesProperty()[0] = SamplerState::LinearClamp;
    device_.setBlendStateProperty(BlendState::NonPremultiplied);
    device_.setDepthStencilStateProperty(DepthStencilState::DepthRead);
    // The camera is at the sky centre, but floating-origin and future renderer
    // conventions must not make a single-sided atmospheric card disappear.
    device_.setRasterizerStateProperty(RasterizerState::CullNone);
    // Heavier cloud repeats the deterministic layer at turned headings; tint follows the light.
    effect_->setDiffuseColorProperty(atmosphere_.cloudTint);
    const int cloudLayers = atmosphere_.cloudCover > 0.75f ? 3 : atmosphere_.cloudCover > 0.35f ? 2 : 1;
    for (int layer = 0; layer < cloudLayers; ++layer)
        drawMesh(*clouds_,Matrix::CreateRotationY(static_cast<float>(layer)*2.39996323f),
                 atmosphereView,projection,stats);
    effect_->setDiffuseColorProperty(Vector3::One);
    stats.cloudLayers = cloudLayers;
    stats.skyCloudBillboards = skyCloudBillboardCount_;
    device_.setBlendStateProperty(BlendState::Opaque);
    device_.setDepthStencilStateProperty(DepthStencilState::Default);
    device_.setRasterizerStateProperty(RasterizerState::CullCounterClockwise);
    effect_->setTextureProperty(whiteMaterialTexture_.get());
    effect_->setLightingEnabledProperty(true);
    effect_->setFogEnabledProperty(true);

    // Outdoor illumination fades at tunnel portals while the train's own interior lighting is
    // restored below. Tunnel fixtures remain visible through the deliberately non-black ambient.
    frame_.outdoor = exteriorLighting(tunnelLightingMix_,atmosphere_.light);
    frame_.bounceDirection = groundBounceDirection(daylight);
    frame_.uplightDirection = groundUplightDirection();
    // Platform lamps light the paving after dark (ENV-138).
    frame_.floodlit = exteriorLighting(tunnelLightingMix_,stationLighting(atmosphere_));
    frame_.fogColour = atmosphere_.fogColour*(1.0f-tunnelLightingMix_)
        +Vector3{0.075f,0.082f,0.090f}*tunnelLightingMix_;
    frame_.fogStart = atmosphere_.fogStart;
    frame_.fogEnd = atmosphere_.fogEnd;
    // The water's analytic reflection scales with the atmosphere's zenith (ENV-138/139).
    const Vector3 zenith = atmosphere_.sky.zenith;
    frame_.waterSky = Vector3{0.35f*(zenith.X/69.0f),0.59f*(zenith.Y/136.0f),
                              0.76f*(zenith.Z/198.0f)};
    frame_.sunSparkle = atmosphere_.direct*atmosphere_.sunDisc;
    frame_.serviceSeconds = service.serviceElapsedSeconds;

    // Recent travel per rendered frame, a decaying maximum so acceleration is seen at once.
    // Teleports and the first frame do not count as travel.
    if (lastStreamingDistance_ >= 0.0)
    {
        const double travelled = std::abs(service.routeDistanceMetres-lastStreamingDistance_);
        if (travelled < 100.0)
            streamingMetresPerFrame_ = std::max(travelled,streamingMetresPerFrame_*0.9);
    }
    lastStreamingDistance_ = service.routeDistanceMetres;
    updateStaticWorld(route, world, service);
    stats.worldPrefetchHits = worldStreaming_->prefetchHits;
    stats.worldSynchronousBuilds = worldStreaming_->synchronousBuilds;
    stats.worldLatePrefetches = worldStreaming_->latePrefetches;
    stats.worldRebuilds = worldRebuilds_;
    stats.worldBufferReuses = worldBufferReuses_;
    stats.routePointCacheHits = routePointCacheHits_;
    stats.routePointCacheMisses = routePointCacheMisses_;
    stats.worldRebuildTotalMs = worldRebuildTotalMs_;
    stats.worldRebuildMaxMs = worldRebuildMaxMs_;
    stats.worldGenerationTotalMs = worldGenerationTotalMs_;
    stats.worldGenerationMaxMs = worldGenerationMaxMs_;
    stats.worldUploadTotalMs = worldUploadTotalMs_;
    stats.worldUploadMaxMs = worldUploadMaxMs_;
    effect_->setDiffuseColorProperty(Vector3::One);
    const LocalRoutePoint worldAnchor = localRoutePoint(
        route, service.routeDistanceMetres, worldAnchorDistance_);
    const Matrix worldPlacement = Matrix::CreateRotationY(worldAnchor.yaw)
                                * Matrix::CreateTranslation(worldAnchor.x, worldAnchor.y, worldAnchor.z);
    // The dot-matrix font over its own unlit effect, blended without writing depth.
    const auto queuePrintedText=[&](const std::unique_ptr<GpuMesh>& lettering,const Matrix& placement) {
        if (!lettering) return;
        Material& material = frameMaterial();
        material.shading = ShadingModel::Unlit;
        material.blend = BlendMode::AlphaBlend;
        material.straightAlpha = true;
        material.vertexColors = true;
        material.baseTexture = passengerDisplayFontTexture_.get();
        material.sampler = &SamplerState::LinearClamp;
        material.fog = false;
        queue(*lettering,placement,material,frameLook({Look::Effect::Lettering}),stats);
    };
    if (terrain_)
    {
        // Terrain alone needs wrapping and anisotropy. Constant-UV neutral materials use the
        // stock clamp sampler; leaving the anisotropic sampler installed makes zero-derivative
        // lookups render black or striped on Mesa llvmpipe and is needless on hardware too.
        queueMaterial(*terrain_,worldPlacement,Light::Outdoor,
                      {.texture = terrainDetailTexture_.get(), .sampler = detailSampler});
        stats.terrainMaterialTriangles = terrain_->triangles();
    }
    if (foliageMaterial_)
    {
        queueMaterial(*foliageMaterial_,worldPlacement,Light::Outdoor,
                      {.texture = foliageDetailTexture_.get(), .sampler = &SamplerState::LinearWrap});
        stats.foliageMaterialTriangles = foliageMaterial_->triangles();
    }
    if (needleMaterial_)
    {
        queueMaterial(*needleMaterial_,worldPlacement,Light::Outdoor,
                      {.texture = needleDetailTexture_.get(), .sampler = &SamplerState::LinearWrap});
        stats.foliageMaterialTriangles += needleMaterial_->triangles();
    }
    if (leafSprays_ || needleSprays_)
    {
        // Spray vertex colours carry the reference daylight; scale them with the atmosphere's
        // open-air light so canopies darken at dusk, at night and under cloud (ENV-138/139).
        const ExteriorLighting reference = exteriorLighting(0.0f);
        const ExteriorLighting& air = atmosphere_.light;
        const auto ratio = [](float now, float base) { return std::clamp(now/base,0.0f,1.4f); };
        const Vector3 sprayTint = (air.ambient == reference.ambient && air.diffuse == reference.diffuse)
            ? Vector3::One
            : Vector3{ratio(air.ambient.X+0.6f*air.diffuse.X,reference.ambient.X+0.6f*reference.diffuse.X),
                      ratio(air.ambient.Y+0.6f*air.diffuse.Y,reference.ambient.Y+0.6f*reference.diffuse.Y),
                      ratio(air.ambient.Z+0.6f*air.diffuse.Z,reference.ambient.Z+0.6f*reference.diffuse.Z)};
        // Alpha-tested cards, fogged like the materials, drawn where they stand in the order.
        for (const auto& [sprayMesh,sprayTexture] : std::array{
                 std::pair{leafSprays_.get(),leafSprayTexture_.get()},
                 std::pair{needleSprays_.get(),needleSprayTexture_.get()}})
        {
            if (!sprayMesh) continue;
            Material& material = frameMaterial();
            material.shading = ShadingModel::Unlit;
            material.blend = BlendMode::AlphaTest;
            material.vertexColors = true;
            material.baseTexture = sprayTexture;
            material.sampler = &SamplerState::LinearClamp;
            material.diffuseColor = sprayTint;
            queue(*sprayMesh,worldPlacement,material,frameLook({Look::Effect::Spray}),stats);
            stats.foliageMaterialTriangles+=sprayMesh->triangles();
        }
    }
    if (roofMaterial_)
        queueMaterial(*roofMaterial_,worldPlacement,Light::Outdoor,
                      {.texture = roofDetailTexture_.get(), .sampler = &SamplerState::LinearWrap});
    if (waterMaterial_)
    {
        // Medium/High use filtered animated normals and analytic sky Fresnel in one pass.
        // Low, or an unavailable custom effect, retains the stock textured material.
        const Detail water{.texture = waterDetailTexture_.get(), .sampler = detailSampler,
                           .specular = Vector3{0.34f,0.42f,0.48f}, .power = 72.0f,
                           .keySpecular = Vector3{0.62f,0.62f,0.54f}};
        if (graphicsQuality_>=1 && !waterEffect_)
            waterEffect_=std::make_unique<OpenGL33WaterEffect>(device_);
        if (graphicsQuality_>=1 && waterEffect_->valid())
        {
            queueMaterial(*waterMaterial_,worldPlacement,Light::Outdoor,water,Look::Effect::Water);
            ++stats.waterShaderDraws;
        }
        else
            queueMaterial(*waterMaterial_,worldPlacement,Light::Outdoor,water);
        stats.waterMaterialTriangles = waterMaterial_->triangles();
    }
    if (platformMaterial_)
    {
        // Platform lamps light the paving after dark (ENV-138).
        queueMaterial(*platformMaterial_,worldPlacement,
                      atmosphere_.night > 0.0f ? Light::Floodlit : Light::Outdoor,
                      {.texture = concreteDetailTexture_.get(), .sampler = detailSampler,
                       .specular = Vector3{0.055f,0.055f,0.05f}, .power = 18.0f,
                       .keySpecular = Vector3{0.30f,0.29f,0.25f}});
        stats.platformMaterialTriangles = platformMaterial_->triangles();
    }
    if (facadeMaterial_)
    {
        const Detail masonry{.texture = masonryDetailTexture_.get(), .sampler = detailSampler,
                             .specular = Vector3{0.035f,0.033f,0.03f}, .power = 14.0f,
                             .keySpecular = Vector3{0.25f,0.23f,0.20f}};
        if (facadeLightingMode_>0)
        {
            if (!facadeEffect_) facadeEffect_=std::make_unique<OpenGL33FacadeEffect>(device_);
            queueMaterial(*facadeMaterial_,worldPlacement,Light::Outdoor,masonry,Look::Effect::Facade);
            ++stats.facadeShaderDraws;
        }
        else queueMaterial(*facadeMaterial_,worldPlacement,Light::Outdoor,masonry);
        stats.facadeMaterialTriangles = facadeMaterial_->triangles();
    }
    if (renderedFacadeMaterial_)
    {
        const Detail plaster{.texture = renderedFacadeDetailTexture_.get(), .sampler = detailSampler,
                             .specular = Vector3{0.022f,0.022f,0.020f}, .power = 11.0f,
                             .keySpecular = Vector3{0.22f,0.21f,0.19f}};
        if (facadeLightingMode_>0)
        {
            if (!facadeEffect_) facadeEffect_=std::make_unique<OpenGL33FacadeEffect>(device_);
            queueMaterial(*renderedFacadeMaterial_,worldPlacement,Light::Outdoor,plaster,
                          Look::Effect::Facade);
            ++stats.facadeShaderDraws;
        }
        else queueMaterial(*renderedFacadeMaterial_,worldPlacement,Light::Outdoor,plaster);
        stats.renderedFacadeMaterialTriangles = renderedFacadeMaterial_->triangles();
    }
    if (staticWorldShadows_)
    {
        // Baked shadows fade with a low sun and under cloud (ENV-138/139).
        queueVertex(*staticWorldShadows_,worldPlacement,Light::Outdoor,
                    {.lit = false, .alpha = std::min(atmosphere_.shadowStrength,1.0f),
                     .blend = BlendMode::AlphaBlend});
        stats.staticContactShadowTriangles = staticWorldShadows_->triangles();
    }
    queueVertex(*staticWorld_,worldPlacement,Light::Outdoor);
    queuePrintedText(stationLettering_,worldPlacement);
    stats.lakesideWaterPatches = worldCounters_.staticWorldLakePatches_;
    stats.lakesideBankTrees = worldCounters_.staticWorldLakeBankTrees_;
    stats.hayBaleStacks = worldCounters_.staticWorldHayBaleStacks_;
    stats.riverWaterSections = worldCounters_.staticWorldRiverWaterSections_;
    stats.riverBankClumps = worldCounters_.staticWorldRiverBankClumps_;
    stats.stationWaterfrontSections = worldCounters_.staticWorldStationWaterfrontSections_;
    stats.catenaryDroppers = worldCounters_.staticWorldCatenaryDroppers_;
    stats.catenaryFoundations = worldCounters_.staticWorldCatenaryFoundations_;
    stats.catenaryMessengerSegments = worldCounters_.staticWorldCatenaryMessengerSegments_;
    stats.catenaryRegistrationAssemblies = worldCounters_.staticWorldCatenaryRegistrationAssemblies_;
    stats.linesideEquipmentCabinets = worldCounters_.staticWorldLinesideEquipmentCabinets_;
    stats.tunnelLuminaires = worldCounters_.staticWorldTunnelLuminaires_;
    stats.tunnelSafetyNiches = worldCounters_.staticWorldTunnelSafetyNiches_;
    stats.tunnelLiningRings = worldCounters_.staticWorldTunnelLiningRings_;
    stats.tunnelMarkerPlates = worldCounters_.staticWorldTunnelMarkerPlates_;
    stats.bridgeSections = worldCounters_.staticWorldBridgeSections_;
    stats.riverBridgeSections = worldCounters_.staticWorldRiverBridgeSections_;
    stats.viaductSections = worldCounters_.staticWorldViaductSections_;
    stats.bridgeAbutments = worldCounters_.staticWorldBridgeAbutments_;
    stats.terminalBufferStops = worldCounters_.staticWorldTerminalBufferStops_;
    stats.terminalShedColumns = worldCounters_.staticWorldTerminalShedColumns_;
    stats.bridgeParapetPosts = worldCounters_.staticWorldBridgeParapetPosts_;
    stats.fencePosts = worldCounters_.staticWorldFencePosts_;
    stats.cableTroughUnits = worldCounters_.staticWorldCableTroughUnits_;
    stats.vergeFlowerClumps = worldCounters_.staticWorldVergeFlowerClumps_;
    stats.palisadePosts = worldCounters_.staticWorldPalisadePosts_;
    stats.houseOutbuildings = worldCounters_.staticWorldHouseOutbuildings_;
    stats.houseRearWings = worldCounters_.staticWorldHouseRearWings_;
    stats.blockPenthouses = worldCounters_.staticWorldBlockPenthouses_;
    stats.platformAmenities = worldCounters_.staticWorldPlatformAmenities_;
    stats.blockBalconies = worldCounters_.staticWorldBlockBalconies_;
    stats.canopyValanceBoards = worldCounters_.staticWorldCanopyValanceBoards_;
    stats.tramlineSegments = worldCounters_.staticWorldTramlineSegments_;
    stats.worldGpuMegabytes = static_cast<double>(worldCounters_.worldGpuBytes_)/1048576.0;
    stats.largestWorldMaterialMegabytes = static_cast<double>(worldCounters_.largestMaterialBytes_)/1048576.0;
    stats.largestWorldMaterialVertices = static_cast<double>(worldCounters_.largestMaterialVertices_);
    stats.largestWorldMaterial = worldCounters_.largestMaterial_;
    stats.worldGpuSegments = worldCounters_.worldGpuSegments_;
    stats.fieldBoundaryTrees = worldCounters_.staticWorldFieldBoundaryTrees_;
    stats.platformSurfaceJoints = worldCounters_.staticWorldPlatformSurfaceJoints_;
    stats.platformCanopyLights = worldCounters_.staticWorldPlatformCanopyLights_;
    stats.platformCanopyDrainageAssemblies =
        worldCounters_.staticWorldPlatformCanopyDrainageAssemblies_;
    stats.platformPlanters = worldCounters_.staticWorldPlatformPlanters_;
    stats.platformPosterCases = worldCounters_.staticWorldPlatformPosterCases_;
    stats.platformEquipmentCabinets = worldCounters_.staticWorldPlatformEquipmentCabinets_;
    stats.platformBenchAssemblies = worldCounters_.staticWorldPlatformBenchAssemblies_;
    stats.platformWeatheringMarks = worldCounters_.staticWorldPlatformWeatheringMarks_;
    stats.stationFacadeWeatheringStreaks =
        worldCounters_.staticWorldStationFacadeWeatheringStreaks_;
    stats.platformBoundarySections = worldCounters_.staticWorldPlatformBoundarySections_;
    stats.tracksideDrainageSections = worldCounters_.staticWorldTracksideDrainageSections_;
    stats.tracksideVergeTufts = worldCounters_.staticWorldTracksideVergeTufts_;
    stats.fieldBoundarySegments = worldCounters_.staticWorldFieldBoundarySegments_;
    stats.distantWoodlandTrees = worldCounters_.staticWorldDistantWoodlandTrees_;
    stats.distantRidgeMasses = worldCounters_.staticWorldDistantRidgeMasses_;
    stats.treeBranchCues = worldCounters_.staticWorldTreeBranchCues_;
    stats.coniferBranchTiers = worldCounters_.staticWorldConiferBranchTiers_;
    stats.understoreyGroups = worldCounters_.staticWorldUnderstoreyGroups_;
    stats.distantSettlementBuildings = worldCounters_.staticWorldDistantSettlementBuildings_;
    stats.buildingEndWallWindows = worldCounters_.staticWorldBuildingEndWallWindows_;
    stats.towerEndWallWindows = worldCounters_.staticWorldTowerEndWallWindows_;
    stats.detailedWarehouses = worldCounters_.staticWorldDetailedWarehouses_;
    stats.industrialCompounds = worldCounters_.staticWorldIndustrialCompounds_;
    stats.industrialYardVehicles = worldCounters_.staticWorldIndustrialYardVehicles_;
    stats.industrialTrailers = worldCounters_.staticWorldIndustrialTrailers_;
    stats.industrialUtilityGroups = worldCounters_.staticWorldIndustrialUtilityGroups_;
    stats.industrialEdgePlantingGroups = worldCounters_.staticWorldIndustrialEdgePlantingGroups_;
    stats.urbanRoadEdgeMarkingSections = worldCounters_.staticWorldUrbanRoadEdgeMarkingSections_;
    stats.urbanRoadDrainGrates = worldCounters_.staticWorldUrbanRoadDrainGrates_;
    stats.urbanResidentialPlotSurfaces = worldCounters_.staticWorldUrbanResidentialPlotSurfaces_;
    stats.urbanStreetPockets = worldCounters_.staticWorldUrbanStreetPockets_;
    stats.urbanResidentialEntrancePaths = worldCounters_.staticWorldUrbanResidentialEntrancePaths_;
    if (worldLuminaires_)
        queueVertex(*worldLuminaires_,worldPlacement,Light::Outdoor,{.lit = false});
    // Lit windows, lamp heads and light pools fade in after sunset (ENV-138).
    if (worldNightGlow_ && atmosphere_.night > 0.01f)
    {
        queueVertex(*worldNightGlow_,worldPlacement,Light::Outdoor,
                    {.lit = false, .alpha = atmosphere_.night, .blend = BlendMode::Additive,
                     .doubleSided = true});
        stats.nightGlowTriangles = worldNightGlow_->triangles();
    }

    // One live departure board belongs to the platform currently being approached or occupied.
    // It is a small independent batch because its destination and punctuality must survive
    // direction reversals without rebuilding the seven-chunk scenery mesh.
    const std::size_t platformInformationStation =
        (service.phase == Sim::ServicePhase::Dwelling
         || service.phase == Sim::ServicePhase::ClosingDoors)
        ? service.currentStation : service.nextStation;
    const Sim::Station& informationStation = route.stations()[platformInformationStation];
    if (std::abs(informationStation.distanceMetres - service.routeDistanceMetres) < 900.0)
    {
        const int platformEta = std::clamp(
            static_cast<int>(std::ceil(std::max(0.0, etaSeconds) / 60.0)), 0, 99);
        const PlatformInformationState platformState{
            platformInformationStation, service.direction, static_cast<int>(service.phase),
            platformEta,
            static_cast<int>(std::round(scheduleDeviationSeconds / 60.0))};
        if (!platformInformation_ || platformInformationState_ != platformState)
            rebuildPlatformInformation(route,service,platformInformationStation,platformEta,
                                       scheduleDeviationSeconds);
        const LocalRoutePoint placement = localRoutePoint(
            route,service.routeDistanceMetres,informationStation.distanceMetres);
        const Matrix platformWorld = Matrix::CreateRotationY(placement.yaw)
                                   * Matrix::CreateTranslation(
                                       placement.x,placement.y,placement.z);
        queueVertex(*platformInformation_,platformWorld,Light::Outdoor,{.lit = false});
        queuePrintedText(platformLettering_,platformWorld);
        ++stats.platformInformationBoards;
    }

    // Oncoming and parked consists (ENV-137) draw one complete mesh per vehicle. The leading
    // vehicle sits at the lowest route distance because these services travel toward it.
    const auto drawOtherConsist = [&](const World::OtherTrainConsist& consist,
                                      double centreDistance, float range,
                                      const auto& lateralAt) {
        for (std::size_t index = 0; index < consist.vehicles.size(); ++index)
        {
            const World::OtherTrainVehicle& vehicle = consist.vehicles[index];
            const double distance = centreDistance - consist.leadOffsetMetres(index);
            if (distance < 0.0 || distance > route.lengthMetres()) continue;
            const LocalRoutePoint point = localRoutePoint(
                route,service.routeDistanceMetres,distance,lateralAt(distance));
            if (std::abs(point.z) > range + static_cast<float>(vehicle.lengthMetres*0.5))
                continue;
            const Matrix vehicleWorld = Matrix::CreateRotationY(point.yaw + kPi)
                                      * Matrix::CreateTranslation(point.x,point.y,point.z);
            queueVertex(*otherVehicles_[static_cast<std::size_t>(vehicle.kind)],vehicleWorld,
                        Light::Outdoor);
            if (GpuMesh* glow = otherVehicleNight_[static_cast<std::size_t>(vehicle.kind)].get();
                glow != nullptr && atmosphere_.night > 0.05f)
            {
                queueVertex(*glow,vehicleWorld,Light::Outdoor,
                            {.lit = false, .alpha = atmosphere_.night, .blend = BlendMode::Additive});
                ++stats.otherTrainNightOverlays;
            }
            ++stats.otherTrainCars;
            stats.otherTrainCabEnds += vehicle.cabEnds;
            if (otherVehicleIsLocomotive(vehicle.kind)) ++stats.otherTrainLocomotives;
            if (otherVehicleIsFreightWagon(vehicle.kind)) ++stats.otherTrainFreightWagons;
        }
    };

    const bool lampsGlow = atmosphere_.night > 0.05f;
    // Only genuinely moving scenery remains outside the static batch.
    const float dynamicRange = graphicsQuality_ == 0 ? 450.0f
                             : graphicsQuality_ == 1 ? 650.0f : 850.0f;
    for (const World::Chunk& chunk : world.chunks())
    {
        for (const World::WorldFeature& feature : chunk.features)
        {
            if (feature.kind != World::FeatureKind::RoadVehicle
                && feature.kind != World::FeatureKind::Signal
                && feature.kind != World::FeatureKind::LevelCrossing) continue;
            const Sim::RoutePose featurePose = route.sample(feature.routeDistance);
            if ((featurePose.tunnel || featurePose.bridge)
                && feature.kind == World::FeatureKind::RoadVehicle) continue;
            const LocalRoutePoint point = localRoutePoint(
                route, service.routeDistanceMetres, feature.routeDistance, feature.lateralMetres);
            const float featureRange = feature.kind == World::FeatureKind::LevelCrossing
                ? 900.0f : dynamicRange;
            if (std::abs(point.z) > featureRange) continue;
            const float scale = static_cast<float>(feature.scale);
            if (feature.kind == World::FeatureKind::Signal)
            {
                const int facingDirection = World::signalFacingDirection(feature);
                const World::SignalAspect signalAspect = World::signalAspect(feature, route, service);
                Color aspect;
                float lensHeight = 4.10f;
                switch (signalAspect)
                {
                    case World::SignalAspect::Red:
                        aspect = Color(215,49,39,255);
                        lensHeight = 4.42f;
                        ++stats.redSignals;
                        break;
                    case World::SignalAspect::Yellow:
                        aspect = Color(235,182,36,255);
                        lensHeight = 4.10f;
                        ++stats.yellowSignals;
                        break;
                    case World::SignalAspect::Green:
                        aspect = Color(45,188,75,255);
                        lensHeight = 3.78f;
                        ++stats.greenSignals;
                        break;
                }

                const double lensLateral = feature.lateralMetres
                    - std::copysign(0.20, feature.lateralMetres);
                const LocalRoutePoint lens = localRoutePoint(
                    route, service.routeDistanceMetres,
                    feature.routeDistance - 0.27 * static_cast<double>(facingDirection),
                    lensLateral);
                // Lit aspects glow unshaded after dark (ENV-138).
                queueBox(orientedBoxMatrix(lens.x,lens.y+lensHeight,lens.z,
                                           0.18f,0.18f,0.09f,
                                           lens.yaw + (facingDirection < 0 ? kPi : 0.0f)),
                         aspect,Light::Outdoor,!lampsGlow);
            }
            else if (feature.kind == World::FeatureKind::LevelCrossing)
            {
                const double requiredClosure = world.requiredLevelCrossingClosure(
                    feature.routeDistance,service.routeDistanceMetres,seconds);
                const float closure = static_cast<float>(requiredClosure);
                const float barrierAngle = (1.0f-closure)*1.309f;
                for (double side : {-1.0,1.0})
                {
                    const LocalRoutePoint pivot = localRoutePoint(
                        route,service.routeDistanceMetres,feature.routeDistance-4.2,
                        side*8.3);
                    const Matrix barrierWorld = Matrix::CreateRotationX(-barrierAngle)
                        * Matrix::CreateRotationY(pivot.yaw)
                        * Matrix::CreateTranslation(pivot.x,pivot.y+1.55f,pivot.z);
                    queueVertex(*levelCrossingBarrier_,barrierWorld,Light::Outdoor);
                    ++stats.levelCrossingBarriers;
                }

                if (closure > 0.0f)
                {
                    const double lensOffset = (static_cast<int>(seconds*2.2) & 1) == 0
                        ? -0.18 : 0.18;
                    for (double side : {-1.0,1.0})
                    {
                        const LocalRoutePoint lens = localRoutePoint(
                            route,service.routeDistanceMetres,
                            feature.routeDistance-4.2+lensOffset,side*8.44);
                        queueBox(orientedBoxMatrix(lens.x,lens.y+2.15f,lens.z,
                                                   0.055f,0.16f,0.16f,lens.yaw),
                                 Color(239,49,39,255),Light::Outdoor,!lampsGlow);
                        ++stats.levelCrossingWarningLights;
                    }
                }
            }
            else
            {
                const float direction = feature.variant % 2 == 0 ? 1.0f : -1.0f;
                const float motion = std::fmod(static_cast<float>(seconds)
                                               * (7.0f + static_cast<float>(feature.variant))
                                               + static_cast<float>(feature.variant) * 13.0f,
                                               90.0f) - 45.0f;
                const double vehicleDistance = std::clamp(
                    feature.routeDistance + static_cast<double>(motion * direction),
                    0.0, route.lengthMetres());
                const LocalRoutePoint vehiclePoint = localRoutePoint(
                    route, service.routeDistanceMetres, vehicleDistance, feature.lateralMetres);
                const Matrix vehicleWorld = Matrix::CreateScale(scale)
                    * Matrix::CreateRotationY(vehiclePoint.yaw + (direction < 0.0f ? kPi : 0.0f))
                    * Matrix::CreateTranslation(vehiclePoint.x,vehiclePoint.y-0.20f,
                                                vehiclePoint.z);
                queueVertex(*roadVehicles_[feature.variant % roadVehicles_.size()],vehicleWorld,
                            Light::Outdoor);
                ++stats.roadVehicles;
            }
        }
    }

    // Moving trains come from the wider traffic window and are culled per vehicle; their
    // deterministic anchor may be nearly a kilometre from a currently visible pass.
    for (const World::WorldFeature& feature : world.otherTrains())
    {
        if (!World::movingOtherTrainVisible(feature,seconds,route.lengthMetres())) continue;
        const double otherDistance = World::movingOtherTrainDistance(
            feature,seconds,route.lengthMetres());
        drawOtherConsist(World::otherTrainConsist(World::otherTrainType(feature.variant)),
                         otherDistance,dynamicRange,[&route](double distance) {
                             return route.parallelTrackLateralMetres(distance);
                         });
    }

    // Major stations may hold one short parked service on their authored siding: a regional unit
    // at even-numbered stations and a diesel unit at odd ones, without dispatch AI.
    const std::size_t nearbyStationIndex = route.nearestStation(service.routeDistanceMetres);
    const Sim::Station& nearbyStation = route.stations()[nearbyStationIndex];
    if (graphicsQuality_ >= 1 && nearbyStation.major
        && std::abs(nearbyStation.distanceMetres - service.routeDistanceMetres) < 720.0)
    {
        double parkedCentre = nearbyStation.distanceMetres;
        if (nearbyStationIndex == 0) parkedCentre += 48.0;
        else if (nearbyStationIndex + 1 == route.stations().size()) parkedCentre -= 48.0;
        drawOtherConsist(World::otherTrainConsist(nearbyStationIndex % 2 == 0
                                                       ? World::OtherTrainType::RegionalEmu
                                                       : World::OtherTrainType::DieselUnit),
                         parkedCentre,850.0f,[&route](double distance) {
                             return stationSidingLateral(route,distance)
                                 .value_or(route.parallelTrackLateralMetres(distance));
                         });
    }

    // Ballast, rails and the dense sleeper field are one batch. Only the small sleeper repeat
    // offset changes, which avoids hundreds of tiny draw calls per frame.
    const LocalRoutePoint trackAnchor = localRoutePoint(
        route, service.routeDistanceMetres, trackAnchorDistance_);
    const Matrix trackWorld = Matrix::CreateRotationY(trackAnchor.yaw)
                            * Matrix::CreateTranslation(trackAnchor.x, trackAnchor.y, trackAnchor.z);
    if (trackBedMaterial_)
    {
        queueMaterial(*trackBedMaterial_,trackWorld,Light::Outdoor,
                      {.texture = ballastDetailTexture_.get(), .sampler = detailSampler});
        stats.ballastMaterialTriangles = trackBedMaterial_->triangles();
    }
    if (railHeadMaterial_)
    {
        queueMaterial(*railHeadMaterial_,trackWorld,Light::Outdoor,
                      {.texture = whiteMaterialTexture_.get(), .sampler = &SamplerState::LinearClamp,
                       .specular = Vector3{0.34f,0.36f,0.35f}, .power = 64.0f,
                       .keySpecular = Vector3{0.56f,0.54f,0.48f}});
        stats.railHeadMaterialTriangles = railHeadMaterial_->triangles();
    }
    queueVertex(*trackDetail_,trackWorld,Light::Outdoor);
    render();

    // Draw people before the train shell. The opaque interior can then mask bodies behind seat
    // backs, toilet walls, and cab doors while the final transparent-glass pass keeps the intended
    // window views. Explicit state restoration contains any AvatarRenderer state changes.
    drawAvatars(view, projection, passengers, seconds,
                static_cast<float>(renderDelta), playerX, playerZ, stats);
    drawPlatformPeople(view, projection, route, service, stats);
    device_.setBlendStateProperty(BlendState::Opaque);
    device_.setDepthStencilStateProperty(DepthStencilState::Default);
    queueStationClock(route,service,stats);

    // The passenger saloon is independently lit (Light::Saloon), so it stays comfortable and
    // readable while the scenery beyond the windows transitions through a tunnel.
    const Matrix trainWorld = Matrix::CreateTranslation(0.0f,kTrainFloorElevation,0.0f);
    if (interiorMaterial_)
    {
        // Passenger-space materials use the independent interior illumination even in tunnels.
        queueMaterial(*interiorMaterial_,trainWorld,Light::Saloon,
                      {.texture = carpetDetailTexture_.get(), .sampler = detailSampler,
                       .specular = Vector3{0.018f,0.018f,0.016f}, .power = 8.0f,
                       .keySpecular = Vector3{0.22f,0.21f,0.18f}});
        stats.interiorMaterialTriangles = interiorMaterial_->triangles();
    }
    if (upholsteryMaterial_)
    {
        queueMaterial(*upholsteryMaterial_,trainWorld,Light::Saloon,
                      {.texture = upholsteryDetailTexture_.get(), .sampler = detailSampler,
                       .specular = Vector3{0.012f,0.012f,0.012f}, .power = 6.0f,
                       .keySpecular = Vector3{0.20f,0.20f,0.20f}});
        stats.interiorMaterialTriangles += upholsteryMaterial_->triangles();
    }
    if (interiorPanelMaterial_)
    {
        queueMaterial(*interiorPanelMaterial_,trainWorld,Light::Saloon,
                      {.texture = interiorPanelDetailTexture_.get(), .sampler = detailSampler,
                       .specular = Vector3{0.12f,0.12f,0.105f}, .power = 42.0f,
                       .keySpecular = Vector3{0.38f,0.36f,0.31f}});
        stats.interiorPanelMaterialTriangles = interiorPanelMaterial_->triangles();
    }
    if (interiorResilientMaterial_)
    {
        queueMaterial(*interiorResilientMaterial_,trainWorld,Light::Saloon,
                      {.texture = resilientFloorDetailTexture_.get(), .sampler = detailSampler,
                       .specular = Vector3{0.075f,0.075f,0.068f}, .power = 24.0f,
                       .keySpecular = Vector3{0.31f,0.30f,0.26f}});
        stats.interiorResilientMaterialTriangles = interiorResilientMaterial_->triangles();
    }
    if (graphicsQuality_ >= 1 && interiorShadows_)
    {
        queueVertex(*interiorShadows_,trainWorld,Light::Saloon,
                    {.lit = false, .blend = BlendMode::AlphaBlend});
        stats.interiorContactShadowTriangles = interiorShadows_->triangles();
    }
    queueVertex(*interior_,trainWorld,Light::Saloon);
    if (interiorEmitters_)
        queueVertex(*interiorEmitters_,trainWorld,Light::Saloon,{.lit = false});
    queuePrintedText(interiorLettering_,trainWorld);
    stats.paSpeakerGrilles = paSpeakerGrilleCount_;
    stats.hvacVents = hvacVentCount_;
    stats.carIdentitySigns = carIdentitySignCount_;
    stats.carFunctionFixtureGroups = carFunctionFixtureGroupCount_;
    stats.cabControlGroups = cabControlGroupCount_;
    stats.vestibulePortalDetailGroups = vestibulePortalDetailGroupCount_;
    stats.overheadLuggageItems = overheadLuggageItemCount_;
    stats.floorLuggageCases = floorLuggageCaseCount_;
    stats.floorLuggageWheelPairs = floorLuggageWheelPairCount_;
    stats.roundedSeatCushions = roundedSeatCushionCount_;
    stats.roundedSeatBacks = roundedSeatBackCount_;
    stats.seatPedestals = seatPedestalCount_;
    stats.seatMountFasteners = seatMountFastenerCount_;
    stats.roundedAisleArmrests = roundedAisleArmrestCount_;
    stats.roundedTableTops = roundedTableTopCount_;
    stats.tableSupportPlates = tableSupportPlateCount_;
    stats.aisleRunnerInlays = aisleRunnerInlayCount_;
    stats.nonCarpetedFloorZones = nonCarpetedFloorZoneCount_;
    stats.interiorCovingSections = interiorCovingSectionCount_;
    stats.ceilingPanelSeams = ceilingPanelSeamCount_;
    stats.vestibuleWearMarks = vestibuleWearMarkCount_;

    if (cabActiveDisplay_)
    {
        const float operatingDirection = service.direction >= 0 ? 1.0f : -1.0f;
        constexpr float cabNose = 72.8f;
        constexpr float screenFromNose = 1.21f;
        constexpr float screenFaceOffset = 0.061f;
        const float activeDisplayZ = operatingDirection
            * (cabNose-screenFromNose-screenFaceOffset);
        const Matrix activeDisplayWorld = Matrix::CreateRotationY(
            operatingDirection < 0.0f ? kPi : 0.0f)
            * Matrix::CreateTranslation(0.0f,kTrainFloorElevation+1.51f,activeDisplayZ);
        queueVertex(*cabActiveDisplay_,activeDisplayWorld,Light::Saloon,{.lit = false});
        stats.activeCabDisplays = 1;
    }
    if (graphicsQuality_ >= 1 && interiorLightGradients_)
    {
        queueVertex(*interiorLightGradients_,trainWorld,Light::Saloon,
                    {.lit = false, .blend = BlendMode::AlphaBlend});
        stats.interiorLightGradientQuads = interiorLightGradientQuadCount_;
    }
    const float sunlightAmount = std::clamp(1.0f-tunnelLightingMix_*1.05f,0.0f,1.0f)
        * (atmosphere_.direct >= 1.0f && atmosphere_.daylight >= 1.0f
           ? 1.0f : atmosphere_.direct*atmosphere_.sunDisc);
    if (sunlightAmount > 0.01f)
    {
        queueVertex(*sunlightPatches_,trainWorld,Light::Saloon,
                    {.lit = false, .alpha = sunlightAmount, .blend = BlendMode::AlphaBlend});
        stats.sunlightPatches = sunlightPatchCount_;
    }
    const int displaySpeed = std::clamp(
        static_cast<int>(std::lround(service.speedMetresPerSecond * 3.6 / 5.0)) * 5, 0, 999);
    const int displayEta = std::clamp(
        static_cast<int>(std::ceil(std::max(0.0, etaSeconds) / 60.0)), 0, 99);
    const int informationDisplayPage = static_cast<int>(seconds / 6.0) % 3;
    const InformationDisplayState displayState{
        service.nextStation, service.direction,
        service.phase == Sim::ServicePhase::Dwelling,
        informationDisplayPage, displaySpeed, displayEta};
    if (!informationDisplays_ || informationDisplayState_ != displayState)
        rebuildInformationDisplays(route, service, displaySpeed, displayEta,
                                   informationDisplayPage);
    queuePrintedText(informationDisplays_,trainWorld);

    // Each car has a double sliding passenger door. Only the platform side opens; the opposing
    // side remains visibly interlocked. Player collision retains the onboard boundary.
    for (int car = 0; car < 6; ++car)
    {
        const float doorZ = static_cast<float>(Sim::TrainInterior::passengerDoorZ(car));
        for (float side : {-1.0f, 1.0f})
        {
            const bool platformOnNegativeX = Sim::platformOnNegativeTrainX(service);
            const bool platformSide = side < 0.0f ? platformOnNegativeX : !platformOnNegativeX;
            const float openness = platformSide ? doorOpenAmount_ : 0.0f;
            const float panelOffset = 0.33f + openness * 0.63f;
            for (float panel : {-1.0f, 1.0f})
                queueVertex(*passengerDoor_,
                            Matrix::CreateRotationY(panel < 0.0f ? kPi : 0.0f)
                                * Matrix::CreateTranslation(side * 1.455f,
                                                            kTrainFloorElevation+1.17f,
                                                            doorZ + panel * panelOffset),
                            Light::Saloon);
            queueBox(boxMatrix(side * 1.46f,kTrainFloorElevation+2.32f,doorZ,
                               0.10f,0.08f,2.05f),
                     Color(230,181,40,255),Light::Saloon);
            // A small passenger door button gives an immediate, believable indication of which
            // side is released. It remains dark while the train is moving and turns amber during
            // the interlocked closing phase.
            const Color buttonColor = !platformSide ? Color(49,58,60,255)
                : service.phase == Sim::ServicePhase::Dwelling ? Color(73,215,123,255)
                : service.phase == Sim::ServicePhase::ClosingDoors ? Color(238,178,45,255)
                : Color(49,58,60,255);
            queueVertex(*passengerDoorButton_,Matrix::CreateTranslation(
                side*1.45f,kTrainFloorElevation+1.36f,
                doorZ-static_cast<float>(Sim::TrainInterior::PassengerDoorHalfOpening)),
                Light::Saloon,{.diffuse = rgb(buttonColor)});
            if (platformSide && openness > 0.01f)
            {
                // Deploy before the leaves finish opening and overlap the platform coping by two
                // centimetres at full travel. The collision capsule remains inside the body, so
                // this accessibility detail cannot accidentally let the player leave the train.
                const float extension = std::clamp(openness * 1.5f,0.0f,1.0f);
                const Matrix stepWorld = Matrix::CreateScale(extension,1.0f,1.0f)
                    * Matrix::CreateRotationY(side < 0.0f ? kPi : 0.0f)
                    * Matrix::CreateTranslation(side*static_cast<float>(
                        Sim::TrainInterior::BoardingStepInnerEdge),
                        kPlatformSurfaceElevation,doorZ);
                queueVertex(*boardingStep_,stepWorld,Light::Saloon);
                ++stats.deployedBoardingSteps;
            }
        }
    }

    const Matrix doorWorld = Matrix::CreateRotationY(-1.45f * toiletDoorOpenAmount_)
                           * Matrix::CreateTranslation(0.37f,kTrainFloorElevation,4.07f);
    queueVertex(*toiletDoor_,doorWorld,Light::Saloon);
    const Matrix indicatorWorld = boxMatrix(-0.067f,1.72f,-0.35f,0.038f,0.16f,0.18f)
                                * doorWorld;
    queueBox(indicatorWorld,toiletOccupied ? Color(210,55,45,255) : Color(57,177,91,255),
             Light::Saloon);
    stats.occupiedToiletIndicators = toiletOccupied ? 1 : 0;

    // A low-opacity tint plus a narrow highlight gives the windows readable glass without
    // overpowering the scenery. DepthRead (every blended draw) prevents transparent panes from
    // occluding each other.
    const Surface glass{.blend = BlendMode::AlphaBlend};
    // Automatic inter-car leaves react to both the player and aisle actors. Their transparent
    // centre pane and opaque frame share one mesh and pass; distant boundaries are culled.
    for (int boundary = 0; boundary < Sim::TrainInterior::GangwayCount; ++boundary)
    {
        const float boundaryZ = static_cast<float>(Sim::TrainInterior::gangwayZ(boundary));
        if (std::abs(playerZ - boundaryZ) > 38.0f) continue;
        const float openness = gangwayDoorOpenAmounts_[static_cast<std::size_t>(boundary)];
        for (float panel : {-1.0f, 1.0f})
            queueVertex(*gangwayDoor_,
                        Matrix::CreateTranslation(
                            panel * (0.29f + openness * 0.64f),
                            kTrainFloorElevation + 1.20f,boundaryZ),
                        Light::Saloon,glass);
    }
    // Rain falls outside the cars only: the tile's empty corridor encloses the bodyshell, and
    // the opaque interior already occludes the streaks beyond the far wall (ENV-139).
    const float rainVisibility = atmosphere_.rain*(1.0f-tunnelLightingMix_);
    if (rainVisibility > 0.02f)
    {
        const Vector3 eye = Matrix::Invert(view).getTranslationProperty();
        const float relativeWind = static_cast<float>(service.direction)
            * static_cast<float>(service.speedMetresPerSecond);
        constexpr float fallSpeed = 9.0f;
        const float shear = std::clamp(relativeWind/fallSpeed,-3.5f,3.5f);
        const auto wrap = [](double value, double period) {
            const double wrapped = std::fmod(value,period);
            return static_cast<float>(wrapped < 0.0 ? wrapped+period : wrapped);
        };
        const float yScroll = wrap(seconds*fallSpeed,kRainTileHeight);
        const float zScroll = wrap(-seconds*static_cast<double>(relativeWind),kRainTileLength);
        const float reach = 45.0f+std::abs(shear)*kRainTileHeight*0.5f;
        const int firstTile = static_cast<int>(std::floor((eye.Z-reach-zScroll)/kRainTileLength));
        const int lastTile = static_cast<int>(std::floor((eye.Z+reach-zScroll)/kRainTileLength));
        const Surface streaks{.lit = false,
                              .diffuse = Vector3{0.24f,0.26f,0.30f}*(1.0f-atmosphere_.daylight)
                                         +Vector3::One*atmosphere_.daylight,
                              .alpha = std::min(1.0f,rainVisibility),
                              .blend = BlendMode::AlphaBlend, .doubleSided = true};
        Matrix shearAboutEye = Matrix::getIdentityProperty();
        shearAboutEye.M23 = shear;
        shearAboutEye.M43 = -shear*eye.Y;
        // Beside a platform the canopy shelters its side of the train.
        const Sim::Station& rainStation = route.stations()[route.nearestStation(
            service.routeDistanceMetres)];
        const bool sheltered = std::abs(rainStation.distanceMetres-service.routeDistanceMetres)
            < 110.0;
        const bool platformNegative = Sim::platformOnNegativeTrainX(service);
        for (std::size_t half = 0; half < rainStreaks_.size(); ++half)
        {
            if (!rainStreaks_[half] || (sheltered && (half == 0) == platformNegative)) continue;
            for (int tile = firstTile; tile <= lastTile; ++tile)
                for (int layer = 0; layer < 2; ++layer)
                {
                    const Matrix placement = Matrix::CreateTranslation(
                        0.0f,eye.Y-yScroll+static_cast<float>(layer)*kRainTileHeight,
                        static_cast<float>(tile)*kRainTileLength+zScroll) * shearAboutEye;
                    queueVertex(*rainStreaks_[half],placement,Light::Saloon,streaks);
                    ++stats.rainStreakDraws;
                }
        }
    }
    queueVertex(*passengerDoorGlass_,trainWorld,Light::Saloon,glass);
    queueVertex(*windowGlass_,trainWorld,Light::Saloon,glass);
    if (rainDroplets_ && atmosphere_.rain > 0.02f)
    {
        // Beads persist on the glass while it rains, lit by the cabin as much as the sky.
        queueVertex(*rainDroplets_,trainWorld,Light::Saloon,
                    {.lit = false,
                     .diffuse = Vector3{0.55f,0.56f,0.58f}*(1.0f-atmosphere_.daylight)
                                +Vector3::One*atmosphere_.daylight,
                     .alpha = std::min(1.0f,atmosphere_.rain*1.2f),
                     .blend = BlendMode::AlphaBlend, .doubleSided = true});
        stats.rainDropletTriangles = rainDroplets_->triangles();
    }
    if (graphicsQuality_ >= 1)
    {
        // A fake Fresnel term is weakest when looking squarely through a side window and grows
        // while looking along the pane. It modulates only restrained reflection accents, never
        // the transparent base glass or exterior scene.
        const float faceOn = std::abs(std::sin(cameraYaw));
        for (auto& reflections : windowReflections_)
            if (reflections)
                queueVertex(*reflections,trainWorld,Light::Saloon,
                            {.alpha = 0.13f+(1.0f-faceOn)*0.25f, .blend = BlendMode::AlphaBlend});
        stats.windowReflectionAccents = windowReflectionAccentCount_;
    }
    render();
    stats.avatarVisuals = static_cast<int>(avatars_.size());
    stats.readyAvatarVisuals = static_cast<int>(std::count_if(
        avatars_.begin(),avatars_.end(),[](const auto& avatar) {
            return avatar->renderer->getStateProperty() == AvatarRendererState::Ready;
        }));
    stats.civilianAvatarDescriptions = civilianAvatarDescriptionCount_;
    return stats;
}


} // namespace TrainSim::Render
