#include "trainsim/app/TrainSimulatorGame.hpp"
#include "trainsim/sim/Environment.hpp"

#include "CnaEngine/Core/SettingsFile.hpp"
#include "CnaEngine/Graphics/AutomatedRun.hpp"

#include <array>
#include <cmath>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

namespace fs = std::filesystem;

bool validAssetDirectory(const fs::path& directory)
{
    return fs::is_regular_file(directory / "config" / "train-simulator.cfg")
        && fs::is_directory(directory / "audio");
}

fs::path executablePath(const char* argumentZero)
{
    fs::path supplied(argumentZero == nullptr ? "train-simulator" : argumentZero);
    if (supplied.has_parent_path()) return fs::weakly_canonical(fs::absolute(supplied));

    if (const char* environmentPath = std::getenv("PATH"); environmentPath != nullptr)
    {
        const std::string searchPath(environmentPath);
        std::size_t begin = 0;
        while (begin <= searchPath.size())
        {
            const std::size_t end = searchPath.find(':', begin);
            const std::string entry = searchPath.substr(
                begin, end == std::string::npos ? std::string::npos : end - begin);
            const fs::path candidate = (entry.empty() ? fs::current_path() : fs::path(entry))
                                     / supplied;
            if (fs::is_regular_file(candidate)) return fs::weakly_canonical(candidate);
            if (end == std::string::npos) break;
            begin = end + 1;
        }
    }
    return fs::weakly_canonical(fs::absolute(supplied));
}

fs::path discoverAssetDirectory(const char* argumentZero)
{
    const fs::path executable = executablePath(argumentZero);
    const std::array candidates = {
        executable.parent_path() / "assets",
        executable.parent_path().parent_path() / "share" / "train-simulator" / "assets",
        fs::current_path() / "assets",
    };
    for (const fs::path& candidate : candidates)
        if (validAssetDirectory(candidate)) return fs::weakly_canonical(candidate);
    throw std::runtime_error(
        "Could not find runtime assets beside the executable, in the install prefix, or in the working directory");
}

fs::path discoverUserConfigFile()
{
    fs::path local = fs::current_path() / "train-simulator.cfg";
    if (fs::is_regular_file(local)) return local;
    fs::path user = CnaEngine::UserConfigPath("train-simulator","train-simulator.cfg");
    return user.empty() ? local : user;
}

} // namespace

