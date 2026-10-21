# Architecture

## Water surface rendering

`OpenGL33WaterEffect` confines the supported renderer's analytic ripple/Fresnel
approximation to rendering. Medium/High replace the existing water submission,
not add a pass; Low and shader-invalid fallback use the stock material. No scene
reflection or refraction is claimed. Derivative filtering reduces distant wave
aliasing, and `waterWavePhases` reduces double-precision service time before float
upload. `water_shader_draws` makes the selected path testable. Old detached glint
boxes have been removed from lake and river meshes at all qualities.

`ShoreGeometry.hpp` clips terrain triangles at water-relative material contours
without overlays or topology gaps. Lake terrain uses additional shore samples;
outer edges match coarse neighbours. Reed roots are solved against the rendered
terrain cell, not the analytic height function. `streamedBackdropRange` adds a
500 m background-only terrain/water apron to the first and last detail chunks.
The seven-chunk simulation window is unchanged; buildings, people and other
detail are not generated in this apron. This fixes the lake horizon cut found
during the audit, not the separate synchronous streaming hitch.

River banks reuse the terrain-contour split with independent side phases.
Medium/High reed and shrub groups occupy absolute 23 m slots, with roots solved
against rendered terrain; `river_bank_clumps` exposes this bounded detail. Low
omits those groups. The same existing vegetation/static batches are reused.

`BridgeGeometry.hpp` holds the shared bridge parapet and the viaduct valley
profile. Both authored crossings carry a concrete upstand with an overhanging
coping and drip line, an inboard cable trough and a galvanised two-rail tubular
handrail on 2.5 m posts; `bridge_parapet_posts` exposes the population. Low
quality omits the post base plates and trough joints. The dry-valley terrain is
no longer a fixed depth along the whole span: `viaductValleyTerrain` blends the
natural approach terrain to a bowl that is deepest near mid-span and closed in
by lateral spurs, with zero weight at both abutments so the formation stays
seamless. Viaduct pier shafts thicken with height and gain footings and tie
beams; piers and parapet remain in the existing static scenery batch.

`TunnelGeometry.hpp` builds the tunnel lining per 20 m segment and wall: a
textured precast face in the shared concrete batch, a roof haunch, longitudinal
and absolute 2 m ring joints with bolt pockets on Medium/High, a handrail and
cable tray on 4 m brackets, and bracketed wall luminaires whose gradient wash is
drawn in the same textured batch so its edges match the lining. Niche bays add an
escape-route sign (direction and distance to the nearer portal from
`Sim::kTunnelExtentsMetres`) and a hectometre plate in the printed-text batch.
`tunnel_lining_rings` and `tunnel_marker_plates` expose the populations.

`TrackGeometry.hpp` holds the permanent-way cross-sections used by the 2 km
track batch: the ballast toe wobble, the chamfered monoblock sleeper (detailed
inside the fastener radius, a box beyond it) and the foot/web/head rail profile
under the specular running-surface quad. Ballast shoulders are sloped textured
quads in the ballast material; the formation box is hidden under the crown.

`LinesideGeometry.hpp` provides `addBarBetween` (a pitched, yawed bar between
two world points) used for catenary contact/messenger chords, fence strands and
struts, plus the boundary fence with absolute post cadence and strained end
posts. `SignalGeometry.hpp` builds the colour-light signal; its lens centres are
shared with the live aspect overlay in `SceneRenderer`.

`InteriorFittingsGeometry.hpp` holds close-inspection passenger fittings in the
shared interior batch: seat pedestals, rack suitcases, cab-door hardware,
extinguisher fittings, the emergency intercom, seated panel fasteners, the
gangway bridge plate and grab-rail end caps. Seat grip recess and literature
pocket construction stay in `addSeatRearShell`, bounded by the existing rear
envelope and triangle budget tests.

`ToiletGeometry.hpp` builds the accessible compartment's fittings (toilet, support
rail, paper dispenser, floor coves, sink vanity and spout, mirror, dispensers,
threshold) in absolute train-local coordinates within the compartment envelope.

`TrainExteriorGeometry.hpp` builds the passenger train's exterior kit in the
interior batch outside the structural walls: lower bodyside with skirt cut-outs,
bogies and underframe modules, livery bands, window gaskets and mullion faces,
door reveals, roof crown with HVAC housing and pantographs, inter-car bellows and
the cab nose, lamps, coupler, visor, gasket and wipers. Lamps use the unlit
interior emitter batch; nothing is drawn with an extra call.

`TerminalGeometry.hpp` authors both termini: buffer stops, the train shed, the
head house and the forecourt taxi rank, placed from the station datum with the
endpoint tangent extrapolation of `Route::relativePose`. The track batch overruns
each route endpoint by `kTerminalTrackOverrunMetres` so the stopped consist and
the buffer stops sit on real track.

## Station detail geometry and clearance

`StationGeometry.hpp` contains bounded canopy frames, panel soffits and sealed
luminaire geometry. `PlatformLayout.hpp` shares structural bay placement with
passenger fixture clearance. Columns avoid station entrances and centre name
boards; light and live-display placement follows the bays. Live display heights
are bounded below the beam underside, including frame and glyph extents.
Soffit grain reuses the streamed concrete material. Luminaire diffusers share
`worldLuminaires_` with tunnel emitters; the housing retains normal daylight
shading. Public `tunnel_luminaires` statistics still count only tunnel fixtures.
No renderer-specific geometry path, shadow map or new texture is required.

Medium/High platform paving consists of staggered 600 mm modules in the existing
concrete batch. The 400 mm tactile band adds bounded 5 mm relief within ±78 m of
the station centre; farther tiles retain grain and joints. Low uses simpler grids.
Fence runs explicitly terminate at posts on both sides of the access opening.
Station window interiors are opaque shallow cues, not traversable rooms or
per-pane transparency. Family palettes select existing masonry/render materials;
plinth caps are split around the public door and corner trims wrap the wall shell.

## Terminal frontage and streets

`StationSite.hpp` reserves a deterministic endpoint forecourt footprint before
procedural feature resolution, including relocation candidates. `ForecourtGeometry.hpp`
builds the Medium/High graded apron, clipped slab joints, raised planting beds and
benches into existing batches. Placement samples the same terrain cells as the
ground mesh; endpoint-local distance is mirrored at Westhaven. Joint strips do not
overlap slab faces, avoiding the depth artifacts found during GPU inspection.

`terminalGroundTint` adds endpoint-local service-margin/dry-verge/managed-lawn colour zoning
to the existing urban terrain texture. A smooth 350–600 m endpoint fade restores the original
urban tint, preserving the railway corridor inside 10.4 m and distant terrain beyond 260 m.
It is sampled at existing terrain vertices, so it adds no geometry or material pass and does
not represent a detailed ground-cover blend or fine grass simulation.

Medium/High terminal skyline blocks have bounded raised entrance terraces, pavement joints and
planted ends. Terrace datum follows its building; buried slab sides accommodate coarse terrain.
Two 650 m street strips lie 58 m from the railway, with seven-metre carriageways and 2.6 m
sidewalks. Their ten-metre geometry patches sample the existing terrain triangle diagonal in
the 40–100 m lateral bands. This assumes the current route's 50 m-aligned terminal endpoints;
changing route length/grid requires revisiting that sampler. All geometry shares the static
world batch, with no new textures, draw passes or traffic AI. These authored strips are not yet
a connected street network, and the railway-side midground still needs composition work.
Deterministic paired broadleaf groups occupy the route-exterior terminal extension only. Their
45.2–46.8 m lateral band and bounded crown size leave the street sidewalks clear. The platform
side excludes the first 90 m for station architecture. Roots and soft contact shadows share the
street terrain sampler, and Medium/High reuse existing foliage/structural/shadow batches. This
may activate the foliage batch in a view that previously had no textured trees. Low omits it.

## Upholstery material

Complete seat cushions, backrest bodies, crowns and shaped fabric insets use one static batch and a project-generated 256-square
opaque woven texture, separate from the floor carpet. The 16 cm repeat combines 1.25 mm warp/weft
with heathered yarn variation and tapered flecks of varying length, lean and position,
retaining each car's authored vertex tint. Lower-resolution sources are filtered from the native
design rather than sparsely sampling the fine weave. Backrest UVs are
seat-local rather than absolute coordinates; cushion UVs use the same physical repeat. Backrest UVs
use dominant-face projection at 16 cm per tile, including the previously flat-coloured sides.
Cushions unfold their fabric coordinates continuously over the top roll and down the sides,
avoiding a projection-axis switch along the rounded edge.
Plastic rear shells, hardware and removable head covers remain separate. A full
mip chain and High's anisotropic sampling limit distant aliasing. Seat collision
is unchanged; the redundant top fabric decal has been removed. Coverage expansion reuses the
existing upholstery draw and texture. `interiorMaterialTriangles` remains the
aggregate of carpet and upholstery so existing interior coverage reports remain comparable.
Carpet runner traffic wear uses a bounded vertex-colour grid with a lighter/desaturated
walked centre and protected edges, retaining the 0.85 m woven texture repeat. Eight-millimetre
woven perimeter tapes close at the carpet ends; installation seams are only 3 mm wide.
These flush details share the carpet material batch and do not change collision.

Tests cover deterministic pixels, opacity, mip brightness, distinctness from carpet, UV scale
and translation invariance. This does not add fibre geometry or normal mapping.

Headrest crowns use 24 longitude segments and 10 latitude bands, retaining the
original cushion envelope. Removable head covers are two-sided, smoothly shaded
cloth strips rather than rounded plastic plaques: a 380 mm width, shallow 5 mm
flutes, a 4 mm lower hem and a short wrap over the elliptical crown. They remain
in the existing opaque interior batch and do not add a texture or draw call.
Tests check bounds, finite normals, non-degenerate faces, and vertex/triangle-centre
clearance from the crown in both seat directions. This is static draped geometry,
not a cloth simulation or a woven material for the pale head cover.

Rear shells share the existing interior-panel polymer texture and material batch.
A 14 mm perimeter bevel rises 7 mm from its return to the main panel; the recessed
grab detail has a raised surround and lower finger ledge. The lighter blue-grey
polymer retains the original shell envelope and collision clearance. Grain uses
a fixed object-local XY projection at a 120 mm repeat (approximately 0.94 mm
per texel), not normal-dependent projection, so rounded-edge axis ties cannot
change the pattern between cars. Thin return edges inherit this projection;
this is albedo grain and stock specular lighting, not a normal-mapped material.

Passenger glazing accents are soft four-by-four vertex-alpha patches, with zero
alpha around their whole boundary. They replace translucent boxes and the old
hard horizontal stripe; angle-dependent strength remains a cheap approximation.
Mullions retain their structural shell but receive a rounded 25 mm-deep polymer
cap in the existing panel-material batch, inset 4 mm from each pier edge. End
grooves are 2 mm wide; the glass aperture and collision envelope are unchanged.

Armrests separate a narrow carrier from a resilient vinyl pad. Two smoothly
shaded contour bands roll the top into the sides, within the original 90 × 540 mm
plan and 0.88–0.94 m height. Pads share the existing resilient-material batch at
a 60 mm detail repeat; the metal support and gameplay collision are unchanged.

Suspended handles use a closed 28 mm round-section oval polymer grip, a narrow
metal hanger and a separate connector. The ceiling plate meets the actual 2.914 m
skin; the grip's lowest point is 2.336 m. Smooth normals and the existing resilient
material batch soften the hand-contact surface without an extra draw pass.

Luggage shelves use rounded 32 × 16 mm aluminium slats with 28 mm gaps, a retaining
lip, rear rail and end closures. Tapered 22 mm-wide supports meet the 2.214 m
underside and bolt to the finished structural mullion. Shelf tops remain at
2.230 m so existing luggage does not float. Toilet exclusion spans are unchanged.

Passenger information screens have a 1.97 m rounded double-sided enclosure with
recessed display faces and two ceiling stays. Dynamic text is constrained inside
the screen instead of extending into the independent car-role placard below.

Onboard dynamic typography uses an embedded DejaVu Sans ASCII coverage atlas
(512 × 256, mipmapped, proportional metrics). Each visible glyph is one quad,
with larger mixed-case next-station text and subordinate destination/status rows.
The atlas has its own permanently textured, unlit BasicEffect so the main scene's
white identity texture and cloud texture state are never changed by the text pass.
The existing single display draw remains; mesh updates follow the previous route,
page, quantized speed and ETA state key. Font generation is optional offline work,
not a runtime system-font dependency; see `ASSETS.md` for the retained font license.

Passenger door leaves have genuine apertures, rounded flanges and rubber returns.
Their low-opacity glass uses one shared transparent draw for all 24 leaves rather
than 24 additional passes. A persistent CPU scratch mesh and exact-sized streamed
GPU buffers update only when door openness or the released side changes. The pane
centres use the same offset as the opaque animated leaves; collision and boarding
interlocks remain owned by simulation rather than by the new visual opening.

Door-release indicators have static jamb-mounted housings and symbols on both
faces, plus a reusable circular face mesh using the prior per-control draw slot.
The label plane clears the window liner; status colours retain the existing
released/closing/locked semantics. Controls remain indicators for the automatically
operated service, not permission for the player to open doors or leave the train.

Gangway bellows are a closed 12 mm corrugated skin with seven rounded folds and
rounded roof corners, using the existing resilient-material pass. Both sides,
mounting ends and sill edges are closed; the structural header clears the maximum
2.416 m skin height. This is static bounded geometry, not dynamic rubber simulation.

Vestibule bulkheads use separate upper/lower polymer service skins over narrow
dark seals, including the reverse face. Their 230 mm grain repeat shares the
existing panel-material pass; header skins stay behind the signage and safety
equipment remains on the original mounts. None of these skins changes navigation.

Tables share the existing neutral polymer detail material at a 180 mm repeat,
multiplied by their car-specific laminate tint. Two perimeter bands soften the
upper edge; a rounded inset tray replaces the protruding rectangular underside.
The authored top height and usable footprint remain unchanged.

The laminate top is partitioned around two circular cup inserts rather than
covering them with a continuous face. Each insert has a 116 mm outer diameter,
100 mm inner lip, tapered wall and a floor at 0.848 m, 12 mm below the laminate.
Its lip rises only 1 mm above the tabletop. Tests sample each opening to ensure
no top triangle covers it; gameplay table collision remains unchanged.

Ceiling lights are separate 1.38 m modules on a 1.50 m pitch. Their recessed
housings, side rails and end caps surround a slightly crowned opal diffuser with
restrained edge falloff. Emitting faces remain in the existing unlit batch, so
tunnels and changing sunlight cannot turn the powered lamp faces dark. No
per-fixture dynamic lights or extra rendering batches are introduced.

Furniture grounding on Medium/High uses soft elliptical contact patches in the
existing interior-shadow batch: broad low-opacity seat shadows plus tighter
seat/table pedestal contacts. Every outer edge fades to zero alpha. This replaces
hard rectangular patches without changing global cabin exposure; it is a static
ambient-occlusion approximation rather than dynamic light visibility or SSAO.

The cushion now uses a closed 192-triangle, six-ring padded profile: a 24 mm central
dish, a 50 mm upper roll, straight sides and an underside tuck, within the original
450 × 700 × 200 mm body envelope. Interpolated normals soften the deliberately
bounded tessellation. A 3 mm side welt replaces three floating top piping bars.
The seating coordinates, aisle clearance and backrest geometry are unchanged.
Tests weld geometric edge positions to check two incident triangles per edge,
nondegeneracy, winding, bounds, profile depth and translation-invariant UVs.

## Passenger window construction

The saloon aperture is a five-band rounded assembly in the existing opaque interior mesh:
overlapping flange, sloped reveal, charcoal gasket, inner glass-edge return and exterior seal.
The existing transparent pane remains behind the opaque frame. Narrow door-adjacent spans
clamp corner radii to preserve a positive opening. Structural mullions and luggage-rack brackets
derive from the same `TrainInterior` window layout; an independent car-shell offset previously
misaligned the posts by 250 mm. Frame geometry does not change collision or doorway access.
The [visual audit](VISUAL_AUDIT.md) records matched camera evidence and performance costs.

## Station canopy shading

The flat canopy roof has a bounded analytic shadow rather than a general shadow map. Its
4.6 m width and authored station length are projected along `daylightDirection` onto a receiver
height. Medium/High draw one translucent rectangle clipped to the platform surface, replacing
two unshifted faint contact rectangles. Platform avatars sample the same roof projection at
torso height; direct sunlight falls to 12% under the roof, with a 25 cm edge blend and unchanged
ambient light. This uses the existing ground-shadow batch and CNA avatar light properties.
Handheld platform props use their holder's light. Luggage samples at its own location and a
lower receiver height; the shared world effect is restored before subsequent draws. This does
not shadow static benches or arbitrary meshes, and one torso sample cannot represent partial
shade across a person's body. Low omits the ground decal as before. The fixed Stonebridge
showcase validator compares two unoccupied paving patches, rejecting missing shade or black
receivers; this is an authored-view regression, not a general shadow detector.

## Exterior daylight balance

`Daylight.hpp::exteriorLighting` supplies both structural and textured world batches and
platform avatars with the same cool ambient / warm directional-light balance. The daylight
ambient is `(0.35, 0.42, 0.52)` and diffuse is `(0.67, 0.59, 0.47)`: their component-wise sum
matches the preceding lighting, but faces away from the sun receive less fill. This improves
orientation cues without changing geometry or introducing another rendering pass. The tunnel
mix is clamped and interpolates to the existing tunnel endpoint. Cabin lighting remains separate.
Tests cover colour balance, summed intensity, interpolation, endpoints and clamping. This is
directional shading, not a shadow map; occlusion and material-specific light transport remain
approximations.

Two fill lights approximate light reflected from sunlit ground (ENV-129/131). A warm bounce
(`groundBounceDirection`) travels against the sun's horizontal heading and slightly upward, so
facades turned away from the sun keep their own hue instead of all turning the cool ambient
blue-grey; a weaker uplight (`groundUplightDirection`) reaches only down-facing surfaces such as
canopy soffits. Neither reaches terrain, roofs or any up-facing surface, both fade to zero in
tunnels, and the OpenGL 3.3 facade shader applies the same terms as the stock effects.

## Broadleaf foliage material

