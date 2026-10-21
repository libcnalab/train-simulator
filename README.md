# Train Simulator

Train Simulator is a C++23 first-person passenger experience built with CNA. It follows the
fictional **Northern Valley Express** over a deterministic 286.4 km railway while the player
walks through a six-car electric train, uses a passenger toilet, takes a seat, watches the world
through the windows, and shares the service with an active avatar population and conductor.

This repository is an early but playable showcase foundation, not a finished commercial game.
Its simulation, interaction, streaming, UI, audio, and procedural rendering systems are integrated
and tested. Visual realism is under active development: materials, lighting, station composition
and scenery are improving, but repetitive geometry and stylized avatars remain substantial gaps.
See the evidence and open findings in [the realism audit](docs/AUDIT.md); passing automated tests
does not certify visual realism or resolve the reported intermittent physical-mouse freeze.

The ranked screenshot campaign is tracked in [the visual audit](docs/VISUAL_AUDIT.md); all 123
listed findings have before/after engine evidence as of 2026-10-10. A second screenshot pass
([VISUAL_AUDIT_2.md](docs/VISUAL_AUDIT_2.md), ENV-124 to ENV-133) addressed the lineside verge,
repeated houses and blocks, field boundaries, station canopies, interior signage and the cab
sides on 2026-10-12. That closes the listed defects, not visual realism in general. The owner's
follow-up goal list (ENV-134 to ENV-139, PERF-003) added building floor plans, busier platforms,
four oncoming train types, time of day and weather, a faster parallel world build, and split
`SceneRenderer` into focused translation units.
See [the development handoff](handoff.md) for verified progress, remaining work and build constraints.

## Screenshots

The committed captures use the real first-person camera and current High quality profile. At the
requested frame the application freezes simulation, completes the normal serialized CNA-avatar
warm-up, and only then reads the framebuffer, so the simulated saloon and platform populations
cannot be hidden by asynchronous startup.

### Passenger saloon

![Populated passenger saloon with a clear six-car aisle, information displays and ceiling ventilation](docs/screenshots/passenger-saloon.png)

### Live passenger information

![Interactive onboard passenger information with the complete route, train position, speed and following calls](docs/screenshots/passenger-information.png)

### Lakehurst approach

![Seated window view across Lakehurst](docs/screenshots/lakehurst-window.png)

### Stonebridge station stop

![Live Stonebridge platform board and CNA station passenger seen from a real window seat](docs/screenshots/stonebridge-platform.png)

### Live route map

![Non-pausing route map at Stonebridge](docs/screenshots/route-map.png)

Regenerate all five 1280x720 images from the current executable without manual input with:

```bash
./tools/capture_showcase.sh
```

The script uses throwaway default preference files, so an earlier local FOV or comfort-menu change
cannot alter the committed framing. Each capture is decoded and checked for complete HUD glyph
coverage before replacing the existing PNG; an incomplete CNA offscreen readback or failed process
is retried up to four times.

Visible-window play is the authoritative visual path. See the known offscreen readback limitation
below before using long accelerated captures for pixel comparisons. The screenshots are project
output rather than external art assets and may be refreshed as the procedural art direction evolves.

## Current experience

- Six connected passenger cars with sensor-operated glazed gangway doors, sealed glazing,
  illuminated request sensors, ribbed metal kick plates, layered vestibule end-wall panels, flexible
  bellows, lit inter-car thresholds and anti-slip bridge plates, individually modelled
  seats with contrasting upholstery seams and piping, passenger-side headrest covers, rear shells,
  grab recesses, literature pockets and
  aisle armrests, physical facing-seat tables with cup recesses and
  power outlets, a woven-edge and transversely bound continuous aisle runner, luggage areas, racks,
  grab poles,
  suspended grab handles, ceiling PA speaker grilles, paired climate-control diffusers and
  individually colored luggage with a visibly busier dedicated luggage car,
  updating double-sided information displays with station, destination, speed, ETA, following
  stops across rotating pages, and platform-side door guidance; an interactable full passenger
  information view adds a live twelve-station route line, train position and direction, plus illuminated passenger door buttons,
  physical quiet, work/table, accessible, family, luggage, and standard-car identity signs with
  restrained matching upholstery accents plus close-role details such as books, laptops,
  assistance controls, play blocks, route literature and a full-height luggage stack;
  low-poly rounded cushion, back, armrest, and table edges plus slim seat pedestals with visible
  floor anchors and separately plated table stems soften the
  foreground silhouette, while wall-mounted
  extinguishers, emergency intercoms and
  illuminated exit pictograms in every vestibule, rounded wheeled luggage with pull handles,
  inaccessible double-ended cab shells with direction-aware live controls, labelled
  staff-only bulkheads with observation windows and contextual feedback, a visible active-cab
  driver, real passenger-door openings with glazed sliding panels, jambs and highlighted thresholds,
  and a signed accessible toilet with toilet, sink/faucet, visible mirror approximation,
  support rail, emergency pull cord, paper and soap dispensers, anti-slip floor, local lighting,
  ventilation detail, and a physical available/occupied indicator shared by player and NPC use.
