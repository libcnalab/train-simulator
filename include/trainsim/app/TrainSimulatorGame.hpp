#pragma once

#include "trainsim/audio/TrainAudio.hpp"
#include "trainsim/app/FrameTiming.hpp"
#include "trainsim/config/Settings.hpp"
#include "trainsim/render/BitmapFont.hpp"
#include "trainsim/render/SceneRenderer.hpp"
#include "trainsim/sim/PassengerSystem.hpp"
#include "trainsim/sim/Route.hpp"
#include "trainsim/sim/RouteSimulation.hpp"
#include "trainsim/world/StreamingWorld.hpp"

#include "Microsoft/Xna/Framework/Game.hpp"
#include "Microsoft/Xna/Framework/GraphicsDeviceManager.hpp"
#include "Microsoft/Xna/Framework/Graphics/SpriteBatch.hpp"
#include "Microsoft/Xna/Framework/Graphics/Texture2D.hpp"
#include "Microsoft/Xna/Framework/Input/KeyboardState.hpp"
#include "Microsoft/Xna/Framework/Input/Keys.hpp"
#include "Microsoft/Xna/Framework/Input/MouseState.hpp"

#include <chrono>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>

namespace TrainSim::App {

struct LaunchOptions
{
    std::filesystem::path assetDirectory;
    std::filesystem::path userConfigFile;
    int frameLimit = 0;
    double runSeconds = 0.0;
    std::optional<std::array<float,5>> auditCamera;
    double timeScale = 2.4;
    bool startWithHelp = false;
    bool startWithMap = false;
    bool startWithDisplayInspection = false;
    bool startWithDebug = false;
    bool startPaused = false;
    bool startWithPassengerHud = true;
    bool developmentDwell = false;
    bool audioEnabled = true;
    bool avatarRenderingEnabled = true;
    bool verticalSyncEnabled = true;
    bool fixedTimeStep = true;
    // One 1/60 s step per drawn frame, however long the frame took, so the same arguments give
    // the same pictures (fixed-step catch-up otherwise runs a timing-dependent number of steps).
    bool lockstep = false;
    int facadeLightingMode = 0;
    bool reportState = false;
    bool interactionSmoke = false;
    bool startInToilet = false;
    bool startAtSink = false;
    bool startAtWindow = false;
    bool startAtLeftWindow = false;
    bool startAtPlatformWindow = false;
    bool startAtDoor = false;
    bool startAtGangway = false;
    bool startAtVestibule = false;
    bool startAtSunlight = false;
    bool startAtPassenger = false;
    bool startAtSignal = false;
    std::string screenshotPath;
    std::optional<std::size_t> startStation;
    std::optional<double> startRouteKilometres;
    std::optional<int> startDirection;
    std::optional<int> startCar;
    std::optional<int> graphicsQuality;
    // Runtime overrides of the time-of-day and weather preferences; never persisted.
    std::optional<int> serviceStartMinutes;
    std::optional<int> weatherMode;
};

class TrainSimulatorGame final : public Microsoft::Xna::Framework::Game
{
public:
    explicit TrainSimulatorGame(LaunchOptions options);
    ~TrainSimulatorGame() override;

protected:
    void Initialize() override;
    void LoadContent() override;
    void Update(Microsoft::Xna::Framework::GameTime& gameTime) override;
    void Draw(const Microsoft::Xna::Framework::GameTime& gameTime) override;

private:
    // Seconds one Update or Draw advances: the measured step, or the fixed one in lockstep.
    [[nodiscard]] float stepSeconds(const Microsoft::Xna::Framework::GameTime& gameTime);
    [[nodiscard]] bool pressed(const Microsoft::Xna::Framework::Input::KeyboardState& now,
                               Microsoft::Xna::Framework::Input::Keys key) const;
    [[nodiscard]] bool playerInsideToilet() const noexcept;
    [[nodiscard]] bool toiletDoorEffectivelyOpen() const noexcept;
    [[nodiscard]] bool canOccupy(float x, float z) const;
    [[nodiscard]] float cameraTransitionRemaining() const noexcept;
    void transitionPlayerTo(float x, float z, float yaw) noexcept;
    void updatePlayer(float dt, const Microsoft::Xna::Framework::Input::KeyboardState& keyboard,
                      const Microsoft::Xna::Framework::Input::MouseState& mouse);
    void releaseMouseCapture(bool unexpected);
    void applyAudioSettings();
    void persistSettings() const;
    void applyEnvironmentSettings();
    [[nodiscard]] std::optional<std::size_t> interactableSeat() const;
    void interact();
    void runInteractionSmoke();
    void drawUi();
    void drawMap();
    void drawStartMenu();
    void leaveStartMenu();
    void drawPanel(int x, int y, int width, int height,
                   const Microsoft::Xna::Framework::Color& color);
    void drawText(const std::string& text, float x, float y,
                  const Microsoft::Xna::Framework::Color& color,
                  float scale = 2.0f);
    void captureScreenshot(const std::string& path);
    [[nodiscard]] std::string interactionPrompt() const;

