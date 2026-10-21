cmake_minimum_required(VERSION 3.23)

project(TrainSimulator
    VERSION 0.1.0
    DESCRIPTION "A first-person passenger train simulator built with CNA"
    LANGUAGES C CXX)

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_program(TRAINSIM_CCACHE_EXECUTABLE ccache REQUIRED)
foreach(_language IN ITEMS C CXX)
    if(NOT CMAKE_${_language}_COMPILER_LAUNCHER)
        set(CMAKE_${_language}_COMPILER_LAUNCHER "${TRAINSIM_CCACHE_EXECUTABLE}"
            CACHE STRING "Mandatory Train Simulator compiler cache" FORCE)
    elseif(NOT "${CMAKE_${_language}_COMPILER_LAUNCHER}" MATCHES "ccache")
        message(FATAL_ERROR "Train Simulator requires ccache as the ${_language} compiler launcher")
    endif()
endforeach()

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
endif()

option(TRAINSIM_BUILD_GAME "Build the CNA OpenGL 3.3/WebGL 2 application" ON)
option(TRAINSIM_BUILD_TESTS "Build deterministic simulation tests" ON)
option(TRAINSIM_WARNINGS_AS_ERRORS "Treat warnings in project code as errors" OFF)

set(TRAINSIM_CNA_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../cna" CACHE PATH
    "Path to the CNA next-branch checkout")
set(TRAINSIM_SHARP_RUNTIME_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../sharp-runtime" CACHE PATH
    "Path to the sharp-runtime next-branch checkout")
set(TRAINSIM_CNA_ENGINE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../cna-engine" CACHE PATH
    "Path to the cna-engine checkout (strict modules only)")

foreach(_dependency IN ITEMS TRAINSIM_CNA_ROOT TRAINSIM_SHARP_RUNTIME_ROOT TRAINSIM_CNA_ENGINE_ROOT)
    if(NOT EXISTS "${${_dependency}}/CMakeLists.txt")
        message(FATAL_ERROR "${_dependency} does not name a valid sibling checkout: ${${_dependency}}")
    endif()
endforeach()

add_library(trainsim_warnings INTERFACE)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(trainsim_warnings INTERFACE
        -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wnon-virtual-dtor)
    if(TRAINSIM_WARNINGS_AS_ERRORS)
        target_compile_options(trainsim_warnings INTERFACE -Werror)
    endif()
elseif(MSVC)
    target_compile_options(trainsim_warnings INTERFACE /W4 /permissive-)
    if(TRAINSIM_WARNINGS_AS_ERRORS)
        target_compile_options(trainsim_warnings INTERFACE /WX)
    endif()
endif()

add_library(trainsim_core STATIC
    src/sim/Environment.cpp
    src/sim/PassengerSystem.cpp
    src/sim/Route.cpp
    src/sim/RouteSimulation.cpp
    src/sim/TrainInterior.cpp
    src/world/StreamingWorld.cpp)