Medium/High route-feature crowns carry three crossed double-sided leaf-spray cards per lobe
(108/180 triangles per tree)
in one additional shared streamed batch. CNA's public `AlphaTestEffect` discards texture samples
below reference alpha 96 with normal opaque blending/depth writing; cards need no transparent
sorting or culling override. A project-generated 128-square neutral straight-alpha motif contains
26 irregularly placed leaves. RGB stays neutral even in transparent texels to avoid dark filtered
fringes. The pass copies exterior fog settings and uses subdued baked vertex tint (the stock
alpha-test effect has no directional lighting). A shared lobe-layout visitor places each card
through its actual crown lobe; its opaque core is contracted to 60% radius so the
cutout silhouette and gaps are not hidden behind full-size solids. Low, terminal continuation trees and distant LODs
retain their previous paths. This initial edge treatment still needs
moving-view validation and LOD tuning; thin edge-on cards can remain conspicuous.
Leaf textures alone request coverage-aware mip generation at the effect's reference alpha 96.
Each averaged level scales alpha toward the base image's covered fraction using a bounded
search, retaining the closest attainable coverage rather than one arbitrary search endpoint.
RGB and fully transparent texels are unchanged. Tiny levels cannot represent a fractional
covered pixel exactly, so this is not a guarantee against distant disappearance or temporal
shimmer. All other materials retain ordinary averaged mipmaps.

Close clustered crowns vary branch azimuth, radial reach, lobe height/aspect and per-tree width
and depth within their existing envelope. Regular angular rings no longer determine every lobe
centre. Route-feature crown identities use `treeShapeSeed(distance,lateral,species)`, so opposite
sides differ and origin rebasing cannot change shape. High retains fifteen lobes/630 triangles;
Medium retains nine lobes/324 triangles. No new batches or alpha-sorted leaves are introduced.
This remains a hybrid mass/cutout approximation, not individually simulated leaves.
Medium/High route bushes and understorey reuse those foliage/cutout batches. Each shrub has
three 42-triangle smooth inner lobes and nine double-sided cards (36 triangles), deterministic
tree-local orientation, ground-clipped leaf bases and soft standalone contact shadows. Low
keeps its original cheap faceted bodies. No per-shrub draw or simulation object is introduced.

Smooth foliage lobes apply an object-local radial field in the range 0.84–1.0 of their ellipsoid
radius. Its analytic tangent gradient supplies the deformed surface normal, transformed by the
ellipsoid's inverse radii. Existing indexed topology and UV seams are retained; Low's unsmoothed
lobes are unchanged. Unit coverage checks contraction, outward unit normals, unchanged Low
geometry and translated-origin stability. This adds CPU arithmetic during mesh construction,
not vertices or draw calls; it is not an optimization claim.

Near broadleaf crowns on Medium/High share one streamed opaque foliage-material batch. The
128-square project-generated neutral texture uses periodic leaf-shaped highlights, a full mip
chain and linear wrapping, multiplied by the existing crown colours. Stock CNA BasicEffect
provides per-pixel lighting; no custom renderer or dependency patch is involved. Ellipsoid UVs
come from generating slice indices, with midpoint UVs at cap poles and integer circumference
repeat counts so the seam closes. This keeps geometry counts unchanged and avoids alpha sorting.
Low retains the original untextured batch. The material adds one draw when present; tree trunks,
conifers, distant crowns and understorey retain their existing paths.
`--report-state` exposes `foliage_material_triangles` from the actual draw. Three quality-profile
runtime tests check absence on Low and presence on Medium/High at a fixed rural view; UV regression
coverage checks both seam span and origin-translation stability.

## Frame pacing diagnostics

The optional `--no-vsync` launch flag changes only the public CNA GraphicsDeviceManager
vertical-retrace request before device initialization. Normal launches keep synchronization on;
fixed-step simulation still targets 60 Hz and no saved setting is changed. `vsync_requested`
is appended to final telemetry. Two smoke tests cover default-on and explicit-off argument
routing, not driver enforcement. Unsynchronized presentation can tear and is a diagnostic
comparison, not an automatic global performance fix.

Interactive launches with `--report-state` and no finite `--frames` limit emit `INPUT_TRACE`
every five wall-clock seconds. It records focus, project/native capture flags, pause/map state,
cumulative nonzero look-motion samples, view angles and player position. It does not log text
input. Help/display-inspection flags, drawn-frame count and measured wall-time between traces
also distinguish intentionally disabled look and a stalled update loop; the first interval is zero.
This separates missing motion delivery from frame pacing without adding output to normal
play or finite screenshot tests.

`bash tools/test_x11_input.sh` runs an optional isolated Xvfb/X11 event-delivery probe using
`xdotool`, `xmessage`, ImageMagick `import` and a bounded process timeout. It requires display/socket
permissions and limits Mesa workers to four. It creates its own display, never targets the
user's desktop, and cleans up only its own processes and named temporary files. Evidence stays
in `build/x11-input-test/`; `result.txt` first marks the run in progress, then records exit status
and selected options even on failure, so a previous successful receipt cannot describe a new run.
Held clicks/keys exercise look, pause, map, focus loss/recovery,
stand and a short walk. The default three phases disable avatars; set
`INPUT_PROBE_PHASES=12 INPUT_PROBE_AVATARS=1` before the command for twelve phases with avatars enabled. Phase count is
bounded to 3–18 and the watchdog is `60 + 30 * phases` seconds. The probe requires capture before
motion, event progress and at least 0.5 radians of wrap-aware camera yaw travel in newly emitted
active traces during every phase, at least `phases + 1` acquisitions, the exact number of
deliberate focus losses and a final standing state. Its focus target belongs to the private
display and is explicitly destroyed after each recovery. Final telemetry must also show at least
three world rebuilds, seven retained chunks and 500 metres of route travel. Five-second traces
detect sustained frozen yaw, not every transient input stall; event counts alone cannot pass.
The default Xvfb path is not a physical mouse, compositor, all-car traversal or long-play
certification.

`INPUT_PROBE_BACKEND=xwayland INPUT_PROBE_QUALITY=2 INPUT_PROBE_PHASES=12
INPUT_PROBE_AVATARS=1 bash tools/test_x11_input.sh` selects the alternate private Weston/Xwayland
path. It invokes the sibling CNA private-display runner read-only, verifies its private runtime
socket and environment, then sends X11 events only to that session. It never accepts the caller's
arbitrary desktop display. `renderer.txt` records GLX renderer/acceleration information and the
result receipt includes backend and quality. Missing private compositor support is a skip/failure,
not permission to fall back to the live desktop. This exercises a compositor and GPU presentation,
but remains synthetic X11 input, not native SDL Wayland or physical-device testing. The existing
Xvfb path remains the default; quality defaults to Medium.

`--report-state` includes wall-time intervals between successive `Draw` calls. The bounded
`FrameTiming` ring keeps the newest 2048 intervals, with no allocations or sorting during a frame.
The final report sorts a copy and emits `frame_mean_ms`, nearest-rank `frame_p50_ms`,
`frame_p95_ms`, `frame_p99_ms`, and `frame_window_max_ms`. `frame_window_samples` and
`frame_total_samples` distinguish a short run from a rolling-window report; `frame_all_max_ms`
retains the worst interval even after it leaves the window. Invalid negative/non-finite samples
are ignored. These are end-to-end draw-cadence diagnostics, not GPU timers: simulation updates,
frame limiting, presentation and avatar startup can all contribute. The first draw has no prior
interval, asset loading before it is excluded, and screenshot capture/exit is not timed as a frame.
Use these together with world-rebuild timing, not as a substitute for real desktop input testing.

The separate `steady_frame_*` window only records intervals whose endpoints are both after
180 drawn frames and with the full avatar pool ready. This is an explicit warm-up heuristic,
not proof that shader compilation or every resource upload has finished. Normal world rebuilds
and station transitions remain in that window; they are not filtered out as outliers.

Static-world generation is CPU-only `WorldGeometry` output: fourteen material builders plus
geometry counters. One asynchronous job owns a route copy and a deterministic `StreamingWorld`
snapshot for the next chunk in the service direction. Its builders retain capacity. Each
material is split into contiguous segments of at most 262,144 vertices (`meshSegments`), below
the graphics profile's 64 MiB buffer limit. The render thread polls completion and stages 3 MiB
slices into a second GPU window within a 4 ms per-frame budget, raised when the boundary is
near (`stagedUploadBytesPerFrame`); empty slots cost nothing. All meshes,
anchor and counters swap together at the requested boundary. The 2 km track batch follows the
same pattern through `TrackGeometry`: a worker prepares the next 500 m anchor, the render thread
stages its three meshes and swaps them at the switch point (PERF-002). Two GPU windows and one CPU window
bound residency independently of journey duration. No worker accesses CNA graphics resources,
avatar state or live simulation data. Renderer destruction joins the independently owned job.

Initial loads, teleports, setting changes and missed predictions have an immediate complete-scene
fallback. Reversal rejects an obsolete prediction. Existing exact-size dynamic Discard uploads
still reuse matching buffer objects independently, and empty materials release their staging mesh.
Static train/prop meshes remain immutable. `world_buffer_reuses` counts CNA objects, not driver
allocations. New preparation-hit, synchronous-build and late-preparation counters expose whether
the fallback actually occurs. See [the streaming audit](STREAMING.md) for evidence and limits.

`world_rebuild_*_ms` now measures render-thread streaming work, including staged uploads and
fallback waits, across all frames. `world_generation_*_ms` measures completed CPU generation,
usually on the worker. `world_upload_total_ms` counts submissions, while `world_upload_max_ms`
is now a single material submission rather than the old whole-window upload. None measures
GPU completion. Generation can include a prepared window that is not yet visible.
`Route::relativePose` samples only shared curve/elevation functions, avoiding station-influence
and environmental metadata that it never returns. Full `Route::sample` remains authoritative for
urbanity, water, tunnels, bridges and curvature; endpoint tangent extrapolation is preserved.

During a static-world build, `RoutePointCache` retains 128 exact route-distance samples and their
heading sine/cosine. Lateral offsets reuse the same sample. Its key also checks route identity
and reference distance; it never rounds coordinates. The cache lives only for that build, uses
fixed storage and does not survive route mutation or accumulate old chunks. Hash collisions
replace a slot and are verified by the exact key, never accepted as hits. The report's cumulative
`route_point_cache_hits` / `route_point_cache_misses` show effectiveness without implying a GPU
or overall frame-time improvement. Non-world rendering retains its uncached sampling path.

For a controlled CPU comparison, build and run the optional developer target:

```sh
cmake --build build --target trainsim_route_benchmark --parallel 4
./build/trainsim_route_benchmark
```

It is excluded from normal builds and CTest timing gates. The benchmark checks coordinate
equivalence on 1024 route/terminal inputs, then alternates old full-metadata and current
geometry-only sampling order over six pairs of 262144 calls. Checksums prevent unused-result
elimination and validate the repeated batches; reported medians concern this function only.
It does not benchmark geometry upload, rendering, streaming architecture or end-to-end FPS.

## Design boundaries

Train Simulator has three deliberate layers:

1. `trainsim_core` owns route geometry, service state, deterministic passenger behavior,
   configuration parsing, and streaming decisions. It has no CNA dependency and is exercised by
   fast headless tests.
2. `trainsim_render` translates immutable simulation snapshots into CNA OpenGL 3.3 drawing and
   audio. It owns procedural GPU meshes, CNA avatars, the world renderer, and the reactive mix.
3. `train-simulator` owns the CNA `Game` lifecycle, input, first-person interaction, camera, and UI.

This keeps the renderer from becoming the authoritative simulation and leaves game logic portable
to future CNA targets without developing any such target now.

Configuration similarly distinguishes persisted preferences from launch-only QA policy. A
`--quality` profile changes the live renderer without leaking into a later volume/FOV save; only an
explicit quality adjustment in the pause menu replaces the stored profile. A pure core test guards
all three persistence cases.

Runtime assets are resolved from the executable's adjacent `assets` directory, the conventional
installed `share/train-simulator/assets` location, or an explicit override. No compiled absolute
source-tree path is required, and a separate-working-directory application smoke guards that
property. The Linux install tree carries the project-local SDL runtime products (CNA is linked
statically, as cna-engine requires) and an origin-relative runtime search path, so it can be moved
as one directory without referring back to the build or dependency checkouts. Project, CNA, sharp-runtime, and SDL license texts accompany it.
The installed documentation preserves the repository's `docs/` hierarchy, including the compact
showcase PNG gallery, so README links remain valid after packaging. The source-only
`tools/capture_showcase.sh` command recreates those images through production cameras rather than
maintaining a separate presentation renderer. Its saloon view waits for the normal serialized
avatar warm-up, while scenery and map views stay short to avoid wasting offscreen render time.
Each run also uses temporary default preferences, preventing a developer's saved FOV or graphics
settings from silently changing the documented framing. Captures are first written beside those
temporary files. A standard-library PNG decoder validates CRCs, dimensions, all five PNG filter
types, per-word HUD glyph coverage, and the route-map line before an atomic move replaces the
gallery image. A missing word in the built-in negative self-test must be rejected. A failed process
or incomplete EasyGL readback receives up to four fresh-process attempts, preserving the last known
good gallery file if every attempt fails.

`tools/verify_install.sh` exercises the other side of that boundary. It installs the current
incremental build to a throwaway prefix, checks that the executable resolves CNA and SDL from that
prefix, confirms the default configuration and audio sidecars, then launches the application from
an unrelated temporary working directory through CNA OpenGL 3.3. Both temporary trees are removed
through one exit trap, including on validation failure.

## Coordinate model

The authoritative route distance is a double in the closed interval `[0, 286400]` metres. Route
sampling also uses doubles. Immediately before rendering, route samples are transformed into the
train tangent frame and only then converted to float. Cached track and scenery meshes use nearby
route-distance anchors, so curves remain visible without exposing the camera to 286 km float
coordinates. The train and first-person player therefore remain close to the floating-point origin
even after many terminal reversals.

The passenger interior uses a stable, train-local coordinate system. Six 21 m cars span roughly
`z=-63` to `z=63`; the world moves relative to that local frame. This avoids player, camera, and
NPC jitter without rebasing interaction state. The route is deliberately broad enough that the
maximum sampled heading change between adjacent car centres is only `0.0834` degrees, below the
clearance implied by the flexible gangways. A deterministic test guards the `0.1`-degree design
tolerance. Exterior AI consists do sample every car independently and visibly articulate.

The route's render-space baseline is the rail head. The complete low-floor train-local render
frame—including camera, interior, doors, glass, displays, driver, and passenger avatars—is raised
0.70 m above it. The local floor surface at 0.08 m therefore meets the authored platform surface at
0.78 m exactly. The 1.50 m body half-width and 5.00 m platform around a 4.20 m centreline leave a
0.20 m horizontal gap. Invariant tests guard both parts of this level-boarding relationship, while
coping, a warning line and segmented tactile paving expose the boundary visually. A shared
anti-slip bridge-plate mesh deploys only beneath the six released platform-side doors, follows the
same smooth opening amount as their leaves, and overlaps the coping by 2 cm at full travel.
The normal four-second departure interlock exceeds the 1.35-second leaf animation. If a developer
time scale skips most intermediate render frames, entering any moving phase clamps the remaining
visual fraction shut so neither a leaf nor a bridge plate can trail behind the authoritative
interlock. Passenger AI shares that timing contract: doorway motion is held until a conservative
1.45-second passage release, and the application requires one complete passenger update with that
opening before an accelerated development dwell may close. Production dwells exceed the guard
naturally.

## Simulation flow

`RouteSimulation` is a deterministic state machine:

```text
Dwelling -> ClosingDoors -> Accelerating <-> Cruising -> Coasting -> Braking -> Dwelling
```

It tracks direction, absolute route distance, current/next station, target speed, platform side,
doors, dwell time, completed legs, simulated service time, the next scheduled arrival, and arrival
deviation. A stopping-speed envelope prevents integration from stalling just outside a station.
Platform side is mirrored for reverse running. At either terminal the direction changes before the
next leg. The timetable uses explicit authored inter-station running times rather than deriving a
supposed schedule from the current frame's ETA.

Each inter-station segment also owns a 120, 140, or 160 km/h limit. Acceleration, cruise state,
braking envelopes, ETA prediction, and diagnostics consume the active limit; the reactive audio
follows the resulting real speed. This gives suburban and open-country legs distinct operating
rhythms.

Before the stopping-speed envelope requests service braking, a 650 m anticipation window applies
only gentle rolling deceleration. This creates an audible and diagnostic coasting phase without
using implausible acceleration or sacrificing stopping precision.

The application advances the service using the selected time scale, updates passenger intent,
then asks `StreamingWorld` for the bounded corridor around the new position. Rendering and audio
consume the resulting state but do not mutate it.

The service snapshot publishes its measured signed longitudinal acceleration rather than asking
camera or audio consumers to infer physics from a phase label. A damped train-local camera response
uses that value for a sub-degree pitch and centimetre-scale displacement under traction, coasting,
and braking. It shares the existing comfort multiplier; setting camera motion to zero removes the
response immediately. Runtime telemetry and a moving OpenGL smoke guard both the physical signal
and its visible response. The reactive audio derives smoothed traction and braking effort from the
same magnitude, avoiding a binary phase-driven motor or brake layer.

The pause state is propagated to every project-owned CNA `SoundEffectInstance`. Active loops,
announcements, spatial one-shots, and warning sounds therefore retain their exact playback cursor
while the route, passenger AI, and player are frozen, then resume without duplicate triggers. A
runtime diagnostic exposes playing and paused instance counts so the offscreen dummy-mixer smoke
can prove that no sound continues behind the comfort menu.

## Streaming and procedural content

Pass-2 audit additions (ENV-124 to ENV-133, see [VISUAL_AUDIT_2.md](VISUAL_AUDIT_2.md)) live in
focused headers: `VergeGeometry.hpp` (rural and urban lineside zones, cable trough, wildflower
clumps, palisade), `TerrainPatchGeometry.hpp::addGridTerrainCell` (diagonal-preserving terrain
subdivision), `LandscapeVariation.hpp` (field-boundary layouts and tramline spacing),
`ProceduralMaterials` (irregular per-side parcel boundaries and arable parcels),
`FacadeGeometry.hpp` (house and block styles, rural outbuildings), `StationGeometry.hpp` (canopy
palettes and valance) and `TrainExteriorGeometry.hpp::addCabBodyside`.

