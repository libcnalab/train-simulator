#include "trainsim/app/TrainSimulatorGame.hpp"
#include "trainsim/app/MouseLook.hpp"

#include "trainsim/sim/Environment.hpp"
#include "trainsim/sim/TrainInterior.hpp"

#include "CnaEngine/Graphics/ScreenCapture.hpp"

#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/GameTime.hpp"
#include "Microsoft/Xna/Framework/GameWindow.hpp"
#include "Microsoft/Xna/Framework/GamerServices/GamerServicesComponent.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsProfile.hpp"
#include "Microsoft/Xna/Framework/Graphics/Viewport.hpp"
#include "Microsoft/Xna/Framework/Input/Keyboard.hpp"
#include "Microsoft/Xna/Framework/Input/Keys.hpp"
#include "Microsoft/Xna/Framework/Input/Mouse.hpp"
#include "Microsoft/Xna/Framework/MathHelper.hpp"
#include "Microsoft/Xna/Framework/Matrix.hpp"
#include "Microsoft/Xna/Framework/Rectangle.hpp"
#include "Microsoft/Xna/Framework/Vector2.hpp"
#include "Microsoft/Xna/Framework/Vector3.hpp"
#include "System/TimeSpan.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cmath>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;
using namespace Microsoft::Xna::Framework::Input;

namespace TrainSim::App {

namespace {
constexpr float kPi = 3.14159265358979323846f;
#ifdef __EMSCRIPTEN__
// Pointer-lock motion is reported in CSS pixels.  The same numeric sensitivity feels markedly
// slower than SDL's native relative mouse counts, especially on high-DPI displays, so compensate
// at the input boundary while preserving the player's configured sensitivity and menu range.
constexpr float kMouseLookPlatformScale = 2.5f;
#else
constexpr float kMouseLookPlatformScale = 1.0f;
#endif

std::string speedText(double metresPerSecond)
{
    return std::to_string(static_cast<int>(std::round(metresPerSecond * 3.6))) + " KM/H";
}

std::string scheduleText(double deviationSeconds)
{
    if (std::abs(deviationSeconds) < 45.0) return "SERVICE ON TIME";
    const int minutes = std::max(1, static_cast<int>(std::ceil(std::abs(deviationSeconds) / 60.0)));
    return "SERVICE " + std::to_string(minutes)
         + (deviationSeconds > 0.0 ? " MIN LATE" : " MIN EARLY");
}

const char* graphicsQualityText(int quality)
{
    if (quality <= 0) return "LOW";
    if (quality == 1) return "MEDIUM";
    return "HIGH";
}

float effectiveVegetationDistance(const Config::Settings& settings)
{
    const float cap = settings.graphicsQuality <= 0 ? 400.0f
                    : settings.graphicsQuality == 1 ? 700.0f : 1'000.0f;
    return std::min(settings.vegetationDistanceMetres, cap);
}

Config::Settings loadLaunchSettings(const LaunchOptions& options)
{
    const auto defaultConfig = options.assetDirectory / "config" / "train-simulator.cfg";
    Config::Settings settings = Config::Settings::load(defaultConfig);
    settings = Config::Settings::load(options.userConfigFile,settings);
    return settings;
}

std::string wrapMonospaceWords(std::string_view text, std::size_t maximumColumns)
{
    maximumColumns = std::max<std::size_t>(1U,maximumColumns);
    std::string wrapped;
    std::size_t paragraphStart = 0;
    bool firstParagraph = true;
    while (paragraphStart <= text.size())
    {
        const std::size_t paragraphEnd = text.find('\n',paragraphStart);
        const std::string_view paragraph = text.substr(
            paragraphStart, paragraphEnd == std::string_view::npos
                ? std::string_view::npos : paragraphEnd-paragraphStart);
        if (!firstParagraph) wrapped.push_back('\n');
        firstParagraph = false;

        std::istringstream words{std::string(paragraph)};
        std::string word;
        std::size_t lineLength = 0;
        while (words >> word)
        {
            if (lineLength != 0 && lineLength + 1U + word.size() > maximumColumns)
            {
                wrapped.push_back('\n');
                lineLength = 0;
            }
            else if (lineLength != 0)
            {
                wrapped.push_back(' ');
                ++lineLength;
            }
            wrapped += word;
            lineLength += word.size();
        }

        if (paragraphEnd == std::string_view::npos) break;
        paragraphStart = paragraphEnd + 1U;
    }
    return wrapped;
}

// The launch menu's only action. Drawing and mouse hit-testing share this so the clickable area
// always matches the painted button at any window size.
struct StartButton
{
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;