    LaunchOptions options_;
    Microsoft::Xna::Framework::GraphicsDeviceManager graphics_{this};
    Sim::Route route_ = Sim::Route::NorthernValleyExpress();
    Sim::RouteSimulation service_{route_};
    Sim::PassengerSystem passengers_{route_.seed() ^ 0x50415353454e4745ULL, 6, 34};
    World::StreamingWorld streaming_{route_, 250.0, 3};
    Config::Settings settings_;
    int loadedGraphicsQuality_ = 2;
    bool graphicsQualityAdjustedInMenu_ = false;
    int loadedServiceStartMinutes_ = 9*60 + 17;
    int loadedWeatherMode_ = 0;
    bool environmentAdjustedInMenu_ = false;

    std::unique_ptr<Render::SceneRenderer> scene_;
    std::unique_ptr<Audio::TrainAudio> audio_;
    std::unique_ptr<Microsoft::Xna::Framework::Graphics::SpriteBatch> spriteBatch_;
    std::unique_ptr<Microsoft::Xna::Framework::Graphics::Texture2D> whitePixel_;
    std::unique_ptr<Render::BitmapFont> font_;

    Microsoft::Xna::Framework::Input::KeyboardState previousKeyboard_;
    Microsoft::Xna::Framework::Input::MouseState previousMouse_;
    bool mouseCaptured_ = false;
    std::uint32_t relativeMouseModeAcquireCount_ = 0;
    std::uint32_t relativeMouseModeLossCount_ = 0;
    std::uint64_t mouseMotionSamples_ = 0;
    std::chrono::steady_clock::time_point lastInputTrace_{};
    bool paused_ = false;
    int pauseSelection_ = 0;
    bool startMenu_ = false;
    bool showMap_ = false;
    bool showHelp_ = false;
    bool showDebug_ = false;
    bool showPassengerHud_ = true;
    bool inspectDisplay_ = false;
    bool toiletDoorOpen_ = false;
    bool seated_ = false;
    std::uint32_t playerSitTransitionCount_ = 0;
    std::uint32_t playerStandTransitionCount_ = 0;
    std::uint32_t playerToiletDoorInteractionCount_ = 0;
    bool playerWalking_ = false;
    float seatedBlend_ = 0.0f;
    float cameraTransitionProgress_ = 1.0f;
    float cameraTransitionOffsetX_ = 0.0f;
    float cameraTransitionOffsetZ_ = 0.0f;
    float cameraTransitionYawOffset_ = 0.0f;
    std::optional<std::size_t> playerSeat_;
    float playerX_ = 0.0f;
    float playerZ_ = -48.0f;
    float playerYaw_ = 0.0f;
    float playerPitch_ = 0.0f;
    float cameraLongitudinalLean_ = 0.0f;
    float cameraLongitudinalOffset_ = 0.0f;
    float conductorInfoRemaining_ = 0.0f;
    double elapsedSeconds_ = 0.0;
    double passengerDoorsOpenRealSeconds_ = 0.0;
    bool passengerDoorPassageReleased_ = false;
    double initialRouteDistanceMetres_ = 0.0;
    float frameRate_ = 60.0f;
    FrameTiming frameTiming_;
    FrameTiming steadyFrameTiming_;
    bool previousDrawWasSteady_ = false;
    std::chrono::steady_clock::time_point previousDrawTime_{};
    std::chrono::steady_clock::time_point frameRateWindowStart_{};
    int frameRateWindowFrames_ = 0;
    int framesDrawn_ = 0;
    int appliedMultisampleCount_ = 0;
    bool screenshotSaved_ = false;
    bool screenshotPending_ = false;
    bool finishingAvatarWarmup_ = false;
    int interactionSmokeStage_ = 0;
    int interactionSmokeRenderCheckpoint_ = -1;
    Render::RenderStats renderStats_;
};

} // namespace TrainSim::App