The 286.4 km route is never loaded as a static mesh. `StreamingWorld` divides it into deterministic
250 m chunks and retains the current chunk plus three on either side (fewer at endpoints). A fixed
route seed and per-chunk hash determine vegetation, modular buildings, railway equipment, and
occasional other trains. Dense approaches supplement scattered architecture with at most fifteen
narrow townhouses per chunk, alternating between the two street sides with restrained longitudinal
jitter. Larger random houses/apartments/warehouses start behind a 44 m inner-edge setback where
this frontage exists, preserving a separate rear layer instead of competing with front gardens.
The seven-chunk major-city regression requires at least sixty townhouses and checks same-side
parcel spacing; generated counts remain bounded. Core, urban, High, Low and material checks pass
(5/5, 7.80 seconds), and a production km-2.3 window capture was inspected. Cross-chunk parcel
conflicts, industrial-yard overlap and pedestrian-access routing still need broader auditing.
The industrial compound regression now also reserves 58 m along the route around its centre
against same-side townhouses. It failed on the prior generator (99/100 core cases) and passes
after conflicting frontage switches side or is omitted when an opposite-side substation also
occupies the slot. Both reservations are rechecked after switching. Five focused core/urban/
High/Low/material targets pass (5/5, 9.22 seconds). The km-2.6 capture exposes a separate remaining
vegetation defect: tree crowns could intersect the denser residential facades. A subsequent
deterministic post-generation pass rejects tree/bush records whose crown footprint overlaps a
same-chunk building parcel (including houses, towers, warehouses, farms and substations).
Independent parcel records avoid reading a vector while erase-if moves its elements. The new
three-location townhouse/crown regression failed before the fix; the complete core suite now
passes 101 cases. A same-camera km-2.6 capture confirms removal of the two facade-intersecting
trees. Conflicting records now try up to eighteen deterministic nearby planting positions before
being omitted: route offsets of 12/24/36 m in either direction and outward offsets of 0/8/16 m.
Candidates retain the original scale and variant, remain inside the owning chunk with a crown
margin, respect building parcels and terrain eligibility, stay beyond urban pavements, and avoid
existing tree/bush crown footprints. No random numbers or extra feature records are introduced.
The km-2.6 recapture restores vegetation between the houses without the former facade collisions;
six focused core/urban/rural/High/Low/material checks pass (6/6, 9.99 seconds), including generation
determinism and parcel clearance. Building clearance now also inspects deterministic raw parcel
snapshots from both adjacent chunks. Raw generation exits before vegetation resolution, preventing
recursion and avoiding dependence on loaded-window order. Only parcel footprints are retained;
the two temporary snapshots add bounded generation work, not active chunks or per-frame work.
A route-wide test at 500 m intervals compares tree crowns against all seven building/reservation
families in the three-chunk window. A separate test compares direct city loading with approach
from the opposite direction, including positions, scales and variants. Five focused core/urban/
High/Low/material targets pass (5/5, 10.29 seconds; core took 4.22 seconds under that concurrent
run). Crown-to-crown spacing across seams and additional renderer-generated vegetation still
need coverage; this does not claim a complete spatial collision system.
The parallel road receives joined
kerbs, raised footways, and route-aligned street lamps; all share the static scenery batch.
Existing overlapping chunks are moved into the next active set; distant chunks are released.

Static content from the active chunks is combined into one vertex-coloured GPU batch whenever the
current chunk changes. It includes a deterministic 2.4 km-wide relief ribbon extending beyond the
fog limit. A second broad-scale ridge component produces a hazy, non-flat distant horizon, while
subtle seeded terrain colours divide rural ground into readable field parcels. Authored crop
features add broad terrain-conforming soil parcels, segmented rows, and optional relief-following
hedge boundaries at closer window distance. Each eligible rural chunk retains two established
random parcels and adds one independently hashed parcel on alternating sides. Its independent hash
does not consume the chunk generator stream, so later buildings and authored-looking landmarks do
not move when field coverage changes. A focused invariant requires both passenger sides to retain
at least five fields across a seven-chunk rural window. A sparse bounded farmstead feature combines a yard,
pitched-roof barn, farmhouse, silo, hay storage, and two-rail perimeter fence in the same static
batch. Near vegetation is backed by a second sparse band of
smaller trees at 88-233 m lateral distance, so open countryside has a woodland horizon rather than
ending in empty terrain. Both layers remain deterministic and add no draw call. The same batch also
contains tunnel segments and
portals, bridge decks/piers, stations, fences, crossings, vegetation, and architecture. Moving road
vehicles and other trains remain separate. Two sets of rails, sleepers, and ballast use a second
curved batch rebuilt around a coarser 500 m anchor, so passing trains have a real alignment beneath
them. Absolute one-kilometre markers, their posts, bordered faces and pixel-glyph numbers join this
same batch, are omitted from tunnels, bridges and platform throats, and therefore add no draw call.
The secondary alignment eases outward around left-side stations to clear island platforms;
its rails, contact wire, traffic and outside catenary-mast placement share one route-owned offset
function. Contact wire remains continuous while quality-scaled 25/12.5 m messenger pieces sample a
22 cm parabolic sag across each 50 m span. Deterministic 25 m droppers shorten at mid-span to meet
that curve. Mast cantilevers add an insulator, triangular brace and staggered registration arm at
the supported track. These parts remain in the static chunk batch and create a credible two-wire
silhouette and speed rhythm without a new draw call. Each passing-train car samples its
own route position so the short consists articulate through curves instead of moving as one rigid
mesh. The shared car batch includes separate windows and doors, end gangways, bogies/wheels,
underfloor equipment, and a roof unit without increasing per-car draw calls. A separate compact
diamond-frame pantograph is submitted once on the centre motor car of each visible consist; its
carbon strip reaches the same 5.85 m contact-wire height used by the route. Lightweight overlays
replace the exposed gangway face at each end with a fictional driving cab: the leading end carries
warm-white lamps and the trailing end red markers, while both provide cab-side glass, a divided
windscreen, wipers, destination pixels and a low crash skirt. Render telemetry counts both terminal
overlays independently of the three shared car bodies, and the passing-train smoke requires all
five expected submissions. The same meshes form a bounded parked service on authored
major-station sidings. Replaced
batches are released immediately: CNA's tested graphics-resource lifetime contract
keeps already submitted work valid across deferred replay, so the project does not retain obsolete
corridor meshes.
The moving AI train's route-distance lifecycle is shared by rendering and audio. Each service runs
one continuous 1.9 km traversal at 31 m/s, then remains absent for a ten-second reset interval;
its only position discontinuity is therefore outside the visible and audible lifecycle. Dynamic
culling uses the live consist centre rather than the procedural feature anchor. When the three-car
consist reaches the listener's train-local position, a generated spatial pass-by event plays from
the same route-owned parallel alignment used by its wheels and catenary; an eight-second guard
prevents duplicate triggers while the same consist remains nearby.
Procedural warehouses are no longer undifferentiated boxes with dark openings. Four seeded,
restrained wall/shutter/service-band palettes wrap a concrete plinth and a paved loading apron.
Each route-facing loading elevation carries three framed slatted roller shutters, dock slabs,
paired rubber bumpers, clerestory glazing, vertical cladding ribs, and a service identity panel;
two raised rooflights break the roof silhouette. Random standalone warehouses use a deliberately
low frequency. Eligible urban chunks instead form bounded two- or three-building compounds with a
shared palette and exact route-relative spacing. A compound leader encodes its size in otherwise
unused `WorldFeature::variant` bits, retaining the 32-byte feature layout while allowing the
renderer to build curve-following hardstanding, a gated front fence, marked staff parking, and
pallet stacks once per premises. The non-leading buildings apply a bounded scale offset, avoiding
an exact repeated roofline while preserving the leader's spacing datum. Each compound also parks
one project-generated rigid delivery truck squarely against the leader's centre shutter: chassis,
cargo box and stripe, cab, windscreen, four wheels, and front lamps join the static batch rather
than consuming a dynamic road-traffic actor. Three-building compounds additionally use an empty
marked bay for a glazed service van and a second loading face for one detached box trailer with
rear-door ribs, tandem axle and landing legs. Parallel urban roads now run on both sides of the
railway, with the bounded traffic palette divided between their opposing lane pairs. Each road
exposes continuous edge markings and sparse slotted kerb inlets between the established centre
dashes; modular buildings yield at least 25 m from the railway centreline so their footprints stay
behind the outer footway. Urban trees and bushes use a matching 26.2/26.5 m minimum setback, which
prevents random vegetation from occupying a carriageway without consuming a different random
sequence. All pieces remain inside the single streamed static mesh; `warehouse_details`,
`industrial_compounds`, `industrial_vehicles`, `industrial_trailers`, road-detail telemetry, the urban OpenGL smoke,
and a whole-route structural test prevent the richer industrial archetype from silently
regressing. Bounded fence-edge planting now breaks up the largest hardstanding expanses; more
varied utility/loading activity and broader land-use transitions remain follow-up work.

Urban residential footprints no longer rise directly from the route-wide terrain sheet. In
urbanity above 0.45, houses and townhouses receive a small muted planted plot, while apartments and
towers receive a paved forecourt sized to their real footprint. Each existing house/townhouse door
connects to a short pale entrance path and threshold; apartments and stepped towers now also carry
a project-built framed entrance in the formerly blank ground-floor centre. The surfaces are only
6-8 cm deep, stay above the textured terrain without z-fighting, and join the opaque static world
mesh. `urban_plot_surfaces` and `urban_entrance_paths` report identical complete groups; the
representative seven-chunk High window reported 123 of each at 106 draw calls and 532,512
triangles.

The streamer swaps two capacity-retaining seven-chunk vectors on each boundary crossing; retained
chunks still move their feature storage, but the active-set container itself no longer reallocates.
Near either route endpoint the complete seven-chunk window shifts inward instead of being truncated,
so a terminal dwell and reversal retain the same scenery depth and bounded memory as open line.
The frequently iterated `WorldFeature` record orders three coordinates, its variant and the
explicit eight-bit kind to fit in 32 bytes on the supported desktop ABI rather than 40; a layout
test guards this cache-density assumption without exposing it to rendering logic.

The unlit sky uses a closed daylight-gradient sphere plus one quality-scaled cloud batch.
A deterministic 512 px RGBA atlas contains four 256 px silhouettes built from overlapping
analytic lobes, two smooth value-noise scales, vertical colour grading, and alpha feathering.
Low/Medium/High submit 4/6/8 distant inward-facing cards; High retains two broad low banks to
soften empty horizon sectors. The cloud pass uses ordinary CNA texture, non-premultiplied blend,
depth-read, linear-clamp, and explicit two-sided raster state, then restores all state before world
rendering. It is therefore one extra bounded submission, renderer-independent, and safely
occluded by terrain, stations, and the train. A deterministic texture test guards transparent,
feathered, and dense coverage, while `sky_clouds` exposes the selected budget.

Rural chunks add desaturated faceted ridge masses beyond their distant woodland belts. Low draws
one alternating side per chunk, Medium draws both sides, and High adds a second farther layer; the
system is rejected around cities, stations, tunnels, bridges, and waterfronts. The masses share the
ordinary static world batch and therefore close long agricultural horizons without a new draw
submission or a route-wide mesh. Their count is exposed in renderer telemetry and the rural smoke
requires a positive result while the urban smoke requires zero.

Tunnel interiors use the static streamed shell plus one unlit luminaire batch. Thin continuous
upper and lower guidance lines keep a side-window passenger oriented between compact ten-metre
service lights, while small warm fixture faces provide speed flashes. This is intentionally a
robust OpenGL 3.3 approximation rather than hundreds of dynamic point lights; it adds one draw call
only while tunnel geometry is loaded. Extra lateral terrain samples around the estuary shoreline
smooth the rise from submerged channel to vegetated bank without a water shader or dense heightmap.

The route pose also carries an optional adjacent-water side and absolute surface elevation. Along
the Lakehurst approach this lets the terrain form a graded basin while every streamed water quad
shares one genuinely level elevation despite changes in railway grade. Reeds
and a quality-scaled, staggered mix of conifer and deciduous far-bank crowns remain in the existing
static chunk batch; procedural terrain features reject the water footprint before generation.
Runtime diagnostics expose both water-patch and wooded-bank-tree counts so an OpenGL smoke can
prove the complete landmark survived streaming and quality-profile changes.

Terrain-anchored features are checked at their exact sampled distance and lateral position rather
than only at the chunk midpoint. Tunnels and the clear corridor beneath either bridge reject them;
the estuary moves natural features onto its raised banks, while the dry viaduct permits vegetation
on the valley slopes. Road traffic never appears on either unsupported crossing. Sparse road
overpasses clear the contact wire, keep piers out of the running lines and parallel road, and join
local terrain through sloped approaches. Station/tunnel/bridge exclusion zones and a full-route
invariant test guard the policy. The route pose explicitly distinguishes a water crossing from a
dry viaduct, so they cannot silently share the same blue ground treatment again. Bridge piers derive
their height from the local generated terrain, and low curbs plus open rails preserve the primary
seated-window sightline. Deck, fascia, longitudinal girders, rails, and cross-members now sample
the route every 20 m rather than inheriting one straight 250 m chunk pose. The river bridge uses a
dark steel edge and broad blade piers; the dry viaduct uses a pale concrete parapet, paired slender
shafts, and wide caps. The global 20 m grid clips cleanly at 250 m chunk boundaries, while absolute
12.5 m cross-members and 55 m piers do not duplicate there. Concrete breast walls, landward wing
walls, and dark expansion joints terminate all four authored crossing ends. The route pose carries
an absolute estuary surface elevation; 50 m curved water strips consume it, so
the river no longer starts at a chunk boundary or inherits the railway grade. Runtime telemetry
distinguishes river-water strips, total/river/viaduct deck sections, and loaded abutments. Independent
CNA audio-enabled OpenGL smokes prove both construction types while requiring unrelated geometry
counts to remain zero.

Rural level crossings are explicit deterministic streamed features rather than renderer-only
chunk coincidences. Their cross-road surface, edge lines, posts, cabinets, signal housings and dark
lenses remain in the static scenery batch. Two shared red/white barrier meshes close smoothly by
330 m from the train, rise after 520 m, and remain visible throughout the 900 m crossing range;
one lens per road approach alternates while the crossing is protected. Only the two booms and two
lit lenses are dynamic submissions. A crossing takes the maximum closure requested by the player's
service and every currently valid streamed AI train, preventing a visible opposing consist from
using an open road. The same shared closure query drives a low-volume spatial warning-bell loop at
the closest roadside post; it fades with distance and cannot disagree with the barrier state.
Route-wide tests keep crossings out of tunnels, bridges and a 650 m station
envelope. One OpenGL smoke protects a nearby crossing for the player, and an audio-enabled variant
also requires the synchronized spatial warning event; a second places the player 800 m away and
requires the same barriers and lamps solely because a rendered AI consist is 445 m
from the road.

The platform currently being approached or occupied receives one separate emissive departure-board
batch. Its destination, platform number, ETA/boarding state, and timetable deviation update without
forcing the much larger seven-chunk scenery batch to rebuild; terminal direction reversal changes
the same authoritative content immediately. The board uses a 3.05 x 1.08 m framed case behind the
principal waiting line rather than the earlier 3.70 x 1.32 m billboard-like face. Its along-platform
offset keeps all three text rows legible from a real fourth-car window seat without obscuring the
platform population or most of the station backdrop.

Every major station also carries two low planters at the rear of its platform. Stone cases, inset
soil, and three size-varied evergreen lobes stay in the existing static scenery submission. Their
count is reported independently and the major-station OpenGL smoke requires both instances, making
the modest platform softening deterministic rather than screenshot-only decoration.

Every platform adds two framed poster cases with a deterministic station-family colour rotation.
One service cabinet is retained at minor stops and two at major stations; inset doors, three vent
slots and a yellow warning plate distinguish them from bins and ticket machines. Both classes stay
in the static scenery batch, expose telemetry, and are required by the platform-view smoke.

Six restrained platform material families distinguish terminal, suburban, rural, waterfront,
urban, and market-town stops without replacing their structural station archetypes. They vary the
platform base, concrete surface, and bench finish. Minor stations receive two bench assemblies and
major stations three. Medium/High add five/seven deterministic low-alpha wear patches respectively
and a contact patch beneath every bench; Low omits the wear geometry. These details remain in the
existing streamed static/contact batches and have explicit platform-view telemetry coverage.

The outer edge of every raised platform now receives 6 m modular steel boundary bays with upper
and lower rails. The generator omits bays around one access opening aligned to the station-building
entrance, so the safety treatment does not imply a sealed platform. Rural and suburban families
place five bounded terrain-level shrubs outside that rail where the access gap permits. Rails and
planting reuse the static scenery batch, and `platform_boundaries` plus the Low platform smoke
protect the otherwise easy-to-lose edge condition.

Major-station clock cases, marked faces and twelve hour ticks remain in the static scenery batch.
Only their two slim hands are dynamic submissions while the train is within 260 m. They derive a
continuous analog time from the simulation's monotonically increasing service clock rather than
the host wall clock, so pause, accelerated QA and indefinite terminal reversals stay coherent. A
runtime telemetry count and dedicated OpenGL smoke guard both hand submissions.

Vegetation outside the configured 250-1000 m range is omitted when the current static batch is
built; changing the range invalidates that batch without resetting the simulation or streamer.
Station locations, bridge/tunnel zones, route shape, elevation, and urban envelopes are authored.
Northbridge and Meadow Junction use broad major-city envelopes, while smaller stops retain compact
town envelopes. Modular urban footprints are scale-limited and placed from their actual half-width,
so even warehouses and apartments preserve a deliberate road/railway setback. The countryside
between them is procedural. Chunks above the dense-city threshold add only one or two alternating
far-side towers, 92-188 m from the track. Their podiums, stepped upper volumes, window grids and
roof plant remain in the static batch and create depth behind the near streetfront without an
unbounded skyline population. Grass clumps and crop rows join bushes and trees in
the same static batch, and all natural/building features sample the terrain relief at their exact
route/lateral coordinate rather than assuming track height. Cheap ground-colored tree and shrub
contact shapes improve attachment without another pass or draw call. Conifers use three distant or
four near short, slightly offset branch skirts and a two-stage tapered trunk, while each nearby
broadleaf trunk forks into three rotated box limbs beneath its five-lobed crown. The generator caps
close-tree scale as lateral distance approaches the railway and moves the crown beyond an 8.8 m
maintained-corridor envelope. This prevents an unlucky mature tree from occupying a complete window
or intersecting the fence while retaining larger specimens farther across a field. Broadleaf crowns
and shrubs use a project-owned low-segment faceted ellipsoid whose
finite unit normals are covered by the render tests. Low quality skips half of the far woodland
features while retaining the near speed cues. This hybrid gives important places a stable identity
while keeping memory bounded.