- Stable train-relative first-person walking, collision, context interaction, smooth seat-camera
  transition, level platform boarding geometry with a 20 cm horizontal gap, coping stones,
  safety lines, segmented tactile paving and correct-side retractable bridge plates,
  window-bay-aligned seats with unobstructed views,
  low-opacity tinted glazing with subtle quality-scaled view-angle reflection accents,
  quality-scaled ceiling and upper-wall reflected-light gradients,
  modular ceiling seams, continuous wall coving/skirting, and restrained vestibule traffic wear,
  visually segmented resilient vestibule flooring aligned with the surface-aware footstep zones,
  and
  comfort-scaled rail vibration,
  suspension sway, acceleration/braking pitch and curve lean.
- Continuous relative-mouse look consumes CNA's per-frame deltas directly instead of
  differencing them as positions. Project capture, CNA relative mode, and desktop focus are
  reconciled every frame; focus loss or stale mode explicitly releases the cursor, while map,
  help, display, and pause overlays release it intentionally. One click safely reacquires it.
- 34 onboard CNA avatar passengers, a bounded two-identity station waiting pool, a conductor and
  driver, with authoritative seat ownership,
  smoothly blended custom seated bone poses, distance-driven procedural walking gait with ankle
  and toe follow-through over varied upper-body idles, seat-anchored stand-up transitions with
  aisle-facing weight transfer,
  paired conversational gestures with partner-facing head motion, slow standing/seated gaze,
  short aisle-side conversations with reciprocal seated/standing eye contact and restrained
  single-arm gestures,
  window-view scanning, nearby player acknowledgement with restrained torso follow-through,
  deterministic two-handed reading idles, and visible screened-handset checking poses, lightweight
  walking/idle/window behaviors, exclusive
  enter/use/leave toilet visits, approach-time walks from real seats to the vestibule, visible
  platform-side boarding and alighting, and contextual
  service information plus visible passenger-area inspection stops from the conductor.
  The conductor carries a dedicated screened handheld terminal and uses a two-arm inspection pose.
- A 12-station bidirectional service with smooth acceleration, cruising, braking, correct-side
  platform state, 120/140/160 km/h segment limits, an authored 135-minute timetable with live
  early/late prediction, animated
  platform-side passenger doors, a shared animation/AI passage interlock that keeps boarding
  actors behind the leaves until the opening is clear, terminal reversal, and indefinite
  operation.
