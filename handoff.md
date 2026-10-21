# Train Simulator — AI development handoff

Updated: 2026-10-12 late evening (the whole goal list is implemented, documented, verified and,
with the owner's approval, committed and pushed to `origin/develop` as `de92449`); on
2026-10-21 the game was moved onto cna-engine on the branch `engine-adoption` (see "cna-engine
adoption" below).
Repository: `libcna/train-simulator`, current branch `develop`. Written for the next agent
(expected: Codex, "astra high") starting from a fresh context.

## Owner instructions so far

- 2026-10-09: finish the river-bank repair, write this English handoff, commit,
  push, stop. Done; that commit was the initial project snapshot.
- 2026-10-10: resume from this document and continue the visual campaign until
  every ranked finding is done, pushing after each group. Done: ENV-067 to
  ENV-123 were repaired, committed and pushed one group at a time (see the Git
  history). The owner then asked to finish the current item, rewrite this
  handoff, commit, push and stop. **Do not continue work on your own; wait for
  the owner's next instruction.** The owner manages the application goal; never
  create or change goals implicitly.
- 2026-10-11: the owner resumed from this handoff, then explicitly requested
  implementation of the agreed work followed by an indefinite loop of fresh
  Train Simulator audits, proposed improvements and implementation. The earlier
  stop instruction is superseded. Continue useful work without asking between
  groups. The owner still manages the application goal; do not mark this ongoing
  goal complete merely because one engineering item is finished.
- Later on 2026-10-11 the owner narrowed the goal to the work estimated at
  roughly 4–6 hours: finish streaming and its validation; perform a long
  keyboard/mouse, six-car/interactions check and fix reproduced issues; run a
  multi-hour stability/memory test, evaluate it and fix discovered failures.
  **Do not start further audits or add unrelated improvements after those items.**
  This supersedes the indefinite audit loop requested earlier the same day.
- 2026-10-12: the owner resumed from this handoff and asked for a new visual
  audit, an analysis of the remaining streaming risks and repairs of the visual
  design gaps. Done in this session: pass-2 audit ENV-124 to ENV-133 (all
  addressed, `docs/VISUAL_AUDIT_2.md`) and PERF-002 (`docs/STREAMING.md`). The
  work was committed and pushed to `origin/develop` with the owner's approval.
  Do not start further work on
  your own.
- Later on 2026-10-12 the owner set a new goal list (Czech, translated): *faster world
  preparation; split `SceneRenderer`; more varied building floor plans; busier platform life:
  more people waiting on benches, travellers with suitcases and more varied station
  furniture; more types of oncoming trains such as freight trains and other passenger sets;
  time of day and weather.* The first four are implemented (see "Work in progress" below);
  the last two are **not started**. The owner then asked to finish the current item and
  write this handoff. The work is **not committed or pushed**; ask the owner before
  committing. Continue with the remaining goal items only when the owner asks you to.
- Still on 2026-10-12 the owner said (Czech) "read handoff.md and continue". The next agent
  finished the remaining goal items (oncoming train types ENV-137, time of day ENV-138, weather
  ENV-139), the outstanding documentation for ENV-134/136 and PERF-003, formal capture scripts,
  and repairs of defects its captures exposed. The owner then asked to commit and push
  everything; that is done (`de92449` on `origin/develop`). Do not start further work without a
  new instruction.
- 2026-10-21: working in `libcna/cna-engine`, the owner asked (Czech) "migrate the train
  simulator" onto cna-engine. Done on the branch `engine-adoption` (worktree
  `libcnalab/train-simulator-engine`); at the owner's request it was fast-forwarded into
  `develop` and pushed the same day, after cna-engine's `develop`. See "cna-engine adoption"
  below.
- The owner's conversation is Czech; all repository content, commit messages
  and this document are English.

## What this project is

A playable but unfinished C++23 first-person passenger simulator on CNA's
OpenGL 3.3 renderer. The fictional Northern Valley Express runs a 286.4 km,
12-station route indefinitely, reversing at both termini. Six passenger cars,
seating, gangways, toilet interactions, passenger/conductor CNA avatars, route
map, passenger displays, generated railway audio and offline English
announcements are integrated. The player stays aboard; it is not a driving game.
Interactive launches open on a start menu (Start / Enter / Space; F12 quits);
automated runs with `--frames`, `--run-seconds`, `--screenshot` or
`--interaction-smoke` skip it.

Read, in this order: `README.md`, `docs/ARCHITECTURE.md`, `docs/VISUAL_AUDIT.md`,
`docs/VISUAL_AUDIT_2.md`, `docs/VISUAL_CAMPAIGN.md`, `docs/AUDIT.md`, `bugs.md`, then
`docs/STREAMING.md` (PERF-001 and PERF-002) and `docs/STABILITY.md`. Automated tests do not
prove realism; only inspected engine screenshots do.

## Non-negotiable constraints

- Sibling dependencies `../cna` (CNA `next`) and `../sharp-runtime` are
  immutable: never edit, patch, stash or commit in them. Record probable
  dependency defects in `bugs.md`; implement workarounds here. Other agents work
  in those checkouts concurrently; on 2026-10-10 `sharp-runtime` briefly failed
  to compile mid-edit and recovered on its own. If that happens, wait and retry;
  do not touch it.
- Desktop CNA **OPENGL33 only**. No other renderers, web/mobile or platform
  work. Keep simulation renderer-independent.
- C++23, MIT project licence; keep third-party notices in `docs/ASSETS.md` and
  `assets/licenses/`.
- Build with at most **four compilation workers**, the system `/usr/bin/ccache`,
  `CCACHE_DIR=/rv/cnaccache`, `CCACHE_BASEDIR=/rv`. Never create another cache
  or change the shared cache size. Use only the existing `build/`; no clean
  rebuilds, duplicate dependency clones or new build directories.
- Never run two heavy jobs at once (build, CTest, GPU captures). Other agents
  share the host; load averages above 20 were common on 2026-10-10 and distort
  frame-time measurements.
- Preserve unrelated work. Do not spawn subagents unless the owner says so.

## Build, run and verification

```bash
export CCACHE_DIR=/rv/cnaccache CCACHE_BASEDIR=/rv
cmake --build build --parallel 4                 # incremental; already configured
./build/trainsim_render_tests                    # 209 render/geometry/settings unit tests
./build/trainsim_tests                           # 126 simulation unit tests
env LP_NUM_THREADS=4 ctest --test-dir build --parallel 1 --output-on-failure   # 75 tests; shared-host duration varies
cmake --build build --parallel 4 --target trainsim_world_benchmark   # optional CPU window benchmark
python3 tools/test_streaming_risks.py            # six moving GPU scenarios (wraps itself in the private display)
./build/train-simulator                          # interactive play (start menu)
```

Private hardware-GPU captures (Radeon 780M via a private Weston/Xwayland; never
touches the owner's desktop):

```bash
env LP_NUM_THREADS=4 ../cna/tools/platform/run_gpu_tests_private.sh --exec \
  env SDL_VIDEODRIVER=x11 SDL_AUDIODRIVER=dummy bash tools/<capture script>.sh after
```

CTest offscreen cases use software Mesa. Private-compositor frame pacing of
about 26 ms is **not** a 60 FPS claim.

## Audit workflow and evidence (how every repair was done)

`docs/VISUAL_AUDIT.md` is the authoritative ranked list and repair ledger: 123
findings (ENV-001 to ENV-123), **all addressed with evidence** as of this
handoff. Each evidence section names a capture prefix, the inspected matched
before/after frames, the tests and a 600-frame stationary performance sample.
Do not reopen closed IDs; a new visual pass starts from new screenshots and a
new ranked list.

The repeatable method, to reuse for any future repair:

1. Write or reuse a `tools/capture_<topic>.sh` script (pattern: `before|after`
   stage, optional prefix, fixed `--audit-camera` poses, a 600-frame
   `--report-state` performance run, a binary SHA-256 receipt; refuses to
   overwrite a preserved `before`).
2. Capture `before` with the current binary and inspect the real images.
3. Implement in a focused header under `include/trainsim/render/` and wire it
   into `src/render/SceneRenderer.cpp`; add unit tests in
   `tests/MeshBuilderTests.cpp`; expose a `--report-state` counter when a smoke
   test can use it.
4. Build, run both unit test binaries, capture `after`, inspect, compare
   performance, run the whole CTest suite on the final binary, record
   everything in `docs/VISUAL_AUDIT.md` (ranked row + evidence section),
   `docs/ARCHITECTURE.md` and here, then commit and push.
5. If a new pose is needed after the `before` stage, rebuild the old binary with
   `git stash push -- include src tests` (never stash scripts or docs), capture
   the new pose under a new prefix, pop and rebuild.

Audit camera: train-local metres, absolute render Y (seated eye 1.90 m,
standing 2.36 m); yaw 0 looks toward negative Z, +90 toward positive X;
positive pitch looks up; route distance grows toward local positive Z; the
cars are centred at Z = -52.5 + 21·car (car 0..5); the cab bulkheads are at
Z = ±63.2 and the noses at ±72.8. Station command-line indices are one-based.
`--audit-camera` requires `--frames` and `--screenshot` (a performance run must
not use it). Static captures disable avatars/audio and nearly freeze route time.

All QA images and logs are local, ignored files under `build/visual-audit/`;
keep them. A fresh clone can reproduce `after` captures but not the historical
`before` binaries.

## Implementation map of the 2026-10-10 session (ENV-067 to ENV-123)

New focused headers, each with unit tests, each wired into `SceneRenderer.cpp`:

| Header | Covers | Report counters / scripts |
| --- | --- | --- |
| `BridgeGeometry.hpp` | bridge parapets (upstand, coping, cable trough, tubular handrail on 2.5 m posts), viaduct valley profile (`viaductValleyTerrain`, bowl + spurs, seamless abutments), footed/tied piers | `bridge_parapet_posts`; `capture_bridge_detail.sh` (prefixes `bridge-parapet`, `viaduct-valley`) |
| `TunnelGeometry.hpp` | precast lining (textured face in the concrete batch, ring/longitudinal joints, haunch), bracketed luminaires with seamless wash, handrail/cable tray, escape-route and hectometre signs (`Sim::kTunnelExtentsMetres`) | `tunnel_lining_rings`, `tunnel_marker_plates` (required by the tunnel smoke test); `capture_tunnel_detail.sh` (`tunnel-lining`, `tunnel-markers`) |
| `TrackGeometry.hpp` + `makeBallastDetailImage` | angular ballast texture, sloped shoulders to a wobbling toe, chamfered monoblock sleepers inside the fastener radius, foot/web/head rail profile | `capture_track_detail.sh` (`track-surface`) |
| `LinesideGeometry.hpp`, `SignalGeometry.hpp` | `addBarBetween` chord bars (catenary wires now follow curves), wire fences with strained ends, rebuilt colour-light signal (lens centres shared with the live aspect overlay) | `fence_posts`; `capture_lineside_detail.sh` (`lineside`) |
| `InteriorFittingsGeometry.hpp` + `addSeatRearShell` | seat grip recess, literature pocket, pedestals, rack suitcases, cab-door hardware, extinguisher, intercom, seated fasteners, gangway plate, grab-rail caps, ceiling diffusers | `capture_interior_fittings.sh` (`interior-fittings`, `interior-close`), `capture_hvac_detail.sh` (`hvac-grille`, `hvac-close`) |
| `ToiletGeometry.hpp` | toilet, support rail, paper dispenser, coves, vanity + sensor spout, gradient mirror, dispensers, threshold | `capture_toilet_detail.sh` (`toilet-fittings`; its `mirror` pose is invalid, see the audit) |
| `TrainExteriorGeometry.hpp` | lower bodyside with bogie cut-outs, bogies, underframe modules, livery bands, window gaskets, mullion faces, door reveals, roof crown + HVAC + pantographs (cars 2 and 5), inter-car bellows, cab nose/lamps/coupler/visor/gasket/wipers | `capture_train_exterior.sh` (`train-exterior`) |
| `TerminalGeometry.hpp` | buffer stops, train shed, head house with name lettering, forecourt taxi rank; the track batch now overruns both route ends by 125 m (`kTerminalTrackOverrunMetres`) | `terminal_buffer_stops`, `terminal_shed_columns`; `capture_terminal_composition.sh` (`terminal-composition`) |

Also: `Route.hpp` exports `kValleyViaductBegin/EndMetres` and
`kTunnelExtentsMetres`. `SceneRenderer.cpp` has since been split (see below); keep
adding reusable geometry in headers rather than inline.

## Implementation map of the 2026-10-12 session (ENV-124 to ENV-133, PERF-002)

| Area | Where | Counters / tools |
| --- | --- | --- |
| Rural/urban verge zones, cable trough, wildflowers, palisade | `VergeGeometry.hpp`; near-cell grid via `TerrainPatchGeometry.hpp::addGridTerrainCell` | `cable_trough_units`, `verge_flower_clumps`, `palisade_posts` |
| Irregular per-side parcels, boundary types, tramlines | `ProceduralMaterials::ruralParcelBoundary/Index/Arable`, `LandscapeVariation.hpp::fieldBoundaryLayout`, `kTramlineLaterals` | `field_boundary_trees`, `tramline_segments` |
| House styles and outbuildings | `FacadeGeometry.hpp::houseStyle/houseEnvelope/addRuralOutbuilding`, `StreamingWorld.hpp::houseHasOutbuilding` (parcel reservation) | `house_outbuildings` |
| Apartment/tower palettes, balconies, spandrels; lighter sheds | `FacadeGeometry.hpp::blockStyle`, Apartment/Tower/Warehouse cases | `block_balconies` |
| Ground bounce and uplight | `Daylight.hpp`, `OpenGL33FacadeEffect.cpp`, exterior light setup in `SceneRenderer::draw` | — |
| Canopy palettes and rural valance | `StationGeometry.hpp::canopyPalette/addCanopyValance` | `canopy_valance_boards` |
| Antialiased fixed signage | `PassengerDisplayText.hpp::addPrintedPlaqueText`, `interiorLettering_` | — |
| Driving-cab bodyside | `TrainExteriorGeometry.hpp::addCabBodyside` | — |
| Buffer segments and sliced staging | `MeshBuilder::meshSegments`, `GpuMesh` segments / `stageSegment`, `stagedUploadBytesPerFrame` | `world_gpu_mb`, `world_largest_material*`, `world_gpu_segments` |
| Track batch on a worker | `WorldGeometry.hpp::TrackGeometry`, `SceneRenderer::updateTrack` | `track_*` |

Capture scripts: `tools/capture_visual_audit_pass2.sh` (59-pose baseline, prefix `audit2`),
`tools/capture_audit2_repairs.sh <stage>` (matched repair poses plus 600-frame stationary and
1,800-frame 8x ride; `before` is preserved), `tools/test_streaming_risks.py`. Evidence is in
`build/visual-audit/audit2-*`, `audit2-repairs-*`, `streaming-risk-*` and the comparison sheets
in `build/visual-audit/audit2-sheets/`.

## Goal list of 2026-10-12 evening: done, committed as `de92449`

Everything below builds without project warnings (the only warning is sharp-runtime's own
`__int128` pedantic note) and passes every test. Evidence and limits are
in `docs/VISUAL_AUDIT_2.md` ("Owner goal list") and `docs/STREAMING.md` (PERF-003).

| Goal item | Where | Evidence |
| --- | --- | --- |
| Split `SceneRenderer` | `SceneRenderer.cpp` ~1,900 lines; `SceneShared.hpp`, `SceneAvatars.cpp`, `SceneStreaming.cpp`, `WorldGeometryBuild.cpp`, `TrackGeometryBuild.cpp`, `TrainInteriorBuild.cpp` (see `docs/ARCHITECTURE.md`) | `build/SceneRenderer.cpp.presplit` kept as backup; `build/visual-audit/split-check.sh` |
| Faster world preparation (PERF-003) | `WorldGeometry::build` on up to 4 threads, bit-identical to 1 thread | `trainsim_world_benchmark`; table in `docs/STREAMING.md` |
| Building floor plans (ENV-134) | `HousePlan`/`BlockPlan` in `StreamingWorld.hpp`, `FacadeGeometry.hpp`, `WorldGeometryBuild.cpp` | `tools/capture_plans_platforms.sh` (`plans-*`) |
| Busier platforms (ENV-136) | `PlatformLayout.hpp`, `StationGeometry.hpp`, `SceneAvatars.cpp` | `capture_plans_platforms.sh` (`platform-station-*`). This session moved the vending machine from +12.6 to +25.4 m: it hid the departure board at major stations and termini. |
| Oncoming train types (ENV-137) | `world/OtherTrainConsist.hpp`, `render/OtherTrainGeometry.hpp`, `StreamingWorld::otherTrains()` | `tools/capture_oncoming_trains.sh` (`oncoming-*`); smokes `trainsim_freight_pass_view_smoke`, `trainsim_traffic_window_view_smoke` |
| Time of day (ENV-138) | `sim/Environment.hpp/.cpp`, `render/Atmosphere.hpp`, `NightLighting.hpp`, `SkyGeometry.hpp::addAtmosphereSky`, `WorldMaterial::NightGlow` | `tools/capture_time_weather.sh` (`time-weather-*`); smoke `trainsim_night_environment_smoke` |
| Weather (ENV-139) | same, plus `WeatherGeometry.hpp`, rain loop in `TrainAudio.cpp`, `assets/audio/rain.wav` (generated by `tools/generate_audio.py`) | `capture_time_weather.sh`; smoke `trainsim_rain_environment_smoke` |

Key facts for the next agent:

- Oncoming types: phase in variant bits 0-1, type above (`otherTrainVariant`), rotation by service
  ordinal (`index/37`): freight, regional, intercity, diesel. Ordinals 1 and 29 stay regional, so
  the old pass-by and AI-crossing smokes are unchanged. Speeds 31/38/25/28 m/s. Crossings close
  for the nearest vehicle. Repaired pre-existing defect: services used to exist only while their
  anchor chunk was loaded and could pop in beside the player; `otherTrains()` spans eight chunks.
- Time of day: the station clock start is `service_start_minutes` (default 557 = 09:17) or
  `--time HH:MM`; weather is `weather` 0..5 or `--weather NAME`; both also in the pause menu
  (items 11-12) and persisted like other settings (CLI values are not saved). The default 09:17
  clear start is bit-identical to the old fixed daylight (unit test, and zero-pixel differences on
  static recaptures), so earlier evidence remains valid.
- Shadows are baked per streamed window from `worldSun_` (whole-degree steps, at least 25
  degrees) and fade at draw time; the sun is not part of the prefetch validity key.
- Repaired after the first captures: unlit leaf sprays stayed bright at night (now tinted), window
  beads looked like large white triangles (smaller/fainter), rain fell under platform canopies (the
  platform half of the rain field is skipped beside a station).
- Known limits: no wet surfaces/puddles/snow/lightning; the player train's exterior keeps saloon
  lighting at night (audit cameras only); an oncoming service still appears/vanishes at the ends
  of its 1.9 km traversal.

## cna-engine adoption (2026-10-21, branch `engine-adoption`)

The game uses [cna-engine](https://github.com/libcna/cna-engine)'s strict modules (XNA 4.0 types
only) from a third sibling checkout, `../cna-engine` (`-DTRAINSIM_CNA_ENGINE_ROOT=`); they need CNA
built statically, which the build forces. One commit per step, each checked against the step
before it:

| Step | Moved onto the engine | Stays the game's |
| --- | --- | --- |
| lockstep | — (`--lockstep`: one 1/60 s step per drawn frame, so captures repeat) | — |
| build | `CnaEngine::Graphics` linked; CNA static | the install verifier now rejects a shared `libcna` |
| screenshots | `CaptureBackBuffer` (reads bytes, writes the PNG) | — |
| camera | `Camera::SetLookAt`/`SetPerspective` for the eye and the audit pose | the eye's sway, lean and seat blend |
| meshes | each `GpuMesh` segment is drawn as a borrowed engine `GpuMesh` | streaming, staging, buffer reuse |
| system keys | `ActionMap` (start, quit, pause, map, HUD, help, diagnostics, freeze, teleports, menu arrows, use) | walking and mouse look |
| scene | the world, the station clocks and the train are queued in the engine's `Sequenced` bucket and drawn by its `ForwardRenderer`; `SceneRenderer::Binder` configures the scene's effects (`docs/ARCHITECTURE.md`, "Drawing through cna-engine") | the sky, sun, moon and clouds, the people, the HUD; all effects and shaders |

`FrameTiming` stays the game's: its report needs the lifetime maximum and exact window means,
which cna-engine's `FrameStatistics` (a running sum in seconds) does not give.

Verification, against CNA `900d0db3b`, sharp-runtime `f2aecfef` and easy-gl `v0.1.1` (system SDL
from the CNA checkout), Release, three jobs: 56 lockstep captures (the 37 visual-audit views, night,
dusk, rain, fog, overcast, quality 0 and 1, both facade shaders, avatars, the HUD overlays). Two
runs of the unchanged game differed in 30 views before `--lockstep` and in the two onboard avatar
views after it (avatars load asynchronously while their animation advances per drawn frame); the
other 54 are pixel-identical after every step under Xvfb (llvmpipe) and, comparing the first and
the last step, on the Radeon 780M (`SDL_VIDEODRIVER=offscreen` without a display). Every
`--report-state` counter is unchanged; uncapped GPU frame times stayed within the noise (about
5 ms). Real X11 key presses (start menu, M, F1, Esc, the pause-menu arrows, E, F12) behave as
before. Both unit binaries pass (134 and 200). CTest: 73 of 75; `trainsim_world_prefetch_-1`
fails the same way before the first step (two synchronous world builds where one is expected) and
`trainsim_install_layout` cannot pass with the system SDL used here (it installs the project-local
SDL build). Not checked: the WebGL 2 build, which now also compiles the engine.

The first interactive run on the owner's desktop then stopped with "Every avatar bone transform
must be decomposable": since CNA's 2026-10-19 change to XNA's exact `Matrix::Decompose`
(`705ea2324`, part of the CNA these builds use and of `next`), a sheared bone is refused, and the
sit/stand transition blended the shoulder, elbow and wrist with `Matrix::Lerp`, which shears
mid-way. The unmodified `develop` crashes the same way (`--run-seconds 120 --route-km 52 --car 3
--time-scale 20`, at about 108 s). `blendBone` (`AvatarProportions.hpp`) now blends the rotations
as quaternions and the translations linearly; poses outside a transition are unchanged, the 54
views stay identical, three 180-second avatar runs at stations and on the line complete, and a
render test checks that blended bones decompose.

### Shared facilities (2026-10-21, branch `engine-shared`, cna-engine SHARE-001)

Three more steps moved code the other simulators also wrote into cna-engine:

| Step | Moved onto the engine | Stays the game's |
| --- | --- | --- |
| procedural textures | `Graphics::Image`: `ProceduralImage::downsampled`, `downsampledForAlphaTest` and `uploadMipmappedTexture` delegate to `Downsample`, `DownsampleForAlphaTest` and `UploadTexture` (the same truncating box filter and coverage bisection) | the texture generators, validation and messages |
| preferences | `SettingsFile` reads and writes `train-simulator.cfg`; `UserConfigPath` finds the user file (now `%APPDATA%` on Windows) | strictness on top: a line without `=`, an unknown key or a non-finite value is still an error naming its line; of a key given twice only the last value is checked now |
| automated runs | `AutomatedRunOptions`/`ParseAutomatedRunOption` (`--frames`, `--run-seconds`, `--screenshot`, `--lockstep`; a negative or non-finite value is refused) and `AutomatedRun` (simulated time, the frozen simulation at the limit, drawing on until the avatars are loaded, the screenshot read after Present) | the step length (`stepSeconds`, capped at 0.1 s), the elapsed service time, the start menu |

`src/config/Settings.cpp` moved out of `trainsim_core` into a game-only `trainsim_config`, so the
`TRAINSIM_BUILD_GAME=OFF` core stays CNA-free; `SettingsTests` run in `trainsim_render_tests`.
cna-engine's image and settings probes compare the engine with frozen copies of the replaced code
(`tests/fixtures/simulator_oracles`).

Verification (Xvfb, llvmpipe, the 56 lockstep views as above): 54 pixel-identical to the views
before the first step, the two onboard avatar views differing as they did between runs before;
every `--report-state` counter equal except `terminal-platform`'s route-point cache counts, which
equal the original game's GPU run and so vary between runs of the original too. Unit binaries 126
and 209 (the eight settings tests moved); CTest 73 of 75 with the same two failures as above.

## Known open issues (outside the ranked list)

1. **Streaming follow-up (updated by PERF-002, 2026-10-12).** World materials are now split
   below CNA's 64 MiB buffer limit (Northbridge previously exceeded it after the pass-2
   additions and aborted), staged in 3 MiB slices with an urgency-raised 4 ms budget, and the
   track batch is prepared on a worker. Measured remaining limits: at 24x journey speed
   generation is the bottleneck (3 late world, 1 late track preparation); synchronous fallbacks
   still cost hundreds of ms; one unattributed transient (p99 37 ms) on the first run after the
   compositor started. Details in `docs/STREAMING.md`. Earlier text follows.
   CPU-only `WorldGeometry` preparation now runs on one
   worker, with a route copy and a separate deterministic world snapshot. The
   render thread stages at most one non-empty material per frame into a second
   GPU window; meshes, counters and anchor swap together at the boundary. Empty
   slots are skipped immediately (new software-rendering smokes caught wasted
   frames here). Startup, teleport, settings changes and late predictions use a
   complete-scene synchronous fallback. See `docs/STREAMING.md` (PERF-001) for
   evidence, telemetry semantics and reproduction. Final moving
   evidence reduced p99 from 173.4 to 26.9 ms with 45 ready swaps and no late
   predictions; the avatar-enabled ride measured p99 27.1 ms with 18 ready
   visuals and the same 45 ready swaps. Final verification is recorded below.
   Remaining risks are large
   individual uploads, separate track rebuilds, heavy-load missed deadlines and
   the extra GPU window's memory footprint; do not claim every hitch is gone.
2. **Intermittent physical-mouse camera freeze** reported by the owner; not
   reproduced by the 59-minute, 180-cycle private native X11 input test. Six-car
   walking and seating/display/toilet checks exposed and repaired crowd-yield
   deadlocks and an NPC reservation race during player toilet entry. See
   `docs/STABILITY.md` for exact component results and harness limits. Injected
   events cannot certify the owner's physical mouse; do not claim its issue fixed.
3. **Multi-hour resource validation completed.** The uninterrupted 2.5-hour
   populated journey passed with 24 station legs, both terminal reversals,
   2,470 prepared swaps and zero late preparations. Process RSS showed no
   sustained growth; its final-quarter median was 933.8 MiB and kernel peak
   1,210.3 MiB. Threads, file descriptors and populations stayed bounded. See
   `docs/STABILITY.md`; this finite result is not a proof of leak freedom or a
   separate GPU-memory measurement.
4. Several frame-time comparisons on 2026-10-10 were distorted by host load
   from other agents (documented per section). Re-measure on a quiet host before
   treating any p95/p99 shift as a regression.
5. Remaining visual gaps after pass 2: avatar style is the immutable CNA catalog's;
   houses and blocks now have four plans each (ENV-134) but still one window module; the verge is vertex colour on
   a neutral detail texture; flower heads are small at seated distance. A further pass needs new
   screenshots and a new ranked list.

## Current verification (2026-10-12 late evening, commit `de92449`)

Binary SHA-256 `bd0855ca8d65ed9ad79b4b32fef6df372ea86a5065e13143e739ae98c1714c40`: render/geometry
**200/200**, simulation **134/134**, full CTest **75/75** in 148.7 s
(`build/visual-audit/env-ctest-final.log`). The captures were taken with the immediately preceding
binaries of this session (the last relink only picked up a dependency library rebuilt by another
agent and the vending-machine move, which `plans-platforms-*` recaptured). Moving 8x rides from
km 52 (`build/visual-audit/env-ride-*.log`): default p95 25.6 / p99 25.9 ms, night 25.7 / 29.5,
rain 25.7 / 26.2, changing 25.7 / 26.1, each with 45 prepared swaps and no late preparation; peak
RSS about 1.0 GiB. The world benchmark this session ran at host load ~26 and is not comparable.
Dependencies were only read and built from.

Contact sheets: build them as JPEG (`montage ... -depth 8 -quality 85 x.jpg`). The default
16-bit PNG sheets reach 5–6.5 MB and reading them ended three agent sessions on 2026-10-12.
Sheets of this session: `build/visual-audit/audit2-sheets/oncoming-*.jpg`, `time-weather-*.jpg`,
`tw2.jpg`, `tw3.jpg`, `plans-platforms-*.jpg`.

## Previous verification (2026-10-12, commit `ecd835e`)

Final binary SHA-256 `3e89842194b2ee994aabca6e64545cc164a51a891a30ceebc8fc36a122648f0e`:
render/geometry **193/193**, simulation **128/128**, full CTest **69/69** in 149.9 s
(`build/visual-audit/audit2-ctest-final.log`). The matched 8x ride improved from p99 27.3 /
max 55.9 ms to 26.8 / 27.8 ms with 45 prepared swaps and no late preparation; the stationary
sample stayed at the compositor's ~26 ms cadence (p99 26.1 → 26.6 ms) with 7% more triangles.
All six PERF-002 scenarios exited 0 (see `docs/STREAMING.md`). Dependencies were not modified:
CNA (`2a5100d72` with other agents' uncommitted edits in its worktree) and sharp-runtime
(`c20373ba`) were only read and built from.

## Earlier continuation verification (2026-10-11)

The original 2026-10-10 checkpoint was commit `41cbc1d`, 67/67 CTests,
176 render tests and 124 simulation tests. The resumed checkout was clean on
`develop`, pointing to the same commit as `main`; the continuation stays on
`develop` rather than moving the owner's branch.

The streaming continuation adds `WorldGeometry.hpp`, preparation telemetry,
full vertex/index/counter equivalence tests, two directional prefetch smokes,
`tools/capture_streaming.sh` and `tools/test_world_prefetch_input.sh`. Its exact
results are in `docs/STREAMING.md` and `build/visual-audit/streaming-*`.

Final streaming build: **69/69 CTests passed, 339.49 s**; render/geometry
**178/178**, simulation **124/124**. The private settings/teleport input probe
passed with 12 synchronous loads and no late preparation. Matched static captures,
1,800-frame no-avatar and 1,800-frame populated GPU rides were inspected. Peak RSS
was about 844 MiB without avatars and 996 MiB with avatars in those short runs;
these are not long-term leak results. CNA `2b15506a4` and sharp-runtime `c20373ba`
were clean and untouched by this project.

The streaming group is committed and pushed as `9ccf390` to `origin/develop`;
the owner explicitly approved that commit and destination. The input repairs and
resource-test helpers are committed and pushed as `fdb2941`, also with the owner's
explicit approval for that commit. Input work now has 128/128 passing
simulation tests and 178/178 render tests. The final full CTest run passed 68/69;
the gallery validator timed out at 15 s, then passed alone in 4.17 s. Both receipts
are retained; do not describe that as a single clean full-suite pass.
The 20-second stability-helper smoke and the full 2.5-hour resource journey
passed. The latter completed normally in 9,001.817 wall seconds, preserving 901
resource samples, 1,796 application traces and an inspected final screenshot.
Its exact resource analysis and limitations are in `docs/STABILITY.md`. The
tested binary SHA-256 is
`071285233baafd7dbeada5f34c5e01d057323c15074172afad55a7c819bc4457`.
Current tools:
`tools/test_native_walk.py` (zero rounds isolates interactions),
`tools/run_stability_soak.py` and the extended `tools/test_x11_input.sh`.
The first `stability-long-*` attempt was cut off by a host restart at about
50 minutes, with no final result. Preserve it as incomplete evidence. The
replacement `stability-restart-*` run completed the full 9,000 seconds and its
summary reports success. No test process needs to be resumed or restarted.
That 2026-10-11 scope was complete; the 2026-10-12 session above followed an explicit
new owner request. Do not start further work without the owner's instruction. Dependencies
remain read-only. Verify commits and worktree state with `git log -1` / `git status -sb`;
do not rely on this document alone.