Open countryside samples additional terrain vertices at 8.8, 10.4, and 12 m from the alignment on
both sides. A smooth 34 cm bowl between those samples forms a real drainage depression immediately
outside the fence, with a darker verge tint but no overlaid decal. Station/city envelopes, tunnels,
bridges, and the Lakehurst shore reject this cross-section. Sparse three-blade clumps follow the
outer bank on an absolute 50/32/24 m Low/Medium/High cadence; Low alternates sides. The runtime
exposes both the number of 50 m drain sections and verge tufts; paired rural/urban OpenGL smokes
require them to be present and absent respectively.

Rural terrain colours use 500 m longitudinal cells rather than changing at every 250 m streaming
boundary. At each eligible cell boundary, ten short hedge pieces on either side follow the sampled
relief from 12-108 m off the alignment. Station envelopes, water, bridges, and tunnels remain clear;
the same rural/urban smoke pair guards positive and zero boundary counts.
Medium/High hedges use a rounded smoothly shaded core with tapered ends, a small irregular
foliage lobe per 1.5 m span and double-sided side/crown/end leaf fringes. They join existing
foliage/leaf batches rather than adding per-hedge submissions. Terrain sampling and absolute
route identity are retained. Low continues to use the original six-point opaque profile.

Close conifers retain seven staggered whorls but now build each from individual drooping needle
fans around a narrow central crown. Each fan has four upper facets and two underside triangles;
tree-local deterministic variation changes reach, azimuth, droop and subdued needle colour.
Gaps between fans break the former continuous conical skirts. The central cone plus fans use
exactly the former triangle budget (504 triangles at nine branches per tier). Branch count is
clamped to 6–12, distant three-cone LODs are unchanged, and no draw batch or alpha sorting is added.
Medium/High near conifers now use a separate 128-square opaque needle-spray detail texture with
a full mip chain. Dominant-face planar UVs use a tree-local origin and a 0.75 m period, retaining
physical detail scale and avoiding floating-origin texture swimming. All such crowns share one
additional streamed mesh/material submission (not one per tree); `foliage_material_triangles`
includes both broadleaf and needle batches. The texture is neutral procedural modulation, not
alpha-cutout needle geometry. Low and distant conifers keep the original untextured submission.
On Medium/High, the latest needle silhouette pass replaces the broad solid fans with narrow
branch supports and three crossed cutout fronds per branch. The central cone contracts to 29%
of the whorl radius. Each nine-branch, seven-tier near tree now uses 378 opaque plus 756 cutout
triangles; Low retains the 504-triangle solid fan path. A generated 128-square needle atlas
contains irregular twig/needle strokes. Coverage-aware mips use alpha reference 96. One additional
shared streamed needle mesh reuses the leaf alpha-test effect with explicit per-batch texture
selection and copied fog state. Neither foliage batch changes global culling or blending.
This remains a low-cost approximation without wind animation.

The broadleaf and needle CPU mesh staging buffers are retained across world rebuilds alongside
the main opaque staging mesh. They are cleared (not moved out) before regeneration; GPU uploads
snapshot their data. This removes repeated vector growth for these large batches, while retaining
the largest encountered capacity until renderer destruction. It is two buffers, not a cache of
every visited chunk. The tradeoff is retained peak staging memory; it does not solve synchronous
regeneration or GPU upload stalls, nor change the seven-chunk residency policy.

Feature conifers use a mixed shape seed from quarter-metre absolute route/lateral coordinates
and species identity, rather than passing the species number as the entire shape seed. Thus
same-species neighbours no longer share just two close-crown patterns. Width varies within
88–100% of the existing footprint and interior whorl heights vary by at most 8 cm before scale;
bottom/top envelope and triangle counts remain unchanged. The seed excludes camera position,
floating origin, loaded chunk order and service direction. Existing feature placement already
encodes the route seed. Distant lake-bank and horizon populations keep their compact authored
variants; this is not yet a full tree-age or ecological succession model.

Near-tree features on Medium/High additionally produce two/three compact outward-side understorey
groups. Their positions offset along the route and away from the maintained corridor, resample the
real terrain, and are rejected by the same tunnel/bridge/water clearance contract as their parent.
High gives alternating compatible identities one smaller young tree, creating mixed-age clusters
instead of uniformly isolated mature specimens. Low pays no geometry cost. Shrubs, saplings, and
their small contact patches remain in the existing static-world and shadow batches; no simulation
entity or draw submission is added. `understorey` reports accepted source groups. A representative
High rural window reported 162 groups, 85 draw calls, and 431,708 triangles.

Rural terrain colours follow absolute 500 m parcels, matching the existing transverse hedge
rhythm, with separate inner (26–100 m) and outer (100–260 m) fields on each side. A mixed parcel
identity selects restrained pasture, hay, stubble or fallow tones. Adjacent terrain bands within
one field no longer receive unrelated green stripes, and colour identity is independent of the
loaded chunk centre. Track/drainage/maintained verge colours below 26 m and distant haze colours
above 260 m are preserved. This changes vertex tint only: no new meshes, textures or submissions.
Not all longitudinal parcel boundaries have modeled hedges yet; crop-row geometry remains a
separate feature system rather than a complete land-use simulation.

Terrain beyond 260 m receives a progressively lighter, desaturated tint before stock fog blends it
into the sky colour. Rural chunks place a second quality-scaled line of 2/3/4 trees per side at
310-422 m, using tapered five-sided trunks and compact six-sided layered conifer or broadleaf
crowns. This layer is
ordinary streamed static geometry, excludes tunnel/bridge/water footprints, adds no draw call, and
is reported separately; conifer tier telemetry is also required, so the rural smoke cannot pass on
foreground broadleaf trees alone. Urban and
suburban chunks use the same bounded approach for 1/2/3 distant buildings per side on
Low/Medium/High at 155-239 m. Their muted facades and deterministic pitched/flat roofs provide
settlement depth without competing with detailed foreground streets; the urban smoke requires the
layer while the rural smoke proves it does not leak into open countryside.

A physical High-quality rural window capture after this pass reported 443 visible conifer branch
tiers, 82 draw calls, 372,878 triangles, and the 60 FPS frame cap. This is a short visual/performance
sample rather than a sustained benchmark; a moving physical-GPU ride remains required.

At each route endpoint, a flat subdued apron sits just beneath ordinary streamed terrain and
extends beyond the terminal datum. This covers the portion of the centred consist that necessarily
projects past the closed simulation interval during a reversal. A small batched two-sided skyline
breaks the resulting horizon; neither measure extends simulation state or keeps extra chunks alive.

Sparse four-chunk high-voltage corridors and isolated traction substations are deterministic world
features. Their pylons, conductor spans, transformer equipment, and perimeter structures join the
same static world batch, so the broader electrified landscape adds no per-frame submissions.

Direction-facing block-signal foundations, relay cabinets, masts, access ladders, numbered plates,
heads and three dark lens wells remain in the streamed static batch. The selected aspect face is
drawn separately at the corresponding physical height within the normal dynamic-object range: a just-occupied block
behind the service is red, a departure signal remains red through the dwell/door-closing phases,
an approach to the next booked stop can show yellow, and a clear block is green. Opposite-facing
signals alternate along the route, so the bidirectional service no longer encounters randomized
decorative aspects.
The `--signal-view` QA entry point starts at the first open-line block signal, reserves an actual
clear seat at the aligned same-side window, and derives camera yaw and pitch from that generated feature. It
therefore validates the production seat, window, streamed structure and live aspect together rather
than relying on a detached exterior spectator camera. The service begins frozen so asynchronous
avatar initialization cannot move the target out of frame; `F5` returns immediately to live motion.
The seated view deliberately validates the complete signal head, live lens and ladder; the train's
real window sill naturally occludes the track-level foundation from this passenger viewpoint.

The two recurring CPU mesh builders reserve measured capacities for the curved 2 km track batch
and seven-chunk static world batch. Rebuilds therefore allocate one bounded, measured working set
instead of repeatedly growing vectors as dense sleeper, crop, and station geometry is appended.
Each six-metre track segment uses a fixed three-centre array, removing hundreds of tiny temporary
allocations from every track recenter.

The Low/Medium/High graphics profile applies an additional vegetation cap, dynamic-object range,
6/9/12-person onboard avatar render budget, station-crowd budget, contact-shadow policy, and
parked-service policy. It can be changed from the pause menu and invalidates only the affected
static batch; all 38 authoritative character states and the service/streamer continue uninterrupted.

Road traffic follows the same nearby route-relative corridor on mirrored urban roads beyond both
railway verges. The deterministic generator keeps buildings and vegetation behind the outer
footways, so neither can overlap the kerbed, centre-marked traffic bands. The six vehicle variants
are divided between sides while parity retains separate opposing lanes. The production render
statistics count only cars that survive the dynamic range cull. A dedicated right-window OpenGL
smoke starts on the Alderwick city approach and requires at least one such animated vehicle,
guarding the complete generated-feature-to-visible-mesh path.
Urban house and townhouse plots now carry three low masonry/hedge boundaries inside their
existing footprint. The railway-facing frontage stays open for entrance paths, while the far
edge and two side edges establish domestic parcel boundaries. The six additional boxes per
eligible plot are part of the existing streamed opaque batch, with deterministic colour variants
and no additional submissions or actors. An urban km-2.3 window capture confirms visible side
boundaries without covering the entrance. Urban, High, Low and material-integrity tests pass
(4/4, 8.38 seconds). The hedges remain coarse block approximations; this does not solve missing
the extensive uniform urban ground between parcels.
Near-frontage entrance aprons now extend toward the outer pavement at lateral 24.6 m, using
two-metre-or-shorter paving panels with restrained colour variation. Paths are restricted to
urban, non-bridge/non-tunnel chunks and apron edges below 80 m; skyline buildings are excluded.
Panel heights respect both analytic terrain and its coarse lateral strip interpolation, plus the
apron/pavement height transition. This adds no draw calls. The km-2.3 capture shows a new connection
at the second frontage. Initial captures left the closest frontage apparently disconnected;
the street-formation correction below exposed the obscured road and pavement. Completeness
across all plot arrangements is not established. The three urban/High/material-integrity checks
pass (3/3, 3.45 seconds), but do not detect buried or occluded paths. Terrain/road grade integration
and obstruction-aware path routing remain open audit items.

The follow-up exposed a shared terrain/road grade defect: lateral terrain relief began at 12 m,
but the road and its two pavements occupied 12.1--24.7 m at a fixed route-relative height. Terrain
could therefore bury asphalt, markings, and entrance links. Urban, non-tunnel terrain now holds
the -0.34 m formation through 26 m and smoothly returns to natural relief by 40 m. Additional
mesh bands at both signed 26/40 m locations retain that formation in the actual rendered mesh;
the entrance path's strip-height estimate uses the same breakpoints. Bridge terrain still takes
its existing early-return branch. The same km-2.3 capture now shows continuous asphalt, lane
markings, both footways and the second frontage path joining the pavement. Seven focused urban,
High/Low, material, rural, waterfront and crossing checks pass (7/7, 11.10 seconds).
The subsequent street-geometry pass replaces chunk-length boxes for asphalt, kerbs, continuous
edge lines and both footways with route-sampled strips no longer than 25 m. Adjacent sections
share endpoint positions and elevations; only top faces, side skirts and chunk end caps are
emitted, still in the existing opaque world batch. Logical marking counters continue to count
road sections, not tessellation pieces. Discrete centre-line dashes retain their short boxes.
A 90-frame production window run shows continuous pavement and road surfaces; urban, crossing,
High, Low and material-integrity checks pass (5/5, 12.11 seconds). This short run is not proof of
every curved seam or urban-envelope boundary: terrain uses 50 m longitudinal samples and urban
road presence is still selected at chunk level, which need further transition coverage.

Dense urban terrain no longer uses one grey tint for every unbuilt surface. Absolute-coordinate
vertex tints blend a neutral railway verge into muted green ground beyond the maintained corridor,
with low-amplitude broad variation and desaturated distant colours. Existing road, yard, plot and
forecourt meshes retain their authored materials above it. Four corner colours interpolate within
each terrain quad without adding meshes, textures or submissions. A unit test covers deterministic
results, opaque bounded colours and local continuity. The km-2.3 production capture separates
planted ground from the asphalt street; render, urban, High, Low and material checks pass (5/5,
9.52 seconds; 28 render unit cases). This remains a base land-cover approximation: vacant parcels
are still too broad, urban/rural selection is chunk-based, and more specific park/soil/paved
land-use masks are needed rather than treating every unbuilt parcel as lawn.
Six shared colour meshes provide rounded body corners, a tapered cabin shell, sloped front and
rear glazing, side windows, mirrors, circular twelve-segment tyres and hubs, bumpers, and
directional lamps. The cabin surfaces share their slope with the inset glazing: the former solid
upright cabin intersected the windscreens. Surface winding follows the existing box convention;
the corrected shell was checked in an OpenGL passenger-window capture at km 2.3, and the render
and urban runtime tests passed. A subsequent capture exposed far-side wheels showing through the
body: the shared XZ-rounded box had opposite triangle winding to the ordinary box and the XY/YZ
rounded variants. Correcting its caps and walls restores opaque body occlusion without adding
geometry or submissions. An indexed-triangle regression checks all four box variants against the
same exterior culling convention; it failed before the correction and passes afterwards. Wheel
arches and body proportions still need refinement; this remains a low-poly traffic approximation.

## Train and interaction

Lighting normals are outward-facing independently of the clockwise index convention. The former
normal helper took the right-handed cross product of the emitted clockwise indices, incorrectly
lighting the inside of otherwise correctly culled boxes and ground planes. It now uses the
opposite cross product. Cylinders, cones, ellipsoids, and horizontal discs also had inconsistent
face ordering; their exterior winding is now aligned with boxes. Vertex/index counts are
unchanged. Tests independently verify outward normal direction and clockwise indexed faces for
box variants and curved solids. Same-camera urban and saloon captures show brighter upward-facing
surfaces, correct directional facade response, and visible tabletop cup recess markings. No light
intensity or exposure adjustment was used to conceal the geometry issue. The remaining stylized
materials, sparse urban ground cover, and flat ambient-light model still need visual development.
Validation covered all 51 CTest targets across two runs: the initial full run was terminated by
signal after 31 passing targets; an explicitly filtered remainder then passed 20/20 in 67.68
seconds with at most four workers. The render binary reports 27/27 cases. This is aggregate
coverage, not a claim that the interrupted single full-suite invocation completed successfully.

All three rounded-box profiles retain both tangent endpoints of every quarter arc. Previously the
last endpoint was omitted, connecting the incomplete arc directly to the next corner and tilting
what should have been a straight side. With three arc segments, each profile now has sixteen
perimeter points instead of twelve (64 triangles rather than 48 per solid). This bounded cost
adds no submissions and leaves collision envelopes unchanged. A regression checks all eight
tangent endpoints in each profile plane, in addition to bounds, normals, and indexed winding.
After this correction, 26 render unit cases and six focused render/interaction/urban/High/Low/
material-integrity CTest targets passed; a production car-2 saloon capture was inspected. This
corrects asymmetric silhouettes, not the remaining flat shading and coarse upholstery shapes.