- A deterministic, bounded 250 m chunk streamer with broad relief terrain, countryside, urban
  density with footprint-aware railway setbacks, broad city approaches, endpoint terrain aprons
  and terminal skylines, hazy distant ridges, seeded broad field parcels with segmented crop rows and low
  terrain-following hedges, occasional strapped straw-bale stacks, terrain-bound grass clumps,
  bushes, forked tapered trunks and distance-bounded faceted broadleaf/conifer crowns with
  stepped branch silhouettes, deterministic palette variation, terrain-following understorey and
  mixed-age sapling clusters on Medium/High,
  distant woodland belts, shallow terrain-cut lineside drainage with quality-scaled outer-bank
  grass outside both fences, coherent 500 m field-tint parcels with segmented terrain-following
  hedges, desaturated far terrain, a quality-scaled second woodland line and muted distant
  settlement silhouettes, sparse fenced farm
  compounds with barns and silos, distance-scaled window detail on track- and route-facing
  building elevations including both end faces of stepped distant towers, and batched
  vegetation contact shadows,
  detailed modular houses, bounded city streetfronts, apartments, loading-dock warehouses with
  slatted shutters, docks, clerestories, cladding ribs, rooflights, rear HVAC plant, downpipes and
  roof extractors, plus bounded shared industrial yards with gates, parking markings, pallet
  stacks, planted fence-edge buffers, parked delivery trucks, an occasional service van and
  detached tandem-axle box trailer,
  sparse stepped skyline towers behind the densest city approaches, two-sided paved urban streets
  with footways, lighting, continuous road-edge markings and slotted kerb drainage; urban trees
  and shrubs remain behind the outer footway, while houses and blocks sit
  on bounded planted or paved plots with real thresholds and short entrance paths,
  double track, fencing, protected level crossings with distance-driven striped barriers and
  alternating red road-warning lamps that react to both the player's service and visible AI
  trains, plus a quiet matching spatial warning bell,
  major-station sidings and turnouts with concrete-mounted point motors, drive rods and route
  indicators, correctly offset catenary masts with contact wire, sagged
  messenger, variable 25 m droppers, insulators and staggered registration arms, signals,
  working-limit speed boards,
  passenger-visible numbered kilometre posts, deterministic signalling/telecom cabinets with
  louvres, warning plates and cable markers,
  sparse high-voltage corridors and trackside electrical substations,
  maintained direction-facing three-lens block signals with live red/yellow/green aspects,
  named stations, curved tunnels with low-cost wall/ceiling luminaires, reflective guidance,
  raised evacuation walks, structural joints, and signed emergency niches,
  a level streamed Lakehurst shoreline with a carved basin, mipmapped directional-wave material,
  shore-to-depth colour, reeds, surface glints and a
  quality-scaled layered woodland silhouette on the far bank,
  a level reflective river/estuary crossing with dark steel fascia, concrete abutments and broad
  blade piers, and a
  separately modelled wooded dry-valley viaduct with concrete parapets and paired slender shafts;
  both use curve-following 20 m deck sections, underside girders, cross-members, terrain-reaching
  supports, and open upper railings that preserve passenger views,
  sparse road overpasses with terrain-reaching ramps, supports and guardrails,
  lightweight bidirectional road traffic with rounded low-poly bodies, sloped glazing, mirrors,
  wheels, hub cues and directional lights,
  four types of oncoming train (ENV-137) and short parked regional or diesel units at
  major stations: the three-car red/cream regional unit, a locomotive-hauled intercity (blue
  electric locomotive and five coaches), a 200 m freight (green and yellow locomotive with
  container flats, tank wagons, covered hoppers and loaded open wagons) and a two-car diesel unit.
  Each type has its own speed, length and passing sound; vehicles carry bogies, buffers,
  pantographs (raised trailing, lowered leading on locomotives), raked cabs with direction-correct
  white or red lamps and, at night, lit saloon windows.
  Moving consists complete a continuous 1.9 km pass before an out-of-view reset, and both their
  spatial audio and geometry follow the same widened parallel alignment through island stations.
  Static scenery and the 2 km track batch are prepared on CPU workers ahead of each transition,
  split into buffer segments below the graphics-profile limit, staged in small per-frame slices
  and published as complete windows. Initial loads and discontinuous changes retain a
  synchronous fallback; see [streaming evidence](docs/STREAMING.md) (PERF-001 and PERF-002).
- A deterministic mipmapped material palette adds fine non-repeating tonal structure to terrain,
  track ballast, polished rail heads, station concrete, lake/estuary water, carriage carpet, seat
  upholstery, resilient vestibule/WC flooring, and close passenger-wall and ceiling panels.
  Medium/High select per-pixel material lighting with restrained surface-specific highlights and
  wrapped filtering (anisotropic on High), restore the neutral sampler after every material pass,
  and add batched soft contact beneath buildings, canopies, vegetation, and seat groups; Low keeps
  the inexpensive path. The runtime reports each material batch independently for visual and
  performance regression. Rails separately model their oxidized foot/web and wheel-polished head;
  dark baseplates and paired elastic clips connect them to deterministically varied concrete
  sleepers, while catenary masts have concrete foundations. Track-facing houses and apartments use
  a shared rendered-plaster batch, townhouses use course masonry, and station buildings combine a
  masonry plinth with rendered upper walls rather than perfectly flat facade colour. Nearby
  passenger-facing windows add projecting pale frames, inset glazing, sills, and quality-scaled
  mullions while distant openings collapse to one bounded pane.
