# Mountain–River–Forest–Rain acceptance

**Target map:** `/Game/__UeremcpPoc/MountainRiverRain/L_MountainRiverRain`  
**Capture directory:** `Saved/UEREMCP/MountainRiverRain/`  
**Disposition:** independent acceptance gate; never infer PASS from tool completion.

## Safety and evidence

Do not connect to, focus, move, possess, save, reload, or run commands in the builder's
editor. While it is active, validation is limited to reading already-written PNGs and
reports. Run map inspection, play/camera checks, compile checks, and save/reload checks
later in a separate editor process after the builder releases the project.

Retain one machine-readable report plus the original PNGs. Every row below is
`PASS`, `FAIL`, or `BLOCKED`; `BLOCKED` is mandatory when evidence is absent. Overall
PASS requires every required row to PASS.

## Required evidence set

1. `player_start.png`: 1920x1080 player view showing all four subjects.
2. `river_upstream.png` and `river_downstream.png`: views along the channel.
3. `valley_cross_section.png`: oblique view showing both valley shoulders and floor.
4. `rain_camera_a.png` and `rain_camera_b.png`: same camera orientation, camera
   positions at least 1000 uu apart, captured after rain has reached steady state.
5. `overview.png`: high oblique or top-down view of river continuity and forest layout.
6. `structure.json`: read-only inventory, transforms/bounds, landscape samples,
   river centerline or equivalent ordered corridor samples, tree instance positions,
   rain-system attachment/local offset, live particle count, and compile/renderer state.
7. `reload.json`: before/after-restart package identity, map inventory summary, and
   clean-load diagnostics from the isolated validation process.
8. `performance.json`: loaded actor count, foliage/tree instance count, peak live rain
   particle count, and a 10-second warmed frame-time sample.

## Gates

### Structure and composition

- **Landscape/valley:** sampled terrain elevation range is at least 1000 uu. At one
  river cross-section, the channel floor is at least 300 uu below both shoulders.
  `valley_cross_section.png` must visibly corroborate the sampled relief; a flat plane
  fails.
- **River continuity:** ordered corridor samples form one unbroken path through the
  composed area, with no dry gap or disconnected segment. `overview.png` and the two
  along-channel views must show a continuous visible water corridor from foreground
  through mid-ground; isolated ponds or hidden discontinuities fail.
- **Trees/banks:** trees occur on both sides of the river in at least three separated
  longitudinal sections. No tree bounds intersect the defined open-channel envelope,
  and the screenshots must leave a clearly readable water path. Sparse intentional
  clearings pass; a wall of trees across the channel fails.
- **Player framing:** `player_start.png` must simultaneously contain identifiable
  mountain relief, visible river water, multiple trees reading as forest, and visible
  rain streaks/drops. A subject visible only in another capture does not satisfy this
  gate.

### Rain behavior

- Rain is visibly present against both sky and dark terrain in A and B; it must not
  read as a few isolated particles.
- The rain system's world displacement between A and B matches the player-camera
  displacement within 10%, while its camera-local offset changes by no more than
  100 uu.
- World landmarks must shift between A and B while rain coverage remains centered on
  the frame. Identical images, a stationary world-space rain patch, or transform data
  without paired visual evidence fails.

### Readability

- No required screenshot may have crushed black terrain, clipped white sky over more
  than 20% of pixels, or fog that erases the river/valley silhouette.
- Mountains must separate from sky, river from banks, trees from terrain, and rain
  from at least two background tones. Lighting may be dramatic, but all four subjects
  must remain legible without exposure adaptation between paired captures.

### Durability and asset validity

- In the isolated validation process, hash and timestamp the completed packages, load
  the target map, record inventory, close without saving, start a fresh process, reload,
  and compare. Package hashes/timestamps must remain unchanged. The target map, river,
  terrain, trees, rain system, player framing, and referenced assets must persist with
  matching transforms/counts. Any load error, missing reference, unexpected dirty
  package, package write, or count drift fails.
- All referenced Niagara systems and materials must report settled compile success
  from read-only status/diagnostics; the validator must not request an asset save.
  Every enabled Niagara emitter used by rain must have at least one valid enabled
  renderer and no missing material. Warnings are recorded and reviewed; compile
  errors, unresolved shaders, missing renderers, or fallback materials fail.

### Performance sanity

This is a runaway-content gate, not a platform performance certification:

- loaded actors <= 2500;
- tree/foliage instances <= 100,000;
- peak live rain particles <= 150,000;
- after warm-up, 10-second p95 game-frame time <= 33.3 ms in the isolated player run.

Record hardware, resolution, scalability, and whether the editor or standalone player
was used. Exceeding a cap fails unless a replacement budget was approved before the
run; do not waive it after observing results.

### PNG integrity and visual review

For every required PNG:

- decode succeeds as PNG; dimensions are exactly 1920x1080;
- file is non-empty and pixel count is exactly 2,073,600;
- normalized luminance standard deviation >= 0.03;
- near-black pixels (luminance <= 0.01) < 95%;
- near-white pixels (luminance >= 0.99) < 95%;
- at least 1,000 distinct RGB colors after 8-bit quantization.

Numeric checks reject blank/corrupt captures; they do not prove composition. A reviewer
must also record yes/no for each visible subject, river continuity, open channel,
valley relief, rain coverage, silhouette separation, clipping, and obvious rendering
artifacts. Any required “no” is FAIL.

## Result record

Report the eight gate groups above with evidence filenames and measured values. Use
`BLOCKED` for missing captures, unavailable isolated-editor access, or absent structural
reports. Never report `created_and_validated` or equivalent for this world unless the
artifact, structural, reload, compile/renderer, and performance gates all pass.