The train is one batched procedural interior/cab mesh wherever possible. Its individual seat
cushions, backs, passenger-side headrest covers, rear shells with grab recesses and literature
pockets, aisle armrests, woven-edge floor runner with 72 tested inlays, rack ribs, and overhead cases,
fixtures, shell, and cab detail
therefore retain a single stable draw cost. The front/rear seat trim is emitted as correctly wound
one-sided quads rather than dozens of permanently visible thin boxes, preserving the aisle detail
without paying for hidden faces. Collision uses explicit passenger-space dimensions
instead of deriving physics from rendering geometry. Seats, display
inspection, sitting/standing, and the accessible toilet door share one distance-based context
interaction path. Player seat reservations use the same authoritative occupancy system as NPCs.
Extended native walking exposed opposing actors that could block each other's yield. Nearby
actors now choose a consistent side for a centred player, notice the player across a 2.5 m
seating bay, and retain the manoeuvre until 3 m away. A blocked sideways move can become a
clearance-checked longitudinal retreat; the toilet wall also forces a retreat when no usable
opposite side exists. Interior bounds, body separation and car identity remain authoritative.
Opening the toilet gives a player within 1.8 m of its entrance priority over new NPC
reservations. Being inside retains that priority; closing outside or walking away releases it.
An existing NPC reservation still blocks the opening interaction. Regression and native-input
evidence, including unsuccessful harness iterations, is recorded in [STABILITY.md](STABILITY.md).
Display inspection remains live rather than pausing simulation and expands the physical display
state into a twelve-stop route diagram with passed/upcoming markers, continuous train position,
direction, speed, ETA, following calls, and schedule status. `--inspect-display` exposes the same
production overlay for deterministic renderer QA.
First-person look treats CNA relative-mode `MouseState.X/Y` as consumable accumulated deltas, not
absolute positions. A 240-frame constant-motion regression protects against accidentally taking a
second difference and cancelling steady movement. Runtime capture compares three independent
facts every update: the project's captured flag, CNA's authoritative relative-mode flag, and the
window's active state. Any disagreement or focus loss explicitly disables native relative mode,
reveals the cursor, records an unexpected loss, and requires one clean click to reacquire. This
also handles a window manager that leaves the native flag enabled after raw motion delivery has
stopped. Map, help, display-inspection, and pause overlays use an intentional release path that
does not inflate loss telemetry. Unit coverage distinguishes focus/mode failures, requested
overlay releases, acquisition, and a healthy stream.
Sitting and standing update that authoritative occupancy immediately, while a 0.65-second smooth
camera path interpolates lateral position, longitudinal position, eye height, and the initial
view turn. A new transition samples the currently displayed pose, so rapidly repeated interactions
remain continuous rather than snapping back to a stale endpoint.
Each saloon has a fixed passenger-facing role: quiet, work/table, accessible, family, luggage,
or standard. Double-sided function signs, restrained seat/headrest/table colour accents, and a
denser overhead-case population in the luggage car provide variation without fragmenting the
coherent train design or adding draw submissions. Render telemetry protects all six signs and the
27 permitted overhead cases; the same layout predicate still prevents luggage from entering the
toilet volume.
Function-specific close fixtures make those roles legible below sign-reading distance: the quiet
car has an open book, the work car open laptops and keys, the accessible car an assistance-call
panel and floor pictograms, the family car a printed activity book and crayons, the luggage car
a full-height vestibule rack with retaining straps and extra
cases, and the standard car a route-leaflet pocket. All remain outside the authoritative aisle and
seat collision envelopes.
Car role labels use the existing antialiased display atlas with separate project-created icons.
Work tables have inset charging ports. The luggage array is not indexed by passenger-car number:
its fourth entry at z=23 m belongs to Car 5 because the toilet vestibule is omitted. The tall
rack uses a thin rear panel so it does not intersect the upper cases.
The fourth-car toilet interrupts the positive-side overhead shelf at an authoritative pair of
layout boundaries; supports and generated cases share the same predicate. A deterministic layout
test protects the compartment from future rack/detail intersections. Its sanitary fixtures,
panelled wipe-clean walls, coved skirting, anti-slip floor, mirror approximation, support rail,
emergency pull cord, dispensers, ventilation grille, and warm local light remain in the same
interior batch, so the more legible compartment adds no draw call.
All eight seat rows in every car derive from the centres of the corresponding clear window bays;
rendered cushions, collision, NPC poses, alternating facing, and the seated camera consume that one
layout definition. A geometry invariant prevents a nominal window seat from drifting onto a
mullion as the two modular pitches change.
The visual mesh now uses a tested low-segment rounded horizontal-box primitive for all 182 cushion
volumes and 33 table tops. Collision retains the simpler authoritative boxes, so silhouette
softening cannot narrow the aisle or introduce unstable first-person edges.
A matching upright rounded primitive softens all 182 seat-back volumes. Every seat now uses one
small floor foot, a slim central pedestal, and a rounded under-cushion bracket instead of two broad
rectangular plate legs; the 93 aisle-side armrests combine an eight-sided vertical tube with a
rounded horizontal pad. Each pedestal foot carries two small six-sided anchors (364 total), while
each of the 33 table stems terminates in a separate twelve-sided floor plate with two anchors in
addition to its wall bracket. These parts remain wholly inside the prior furniture footprint, add no draw
submission, and are counted exactly in runtime telemetry. A close High capture reported 103 draw calls,
431,018 triangles, and the 60 FPS cap; sustained physical-GPU motion profiling remains outstanding.
The audio layer exposes the caption belonging to its currently playing announcement instance, so
the accessibility overlay cannot drift from delayed station or terminal speech.
Two shallow perforated PA grilles in each car give that chime-and-speech sequence a consistent
physical source. They are part of the immutable interior batch and therefore add no draw call.
Two paired louvred climate diffusers per car share that batch and correspond to the continuous
project-synthesized ventilation bed, which remains present while the train is stationary.
Physical cab bulkheads and the train shell enforce the play boundary. Six double-sided in-world
pixel displays rebuild as one batch only when their content changes. Their lower line alternates
between speed/ETA and the next two following stations, and reports the correct door side while
stopped. Door
buttons use green, amber, and dark states for released, closing, and interlocked doors. NPC toilet
reservation drives the same door and occupied state seen by the player. At an open platform-side
door, a contextual through-service reminder makes the intentional onboard boundary explicit rather
than leaving the collision response unexplained. Approaching the roaming conductor offers a
non-blocking context interaction with the live next stop and timetable status; this reads the same
authoritative service snapshot as the passenger displays and route map. The map keeps the simulation
live, shows every station on a distance-proportional line and full-name list, and explicitly keys
passed stations, the directional train marker, and upcoming stops. The toilet's physical
red/green door indicator combines exclusive NPC occupancy with the player's authoritative
compartment volume, so closing the door from inside cannot leave a misleading green availability
lamp in the saloon.
The compact screen-space passenger display is independently toggleable with `H`. Hiding it leaves
the physical train displays, contextual prompts, announcement captions, map, and comfort menu
active, so an unobstructed window ride does not discard safety or accessibility feedback. The
`--hide-hud` launch path makes that presentation state reproducible for visual QA.
That combined player/NPC open state also drives one spatialized generated hinge/latch event on
each transition, so the animated toilet door cannot move silently or double-trigger when ownership
changes.
The dedicated toilet-view OpenGL smoke starts the player inside the real compartment and requires
the rendered occupied-indicator telemetry, while the bidirectional interaction smoke closes the
door from inside and waits across real `Draw` boundaries before accepting either indicator state.
The `--window-view` and platform-aware `--platform-view` showcase/QA entry points reserve a real
free window seat through that same occupancy system, so their camera and stand-up behavior exercise
production gameplay rather than a detached spectator camera. The latter consumes the authoritative
train-local platform-side helper and works in both service directions. Both prefer an available
clear-approach row nearest the selected car centre, avoiding an accidental vestibule-edge view.
Small render-only offsets beyond either route endpoint are extrapolated along the endpoint tangent.
The service simulation remains strictly clamped to 0-286.4 km, while terminal canopy supports,
platform people, signs, and live boards retain their authored spacing around the train-centre datum.
The two inaccessible cab shells contain the same immutable physical control group: a low desk,
rounded two-screen bezel, six-button strip, combined power/brake handle, driver seat and paired
interior wipers. A separate luminous screen mesh is submitted once at the operating end selected
by `ServiceState::direction`; terminal reversal therefore powers down the old end and lights the
new one without rebuilding the interior. Runtime statistics require two physical control groups
and one active display, while the staff-only collision boundary remains authoritative.
Both onboard and platform information batches compare typed state snapshots rather than rebuilding
formatted string cache keys every frame; strings are assembled only when visible content changes.
Passenger-door apertures are cut out of the batched lower bodyside and window spans, with static
jambs and a highlighted threshold. Each animated leaf is a shared one-draw mesh with inset glazing
and handles, rather than an opaque box sliding in front of an unbroken wall.
Five paired inter-car leaves use a second shared glazed mesh. Proximity sensors consider the player
and every unseated non-driver actor, then ease each doorway independently; no collision is attached
to a moving leaf, so a late close can never trap or displace a passenger in the narrow gangway.
The surrounding immutable interior batch supplies accordion ribs, a continuous rubber inner skin,
mounted grab rails, a lit two-sided header and a flush anti-slip bridge plate at every connection.
These details close the visible car-body gap without changing the stable walkable collision plane
or adding a per-gangway draw call.
The shared leaf itself layers a four-sided black glazing seal, ribbed brushed-metal kick plate and
illuminated request sensor over the transparent centre. Every passenger-side vestibule end wall
adds a shallow two-panel field with perimeter rails, four visible fasteners per panel and a dark
three-sided portal gasket behind the safety fixtures. These additions stay within the old wall and
leaf envelopes; `vestibule_portal_details=6` and the dedicated vestibule smoke protect their
coverage while gangway animation and collision continue to use the original dimensions.
Each of the six passenger vestibules also carries a wall-mounted extinguisher, a contrasting emergency
intercom with call button, speaker grille and status lamp, plus two illuminated exit pictograms.
Their position derives from the same `TrainInterior::passengerDoorZ` definition consumed by door
animation, player prompts, exchange AI and QA cameras. All are outside the walkable envelope and
remain in the one immutable interior batch. The dedicated `--vestibule-view` entry point makes the
end-wall equipment reproducible for visual regression inspection; the ordinary aisle view also
keeps the two door-side exit signs in frame.
The six vestibule storage positions contain fourteen bounded cases. A unit-tested YZ-rounded mesh
primitive softens the silhouette seen from the aisle; the six floor cases also receive shallow
zipper bands, paired caster wheels and two-post telescopic handles. All details remain behind
`x=-0.77 m`, outside the clear aisle, in the immutable interior submission. Runtime telemetry
requires `floor_luggage_cases=14` and `luggage_wheel_pairs=6` in both interior smoke profiles.
Seat prompts and activation share one nearest-target query with a forward view cone. A nearby seat
behind the player therefore cannot replace the context for the doorway or fixture being observed.
Three facing-seat bays per car use batched window tables with recessed cup positions, support legs,
wall brackets and paired power outlets. The accessible companion bay and the toilet-replaced side
remain clear. Their core layout owns the same physical footprint used by rendering, preserving the
0.84 m aisle while preventing a decorative-only table that the player could walk through.

## Renderer translation units

`SceneRenderer` was split on 2026-10-12 from one ~9,800-line file into focused translation units
that share `src/render/SceneShared.hpp` (a private header with anonymous-namespace helpers and
constants such as `kPlatformSeatedRootDrop` and the avatar budgets):

| File | Contents |
| --- | --- |
| `SceneRenderer.cpp` | construction of shared meshes and effects, the per-frame `draw` pass order |
| `SceneAvatars.cpp` | onboard and platform avatars, platform population and lighting |
| `SceneStreaming.cpp` | world and track streaming state, staged uploads and swaps |
| `WorldGeometryBuild.cpp` | `WorldGeometry::build` and `WorldBuildContext` (chunks in parallel, then stations) |
| `TrackGeometryBuild.cpp` | the 2 km permanent-way batch |
| `TrainInteriorBuild.cpp` | the immutable train interior, glazing and window rain beads |

Reusable geometry stays in headers under `include/trainsim/render/` with unit tests in
`tests/MeshBuilderTests.cpp`.

## Oncoming traffic (ENV-137)

`World::OtherTrainConsist` (`include/trainsim/world/OtherTrainConsist.hpp`) is renderer
independent: vehicle kinds, coupled lengths, cab ends and a speed per type. An `OtherTrain`
feature keeps its motion phase in variant bits 0-1 and its type above them
(`otherTrainVariant`, `otherTrainType`); the type rotates with the service ordinal. Motion
(`movingOtherTrainDistance/Visible`), the level-crossing closure (nearest vehicle), the passing
sound and rendering all consume the same consist description. `StreamingWorld::otherTrains()` is
a traffic window eight chunks each side of the player, wider than the scenery window, because a
service travels up to 950 m from its anchor. `Render::addOtherTrainVehicle`
(`OtherTrainGeometry.hpp`) builds one mesh per vehicle kind in vehicle-local metres (+Z leading);
the renderer turns it a half turn onto the parallel track. Parked services at major stations
alternate between the regional and diesel units.

## Time of day and weather (ENV-138/139)

- `Sim::Environment` (`include/trainsim/sim/Environment.hpp`): `serviceClockHours` from the
  configured start and simulated service seconds, `parseClockTime`, weather modes and continuous
  `WeatherConditions` (cloud cover, overcast, rain, fog), including the deterministic changing
  itinerary. Settings keys `service_start_minutes` and `weather`; CLI `--time`, `--weather`.
- `Render::Atmosphere` (`Atmosphere.hpp`, header only): `sunPosition` (summer arc calibrated to
  the old fixed sun at 09:17), `sunRayDirection`, `moonPosition`, `shadowCastingPosition` (at
  least 25 degrees), and `atmosphere(hours, weather)` with exterior light, sky palette (0..255
  units so the reference gradient is reproduced exactly), fog range and colour, shadow strength,
  sun/moon disc visibility, cloud tint and the night factor; `stationLighting` adds floodlighting.
- `SceneRenderer::draw` computes the atmosphere once per frame. It rebuilds the streamed sky mesh
  (`addAtmosphereSky`: gradient, twilight glow toward the sun, stars) only on a visible palette or
  glow-direction change, draws the sun and moon discs, repeats the cloud layer under heavier
  cover, fades the baked shadows and window sunlight patches, scales the water reflection, tints
  the unlit leaf-spray effect and floodlights the platform material after dark.
- World builds bake projected shadows from the renderer's whole-degree `worldSun_`.
  `WorldMaterial::NightGlow` (`NightLighting.hpp`) holds lit windows (two in five rooms, seeded per
  building), lamp heads and light pools, drawn additively with alpha = night. Oncoming vehicles
  get additive night overlays made by `nightGlowOverlay` from their own glazing and lamp faces.
  Signal and crossing lenses draw unlit after dark.
- Rain (`WeatherGeometry.hpp`): two half-tiles of crossed streak quads outside a 2.4 m corridor,
  repeated around the camera in train-local metres, scrolled by fall speed and train speed and
  sheared along the relative wind; the platform half is skipped beside a station. Beads on the
  outer face of every saloon window are drawn after the glass while it rains. Audio plays
  `rain.wav` with the same rain level.

## Rendering and avatars

The new `--passenger-view --car N` inspection preset selects an occupied seat and frames it
obliquely from the real aisle, after normal car spawn initialization. It is mutually exclusive
with other view presets. Its first usable front-quarter capture exposed a deeper seated-pose
error: the body faced the seat back while the procedural knees faced the opposite way. Read-only
CNA inspection confirmed the public avatar coordinate convention is -Z forward. Seat-row yaw
and hip/knee bend signs are now corrected together; the seat-facing regression verifies the
native forward vector for alternating rows. A second 180-frame capture visibly aligns faces,
torsos and knees and shows hands in front rather than behind the back. Core/population/interaction
targets pass (3/3, 23.64 seconds). A subsequent fitting pass uses 1.55 rad hips with opposite
knees and a 10 cm forward visual pelvis offset weighted by seated blend. Navigation and seat
ownership remain unchanged. The inspected 180-frame capture clears the cushion front with the
shins; foot-floor contact still needs proportion-aware fitting. `avatarFacingDirection` converts
onboard walking, driver and social/player attention targets to the same -Z forward convention.
Its regression reconstructs forward vectors for eight cardinal/diagonal directions. Four focused
core/render/population/interaction targets pass (18.82 seconds). Platform facing and every
reading/social gesture are not validated by these checks. Earlier rear-view captures were
insufficient to discover this bug.

Seated window-looking no longer changes the avatar's whole-body yaw by ninety degrees. Seat-row
orientation remains authoritative for the pelvis and legs; a bounded relative window gaze drives
the existing upper-back/head layers instead. This avoids rotating seated knees into the side
armrest. A regression checks both windows across six alternating rows and bounds gaze angles.
Core, population and interaction targets pass (3/3, 23.24 seconds), and a 180-frame saloon capture
was inspected. The capture still shows crossed-hand standing-idle influence on a seated avatar;
neutral seated arm placement was investigated separately from this yaw correction. Read-only
inspection of CNA's animation authoring source confirmed Stand1 clasps hands behind the back.
Seated avatars now blend shoulders, elbows and wrists toward a project-owned front-rest base
before reading, phone and conversational overrides. Native joint translations remain unchanged.
The first capture exposed excessive lateral spread; corrected rotation signs remove that spread
and the visible behind-back clasp in a second 180-frame capture. Render and interaction checks
pass for the final pose; the earlier population smoke also passed, before the sign adjustment.
The inspected view is predominantly from behind, so exact palm/thigh contact and male/female
body proportions still need side/front pose inspection. This is not full seated IK.

Townhouse facades use a shared storey layout across their passenger-facing and end walls. The
three height variants have three/four/four storeys, with nominal floor pitches between 2.6 and
3.1 metres before building scale. Window centres distribute those storeys over the wall height;
1.35 m portrait panes replace the former 0.86 m front panes. Planters follow the actual second-row
sill instead of a fixed height. Unit checks cover all variants, scaling, top-frame clearance and
ground-floor clearance. Footprints, roof heights, entrances and shadow envelopes are unchanged.

Townhouse street placement now reserves the full 4.3-times-scale parcel half-width rather than
the 3-times-scale wall half-width, keeping the entire plot beyond the 25 m street setback. Near
Medium/High parcels add a low masonry-and-metal front boundary and bounded soil/planting strips.
The 1.84-times-scale opening follows the alternating door offset and clears its 1.42-times-scale
entrance path. Geometry is mirrored on both route sides, rests on the fixed 64 mm plot top at
every building scale, and remains within the existing parcel. It adds fewer than 400 triangles
per eligible building to the existing world batch; Low and buildings beyond 500 local metres
omit the detail. Unit checks cover both mirrored sides, entrance clearance, footprint, triangle
budget and height alignment; streaming tests reserve the plot instead of merely the wall.

`StreetLayout.hpp` selects at most one public street court per chunk side from sufficiently wide
gaps between townhouse parcels. Selection is deterministic and order-independent; conservative
feature bounds reserve nearby buildings, vegetation and infrastructure. The renderer additionally
excludes station approaches, bridge/tunnel/water exclusions, steep receivers and nearby features
in neighbouring chunks. An 8 x 12 m paved rectangle connects to the outer pavement and contains
two simple facing benches and two low planted beds at the rear edge. Its 434 triangles share
the opaque world batch; Medium/High only. Planting leaves the central circulation area clear.
Paving follows sampled rendered terrain and reverses winding on the opposite route side. Bench
furniture uses the local route heading and a nearly-flat receiver restriction, not full foundation
levelling. `urban_street_pockets` reports the loaded count. Placement also reserves the rendered
entrance-path widths/offsets of houses, townhouses, apartments and towers, including houses whose
walls are farther away than the court's collision bounds. Opposite-side paths and skyline buildings
without a road connection do not exclude the court. General pedestrian navigation and district-wide
block planning remain future work; this is a bounded infill layer.

Medium/High residential scenery supplements contact patches with a bounded, subdivided projected
ground silhouette for each house, townhouse and apartment block. The convex outline joins the footprint to a roof-height
offset using the fixed daylight direction ratios (0.42/-0.84/0.34); sampled terrain heights and a
small depth bias place it above the ground. It shares the existing transparent shadow batch and
adds no draw calls; Low omits it. A km-2.6 production capture shows restrained side/rear ground
shadows, and five render/urban/High/Low/material targets pass (5/5, 9.40 seconds). This is explicitly
an approximation, not shadow mapping: roofs are simplified, terrain tessellation can differ from
analytic samples, and buildings/vehicles cannot receive these shadows. Grazing-angle terrain
contact needs further audit before increasing opacity or extending this to tall towers.

Apartment slabs share a `ResidentialEnvelope` between opaque construction and projected ground
shadows, including all four height variants and the existing fixed 0.5 m roof thickness. Their
own raised forecourt is a receiving surface, just like house/townhouse plots. This extension adds
no material or draw batch. Tiered towers and compound warehouses still have contact patches only;
their geometry and receiving yards need a separate treatment rather than a mismatched residential
roof approximation.

Apartment facade rows now follow the same four height variants (five through eight storeys),
on both passenger-facing and end walls. Previously every slab had only five rows, leaving the
tallest variant with roughly seven metres of blank wall above its windows. Geometry tests cover
all variant residues at three scales and keep the top frame below the roof with a bounded margin.