    [[nodiscard]] bool contains(int pointX, int pointY) const noexcept
    {
        return pointX >= x && pointX < x+width && pointY >= y && pointY < y+height;
    }
};

StartButton startButton(int viewportWidth, int viewportHeight)
{
    constexpr int width = 280;
    constexpr int height = 64;
    return {(viewportWidth-width)/2, viewportHeight/2+10, width, height};
}
}

TrainSimulatorGame::TrainSimulatorGame(LaunchOptions options)
    : options_(std::move(options)), settings_(loadLaunchSettings(options_))
{
    graphics_.setGraphicsProfileProperty(GraphicsProfile::HiDef);
    graphics_.setPreferredBackBufferWidthProperty(1280);
    graphics_.setPreferredBackBufferHeightProperty(720);
    loadedGraphicsQuality_ = settings_.graphicsQuality;
    if (options_.graphicsQuality.has_value())
        settings_.graphicsQuality = *options_.graphicsQuality;
    loadedServiceStartMinutes_ = settings_.serviceStartMinutes;
    loadedWeatherMode_ = settings_.weatherMode;
    if (options_.serviceStartMinutes) settings_.serviceStartMinutes = *options_.serviceStartMinutes;
    if (options_.weatherMode) settings_.weatherMode = *options_.weatherMode;
    // Thin catenary, window trim, seat edges and platform furniture otherwise shimmer badly in
    // motion. CNA negotiates the actual desktop sample count and safely clamps the request to the
    // OpenGL device. Keep Low free of the fill/resolve cost used by the software-GL smoke suite.
    graphics_.setPreferMultiSamplingProperty(settings_.graphicsQuality >= 1);
    // Diagnostic override only; preserve the normal 60 Hz simulation target and saved settings.
    graphics_.setSynchronizeWithVerticalRetraceProperty(options_.verticalSyncEnabled);
    getComponentsProperty().Add(new Microsoft::Xna::Framework::GamerServices::GamerServicesComponent(*this));
    setTargetElapsedTimeProperty(System::TimeSpan::FromTicks(166667));
    // Lockstep uses CNA's variable-step loop for one Update per Draw; stepSeconds() then gives
    // every step the fixed-step duration instead of the measured one.
    setIsFixedTimeStepProperty(options_.fixedTimeStep && !options_.lockstep);
    showHelp_ = options_.startWithHelp;
    showMap_ = options_.startWithMap;
    inspectDisplay_ = options_.startWithDisplayInspection;
    showDebug_ = options_.startWithDebug;
    showPassengerHud_ = options_.startWithPassengerHud;
    paused_ = options_.startPaused;
    // An interactive launch opens on the start menu. Every automated run (finite frames, timed
    // runs, screenshots and the interaction smoke) keeps starting directly aboard the train.
    startMenu_ = options_.frameLimit == 0 && options_.runSeconds == 0.0
        && options_.screenshotPath.empty() && !options_.interactionSmoke;
    if (options_.startAtSignal)
        service_.teleportToRouteDistance(220.0,1);
    else if (options_.startRouteKilometres.has_value())
        service_.teleportToRouteDistance(*options_.startRouteKilometres * 1'000.0,
                                         options_.startDirection.value_or(1));
    else if (options_.startStation.has_value())
        service_.teleportToStation(
            *options_.startStation,
            options_.startDirection.value_or(
                *options_.startStation + 1 == route_.stations().size() ? -1 : 1));
    if (options_.startCar.has_value())
    {
        const int car = *options_.startCar;
        const double centre = Sim::TrainInterior::FirstCarCentre
            + static_cast<double>(car) * Sim::TrainInterior::CarLength;
        if (car == 0)
        {
            playerZ_ = static_cast<float>(centre - 5.2);
            playerYaw_ = 0.0f;
        }
        else if (car + 1 == Sim::TrainInterior::PassengerCars)
        {
            playerZ_ = static_cast<float>(centre + 5.2);
            playerYaw_ = kPi;
        }
        else
        {
            playerZ_ = static_cast<float>(centre + 5.2);
            playerYaw_ = 0.0f;
        }
    }
    if (options_.startAtWindow || options_.startAtLeftWindow
        || options_.startAtPlatformWindow)
    {
        const int preferredCar = options_.startCar.value_or(3);
        bool negativeSide = options_.startAtLeftWindow;
        if (options_.startAtPlatformWindow)
        {
            const bool stoppedAtPlatform = service_.state().phase == Sim::ServicePhase::Dwelling
                || service_.state().phase == Sim::ServicePhase::ClosingDoors;
            negativeSide = stoppedAtPlatform
                ? Sim::platformOnNegativeTrainX(service_.state())
                : service_.nextPlatformOnNegativeTrainX();
        }
        const auto chooseWindowSeat = [this,negativeSide](std::optional<int> car) {
            auto best = passengers_.seats().end();
            double bestDistance = 1.0e9;
            const double targetZ = car.has_value() ? Sim::TrainInterior::carCentre(*car) : 0.0;
            for (auto seat = passengers_.seats().begin(); seat != passengers_.seats().end(); ++seat)
            {
                const bool requestedSide = negativeSide ? seat->x < 0.0 : seat->x > 0.0;
                if (!seat->window || !requestedSide || !passengers_.seatAvailable(seat->id)
                    || !passengers_.seatApproachClear(seat->id)
                    || (car.has_value() && seat->car != *car)) continue;
                const double distance = std::abs(seat->z-targetZ);
                if (distance >= bestDistance) continue;
                best = seat;
                bestDistance = distance;
            }
            return best;
        };
        auto seat = chooseWindowSeat(preferredCar);
        if (seat == passengers_.seats().end()) seat = chooseWindowSeat(std::nullopt);
        if (seat != passengers_.seats().end() && passengers_.occupySeatForPlayer(seat->id))
        {
            seated_ = true;
            seatedBlend_ = 1.0f;
            playerSeat_ = seat->id;
            playerX_ = static_cast<float>(seat->x);
            playerZ_ = static_cast<float>(seat->z);
            playerYaw_ = negativeSide ? -kPi*0.5f : kPi*0.5f;
        }
    }
    if (options_.startInToilet || options_.startAtSink)
    {
        toiletDoorOpen_ = true;
        // Face diagonally into the compartment so the visual-QA view covers the sink, mirror,
        // support rail, alarm and toilet rather than stopping almost flush against the mirror.
        playerX_ = 0.80f;
        playerZ_ = 6.00f;
        playerYaw_ = 3.02f;
        playerPitch_ = -0.78f;
        settings_.fieldOfViewDegrees = 82.0f;
        if (options_.startAtSink)
        {
            toiletDoorOpen_ = false;
            playerZ_ = 5.65f;
            playerYaw_ = 0.22f;
            playerPitch_ = -0.55f;
        }
    }
    if (options_.startAtGangway)
    {
        playerX_ = 0.0f;
        playerZ_ = static_cast<float>(Sim::TrainInterior::FirstGangwayZ + 2.60);
        playerYaw_ = 0.0f;
    }
    if (options_.startAtVestibule)
    {
        const int car = options_.startCar.value_or(3);
        playerX_ = 0.0f;
        playerZ_ = static_cast<float>(Sim::TrainInterior::passengerDoorZ(car)-0.60);
        playerYaw_ = kPi;
        playerPitch_ = 0.0f;
        settings_.fieldOfViewDegrees = 84.0f;
    }
    if (options_.startAtSunlight)
    {
        // Reproducible visual-QA framing for the low-cost sunlight projection. Look down the
        // aisle across a complete patch rather than relying on a manually positioned camera.
        const int car = options_.startCar.value_or(2);
        playerX_ = 0.12f;
        playerZ_ = static_cast<float>(Sim::TrainInterior::carCentre(car)+8.0);
        playerYaw_ = 0.0f;
        playerPitch_ = -0.80f;
        settings_.fieldOfViewDegrees = 76.0f;
    }
    if (options_.startAtPassenger)
    {
        const int car = options_.startCar.value_or(2);
        const auto target = std::find_if(passengers_.passengers().begin(),passengers_.passengers().end(),
            [car](const Sim::Passenger& passenger) {
                return passenger.car == car && passenger.seat.has_value()
                    && passenger.role == Sim::PassengerRole::Passenger;
            });
        if (target != passengers_.passengers().end())
        {
            // Apply after the car spawn, keeping this inspection camera in the real aisle.
            const int row = Sim::TrainInterior::seatRowIndex(car,target->z);
            playerX_ = 0.0f;
            playerZ_ = static_cast<float>(target->z+(row%2 == 0 ? -1.35 : 1.35));
            playerYaw_ = std::atan2(static_cast<float>(target->x),
                                   playerZ_-static_cast<float>(target->z));
            playerPitch_ = -0.36f;
            settings_.fieldOfViewDegrees = 76.0f;
        }
    }
    if (options_.startAtSignal)
    {
        // Use the real generated direction-facing signal, a production window seat and the normal
        // first-person camera. The aligned same-side window and wide QA field of view keep the
        // complete head, active lens and access ladder visible without a detached spectator camera.
        service_.freeze(true);
        streaming_.update(service_.state().routeDistanceMetres);
        const World::WorldFeature* selectedSignal = nullptr;
        double bestDistance = 1.0e9;
        for (const World::Chunk& chunk : streaming_.chunks())
            for (const World::WorldFeature& feature : chunk.features)
            {
                if (feature.kind != World::FeatureKind::Signal
                    || World::signalFacingDirection(feature) != service_.state().direction)
                    continue;
                const double distance = std::abs(
                    feature.routeDistance-service_.state().routeDistanceMetres);
                if (distance >= bestDistance) continue;
                selectedSignal = &feature;
                bestDistance = distance;
            }
        if (selectedSignal == nullptr)
            throw std::runtime_error("Signal showcase could not find a streamed facing signal");

        const Sim::RelativeRoutePose relative = route_.relativePose(
            service_.state().routeDistanceMetres,selectedSignal->routeDistance);
        const bool negativeSide = selectedSignal->lateralMetres < 0.0;
        const double desiredSeatZ = relative.z;
        auto selectedSeat = passengers_.seats().end();
        bestDistance = 1.0e9;
        for (auto seat = passengers_.seats().begin(); seat != passengers_.seats().end(); ++seat)
        {
            const bool requestedSide = negativeSide ? seat->x < 0.0 : seat->x > 0.0;
            if (!seat->window || !requestedSide || !passengers_.seatAvailable(seat->id)
                || !passengers_.seatApproachClear(seat->id)) continue;
            const double distance = std::abs(seat->z-desiredSeatZ);
            if (distance >= bestDistance) continue;
            selectedSeat = seat;
            bestDistance = distance;
        }
        if (selectedSeat == passengers_.seats().end()
            || !passengers_.occupySeatForPlayer(selectedSeat->id))
            throw std::runtime_error("Signal showcase could not reserve a clear window seat");

        seated_ = true;
        seatedBlend_ = 1.0f;
        playerSeat_ = selectedSeat->id;
        playerX_ = static_cast<float>(selectedSeat->x);
        playerZ_ = static_cast<float>(selectedSeat->z);
        const double cosine = std::cos(relative.headingRadians);
        const double sine = std::sin(relative.headingRadians);
        const double signalX = relative.x+selectedSignal->lateralMetres*cosine;
        const double signalZ = relative.z-selectedSignal->lateralMetres*sine;
        const double horizontal = std::hypot(signalX-playerX_,signalZ-playerZ_);
        playerYaw_ = static_cast<float>(std::atan2(
            signalX-playerX_,-(signalZ-playerZ_)));
        const double seatedEyeY = Sim::TrainInterior::FloorElevationAboveRail+1.20;
        playerPitch_ = static_cast<float>(std::atan2(
            relative.elevation+2.30-seatedEyeY,horizontal));
        settings_.fieldOfViewDegrees = 100.0f;
    }
    if (options_.startAtDoor)
    {
        const bool stoppedAtPlatform = service_.state().phase == Sim::ServicePhase::Dwelling
            || service_.state().phase == Sim::ServicePhase::ClosingDoors;
        const bool negativeSide = stoppedAtPlatform
            ? Sim::platformOnNegativeTrainX(service_.state())
            : service_.nextPlatformOnNegativeTrainX();
        const int car = options_.startCar.value_or(3);
        playerX_ = negativeSide ? -1.26f : 1.26f;
        playerZ_ = static_cast<float>(Sim::TrainInterior::passengerDoorZ(car));
        playerYaw_ = negativeSide ? -kPi*0.5f : kPi*0.5f;
        // Keep the deployed plate at the lower edge while retaining station context above it.
        playerPitch_ = -0.48f;
        settings_.fieldOfViewDegrees = 82.0f;
    }
    initialRouteDistanceMetres_ = service_.state().routeDistanceMetres;
    if (options_.developmentDwell) service_.setDevelopmentDwellScale(0.08);
}

TrainSimulatorGame::~TrainSimulatorGame()
{
    if (mouseCaptured_ || Mouse::getIsRelativeMouseModeEXTProperty())
        Mouse::setIsRelativeMouseModeEXTProperty(false);
    if (options_.reportState)
    {
        const auto timing = frameTiming_.summary();
        const auto steadyTiming = steadyFrameTiming_.summary();
        const auto& state = service_.state();
        const auto driver = std::find_if(passengers_.passengers().begin(),
                                         passengers_.passengers().end(),
            [](const Sim::Passenger& passenger) {
                return passenger.role == Sim::PassengerRole::Driver;
            });
        const int driverCar = driver == passengers_.passengers().end() ? -99 : driver->car;
        const std::string_view announcementCaption = audio_
            ? audio_->currentAnnouncementCaption() : std::string_view{};
        const std::string_view lastAnnouncementCaption = audio_
            ? audio_->lastAnnouncementCaption() : std::string_view{};
        const std::string_view queuedAnnouncementCaption = audio_
            ? audio_->queuedAnnouncementCaption() : std::string_view{};
        const bool announcementNamesNextStation = !announcementCaption.empty()
            && announcementCaption.find(route_.stations()[state.nextStation].name)
                != std::string_view::npos;
        const bool announcementNamesDestination = !announcementCaption.empty()
            && announcementCaption.find(service_.destinationName()) != std::string_view::npos;
        const bool lastAnnouncementNamesNextStation = !lastAnnouncementCaption.empty()
            && lastAnnouncementCaption.find(route_.stations()[state.nextStation].name)
                != std::string_view::npos;
        const bool lastAnnouncementNamesDestination = !lastAnnouncementCaption.empty()
            && lastAnnouncementCaption.find(service_.destinationName()) != std::string_view::npos;
        const bool selectedAnnouncementNamesNextStation =
            (!queuedAnnouncementCaption.empty()
             && queuedAnnouncementCaption.find(route_.stations()[state.nextStation].name)
                 != std::string_view::npos)
            || announcementNamesNextStation || lastAnnouncementNamesNextStation;
        std::cout << "FINAL_STATE route_km=" << std::fixed << std::setprecision(3)
                  << state.routeDistanceMetres / 1000.0
                  << " direction=" << state.direction
                  << " phase=" << service_.phaseName()
                  << " speed_kmh=" << std::setprecision(1)
                  << state.speedMetresPerSecond * 3.6
                  << " longitudinal_accel="
                  << state.longitudinalAccelerationMetresPerSecondSquared
                  << " service_time_s=" << state.serviceElapsedSeconds
                  << " station_legs=" << state.completedLegs
                  << " generated_chunks=" << streaming_.generatedChunkCount()
                  << " loaded_chunks=" << streaming_.chunks().size()
                  << " passengers=" << passengers_.passengers().size()
                  << " driver_car=" << driverCar
                  << " conductor_checks=" << passengers_.conductorInspectionCount()
                  << " lane_changes=" << passengers_.passengerLaneChangeCount()
                  << " player_yields=" << passengers_.playerYieldCount()
                  << " seat_walkers=" << passengers_.walkingToSeatCount()
                  << " exchange_actors=" << passengers_.stationExchangeActorCount()
                  << " exchange_started="
                  << passengers_.lastStationExchangeParticipantCount()
                  << " offboard=" << passengers_.offboardPassengerCount()
                  << " clock_minutes=" << renderStats_.clockMinutes
                  << " weather=" << Sim::weatherModeName(
                         static_cast<Sim::WeatherMode>(settings_.weatherMode))
                  << " sun_elevation_deg=" << renderStats_.sunElevationDegrees
                  << " night=" << renderStats_.atmosphereNight
                  << " rain=" << renderStats_.atmosphereRain
                  << " overcast=" << renderStats_.atmosphereOvercast
                  << " sky_rebuilds=" << renderStats_.skyRebuilds
                  << " cloud_layers=" << renderStats_.cloudLayers
                  << " rain_streak_draws=" << renderStats_.rainStreakDraws
                  << " rain_droplet_triangles=" << renderStats_.rainDropletTriangles
                  << " night_glow_triangles=" << renderStats_.nightGlowTriangles
                  << " other_train_night_overlays=" << renderStats_.otherTrainNightOverlays
                  << " draw_calls=" << renderStats_.drawCalls
                  << " triangles=" << renderStats_.triangles
                  << " graphics_quality=" << settings_.graphicsQuality
                  << " msaa_samples=" << appliedMultisampleCount_
                  << " terrain_material_triangles=" << renderStats_.terrainMaterialTriangles
                  << " ballast_material_triangles=" << renderStats_.ballastMaterialTriangles
                  << " rail_head_material_triangles="
                  << renderStats_.railHeadMaterialTriangles
                  << " track_fasteners=" << renderStats_.trackFastenerAssemblies
                  << " turnout_motors=" << renderStats_.turnoutPointMotors
                  << " turnout_blades=" << renderStats_.turnoutBladeAssemblies
                  << " interior_material_triangles=" << renderStats_.interiorMaterialTriangles
                  << " interior_panel_material_triangles="
                  << renderStats_.interiorPanelMaterialTriangles
                  << " interior_resilient_material_triangles="
                  << renderStats_.interiorResilientMaterialTriangles
                  << " water_material_triangles=" << renderStats_.waterMaterialTriangles
                  << " facade_material_triangles=" << renderStats_.facadeMaterialTriangles
                  << " rendered_facade_material_triangles="
                  << renderStats_.renderedFacadeMaterialTriangles
                  << " static_shadow_triangles=" << renderStats_.staticContactShadowTriangles
                  << " platform_material_triangles=" << renderStats_.platformMaterialTriangles
                  << " interior_shadow_triangles="
                  << renderStats_.interiorContactShadowTriangles
                  << " visible_npc=" << renderStats_.visiblePassengers
                  << " npc_gaze=" << renderStats_.animatedGazeAvatars
                  << " npc_reading=" << renderStats_.readingIdleAvatars
                  << " npc_phone=" << renderStats_.phoneCheckingAvatars
                  << " npc_social=" << renderStats_.onboardSocialAvatars
                  << " npc_distance_gait=" << renderStats_.distanceDrivenGaitAvatars
                  << " npc_seat_transitions=" << renderStats_.seatTransitionAvatars
                  << " npc_acknowledgement=" << renderStats_.playerAcknowledgingAvatars
                  << " conductor_devices=" << renderStats_.conductorHandheldDevices
                  << " platform_phone=" << renderStats_.platformPhoneUsers
                  << " platform_social=" << renderStats_.platformSocialAvatars
                  << " sky_clouds=" << renderStats_.skyCloudBillboards
                  << " avatar_rendering=" << (options_.avatarRenderingEnabled ? 1 : 0)
                  << " avatar_visuals=" << renderStats_.avatarVisuals
                  << " ready_avatar_visuals=" << renderStats_.readyAvatarVisuals
                  << " civilian_avatar_pool=" << renderStats_.civilianAvatarDescriptions
                  << " fps=" << frameRate_
                  << " frame_window_samples=" << timing.windowSamples
                  << " frame_total_samples=" << timing.totalSamples
                  << " frame_mean_ms=" << timing.meanMs
                  << " frame_p50_ms=" << timing.p50Ms
                  << " frame_p95_ms=" << timing.p95Ms
                  << " frame_p99_ms=" << timing.p99Ms
                  << " frame_window_max_ms=" << timing.windowMaxMs
                  << " frame_all_max_ms=" << timing.allMaxMs
                  << " steady_frame_samples=" << steadyTiming.windowSamples
                  << " steady_frame_mean_ms=" << steadyTiming.meanMs
                  << " steady_frame_p95_ms=" << steadyTiming.p95Ms
                  << " steady_frame_p99_ms=" << steadyTiming.p99Ms
                  << " steady_frame_max_ms=" << steadyTiming.windowMaxMs
                  << " world_prefetch_hits=" << renderStats_.worldPrefetchHits
                  << " world_synchronous_builds=" << renderStats_.worldSynchronousBuilds
                  << " world_late_prefetches=" << renderStats_.worldLatePrefetches
                  << " world_rebuilds=" << renderStats_.worldRebuilds
                  << " world_buffer_reuses=" << renderStats_.worldBufferReuses
                  << " route_point_cache_hits=" << renderStats_.routePointCacheHits
                  << " route_point_cache_misses=" << renderStats_.routePointCacheMisses
                  << " world_rebuild_total_ms=" << renderStats_.worldRebuildTotalMs
                  << " world_rebuild_max_ms=" << renderStats_.worldRebuildMaxMs
                  << " world_generation_total_ms=" << renderStats_.worldGenerationTotalMs
                  << " world_generation_max_ms=" << renderStats_.worldGenerationMaxMs
                  << " world_upload_total_ms=" << renderStats_.worldUploadTotalMs
                  << " world_upload_max_ms=" << renderStats_.worldUploadMaxMs
                  << " culled_onboard_avatars=" << renderStats_.culledOnboardAvatars
                  << " signals_r=" << renderStats_.redSignals
                  << " signals_y=" << renderStats_.yellowSignals
                  << " signals_g=" << renderStats_.greenSignals
                  << " platform_boards=" << renderStats_.platformInformationBoards
                  << " platform_joints=" << renderStats_.platformSurfaceJoints
                  << " canopy_lights=" << renderStats_.platformCanopyLights
                  << " canopy_drainage="
                  << renderStats_.platformCanopyDrainageAssemblies
                  << " platform_planters=" << renderStats_.platformPlanters
                  << " platform_posters=" << renderStats_.platformPosterCases
                  << " platform_cabinets=" << renderStats_.platformEquipmentCabinets
                  << " platform_benches=" << renderStats_.platformBenchAssemblies
                  << " platform_weathering=" << renderStats_.platformWeatheringMarks
                  << " station_facade_streaks="
                  << renderStats_.stationFacadeWeatheringStreaks
                  << " platform_boundaries=" << renderStats_.platformBoundarySections
                  << " drainage_sections=" << renderStats_.tracksideDrainageSections
                  << " verge_tufts=" << renderStats_.tracksideVergeTufts
                  << " field_boundaries=" << renderStats_.fieldBoundarySegments
                  << " distant_woodland=" << renderStats_.distantWoodlandTrees
                  << " distant_ridges=" << renderStats_.distantRidgeMasses
                  << " tree_branches=" << renderStats_.treeBranchCues
                  << " conifer_tiers=" << renderStats_.coniferBranchTiers
                  << " understorey=" << renderStats_.understoreyGroups
                  << " distant_settlement=" << renderStats_.distantSettlementBuildings
                  << " building_end_windows=" << renderStats_.buildingEndWallWindows
                  << " tower_end_windows=" << renderStats_.towerEndWallWindows
                  << " warehouse_details=" << renderStats_.detailedWarehouses
                  << " industrial_compounds=" << renderStats_.industrialCompounds
                  << " industrial_vehicles=" << renderStats_.industrialYardVehicles
                  << " industrial_trailers=" << renderStats_.industrialTrailers
                  << " industrial_utilities=" << renderStats_.industrialUtilityGroups
                  << " industrial_edge_planting="
                  << renderStats_.industrialEdgePlantingGroups
                  << " road_edge_markings=" << renderStats_.urbanRoadEdgeMarkingSections
                  << " road_drain_grates=" << renderStats_.urbanRoadDrainGrates
                  << " urban_plot_surfaces="
                  << renderStats_.urbanResidentialPlotSurfaces
                  << " urban_entrance_paths="
                  << renderStats_.urbanResidentialEntrancePaths
                  << " platform_luggage=" << renderStats_.platformLuggageItems
                  << " boarding_steps=" << renderStats_.deployedBoardingSteps
                  << " lake_patches=" << renderStats_.lakesideWaterPatches
                  << " lake_bank_trees=" << renderStats_.lakesideBankTrees
                  << " hay_bale_stacks=" << renderStats_.hayBaleStacks
                  << " river_water_sections=" << renderStats_.riverWaterSections
                  << " waterfront_sections=" << renderStats_.stationWaterfrontSections
                  << " km_posts=" << renderStats_.tracksideKilometrePosts
                  << " catenary_droppers=" << renderStats_.catenaryDroppers
                  << " catenary_foundations=" << renderStats_.catenaryFoundations
                  << " catenary_messenger=" << renderStats_.catenaryMessengerSegments
                  << " catenary_registration=" << renderStats_.catenaryRegistrationAssemblies
                  << " lineside_cabinets=" << renderStats_.linesideEquipmentCabinets
                  << " other_train_cars=" << renderStats_.otherTrainCars
                  << " other_train_cabs=" << renderStats_.otherTrainCabEnds
                  << " other_train_locomotives=" << renderStats_.otherTrainLocomotives
                  << " other_train_freight_wagons=" << renderStats_.otherTrainFreightWagons
                  << " road_vehicles=" << renderStats_.roadVehicles
                  << " tunnel_luminaires=" << renderStats_.tunnelLuminaires
                  << " tunnel_niches=" << renderStats_.tunnelSafetyNiches
                  << " bridge_sections=" << renderStats_.bridgeSections
                  << " river_bridge_sections=" << renderStats_.riverBridgeSections
                  << " viaduct_sections=" << renderStats_.viaductSections
                  << " bridge_abutments=" << renderStats_.bridgeAbutments
                  << " clock_hands=" << renderStats_.stationClockHands
                  << " crossing_barriers=" << renderStats_.levelCrossingBarriers
                  << " crossing_lights=" << renderStats_.levelCrossingWarningLights
                  << " crossing_audio="
                  << (audio_ ? audio_->crossingWarningEventCount() : 0U)
                  << " gangway_doors=" << renderStats_.openGangwayDoors
                  << " toilet_occupied_indicators="
                  << renderStats_.occupiedToiletIndicators
                  << " pa_speakers=" << renderStats_.paSpeakerGrilles
                  << " hvac_vents=" << renderStats_.hvacVents
                  << " car_signs=" << renderStats_.carIdentitySigns
                  << " car_function_groups=" << renderStats_.carFunctionFixtureGroups
                  << " cab_control_groups=" << renderStats_.cabControlGroups
                  << " active_cab_displays=" << renderStats_.activeCabDisplays
                  << " vestibule_portal_details="
                  << renderStats_.vestibulePortalDetailGroups
                  << " overhead_luggage=" << renderStats_.overheadLuggageItems
                  << " floor_luggage_cases=" << renderStats_.floorLuggageCases
                  << " luggage_wheel_pairs=" << renderStats_.floorLuggageWheelPairs
                  << " rounded_seat_cushions=" << renderStats_.roundedSeatCushions
                  << " rounded_seat_backs=" << renderStats_.roundedSeatBacks
                  << " seat_pedestals=" << renderStats_.seatPedestals
                  << " seat_mount_fasteners=" << renderStats_.seatMountFasteners
                  << " rounded_armrests=" << renderStats_.roundedAisleArmrests
                  << " rounded_table_tops=" << renderStats_.roundedTableTops
                  << " table_support_plates=" << renderStats_.tableSupportPlates
                  << " window_reflections=" << renderStats_.windowReflectionAccents
                  << " interior_light_gradients=" << renderStats_.interiorLightGradientQuads
                  << " sunlight_patches=" << renderStats_.sunlightPatches
                  << " aisle_inlays=" << renderStats_.aisleRunnerInlays
                  << " floor_zones=" << renderStats_.nonCarpetedFloorZones
                  << " interior_coving=" << renderStats_.interiorCovingSections
                  << " ceiling_panel_seams=" << renderStats_.ceilingPanelSeams
                  << " vestibule_wear=" << renderStats_.vestibuleWearMarks
                  << " announcement_chime_events="
                  << (audio_ ? audio_->announcementChimeEventCount() : 0U)
                  << " announcement_events="
                  << (audio_ ? audio_->announcementEventCount() : 0U)
                  << " announcement_caption="
                  << (!announcementCaption.empty() ? 1 : 0)
                  << " announcement_next_station="
                  << (announcementNamesNextStation ? 1 : 0)
                  << " announcement_destination="
                  << (announcementNamesDestination ? 1 : 0)
                  << " announcement_last_next_station="
                  << (lastAnnouncementNamesNextStation ? 1 : 0)
                  << " announcement_last_destination="
                  << (lastAnnouncementNamesDestination ? 1 : 0)
                  << " announcement_selected_next_station="
                  << (selectedAnnouncementNamesNextStation ? 1 : 0)
                  << " terminal_announcement_events="
                  << (audio_ ? audio_->terminalAnnouncementEventCount() : 0U)
                  << " passenger_murmur_audio="
                  << (audio_ ? audio_->passengerMurmurEventCount() : 0U)
                  << " passing_train_audio="
                  << (audio_ ? audio_->passingTrainEventCount() : 0U)
                  << " rain_audio=" << (audio_ ? audio_->rainLevel() : 0.0f)
                  << " door_audio="
                  << (audio_ ? audio_->doorTransitionEventCount() : 0U)
                  << " toilet_door_audio="
                  << (audio_ ? audio_->toiletDoorEventCount() : 0U)
                  << " carpet_footsteps="
                  << (audio_ ? audio_->carpetFootstepEventCount() : 0U)
                  << " noncarpet_footsteps="
                  << (audio_ ? audio_->nonCarpetedFootstepEventCount() : 0U)
                  << " tunnel_audio="
                  << (audio_ && audio_->tunnelAcousticsActive() ? 1 : 0)
                  << " bridge_audio="
                  << (audio_ && audio_->bridgeAcousticsActive() ? 1 : 0)
                  << " hvac_audio="
                  << (audio_ && audio_->interiorAmbienceActive() ? 1 : 0)
                  << " traction_effort=" << (audio_ ? audio_->tractionEffort() : 0.0f)
                  << " braking_effort=" << (audio_ ? audio_->brakingEffort() : 0.0f)
                  << " audio_paused="
                  << (audio_ && audio_->paused() ? 1 : 0)
                  << " audio_playing_instances="
                  << (audio_ ? audio_->playingInstanceCount() : 0U)
                  << " audio_paused_instances="
                  << (audio_ ? audio_->pausedInstanceCount() : 0U)
                  << " schedule_deviation_s="
                  << service_.predictedScheduleDeviationSeconds()
                  << " map_open=" << (showMap_ ? 1 : 0)
                  << " hud_visible=" << (showPassengerHud_ ? 1 : 0)
                  << " paused=" << (paused_ ? 1 : 0)
                  << " route_delta_m=" << std::abs(
                         state.routeDistanceMetres-initialRouteDistanceMetres_)
                  << " player_x=" << playerX_
                  << " player_z=" << playerZ_
                  << " player_yaw_deg=" << playerYaw_ * 180.0f / kPi
                  << " seated=" << (seated_ ? 1 : 0)
                  << " sit_transitions=" << playerSitTransitionCount_
                  << " stand_transitions=" << playerStandTransitionCount_
                  << " toilet_door_open=" << (toiletDoorOpen_ ? 1 : 0)
                  << " inside_toilet=" << (playerInsideToilet() ? 1 : 0)
                  << " toilet_door_interactions="
                  << playerToiletDoorInteractionCount_
                  << " mouse_mode_acquires=" << relativeMouseModeAcquireCount_
                  << " mouse_mode_losses=" << relativeMouseModeLossCount_
                  << " display_open=" << (inspectDisplay_ ? 1 : 0)
                  << " camera_suspension_mdeg=" << std::lround(
                         cameraLongitudinalLean_*180.0f/kPi*1'000.0f)
                  << " camera_suspension_mm=" << std::lround(
                         cameraLongitudinalOffset_*1'000.0f)
                  << " screenshot_saved=" << (screenshotSaved_ ? 1 : 0)
                  << " foliage_material_triangles=" << renderStats_.foliageMaterialTriangles
                  << " vsync_requested=" << options_.verticalSyncEnabled
                  << " fixed_time_step=" << options_.fixedTimeStep
                  << " urban_street_pockets=" << renderStats_.urbanStreetPockets
                  << " facade_lighting=" << options_.facadeLightingMode
                  << " facade_shader_draws=" << renderStats_.facadeShaderDraws
                  << " water_shader_draws=" << renderStats_.waterShaderDraws
                  << " river_bank_clumps=" << renderStats_.riverBankClumps
                  << " bridge_parapet_posts=" << renderStats_.bridgeParapetPosts
                  << " tunnel_lining_rings=" << renderStats_.tunnelLiningRings
                  << " tunnel_marker_plates=" << renderStats_.tunnelMarkerPlates
                  << " fence_posts=" << renderStats_.fencePosts
                  << " cable_trough_units=" << renderStats_.cableTroughUnits
                  << " verge_flower_clumps=" << renderStats_.vergeFlowerClumps
                  << " palisade_posts=" << renderStats_.palisadePosts
                  << " house_outbuildings=" << renderStats_.houseOutbuildings
                  << " house_rear_wings=" << renderStats_.houseRearWings
                  << " block_penthouses=" << renderStats_.blockPenthouses
                  << " platform_amenities=" << renderStats_.platformAmenities
                  << " platform_seated=" << renderStats_.platformSeatedAvatars
                  << " platform_suitcases=" << renderStats_.platformSuitcaseTravellers
                  << " block_balconies=" << renderStats_.blockBalconies
                  << " canopy_valance_boards=" << renderStats_.canopyValanceBoards
                  << " tramline_segments=" << renderStats_.tramlineSegments
                  << " world_gpu_mb=" << renderStats_.worldGpuMegabytes
                  << " world_largest_material=" << renderStats_.largestWorldMaterial
                  << " world_largest_material_mb=" << renderStats_.largestWorldMaterialMegabytes
                  << " world_largest_material_vertices=" << renderStats_.largestWorldMaterialVertices
                  << " world_gpu_segments=" << renderStats_.worldGpuSegments
                  << " track_rebuilds=" << renderStats_.trackRebuilds
                  << " track_rebuild_total_ms=" << renderStats_.trackRebuildTotalMs
                  << " track_rebuild_max_ms=" << renderStats_.trackRebuildMaxMs
                  << " track_generation_max_ms=" << renderStats_.trackGenerationMaxMs
                  << " track_prefetch_hits=" << renderStats_.trackPrefetchHits
                  << " track_synchronous_builds=" << renderStats_.trackSynchronousBuilds
                  << " track_late_prefetches=" << renderStats_.trackLatePrefetches
                  << " field_boundary_trees=" << renderStats_.fieldBoundaryTrees
                  << " terminal_buffer_stops=" << renderStats_.terminalBufferStops
                  << " terminal_shed_columns=" << renderStats_.terminalShedColumns << '\n';
    }
}

void TrainSimulatorGame::Initialize()
{
    getWindowProperty().setTitleProperty("Northern Valley Express - passenger service");
    setIsMouseVisibleProperty(true);
    streaming_.update(service_.state().routeDistanceMetres);
    Game::Initialize();
}

void TrainSimulatorGame::LoadContent()
{
    auto& device = getGraphicsDeviceProperty();
    appliedMultisampleCount_ = device.getPresentationParametersProperty()
                                   .getMultiSampleCountProperty();
    scene_ = std::make_unique<Render::SceneRenderer>(
        device, settings_.vegetationDistanceMetres, settings_.graphicsQuality,
        service_.state().doorsOpen, options_.avatarRenderingEnabled);
    scene_->setFacadeLightingMode(options_.facadeLightingMode);
    spriteBatch_ = std::make_unique<SpriteBatch>(device);
    whitePixel_ = std::make_unique<Texture2D>(device, 1, 1);
    const Color pixel = Color::White;
    whitePixel_->SetData(&pixel, 1);
    font_ = std::make_unique<Render::BitmapFont>(device);
    if (options_.audioEnabled)
    {
        audio_ = std::make_unique<Audio::TrainAudio>(
            options_.assetDirectory / "audio",
            Audio::TrainAudioSettings{settings_.masterVolume, settings_.announcementVolume,
                                      settings_.passengerVolume, settings_.trainVolume},
            service_.state().doorsOpen, toiletDoorEffectivelyOpen());
        audio_->setPaused(paused_ || startMenu_);
        if (options_.interactionSmoke && !paused_ && audio_->available())
        {
            // Exercise the same transition used by Escape against CNA's real dummy mixer. This
            // catches a partial pause (for example, a spatial loop left playing) and a resume
            // that restarts only some of the instances before the broader interaction smoke.
            const std::size_t initiallyPlaying = audio_->playingInstanceCount();
            audio_->setPaused(true);
            if (!audio_->paused() || initiallyPlaying == 0
                || audio_->playingInstanceCount() != 0
                || audio_->pausedInstanceCount() != initiallyPlaying)
                throw std::runtime_error("Audio interaction smoke could not pause every live instance");
            audio_->setPaused(false);
            if (audio_->paused() || audio_->pausedInstanceCount() != 0
                || audio_->playingInstanceCount() != initiallyPlaying)
                throw std::runtime_error("Audio interaction smoke could not resume every live instance");
        }
    }
    applyEnvironmentSettings();
    previousKeyboard_ = Keyboard::GetState();
    previousMouse_ = Mouse::GetState();
}

bool TrainSimulatorGame::pressed(const KeyboardState& now, Keys key) const
{
    return now.IsKeyDown(key) && previousKeyboard_.IsKeyUp(key);
}

bool TrainSimulatorGame::playerInsideToilet() const noexcept
{
    return Sim::TrainInterior::insideToilet(playerX_, playerZ_);
}

bool TrainSimulatorGame::toiletDoorEffectivelyOpen() const noexcept
{
    return !passengers_.toiletOccupied()
        && (toiletDoorOpen_ || passengers_.toiletDoorRequestedOpen());
}

bool TrainSimulatorGame::canOccupy(float x, float z) const
{
    if (!Sim::TrainInterior::canOccupy(x, z, toiletDoorEffectivelyOpen())) return false;
    const bool enteringReservedToilet = passengers_.toiletReserved()
        && !playerInsideToilet() && Sim::TrainInterior::insideToilet(x, z);
    if (enteringReservedToilet) return false;

    // Standing and walking passengers occupy the aisle. If an NPC happens to choose an activity
    // on top of the player's current position, allow motion away but never deeper into the overlap.
    for (const Sim::Passenger& passenger : passengers_.passengers())
    {
        if (passenger.role == Sim::PassengerRole::Driver
            || passenger.state == Sim::PassengerState::Offboard
            || passenger.seat.has_value()) continue;
        const double candidate = std::hypot(static_cast<double>(x) - passenger.x,
                                            static_cast<double>(z) - passenger.z);
        const double current = std::hypot(static_cast<double>(playerX_) - passenger.x,
                                          static_cast<double>(playerZ_) - passenger.z);
        if (candidate < 0.48 && candidate < current) return false;
    }

    return true;
}

float TrainSimulatorGame::cameraTransitionRemaining() const noexcept
{
    const float progress = std::clamp(cameraTransitionProgress_,0.0f,1.0f);
    const float eased = progress * progress * (3.0f - 2.0f * progress);
    return 1.0f - eased;
}

void TrainSimulatorGame::transitionPlayerTo(float x, float z, float yaw) noexcept
{
    // Start from the camera's currently displayed pose rather than the previous logical target.
    // This keeps a rapid sit/stand/sit sequence continuous instead of restarting from a stale seat.
    const float remaining = cameraTransitionRemaining();
    const float displayedX = playerX_ + cameraTransitionOffsetX_ * remaining;
    const float displayedZ = playerZ_ + cameraTransitionOffsetZ_ * remaining;
    const float displayedYaw = MathHelper::WrapAngle(
        playerYaw_ + cameraTransitionYawOffset_ * remaining);

    playerX_ = x;
    playerZ_ = z;
    playerYaw_ = MathHelper::WrapAngle(yaw);
    cameraTransitionOffsetX_ = displayedX - playerX_;
    cameraTransitionOffsetZ_ = displayedZ - playerZ_;
    cameraTransitionYawOffset_ = MathHelper::WrapAngle(displayedYaw - playerYaw_);
    cameraTransitionProgress_ = 0.0f;
}

void TrainSimulatorGame::applyAudioSettings()
{
    if (!audio_) return;
    audio_->setSettings(Audio::TrainAudioSettings{
        settings_.masterVolume, settings_.announcementVolume,
        settings_.passengerVolume, settings_.trainVolume});
}

void TrainSimulatorGame::applyEnvironmentSettings()
{
    const auto weather = static_cast<Sim::WeatherMode>(settings_.weatherMode);
    if (scene_) scene_->setEnvironment(settings_.serviceStartMinutes,weather);
    if (audio_) audio_->setWeather(weather);
}

void TrainSimulatorGame::persistSettings() const
{
    try
    {
        Config::Settings persisted = Config::settingsForPersistence(
            settings_,loadedGraphicsQuality_,options_.graphicsQuality.has_value(),
            graphicsQualityAdjustedInMenu_);
        // Like --quality, --time and --weather are launch overrides, not preference updates.
        if ((options_.serviceStartMinutes || options_.weatherMode) && !environmentAdjustedInMenu_)
        {
            persisted.serviceStartMinutes = loadedServiceStartMinutes_;
            persisted.weatherMode = loadedWeatherMode_;
        }
        persisted.save(options_.userConfigFile);
    }
    catch (const std::exception& error)
    {
        std::cerr << "Could not persist settings: " << error.what() << '\n';
    }
}

std::optional<std::size_t> TrainSimulatorGame::interactableSeat() const
{
    double closest = 1.45;
    std::optional<std::size_t> selected;
    for (std::size_t index = 0; index < passengers_.seats().size(); ++index)
    {
        const Sim::Seat& seat = passengers_.seats()[index];
        if (!passengers_.seatAvailable(index)
            || !passengers_.seatApproachClear(index)
            || !Sim::TrainInterior::interactionTargetVisible(
                playerX_, playerZ_, playerYaw_, seat.x, seat.z))
            continue;
        const double distance = std::hypot(static_cast<double>(playerX_) - seat.x,
                                           static_cast<double>(playerZ_) - seat.z);
        if (distance < closest) { closest = distance; selected = index; }
    }
    return selected;
}

void TrainSimulatorGame::updatePlayer(float dt, const KeyboardState& keyboard, const MouseState& mouse)
{
    playerWalking_ = false;
    bool relativeModeActive = Mouse::getIsRelativeMouseModeEXTProperty();
    const bool windowActive = getIsActiveProperty();
    const bool captureAllowed = !showMap_ && !showHelp_ && !inspectDisplay_;
    const MouseCaptureAction captureAction = mouseCaptureAction(
        mouseCaptured_,relativeModeActive,windowActive,captureAllowed,
        mouse.getLeftButtonProperty() == ButtonState::Pressed);
    if (captureAction == MouseCaptureAction::ReleaseUnexpected
        || captureAction == MouseCaptureAction::ReleaseRequested)
    {
        releaseMouseCapture(captureAction == MouseCaptureAction::ReleaseUnexpected);
        relativeModeActive = false;
    }
    if (!windowActive) return;

    bool acquiredMouseThisFrame = false;
    if (captureAction == MouseCaptureAction::Acquire)
    {
        Mouse::setIsRelativeMouseModeEXTProperty(true);
        relativeModeActive = Mouse::getIsRelativeMouseModeEXTProperty();
        if (relativeModeActive)
        {
            mouseCaptured_ = true;
            ++relativeMouseModeAcquireCount_;
            acquiredMouseThisFrame = true;
            setIsMouseVisibleProperty(false);
        }
    }
    if (mouseCaptured_ && !acquiredMouseThisFrame)
    {
        const MouseLookDelta delta = decodeMouseLookDelta(
            mouse.getXProperty(),mouse.getYProperty(),
            previousMouse_.getXProperty(),previousMouse_.getYProperty(),relativeModeActive);
        if (delta.x != 0 || delta.y != 0) ++mouseMotionSamples_;
        const float lookSensitivity = settings_.mouseSensitivity * kMouseLookPlatformScale;
        playerYaw_ = MathHelper::WrapAngle(
            playerYaw_ + static_cast<float>(delta.x) * lookSensitivity);
        playerPitch_ = std::clamp(
            playerPitch_ - static_cast<float>(delta.y) * lookSensitivity,
            -1.45f, 1.45f);
    }

    if (seated_) return;
    float forward = 0.0f;
    float right = 0.0f;
    if (keyboard.IsKeyDown(Keys::W)) forward += 1.0f;
    if (keyboard.IsKeyDown(Keys::S)) forward -= 1.0f;
    if (keyboard.IsKeyDown(Keys::D)) right += 1.0f;
    if (keyboard.IsKeyDown(Keys::A)) right -= 1.0f;
    const float length = std::sqrt(forward * forward + right * right);
    if (length < 0.01f) return;
    forward /= length;
    right /= length;
    const float speed = 1.75f;
    const float previousX = playerX_;
    const float previousZ = playerZ_;
    const float dx = (std::sin(playerYaw_) * forward + std::cos(playerYaw_) * right) * speed * dt;
    const float dz = (-std::cos(playerYaw_) * forward + std::sin(playerYaw_) * right) * speed * dt;
    if (canOccupy(playerX_ + dx, playerZ_)) playerX_ += dx;
    if (canOccupy(playerX_, playerZ_ + dz)) playerZ_ += dz;
    playerWalking_ = std::hypot(playerX_ - previousX, playerZ_ - previousZ) > 0.001f;
}

void TrainSimulatorGame::releaseMouseCapture(bool unexpected)
{
    const bool relativeModeActive = Mouse::getIsRelativeMouseModeEXTProperty();
    const bool hadCaptureState = mouseCaptured_ || relativeModeActive;
    mouseCaptured_ = false;
    if (relativeModeActive) Mouse::setIsRelativeMouseModeEXTProperty(false);
    setIsMouseVisibleProperty(true);
    if (unexpected && hadCaptureState) ++relativeMouseModeLossCount_;
}

void TrainSimulatorGame::interact()
{
    if (inspectDisplay_)
    {
        inspectDisplay_ = false;
        return;
    }

    if (seated_)
    {
        constexpr std::array<float, 7> standOffsets = {0.0f, 0.65f, -0.65f, 1.20f,
                                                       -1.20f, 1.75f, -1.75f};
        std::optional<float> safeZ;
        for (float offset : standOffsets)
            if (canOccupy(0.0f, playerZ_ + offset))
            {
                safeZ = playerZ_ + offset;
                break;
            }
        if (!safeZ.has_value()) return;
        seated_ = false;
        ++playerStandTransitionCount_;
        passengers_.releasePlayerSeat();
        playerSeat_.reset();
        transitionPlayerTo(0.0f,*safeZ,playerYaw_);
        return;
    }

    const auto& conductor = passengers_.passengers()[passengers_.conductorIndex()];
    if (Sim::TrainInterior::interactionTargetVisible(
            playerX_, playerZ_, playerYaw_, conductor.x, conductor.z, 1.6))
    {
        conductorInfoRemaining_ = 6.0f;
        return;
    }

    if (Sim::TrainInterior::interactionTargetVisible(
            playerX_, playerZ_, playerYaw_, 0.34, 4.1, 1.8))
    {
        if (passengers_.toiletReserved()) return;
        toiletDoorOpen_ = !toiletDoorOpen_;
        ++playerToiletDoorInteractionCount_;
        return;
    }

    // The narrow zone directly below an overhead screen takes precedence over seats one metre
    // away. Outside that zone the ordinary closest-seat interaction remains unchanged.
    if (Sim::TrainInterior::informationDisplayCar(playerZ_).has_value())
    {
        inspectDisplay_ = !inspectDisplay_;
        return;
    }

    const std::optional<std::size_t> selected = interactableSeat();
    if (selected.has_value())
    {
        if (!passengers_.occupySeatForPlayer(*selected)) return;
        const Sim::Seat& seat = passengers_.seats()[*selected];
        seated_ = true;
        ++playerSitTransitionCount_;
        playerSeat_ = selected;
        transitionPlayerTo(static_cast<float>(seat.x),static_cast<float>(seat.z),
                           seat.x < 0.0 ? -kPi * 0.5f : kPi * 0.5f);
        return;
    }

}

void TrainSimulatorGame::leaveStartMenu()
{
    startMenu_ = false;
    if (audio_) audio_->setPaused(paused_);
}

void TrainSimulatorGame::runInteractionSmoke()
{
    const auto require = [](bool condition, const char* message) {
        if (!condition) throw std::runtime_error(
            std::string("interaction smoke failed: ") + message);
    };

    switch (interactionSmokeStage_++)
    {
        case 0:
            playerX_ = 0.0f;
            playerZ_ = static_cast<float>(Sim::TrainInterior::FirstCarCentre);
            require(interactionPrompt() == "E  INSPECT PASSENGER INFORMATION",
                    "display prompt was not selected at the car centre");
            interact();
            require(inspectDisplay_, "display inspection did not open");
            break;
        case 1:
            playerZ_ += 3.0f;
            interact();
            require(!inspectDisplay_, "display inspection could not close after moving away");
            break;
        case 2:
        {
            const auto seat = std::find_if(passengers_.seats().begin(), passengers_.seats().end(),
                [this](const Sim::Seat& candidate) {
                    return candidate.car == 2 && passengers_.seatAvailable(candidate.id)
                        && passengers_.seatApproachClear(candidate.id);
                });
            require(seat != passengers_.seats().end(), "no free seat was available in car 3");
            playerX_ = 0.0f;
            playerZ_ = static_cast<float>(seat->z);
            playerYaw_ = seat->x < 0.0 ? -kPi * 0.5f : kPi * 0.5f;
            require(!interactionPrompt().empty(), "available seat did not offer an interaction");
            interact();
            require(seated_ && playerSeat_.has_value(), "seat interaction did not seat the player");
            require(passengers_.playerSeat() == playerSeat_, "player seat ownership diverged");
            require(cameraTransitionProgress_ < 1.0f
                        && std::hypot(cameraTransitionOffsetX_,cameraTransitionOffsetZ_) > 0.5f,
                    "seat interaction did not begin a spatial camera transition");
            break;
        }
        case 3:
            interact();
            require(!seated_ && !playerSeat_.has_value(), "stand interaction did not release seat");
            require(!passengers_.playerSeat().has_value(), "seat remained reserved after standing");
            break;
        case 4:
            require(!passengers_.toiletReserved(), "toilet unexpectedly reserved at smoke start");
            playerX_ = 0.0f;
            playerZ_ = 4.1f;
            playerYaw_ = kPi * 0.5f;
            interact();
            require(toiletDoorOpen_, "toilet door did not open");
            playerX_ = 0.66f;
            playerZ_ = 5.70f;
            require(playerInsideToilet(), "player did not enter toilet volume");
            require(canOccupy(playerX_, playerZ_), "open toilet interior rejected the player");
            interactionSmokeRenderCheckpoint_ = framesDrawn_;
            break;
        case 5:
            if (framesDrawn_ <= interactionSmokeRenderCheckpoint_)
            {
                --interactionSmokeStage_;
                break;
            }
            require(playerInsideToilet(), "player left toilet before closing its door");
            require(renderStats_.occupiedToiletIndicators == 1,
                    "toilet indicator did not show occupied for the player");
            playerYaw_ = 0.0f;
            interact();
            require(!toiletDoorOpen_, "toilet door did not close from inside");
            interactionSmokeRenderCheckpoint_ = framesDrawn_;
            break;
        case 6:
        {
            if (framesDrawn_ <= interactionSmokeRenderCheckpoint_)
            {
                --interactionSmokeStage_;
                break;
            }
            require(playerInsideToilet(), "closing the toilet door displaced the player");
            require(renderStats_.occupiedToiletIndicators == 1,
                    "closed toilet indicator did not remain occupied for the player");
            const Sim::Passenger& conductor =
                passengers_.passengers()[passengers_.conductorIndex()];

            // Exercise the production player/NPC collision path independently of the conductor
            // prompt. The player may back away from a close actor but may not step through one.
            playerX_ = static_cast<float>(conductor.x);
            playerZ_ = static_cast<float>(conductor.z + 0.70);
            require(canOccupy(playerX_,static_cast<float>(conductor.z + 0.90)),
                    "player could not retreat from the conductor");
            require(!canOccupy(playerX_,static_cast<float>(conductor.z + 0.40)),
                    "player collision allowed walking through the conductor");

            playerX_ = static_cast<float>(conductor.x + 0.70);
            playerZ_ = static_cast<float>(conductor.z);
            playerYaw_ = -kPi * 0.5f;
            interact();
            require(conductorInfoRemaining_ > 0.0f,
                    "conductor interaction did not show service information");
            break;
        }
        case 7:
        {
            bool foundClearAisle = false;
            playerX_ = 0.0f;
            playerYaw_ = 0.0f;
            for (int step = 0; step <= 110; ++step)
            {
                const float z = -55.0f + static_cast<float>(step);
                playerZ_ = z;
                if (!Sim::TrainInterior::nonCarpetedFloorAt(0.0,z)
                    && !Sim::TrainInterior::nonCarpetedFloorAt(0.0,z-0.20f)
                    && canOccupy(0.0f,z-0.20f))
                {
                    foundClearAisle = true;
                    break;
                }
            }
            require(foundClearAisle, "no clear aisle position was available for movement audio");
            const float previousZ = playerZ_;
            updatePlayer(0.10f, KeyboardState{Keys::W}, MouseState{});
            require(playerWalking_ && playerZ_ < previousZ,
                    "successful keyboard movement did not publish the walking signal");
            if (audio_)
            {
                const Sim::RoutePose pose = route_.sample(service_.state().routeDistanceMetres);
                audio_->update(0.10, route_, service_, passengers_, streaming_,
                               playerX_, playerZ_, playerYaw_,
                               playerWalking_, toiletDoorEffectivelyOpen(),
                               pose.tunnel, pose.bridge);
                require(audio_->carpetFootstepEventCount() >= 1,
                        "carpet movement did not select a carpet footstep");
                audio_->update(0.50, route_, service_, passengers_, streaming_,
                               playerX_, playerZ_, playerYaw_, false,
                               toiletDoorEffectivelyOpen(), pose.tunnel, pose.bridge);

                bool foundClearVestibule = false;
                constexpr float minimum = static_cast<float>(
                    Sim::TrainInterior::PassengerMinimumZ + 0.20);
                constexpr float maximum = static_cast<float>(
                    Sim::TrainInterior::PassengerMaximumZ - 0.20);
                const int surfaceSteps = static_cast<int>(
                    std::ceil((maximum-minimum)/0.20f));
                for (int step = 0; step <= surfaceSteps; ++step)
                {
                    const float z = std::min(minimum+static_cast<float>(step)*0.20f,maximum);
                    if (!Sim::TrainInterior::nonCarpetedFloorAt(0.0,z)
                        || !Sim::TrainInterior::nonCarpetedFloorAt(0.0,z-0.20f)
                        || !canOccupy(0.0f,z-0.20f)) continue;
                    playerX_ = 0.0f;
                    playerZ_ = z;
                    foundClearVestibule = true;
                    break;
                }
                require(foundClearVestibule,
                        "no clear non-carpeted position was available for movement audio");
                const float vestibulePreviousZ = playerZ_;
                updatePlayer(0.10f, KeyboardState{Keys::W}, MouseState{});
                require(playerWalking_ && playerZ_ < vestibulePreviousZ,
                        "non-carpeted movement did not publish the walking signal");
                audio_->update(0.10, route_, service_, passengers_, streaming_,
                               playerX_, playerZ_, playerYaw_,
                               playerWalking_, toiletDoorEffectivelyOpen(),
                               pose.tunnel, pose.bridge);
                require(audio_->nonCarpetedFootstepEventCount() >= 1,
                        "vestibule movement did not select a non-carpeted footstep");
            }
            updatePlayer(0.10f, KeyboardState{}, MouseState{});
            require(!playerWalking_, "idle player retained the walking signal");
            break;
        }
        case 8:
        {
            // Traverse the exact player collision path through every saloon and gangway in both
            // directions. The small lateral alternatives use the same axis-separated movement
            // rule as normal input and let the smoke route around the roaming conductor.
            const auto moveToZ = [this](float targetZ) {
                const std::array<float, 4> candidateX = {playerX_, 0.0f, -0.36f, 0.36f};
                for (float x : candidateX)
                {
                    if (std::abs(x - playerX_) > 0.01f && !canOccupy(x, playerZ_)) continue;
                    if (!canOccupy(x, targetZ)) continue;
                    playerX_ = x;
                    playerZ_ = targetZ;
                    return true;
                }
                return false;
            };

            const float minimum = static_cast<float>(Sim::TrainInterior::PassengerMinimumZ + 0.10);
            const float maximum = static_cast<float>(Sim::TrainInterior::PassengerMaximumZ - 0.10);
            const int traversalSteps = static_cast<int>(
                std::ceil((maximum-minimum)/0.20f));
            require(moveToZ(minimum), "could not enter the first passenger-car aisle");
            for (int step = 1; step <= traversalSteps; ++step)
                require(moveToZ(std::min(
                            minimum+static_cast<float>(step)*0.20f,maximum)),
                        "forward traversal was blocked inside the accessible consist");
            require(playerZ_ > 62.3f, "forward traversal did not reach the final passenger car");
            require(!canOccupy(0.0f, 63.0f), "forward driving cab was not physically blocked");

            for (int step = 1; step <= traversalSteps; ++step)
                require(moveToZ(std::max(
                            maximum-static_cast<float>(step)*0.20f,minimum)),
                        "reverse traversal was blocked inside the accessible consist");
            require(playerZ_ < -62.3f, "reverse traversal did not return to the first car");
            require(!canOccupy(0.0f, -63.0f), "rear driving cab was not physically blocked");
            break;
        }
        case 9:
        {
            require(service_.state().doorsOpen,
                    "platform boundary check did not begin with open doors");
            const float platformSide = Sim::platformOnNegativeTrainX(service_.state())
                ? -1.0f : 1.0f;
            const float firstDoorZ = static_cast<float>(Sim::TrainInterior::passengerDoorZ(0));
            playerX_ = platformSide;
            playerZ_ = firstDoorZ;
            require(interactionPrompt() == "THIS SERVICE CONTINUES - PLEASE REMAIN ABOARD",
                    "open platform door did not explain the onboard boundary");
            require(canOccupy(platformSide * 1.26f, firstDoorZ),
                    "open doorway rejected the inner threshold");
            require(!canOccupy(platformSide * 1.28f, firstDoorZ),
                    "open doorway allowed the player outside the train body");
            break;
        }
        default:
            if (audio_)
                require(audio_->toiletDoorEventCount() >= 2,
                        "toilet open and close did not emit spatial door audio");
            std::cout << "INTERACTION_SMOKE passed display seat stand toilet conductor "
                         "npc-collision surface-footstep-audio toilet-door-audio "
                         "six-car-traversal "
                         "cab-boundaries "
                         "platform-boundary\n";
            Exit();
            break;
    }
}

float TrainSimulatorGame::stepSeconds(const GameTime& gameTime)
{
    const auto& step = options_.lockstep ? getTargetElapsedTimeProperty()
                                         : gameTime.getElapsedGameTimeProperty();
    return std::clamp(static_cast<float>(step.getTotalSecondsProperty()),0.0f,0.10f);
}

void TrainSimulatorGame::Update(GameTime& gameTime)
{
    const float dt = stepSeconds(gameTime);
    const KeyboardState keyboard = Keyboard::GetState();
    const MouseState mouse = Mouse::GetState();

    // A finite QA run freezes its authoritative state once the requested frame has been drawn,
    // but may need a few additional render-only frames to let CNA's asynchronous avatar loader
    // become idle. This preserves --frames semantics while avoiding teardown during a load.
    if (finishingAvatarWarmup_)
    {
        previousKeyboard_ = keyboard;
        previousMouse_ = mouse;
        Game::Update(gameTime);
        return;
    }

    // The service does not run until the passenger leaves the start menu. The button fires on
    // release, like a desktop button, so the click that starts the game cannot also capture the
    // mouse for first-person look on the following frame.
    if (startMenu_)
    {
        const auto& viewport = getGraphicsDeviceProperty().getViewportProperty();
        const StartButton button = startButton(viewport.getWidthProperty(),
                                               viewport.getHeightProperty());
        const bool clicked = getIsActiveProperty()
            && mouse.getLeftButtonProperty() == ButtonState::Released
            && previousMouse_.getLeftButtonProperty() == ButtonState::Pressed
            && button.contains(mouse.getXProperty(),mouse.getYProperty());
        if (clicked || pressed(keyboard, Keys::Enter) || pressed(keyboard, Keys::Space))
            leaveStartMenu();
        if (pressed(keyboard, Keys::F12)) Exit();
        previousKeyboard_ = keyboard;
        previousMouse_ = mouse;
        Game::Update(gameTime);
        return;
    }

    if (pressed(keyboard, Keys::Escape))
    {
        paused_ = !paused_;
        if (audio_) audio_->setPaused(paused_);
        if (mouseCaptured_ || Mouse::getIsRelativeMouseModeEXTProperty())
        {
            releaseMouseCapture(false);
        }
    }
    // CNA reserves F9/F10 for renderer context-loss diagnostics before game input is updated.
    // Keep the player-facing quit binding away from those dependency-owned debug hotkeys.
    if (pressed(keyboard, Keys::F12)) Exit();
    if (pressed(keyboard, Keys::M)) showMap_ = !showMap_;
    if (pressed(keyboard, Keys::H)) showPassengerHud_ = !showPassengerHud_;
    if (pressed(keyboard, Keys::F1)) showHelp_ = !showHelp_;
    if (pressed(keyboard, Keys::F3)) showDebug_ = !showDebug_;
    if (pressed(keyboard, Keys::F5)) service_.freeze(!service_.frozen());
    if (pressed(keyboard, Keys::F6))
    {
        passengers_.resetStationCycleForTeleport();
        passengerDoorsOpenRealSeconds_ = 0.0;
        passengerDoorPassageReleased_ = false;
        const std::size_t station = service_.state().currentStation == 0
            ? 0 : service_.state().currentStation - 1;
        service_.teleportToStation(station, -1);
        streaming_.update(service_.state().routeDistanceMetres);
    }
    if (pressed(keyboard, Keys::F7))
    {
        passengers_.resetStationCycleForTeleport();
        passengerDoorsOpenRealSeconds_ = 0.0;
        passengerDoorPassageReleased_ = false;
        const std::size_t station = std::min(service_.state().currentStation + 1,
                                             route_.stations().size() - 1);
        service_.teleportToStation(station, 1);
        streaming_.update(service_.state().routeDistanceMetres);
    }
    if (paused_)
    {
        constexpr int pauseItems = 12;
        if (pressed(keyboard, Keys::Up))
            pauseSelection_ = (pauseSelection_ + pauseItems - 1) % pauseItems;
        if (pressed(keyboard, Keys::Down))
            pauseSelection_ = (pauseSelection_ + 1) % pauseItems;
        const float direction = pressed(keyboard, Keys::Left) ? -1.0f
                              : pressed(keyboard, Keys::Right) ? 1.0f : 0.0f;
        if (direction != 0.0f && pauseSelection_ == 0)
            settings_.fieldOfViewDegrees = std::clamp(
                settings_.fieldOfViewDegrees + direction * 5.0f, 55.0f, 100.0f);
        if (direction != 0.0f && pauseSelection_ == 1)
            settings_.mouseSensitivity = std::clamp(
                settings_.mouseSensitivity + direction * 0.0003f, 0.0005f, 0.010f);
        if (direction != 0.0f && pauseSelection_ == 2)
            settings_.cameraVibration = std::clamp(
                settings_.cameraVibration + direction * 0.10f, 0.0f, 1.0f);
        if (direction != 0.0f && pauseSelection_ == 3)
        {
            settings_.graphicsQuality = std::clamp(
                settings_.graphicsQuality + static_cast<int>(direction), 0, 2);
            graphicsQualityAdjustedInMenu_ = true;
            if (scene_) scene_->setGraphicsQuality(settings_.graphicsQuality);
        }
        float* selectedVolume = pauseSelection_ == 4 ? &settings_.masterVolume
                              : pauseSelection_ == 5 ? &settings_.trainVolume
                              : pauseSelection_ == 6 ? &settings_.announcementVolume
                              : pauseSelection_ == 7 ? &settings_.passengerVolume : nullptr;
        if (direction != 0.0f && selectedVolume != nullptr)
        {
            *selectedVolume = std::clamp(*selectedVolume + direction * 0.05f, 0.0f, 1.0f);
            applyAudioSettings();
        }
        if (direction != 0.0f && pauseSelection_ == 8)
        {
            settings_.vegetationDistanceMetres = std::clamp(
                settings_.vegetationDistanceMetres + direction * 100.0f, 250.0f, 1'000.0f);
            if (scene_) scene_->setVegetationDistance(settings_.vegetationDistanceMetres);
        }
        if (direction != 0.0f && pauseSelection_ == 9)
            settings_.announcementCaptions = !settings_.announcementCaptions;
        if (direction != 0.0f && (pauseSelection_ == 10 || pauseSelection_ == 11))
        {
            // Time moves the clock in half-hour steps; weather cycles through every mode.
            if (pauseSelection_ == 10)
                settings_.serviceStartMinutes = (settings_.serviceStartMinutes
                    + static_cast<int>(direction)*30 + 24*60) % (24*60);
            else
            {
                constexpr int modes = static_cast<int>(Sim::WeatherMode::Count);
                settings_.weatherMode = (settings_.weatherMode + static_cast<int>(direction)
                                         + modes) % modes;
            }
            environmentAdjustedInMenu_ = true;
            applyEnvironmentSettings();
        }
        if (direction != 0.0f) persistSettings();
    }
    if (pressed(keyboard, Keys::E) && !paused_ && !showMap_ && !showHelp_) interact();

    if (!paused_)
    {
        updatePlayer(dt, keyboard, mouse);
        conductorInfoRemaining_ = std::max(0.0f, conductorInfoRemaining_ - dt);
        const float seatedTarget = seated_ ? 1.0f : 0.0f;
        const float blendStep = dt / 0.65f;
        seatedBlend_ += std::clamp(seatedTarget - seatedBlend_, -blendStep, blendStep);
        cameraTransitionProgress_ = std::min(
            1.0f,cameraTransitionProgress_ + dt / 0.65f);
        if (service_.state().doorsOpen)
            passengerDoorsOpenRealSeconds_ += static_cast<double>(dt);
        else
        {
            passengerDoorsOpenRealSeconds_ = 0.0;
            passengerDoorPassageReleased_ = false;
        }
        const double requestedSimulationStep = static_cast<double>(dt) * options_.timeScale;
        const double boundedSimulationStep = std::min(requestedSimulationStep,2.0);
        // A very short development dwell or high time scale must not close the authoritative
        // doors before passengers have received one full update with a safely exposed opening.
        // Normal production dwells reach this guard only after it has already become inactive.
        const bool wouldCloseBeforePassageIsClear = service_.state().doorsOpen
            && service_.state().phase == Sim::ServicePhase::Dwelling
            && service_.state().dwellRemainingSeconds <= boundedSimulationStep+1.0e-9
            && !passengerDoorPassageReleased_;
        if (!wouldCloseBeforePassageIsClear)
            service_.update(requestedSimulationStep);
        if (!service_.state().doorsOpen)
        {
            passengerDoorsOpenRealSeconds_ = 0.0;
            passengerDoorPassageReleased_ = false;
        }
        const bool atStation = service_.state().phase == Sim::ServicePhase::Dwelling;
        const bool approachingStation = service_.state().phase == Sim::ServicePhase::Coasting
            || service_.state().phase == Sim::ServicePhase::Braking;
        const bool platformOnNegativeX = atStation
            ? Sim::platformOnNegativeTrainX(service_.state())
            : service_.nextPlatformOnNegativeTrainX();
        passengers_.update(static_cast<double>(dt), atStation, service_.state().doorsOpen,
                           platformOnNegativeX ? -1.0 : 1.0, service_.state().direction,
                           Sim::TrainInterior::toiletAvailableToPassengers(
                               playerX_,playerZ_,toiletDoorOpen_),
                           playerX_, playerZ_, approachingStation);
        if (atStation && service_.state().doorsOpen
            && passengerDoorsOpenRealSeconds_
                >= Sim::TrainInterior::PassengerDoorPassageSeconds)
            passengerDoorPassageReleased_ = true;
        streaming_.update(service_.state().routeDistanceMetres);
        if (audio_)
        {
            const Sim::RoutePose routePose = route_.sample(service_.state().routeDistanceMetres);
            audio_->update(dt, route_, service_, passengers_, streaming_,
                           playerX_, playerZ_, playerYaw_,
                           playerWalking_, toiletDoorEffectivelyOpen(),
                           routePose.tunnel, routePose.bridge);
        }
        elapsedSeconds_ += dt;
        if (options_.interactionSmoke) runInteractionSmoke();
    }

    if (options_.reportState && options_.frameLimit == 0)
    {
        const auto now=std::chrono::steady_clock::now();
        if (now-lastInputTrace_ >= std::chrono::seconds(5))
        {
            const double traceIntervalSeconds = lastInputTrace_.time_since_epoch().count() == 0
                ? 0.0 : std::chrono::duration<double>(now-lastInputTrace_).count();
            lastInputTrace_=now;
            const auto steadyTiming = steadyFrameTiming_.summary();
            std::ostringstream nearbyActors;
            nearbyActors << std::fixed << std::setprecision(3);
            for (std::size_t i = 0; i < passengers_.passengers().size(); ++i)
            {
                const auto& actor = passengers_.passengers()[i];
                if (actor.role == Sim::PassengerRole::Driver || actor.seat.has_value()
                    || actor.state == Sim::PassengerState::Offboard
                    || std::hypot(actor.x-playerX_,actor.z-playerZ_) > 2.0) continue;
                if (nearbyActors.tellp() > 0) nearbyActors << ';';
                nearbyActors << i << ',' << actor.x << ',' << actor.z << ','
                             << static_cast<int>(actor.state);
            }
            std::cout << "INPUT_TRACE focus=" << getIsActiveProperty()
                      << " captured=" << mouseCaptured_
                      << " native=" << Mouse::getIsRelativeMouseModeEXTProperty()
                      << " paused=" << paused_ << " map=" << showMap_
                      << " motion_samples=" << mouseMotionSamples_
                      << " yaw_rad=" << playerYaw_ << " pitch_rad=" << playerPitch_
                      << " x=" << playerX_ << " z=" << playerZ_
                      << " help=" << showHelp_ << " display=" << inspectDisplay_
                      << " frames=" << framesDrawn_
                      << " trace_interval_s=" << traceIntervalSeconds
                      << " elapsed_s=" << elapsedSeconds_
                      << " route_km=" << service_.state().routeDistanceMetres/1000.0
                      << " direction=" << service_.state().direction
                      << " phase=" << service_.phaseName()
                      << " station_legs=" << service_.state().completedLegs
                      << " loaded_chunks=" << streaming_.chunks().size()
                      << " generated_chunks=" << streaming_.generatedChunkCount()
                      << " passengers=" << passengers_.passengers().size()
                      << " avatar_visuals=" << renderStats_.avatarVisuals
                      << " ready_avatar_visuals=" << renderStats_.readyAvatarVisuals
                      << " world_rebuilds=" << renderStats_.worldRebuilds
                      << " world_prefetch_hits=" << renderStats_.worldPrefetchHits
                      << " world_late_prefetches=" << renderStats_.worldLatePrefetches
                      << " audio_playing_instances=" << (audio_ ? audio_->playingInstanceCount() : 0U)
                      << " audio_paused_instances=" << (audio_ ? audio_->pausedInstanceCount() : 0U)
                      << " steady_frame_samples=" << steadyTiming.windowSamples
                      << " steady_frame_p99_ms=" << steadyTiming.p99Ms
                      << " seated=" << seated_
                      << " toilet_door_open=" << toiletDoorOpen_
                      << " inside_toilet=" << playerInsideToilet()
                      << " gangway_doors=" << renderStats_.openGangwayDoors
                      << " nearby_actors=" << (nearbyActors.str().empty() ? "none" : nearbyActors.str())
                      << std::endl;
        }
    }
    previousKeyboard_ = keyboard;
    previousMouse_ = mouse;
    Game::Update(gameTime);
}

void TrainSimulatorGame::drawPanel(int x, int y, int width, int height, const Color& color)
{
    spriteBatch_->Draw(*whitePixel_, Rectangle(x,y,width,height), color);
}

void TrainSimulatorGame::drawText(const std::string& text, float x, float y,
                                  const Color& color, float scale)
{
    font_->draw(*spriteBatch_, text, Vector2{x+scale,y+scale}, Color(0,0,0,180), scale);
    font_->draw(*spriteBatch_, text, Vector2{x,y}, color, scale);
}

std::string TrainSimulatorGame::interactionPrompt() const
{
    if (seated_) return "E  STAND UP";
    if (std::abs(playerZ_) > 60.8f) return "DRIVER CAB - STAFF ONLY";
    if (service_.state().doorsOpen && std::abs(playerX_) > 0.88f)
    {
        const bool playerOnNegativeX = playerX_ < 0.0f;
        const bool platformOnNegativeX = Sim::platformOnNegativeTrainX(service_.state());
        if (playerOnNegativeX == platformOnNegativeX)
            for (int car = 0; car < Sim::TrainInterior::PassengerCars; ++car)
            {
                const float doorZ = static_cast<float>(
                    Sim::TrainInterior::passengerDoorZ(car));
                if (std::abs(playerZ_ - doorZ) < 1.25f)
                    return "THIS SERVICE CONTINUES - PLEASE REMAIN ABOARD";
            }
    }
    const auto& conductor = passengers_.passengers()[passengers_.conductorIndex()];
    if (Sim::TrainInterior::interactionTargetVisible(
            playerX_, playerZ_, playerYaw_, conductor.x, conductor.z, 1.6))
        return "E  ASK CONDUCTOR ABOUT THIS SERVICE";
    if (Sim::TrainInterior::interactionTargetVisible(
            playerX_, playerZ_, playerYaw_, 0.34, 4.1, 1.8))
    {
        if (passengers_.toiletReserved()) return "TOILET OCCUPIED";
        return toiletDoorOpen_ ? "E  CLOSE TOILET DOOR" : "E  OPEN ACCESSIBLE TOILET";
    }
    if (Sim::TrainInterior::informationDisplayCar(playerZ_).has_value())
        return "E  INSPECT PASSENGER INFORMATION";
    if (const std::optional<std::size_t> selected = interactableSeat(); selected.has_value())
    {
        const Sim::Seat& seat = passengers_.seats()[*selected];
        return seat.window ? "E  SIT BY WINDOW" : "E  SIT DOWN";
    }
    return {};
}

void TrainSimulatorGame::drawMap()
{
    const auto& viewport = getGraphicsDeviceProperty().getViewportProperty();
    const int width = viewport.getWidthProperty();
    const int height = viewport.getHeightProperty();
    const int x = width / 12;
    const int y = height / 5;
    const int panelWidth = width * 5 / 6;
    const int panelHeight = height * 3 / 5;
    drawPanel(x,y,panelWidth,panelHeight,Color(12,27,36,232));
    drawText("NORTHERN VALLEY EXPRESS", static_cast<float>(x+28), static_cast<float>(y+24), Color(235,243,245,255), 3.0f);
    drawText("ALDERWICK CENTRAL  < >  WESTHAVEN CENTRAL", static_cast<float>(x+28),
             static_cast<float>(y+58), Color(134,206,219,255), 2.0f);

    const int lineX = x + 55;
    const int lineWidth = panelWidth - 110;
    const int lineY = y + 135;
    const int etaMinutes = static_cast<int>(
        std::ceil(service_.estimatedSecondsToNextStation() / 60.0));
    drawText("TO " + service_.destinationName() + "  -  NEXT "
                 + route_.stations()[service_.state().nextStation].name + "  -  ETA "
                 + std::to_string(etaMinutes) + " MIN",
             static_cast<float>(x+28), static_cast<float>(y+91),
             Color(225,234,231,255), 1.5f);

    const float routeProgress = static_cast<float>(
        service_.state().routeDistanceMetres / route_.lengthMetres());
    const int trainX = lineX + static_cast<int>(routeProgress * static_cast<float>(lineWidth));
    drawPanel(lineX,lineY,lineWidth,5,Color(80,174,194,255));
    if (service_.state().direction > 0)
        drawPanel(lineX,lineY,std::max(0,trainX-lineX),5,Color(75,98,103,255));
    else
        drawPanel(trainX,lineY,std::max(0,lineX+lineWidth-trainX),5,Color(75,98,103,255));
    for (std::size_t index = 0; index < route_.stations().size(); ++index)
    {
        const auto& station = route_.stations()[index];
        const float progress = static_cast<float>(station.distanceMetres / route_.lengthMetres());
        const int sx = lineX + static_cast<int>(progress * static_cast<float>(lineWidth));
        const bool passed = service_.state().direction > 0
            ? station.distanceMetres < service_.state().routeDistanceMetres
            : station.distanceMetres > service_.state().routeDistanceMetres;
        drawPanel(sx-5,lineY-7,14,19,passed ? Color(89,116,121,255) : Color(235,241,236,255));
        drawText(std::to_string(index + 1), static_cast<float>(sx-4),
                 static_cast<float>(lineY + (index%2==0 ? 22 : -27)),
                 Color(218,227,228,255), 1.1f);
    }
    drawPanel(trainX-8,lineY-18,20,13,Color(242,180,46,255));
    drawText(service_.state().direction > 0 ? ">" : "<", static_cast<float>(trainX-2),
             static_cast<float>(lineY-18), Color(20,28,30,255), 1.5f);

    const int legendY = y + 169;
    drawPanel(x+35,legendY,12,12,Color(89,116,121,255));
    drawText("PASSED",static_cast<float>(x+53),static_cast<float>(legendY-1),
             Color(152,174,177,255),1.1f);
    drawPanel(x+142,legendY,15,10,Color(242,180,46,255));
    drawText("TRAIN",static_cast<float>(x+164),static_cast<float>(legendY-1),
             Color(242,197,79,255),1.1f);
    drawPanel(x+247,legendY,12,12,Color(235,241,236,255));
    drawText("UPCOMING",static_cast<float>(x+265),static_cast<float>(legendY-1),
             Color(222,232,231,255),1.1f);

    // Full-name legend keeps all twelve stations readable while the line above preserves scale.
    for (std::size_t index = 0; index < route_.stations().size(); ++index)
    {
        const bool rightColumn = index >= 6;
        const std::size_t row = index % 6;
        const auto& station = route_.stations()[index];
        const bool passed = service_.state().direction > 0
            ? station.distanceMetres < service_.state().routeDistanceMetres
            : station.distanceMetres > service_.state().routeDistanceMetres;
        const bool next = index == service_.state().nextStation;
        const Color statusColor = next ? Color(246,188,45,255)
                                      : passed ? Color(112,139,144,255)
                                               : Color(222,232,231,255);
        std::ostringstream label;
        label << std::setw(2) << index + 1 << "  " << station.name
              << "  " << std::fixed << std::setprecision(1)
              << station.distanceMetres / 1000.0 << " KM";
        drawText(label.str(), static_cast<float>(x + 35 + (rightColumn ? panelWidth/2 : 0)),
                 static_cast<float>(y + 205 + static_cast<int>(row) * 29), statusColor, 1.35f);
    }
    std::ostringstream liveStatus;
    liveStatus << "CURRENT " << std::fixed << std::setprecision(1)
               << service_.state().routeDistanceMetres / 1000.0 << " KM  -  "
               << service_.phaseName() << "  -  "
               << scheduleText(service_.predictedScheduleDeviationSeconds());
    drawText(liveStatus.str(), static_cast<float>(x+28), static_cast<float>(y+panelHeight-30),
             Color(160,196,201,255), 1.35f);
    drawText("M  CLOSE MAP", static_cast<float>(x+panelWidth-145), static_cast<float>(y+panelHeight-30),
             Color(160,180,184,255), 1.5f);
}

void TrainSimulatorGame::drawStartMenu()
{
    auto& device = getGraphicsDeviceProperty();
    const auto& viewport = device.getViewportProperty();
    const int width = viewport.getWidthProperty();
    const int height = viewport.getHeightProperty();
    device.Clear(Color(14,38,50,255));
    spriteBatch_->Begin();

    // Calm dusk-blue backdrop from the HUD palette, darkening gently toward the bottom edge.
    constexpr int bands = 48;
    for (int band = 0; band < bands; ++band)
    {
        const float t = static_cast<float>(band) / static_cast<float>(bands-1);
        const auto channel = [t](int top, int bottom) {
            return static_cast<int>(std::lround(
                static_cast<float>(top) + static_cast<float>(bottom-top)*t));
        };
        const int top = height*band/bands;
        const int bottom = height*(band+1)/bands;
        drawPanel(0,top,width,bottom-top,
                  Color(channel(36,10),channel(82,30),channel(102,40),255));
    }

    const std::string title = "NORTHERN VALLEY EXPRESS";
    constexpr float titleScale = 4.0f;
    drawText(title,(static_cast<float>(width)-font_->measure(title,titleScale))*0.5f,
             static_cast<float>(height/2-130),Color(235,243,245,255),titleScale);
    const std::string subtitle = "PASSENGER TRAIN SIMULATOR";
    drawText(subtitle,(static_cast<float>(width)-font_->measure(subtitle,2.0f))*0.5f,
             static_cast<float>(height/2-78),Color(123,213,225,255),2.0f);

    const StartButton button = startButton(width,height);
    const bool hovered = button.contains(previousMouse_.getXProperty(),
                                         previousMouse_.getYProperty());
    drawPanel(button.x-3,button.y-3,button.width+6,button.height+6,Color(7,21,29,230));
    drawPanel(button.x,button.y,button.width,button.height,
              hovered ? Color(255,209,96,255) : Color(245,190,49,255));
    const std::string label = "START";
    constexpr float labelScale = 3.5f;
    font_->draw(*spriteBatch_,label,
                Vector2{(static_cast<float>(width)-font_->measure(label,labelScale))*0.5f,
                        static_cast<float>(button.y)
                            + (static_cast<float>(button.height)-7.0f*labelScale)*0.5f},
                Color(15,26,31,255),labelScale);

    const std::string hint = "ENTER  START     F12  QUIT";
    drawText(hint,(static_cast<float>(width)-font_->measure(hint,1.45f))*0.5f,
             static_cast<float>(button.y+button.height+34),Color(188,205,207,255),1.45f);
    spriteBatch_->End();
}

void TrainSimulatorGame::drawUi()
{
    const auto& state = service_.state();
    const auto& viewport = getGraphicsDeviceProperty().getViewportProperty();
    const int width = viewport.getWidthProperty();
    const int height = viewport.getHeightProperty();
    spriteBatch_->Begin();

    // The compact display is convenient while exploring, but a passenger can hide it for an
    // unobstructed long window ride. Interaction prompts, announcements, maps and menus remain
    // independent so clean-view mode does not remove essential feedback.
    if (showPassengerHud_)
    {
        drawPanel(18,18,455,112,Color(7,21,29,218));
        drawText("NORTHERN VALLEY EXPRESS",34,32,Color(123,213,225,255),2.2f);
        drawText("DESTINATION  " + service_.destinationName(),34,58,
                 Color(238,243,239,255),1.6f);
        const std::string positionLine = state.phase == Sim::ServicePhase::Dwelling
            ? "AT " + route_.stations()[state.currentStation].name
                + "  NEXT " + route_.stations()[state.nextStation].name
            : "NEXT  " + route_.stations()[state.nextStation].name;
        drawText(positionLine,34,80,Color(245,190,49,255),1.8f);
        const int etaMinutes = static_cast<int>(
            std::ceil(service_.estimatedSecondsToNextStation() / 60.0));
        drawText(speedText(state.speedMetresPerSecond) + "   ETA "
                     + std::to_string(etaMinutes) + " MIN",
                 34,104,Color(188,205,207,255),1.45f);
    }

    // A visible desktop launch begins with a free cursor. Give a first-time passenger the one
    // action needed to enter first-person control without leaving this permanently on screenshots
    // and automated smoke runs, all of which use a finite frame limit.
    if (!mouseCaptured_ && options_.frameLimit == 0 && !paused_ && !showMap_ && !showHelp_
        && !inspectDisplay_ && conductorInfoRemaining_ <= 0.0f)
    {
        drawPanel(width-302,18,284,72,Color(7,21,29,218));
        drawText("LEFT CLICK TO LOOK",static_cast<float>(width-284),34,
                 Color(245,190,49,255),1.75f);
        drawText("F1  CONTROLS",static_cast<float>(width-284),62,
                 Color(188,205,207,255),1.45f);
    }

    if (mouseCaptured_ && !paused_ && !showMap_ && !showHelp_ && !inspectDisplay_)
        drawPanel(width/2-1,height/2-1,3,3,Color(225,234,231,150));

    if (conductorInfoRemaining_ > 0.0f && !showMap_ && !showHelp_ && !paused_)
    {
        drawPanel(width-402,18,384,112,Color(8,23,32,228));
        drawText("CONDUCTOR",static_cast<float>(width-382),32,
                 Color(121,211,224,255),2.1f);
        drawText("NEXT  " + route_.stations()[state.nextStation].name,
                 static_cast<float>(width-382),60,Color(244,244,236,255),1.65f);
        drawText(scheduleText(service_.predictedScheduleDeviationSeconds()),
                 static_cast<float>(width-382),84,Color(164,211,181,255),1.45f);
        drawText("HAVE A PLEASANT JOURNEY",static_cast<float>(width-382),106,
                 Color(173,188,191,255),1.25f);
    }

    if (settings_.announcementCaptions && audio_ && !paused_ && !showMap_ && !showHelp_
        && !inspectDisplay_)
    {
        const std::string_view caption = audio_->currentAnnouncementCaption();
        if (!caption.empty())
        {
            const int panelWidth = std::min(width - 36, 1'000);
            const int panelX = (width - panelWidth) / 2;
            const int panelY = height - 190;
            drawPanel(panelX,panelY,panelWidth,104,Color(5,15,20,224));
            drawText("PASSENGER ANNOUNCEMENT",static_cast<float>(panelX+18),
                     static_cast<float>(panelY+12),Color(120,211,225,255),1.35f);
            constexpr float captionScale = 1.45f;
            const float captionWidth = static_cast<float>(panelWidth-36);
            const auto maximumColumns = static_cast<std::size_t>(std::max(
                1.0f,std::floor(captionWidth/(6.0f*captionScale))));
            drawText(wrapMonospaceWords(caption,maximumColumns),static_cast<float>(panelX+18),
                     static_cast<float>(panelY+38),Color(242,243,235,255),1.45f);
        }
    }

    const std::string prompt = interactionPrompt();
    if (!paused_ && !showMap_ && !showHelp_ && !inspectDisplay_ && !prompt.empty())
    {
        const float promptWidth = font_->measure(prompt,2.0f) + 28.0f;
        drawPanel(static_cast<int>((static_cast<float>(width)-promptWidth)*0.5f), height-72,
                  static_cast<int>(promptWidth),38,Color(8,18,23,210));
        drawText(prompt,(static_cast<float>(width)-font_->measure(prompt,2.0f))*0.5f,
                 static_cast<float>(height-62),Color(242,242,232,255),2.0f);
    }

    if (inspectDisplay_)
    {
        constexpr int panelWidth = 700;
        constexpr int panelHeight = 420;
        const int panelX = (width-panelWidth)/2;
        const int panelY = (height-panelHeight)/2;
        drawPanel(panelX,panelY,panelWidth,panelHeight,Color(5,20,28,248));
        drawText("PASSENGER INFORMATION",static_cast<float>(panelX+34),
                 static_cast<float>(panelY+24),Color(117,217,229,255),2.5f);
        drawText("DESTINATION",static_cast<float>(panelX+34),
                 static_cast<float>(panelY+64),Color(151,174,178,255),1.5f);
        drawText(service_.destinationName(),static_cast<float>(panelX+34),
                 static_cast<float>(panelY+87),Color(250,250,242,255),2.3f);
        const std::string inspectedPosition = state.phase == Sim::ServicePhase::Dwelling
            ? "AT " + route_.stations()[state.currentStation].name + "  NEXT "
                + route_.stations()[state.nextStation].name
            : "NEXT  " + route_.stations()[state.nextStation].name;
        drawText(inspectedPosition,static_cast<float>(panelX+34),static_cast<float>(panelY+130),
                 Color(245,188,41,255),2.0f);

        const int lineX = panelX+42;
        constexpr int lineWidth = panelWidth-84;
        const int lineY = panelY+194;
        drawPanel(lineX,lineY,lineWidth,4,Color(73,165,185,255));
        const float routeProgress = static_cast<float>(
            state.routeDistanceMetres/route_.lengthMetres());
        const int trainX = lineX+static_cast<int>(routeProgress*static_cast<float>(lineWidth));
        for (std::size_t index = 0; index < route_.stations().size(); ++index)
        {
            const Sim::Station& stationInfo = route_.stations()[index];
            const float progress = static_cast<float>(
                stationInfo.distanceMetres/route_.lengthMetres());
            const int stationX = lineX+static_cast<int>(progress*static_cast<float>(lineWidth));
            const bool passed = state.direction > 0
                ? stationInfo.distanceMetres < state.routeDistanceMetres
                : stationInfo.distanceMetres > state.routeDistanceMetres;
            const bool next = index == state.nextStation;
            const Color markerColor = next ? Color(245,188,41,255)
                                    : passed ? Color(83,108,113,255)
                                             : Color(226,235,232,255);
            drawPanel(stationX-4,lineY-6,10,16,markerColor);
            drawText(std::to_string(index+1),static_cast<float>(stationX-3),
                     static_cast<float>(lineY+(index%2U == 0U ? 18 : -24)),
                     Color(186,202,203,255),1.0f);
        }
        drawPanel(trainX-8,lineY-20,18,11,Color(245,188,41,255));
        drawText(state.direction > 0 ? ">" : "<",static_cast<float>(trainX-3),
                 static_cast<float>(lineY-20),Color(15,26,31,255),1.3f);

        const int etaMinutes = static_cast<int>(
            std::ceil(service_.estimatedSecondsToNextStation()/60.0));
        drawText("SPEED",static_cast<float>(panelX+500),static_cast<float>(panelY+64),
                 Color(151,174,178,255),1.4f);
        drawText(speedText(state.speedMetresPerSecond),static_cast<float>(panelX+500),
                 static_cast<float>(panelY+87),Color(226,235,232,255),1.9f);
        drawText("ETA  " + std::to_string(etaMinutes) + " MIN",static_cast<float>(panelX+500),
                 static_cast<float>(panelY+119),Color(164,211,181,255),1.5f);

        drawText("FOLLOWING STATIONS",static_cast<float>(panelX+34),
                 static_cast<float>(panelY+247),Color(151,174,178,255),1.5f);
        int station = static_cast<int>(state.nextStation) + state.direction;
        for (int row = 0; row < 3 && station >= 0 && station < static_cast<int>(route_.stations().size()); ++row)
        {
            drawText(route_.stations()[static_cast<std::size_t>(station)].name,
                     static_cast<float>(panelX+34),static_cast<float>(panelY+273+row*24),
                     Color(226,233,230,255),1.6f);
            station += state.direction;
        }
        drawText(scheduleText(service_.predictedScheduleDeviationSeconds()),
                 static_cast<float>(panelX+34),static_cast<float>(panelY+376),
                 Color(151,205,174,255),1.4f);
        drawText("E  CLOSE",static_cast<float>(panelX+570),static_cast<float>(panelY+376),
                 Color(151,174,178,255),1.4f);
    }

    if (showDebug_)
    {
        std::ostringstream debug;
        debug << "F3 DIAGNOSTICS\nFPS " << static_cast<int>(frameRate_)
              << "  DRAW " << renderStats_.drawCalls << "  TRI " << renderStats_.triangles
              << "\nROUTE KM " << std::fixed << std::setprecision(1) << state.routeDistanceMetres/1000.0
              << "  CHUNK " << streaming_.currentChunk()
              << "\nLOADED " << streaming_.chunks().size() << "  NPC " << passengers_.passengers().size()
              << "  VIS " << renderStats_.visiblePassengers
              << "/" << renderStats_.avatarVisuals
              << " READY " << renderStats_.readyAvatarVisuals
              << "  EXCH " << passengers_.stationExchangeActorCount()
              << "/" << passengers_.lastStationExchangeParticipantCount()
              << "  OFF " << passengers_.offboardPassengerCount()
              << "\nSOCIAL " << passengers_.conversationPairCount()
              << " PAIRS  SEAT WALK " << passengers_.walkingToSeatCount()
              << "  LANE " << passengers_.passengerLaneChangeCount()
              << "\nSTATE " << service_.phaseName() << "  LEGS " << state.completedLegs
              << "  CHECK " << passengers_.conductorInspectionCount()
              << "\nACCEL " << std::fixed << std::setprecision(2)
              << state.longitudinalAccelerationMetresPerSecondSquared
              << " M/S2  SUSP " << std::setprecision(1)
              << cameraLongitudinalLean_*180.0f/kPi*1'000.0f << " MDEG"
              << "\nSIGNAL R" << renderStats_.redSignals
              << " Y" << renderStats_.yellowSignals
              << " G" << renderStats_.greenSignals
              << "  BOARD " << renderStats_.platformInformationBoards
              << "  PAVE " << renderStats_.platformSurfaceJoints
              << "/" << renderStats_.platformCanopyLights
              << "\nLAKE " << renderStats_.lakesideWaterPatches
              << "/" << renderStats_.lakesideBankTrees
              << " RIVER " << renderStats_.riverWaterSections
              << " KM " << renderStats_.tracksideKilometrePosts
              << " CAT " << renderStats_.catenaryDroppers
              << "\nEXT " << renderStats_.otherTrainCars
              << " CABS " << renderStats_.otherTrainCabEnds
              << " ROAD " << renderStats_.roadVehicles
              << " TN " << renderStats_.tunnelLuminaires
              << "/" << renderStats_.tunnelSafetyNiches
              << " BR " << renderStats_.bridgeSections
              << "/" << renderStats_.riverBridgeSections
              << "/" << renderStats_.viaductSections
              << " A" << renderStats_.bridgeAbutments
              << " CLK " << renderStats_.stationClockHands
              << " GW " << renderStats_.openGangwayDoors
              << " PA " << renderStats_.paSpeakerGrilles
              << "/HV " << renderStats_.hvacVents
              << " SUN " << renderStats_.sunlightPatches
              << " CARP " << renderStats_.aisleRunnerInlays
              << "/" << renderStats_.nonCarpetedFloorZones
              << "\nXING " << renderStats_.levelCrossingBarriers
              << "/" << renderStats_.levelCrossingWarningLights
              << " A" << (audio_ ? audio_->crossingWarningEventCount() : 0U)
              << " TN" << (audio_ && audio_->tunnelAcousticsActive() ? 1 : 0)
              << " BR" << (audio_ && audio_->bridgeAcousticsActive() ? 1 : 0)
              << " T" << (audio_ ? audio_->terminalAnnouncementEventCount() : 0U)
              << "  VOICE " << (audio_ ? audio_->passengerMurmurEventCount() : 0U)
              << "\n" << scheduleText(service_.predictedScheduleDeviationSeconds())
              << "\nLIMIT " << static_cast<int>(std::round(
                     service_.currentSpeedLimitMetresPerSecond() * 3.6))
              << " KM/H  SCALE " << options_.timeScale << "  F5 FREEZE"
              << "\nQUALITY " << graphicsQualityText(settings_.graphicsQuality)
              << "  VEG "
              << static_cast<int>(std::round(effectiveVegetationDistance(settings_)));
        if (effectiveVegetationDistance(settings_) + 0.5f < settings_.vegetationDistanceMetres)
            debug << "/" << static_cast<int>(std::round(settings_.vegetationDistanceMetres));
        debug << " M  MSAA " << appliedMultisampleCount_ << "X";
        drawPanel(width-350,18,332,270,Color(7,12,16,220));
        drawText(debug.str(),static_cast<float>(width-334),30,Color(179,230,181,255),1.5f);
    }

    if (showHelp_)
    {
        drawPanel(30,height/2-165,435,330,Color(8,19,25,238));
        drawText("CONTROLS",52.0f,static_cast<float>(height/2-140),Color(126,214,226,255),2.8f);
        drawText("WASD       WALK\nMOUSE      LOOK\nE          INTERACT / SIT / STAND\nM          ROUTE MAP\nH          TOGGLE PASSENGER HUD\nF1         CONTROLS\nF3         DIAGNOSTICS\nF5         FREEZE TRAIN\nF6 / F7    PREVIOUS / NEXT STATION\nESC        PAUSE / COMFORT MENU\nF12        QUIT",
                 52.0f,static_cast<float>(height/2-94),Color(235,239,236,255),1.75f);
    }
    if (showMap_) drawMap();
    if (paused_)
    {
        drawPanel(width/2-245,height/2-305,490,610,Color(5,13,18,240));
        drawText("SERVICE PAUSED",static_cast<float>(width/2-194),
                 static_cast<float>(height/2-275),Color(244,244,236,255),2.8f);
        const auto percent = [](float value) {
            return std::to_string(static_cast<int>(std::round(value * 100.0f))) + " PCT";
        };
        const double clock = Sim::serviceClockHours(settings_.serviceStartMinutes/60.0,
                                                    service_.state().serviceElapsedSeconds);
        const int clockMinutes = static_cast<int>(clock*60.0) % (24*60);
        const auto twoDigits = [](int value) {
            return std::string(value < 10 ? "0" : "") + std::to_string(value);
        };
        std::string weatherName(Sim::weatherModeName(
            static_cast<Sim::WeatherMode>(settings_.weatherMode)));
        std::transform(weatherName.begin(),weatherName.end(),weatherName.begin(),
                       [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
        const std::array<std::string,12> items{
            "FIELD OF VIEW       "
                + std::to_string(static_cast<int>(std::round(settings_.fieldOfViewDegrees))) + " DEG",
            "MOUSE SENSITIVITY   "
                + std::to_string(static_cast<int>(std::round(settings_.mouseSensitivity * 10'000.0f))),
            "CAMERA MOTION       " + percent(settings_.cameraVibration),
            std::string("GRAPHICS QUALITY    ") + graphicsQualityText(settings_.graphicsQuality),
            "MASTER VOLUME       " + percent(settings_.masterVolume),
            "TRAIN VOLUME        " + percent(settings_.trainVolume),
            "ANNOUNCEMENTS       " + percent(settings_.announcementVolume),
            "PASSENGER VOICES    " + percent(settings_.passengerVolume),
            "VEGETATION RANGE    "
                + std::to_string(static_cast<int>(std::round(effectiveVegetationDistance(settings_))))
                + (effectiveVegetationDistance(settings_) + 0.5f < settings_.vegetationDistanceMetres
                    ? "/" + std::to_string(static_cast<int>(std::round(
                        settings_.vegetationDistanceMetres))) : std::string{}) + " M",
            std::string("ANNOUNCEMENT TEXT   ") + (settings_.announcementCaptions ? "ON" : "OFF"),
            "TIME OF DAY         " + twoDigits(clockMinutes/60) + ":" + twoDigits(clockMinutes%60),
            "WEATHER             " + weatherName};
        for (std::size_t item = 0; item < items.size(); ++item)
        {
            const bool selected = pauseSelection_ == static_cast<int>(item);
            drawText(std::string(selected ? ">  " : "   ") + items[item],
                     static_cast<float>(width/2-200),
                     static_cast<float>(height/2-217+static_cast<int>(item)*34),
                     selected ? Color(245,190,49,255) : Color(224,231,229,255),1.8f);
        }
        drawText("UP / DOWN  SELECT    LEFT / RIGHT  ADJUST",
                 static_cast<float>(width/2-200),static_cast<float>(height/2+202),
                 Color(145,190,198,255),1.45f);
        drawText("ESC  RESUME",static_cast<float>(width/2-70),static_cast<float>(height/2+250),
                 Color(233,238,233,255),1.6f);
    }
    spriteBatch_->End();
}

void TrainSimulatorGame::captureScreenshot(const std::string& path)
{
    // cna-engine reads the back buffer as bytes (CNA's Color is a polymorphic packed-vector type
    // four times their size), which keeps screenshot peak memory bounded during automated
    // visual QA, and writes the PNG.
    const auto capture = CnaEngine::Graphics::CaptureBackBuffer(getGraphicsDeviceProperty(), {.path = path});
    if (!capture.success) throw std::runtime_error("Screenshot failed: " + capture.error);
    screenshotSaved_ = true;
}

void TrainSimulatorGame::Draw(const GameTime& gameTime)
{
    auto& device = getGraphicsDeviceProperty();
    // Capture the previous completed frame after CNA's normal EndDraw/Present boundary. Reading
    // the current back buffer before that boundary is intermittent on EasyGL's offscreen path.
    // Freeze simulation at the requested frame, but keep drawing until the complete quality-sized
    // avatar pool has crossed its first GPU-upload boundary. This prevents deterministic gallery
    // cameras from photographing an otherwise valid station before its people are ready.
    if (screenshotPending_ && scene_->avatarLoadingComplete())
    {
        captureScreenshot(options_.screenshotPath);
        screenshotPending_ = false;
        if (scene_->avatarLoadingComplete())
        {
            Exit();
            Game::Draw(gameTime);
            return;
        }
    }
    if (startMenu_)
    {
        drawStartMenu();
        Game::Draw(gameTime);
        return;
    }
    const auto wallNow = std::chrono::steady_clock::now();
    const bool steady = framesDrawn_ >= 180 && scene_->avatarLoadingComplete();
    if (previousDrawTime_.time_since_epoch().count() != 0)
    {
        const double interval = std::chrono::duration<double,std::milli>(wallNow-previousDrawTime_).count();
        frameTiming_.record(interval);
        if (steady && previousDrawWasSteady_) steadyFrameTiming_.record(interval);
    }
    previousDrawWasSteady_ = steady;
    previousDrawTime_ = wallNow;
    if (frameRateWindowStart_.time_since_epoch().count() == 0)
        frameRateWindowStart_ = wallNow;
    ++frameRateWindowFrames_;
    const float wallWindowSeconds = std::chrono::duration<float>(
        wallNow-frameRateWindowStart_).count();
    if (wallWindowSeconds >= 0.5f)
    {
        frameRate_ = static_cast<float>(frameRateWindowFrames_) / wallWindowSeconds;
        frameRateWindowStart_ = wallNow;
        frameRateWindowFrames_ = 0;
    }
    device.Clear(Color(105,165,215,255));
    const auto& viewport = device.getViewportProperty();
    const float aspect = static_cast<float>(viewport.getWidthProperty())
                       / static_cast<float>(std::max(1, viewport.getHeightProperty()));

    const float easedSeat = seatedBlend_ * seatedBlend_ * (3.0f - 2.0f * seatedBlend_);
    const float transitionRemaining = cameraTransitionRemaining();
    const float cameraX = playerX_ + cameraTransitionOffsetX_ * transitionRemaining;
    const float cameraZ = playerZ_ + cameraTransitionOffsetZ_ * transitionRemaining;
    const float cameraYaw = MathHelper::WrapAngle(
        playerYaw_ + cameraTransitionYawOffset_ * transitionRemaining);
    const float eyeY = static_cast<float>(Sim::TrainInterior::FloorElevationAboveRail)
                     + 1.66f + (1.20f - 1.66f) * easedSeat;
    const float speedRatio = static_cast<float>(service_.state().speedMetresPerSecond / 44.44);
    const float comfort = paused_ ? 0.0f : settings_.cameraVibration;
    if (paused_ || comfort <= 0.0f)
    {
        cameraLongitudinalLean_ = 0.0f;
        cameraLongitudinalOffset_ = 0.0f;
    }
    else
    {
        // A lightly damped car body lets traction and service braking register without turning
        // the passenger camera into a physics rig. Both targets consume the actual simulation
        // acceleration, so coasting is naturally much subtler than a full brake application.
        const float signedAcceleration = static_cast<float>(
            service_.state().longitudinalAccelerationMetresPerSecondSquared)
            * static_cast<float>(service_.state().direction);
        const float targetLean = std::clamp(-signedAcceleration/9.81f*0.080f,
                                             -0.0070f,0.0070f) * comfort;
        const float targetOffset = std::clamp(-signedAcceleration*0.014f,
                                               -0.012f,0.012f) * comfort;
        const float renderSeconds = stepSeconds(gameTime);
        const float response = 1.0f-std::exp(-renderSeconds*3.2f);
        cameraLongitudinalLean_ += (targetLean-cameraLongitudinalLean_)*response;
        cameraLongitudinalOffset_ += (targetOffset-cameraLongitudinalOffset_)*response;
    }
    const float railPhase = static_cast<float>(elapsedSeconds_ * (14.0 + speedRatio * 8.0));
    const float lateralVibration = std::sin(railPhase) * 0.0035f * speedRatio * comfort;
    const float verticalVibration = std::sin(railPhase * 1.37f + 0.8f)
                                  * 0.0025f * speedRatio * comfort;
    const float suspensionSway = std::sin(static_cast<float>(elapsedSeconds_ * 0.83))
                               * 0.006f * speedRatio * comfort;
    const Sim::RoutePose trainPose = route_.sample(service_.state().routeDistanceMetres);
    const double speed = service_.state().speedMetresPerSecond;
    const float lateralAcceleration = static_cast<float>(speed * speed * trainPose.curvature);
    const float curveLean = std::clamp(-lateralAcceleration / 9.81f * 0.34f,
                                       -0.014f, 0.014f) * comfort;
    const Vector3 eye{cameraX + lateralVibration + suspensionSway
                      + lateralAcceleration * 0.014f * comfort,
                      eyeY + verticalVibration, cameraZ + cameraLongitudinalOffset_};
    const float cosPitch = std::cos(playerPitch_);
    const Vector3 unpitchedForward{cosPitch*std::sin(cameraYaw), std::sin(playerPitch_),
                                   -cosPitch*std::cos(cameraYaw)};
    const float suspensionSine = std::sin(cameraLongitudinalLean_);
    const float suspensionCosine = std::cos(cameraLongitudinalLean_);
    const Vector3 forward{
        unpitchedForward.X,
        unpitchedForward.Y*suspensionCosine-unpitchedForward.Z*suspensionSine,
        unpitchedForward.Y*suspensionSine+unpitchedForward.Z*suspensionCosine};
    const Vector3 suspensionUp{0.0f,suspensionCosine,suspensionSine};
    const float leanSine = std::sin(curveLean);
    const float leanCosine = std::cos(curveLean);
    const float oneMinusCosine = 1.0f - leanCosine;
    const Vector3 leanAxisCrossUp{
        forward.Y*suspensionUp.Z-forward.Z*suspensionUp.Y,
        forward.Z*suspensionUp.X-forward.X*suspensionUp.Z,
        forward.X*suspensionUp.Y-forward.Y*suspensionUp.X};
    const float leanAxisDotUp = forward.X*suspensionUp.X
                              + forward.Y*suspensionUp.Y
                              + forward.Z*suspensionUp.Z;
    const Vector3 cameraUp{
        suspensionUp.X*leanCosine + leanAxisCrossUp.X*leanSine
            + forward.X*leanAxisDotUp*oneMinusCosine,
        suspensionUp.Y*leanCosine + leanAxisCrossUp.Y*leanSine
            + forward.Y*leanAxisDotUp*oneMinusCosine,
        suspensionUp.Z*leanCosine + leanAxisCrossUp.Z*leanSine
            + forward.Z*leanAxisDotUp*oneMinusCosine};
    Matrix view = Matrix::CreateLookAt(eye, eye + forward, cameraUp);
    if(options_.auditCamera)
    {
        const auto& p=*options_.auditCamera;
        const Vector3 auditEye{p[0],p[1],p[2]};
        const float yaw=p[3]*kPi/180.0f,pitch=p[4]*kPi/180.0f;
        const Vector3 look{std::cos(pitch)*std::sin(yaw),std::sin(pitch),
                            -std::cos(pitch)*std::cos(yaw)};
        view=Matrix::CreateLookAt(auditEye,auditEye+look,Vector3::Up);
    }
    const float fieldOfView = settings_.fieldOfViewDegrees * kPi / 180.0f;
    const Matrix projection = Matrix::CreatePerspectiveFieldOfView(fieldOfView, aspect, 0.06f, 1'100.0f);
    renderStats_ = scene_->draw(view,projection,route_,service_.state(),streaming_,passengers_,
                                elapsedSeconds_,
                                service_.estimatedSecondsToNextStation(),
                                service_.predictedScheduleDeviationSeconds(),
                                toiletDoorEffectivelyOpen(),
                                passengers_.toiletOccupied() || playerInsideToilet(),
                                options_.auditCamera ? (*options_.auditCamera)[0] : cameraX,
                                options_.auditCamera ? (*options_.auditCamera)[2] : cameraZ,
                                options_.auditCamera ? (*options_.auditCamera)[3]*kPi/180.0f : cameraYaw);
    drawUi();

    ++framesDrawn_;
    const bool durationReached=options_.runSeconds>0.0 && elapsedSeconds_>=options_.runSeconds;
    const bool captureFrameReached = durationReached || (options_.frameLimit == 0
        ? options_.runSeconds==0.0 && framesDrawn_>=1 : framesDrawn_>=options_.frameLimit);
    if (captureFrameReached
        && (options_.frameLimit>0 || options_.runSeconds>0.0 || !options_.screenshotPath.empty()))
    {
        finishingAvatarWarmup_ = true;
        if (!options_.screenshotPath.empty() && !screenshotSaved_)
            screenshotPending_ = true;
        else if (scene_->avatarLoadingComplete())
            Exit();
    }
    Game::Draw(gameTime);
}

} // namespace TrainSim::App