- A small culled CNA-avatar platform population at each stop, including a high-visibility station
  employee, uneven paired travel groups with varied waiting orientation, restrained major-station
  conversation gestures, a wrist-attached handset, and window-readable wheeled luggage beside
  alternating waiting passengers, makes
  station calls feel occupied without creating a persistent city crowd simulation.
- Time of day and weather (ENV-138/139): the station clocks' service time moves a summer sun
  (sunrise 04:30, sunset 21:30) with golden light, twilight glow, stars and a moon; after dark,
  rooms, street and platform lamps light up, platforms are floodlit and signals glow. Weather is
  clear, cloudy, overcast, rain or fog, or changes gradually on a three-hour itinerary: grey
  skies and diffuse light replace the sun, shadows fade, fog closes in, and rain adds streaks
  around the train, beads on the windows and a rain ambience. The default 09:17 clear start is
  pixel-identical to the earlier fixed daylight.
- Normal-based directional lighting over all project-generated geometry, with smooth tunnel
  darkening, depth-masked window sunlight across the saloon floor, subtle avatar contact shadows,
  unlit information displays, quality-scaled procedurally textured soft fair-weather clouds and
  low horizon banks,
  a soft horizon-to-zenith
  daylight gradient, and a stable sun cue.
- Dynamic passenger information, a continuously moving non-pausing route map with live ETA,
  service state, direction, and explicit passed/train/upcoming legend,
  diagnostics, comfort settings,
  speed- and acceleration-reactive rail/traction/braking audio, surface-aware carpet and
  vestibule/gangway footfalls, a quiet continuously running
  interior ventilation bed that settles beneath open-platform ambience, successful-movement footfalls,
  irregular spatial passenger speech-like
  murmur, spatial passenger, automatic gangway-door and accessible-toilet-door sounds,
  synchronized spatial whoosh from
  passing trains, and
  offline-generated English announcements preceded by a project-synthesized onboard PA chime,
  with synchronized optional captions and smooth automatic background ducking.

## Controls

| Input | Action |
|---|---|
| Left click | Capture mouse |
| WASD | Walk |
| Mouse | Look |
| E | Interact, sit, stand, open toilet door, inspect display |
| M | Toggle route map without pausing |
| H | Toggle the compact passenger HUD for an unobstructed window view |
| F1 | Toggle controls |
| F3 | Toggle developer diagnostics |
| F5 | Freeze or resume train simulation |
| F6 / F7 | Developer teleport to previous or next station |
| Esc | Pause simulation and audio, release mouse, and open the comfort menu |
| F12 | Quit |

There is deliberately no jump action and no way for the player to leave the train or enter a cab.
On a normal desktop launch, a compact prompt remains visible until the first left click captures
the mouse; `Esc` releases it again.
Developer station teleports may be used even during boarding: any active doorway flow is settled
safely and the newly selected station starts a fresh exchange on its own platform side.

## Requirements