int main(int argc, char** argv)
{
    TrainSim::App::LaunchOptions options;
    try
    {
        const std::vector<std::string_view> arguments(argv, argv + argc);
        for (int index = 1; index < argc; ++index)
        {
            // --frames, --run-seconds, --screenshot and --lockstep are cna-engine's automated-run
            // options; it rejects a missing, negative or non-finite value.
            std::size_t position = static_cast<std::size_t>(index);
            if (CnaEngine::Graphics::ParseAutomatedRunOption(arguments, position, options.run))
            {
                index = static_cast<int>(position);
                continue;
            }
            const std::string argument = argv[index];
            if (argument == "--time-scale" && index + 1 < argc)
                options.timeScale = std::stod(argv[++index]);
            else if (argument == "--audit-camera" && index + 5 < argc)
            {
                std::array<float,5> pose{};
                for(auto& value:pose) value=std::stof(argv[++index]);
                options.auditCamera=pose;
            }
            else if (argument == "--help-overlay")
                options.startWithHelp = true;
            else if (argument == "--map-overlay")
                options.startWithMap = true;
            else if (argument == "--inspect-display")
                options.startWithDisplayInspection = true;
            else if (argument == "--debug-overlay")
                options.startWithDebug = true;
            else if (argument == "--pause-overlay")
                options.startPaused = true;
            else if (argument == "--hide-hud")
                options.startWithPassengerHud = false;
            else if (argument == "--development-dwell")
                options.developmentDwell = true;
            else if (argument == "--no-audio")
                options.audioEnabled = false;
            else if (argument == "--no-avatars")
                options.avatarRenderingEnabled = false;
            else if (argument == "--no-vsync")
                options.verticalSyncEnabled = false;
            else if (argument == "--variable-step")
                options.fixedTimeStep = false;
            else if (argument == "--facade-control")
                options.facadeLightingMode = 1;
            else if (argument == "--linear-facades")
                options.facadeLightingMode = 2;
            else if (argument == "--report-state")
                options.reportState = true;
            else if (argument == "--interaction-smoke")
                options.interactionSmoke = true;
            else if (argument == "--toilet-view")
                options.startInToilet = true;
            else if (argument == "--sink-view")
                options.startAtSink = true;
            else if (argument == "--window-view")
                options.startAtWindow = true;
            else if (argument == "--left-window-view")
                options.startAtLeftWindow = true;
            else if (argument == "--platform-view")
                options.startAtPlatformWindow = true;
            else if (argument == "--door-view")
                options.startAtDoor = true;
            else if (argument == "--gangway-view")
                options.startAtGangway = true;
            else if (argument == "--vestibule-view")
                options.startAtVestibule = true;
            else if (argument == "--sunlight-view")
                options.startAtSunlight = true;
            else if (argument == "--passenger-view")
                options.startAtPassenger = true;
            else if (argument == "--signal-view")
                options.startAtSignal = true;
            else if (argument == "--asset-dir" && index + 1 < argc)
                options.assetDirectory = argv[++index];
            else if (argument == "--config" && index + 1 < argc)
                options.userConfigFile = argv[++index];
            else if (argument == "--station" && index + 1 < argc)
            {
                const int station = std::stoi(argv[++index]);
                if (station < 1 || station > 12)
                {
                    std::cerr << "--station must be between 1 and 12\n";
                    return 2;
                }
                options.startStation = static_cast<std::size_t>(station - 1);
            }
            else if (argument == "--route-km" && index + 1 < argc)
                options.startRouteKilometres = std::stod(argv[++index]);
            else if (argument == "--direction" && index + 1 < argc)
            {
                const int direction = std::stoi(argv[++index]);
                if (direction != -1 && direction != 1)
                {
                    std::cerr << "--direction must be -1 or 1\n";
                    return 2;
                }
                options.startDirection = direction;
            }
            else if (argument == "--car" && index + 1 < argc)
            {
                const int car = std::stoi(argv[++index]);
                if (car < 1 || car > 6)
                {
                    std::cerr << "--car must be between 1 and 6\n";
                    return 2;
                }
                options.startCar = car - 1;
            }
            else if (argument == "--quality" && index + 1 < argc)
            {
                const int quality = std::stoi(argv[++index]);
                if (quality < 0 || quality > 2)
                {
                    std::cerr << "--quality must be 0, 1, or 2\n";
                    return 2;
                }
                options.graphicsQuality = quality;
            }
            else if (argument == "--time" && index + 1 < argc)
            {
                const auto minutes = TrainSim::Sim::parseClockTime(argv[++index]);
                if (!minutes)
                {
                    std::cerr << "--time must be HH:MM between 00:00 and 23:59\n";
                    return 2;
                }
                options.serviceStartMinutes = *minutes;
            }
            else if (argument == "--weather" && index + 1 < argc)
            {
                const auto mode = TrainSim::Sim::parseWeatherMode(argv[++index]);
                if (!mode)
                {
                    std::cerr << "--weather must be clear, cloudy, overcast, rain, fog or changing\n";
                    return 2;
                }
                options.weatherMode = static_cast<int>(*mode);
            }
            else if (argument == "--help")
            {
                std::cout << "train-simulator [--frames N] [--run-seconds S] [--time-scale X] [--development-dwell] "
                             "[--debug-overlay] [--help-overlay] [--map-overlay] [--no-audio] "
                             "[--inspect-display] "
                             "[--no-avatars] [--no-vsync] [--variable-step] [--lockstep] "
                             "[--facade-control|--linear-facades] "
                             "[--report-state] [--interaction-smoke] [--station 1-12] [--car 1-6] "
                             "[--route-km 0-286.4] [--direction -1|1] "
                             "[--quality 0-2] [--time HH:MM] "
                             "[--weather clear|cloudy|overcast|rain|fog|changing] "
                             "[--window-view] [--left-window-view] "
                             "[--platform-view] "
                             "[--door-view] [--gangway-view] [--vestibule-view] [--sunlight-view] "
                             "[--signal-view] [--passenger-view] "
                             "[--toilet-view] [--sink-view] "
                             "[--pause-overlay] [--hide-hud] [--audit-camera X Y Z YAW PITCH] "
                             "[--asset-dir PATH] [--config PATH] [--screenshot PATH]\n";
                return 0;
            }
            else
            {
                std::cerr << "Unknown or incomplete option: " << argument << '\n';
                return 2;
            }
        }
        if (!std::isfinite(options.timeScale) || options.timeScale <= 0.0
            || options.timeScale > 5'000.0)
            throw std::invalid_argument("--time-scale must be greater than zero and at most 5000");
        if (options.startRouteKilometres.has_value()
            && (!std::isfinite(*options.startRouteKilometres)
                || *options.startRouteKilometres < 0.0
                || *options.startRouteKilometres > 286.4))
            throw std::invalid_argument("--route-km must be between 0 and 286.4");
        if (options.startRouteKilometres.has_value() && options.startStation.has_value())
            throw std::invalid_argument("--route-km and --station are mutually exclusive");
        if (options.startAtSignal
            && (options.startRouteKilometres.has_value() || options.startStation.has_value()
                || options.startDirection.has_value() || options.startCar.has_value()))
            throw std::invalid_argument(
                "--signal-view selects its own route position and cannot use --route-km, "
                "--station, --direction, or --car");
        if (options.startDirection.has_value() && !options.startRouteKilometres.has_value()
            && !options.startStation.has_value())
            throw std::invalid_argument("--direction requires --route-km or --station");
        const int showcaseViews = static_cast<int>(options.startAtWindow)
            + static_cast<int>(options.startAtLeftWindow)
            + static_cast<int>(options.startAtPlatformWindow)
            + static_cast<int>(options.startAtDoor)
            + static_cast<int>(options.startAtGangway)
            + static_cast<int>(options.startAtVestibule)
            + static_cast<int>(options.startAtSunlight)
            + static_cast<int>(options.startAtPassenger)
            + static_cast<int>(options.startAtSignal)
            + static_cast<int>(options.startInToilet)
            + static_cast<int>(options.startAtSink);
        if (showcaseViews > 1)
            throw std::invalid_argument(
                "--window-view, --left-window-view, --platform-view, --door-view, "
                "--gangway-view, --vestibule-view, --sunlight-view, --passenger-view, --signal-view, and "
                "--toilet-view, --sink-view "
                "are mutually exclusive");
        if (options.auditCamera)
        {
            if (options.run.frames <= 0 || options.run.screenshotPath.empty())
                throw std::invalid_argument("--audit-camera requires --frames and --screenshot");
            for (float value : *options.auditCamera)
                if (!std::isfinite(value) || std::abs(value) > 1000)
                    throw std::invalid_argument("Audit camera values must be finite and within +/-1000");
            if (std::abs((*options.auditCamera)[4]) >= 89)
                throw std::invalid_argument("Audit camera pitch must be between -89 and 89 degrees");
        }
        if (options.assetDirectory.empty())
            options.assetDirectory = discoverAssetDirectory(argc > 0 ? argv[0] : nullptr);
        else
            options.assetDirectory = fs::weakly_canonical(fs::absolute(options.assetDirectory));
        if (!validAssetDirectory(options.assetDirectory))
            throw std::invalid_argument(
                "--asset-dir must contain config/train-simulator.cfg and the audio directory");
        if (options.userConfigFile.empty())
            options.userConfigFile = discoverUserConfigFile();
        else
            options.userConfigFile = fs::absolute(options.userConfigFile).lexically_normal();
    }
    catch (const std::exception& error)
    {
        std::cerr << "Invalid command line: " << error.what() << '\n';
        return 2;
    }

    try
    {
        TrainSim::App::TrainSimulatorGame game(options);
        game.Run();
    }
    catch (const std::exception& error)
    {
        std::cerr << "train-simulator: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