`Daylight.hpp` now supplies one absolute-world sunny ray direction, transformed into any local
route heading. Both main scene effects update their light direction in the current train frame;
projected silhouettes use the same direction in each building's route frame. The visible sun is
placed opposite that ray and its two-sided disc is oriented toward the viewer, replacing the
unrelated fixed sky position and oversized radius. A regression transforms five local headings
back to absolute coordinates and checks direction and unit length. Six focused render, urban,
High/material, tunnel and terminal-reversal targets pass (6/6, 7.08 seconds; 29 render unit cases).
A matching urban capture was inspected, but the sun disc itself is outside that side-window view.
Avatar lighting now follows the same main-light direction for onboard and platform draws. Cabin
figures retain cool interior ambient fill while the existing smooth tunnel mix reduces directional
sunlight and blends it toward weak overhead illumination. Platform draws explicitly restore their
outdoor settings because they share the avatar pool. A unit regression checks clamping, normalized
directions and daylight-to-ceiling endpoints. A 180-frame populated car-2 capture was inspected;
four focused render/population/tunnel/station-exchange targets pass (4/4, 30.83 seconds). The cabin
transition is an approximation without per-window occlusion; authored interior sunlight patches
still use separate geometry. This does not establish physically correct lighting for the entire
scene or improve the stylized avatar anatomy and pose limitations by itself.

Rendering uses CNA `BasicEffect`, flat face normals, warm directional daylight, batched streamed
meshes, distance culling, subtle fog, a low-cost unlit gradient sky cylinder, and a small pool of
CNA avatar
visuals. The sun and passenger information pixels use an unlit pass, while the train and world use
the lit vertex-colour path. Up to 12 nearby passengers within 28 m of the
player are drawn, with the conductor prioritized when in range. Each visible character gets one
renderer for that frame; this avoids reusing CNA avatar GPU state within a single command stream.
The closed 32-segment, 16-ring gradient sphere costs one stable draw call and 960 triangles; the
soft-cloud atlas adds one draw and two triangles per quality-selected bank without a shader fork or
post-processing pass.
All passenger-window panes share one transparent depth-read batch. Low-opacity blue tint
keeps the glazing legible without obscuring the scenery.
Medium/High put 204 soft, transparent-edged cabin-light accents into two side-specific
transparent batches. Their draw-time alpha uses a bounded fake Fresnel term derived from camera
yaw: it is weakest when looking squarely outside and rises while looking along the pane. The base
tint is never modulated, Low omits both batches, and the effect does not sample the framebuffer or
assume an OpenGL-specific reflection API. ENV-011's paired capture replaced earlier cuboid glints
and the hard upper stripe with vertex-alpha patches, eliminating their geometric boundaries.
The 0.13–0.38 angle multiplier remains. See the visual audit for current evidence and limits.
A second small transparent batch projects 23 warm, window-shaped gradients onto the saloon floor.
Opaque seats and fixtures already present in the depth buffer mask those highlights, providing a
convincing sunny-interior cue without shadow maps. The batch fades with the authored tunnel-light
mix and is skipped once fully dark; the accessible-toilet partition excludes the floor region it
physically blocks.
Medium/High also draw one transparent batch containing 24 low-alpha reflected-light gradients:
two ceiling and two upper-wall spans in each car. Low omits it. This remains an authored daylight
approximation rather than a dynamic light source, but it breaks up the formerly uniform ambient
saloon exposure at one bounded draw-call cost.
All simulated passengers retain independent identities whether visible or culled. CNA has no
stock seated preset, so seated actors retain their varied upper-body idle while the project adjusts
hip and knee bones through CNA's public local-bone API.

The visible-passenger list, ready-renderer list, and each avatar's mutable bone-pose buffer are
retained as renderer scratch storage. After their first capacity growth, ordinary avatar frames no
longer allocate these vectors repeatedly; persistent visual-slot assignments share the same bounded
12-person onboard render budget. An assigned passenger receives a four-metre distance-order bonus
while visible, preventing centimetre-scale ordering changes from swapping a nearby identity without
allowing a distant actor to starve a substantially closer newcomer. Per-visual gait amplitude eases
in/out over 0.18/0.24 seconds, and facing yaw follows the shortest angular path at a bounded turn
rate rather than snapping when an AI state changes. Each persistent visual also retains its last
train-local actor position. The gait phase advances by actual two-dimensional travel distance,
rejects first samples and jumps above 40 cm as teleports, and stops driving the locomotion blend
when navigation is blocked or the simulation is frozen. Small ankle counter-rotation and toe roll
complete the existing hip, knee, shoulder and vertical layers. This removes time-driven foot
cycling without coupling the renderer back into passenger navigation; pure unit tests cover the
distance/phase contract and `npc_distance_gait` exposes live activation.

The same presentation boundary smooths the start of a seat exit without weakening authoritative
simulation. While `exitingSeat` preserves the source-seat reservation, the rendered avatar blends
from that seat to its already-moving train-local navigation position using the existing eased
seated-pose weight. It simultaneously turns toward the correct aisle lane and applies a bounded
lower-back weight-transfer curve. The blend reaches the navigation position before the standing
pose finishes, so collision, reservations, and subsequent path following need no renderer feedback.
The inverse seated blend reuses the bounded lower-back curve for the visible sit-down weight shift.
Pure tests cover both placement endpoints and the bidirectional transfer midpoint; `npc_seat_transitions`
exposes active visible blends alongside the distance-gait telemetry.

The same persistent visual owns smoothed local head yaw and pitch. Conversation partners derive a
bounded relative look direction from their actual adjacent seats; window-watchers scan slowly,
ordinary seated/standing riders use identity-phased low-amplitude gaze, and one deterministic
quarter of neutral seated riders bends both arms toward a reading-like lap pose. A separate
identity-bounded subset of neutral seated or standing riders raises both arms toward a small shared
project-generated handset, lowers its gaze, and renders the dark housing, blue screen, status cue,
and side key beside the CNA avatar. The handset costs one submission only for each selected visible
user and cannot affect passenger navigation or door reservations. These bone layers allocate no
per-frame storage beyond the existing scratch buffer. Render telemetry counts active gaze,
reading, and phone-check poses; the High population smoke requires all three to be visible.

Passenger construction assigns deterministic residual activity times, so a new process represents
an already-running journey rather than synchronizing every rider behind the same minimum idle.
One bounded activity branch uses the existing source-seat reservation and transfer path to settle
a regular passenger beside the row. Once stationary, the renderer pairs that actor with the nearest
unclaimed normally seated rider in the same car. The standing body faces the partner while both
heads use the persistent gaze filter and the standing skeleton receives one subdued arm gesture.
Seat-exit transfers and moving vestibule actors are excluded. This relationship remains a render
layer over authoritative collision and reservations; `npc_social` covers visible participants in
both this pose and the established seated conversation state.

Stationary riders from a deterministic subset, plus the conductor, can acknowledge the player from
0.70-2.55 m when they are neither talking nor window-watching. The upper torso follows 18% of the
head yaw and 8% of its pitch, avoiding both a disconnected neck turn and an intrusive full-body
stare. The conductor's dedicated handheld terminal combines a rounded housing, screen, status bar,
keys, and wrist loop; inspection pauses use a two-arm device pose while walking clips it at the
side. Telemetry counts both acknowledgement and the visible device.

Fresh animation poses receive a uniform 0.74 head-bone scale before submission (previously 0.82).
The local joint translation and all other bones stay unchanged; native facial parts, hair and
headwear follow CNA's hierarchy. The same helper covers onboard and platform figures. This is
an artistic proportion correction, not anatomical retopology: large eyes and stylized facial
geometry remain, and success on inspected accessories is not a catalog-wide compatibility claim.

Civilian CNA descriptions are selected from at most 128 public random candidates, not a seeded
deterministic population. `AvatarAppearance.hpp` prefers uncovered hair over hats and penalizes
bright headwear and the catalog's broad bucket silhouette independently of clothing quality.
Saturated tops/trousers have penalties larger than the combined accessory/hair bonuses; ordinary
hair cannot compensate for conspicuous primary-colour clothing. Early acceptance requires 29
points. Cream, charcoal, earth tones, muted teal and purple remain in the candidate palette.
Subdued ordinary hats and varied clothes remain possible. Skin tone, body type and facial
features are not ranking criteria. The scorer checks length, magic, format 1/2 and catalog v3
before reading documented offsets; unfamiliar descriptions receive no preference. It never
rewrites description bytes or modifies CNA assets. This is bounded best-effort selection, not
an absolute wardrobe guarantee. The selected civilian pool size is reported alongside the ready
renderer count.

Avatar visuals are still constructed through CNA's asynchronous loader, but Train Simulator
serializes the boundary between models: a renderer that has just become ready receives one culled
main-thread draw to finish its first GPU texture upload. The next background model build is not
enqueued until eight complete rendered frames have crossed CNA's ordinary presentation boundary,
and the final pool member is primed under the same rule. The bounded roughly 133 ms interval at
60 FPS replaced the original one-frame delay after a rare standalone recurrence under a heavily
loaded test run. This project-side workaround avoids a CNA
`next` race between avatar model construction and first EasyGL upload without changing the
dependency; the exact dependency finding is recorded in `bugs.md`. The one-time culled warm-up
draws are not included in steady-state diagnostics.
Runtime telemetry distinguishes allocated visuals from renderers whose asynchronous construction
has reached `Ready`; short process-lifecycle smokes can therefore prove that no loader remains
active when they deliberately exit. Finite-frame and screenshot runs freeze authoritative game
state at the requested frame and continue render-only warm-up until the quality-sized pool is idle.
Screenshot readback then uses the previous fully presented, fully populated framebuffer before CNA
is torn down. The camera, route, passengers, and interactions therefore still represent the
requested frame, while neither the PNG nor process teardown can race a live loader thread.

The spatial passenger-audio selector likewise retains its nearby and actively-talking candidate
buffers. Irregular murmur decisions therefore do not introduce periodic heap churn during a long
journey.

All scenery uses train-relative transforms. The route itself remains renderer-independent even
though the only supported and tested renderer is CNA OpenGL 3.3.

The tail of the quality-sized avatar pool reserves 2/4/6 visuals for the Low/Medium/High
station-side crowd, giving each simultaneously drawn platform person an independent CNA renderer
and appearance. Low therefore constructs only eight visuals in total instead of paying the CPU and
GPU warm-up cost of all eighteen High visuals; raising quality streams the missing renderers through
the same serialized loader. Platform visuals are used only around the nearest stop and culled
outside useful window-view range, so station life adds bounded draw cost and no persistent
route-wide NPC state. One receives a project-rendered high-visibility band to read as station
personnel. Waiting poses are deterministic rather than cloned: a major-station pair faces one another
and layer small alternating hand/head gestures over CNA's stock idle, while one train-centre
traveller carries the shared handset. The handset position is derived from the posed wrist by
walking CNA's published parent-bone hierarchy, so body-height and idle variation cannot leave the
prop floating beside the hand. Parent indices are cached once per visual; this adds no per-frame
allocation. `platform_phone` and `platform_social` telemetry are covered by both quality-budget and
High-population OpenGL smokes.

Platform conversations use authored slots 4/5 only when both are loaded. The phone user and
staff/track watchers are excluded, preventing a one-sided conversation with an occupied or
quality-culled partner. Their longitudinal separation is 1.6 m, with the existing lateral stagger;
each heading is computed from the partner's actual offset using CNA's public -Z forward axis.
The previous fixed angles faced these people away from one another across a 6 m gap. A regression
checks pair availability at all three quality ranges. This is a waiting-pose correction, not a
platform navigation simulation.

`PlatformLayout.hpp` now resolves waiting offsets against canopy supports, lamp posts, benches
and the ticket machine in station-local coordinates. The canopy length is shared with geometry
generation; the other footprints match the current authored fixtures. A 0.48 m clearance envelope
keeps bodies away from their edges. The bounded half-metre candidate search moves conversation
slots together, before quality-tier selection, and shifted anchors also move luggage and shadows.
Regression cases cover all terminal/major flag combinations and preserve the 1.6 m conversation
separation. Conversation candidates additionally clip the complete partner-to-partner segment
against the same fixture rectangles, expanded by 0.15 m. This rejects a support between two
individually clear standing positions without relying on potentially sparse point samples.
Tests cover the reproduced terminal-post obstruction, reversed endpoints and zero-length segments.
New fixture families must be added to this clearance model; this is not a general
scene collision query, nor a promise that foreground posts cannot visually occlude people.

Platform luggage shares one rounded-shell mesh with a dark zipper gasket, shallow moulded ribs,
thin metal telescopic tubes, rounded grip and four small wheel approximations. The broad face is
oriented toward the track. Prop draws explicitly reset BasicEffect diffuse colour to white after
avatar drawing: otherwise the staff member's yellow visibility band tinted later vertex-coloured
cases green. The mesh remains shared and adds no per-case materials or draw calls.
The common `drawBox` helper also saves/restores its incoming diffuse colour, preventing uniform
signal/button/indicator colours from leaking into later vertex-coloured geometry. Existing
explicit white resets remain useful pass boundaries; callers must not depend on a box draw
leaving its tint active.

The saloon uses a separate opaque unlit batch for its ceiling diffuser faces. Six cars share one submission;
their recessed aluminium channels remain in the lit interior batch. The five gangway header
diffusers and single WC ceiling panel use that same emitting batch with separate metal frames.
Two-metre diffuser sections
have narrow service joints, and their warm-white output no longer darkens with sun direction.
This is an emissive-surface approximation, not new dynamic lights or bloom; the existing local
ambient fill and reflected-light gradients still approximate illumination of the surrounding cabin.

The saloon uses two opposed, downward longitudinal diffuse fills to approximate the broad ceiling
strips. They illuminate both seat orientations without adding point highlights; the sun contribution
is reduced to avoid doubling the illumination on upward-facing surfaces. Opaque and textured
interior batches share this setup. Both additional BasicEffect lights are disabled at the start of
every frame before exterior rendering. This is a three-direction approximation, not area lighting,
occlusion or a linear-light rendering pipeline; it adds no mesh submissions or shadow maps.

Exterior sunlight and fog cross-fade at authored tunnel portals. The saloon lighting is restored
before the train interior pass, keeping the passenger space and its unlit information displays
readable while the world beyond the windows becomes appropriately subdued. Tunnel ceiling strips,
wall luminaires, low reflective markers, cable trays, and the illuminated faces of 100 m emergency
signs share one separately unlit streamed batch. Raised evacuation walks, pale inner edges,
structural expansion joints, framed service doors, and red emergency cabinets stay in the existing
static world batch. This robustly approximates local lighting and tunnel safety infrastructure with
one draw call beyond the ordinary world instead of a long chain of point lights. Production
diagnostics count generated tunnel-luminaire samples, individual safety niches, and bridge chunks
while the audio layer exposes its smoothed tunnel and bridge mix state. Paired offscreen OpenGL
smokes begin inside the long tunnel and river bridge and require the expected streamed geometry
together with the matching CNA audio layer, while also requiring the unrelated acoustic layer to
remain silent.

## Validation strategy

Eighty-one core and nine renderer-side deterministic headless cases cover route bounds and sampling,
crossing identity, arbitrary route-distance
QA starts, the quantified passenger-
consist curve tolerance, segment limits, coasting,
timetable drift, multiple service reversals, terminal door-side continuity, platform-side state,
passenger and player seat invariants, toilet ownership, visible platform-side exchange, a
continuous reserved-seat walk without position jumps, blocked window-seat access, a 20,000-second
passenger soak with a stable two-identity offboard reserve, complete
conductor traversal, finite configuration values and atomic
user-preference persistence,
deterministic chunk generation, near/distant rural vegetation coverage, bounded streaming, and
finite faceted-ellipsoid geometry. Automated runtime smokes use CNA's SDL
offscreen video driver and dummy audio driver; accelerated runs exercise many GPU mesh replacements
and station transitions in a short wall-clock interval. Three of those cases directly validate
that procedural quads, boxes, and horizontal discs produce finite unit normals for CNA's lit vertex
path.
One coupled service soak additionally drives the real route state, passenger AI, platform-side
exchange, active driver cab, and seven-chunk streaming window together through three terminal
reversals (A-B-A-B). It checks every arrival interlock, at least 28 full four-person station
exchanges (allowing a bounded number of naturally occupied doorways), bounded chunks, correct
platform sides, and passenger ownership invariants throughout.
A temporary GCC 14 Debug build ran all 81 core cases with AddressSanitizer and
UndefinedBehaviorSanitizer configured to halt on the first error; the complete suite passed.
LeakSanitizer itself cannot operate under the development harness's `ptrace` supervision, so its
unsupported leak phase was disabled rather than mistaken for a project failure. Bounded RSS in the
rendered multi-reversal soaks below remains the long-duration retention check.
A separate 1,000-frame OpenGL 3.3 soak starts at the Redwater bridge approach and advances 180 km
through the Lakehurst shore, two rural calls and the long tunnel. In the validated Low-profile run
it crossed 725 generated chunk instances while retaining exactly seven active chunks, all 38
characters and the two-identity offboard reserve; it exited without swap at a 417 MiB peak RSS.
After the endpoint-window and two-wire catenary pass, a fresh 600-frame accelerated OpenGL soak
advanced 88.9 km through three station legs and 362 generated chunk instances. It retained seven
active chunks, all 38 identities, the two-person offboard reserve and 140 nearby droppers, exiting
cleanly at a 398 MiB peak RSS.
Another validates the complete distributable WAV manifest, its RIFF/WAVE headers, and four unique
surface-footstep fingerprints.
Route-aware announcement tests then cover both welcome destinations, both terminal reversals, and
both station phrasings for all twelve stops. The bitmap-font suite also guards the comma used by
the synchronized terminal caption so unsupported punctuation cannot silently fall back to `?`.
Three dummy-mixer OpenGL smokes complement those pure checks: both service directions must start a
project-generated PA chime followed by a destination-correct welcome instance, and a moving
approach must complete the same non-overlapping chime-to-speech handoff before its live caption
names the authoritative next station.
An offscreen OpenGL application smoke test additionally exercises the real context-interaction
path for display inspection, sitting, standing, toilet entry, conductor information, bidirectional
walking through all six cars and five gangways, both physically blocked driving-cab limits, and the
explained remain-aboard boundary at an actually open platform-side passenger door.
The same smoke creates genuine keyboard-driven carpet and vestibule steps, verifies successful
movement, surface selection, and idle reset, and runs once through the dummy audio mixer so both
footstep one-shot paths are covered without depending on physical audio hardware.
A dedicated deterministic-position smoke starts beside a generated opposing train and requires the
synchronized spatial pass-by event to reach the dummy mixer.
A paired visual smoke reserves a real left-side window seat in the first car at the same pass,
requires all three articulated bodies and both cab ends, and guards the negative train-local camera
position and its 90-degree view toward that window. The generic `--left-window-view` option makes
that side available to other QA captures without creating an exterior spectator camera.
A second station-start smoke keeps production dwell timing, advances the real CNA application into
the boarding window, and requires that all four bounded exchange participants were launched in
`FINAL_STATE`, independently of whether an actor completed before a slow offscreen capture.
A reverse-direction smoke starts at Eastmere with the return service active and requires route km
247.600, direction -1, and all four exchange actors in the real application. Core coverage pairs
this with a reverse moving approach and guards the upcoming physical platform side on every route
segment in both directions.
Another runtime smoke starts on the final Westhaven approach, enters the terminal, completes the
dwell and direction change, and then requires one completed leg, the reverse direction, the driver
in car -1, all 38 identities, the two-person offboard reserve, and exactly seven loaded chunks.
The route-map smoke launches a moving service with the overlay already visible and requires the
reported map state and normal passenger HUD to remain visible, pause state to remain false, and
route advancement to be strictly positive. This guards `M` as a live journey reference rather
than an accidental pause menu.
A separate clean-view smoke requires the HUD to begin hidden while the map remains closed and the
service remains live.
A platform-view smoke starts at left-side Hawthorn Park, reserves a production window seat, and
requires the reported player position to be on negative train-local X. It also requires the
authored 62-piece paving-joint grid and three canopy light panels to survive static batching. Core
coverage separately
requires terminal render offsets to remain distinct beyond both endpoint datums. OpenGL captures at
Hawthorn Park and reverse-direction Westhaven confirm inward-facing text order, readable live boards,
spaced canopy furniture, and platform avatars on the correct side.
An accelerated-dwell smoke deliberately interrupts all four doorway actors, then requires the real
application to report zero remaining exchange actors and the restored two-person offboard reserve.
A separate Low-quality runtime smoke requires exactly six drawn onboard avatars from an eight-visual
pool, guarding both the render LOD and the reduced fresh-process avatar construction budget.
F3 and `FINAL_STATE` derive FPS from completed `Draw` calls over a steady-clock half-second window,
not from CNA's fixed simulation step, so diagnostics remain truthful when rendering falls behind.