add_library(TrainSimulator::Core ALIAS trainsim_core)
target_include_directories(trainsim_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(trainsim_core PRIVATE trainsim_warnings)

if(TRAINSIM_BUILD_GAME)
    if(ANDROID OR IOS OR CMAKE_SYSTEM_NAME STREQUAL "Darwin")
        message(FATAL_ERROR "Train Simulator currently supports desktop OpenGL 3.3 and Emscripten WebGL 2 only")
    endif()

    # Both products use CNA's shared EasyGL implementation, with the exact profile selected for
    # the host. Emscripten's WEBGL2 identity requests a WebGL 2 context and GLSL ES 3.00.
    if(EMSCRIPTEN)
        set(CNA_GRAPHICS_RENDERER WEBGL2 CACHE STRING "CNA renderer" FORCE)
    else()
        set(CNA_GRAPHICS_RENDERER OPENGL33 CACHE STRING "CNA renderer" FORCE)
    endif()
    set(CNA_GRAPHICS_RENDERERS "" CACHE STRING "No multi-renderer build" FORCE)
    set(CNA_CNAEXT ON CACHE BOOL "Relative mouse mode and diagnostics" FORCE)
    set(CNA_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(CNA_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(CNA_BUILD_C_API OFF CACHE BOOL "" FORCE)
    # Avatars live in Gamer Services but do not require the ENet networking module. Avoid linking
    # that unused transport into the browser bundle.
    set(CNA_ENABLE_GAMER_SERVICES ON CACHE BOOL "CNA avatars live in Gamer Services" FORCE)
    if(EMSCRIPTEN)
        set(CNA_ENABLE_NET OFF CACHE BOOL "No browser multiplayer transport is used" FORCE)
    else()
        set(CNA_ENABLE_NET ON CACHE BOOL "CNA desktop networking" FORCE)
    endif()
    set(CNA_ENABLE_VIDEO OFF CACHE STRING "" FORCE)
    set(CNA_ENABLE_DRACO OFF CACHE BOOL "" FORCE)
    # cna-engine links CNA statically: with a shared CNA the engine's and the game's copies of
    # CNA's static state would differ.
    set(CNA_SHARED_LIBRARY OFF CACHE BOOL "Build CNA as one shared library" FORCE)
    set(EASYGL_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(EASYGL_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(METAGL_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(SHARP_RUNTIME_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(CNA_SHARP_RUNTIME_ROOT "${TRAINSIM_SHARP_RUNTIME_ROOT}" CACHE PATH "" FORCE)

    # CNA's generated SDL products must never be written into the immutable dependency checkout.
    set(CNA_SDL_PREBUILT_ROOT
        "${CMAKE_BINARY_DIR}/cna-sdl-prebuilt-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}-${CMAKE_CXX_COMPILER_ID}"
        CACHE PATH "Project-local CNA SDL cache" FORCE)

    # CNA's content reader uses these vendored headers relative to the top-level source directory
    # when embedded as a subdirectory. Supply the correct immutable paths from the consumer.
    foreach(_vendored IN ITEMS cgltf stb)
        if(EXISTS "${TRAINSIM_CNA_ROOT}/third_party/${_vendored}")
            include_directories("${TRAINSIM_CNA_ROOT}/third_party/${_vendored}")
        endif()
    endforeach()

    add_subdirectory("${TRAINSIM_CNA_ROOT}" cna EXCLUDE_FROM_ALL)
    if(EMSCRIPTEN AND NOT CNA_GRAPHICS_RENDERER STREQUAL "WEBGL2")
        message(FATAL_ERROR "Train Simulator's Emscripten build requires CNA WEBGL2")
    elseif(NOT EMSCRIPTEN AND NOT CNA_GRAPHICS_RENDERER STREQUAL "OPENGL33")
        message(FATAL_ERROR "Train Simulator's desktop build requires CNA OpenGL 3.3")
    endif()

    # cna-engine's strict modules (CnaEngine::Core, CnaEngine::Graphics) over the CNA just added:
    # XNA 4.0 types only. A new compiler warning inside the engine must not break this build.
    set(CNA_ENGINE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(CNA_ENGINE_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(CNA_ENGINE_WITH_EXTENDED OFF CACHE BOOL "" FORCE)
    set(CNA_ENGINE_WITH_ENHANCED_GRAPHICS OFF CACHE BOOL "" FORCE)
    set(CNA_ENGINE_INSTALL OFF CACHE BOOL "" FORCE)
    set(CNA_ENGINE_PILOT_PROBES OFF CACHE BOOL "" FORCE)
    set(CNA_ENGINE_WARNINGS_AS_ERRORS OFF CACHE BOOL "" FORCE)
    add_subdirectory("${TRAINSIM_CNA_ENGINE_ROOT}" cna_engine EXCLUDE_FROM_ALL)

    # Preferences are cna-engine's SettingsFile, so they live beside the game rather than in the
    # CNA-free core that TRAINSIM_BUILD_GAME=OFF builds.
    add_library(trainsim_config STATIC src/config/Settings.cpp)
    target_include_directories(trainsim_config PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
    target_link_libraries(trainsim_config PUBLIC CnaEngine::Core PRIVATE trainsim_warnings)
    target_include_directories(trainsim_config SYSTEM PRIVATE "${TRAINSIM_CNA_ENGINE_ROOT}/include")

    add_library(trainsim_render STATIC
        src/audio/TrainAudio.cpp
        src/render/BitmapFont.cpp
        src/render/GpuMesh.cpp
        src/render/OpenGL33FacadeEffect.cpp
        src/render/OpenGL33WaterEffect.cpp
        src/render/MeshBuilder.cpp
        src/render/ProceduralMaterials.cpp
        src/render/SceneAvatars.cpp
        src/render/SceneBinder.cpp
        src/render/SceneRenderer.cpp
        src/render/SceneStreaming.cpp
        src/render/TrackGeometryBuild.cpp
        src/render/TrainInteriorBuild.cpp
        src/render/WorldGeometryBuild.cpp)
    target_include_directories(trainsim_render PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
    target_link_libraries(trainsim_render PUBLIC trainsim_core CNA CNA::GamerServices CnaEngine::Graphics
                          PRIVATE trainsim_warnings)
    # The engine's headers are not written for this project's -Wconversion/-Wshadow set.
    target_include_directories(trainsim_render SYSTEM PUBLIC "${TRAINSIM_CNA_ENGINE_ROOT}/include")

    add_executable(train-simulator
        src/app/Main.cpp
        src/app/TrainSimulatorGame.cpp)
    target_link_libraries(train-simulator PRIVATE trainsim_core trainsim_config trainsim_render CNA trainsim_warnings)
    if(EMSCRIPTEN)
        set_target_properties(train-simulator PROPERTIES SUFFIX ".html")
        target_link_libraries(train-simulator PRIVATE CNA::EmscriptenAsyncify)
        target_link_options(train-simulator PRIVATE
            -sALLOW_MEMORY_GROWTH=1
            -sINITIAL_MEMORY=268435456
            -sMAXIMUM_MEMORY=2147483648
            -sSTACK_SIZE=1048576
            -sFORCE_FILESYSTEM=1
            "SHELL:--preload-file ${CMAKE_CURRENT_SOURCE_DIR}/assets@/assets"
            "SHELL:--shell-file ${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html")
        cna_apply_emscripten_renderer_link_contract(train-simulator)
    endif()
    # Explicit colour-space calibration tool; no shader changes to the production renderer.
    add_executable(trainsim_colour_probe EXCLUDE_FROM_ALL tools/colour_pipeline_probe.cpp)
    target_link_libraries(trainsim_colour_probe PRIVATE trainsim_render CNA trainsim_warnings)
    if(NOT EMSCRIPTEN)
        set_target_properties(train-simulator PROPERTIES
            INSTALL_RPATH "\$ORIGIN/../${CMAKE_INSTALL_LIBDIR}")
    endif()
    if(TARGET cna_shared AND NOT EMSCRIPTEN)
        set_target_properties(cna_shared PROPERTIES INSTALL_RPATH "\$ORIGIN")
    endif()
    file(GLOB_RECURSE TRAINSIM_RUNTIME_ASSETS CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/*")
    set_property(TARGET train-simulator APPEND PROPERTY LINK_DEPENDS
        "${TRAINSIM_RUNTIME_ASSETS}")
    if(NOT EMSCRIPTEN)
        add_custom_command(TARGET train-simulator POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                    "${CMAKE_CURRENT_SOURCE_DIR}/assets" "$<TARGET_FILE_DIR:train-simulator>/assets"
            COMMENT "Copying Train Simulator runtime assets")
    endif()
    if(TARGET SDL3::SDL3main)
        target_link_libraries(train-simulator PRIVATE SDL3::SDL3main)
    endif()

    if(EMSCRIPTEN)
        install(FILES
            "${CMAKE_CURRENT_BINARY_DIR}/train-simulator.html"
            "${CMAKE_CURRENT_BINARY_DIR}/train-simulator.js"
            "${CMAKE_CURRENT_BINARY_DIR}/train-simulator.wasm"
            "${CMAKE_CURRENT_BINARY_DIR}/train-simulator.data"
            DESTINATION ".")
    else()
        install(TARGETS train-simulator
            RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
        if(TARGET cna_shared)
            install(TARGETS cna_shared
                LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}")
        endif()
        install(DIRECTORY assets/
            DESTINATION "${CMAKE_INSTALL_DATADIR}/train-simulator/assets")
        install(DIRECTORY "${CNA_SDL_PREBUILT_ROOT}/install/lib/"
            DESTINATION "${CMAKE_INSTALL_LIBDIR}"
            FILES_MATCHING PATTERN "libSDL3*.so*"
            PATTERN "cmake" EXCLUDE
            PATTERN "pkgconfig" EXCLUDE)
    endif()
    install(FILES LICENSE
        DESTINATION "${CMAKE_INSTALL_DATADIR}/licenses/train-simulator"
        RENAME TrainSimulator-LICENSE)
    install(FILES "${TRAINSIM_CNA_ROOT}/LICENSE"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/licenses/train-simulator"
        RENAME CNA-LICENSE)
    install(FILES "${TRAINSIM_SHARP_RUNTIME_ROOT}/LICENSE"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/licenses/train-simulator"
        RENAME SharpRuntime-LICENSE)
    if(NOT EMSCRIPTEN)
        install(DIRECTORY "${CNA_SDL_PREBUILT_ROOT}/install/share/licenses/"
            DESTINATION "${CMAKE_INSTALL_DATADIR}/licenses/train-simulator/SDL")
    endif()
    install(FILES README.md bugs.md LICENSE
        DESTINATION "${CMAKE_INSTALL_DATADIR}/doc/train-simulator")
    install(FILES docs/ARCHITECTURE.md docs/ASSETS.md docs/AUDIO.md docs/AUDIT.md
                  docs/NPC_AI.md docs/ROUTE.md
        DESTINATION "${CMAKE_INSTALL_DATADIR}/doc/train-simulator/docs")
    install(DIRECTORY docs/screenshots/
        DESTINATION "${CMAKE_INSTALL_DATADIR}/doc/train-simulator/docs/screenshots"
        FILES_MATCHING PATTERN "*.png")
endif()

if(TRAINSIM_BUILD_TESTS)
    enable_testing()
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    add_executable(trainsim_tests
        tests/AvatarMotionTests.cpp
        tests/EnvironmentTests.cpp
        tests/FrameTimingTests.cpp
        tests/MouseLookTests.cpp
        tests/PassengerSystemTests.cpp
        tests/RouteSimulationTests.cpp
        tests/ServiceIntegrationTests.cpp
        tests/StreamingWorldTests.cpp
        tests/TrainInteriorTests.cpp
        tests/TestMain.cpp)
    target_link_libraries(trainsim_tests PRIVATE trainsim_core trainsim_warnings)
    add_test(NAME trainsim_core COMMAND trainsim_tests)
    # Explicit developer benchmark, excluded from normal builds and timing-sensitive CTest gates.
    add_executable(trainsim_route_benchmark EXCLUDE_FROM_ALL tools/benchmark_route.cpp)
    target_link_libraries(trainsim_route_benchmark PRIVATE trainsim_core trainsim_warnings)
    add_test(NAME trainsim_generated_audio_reproducibility
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_audio.py"
                --verify-existing "${CMAKE_CURRENT_SOURCE_DIR}/assets/audio")
    set_tests_properties(trainsim_generated_audio_reproducibility PROPERTIES TIMEOUT 30)
    add_test(NAME trainsim_showcase_screenshot_integrity
        COMMAND "${Python3_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/tools/validate_showcase.py"
                --self-test
                "${CMAKE_CURRENT_SOURCE_DIR}/docs/screenshots/passenger-saloon.png"
                "${CMAKE_CURRENT_SOURCE_DIR}/docs/screenshots/passenger-information.png"
                "${CMAKE_CURRENT_SOURCE_DIR}/docs/screenshots/lakehurst-window.png"
                "${CMAKE_CURRENT_SOURCE_DIR}/docs/screenshots/stonebridge-platform.png"
                "${CMAKE_CURRENT_SOURCE_DIR}/docs/screenshots/route-map.png")
    set_tests_properties(trainsim_showcase_screenshot_integrity PROPERTIES TIMEOUT 15)

    if(TRAINSIM_BUILD_GAME)
        add_executable(trainsim_render_tests
            tests/AudioAssetTests.cpp
            tests/MeshBuilderTests.cpp
            tests/PixelGlyphTests.cpp
            tests/ProceduralMaterialsTests.cpp
            tests/SettingsTests.cpp
            tests/TestMain.cpp)
        target_link_libraries(trainsim_render_tests PRIVATE trainsim_render trainsim_config trainsim_warnings)
        target_compile_definitions(trainsim_render_tests PRIVATE
            TRAINSIM_TEST_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets")
        add_test(NAME trainsim_render COMMAND trainsim_render_tests)
        # Explicit developer benchmark of streamed world generation (PERF-002), excluded from
        # normal builds and timing-sensitive CTest gates.
        add_executable(trainsim_world_benchmark EXCLUDE_FROM_ALL tools/benchmark_world.cpp)
        target_link_libraries(trainsim_world_benchmark PRIVATE trainsim_render trainsim_warnings)
        foreach(foliage_quality RANGE 0 2)
            add_test(NAME trainsim_foliage_quality_${foliage_quality}
                COMMAND train-simulator --frames 2 --route-km 68.49 --time-scale 0.01
                        --window-view --quality ${foliage_quality} --no-audio --no-avatars
                        --report-state
                        --config "${CMAKE_BINARY_DIR}/runtime-asset-smoke/foliage-${foliage_quality}.cfg")
            if(foliage_quality EQUAL 0)
                set(foliage_expected "0")
            else()
                set(foliage_expected "[1-9][0-9]*")
            endif()
            set_tests_properties(trainsim_foliage_quality_${foliage_quality} PROPERTIES
                ENVIRONMENT "SDL_VIDEODRIVER=offscreen;SDL_AUDIODRIVER=dummy;LP_NUM_THREADS=4"
                PASS_REGULAR_EXPRESSION "foliage_material_triangles=${foliage_expected}"
                TIMEOUT 30)
        endforeach()
        add_test(NAME trainsim_interaction_smoke
            COMMAND train-simulator --interaction-smoke --no-audio --no-avatars
                    --config "${CMAKE_BINARY_DIR}/runtime-asset-smoke/interaction-smoke.cfg")
        set_tests_properties(trainsim_interaction_smoke PROPERTIES
            ENVIRONMENT "SDL_VIDEODRIVER=offscreen;SDL_AUDIODRIVER=dummy"
            TIMEOUT 30)
        add_test(NAME trainsim_audio_smoke
            COMMAND train-simulator --interaction-smoke --no-avatars --report-state
                    --config "${CMAKE_BINARY_DIR}/runtime-asset-smoke/audio-smoke.cfg")
        set_tests_properties(trainsim_audio_smoke PROPERTIES
            ENVIRONMENT "SDL_VIDEODRIVER=offscreen;SDL_AUDIODRIVER=dummy"
            PASS_REGULAR_EXPRESSION "carpet_footsteps=1 noncarpet_footsteps=1"
            TIMEOUT 30)
        add_test(NAME trainsim_pause_audio_smoke
            COMMAND train-simulator --frames 4 --pause-overlay --quality 0 --report-state
                    --config "${CMAKE_BINARY_DIR}/runtime-asset-smoke/pause-audio-smoke.cfg")
        set_tests_properties(trainsim_pause_audio_smoke PROPERTIES
            ENVIRONMENT "SDL_VIDEODRIVER=offscreen;SDL_AUDIODRIVER=dummy"
            PASS_REGULAR_EXPRESSION
                "avatar_visuals=8 ready_avatar_visuals=8.*audio_paused=1 audio_playing_instances=0 audio_paused_instances=[1-9][0-9]*.*paused=1 route_delta_m=0\\.0"
            TIMEOUT 30)
        add_test(NAME trainsim_terminal_audio_and_murmur_smoke
            COMMAND train-simulator --frames 540 --route-km 286.399 --direction 1
                    --time-scale 80 --development-dwell --car 3 --quality 0 --no-avatars
                    --report-state
                    --config "${CMAKE_BINARY_DIR}/runtime-asset-smoke/terminal-audio-and-murmur.cfg")
        set_tests_properties(trainsim_terminal_audio_and_murmur_smoke PROPERTIES
            ENVIRONMENT "SDL_VIDEODRIVER=offscreen;SDL_AUDIODRIVER=dummy"
            PASS_REGULAR_EXPRESSION
                "station_legs=[1-9][0-9]*.*terminal_announcement_events=1 passenger_murmur_audio=[1-9][0-9]*"
            # The 540-frame audio soak is intentionally substantial. It profiles near 31 seconds
            # alone, but a warm llvmpipe worker after the full suite has reached roughly twice
            # that time while still producing the correct final state. Keep it exclusive and
            # preserve the long simulation rather than weakening coverage.
            RUN_SERIAL TRUE
            TIMEOUT 90)
        add_test(NAME trainsim_welcome_announcement_smoke
            # Welcome waits 0.10 s before a 0.92 s chime; one second is insufficient at 60 Hz.
            COMMAND train-simulator --frames 120 --quality 0 --no-avatars --report-state
                    --config "${CMAKE_BINARY_DIR}/runtime-asset-smoke/welcome-announcement.cfg")
        set_tests_properties(trainsim_welcome_announcement_smoke PROPERTIES
            ENVIRONMENT "SDL_VIDEODRIVER=offscreen;SDL_AUDIODRIVER=dummy"
            PASS_REGULAR_EXPRESSION
                "announcement_chime_events=1 announcement_events=1.*announcement_last_next_station=0 announcement_last_destination=1"
            TIMEOUT 30)
        add_test(NAME trainsim_reverse_welcome_announcement_smoke
            COMMAND train-simulator --frames 120 --route-km 200 --direction -1
                    --quality 0 --no-avatars --report-state
                    --config "${CMAKE_BINARY_DIR}/runtime-asset-smoke/reverse-welcome-announcement.cfg")
        set_tests_properties(trainsim_reverse_welcome_announcement_smoke PROPERTIES
            ENVIRONMENT "SDL_VIDEODRIVER=offscreen;SDL_AUDIODRIVER=dummy"
            PASS_REGULAR_EXPRESSION
                "direction=-1.*announcement_chime_events=1 announcement_events=1.*announcement_last_next_station=0 announcement_last_destination=1"
            TIMEOUT 30)
        add_test(NAME trainsim_next_station_announcement_smoke
            # Verify the route-selected announcement independently of whether the real-time
            # 0.92-second chime has finished on this particular renderer/runner.
            COMMAND train-simulator --frames 40 --route-km 17.1 --quality 0 --no-avatars
                    --report-state
                    --config "${CMAKE_BINARY_DIR}/runtime-asset-smoke/next-station-announcement.cfg")
        set_tests_properties(trainsim_next_station_announcement_smoke PROPERTIES
            ENVIRONMENT "SDL_VIDEODRIVER=offscreen;SDL_AUDIODRIVER=dummy"
            PASS_REGULAR_EXPRESSION
                "announcement_chime_events=1.*announcement_selected_next_station=1"
            TIMEOUT 30)
        add_test(NAME trainsim_passing_train_audio_smoke
            # Keep the player service essentially fixed while the real-time opposing consist
            # crosses the rear-car listener. Otherwise a slow software-GL runner can move the
            # player beyond the final-frame culling range after the valid audio event occurred.
            COMMAND train-simulator --frames 40 --route-km 9.590 --time-scale 0.01
                    --quality 0 --no-avatars --report-state
                    --config "${CMAKE_BINARY_DIR}/runtime-asset-smoke/passing-train-smoke.cfg")
        set_tests_properties(trainsim_passing_train_audio_smoke PROPERTIES
            ENVIRONMENT "SDL_VIDEODRIVER=offscreen;SDL_AUDIODRIVER=dummy"
            PASS_REGULAR_EXPRESSION
                "avatar_rendering=0 avatar_visuals=0 ready_avatar_visuals=0.*other_train_cars=3.*other_train_cabs=2.*passing_train_audio=1"
            TIMEOUT 30)
        add_test(NAME trainsim_passing_train_view_smoke
            COMMAND train-simulator --frames 2 --route-km 9.550 --time-scale 0.01 --car 1
                    --left-window-view --quality 0 --no-audio --no-avatars --report-state
                    --config "${CMAKE_BINARY_DIR}/runtime-asset-smoke/passing-train-view.cfg")
        set_tests_properties(trainsim_passing_train_view_smoke PROPERTIES
            ENVIRONMENT "SDL_VIDEODRIVER=offscreen;SDL_AUDIODRIVER=dummy"
            PASS_REGULAR_EXPRESSION
                "other_train_cars=3 other_train_cabs=2.*player_x=-1\\.0.*player_yaw_deg=-90\\.0"
            TIMEOUT 30)
        # The first freight service (ENV-137) starts beside car 2 with all twelve vehicles drawn.
        add_test(NAME trainsim_freight_pass_view_smoke
            COMMAND train-simulator --frames 2 --route-km 37.292 --time-scale 0.01 --car 2
                    --left-window-view --quality 0 --no-audio --no-avatars --report-state)
        set_tests_properties(trainsim_freight_pass_view_smoke PROPERTIES
            ENVIRONMENT "SDL_VIDEODRIVER=offscreen;SDL_AUDIODRIVER=dummy"
            PASS_REGULAR_EXPRESSION
                "other_train_cars=12 other_train_cabs=2 other_train_locomotives=1 other_train_freight_wagons=11"
            TIMEOUT 30)
        # Chunk 111's diesel unit starts 950 m beyond its anchor, which is outside the scenery
        # window; the wider oncoming-traffic window must still draw it beside the player.
        add_test(NAME trainsim_traffic_window_view_smoke
            COMMAND train-simulator --frames 2 --route-km 28.780 --time-scale 0.01 --car 2
                    --left-window-view --quality 0 --no-audio --no-avatars --report-state)
        set_tests_properties(trainsim_traffic_window_view_smoke PROPERTIES
            ENVIRONMENT "SDL_VIDEODRIVER=offscreen;SDL_AUDIODRIVER=dummy"
            PASS_REGULAR_EXPRESSION
                "other_train_cars=2 other_train_cabs=2 other_train_locomotives=0"
            TIMEOUT 30)
        # Time of day and weather (ENV-138/139): night lighting with lit windows and lamp
        # glow, rain streaks with window beads, rain ambience, and rejected malformed options.
        add_test(NAME trainsim_night_environment_smoke
            COMMAND train-simulator --frames 2 --route-km 2 --time-scale 0.01 --car 4
                    --window-view --time 23:30 --quality 0 --no-audio --no-avatars --report-state)
        set_tests_properties(trainsim_night_environment_smoke PROPERTIES
            ENVIRONMENT "SDL_VIDEODRIVER=offscreen;SDL_AUDIODRIVER=dummy"
            PASS_REGULAR_EXPRESSION
                "clock_minutes=1410 weather=clear sun_elevation_deg=-1[0-9.]+ night=1\\.0.*night_glow_triangles=[1-9]"
            TIMEOUT 30)
        add_test(NAME trainsim_rain_environment_smoke
            COMMAND train-simulator --frames 90 --route-km 52 --time-scale 0.01 --car 2
                    --window-view --time 13:00 --weather rain --quality 0 --no-avatars
                    --report-state)
        set_tests_properties(trainsim_rain_environment_smoke PROPERTIES
            ENVIRONMENT "SDL_VIDEODRIVER=offscreen;SDL_AUDIODRIVER=dummy"
            PASS_REGULAR_EXPRESSION
                "weather=rain.*rain=1\\.0.*rain_streak_draws=[1-9][0-9]* rain_droplet_triangles=[1-9].*rain_audio=0\\.[0-9]*[1-9]"
            TIMEOUT 60)
        add_test(NAME trainsim_invalid_time_option
            COMMAND train-simulator --frames 1 --time 25:00 --no-audio)
        set_tests_properties(trainsim_invalid_time_option PROPERTIES WILL_FAIL TRUE TIMEOUT 15)
        add_test(NAME trainsim_invalid_weather_option
            COMMAND train-simulator --frames 1 --weather snow --no-audio)
        set_tests_properties(trainsim_invalid_weather_option PROPERTIES WILL_FAIL TRUE TIMEOUT 15)
        add_test(NAME trainsim_urban_traffic_view_smoke
            COMMAND train-simulator --frames 2 --route-km 2 --time-scale 0.01 --car 4
                    --window-view --quality 0 --no-audio --no-avatars --report-state
                    --config "${CMAKE_BINARY_DIR}/runtime-asset-smoke/urban-traffic-view.cfg")
        set_tests_properties(trainsim_urban_traffic_view_smoke PROPERTIES
            ENVIRONMENT "SDL_VIDEODRIVER=offscreen;SDL_AUDIODRIVER=dummy"
            PASS_REGULAR_EXPRESSION
                "drainage_sections=0 verge_tufts=0 field_boundaries=0 distant_woodland=0 distant_ridges=0.*understorey=0 distant_settlement=[1-9][0-9]* building_end_windows=[1-9][0-9]* tower_end_windows=[1-9][0-9]* warehouse_details=[1-9][0-9]* industrial_compounds=[1-9][0-9]* industrial_vehicles=[1-9][0-9]* industrial_trailers=[1-9][0-9]* industrial_utilities=[1-9][0-9]* industrial_edge_planting=[1-9][0-9]* road_edge_markings=[1-9][0-9]* road_drain_grates=[1-9][0-9]* urban_plot_surfaces=[1-9][0-9]* urban_entrance_paths=[1-9][0-9]*.*catenary_messenger=[1-9][0-9]* catenary_registration=[1-9][0-9]*.*road_vehicles=[1-9][0-9]*.*player_x=1\\.0.*player_yaw_deg=90\\.0"
            TIMEOUT 30)
        add_test(NAME trainsim_rural_verge_smoke
            COMMAND train-simulator --frames 2 --route-km 52 --time-scale 0.01 --car 3
                    --left-window-view --quality 0 --no-audio --no-avatars --report-state
                    --config "${CMAKE_BINARY_DIR}/runtime-asset-smoke/rural-verge-smoke.cfg")
        set_tests_properties(trainsim_rural_verge_smoke PROPERTIES
            ENVIRONMENT "SDL_VIDEODRIVER=offscreen;SDL_AUDIODRIVER=dummy"
            PASS_REGULAR_EXPRESSION
                "drainage_sections=[1-9][0-9]* verge_tufts=[1-9][0-9]* field_boundaries=[1-9][0-9]* distant_woodland=[1-9][0-9]* distant_ridges=[1-9][0-9]* tree_branches=[1-9][0-9]* conifer_tiers=[1-9][0-9]* understorey=0 distant_settlement=0.*road_vehicles=0.*player_x=-1\\.0.*player_yaw_deg=-90\\.0"
            TIMEOUT 30)
        add_test(NAME trainsim_hay_bale_view_smoke
            COMMAND train-simulator --frames 2 --route-km 8.625 --time-scale 0.01 --car 3
                    --left-window-view --quality 0 --no-audio --no-avatars --report-state
                    --config "${CMAKE_BINARY_DIR}/runtime-asset-smoke/hay-bale-view.cfg")
        set_tests_properties(trainsim_hay_bale_view_smoke PROPERTIES
            ENVIRONMENT "SDL_VIDEODRIVER=offscreen;SDL_AUDIODRIVER=dummy"
            PASS_REGULAR_EXPRESSION
                "hay_bale_stacks=[1-9][0-9]*.*player_x=-1\\.0.*player_yaw_deg=-90\\.0"
            TIMEOUT 30)
        add_test(NAME trainsim_station_exchange_smoke
            COMMAND train-simulator --frames 90 --station 6 --no-audio --no-avatars
                    --report-state
                    --config "${CMAKE_BINARY_DIR}/runtime-asset-smoke/station-exchange-smoke.cfg")
        set_tests_properties(trainsim_station_exchange_smoke PROPERTIES
            ENVIRONMENT "SDL_VIDEODRIVER=offscreen;SDL_AUDIODRIVER=dummy"
            PASS_REGULAR_EXPRESSION "exchange_started=4"
            TIMEOUT 30)
        add_test(NAME trainsim_reverse_station_exchange_smoke
            COMMAND train-simulator --frames 90 --station 11 --direction -1
                    --quality 0 --no-audio --no-avatars --report-state
                    --config "${CMAKE_BINARY_DIR}/runtime-asset-smoke/reverse-station-exchange-smoke.cfg")
        set_tests_properties(trainsim_reverse_station_exchange_smoke PROPERTIES
            ENVIRONMENT "SDL_VIDEODRIVER=offscreen;SDL_AUDIODRIVER=dummy"
            PASS_REGULAR_EXPRESSION
                "route_km=247\\.600 direction=-1 phase=DWELLING.*exchange_started=4"
            TIMEOUT 30)
        foreach(stream_direction IN ITEMS -1 1)
            add_test(NAME trainsim_world_prefetch_${stream_direction}
                COMMAND train-simulator --run-seconds 8 --route-km 52 --direction ${stream_direction}
                        --time-scale 4 --quality 0 --no-audio --no-avatars --report-state
                        --config "${CMAKE_BINARY_DIR}/runtime-asset-smoke/world-prefetch-${stream_direction}.cfg")
            set_tests_properties(trainsim_world_prefetch_${stream_direction} PROPERTIES
                ENVIRONMENT "SDL_VIDEODRIVER=offscreen;SDL_AUDIODRIVER=dummy"
                PASS_REGULAR_EXPRESSION
                    "world_prefetch_hits=[1-9][0-9]* world_synchronous_builds=1"
                TIMEOUT 45)
        endforeach()
        add_test(NAME trainsim_terminal_reversal_smoke
            # The station exchange intentionally uses real-time doorway traversal, so retain
            # ten simulation-update seconds for all four actors, independently of render
            # rate. Fixed-step catch-up can execute multiple updates per rendered frame.
            COMMAND train-simulator --run-seconds 10 --route-km 285.8 --direction 1
                    --time-scale 40 --development-dwell --quality 0 --no-audio --no-avatars
                    --report-state
                    --config "${CMAKE_BINARY_DIR}/runtime-asset-smoke/terminal-reversal-smoke.cfg")
        set_tests_properties(trainsim_terminal_reversal_smoke PROPERTIES
            ENVIRONMENT "SDL_VIDEODRIVER=offscreen;SDL_AUDIODRIVER=dummy"
            PASS_REGULAR_EXPRESSION
                "direction=-1.*station_legs=1.*loaded_chunks=7 passengers=38 driver_car=-1.*exchange_actors=0.*offboard=2.*world_buffer_reuses=[1-9][0-9]*"
            # Mesa software rendering and shared CI can run slower than wall-clock updates.
            TIMEOUT 90)
        add_test(NAME trainsim_map_overlay_live_smoke
            COMMAND train-simulator --frames 8 --route-km 133.4 --direction 1
                    --time-scale 10 --map-overlay --quality 0 --no-audio --no-avatars
                    --report-state
                    --config "${CMAKE_BINARY_DIR}/runtime-asset-smoke/map-overlay-live-smoke.cfg")
        set_tests_properties(trainsim_map_overlay_live_smoke PROPERTIES
            ENVIRONMENT "SDL_VIDEODRIVER=offscreen;SDL_AUDIODRIVER=dummy"
            PASS_REGULAR_EXPRESSION
                "map_open=1 hud_visible=1 paused=0 route_delta_m=[1-9][0-9]*\\.[0-9]"
            TIMEOUT 30)
        add_test(NAME trainsim_information_display_smoke
            COMMAND train-simulator --frames 8 --route-km 133.4 --direction 1
                    --time-scale 10 --inspect-display --quality 0 --no-audio --no-avatars
                    --report-state
                    --config "${CMAKE_BINARY_DIR}/runtime-asset-smoke/information-display-smoke.cfg")
        set_tests_properties(trainsim_information_display_smoke PROPERTIES
            ENVIRONMENT "SDL_VIDEODRIVER=offscreen;SDL_AUDIODRIVER=dummy"
            PASS_REGULAR_EXPRESSION
                "route_delta_m=[1-9][0-9]*\\.[0-9].*display_open=1"
            TIMEOUT 30)
        add_test(NAME trainsim_clean_view_smoke
            COMMAND train-simulator --frames 2 --hide-hud --quality 0 --no-audio --no-avatars
                    --report-state
                    --config "${CMAKE_BINARY_DIR}/runtime-asset-smoke/clean-view-smoke.cfg")
        set_tests_properties(trainsim_clean_view_smoke PROPERTIES
            ENVIRONMENT "SDL_VIDEODRIVER=offscreen;SDL_AUDIODRIVER=dummy"
            PASS_REGULAR_EXPRESSION "map_open=0 hud_visible=0 paused=0"
            TIMEOUT 30)
        add_test(NAME trainsim_longitudinal_suspension_smoke
            COMMAND train-simulator --frames 40 --route-km 50 --quality 0 --no-avatars
                    --report-state
                    --config "${CMAKE_BINARY_DIR}/runtime-asset-smoke/longitudinal-suspension.cfg")
        set_tests_properties(trainsim_longitudinal_suspension_smoke PROPERTIES
            ENVIRONMENT "SDL_VIDEODRIVER=offscreen;SDL_AUDIODRIVER=dummy"
            PASS_REGULAR_EXPRESSION
                "phase=ACCELERATING.*longitudinal_accel=0\\.6.*traction_effort=1\\.0 braking_effort=0\\.0.*camera_suspension_mdeg=-[1-9][0-9]* camera_suspension_mm=-[1-9][0-9]*"
            TIMEOUT 30)
        file(GENERATE
            OUTPUT "${CMAKE_BINARY_DIR}/reduced-motion-smoke.cfg"
            CONTENT "camera_vibration = 0\n")
        add_test(NAME trainsim_reduced_motion_smoke
            COMMAND train-simulator --frames 40 --route-km 50 --quality 0 --no-audio
                    --no-avatars --report-state
                    --config "${CMAKE_BINARY_DIR}/reduced-motion-smoke.cfg")
        set_tests_properties(trainsim_reduced_motion_smoke PROPERTIES
            ENVIRONMENT "SDL_VIDEODRIVER=offscreen;SDL_AUDIODRIVER=dummy"
            PASS_REGULAR_EXPRESSION
                "phase=ACCELERATING.*longitudinal_accel=0\\.6.*camera_suspension_mdeg=0 camera_suspension_mm=0"
            TIMEOUT 30)
        add_test(NAME trainsim_braking_suspension_audio_smoke
            COMMAND train-simulator --frames 40 --route-km 18.2 --direction 1 --quality 0
                    --no-avatars --report-state
                    --config "${CMAKE_BINARY_DIR}/runtime-asset-smoke/braking-suspension.cfg")
        set_tests_properties(trainsim_braking_suspension_audio_smoke PROPERTIES
            ENVIRONMENT "SDL_VIDEODRIVER=offscreen;SDL_AUDIODRIVER=dummy"
            PASS_REGULAR_EXPRESSION
                "phase=BRAKING.*longitudinal_accel=-0\\.8.*traction_effort=0\\.0 braking_effort=1\\.0.*camera_suspension_mdeg=[1-9][0-9]* camera_suspension_mm=[1-9][0-9]*"
            TIMEOUT 30)
        add_test(NAME trainsim_interior_sunlight_smoke
            COMMAND train-simulator --frames 2 --sunlight-view --quality 0 --no-audio
                    --no-avatars --report-state
                    --config "${CMAKE_BINARY_DIR}/runtime-asset-smoke/interior-sunlight.cfg")
        set_tests_properties(trainsim_interior_sunlight_smoke PROPERTIES
            # Six carpet spans: four perimeter tapes plus nine installation seams each.
            ENVIRONMENT "SDL_VIDEODRIVER=offscreen;SDL_AUDIODRIVER=dummy"
            PASS_REGULAR_EXPRESSION
                "pa_speakers=12 hvac_vents=12 car_signs=6 car_function_groups=6 cab_control_groups=2 active_cab_displays=1 vestibule_portal_details=6 overhead_luggage=27 floor_luggage_cases=14 luggage_wheel_pairs=6 rounded_seat_cushions=182 rounded_seat_backs=182 seat_pedestals=182 seat_mount_fasteners=364 rounded_armrests=93 rounded_table_tops=33 table_support_plates=33 window_reflections=0 interior_light_gradients=0 sunlight_patches=23 aisle_inlays=78 floor_zones=12 interior_coving=36 ceiling_panel_seams=120 vestibule_wear=24"
            TIMEOUT 30)
        add_test(NAME trainsim_interior_hvac_smoke
            COMMAND train-simulator --frames 2 --quality 0 --no-avatars --report-state
                    --config "${CMAKE_BINARY_DIR}/runtime-asset-smoke/interior-hvac.cfg")
        set_tests_properties(trainsim_interior_hvac_smoke PROPERTIES
            ENVIRONMENT "SDL_VIDEODRIVER=offscreen;SDL_AUDIODRIVER=dummy"
            PASS_REGULAR_EXPRESSION
                "avatar_rendering=0 avatar_visuals=0 ready_avatar_visuals=0.*pa_speakers=12 hvac_vents=12 car_signs=6 car_function_groups=6 cab_control_groups=2 active_cab_displays=1 vestibule_portal_details=6 overhead_luggage=27 floor_luggage_cases=14 luggage_wheel_pairs=6 rounded_seat_cushions=182 rounded_seat_backs=182 seat_pedestals=182 seat_mount_fasteners=364 rounded_armrests=93 rounded_table_tops=33 table_support_plates=33 window_reflections=0 interior_light_gradients=0 sunlight_patches=23 aisle_inlays=78 floor_zones=12 interior_coving=36 ceiling_panel_seams=120 vestibule_wear=24.*hvac_audio=1"
            TIMEOUT 30)
        add_test(NAME trainsim_platform_view_smoke
            COMMAND train-simulator --frames 8 --station 3 --direction 1
                    --platform-view --quality 0 --no-audio --no-avatars --report-state
                    --config "${CMAKE_BINARY_DIR}/runtime-asset-smoke/platform-view-smoke.cfg")
        set_tests_properties(trainsim_platform_view_smoke PROPERTIES
            ENVIRONMENT "SDL_VIDEODRIVER=offscreen;SDL_AUDIODRIVER=dummy"
            PASS_REGULAR_EXPRESSION
                "route_km=39\\.800 direction=1 phase=DWELLING.*platform_joints=62 canopy_lights=3 canopy_drainage=1 platform_planters=0 platform_posters=2 platform_cabinets=1 platform_benches=2 platform_weathering=0 station_facade_streaks=0 platform_boundaries=[1-9][0-9]*.*player_x=-1\\.0"
            TIMEOUT 30)
        add_test(NAME trainsim_door_view_smoke
            COMMAND train-simulator --frames 8 --station 4 --direction 1
                    --door-view --quality 0 --no-audio --no-avatars --report-state
                    --config "${CMAKE_BINARY_DIR}/runtime-asset-smoke/door-view-smoke.cfg")
        set_tests_properties(trainsim_door_view_smoke PROPERTIES
            ENVIRONMENT "SDL_VIDEODRIVER=offscreen;SDL_AUDIODRIVER=dummy"
            PASS_REGULAR_EXPRESSION
                "route_km=63\\.200 direction=1 phase=DWELLING.*boarding_steps=6.*player_x=1\\.3"
            TIMEOUT 30)
        add_test(NAME trainsim_left_door_view_smoke
            COMMAND train-simulator --frames 8 --station 3 --direction 1
                    --door-view --quality 0 --no-audio --no-avatars --report-state
                    --config "${CMAKE_BINARY_DIR}/runtime-asset-smoke/left-door-view-smoke.cfg")
        set_tests_properties(trainsim_left_door_view_smoke PROPERTIES
            ENVIRONMENT "SDL_VIDEODRIVER=offscreen;SDL_AUDIODRIVER=dummy"
            PASS_REGULAR_EXPRESSION
                "route_km=39\\.800 direction=1 phase=DWELLING.*boarding_steps=6.*player_x=-1\\.3"
            TIMEOUT 30)
        add_test(NAME trainsim_vestibule_view_smoke
            COMMAND train-simulator --frames 2 --vestibule-view --car 4
                    --quality 0 --no-audio --no-avatars --report-state
                    --config "${CMAKE_BINARY_DIR}/runtime-asset-smoke/vestibule-view-smoke.cfg")
        set_tests_properties(trainsim_vestibule_view_smoke PROPERTIES
            ENVIRONMENT "SDL_VIDEODRIVER=offscreen;SDL_AUDIODRIVER=dummy"
            PASS_REGULAR_EXPRESSION "vestibule_portal_details=6.*player_x=0\\.0 player_z=17\\.9"
            TIMEOUT 30)
        add_test(NAME trainsim_toilet_view_smoke
            COMMAND train-simulator --frames 2 --toilet-view
                    --quality 0 --no-audio --no-avatars --report-state
                    --config "${CMAKE_BINARY_DIR}/runtime-asset-smoke/toilet-view-smoke.cfg")
        set_tests_properties(trainsim_toilet_view_smoke PROPERTIES
            ENVIRONMENT "SDL_VIDEODRIVER=offscreen;SDL_AUDIODRIVER=dummy"
            PASS_REGULAR_EXPRESSION
                "toilet_occupied_indicators=1.*player_x=0\\.8 player_z=6\\.0"
            TIMEOUT 30)
        add_test(NAME trainsim_sink_view_smoke
            COMMAND train-simulator --frames 2 --sink-view
                    --quality 0 --no-audio --no-avatars --report-state
                    --config "${CMAKE_BINARY_DIR}/runtime-asset-smoke/sink-view-smoke.cfg")
        set_tests_properties(trainsim_sink_view_smoke PROPERTIES
            ENVIRONMENT "SDL_VIDEODRIVER=offscreen;SDL_AUDIODRIVER=dummy"
            PASS_REGULAR_EXPRESSION
                "toilet_occupied_indicators=1.*player_x=0\\.8 player_z=5\\.[67].*toilet_door_open=0"
            TIMEOUT 30)
        add_test(NAME trainsim_sink_view_conflict
            COMMAND train-simulator --sink-view --toilet-view)
        set_tests_properties(trainsim_sink_view_conflict PROPERTIES WILL_FAIL TRUE TIMEOUT 10)
        add_test(NAME trainsim_signal_view_smoke
            COMMAND train-simulator --frames 2 --signal-view --quality 0
                    --no-audio --no-avatars --report-state
                    --config "${CMAKE_BINARY_DIR}/runtime-asset-smoke/signal-view-smoke.cfg")
        set_tests_properties(trainsim_signal_view_smoke PROPERTIES
            ENVIRONMENT "SDL_VIDEODRIVER=offscreen;SDL_AUDIODRIVER=dummy"
            PASS_REGULAR_EXPRESSION
                "route_km=0\\.220 direction=1.*signals_g=[1-9].*player_x=1\\.0"
            TIMEOUT 30)
        add_test(NAME trainsim_moving_door_interlock_smoke
            COMMAND train-simulator --frames 2 --route-km 50 --direction 1
                    --quality 0 --no-avatars --report-state
                    --config "${CMAKE_BINARY_DIR}/runtime-asset-smoke/moving-door-interlock.cfg")
        set_tests_properties(trainsim_moving_door_interlock_smoke PROPERTIES
            ENVIRONMENT "SDL_VIDEODRIVER=offscreen;SDL_AUDIODRIVER=dummy"
            PASS_REGULAR_EXPRESSION
                "phase=ACCELERATING speed_kmh=[1-9][0-9]*\\.[0-9].*boarding_steps=0.*door_audio=0"
            TIMEOUT 30)
        add_test(NAME trainsim_departure_door_interlock_smoke
            COMMAND train-simulator --frames 180 --station 4 --development-dwell
                    --time-scale 20 --quality 0 --no-avatars --report-state
                    --config "${CMAKE_BINARY_DIR}/runtime-asset-smoke/departure-door-interlock.cfg")
        set_tests_properties(trainsim_departure_door_interlock_smoke PROPERTIES
            ENVIRONMENT "SDL_VIDEODRIVER=offscreen;SDL_AUDIODRIVER=dummy"
            PASS_REGULAR_EXPRESSION
                "phase=(ACCELERATING|CRUISING) speed_kmh=[1-9][0-9]*\\.[0-9].*boarding_steps=0.*door_audio=1"
            TIMEOUT 45)
        foreach(water_quality RANGE 0 2)
            add_test(NAME trainsim_water_shader_${water_quality}
                COMMAND train-simulator --frames 3 --route-km 84 --window-view
                        --quality ${water_quality} --no-audio --no-avatars --report-state
                        --config "${CMAKE_CURRENT_SOURCE_DIR}/tests/facade-comparison.cfg")
            if(water_quality EQUAL 0)
                set(water_shader_draws 0)
            else()
                set(water_shader_draws 1)
            endif()
            set_tests_properties(trainsim_water_shader_${water_quality} PROPERTIES
                ENVIRONMENT "SDL_VIDEODRIVER=offscreen;SDL_AUDIODRIVER=dummy"
                PASS_REGULAR_EXPRESSION "water_shader_draws=${water_shader_draws}"
                TIMEOUT 30)
        endforeach()
        add_test(NAME trainsim_lakeside_landmark_smoke
            COMMAND train-simulator --frames 2 --route-km 84 --direction 1
                    --window-view --quality 0 --no-audio --no-avatars --report-state
                    --config "${CMAKE_BINARY_DIR}/runtime-asset-smoke/lakeside-landmark.cfg")
        set_tests_properties(trainsim_lakeside_landmark_smoke PROPERTIES
            ENVIRONMENT "SDL_VIDEODRIVER=offscreen;SDL_AUDIODRIVER=dummy"
            PASS_REGULAR_EXPRESSION
                "route_km=84\\..*water_material_triangles=[1-9][0-9]*.*lake_patches=[1-9][0-9]* lake_bank_trees=[1-9][0-9]*.*km_posts=[1-9][0-9]*.*catenary_droppers=[1-9][0-9]*"
            TIMEOUT 30)
        add_test(NAME trainsim_waterfront_station_smoke
            COMMAND train-simulator --frames 2 --station 5 --platform-view --quality 0
                    --no-audio --no-avatars --report-state
                    --config "${CMAKE_BINARY_DIR}/runtime-asset-smoke/waterfront-station.cfg")
        set_tests_properties(trainsim_waterfront_station_smoke PROPERTIES
            ENVIRONMENT "SDL_VIDEODRIVER=offscreen;SDL_AUDIODRIVER=dummy"
            PASS_REGULAR_EXPRESSION
                "water_material_triangles=[1-9][0-9]*.*river_water_sections=0 waterfront_sections=3"
            TIMEOUT 30)
        add_test(NAME trainsim_tunnel_environment_smoke
            COMMAND train-simulator --frames 70 --route-km 123 --window-view --quality 0
                    --no-avatars --report-state
                    --config "${CMAKE_BINARY_DIR}/runtime-asset-smoke/tunnel-environment.cfg")
        set_tests_properties(trainsim_tunnel_environment_smoke PROPERTIES
            ENVIRONMENT "SDL_VIDEODRIVER=offscreen;SDL_AUDIODRIVER=dummy"
            PASS_REGULAR_EXPRESSION
                "river_water_sections=0.*tunnel_luminaires=[1-9][0-9]* tunnel_niches=[1-9][0-9]* bridge_sections=0 river_bridge_sections=0 viaduct_sections=0 bridge_abutments=0.*sunlight_patches=0.*tunnel_audio=1 bridge_audio=0.*tunnel_lining_rings=[1-9][0-9]* tunnel_marker_plates=[1-9][0-9]*"
            TIMEOUT 30)
        add_test(NAME trainsim_river_bank_smoke
            COMMAND train-simulator --frames 3 --route-km 74.5 --window-view --quality 1
                    --no-audio --no-avatars --report-state
                    --config "${CMAKE_CURRENT_SOURCE_DIR}/tests/facade-comparison.cfg")
        set_tests_properties(trainsim_river_bank_smoke PROPERTIES
            ENVIRONMENT "SDL_VIDEODRIVER=offscreen;SDL_AUDIODRIVER=dummy"
            PASS_REGULAR_EXPRESSION "river_bank_clumps=[1-9][0-9]*"
            TIMEOUT 30)
        add_test(NAME trainsim_bridge_environment_smoke
            COMMAND train-simulator --frames 5 --route-km 74.5 --window-view --quality 0
                    --no-avatars --report-state
                    --config "${CMAKE_BINARY_DIR}/runtime-asset-smoke/bridge-environment.cfg")
        set_tests_properties(trainsim_bridge_environment_smoke PROPERTIES
            ENVIRONMENT "SDL_VIDEODRIVER=offscreen;SDL_AUDIODRIVER=dummy"
            PASS_REGULAR_EXPRESSION
                "water_material_triangles=[1-9][0-9]*.*river_water_sections=[1-9][0-9]*.*tunnel_luminaires=0 tunnel_niches=0 bridge_sections=[1-9][0-9]* river_bridge_sections=[1-9][0-9]* viaduct_sections=0 bridge_abutments=2.*tunnel_audio=0 bridge_audio=1"
            TIMEOUT 30)
        add_test(NAME trainsim_viaduct_environment_smoke
            COMMAND train-simulator --frames 5 --route-km 177.9 --window-view --quality 0
                    --no-avatars --report-state
                    --config "${CMAKE_BINARY_DIR}/runtime-asset-smoke/viaduct-environment.cfg")
        set_tests_properties(trainsim_viaduct_environment_smoke PROPERTIES
            ENVIRONMENT "SDL_VIDEODRIVER=offscreen;SDL_AUDIODRIVER=dummy"
            PASS_REGULAR_EXPRESSION
                "river_water_sections=0.*tunnel_luminaires=0 tunnel_niches=0 bridge_sections=[1-9][0-9]* river_bridge_sections=0 viaduct_sections=[1-9][0-9]* bridge_abutments=1.*tunnel_audio=0 bridge_audio=1"
            TIMEOUT 30)
        add_test(NAME trainsim_station_clock_smoke
            COMMAND train-simulator --frames 2 --station 1 --quality 0 --no-audio
                    --no-avatars --report-state
                    --config "${CMAKE_BINARY_DIR}/runtime-asset-smoke/station-clock-smoke.cfg")
        set_tests_properties(trainsim_station_clock_smoke PROPERTIES
            ENVIRONMENT "SDL_VIDEODRIVER=offscreen;SDL_AUDIODRIVER=dummy"
            PASS_REGULAR_EXPRESSION "platform_planters=2.*clock_hands=2"
            TIMEOUT 30)
        add_test(NAME trainsim_level_crossing_smoke
            COMMAND train-simulator --frames 2 --route-km 7.2 --quality 0 --no-audio
                    --no-avatars --report-state
                    --config "${CMAKE_BINARY_DIR}/runtime-asset-smoke/level-crossing-smoke.cfg")
        set_tests_properties(trainsim_level_crossing_smoke PROPERTIES
            ENVIRONMENT "SDL_VIDEODRIVER=offscreen;SDL_AUDIODRIVER=dummy"
            PASS_REGULAR_EXPRESSION "crossing_barriers=2 crossing_lights=2"
            TIMEOUT 30)
        add_test(NAME trainsim_level_crossing_audio_smoke
            COMMAND train-simulator --frames 5 --route-km 7.2 --quality 0 --no-avatars
                    --report-state
                    --config "${CMAKE_BINARY_DIR}/runtime-asset-smoke/level-crossing-audio-smoke.cfg")
        set_tests_properties(trainsim_level_crossing_audio_smoke PROPERTIES
            ENVIRONMENT "SDL_VIDEODRIVER=offscreen;SDL_AUDIODRIVER=dummy"
            PASS_REGULAR_EXPRESSION
                "crossing_barriers=2 crossing_lights=2 crossing_audio=1"
            TIMEOUT 30)
        add_test(NAME trainsim_ai_level_crossing_smoke
            COMMAND train-simulator --frames 1 --route-km 267.575 --time-scale 0.01
                    --quality 0 --no-audio --no-avatars --report-state
                    --config "${CMAKE_BINARY_DIR}/runtime-asset-smoke/ai-level-crossing-smoke.cfg")
        set_tests_properties(trainsim_ai_level_crossing_smoke PROPERTIES
            ENVIRONMENT "SDL_VIDEODRIVER=offscreen;SDL_AUDIODRIVER=dummy"
            PASS_REGULAR_EXPRESSION
                "other_train_cars=3.*crossing_barriers=2 crossing_lights=2"
            TIMEOUT 30)
        add_test(NAME trainsim_interrupted_exchange_recovery_smoke
            COMMAND train-simulator --frames 210 --station 6 --development-dwell
                    --quality 0 --no-audio --no-avatars --report-state
                    --config "${CMAKE_BINARY_DIR}/runtime-asset-smoke/interrupted-exchange-recovery.cfg")
        set_tests_properties(trainsim_interrupted_exchange_recovery_smoke PROPERTIES
            ENVIRONMENT "SDL_VIDEODRIVER=offscreen;SDL_AUDIODRIVER=dummy"
            PASS_REGULAR_EXPRESSION "exchange_actors=0 exchange_started=4 offboard=2"
            # This is deliberately the longest visual smoke at 210 software-GL frames. Leave
            # enough headroom for a loaded CI/desktop without masking a real hang indefinitely.
            TIMEOUT 60)
        foreach(vsync_requested IN ITEMS 0 1)
            set(vsync_argument)
            if(NOT vsync_requested)
                set(vsync_argument --no-vsync)
            endif()
            add_test(NAME trainsim_vsync_request_${vsync_requested}
                COMMAND train-simulator --frames 2 --no-audio --no-avatars --report-state
                        ${vsync_argument}
                        --config "${CMAKE_BINARY_DIR}/runtime-asset-smoke/vsync-${vsync_requested}.cfg")
            set_tests_properties(trainsim_vsync_request_${vsync_requested} PROPERTIES
                ENVIRONMENT "SDL_VIDEODRIVER=offscreen;SDL_AUDIODRIVER=dummy"
                PASS_REGULAR_EXPRESSION "vsync_requested=${vsync_requested} fixed_time_step=1"
                TIMEOUT 30)
        endforeach()
        add_test(NAME trainsim_variable_step_diagnostic
            COMMAND train-simulator --frames 3 --quality 0 --no-audio --no-avatars --report-state
                    --variable-step
                    --config "${CMAKE_BINARY_DIR}/runtime-asset-smoke/variable-step.cfg")
        set_tests_properties(trainsim_variable_step_diagnostic PROPERTIES
            ENVIRONMENT "SDL_VIDEODRIVER=offscreen;SDL_AUDIODRIVER=dummy;LP_NUM_THREADS=4"
            PASS_REGULAR_EXPRESSION "vsync_requested=1 fixed_time_step=0"
            TIMEOUT 30)
        foreach(facade_quality IN ITEMS 1 2)
            add_test(NAME trainsim_facade_lighting_${facade_quality}
                COMMAND "${Python3_EXECUTABLE}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/tools/test_facade_lighting.py"
                        "$<TARGET_FILE:train-simulator>"
                        "${CMAKE_BINARY_DIR}/facade-regression-${facade_quality}"
                        --quality ${facade_quality})
            set_tests_properties(trainsim_facade_lighting_${facade_quality} PROPERTIES
                ENVIRONMENT "SDL_VIDEODRIVER=offscreen;SDL_AUDIODRIVER=dummy;LP_NUM_THREADS=4"
                TIMEOUT 120)
        endforeach()
        add_test(NAME trainsim_high_quality_msaa_smoke
            COMMAND train-simulator --frames 2 --quality 2 --no-audio --no-avatars
                    --car 2 --report-state
                    --screenshot "${CMAKE_BINARY_DIR}/runtime-asset-smoke/material-smoke.png"
                    --config "${CMAKE_BINARY_DIR}/runtime-asset-smoke/high-quality-msaa.cfg")
        set_tests_properties(trainsim_high_quality_msaa_smoke PROPERTIES
            ENVIRONMENT "SDL_VIDEODRIVER=offscreen;SDL_AUDIODRIVER=dummy"
            PASS_REGULAR_EXPRESSION
                # 48 triangles per soft patch: 93 seat pairs, 182 seat bases, 33 tables;
                # six unchanged luggage shadows contribute another 12 triangles.
                "graphics_quality=2 msaa_samples=[1-9][0-9]* terrain_material_triangles=[1-9][0-9]* ballast_material_triangles=[1-9][0-9]* rail_head_material_triangles=[1-9][0-9]* track_fasteners=[1-9][0-9]* turnout_motors=[1-9][0-9]* turnout_blades=[1-9][0-9]* interior_material_triangles=[1-9][0-9]* interior_panel_material_triangles=[1-9][0-9]* interior_resilient_material_triangles=[1-9][0-9]* water_material_triangles=[0-9]+ facade_material_triangles=[1-9][0-9]* rendered_facade_material_triangles=[1-9][0-9]* static_shadow_triangles=[1-9][0-9]* platform_material_triangles=[1-9][0-9]* interior_shadow_triangles=14796.*station_facade_streaks=[1-9][0-9]*.*catenary_foundations=[1-9][0-9]*.*lineside_cabinets=[1-9][0-9]*.*window_reflections=204 interior_light_gradients=24.*screenshot_saved=1"
            TIMEOUT 30)
        add_test(NAME trainsim_material_capture_integrity
            COMMAND "${Python3_EXECUTABLE}"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tools/validate_showcase.py"
                    --scene-materials
                    "${CMAKE_BINARY_DIR}/runtime-asset-smoke/material-smoke.png")
        set_tests_properties(trainsim_material_capture_integrity PROPERTIES
            DEPENDS trainsim_high_quality_msaa_smoke
            TIMEOUT 15)
        add_test(NAME trainsim_passenger_display_capture
            COMMAND train-simulator --frames 3 --quality 2 --route-km 52 --car 2
                    --time-scale 0.000001 --no-audio --no-avatars --hide-hud --report-state
                    --config "${CMAKE_CURRENT_SOURCE_DIR}/tests/facade-comparison.cfg"
                    --screenshot "${CMAKE_BINARY_DIR}/runtime-asset-smoke/passenger-display.png")
        set_tests_properties(trainsim_passenger_display_capture PROPERTIES
            ENVIRONMENT "SDL_VIDEODRIVER=offscreen;SDL_AUDIODRIVER=dummy"
            PASS_REGULAR_EXPRESSION "screenshot_saved=1"
            TIMEOUT 30)
        add_test(NAME trainsim_passenger_display_integrity
            COMMAND "${Python3_EXECUTABLE}"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tools/validate_display_capture.py"
                    "${CMAKE_BINARY_DIR}/runtime-asset-smoke/passenger-display.png")
        set_tests_properties(trainsim_passenger_display_integrity PROPERTIES
            DEPENDS trainsim_passenger_display_capture TIMEOUT 15)
        add_test(NAME trainsim_low_quality_budget_smoke
            COMMAND train-simulator --frames 70 --station 7 --quality 0 --no-audio
                    --report-state
                    --screenshot "${CMAKE_BINARY_DIR}/runtime-asset-smoke/avatar-ready.png"
                    --config "${CMAKE_BINARY_DIR}/runtime-asset-smoke/low-quality-budget.cfg")
        set_tests_properties(trainsim_low_quality_budget_smoke PROPERTIES
            ENVIRONMENT "SDL_VIDEODRIVER=offscreen;SDL_AUDIODRIVER=dummy"
            PASS_REGULAR_EXPRESSION
                "graphics_quality=0 msaa_samples=0.*visible_npc=8 npc_gaze=[0-9]+ npc_reading=[0-9]+ npc_phone=[0-9]+ npc_social=[0-9]+ npc_distance_gait=[0-9]+ npc_seat_transitions=[0-9]+ npc_acknowledgement=[0-9]+ conductor_devices=[0-9]+ platform_phone=1 platform_social=[0-9]+ sky_clouds=4 avatar_rendering=1 avatar_visuals=8 ready_avatar_visuals=8 civilian_avatar_pool=7.*platform_luggage=1.*screenshot_saved=1"
            TIMEOUT 30)
        add_test(NAME trainsim_showcase_population_smoke
            COMMAND train-simulator --frames 90 --car 3 --quality 2 --no-audio
                    --report-state
                    --config "${CMAKE_BINARY_DIR}/runtime-asset-smoke/showcase-population.cfg")
        set_tests_properties(trainsim_showcase_population_smoke PROPERTIES
            ENVIRONMENT "SDL_VIDEODRIVER=offscreen;SDL_AUDIODRIVER=dummy"
            PASS_REGULAR_EXPRESSION
                "passengers=38.*exchange_started=4.*visible_npc=2[0-9] npc_gaze=[1-9][0-9]* npc_reading=[1-9][0-9]* npc_phone=[1-9][0-9]* npc_social=[0-9]+ npc_distance_gait=[0-9]+ npc_seat_transitions=[0-9]+ npc_acknowledgement=[0-9]+ conductor_devices=[0-9]+ platform_phone=1 platform_social=[1-9][0-9]* sky_clouds=8 avatar_rendering=1 avatar_visuals=24 ready_avatar_visuals=24 civilian_avatar_pool=23.*platform_seated=3 platform_suitcases=2"
            # Native avatar construction and first upload profile near 21 seconds alone; retain a
            # measured full-suite margin without reducing the 90-frame readiness/exchange check.
            TIMEOUT 45)
        file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/runtime-asset-smoke")
        add_test(NAME trainsim_sidecar_assets
            COMMAND train-simulator --frames 2 --no-audio --no-avatars
                    --config "${CMAKE_BINARY_DIR}/runtime-asset-smoke/sidecar-smoke.cfg")
        set_tests_properties(trainsim_sidecar_assets PROPERTIES
            WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/runtime-asset-smoke"
            ENVIRONMENT "SDL_VIDEODRIVER=offscreen;SDL_AUDIODRIVER=dummy"
            TIMEOUT 30)

        # CNA avatar construction and first EasyGL upload are lifecycle-sensitive on the current
        # dependency revision. Keep explicit avatar coverage, but give those three processes full
        # runner exclusivity: a resource lock shared only by avatar tests still allowed another
        # no-avatar Mesa process beside them. Unrelated OpenGL smokes remain parallelizable with
        # each other. Normal gameplay and showcase captures retain the production population.
        set(_trainsim_avatar_smokes
            trainsim_pause_audio_smoke
            trainsim_low_quality_budget_smoke
            trainsim_showcase_population_smoke)
        set_tests_properties(${_trainsim_avatar_smokes} PROPERTIES RUN_SERIAL TRUE)

        add_test(NAME trainsim_install_layout
            COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/tools/verify_install.sh"
                    "${CMAKE_BINARY_DIR}")
        set_tests_properties(trainsim_install_layout PROPERTIES
            RUN_SERIAL TRUE
            TIMEOUT 30)
    endif()
endif()