- Linux desktop with an OpenGL 3.3-capable driver
- CMake 3.23 or newer and Ninja
- A C++23 compiler
- system `ccache`
- Python 3 when building the test suite or regenerating audio assets
- sibling checkouts of CNA `next`, sharp-runtime `next` and
  [cna-engine](https://github.com/libcna/cna-engine) `develop`

The expected checkout layout is:

```text
parent/
  cna/
  cna-engine/
  sharp-runtime/
  train-simulator/
```

Other locations are passed as `-DTRAINSIM_CNA_ROOT=`, `-DTRAINSIM_SHARP_RUNTIME_ROOT=` and
`-DTRAINSIM_CNA_ENGINE_ROOT=`. The game uses cna-engine's strict modules (XNA 4.0 types only), which
need CNA built as a static library; the build forces `CNA_SHARED_LIBRARY=OFF`.

The native build uses CNA's OpenGL 3.3 renderer. An Emscripten build uses CNA's WebGL 2 renderer;
mobile, macOS-specific work, Vulkan, DirectX, and other renderers are out of scope.

## Build and run

```bash
export CCACHE_DIR=/rv/cnaccache CCACHE_BASEDIR=/rv
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_C_COMPILER_LAUNCHER=ccache \
  -DCMAKE_CXX_COMPILER_LAUNCHER=ccache
cmake --build build --parallel 4
env LP_NUM_THREADS=4 ctest --test-dir build --output-on-failure --parallel 1
./build/train-simulator
```

The system `ccache` executable is mandatory and CMake rejects a different compiler launcher;
use the shared `CCACHE_DIR=/rv/cnaccache` with `CCACHE_BASEDIR=/rv`. Never raise build parallelism above four. The
repository uses one incremental `build/` tree and copies its small runtime asset set beside the
executable.

### WebGL 2 build

A reproducible Emscripten build is provided for modern browsers. It bundles the runtime assets into
the generated data file and emits `train-simulator.html`, `.js`, `.wasm`, and `.data` in the shared
`build-probe/` tree:

```bash
./tools/build_web.sh
python3 -m http.server 8000 --directory build-probe
```

Open `http://localhost:8000/train-simulator.html`. Do not open the HTML directly from disk: the
browser must fetch the WebAssembly and data files over HTTP. The default web artifact is
single-threaded, so it does not require cross-origin isolation headers. Its canvas scales to the
browser while the renderer retains the game's 16:9 backing resolution; use the page's Fullscreen
button for an immersive view. Pointer-lock look applies a browser-specific sensitivity correction;
the pause-menu mouse sensitivity setting remains available for personal adjustment.

To create a relocatable Linux desktop tree, install into a chosen prefix. The install includes the
executable, CNA and SDL runtime libraries, assets, project/dependency licenses, and asset/audio
documentation:

```bash
cmake --install build --prefix /path/to/train-simulator-install --strip
/path/to/train-simulator-install/bin/train-simulator
```

Validate the complete installed layout in a temporary prefix, including origin-relative CNA/SDL
linkage, sidecar assets, and an OpenGL 3.3 application smoke launched outside the repository, with:

```bash
./tools/verify_install.sh
```

The verifier removes both of its temporary directories on success or failure. An alternate
incremental build directory may be supplied as its first argument.

For seated-avatar visual inspection, `--passenger-view --car 2` starts in the aisle looking
obliquely toward an occupied seat. Allow approximately 180 frames for avatar loading before a
capture. This inspection view is mutually exclusive with the other `--*-view` presets.

Useful launch options are `--time-scale X`, `--development-dwell`, `--debug-overlay`,
`--help-overlay`, `--map-overlay`, `--inspect-display`, `--pause-overlay`,
`--hide-hud` for an unobstructed initial view,
`--no-audio`, `--frames N` for automated smoke runs, `--run-seconds S` to stop after
S unpaused simulation-update seconds (before journey time scaling),
`--station N` to start at station 1-12, and
`--route-km X` to start a moving QA ride at an exact route kilometre. Pair either location option
with `--direction -1|1` to validate a particular service direction; endpoint safety still selects
the only possible onward direction. `--car N` selects passenger car 1-6. `--quality 0|1|2` applies
the production Low/Medium/High
profile for a reproducible QA run without altering the saved preference. `--window-view` starts
genuinely seated at an available right-side window in that car (or car 4 by default), while
`--left-window-view` uses the equivalent left-side seat,
`--platform-view` instead reserves a real window seat on the current or upcoming train-local
platform side, preferring the clearest central row in the selected car,
`--door-view` starts at that side's passenger threshold and looks down across the deployed bridge
plate for boarding-geometry QA,
`--sink-view` frames the washbasin, tap and mirror inside the closed toilet for visual QA.
`--toilet-view` starts inside the open accessible toilet for visual QA, `--gangway-view` starts
beside an automatic inter-car door, `--vestibule-view` frames the extinguisher and emergency
intercom on the selected car's vestibule end wall, `--sunlight-view` looks down across a complete
window-light patch, `--signal-view` reserves a real window seat and
aims the production camera at a generated facing block signal while holding the service for a
repeatable capture (`F5` resumes it), and
`--screenshot PATH` writes a final-frame PNG. Runtime assets are discovered beside the executable,
in an installed `share/train-simulator/assets` layout, or in the working directory; `--asset-dir
PATH` provides an explicit override for packaging and QA. `--config PATH` selects an explicit user
preference file. `--no-avatars` is a diagnostic/test-only fallback for isolating CNA avatar-loader
lifecycle issues; ordinary play and the showcase captures keep the complete avatar population.

For repeatable environment QA, `--audit-camera X Y Z YAW PITCH` overrides only the
render camera (train-local metres, render-space height and angles in degrees).
It requires `--frames` and `--screenshot`; normal gameplay still cannot leave the
train. See the [123-finding visual audit](docs/VISUAL_AUDIT.md) and
`tools/capture_visual_audit.sh` for the baseline capture poses and repair evidence; the second
pass uses `tools/capture_visual_audit_pass2.sh` and `tools/capture_audit2_repairs.sh`, and
`tools/test_streaming_risks.py` runs the moving streaming scenarios with GPU-memory sampling.

For an intermittent camera/input issue, launch normal play with
`./build/train-simulator --report-state > build/input-session.log 2>&1`.
Without a finite `--frames` limit this flushes an `INPUT_TRACE` every five seconds, including
window focus, native/project mouse capture, all look-blocking overlay states, cumulative
look-motion samples, view angles, player position, drawn-frame count and the actual wall-time
interval since the previous trace (zero for the first record). An interval much longer than five
seconds exposes a stalled update loop instead of silently resembling missing mouse motion.
The trace also records route progress, direction, completed station legs, retained world and
avatar populations, streaming readiness, audio instance counts, recent frame p99 and interaction
state. Frame p99 covers the most recent 2,048 measured frames, not the whole journey.
It does not record typed text. Note approximately when the
problem occurred and retain the log; F12 exits if input still responds. These diagnostics do not
claim to fix the reported intermittent freeze. The optional isolated X11 probe and its limits
are documented in [Architecture](docs/ARCHITECTURE.md#frame-pacing-diagnostics).
For an isolated GPU-backed Xwayland probe, use
`INPUT_PROBE_BACKEND=xwayland INPUT_PROBE_QUALITY=2 INPUT_PROBE_PHASES=12 INPUT_PROBE_AVATARS=1 bash tools/test_x11_input.sh`.
This requires the sibling CNA private Weston runner and does not operate your desktop. Check
`build/x11-input-test/renderer.txt` for actual acceleration; synthetic events still cannot certify
the physical-mouse issue.
Extended native input, six-car interactions and process-memory runs are described in
[Long-running validation](docs/STABILITY.md).

`--no-vsync` is a diagnostic presentation override. It keeps the normal 60 Hz simulation target
and all quality settings, does not alter saved preferences, and may cause tearing. Compare it
with the default synchronized run when investigating frame pacing; `vsync_requested` in the
final report records the requested setting, not proof that the driver honored it.

`--variable-step` is a timing diagnostic that disables CNA's fixed-step wait without changing
the VSync request. Use it with `--no-avatars` for timing comparisons: avatar preset playback still
contains per-draw timing assumptions, and this switch is not a supported gameplay/performance
preset. Without effective VSync it can run uncapped. Normal launches keep the fixed 60 Hz target.
`fixed_time_step` in final telemetry records the selected mode. A matched private GPU comparison
still measured approximately 26 ms with either mode; switching did not fix synchronized pacing.

`--lockstep` makes automated captures repeatable: every drawn frame follows exactly one 1/60 s
simulation step, however long the frame took to draw. A fixed-step run otherwise catches up after
a slow frame (the first one builds the world) with as many steps as the wall clock asks for, so
passengers, rain, water and passing traffic differ between runs. With `--no-avatars` the same
arguments then give the same picture on one machine and renderer; avatars still load
asynchronously, and their animation advances with every frame drawn while they load.

`--report-state` prints final route, direction, phase, speed, simulated service time, leg,
loaded/generated streaming, population, active driver car, seat-walker, player-yield response,
active/started station-exchange, offboard,
draw-call,
triangle, visible-avatar, avatar-rendering-mode, quality-budgeted avatar-pool, and fully ready
avatar-pool counters, including active distance-driven gait and seat-exit layers, for
automated soaks, together with visible signal-aspect,
live platform-board, platform-phone, platform-social-pose, platform-planter, platform-luggage,
canopy-drainage, quality-scaled sky-cloud and understorey, industrial-compound, yard-vehicle,
trailer, warehouse-utility, industrial-edge-planting, urban-road-edge, road-drainage,
residential-plot and entrance-path,
Lakehurst water-patch, station-waterfront, far-bank-tree and rural hay-bale-stack counts, nearby kilometre-post,
catenary-dropper, rendered
external-train car/cab-end, turnout-motor/blade, nearby station-clock-hand, and level-crossing
barrier/lamp/audio counts,
visible animated road-vehicle, tunnel-luminaire, tunnel-safety-niche, river-water-section,
total/river/viaduct bridge-section, and loaded bridge-abutment counts,
wall-clock render FPS, active tunnel/bridge acoustics, occupied-toilet indicator state, and
visible interior-sunlight patches plus passing-train/passenger-door/toilet-door and carpet/resilient
floor footstep audio event counts,
live traction/braking effort,
automatic-announcement chime/event/caption state, successfully started terminal announcements, and
nearby passenger-murmur playback count,
route-map/passenger-HUD/display-inspection/pause state, authoritative longitudinal acceleration, visible suspension
response, deployed boarding-step count, distance advanced since launch, and train-local
player coordinates, view yaw, seat/stand/toilet-door transitions, and relative-mouse-mode
acquisition/loss counts. Run
`./build/train-simulator --help` for the complete syntax.

`--interaction-smoke --no-audio` runs the real CNA application through display inspection,
sitting, standing, toilet use, conductor interaction, a collision-checked walk through all six
cars in both directions, both staff-cab boundaries, and the remain-aboard boundary at a genuinely
open platform-side door. It exits successfully only if every state transition and traversal
agrees. The smoke also verifies that actual displacement raises—and idle clears—the walking-audio
signal, and that carpet and non-carpeted movement choose their distinct sounds. CTest runs it both
silently and through CNA's dummy audio mixer, in addition to the
deterministic cases, a synchronized passing-train audio smoke, a live four-actor station-exchange
smoke, automatic bidirectional welcome and route-aware next-station announcement/caption smokes, a
reverse-direction approach and exchange smoke, a Low-profile station smoke that requires
its eight-visual avatar pool and a post-warm-up PNG save, a complete rendered terminal-reversal
smoke, an accelerated
early-door-close recovery smoke, a live non-pausing route-map smoke, a live moving passenger-
information diagram smoke, a left-platform seated-view
smoke, paired left/right platform-door bridge-plate view smokes, vestibule safety-equipment and
accessible-toilet occupancy, generated-signal and passing-train window-view smokes,
synchronized visual/audio level-crossing
smokes, paired streamed tunnel/bridge geometry-and-acoustics smokes, a right-window urban
road-traffic smoke, a natural-cooldown spatial passenger-murmur
playback smoke that also reaches a terminal and
requires the queued final-stop announcement to start after reversal, byte-exact regeneration of
all 27 synthesized non-speech audio files, a full High-profile
showcase-population warm-up, a pause smoke that requires every live CNA audio instance to be paused
with no route movement, and a separate-cwd smoke that proves the executable uses its copied
sidecar assets instead of the source tree. A serialized install-layout smoke additionally creates
a throwaway relocatable prefix, verifies origin-relative CNA/SDL linkage, and launches that installed
copy from an unrelated directory. A moving-start
interlock smoke also requires all passenger doors and bridge plates to be retracted before the
first rendered frame. A separate accelerated-departure smoke proves that the visual leaves and
bridge plates cannot lag the service interlock into a moving phase, while also exercising the
real door-warning audio transition. Even with development dwell compression, the application
grants the passenger system one fully open update before it permits those doors to close.

## Configuration

Defaults live in `assets/config/train-simulator.cfg`. User changes are stored atomically in
`$XDG_CONFIG_HOME/train-simulator/train-simulator.cfg`, or under `~/.config` when the XDG variable
is unset. An existing `train-simulator.cfg` in the working directory remains a compatible explicit
local override. The user file overlays:

- mouse sensitivity;
- master, announcement, passenger, and train volume;
- camera-motion strength, including zero for no vibration, sway, curve lean, or longitudinal
  suspension response;
- vertical field of view;
- Low/Medium/High graphics quality profile;
- vegetation rendering distance;
- synchronized announcement captions;
- service-day start time (`service_start_minutes`, default 557 = 09:17) and weather
  (`weather`: 0 clear, 1 cloudy, 2 overcast, 3 rain, 4 fog, 5 changing).

While paused, the arrow keys adjust field of view, mouse sensitivity, camera-motion intensity,
graphics quality, master volume, train volume, announcements, passenger voices, vegetation
range, announcement text, the time of day (half-hour steps) and the weather. Changes apply
immediately. `--time HH:MM` and `--weather clear|cloudy|overcast|rain|fog|changing` set the
same values for one launch without saving them, like `--quality`. Quality profiles cap vegetation at 400/700/1000 m, bound
dynamic-object range, render the nearest 6/9/12 onboard avatars, and scale platform population,
contact shadows, and parked trains while leaving all 38 simulated characters unchanged. Medium
and High request desktop MSAA from CNA while Low keeps it disabled; because the back buffer is
created at launch, this part of a changed quality preset takes effect on the next launch. The
hardware-clamped sample count is visible in F3 diagnostics.
Menu changes are saved to the selected user file; it is loaded over the project defaults on the
next launch and can also be edited directly. A command-line `--quality` override remains temporary
when another menu value is saved; choosing a new quality in the menu is an explicit persistent
change.

Unknown keys and invalid or non-finite numeric values fail clearly, and valid values are clamped
to comfortable ranges.

## Project layout

```text
assets/                 Runtime configuration and distributable WAV files
include/trainsim/       Public project headers
src/sim/                Route, service, and passenger simulation
src/world/              Deterministic route streaming
src/render/             CNA meshes, avatars, world, train, and UI support
src/audio/              CNA audio playback and route-aware mix logic
src/app/                Game lifecycle, input, interaction, camera, and HUD
tests/                  Deterministic headless simulation tests
tools/                  Reproducible asset-generation tools
docs/                   Design, route, AI, audio, and attribution notes
```

See the living [realism and production audit](docs/AUDIT.md),
[Architecture](docs/ARCHITECTURE.md), [colour-pipeline calibration](docs/COLOUR_PIPELINE.md),
[Route](docs/ROUTE.md), [NPC AI](docs/NPC_AI.md),
[Audio](docs/AUDIO.md), and [Asset attribution](docs/ASSETS.md).

## Known limitations

- Many walls, buildings, fixtures, and vegetation surfaces still use lit vertex colour while the
  new material palette is expanded; silhouettes and authored material variety remain active work.
- Stations share a coherent modular platform language but use distinct authored families: glazed
  endpoint halls, market-town clock turrets, rural wind shelters, suburban footbridges, waterfront
  promenades, and elevated city concourses. Level platforms include subtly jointed modular paving,
  six restrained material palettes, minor/major two- or three-bench compositions, bounded
  Medium/High surface wear, restrained roof-runoff streaks on station façades and bench contact
  shading, framed canopies with warm underside panels,
  and a clearly readable
  coping/safety/tactile edge plus live service boards with destination,
  platform, boarding state, ETA, and punctuality;
  every platform has a numbered sign, station-varied glazed poster cases and vented service
  cabinets, while major stations add paired planted islands and clocks with readable faces and
  hands.
  Their marked analog faces advance continuously from the simulated service clock, including
  during accelerated development runs and repeated return journeys.
- Time of day and weather are approximations: projected ground shadows are baked per streamed
  window from a sun no lower than 25 degrees and fade instead of lengthening; there are no wet
  surfaces, puddles, snow or lightning; rain streaks are a camera-local field and are hidden only
  on the platform side under canopies; at night the player train's own exterior shell keeps its
  saloon lighting (visible only from audit cameras).
- Full cast shadows, real-time glass reflections, and authored motion-capture locomotion clips are
  not implemented; robust contact/highlight approximations and smoothly blended procedural bone
  layers are used instead.
- Avatar locomotion remains procedural: seated, walking, and seat-exit motion uses project-authored
  bone/placement layers over CNA's stock upper-body idles rather than dedicated motion-capture clips.
- Long accelerated screenshots can intermittently lose 2D spans on CNA's offscreen EasyGL readback;
  gameplay is unaffected and the investigation/workaround is recorded in [bugs.md](bugs.md).

The current supported target is Linux desktop with CNA OpenGL 3.3. Game logic is kept independent
of the renderer so future CNA targets remain possible, but they are not developed or tested here.

## License

Project source and project-created assets are released under the [MIT License](LICENSE). Third-party
generation inputs and their provenance are documented in [docs/ASSETS.md](docs/ASSETS.md).