Fresh-process accelerated OpenGL soaks after the door and road-detail pass generated 259, 522, and
592 route chunks while retaining exactly seven at every endpoint. Their peak resident sets were
468, 478, and 473 MB respectively on the development Mesa desktop, so memory did not grow linearly
with streamed distance. The longest sample completed four station legs, reversed direction, and
continued cruising with valid signals and passenger state.

An additional 720-frame audio-enabled soak after announcement captions and retained avatar/audio
scratch buffers completed seven station legs, generated 668 chunks while retaining seven, and
finished with valid passenger and signal state. Its 503 MiB peak also includes the complete decoded
WAV set and software Mesa renderer.

The protected-crossing audio pass then ran 360 OpenGL frames from km 7.0 at accelerated service
time. It advanced 43.2 km through two complete station legs and 179 generated chunks while retaining
seven, emitted six spatial crossing-warning and two passing-train events, completed four door-audio
transitions, used no swap, and peaked at 422 MiB under software Mesa.

After adding the batched power corridors, substations, and live platform board, a 240-frame
high-quality accelerated soak generated 153 chunks, retained seven, completed a station leg, and
peaked at 421 MiB without audio. Long conductor spans therefore remain bounded by the ordinary
chunk lifecycle rather than accumulating route-wide geometry.

The automatic gangway-door audio pass was followed by a 360-frame medium-quality, audio-enabled
accelerated run. It completed two legs and 60.8 route kilometres, generated 247 chunks while
retaining seven, and peaked at 455 MiB under software Mesa plus the dummy audio mixer. The shared
player/NPC door sensors, one-shot spatial audio, announcements, and streaming remained valid.

After the tunnel-lighting, open bridge-rail, and terrain-feature exclusion pass, a 360-frame
high-quality audio-enabled soak began at route km 280, crossed Westhaven Central, reversed, and
continued for three station legs to km 216.6. It generated 308 chunks while retaining exactly seven,
kept all 36 actors valid, and peaked at 465 MiB under software Mesa plus the dummy audio mixer.

After terrain parcel/ridge expansion and full seated-camera interpolation, the same 360-frame
high-quality terminal-crossing profile continued for five station legs to km 150.7 in the reverse
direction. It generated 572 chunks while retaining exactly seven, ended with an active reserved-seat
walker, valid gangway and passing-train audio events, and peaked at 498 MiB under software Mesa plus
the complete decoded audio set.

After serializing each avatar's first GPU upload ahead of the next asynchronous model build, twelve
consecutive fresh-process 70-frame station runs initialized all onboard and platform avatar visuals,
saved screenshots, and exited without allocator damage. A subsequent 360-frame high-quality,
audio-enabled terminal-crossing soak completed four station legs, generated 419 chunks while
retaining seven, kept all 36 simulated actors valid, fired a synchronized passing-train event, and
peaked at 479 MiB.

After adding broad hedged parcels, distant woodland, faceted foliage, and the quality-sized avatar
pool, a 360-frame High terminal-crossing soak completed five station legs to km 145.6 on the return
run. It generated 592 denser scenery chunks while retaining seven, kept 36 simulation actors and all
18 High avatar visuals valid, emitted three synchronized passing-train events, and peaked at 515 MiB.
The deliberately extreme 500x time scale crossed more than one chunk per rendered frame; ordinary
service speed rebuilds the static batch only about once every 5.7 seconds at 160 km/h.

A 300-frame visible-window desktop measurement of the same denser countryside used High quality,
12 drawn onboard avatars, 96 draw calls, and about 203,000 triangles. The steady-clock diagnostic
reported 59.9 FPS; the complete fresh process took 5.7 seconds and peaked at 364 MiB without audio.
The SDL offscreen path reached only about 29 FPS with either 170,000 Low or 205,000 High triangles,
confirming that its software/presentation path—not scenery geometry—was the limiting factor. Low's
quality-sized eight-visual pool nevertheless reduced the comparable fresh-process peak to 299 MiB.

After the HVAC, surface-footstep, and avatar-test-isolation passes, a fresh 240-frame visible High
desktop launch ran the production audio mixer, started the synchronized welcome announcement,
prepared all 18 CNA avatar visuals, drew 16 nearby people, and reported 59.6 FPS. Its initial
terminal scene held 123 draw calls and about 199,000 triangles, confirming that the complete normal
launch path remains interactive rather than merely passing the software-offscreen harness.

The subsequent interior-material pass added six visibly jointed resilient vestibule floors, five
existing metal gangway plates and the accessible-toilet floor to one tested 12-zone surface model,
plus lower-wall joints and seat upholstery piping. All detail remains in the existing immutable
interior batch. A fresh visible High launch still held 123 draw calls, prepared 18/18 avatar
visuals, drew 16 people, rendered about 209,000 triangles, and reported 59.7 FPS.

The saloon runner pass added two woven-edge strips and ten transverse bindings per car: 72 tested
inlays across the six-car consist, all folded into that same immutable batch. A fresh visible High
production-audio launch prepared all 18 avatar visuals, drew 17 people, held 59.9 FPS at 127 draw
calls and rendered about 211,000 triangles while four station-exchange actors were active.

The Lakehurst far-bank pass replaced the former sapling-sized dots with a deterministic,
quality-scaled woodland silhouette: Low keeps one mature tree on alternating 50 m water strips,
while High staggers four conifer/deciduous crowns on every strip. A production-audio 240-frame
visible High ride beside the lake retained 59.7 FPS with all 18 avatar visuals ready, 11 nearby
people drawn, 95 draw calls, about 235,000 triangles, 35 level-water patches, and 140 bank trees.
The same static chunk batch carries water, reeds, glints, terrain, and the new skyline.

After correcting the Northbridge city envelope, footprint-aware building setback, and terminal
apron/skyline pass, a 240-frame High accelerated run started at km 185 and crossed Northbridge plus
Redwater. It completed two station legs, generated 210 chunks while retaining exactly seven, ended
with about 203,000 triangles and all 36 actors valid, and peaked at 459 MiB without audio under the
software Mesa path. The denser city approach therefore remains inside the established bounded
streaming working set.

A seated-window High capture at the first generated road overpass rendered its terrain-reaching
approach, deck, piers, guardrails and lane markings together with the surrounding countryside at
86 draw calls and about 208,000 triangles. The complete structure remains part of the single static
world batch and therefore adds geometry but no scenery draw call.

The first streamed farmstead was likewise inspected from a production window seat at High quality.
Its barn, silo, house, hay store, yard and post-and-rail perimeter remained legible at speed while
the complete scene held 86 draw calls and about 205,000 triangles; the compound adds no new batch.

After the denser ten-metre tunnel-light cadence and two continuous guidance strips were added, an
audio-enabled 240-frame High run started at km 120, traversed the complete 5.8 km long tunnel, made
the Stonebridge stop, and continued to km 138.7. It generated 81 chunks while retaining seven,
kept all 36 actors valid, emitted a synchronized passing-train event, and peaked at 472 MiB under
software Mesa plus the decoded audio set.

After replacing station identity recycling with the authoritative two-person offboard pool, the
20,000-second core soak completed one hundred shortened exchanges while returning the reserve to
exactly two and preserving more than twenty occupied seats. A separate 300-frame OpenGL run crossed
Stonebridge under deliberately accelerated dwell timing, generated 89 chunks while retaining seven,
and departed with all 38 identities valid, no active exchange actor, and exactly two offboard
passengers. This covers both sustained production logic and the harsh early-door-close recovery.

The reverse-direction Eastmere validation was also reached from a moving start 800 metres beyond
the station and captured from a real OpenGL frame. The onboard display had already updated to
Alderwick Central and Redwater, with all 38 identities valid, four active exchange actors, seven
bounded scenery chunks, and 81 draw calls at Low quality. A separate early-door-close regression
confirms that accelerated development timing rolls an incomplete exchange back without consuming
the two-identity offboard reserve.

After the teleport reset, interrupted-exchange rollback, and accessible-toilet material pass, a
480-frame audio-enabled 500x stress run began at route km 280, crossed Westhaven Central, reversed,
and completed three station legs to the Northbridge approach. It generated 399 chunks while
retaining seven, kept all 38 identities valid with the two-person offboard reserve intact, emitted
two synchronized passing-train events, and ended with exactly the two expected alighters preparing
during `COASTING`. Peak resident memory was 467 MiB under the software Mesa/dummy-audio path; the
route, passenger preparation, and streamer therefore remained coherent despite advancing 2,734
simulated seconds in 23.6 wall-clock seconds.

After the bounded city-streetfront and road-overpass passes, a 360-frame Medium 500x run again
started at route km 280, crossed Westhaven Central, reversed, completed three station legs, and
continued to km 201.9. It generated 367 chunks while retaining exactly seven, kept all 38
identities and the two-person offboard reserve valid, placed the driver in reverse cab -1, and
ended with an active reserved-seat walker and gangway sensor. The final streamed scene used 85 draw
calls and about 199,000 triangles; peak resident memory was 436 MiB under software Mesa without
audio, so the additional architecture did not introduce route-distance memory growth.

After farm compounds were added, a 300-frame Medium 500x run departed Alderwick, completed two
station legs and reached km 62.2 while repeatedly streaming the new pitched-roof geometry. It
generated 252 chunks while retaining seven, kept all 38 identities and the two-person offboard
reserve valid, and ended in a normal braking approach with two prepared alighters and an active
seat walker. The scene held 96 draw calls and about 202,000 triangles; peak resident memory was
439 MiB under software Mesa without audio.

The ribbed-gangway pass was inspected from the production player camera in both open countryside
and the long tunnel. The automatic leaves opened from their normal player sensor, the bellows and
mounted rails remained readable under tunnel attenuation, and the full six-car traversal smoke
still passed. The daylight frame used 92 draw calls and about 213,000 triangles; the five detailed
connections add no submission beyond the existing interior batch.

After correcting platform width, the production platform-window path was captured at Greenford
Vale with a live four-actor exchange. The measured train-body-to-coping gap is 0.20 m, both surface
height and gap invariants pass, and all furniture, the live departure board, and the bounded CNA
platform population remain on the widened surface. Coping, warning line, tactile band, and tile
joints share the existing static scenery batch; the High station frame remained bounded at 118
draw calls and about 219,000 triangles. A second production threshold capture verified the dark
anti-slip bridge plate, wider yellow lip, platform context, and remain-aboard prompt; its state
reported exactly six deployed correct-side steps at 122 draw calls and about 219,000 triangles.
The subsequent platform-material pass added a 3 m modular slab grid, three longitudinal seams,
framed canopy edges, and quality-independent linear underside panels to the same static world
batch. A production-audio 240-frame visible High stop at Stonebridge retained 59.8 FPS, prepared
18/18 avatar visuals, drew 15 nearby people during a live four-actor exchange, and reported 125
draw calls with about 241,000 triangles. The stop exposed exactly 62 paving joints and three
canopy panels without increasing submission count for the new geometry.
The current complete 51-test CTest suite includes three explicit CNA avatar tests with full runner
exclusivity because the current dependency avatar-loader lifecycle can fail under overlapping
fresh renderer-process pressure, even if the neighbouring process does not construct avatars.
Those tests require all eight Low visuals, all 18 High visuals, a
double-digit visible population, and correct pause/resume teardown. Unrelated deterministic OpenGL
smokes use the project-owned `--no-avatars` QA mode and remain parallelizable with each other;
ordinary gameplay
and real showcase captures retain the complete avatar population. The dependency limitation and
workaround are recorded in `bugs.md`. The Low lifecycle test also requests a PNG and requires its
successful save only after all eight visuals are ready. A separate CRC-aware PNG check covers every
committed showcase capture. After the current car-role, interior-lighting, station-family, NPC,
ridge, vegetation-silhouette, seat-furniture, and window-reflection batches, the entire 51-test
suite passed in 108.16 seconds with four workers, including both exclusive long-running smokes in
their warmed full-suite positions. After industrial compounds and their delivery vehicles, the
same 51-test suite passed in 102.74 seconds with four workers. After the subsequent tower-end
window, robust mouse-focus capture, and passenger phone-idle work, all 51 tests passed again in
123.49 seconds with four workers on 2026-10-07. After the platform-crowd/board and industrial-
utility passes, the complete suite remained green at 100.97 seconds with four workers. After the
distance-driven gait and foot-roll pass, all 51 CTest targets passed again in 121.55 seconds with
four workers. After adding the source-seat anchored stand-up transition and its telemetry, the
same complete suite passed in 107.07 seconds with four workers; the core binary reported 97/97
unit cases green. After the complementary sit-down weight transfer, procedural soft-cloud pass,
understorey/field coverage, reflection-artifact correction, platform boundary, and close furniture
hardware pass, all 51 CTest targets passed in 98.35 seconds; the expanded binaries reported 99/99
core and 23/23 render cases green.
After the rounded-luggage, cab-control, lineside-equipment, passenger-social, and saloon-junction
passes, all 51 targets passed in 131.42 seconds with four workers; the binaries reported 100/100
core and 24/24 render cases green. The longer wall time reflects the intentionally exclusive CNA
avatar lifecycle smokes rather than a simulation regression.
After canopy drainage, point mechanisms, trailer, urban road detail, and industrial edge-planting
passes, the complete suite remained green at 51/51 in 86.24 seconds with four workers. The later
station-facade weathering pass kept its focused Low/High/material-capture checks green.
After residential plot/entrance surfaces, mirrored urban streets, distributed traffic and
vegetation road clearance, all 51 targets passed again in 95.27 seconds with four workers; the
core and render binaries remained at 100/100 and 24/24 cases. The subsequent turnout blade/frog
and rounded road-vehicle passes retained focused High OpenGL, material-capture and render
coverage, then the complete 51-target suite passed in 109.53 seconds with four workers.
The subsequent shared XZ-rounded-box culling correction passed 25/25 render unit cases and all
seven focused CTest targets (render, interactions, urban traffic, interior sunlight, interior HVAC,
High MSAA, and material-capture integrity) in 3.95 seconds with four workers. Production offscreen
OpenGL captures at urban km 2.3 and inside car 2 were inspected: far-side tyres no longer show
through the car body, and saloon furniture remains visible. These captures are not a physical
mouse/keyboard play session and do not establish that the reported intermittent camera issue is
resolved.
A separate visible High-quality hardware run exercised the production collision path through all
six cars in both directions, sitting/standing, the toilet, conductor and NPC clearance, carpet and
vestibule movement, cab blocks, and the open platform boundary. It passed at the 60 FPS cap with
8x MSAA, 101 draw calls, and 430,874 triangles. This deterministic run does not replace the still
outstanding physical raw-mouse/focus-loss walkthrough.
Paired accelerating/braking service checks require opposite signed suspension responses and full
matching audio effort. A third launch requires exact zero pitch/offset from the same acceleration
when the reduced-motion preference is selected.
A long real-audio run covers a terminal arrival, direction
reversal, deferred final-stop announcement, natural passenger-murmur cooldown, and continued
streaming on the return leg. Three route-local runs require tunnel luminaires and safety niches plus
tunnel audio at km 123.0, curved steel river-bridge sections, 24 level water strips, both loaded
abutments and bridge resonance at km 74.5, and curved concrete viaduct sections, one loaded
abutment and bridge resonance at km 177.9. The unrelated geometry counts and acoustic layer must
remain silent in every case. The longer tunnel sample also requires all window-sunlight patches to
have faded and been skipped. The 210-frame recovery smoke has bounded 60-second headroom for loaded
desktops.
Separate left- and right-platform threshold captures guard the bridge-plate rotation. A moving
122.6 km/h `--route-km` regression additionally requires zero deployed steps on the first rendered
frame, preventing the renderer's normal station-start pose from leaking into a mid-route QA start.

A subsequent 7,000-frame Low-profile rendered soak ran the complete application for 25,210
simulated seconds. It completed 35 station legs and three terminal reversals, rebuilt 3,775
deterministic chunks while retaining exactly seven, and ended with all 38 identities, the
two-person offboard reserve, two active seat walkers and the reverse-cab driver coherent. This
crosses the complete corridor in both directions more than once rather than validating reversal
only from a near-terminal launch.

The first realism-audit material pass was followed by a fresh 1,200-frame Low-profile soak from
route km 72 at 500x service time. It advanced 208.1 km over 5,774 simulated seconds, completed
eight station legs and a terminal reversal, rebuilt 883 deterministic chunks while retaining
exactly seven, preserved all 38 identities plus the two-person offboard reserve, and ended with
the reverse cab active. The new texture owners and repeatedly replaced water, terrain, shadow,
and station batches exited cleanly at a 294,936 KiB peak RSS with no swap.

After adding independently replaceable masonry and polished-rail batches, a 600-frame Low-profile
soak advanced another 123.2 km at 500x time, completed five station legs, generated 499 chunks
while retaining seven, and preserved all 38 identities plus the offboard reserve. The run exited
with no swap at a 321,192 KiB peak RSS, providing a focused lifecycle check for the two new meshes
across hundreds of replacements.

After the water-band, track-fastener, facade-family, and avatar-continuity pass, another 600-frame
Low-profile OpenGL soak started at route km 72 and advanced 113.5 km at 500x time over 2,968
simulated seconds. It completed four station legs, generated 460 chunks while retaining seven,
kept all 38 passenger identities plus the two-person offboard reserve coherent, and exercised
three lane changes, one player-yield response, one conductor check, and an active seat walker. It
exited without swap at a 356,608 KiB peak RSS.

The rural-verge and parcel pass received a focused 300-frame Low-profile OpenGL soak from route km
52. It advanced 86.9 km at 500x time over 2,306 simulated seconds, completed four station legs,
generated 354 chunks while retaining seven, and ended with active drain, verge, and field-boundary
geometry after repeated world-batch replacement. All 38 identities, the two-person offboard
reserve, a seat walker, three lane changes, one player-yield response, and a conductor check stayed
coherent. The process exited with no swap at a 342,000 KiB peak RSS.

After the horizon, end-wall, catenary and platform-equipment pass, the two heaviest exclusive
smokes were profiled directly rather than shortened. The 90-frame High avatar showcase completed
in 21.1 seconds with all 18 visuals ready and a 503,188 KiB peak RSS. The 540-frame Low terminal
audio/reversal soak completed in 31.1 seconds with terminal announcement, passenger murmur and
multiple service legs at a 388,720 KiB peak RSS. Neither used swap. Their 45/90 second CTest limits
retain full coverage while allowing for the measured warm-llvmpipe slowdown after a complete run.

Visible desktop input validation complements those fixed-frame soaks. An isolated X11 session sent
window-system keyboard events through CNA while the OpenGL 3.3 application remained windowed: continuous walking
covered cars one through five and four gangways, a second start covered the fifth gangway and all of
car six up to the exact rear staff-cab boundary, and separate runs exercised stand/sit, two toilet
door interactions, exit from the toilet, and a three-second attempt to walk through an open
platform door. High quality retained all 18 avatar visuals throughout. That pass also exposed CNA's
global F9/F10 renderer-debug bindings, so the application quit key moved to F12 and the dependency
collision is recorded in `bugs.md`. A later live run approached the conductor, recorded one
player-yield response and passed on the opposite shoulder before reaching the front cab boundary.
That earlier session did not deliver synthetic XTest pointer motion to SDL's raw-relative path;
it is not presented as a live mouse result. The newer private-display harness described above
does deliver nonzero relative motion, with native capture flags and changing rendered views
verified, but still does not certify physical hardware or the desktop compositor.
The mouse regression also feeds 240 identical
relative samples through the project decoder and requires every sample to remain a delta. A second
policy regression covers active/inactive focus, project/native mode disagreement, overlay release,
and reacquisition decisions. The actual game explicitly disables native relative mode on focus
loss before requiring a fresh click; acquisition and unexpected-loss counts are both reported.

The normal compiler warning build is clean in project code; the remaining pedantic `__int128`
message originates in the immutable sharp-runtime checkout. Focused Clang-Tidy runs accompany
changed translation units. Its broad opt-in analyzer profile is intentionally not described as a
zero-warning gate: it emits review-only diagnostics for bounded procedural floating-point loops,
same-typed coordinate/geometry parameters, and integer division used for pixel-centred UI. Genuine
signed-index conversions and a redundant trivially-copyable settings move found during that review
were removed rather than hidden with blanket suppressions. Dependency sources remain untouched.

A stripped install tree was also moved to a second temporary prefix and launched from an unrelated
working directory with OpenGL and audio enabled. Dynamic inspection resolved `libcna` and both SDL
runtime libraries inside the moved prefix, while runtime discovery loaded its `share` assets. This
guards the distributed experience against accidental build-tree or source-tree dependencies.

The external-EMU cab pass was inspected from a real reverse-direction platform-side window at
route km 9.59. Both terminal overlays were present on the three articulated bodies; the close pass
made the divided windscreen, twin wipers, amber destination pixels, side-cab glazing and stepped
nose clearly readable. The corresponding automated OpenGL smoke now requires three rendered car
bodies, two rendered cab ends and the synchronized spatial pass-by event in one run.

The first protected crossing was inspected from an actual production window seat with simulation
motion reduced rather than replacing the player camera. That pass exposed and corrected an early
boom axis error: the final striped arm now spans the road lane in the route direction, hinges at
the signal post, and leaves the warning faces aimed outward toward road traffic. The same frame
reported both barrier and both live-lens submissions, while the static road, markings, cabinets
and housings remained in the one scenery batch.

## Procedural material foundation

The realism audit introduced a renderer-independent `ProceduralImage` stage rather than embedding
OpenGL calls or external image-library assumptions in world generation. Project-owned deterministic
generators produce neutral terrain, ballast, woven textile, concrete, pale polymer panel,
hard-wearing resilient floor, masonry, and directional water sources. One utility
builds the complete mip chain on the CPU and hands every level to CNA `Texture2D`; tests cover
determinism, opacity, tonal range, downsampling, and explicit repeat UV preservation.

The scene retains its low submission count by grouping surfaces by lifetime and material: streamed
terrain, the recentered two-kilometre ballast bed and rail head, station paving, lake/estuary
water, the three-band Lakehurst/Redwater waterfronts, and the immutable six-car carpet/seat-fabric,
resilient-floor, and wall/ceiling panel layers
each form one bounded mesh. Five colour bands replace the estuary's former flat-blue box while its
50 m curve-following strips remain in the same water submission. A dedicated station smoke
requires the waterfront material and all three bands where no lake/river patch can mask a missing
conversion. Vertex tint
remains authoritative, so the same
neutral woven source can distinguish blue and teal seats as well as dark aisle carpet. High uses
`AnisotropicWrap`, Medium/Low use `LinearWrap`, and every textured draw explicitly restores
`LinearClamp` before legacy constant-UV geometry. That restoration is a correctness requirement:
leaving the anisotropic wrap sampler installed made zero-derivative material lookups turn black or
striped on Mesa llvmpipe even though draw and triangle counts remained valid.

Medium and High request BasicEffect's per-pixel material path. A small per-batch response replaces
the former global roughness assumption: woven surfaces remain nearly matte, concrete/resilient
flooring and polymer panels use successively tighter highlights, and water plus wheel-polished rail
heads receive the strongest bounded response. Low forces specular to zero and retains vertex
lighting. The rust-coloured rail foot and web remain in the ordinary structural track batch while
only the narrow head adds one material submission for the entire recentered corridor. Dark
baseplate top faces and paired rust-toned clip faces connect both rails to near-field concrete
sleepers without adding a draw call or modelling invisible fastener sides. Their 360/460/560 m
Low/Medium/High build radius always leaves at least 110 m ahead at the worst track-anchor offset,
while rejecting sub-pixel fittings in the far corridor. The runtime reports the assembly count
separately. Streamed catenary poles likewise receive concrete foundations inside the existing world
batch; telemetry exposes both messenger pieces and registration assemblies for OpenGL smoke
coverage.
Absolute 420 m route slots supply maintained signalling/telecom cabinets between stations. Slots
alternate side, reject station approaches, tunnels, bridges and water crossings, and sample the
same procedural terrain as their surroundings. Each group combines a concrete plinth, one of four
muted condition colours, inset door, three louvres, warning plate and adjacent cable marker; Medium
and High append one bounded contact patch. They remain in the existing streamed submissions and
`lineside_cabinets` is required by the High material smoke.

The streamed world owns two facade meshes. Townhouses and station plinths use neutral
offset-course masonry; houses, apartments, and station upper walls use a separate multiscale
rendered-plaster source. Both are multiplied by the existing authored facade palette. Houses,
townhouses, apartments and station buildings now use these batches as four-sided wall shells,
including end and rear elevations, instead of thin overlays over duplicate opaque box walls.
All elevations use a two-metre texture period and matching vertical course coordinates. This
removes depth competition at grazing angles without polygon bias or extra draw submissions.
Existing roofs, ground/plinths and window fixtures retain their separate geometry.
Nearby windows in the ordinary world batch add a
pale projecting frame, inset glazing, sill, and Medium/High cross-mullions; beyond a quality-scaled
300/475/650 m range they collapse to the former single pane so sub-pixel details do not fill all
seven chunks. Houses, townhouses, apartments, warehouses, and both the lower and stepped upper
volumes of distant towers use the same principle on both route-facing end walls, with a slightly
tighter 260/430/600 m detail range. Dedicated `tower_end_windows` telemetry distinguishes the
high-rise coverage from the aggregate count, while the urban smoke protects both. Doors, roofs,
structural fixtures remain in the ordinary world batch. This removes broad perfectly flat walls and prevents one brick pattern from pretending to
be every construction family while keeping all loaded facade material to two shared submissions
rather than per-building texture ownership.

Station public entrances are two-leaf framed assemblies with a recessed surround, glazed upper
panels, lower protection panels, safety manifestation bands, paired pull handles and a low metal
threshold. Local +Z points toward the platform on either route side. Their base is the actual
platform surface elevation, not rail level: the old two-box door was partially buried behind
the raised platform. Geometry remains in the streamed opaque world batch; these are scenery,
not interactive exits for the train-bound player.

Station window layout also uses platform height as its ground-floor datum. Small 4.8 m wall
envelopes contain one tall waiting-hall floor, while 9.5/10.5 m major/terminal buildings contain
three floors distributed across the usable wall height. Windows are 1.45 m high with ground-floor
centres 1.65 m above the platform. Ground-floor bays are omitted by actual overlap with the
entrance surround plus a 0.25 m wall pier, not by a fixed column index; alternate entrance offsets
therefore cannot leave a window behind the door. These are exterior proportions only, not modeled
station interiors.

Crop-ground overlays are clipped to each intersected terrain cell and to both sides of that
cell's a-c diagonal. Their vertices are barycentric combinations of the actual rendered terrain
corners plus an 18 mm lift. This replaces the former single field-sized quad sampled from the
analytic terrain function, which could bridge relief or sink into the coarser terrain mesh.
The traversal respects chunk boundaries and partial final cells, including fields extending
beyond their owning feature chunk. Field hedges now also use the rendered surface: each edge is
one continuous profile with ground samples at every profile vertex and an 8 cm buried skirt.
Longitudinal/transverse mappings preserve handedness, and eliminating internal end caps removes
the old six-segment overlaps/steps. The existing 128-span cap still bounds each edge. Crop rows
now use continuous triangular young-crop canopies sampled at intervals of at most 2 m (128-span
cap), with 30 mm buried roots and deterministic height variation. Their 0.48 m width and 0.26 m
nominal height before feature scaling replace thin, tall boxes whose shaded vertical sides
merged into a dark wall through the window. A 60 m row uses 122 triangles instead of the former
72, with no extra draw submission. Profiles sample rendered terrain at vertices, but unlike the
soil overlay they are not clipped exactly at every source-triangle boundary.

Medium/High base rural terrain blends parcel edges through narrow grass margins.
Equal U/V subdivision preserves the source diagonal and physical texture coordinates;
only colour interpolation changes, never terrain height. Low keeps the original cells.
Woodland placement reuses the existing candidate budget in irregular five-tree copses,
with occasional solitary trees and untouched urban planting. It consumes no extra random
numbers, so buildings and landmarks do not move when vegetation grouping changes.
Crop-footprint rejection and terrain exclusions run after placement as before.

Urban townhouse candidates form paired street blocks with narrow consistent setbacks.
End blocks approach the chunk seams, avoiding a repeating empty frontage every 250 m.
Shared parcel passages participate in tree/bush exclusion, and common street-edge
garden walls leave explicit household, service, crossing and public-court openings.
The rear building layer remains lower density; this is not a full city road network.

Near Medium/High residential windows retain their raised frames but replace uniform
panes with two-sided opaque graded glass and restrained blind/curtain cues. Absolute
feature distance, variant, floor and column select the room pattern, never the
floating render origin. Detail remains behind the frame front and existing mullions.
This is an inexpensive distant-room approximation, not transparent modeled interiors;
Low and distant windows keep their cheaper panes.

Near Medium/High flat residential roofs reuse the neutral terrain-detail material
for a fine mineral-surfaced membrane; coping, roll seams, drainage inserts and one
small louvred vent share opaque geometry. The 450 m cutoff and existing building
candidate budget bound the cost. Pitched tiles retain physical 0.30 × 0.40 m scale
with restrained hashed per-tile variation, not a new texture package.
Pitched overhangs have closed soffits at every quality. Near houses add open
gutter troughs and connected downpipes; distant roofs retain simpler gutters.
Residential door kits add a frame, panels, lever hardware and a threshold within
250 m on Medium/High, sharing the opaque batch. House ground-floor centre windows
are omitted because they overlapped the entrance. These exterior doors are scenery,
not interactable locations; the player remains aboard.
Medium/High streets reuse neutral grain for asphalt and the concrete pass for
footways and grade-following chamfered kerbs. Near 650 mm paving joints stop at
250 m; the outer pavement extends to the kerb rather than leaving a terrain seam.
This adds one concrete draw in city views that previously had no station concrete.
Long residential approaches share that concrete material. Near low edging and
six small planting pockets leave the central walking width clear. Access corridors
reserve door-offset-aware footprints against trees, including unloaded neighbours;
the renderer and generator use the same dimensions. Broad lawns are retained.
Six cached traffic-car colour variants share a tapered body construction with
lined wheel cutouts, graded opaque windows and simple five-spoke hubs. All trim
remains in each vehicle's single mesh; no per-wheel draw or new material is used.
Station names, platform identifiers and live departure boards reuse the onboard
font atlas in two shared alpha batches. Glyphs are flat, top-anchored by actual
font metrics, and face inward on either bank. Static lettering follows world
streaming; live lettering follows the existing destination/ETA/phase state key.
Three compact station name boards use grounded supports and fixture reservations.
The centre board remains in the seated viewing cone without blocking the public
entrance; end boards occupy the rear edge. Small platform identifiers are suspended
away from the live departure board. Rural/waterfront facades add shallow boarding
and entrance hoods; urban/suburban kits use piers, lintels and different window
proportions. All remain in the existing static batches and preserve door clearance.

Distant hills are bounded 4/8/12-grid height-field mounds rather than ellipsoids.
Terrain-sampled skirts are buried 30 cm; asymmetric analytic lobes and smooth
finite-difference normals remove visible undersides without an extra material batch.
The sunny sky uses four procedural cloud silhouettes in one 512-square atlas.
The same 4/6/8 billboard budget is retained, with varied elevation and aspect ratio.
Transparent tile margins protect ordinary mip filtering; no weather simulation is added.

Industrial compounds are encoded by deterministic warehouse variant bits, so no extra feature
records or route-wide ownership are needed. A leader builds the segmented hardstanding, fence,
gate, parking marks, pallets and detailed loading truck once; a three-building compound also adds
one compact glazed service van with four grounded wheel cues. Every warehouse, including a
standalone building, now receives two louvred rear-wall HVAC cabinets, a full-height downpipe and
two low-poly roof extractors. Each compound also receives two compact raised planting beds inside
the front fence, clear of both the gate and loading paths. Three irregular evergreen shrubs per
bed remain in the existing world mesh; High adds secondary foliage lobes, while the shared static
contact-shadow submission grounds each group without another draw call. All of this geometry
remains in the existing streamed static batches.
`industrial_utilities` telemetry counts one complete plant group per loaded warehouse, while the
yard-vehicle count includes the bounded service van and `industrial_edge_planting` counts complete
bed groups. A representative High passenger-window capture at km 2.603 reported 14 utility groups,
two compounds, four vehicles, one trailer, four planted buffers, 104 draw calls and 513,096
triangles.

Station canopies retain the same streamed static ownership as their platform. Both roof edges now
carry a continuous gutter; two deterministic rear-edge downpipes descend to the platform and use
short lateral shoes to meet the already-authored rear drainage channel. The complete construction
is one counted `canopy_drainage` assembly per loaded station and creates no extra submission.
On Medium and High, three to six narrow, palette-matched runoff streaks descend from each station
building's track-facing roof edge between the window bays. These low-contrast opaque strips avoid
alpha sorting and z-fighting in the OpenGL 3.3 path, join the existing static world mesh, and are
counted by `station_facade_streaks`; Low omits them together with platform-surface weathering.

The recentered two-kilometre track batch also owns deterministic point mechanisms at both
approaches to every major-station siding. Each in-range merge receives a concrete pad, compact
motor housing, lateral drive rod, post and yellow route indicator. Placement is absolute relative
to the authored station and rejects tunnel/bridge overlap. Two polished 22 m switch blades follow
the same easing function as the siding rails. Rather than guessing a fixed frog offset, the builder
walks inward from the motor to the first 1.52 m centreline separation and places a dark base plus
converging main/branch rail noses there; this also handles the wider opposite-side turnout family.
`turnout_motors` and `turnout_blades` report matching bounded populations in the existing track
submission. The mechanism is visual infrastructure and deliberately does not imply dispatch AI.

A broad-composition capture now guards this class of regression without a brittle golden image. It
checks the passenger-eye region for excessive black coverage and minimum tonal diversity after a
real High-quality OpenGL 3.3 frame. The same smoke requires positive triangle counts for terrain,
ballast, interior textile and panels, streamed contact shadows, and platform concrete. The contact
shadows are one transparent per-chunk batch with two low-alpha footprint bands under major
structures, canopies, trees, and bushes; a second immutable batch grounds every seat group, all
33 table base plates, and the six vestibule luggage/rack footprints. The latter contains exactly
264 triangles and is protected by the High OpenGL smoke. These cues add depth without another
draw submission or a renderer-specific shadow-map path.
