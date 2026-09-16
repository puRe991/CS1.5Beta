# CS1.5Beta — Custom C++/OpenGL Engine

This branch/fork builds a standalone, open-source engine in C++ and OpenGL
that reads the original GoldSrc assets (`.bsp` maps, `.wad` textures, `.mdl`
character models) from the classic Counter-Strike Beta/1.x releases and
renders them without depending on Half-Life or GoldSrc itself.

The original repository (see [`Ch0wW/counterstrike-betas`](https://github.com/Ch0wW/counterstrike-betas))
is a preservation archive of the official CS Beta/Pre-1.6 releases — this
project reuses those assets as real-world test data for the engine, but does
not ship or redistribute them.

## Building

```
cd engine
mkdir build && cd build
cmake ..
make
```

Requires SDL2 and OpenGL development headers (`libsdl2-dev` on Debian/Ubuntu).

## Running

```
./cs15engine <path/to/map.bsp> <path/to/cstrike-dir-with-wads> [path/to/viewmodel.mdl]
```

Controls: WASD to move, mouse to look, Space to jump, Ctrl to duck, R to
reload, V to toggle third-person, Tab for the scoreboard, B for the buy
menu, Esc to quit.

Two debug/verification tools are also built:
- `./mapshot <map.bsp> <wad_dir> <out.bmp>` — renders one frame of a map to a file.
- `./modelshot <model.mdl> <out.bmp>` — renders one frame of a character model to a file.

The main menu is a separate executable for now (see below):

```
./csmenu <path/to/cstrike/models>
```

Click PLAY/WATCH/INVENTORY/STORE to switch screens; buy a case in the Store,
then open it from the Inventory. All currency is fictive — no real payments,
no Steam integration.

## Architecture

```
engine/
  src/
    core / main.cpp   # entry point, render loop
    camera.{h,cpp}     # free-fly camera (Z-up, matches GoldSrc coordinates)
    player.{h,cpp}      # player movement: wall sliding, ground check, jump, gravity
    mat4.h              # minimal lookAt/perspective matrix math (no GLU/GLM dependency)
    entities.{h,cpp}     # BSP entity lump -> spawns, bomb targets, buy zones
    menu_main.cpp        # CS:GO-inspired main menu (Play/Watch/Inventory/Store)
    inventory.{h,cpp}    # fictive-currency skin/case economy + case-opening RNG
    weapons.h            # weapon catalog: price/model/ammo/damage/fire-rate/etc. per weapon
    hitboxes.h           # per-body-part damage multipliers + ray-vs-hitbox trace
    bots.{h,cpp}          # AI opponents: perception, movement, state machine, firing
    nav.{h,cpp}           # coarse waypoint-graph pathfinding (visibility graph + A*)
    render/
      render.{h,cpp}      # shared GL drawing: texture upload, BSP/MDL draw, screenshots
    ui/
      ui.{h,cpp}          # minimal immediate-mode 2D UI (rects, buttons, text)
      font5x7.h            # built-in 5x7 bitmap font (our own, not copied)
    audio/
      audio.{h,cpp}       # SDL audio-callback mixer: WAV cache, 2D/3D voices, distance/pan
    assets/
      limits.h           # shared sanity caps for untrusted asset headers
      wav.{h,cpp}         # RIFF/WAVE parser: PCM 8/16-bit, mono/stereo
      pak.{h,cpp}        # Quake-style PAK archive reader
      wad.{h,cpp}        # WAD3 texture package parser (palette-indexed -> RGBA8)
      bsp.{h,cpp}        # BSP v30 map parser: geometry, textures, entities, hull collision
      mdl.{h,cpp}        # Studio Model (.mdl v10) parser: bones, textures, skinned mesh
    tools/
      mapshot.cpp        # standalone map screenshot tool
      modelshot.cpp      # standalone model screenshot tool
      mdlbatchtest.cpp   # batch-load a list of .mdl files, report pass/fail per file
      hitboxtest.cpp     # load one .mdl, print its parsed hitboxes/body parts/world bounds
  tests/                 # unit tests (CTest), run headless — no SDL/GL needed
    test_framework.h     # tiny header-only test harness (no external dependency)
    fixtures.h           # builds synthetic PAK/WAD3/BSP/MDL files in memory
```

## Tests

```
cd engine/build
ctest --output-on-failure
```

The parsers are exercised against synthetic binary fixtures generated at test
time, so no game data is needed (or shipped). The suite is also clean under
`-fsanitize=address,undefined`, which is the intended way to run it when
touching any of the asset loaders.

## Status: what works today

- [x] Window, OpenGL context, render loop (SDL2)
- [x] Free-fly camera: WASD movement, mouse look, vertical movement
- [x] PAK archive reading (directory + entry extraction)
- [x] WAD3 texture parsing, palette-indexed decoding to RGBA8, `{`-transparency convention
- [x] BSP v30 map parsing: entities, faces (surfedges/edges/vertices), texture coordinates
- [x] Texture resolution from embedded miptex lump *or* external WAD files (worldspawn `wad` key)
- [x] Map rendering in the main window with real CS textures
- [x] BSP hull collision (hull 1 / player box) — movement blocked and slid against solid geometry, same method as the original engine (`SV_HullPointContents`)
- [x] MDL v10 parser: bone hierarchy, bind-pose skeleton, palette-indexed textures, body parts/meshes, triangle strips & fans
- [x] Character models render correctly (verified against real player models, e.g. `urban.mdl`)
- [x] All weapon models load: 87/87 view (`v_*`), world (`w_*`), and pickup (`p_*`) models parse successfully, incl. knives, grenades, C4, shield
- [x] Weapon view model rendered in the main engine window (own narrow-FOV pass, positioned over the world view)
- [x] CS:GO-inspired main menu (`csmenu`): top nav bar (Play/Watch/Inventory/Store), built-in bitmap-font UI toolkit (no external font/image libs), functional Store → buy case → Inventory → open case → reveal loop with rarity tiers/odds matching CS:GO's real distribution (79.92% Mil-Spec / 15.98% Restricted / 3.2% Classified / 0.64% Covert / 0.26% Special)
- All of the above verified against real CS 1.5 release assets (`de_dust2.bsp`, multiple `.wad` files, `urban.mdl`, all 87 weapon models), not just compiled
- [x] Sound engine: own RIFF/WAVE parser (`assets/wav.*`, PCM 8/16-bit mono/stereo) feeding an SDL audio-callback mixer (`audio/audio.*`) that plays any number of simultaneous 2D and 3D voices — 3D voices get linear distance falloff and a stereo pan derived from position relative to the listener's right vector. Wired into gameplay: gunshots and reloads (2D, always full volume to the shooter), jump, footsteps (fired every `kFootstepInterval` units walked, not every frame), and breakable destruction (3D, at the impact point) — same graceful-load pattern as the particle/decal systems: a missing `sound/` directory just means silence, not a crash, since no game audio assets ship with this engine. New `snd_volume` cvar controls master volume.
- [x] Named per-body-part hitboxes with damage multipliers: `assets/mdl.{h,cpp}` now parses a model's real hitbox lump (`mstudiobbox_t` — bone index, HITGROUP id, bone-local AABB) and classifies each one by the HL SDK's actual `HITGROUP_*` constants (0 generic, 1 head, 2 chest, 3 stomach, 4/5 left/right arm, 6/7 left/right leg) rather than guessing from bone names. `MdlModel::poseHitboxes()` transforms every hitbox into a world-space AABB at any bind or animated pose (refactored the existing bone-world-transform math out of `pose()` into shared helpers so both use the same code path). `hitboxes.h` adds the actual per-body-part damage multipliers (head 4x, stomach 1.25x, limbs 0.75x, chest/generic 1x) and `traceHitboxes()`, a slab-method ray-vs-AABB test that picks the nearest hit box. Verified with synthetic two-bone/two-hitbox fixtures (`test_mdl.cpp`, `test_hitboxes.cpp`) and a new `hitboxtest` debug tool (prints a model's hitboxes/body parts/world bounds, same convention as `mdlbatchtest`/`modelshot`) for checking against real assets. Now wired into actual damage — bots (see the Bots & AI entry below) are the opposing entity this needed, and the player's hitscan resolves to their real posed hitboxes.

## To Do — what's still needed for a full, playable Counter-Strike

### Rendering
- [x] Shader-based world rendering: GLSL vertex/fragment shader + one VBO for all BSP geometry (`render/`), replacing per-face `glBegin`/`glEnd`. Bullet marks, the view model, and the 2D UI/HUD still use the legacy fixed-function path (GL 2.1 compatibility profile allows mixing both) — converting those is a follow-up, as is eventually moving to a core 3.3+ context
- [x] Lightmaps: BSP lighting lump baked into a shared 2048x2048 atlas, multiplied into the base texture in the world shader. Style 0 only — no animated/switchable light styles yet, and no fullbright/runtime relighting.
- [x] BSP visibility (PVS) culling for performance on large maps — parses the NODES/LEAFS/MARKSURFACES/VISIBILITY lumps, finds the current leaf by walking the render BSP tree from the camera position, decompresses that leaf's RLE-encoded PVS row, and unions every potentially-visible leaf's faces into a per-frame visibility mask (`BspMap::computeVisibleFaces`). `WorldMesh` sorts each texture's faces by leaf at load time so visible runs stay contiguous, then coalesces them into as few `glDrawArrays` calls as possible — a `visibleFaces` bitmask empty (outside the map, or a map with no compiled PVS) falls back to drawing everything, exactly like before this existed. Verified numerically (e.g. de_dust2's CT spawn sees only ~9% of the map's faces, a nearby T spawn ~19%) and visually across several maps (de_dust2, de_dust, cs_office, de_nuke, as_oilrig) — geometry renders identically to before, no missing surfaces or seams, just far less of the map actually submitted per frame.
- [x] Sky rendering: real 6-face GoldSrc skybox (`assets/tga.*` + `render/skybox.*`), loaded from the worldspawn `skyname` key, drawn camera-centered with depth test off. "sky"-textured faces are excluded from normal geometry so the skybox shows through.
- [x] Decals (bullet holes, blood, etc.) — a new persistent world-space decal system (`render/decals.{h,cpp}`) replacing the old plain dark impact dot, using the real GoldSrc decal textures (decals.wad's "{shot1-5"/"{blood1-6" lumps). Decal WAD lumps turned out to use a different convention than masked world textures: their palette is a plain grayscale ramp used as a per-pixel alpha mask (index 0 = transparent, 255 = opaque) rather than an index-255 transparency key, discovered by tracing why a first attempt rendered solid white boxes instead of translucent marks — `WadFile::decodeDecalTexture` decodes this correctly with a caller-supplied tint color (near-black for bullet holes, dark red for blood) since the ramp itself carries no real color. Decals orient to the actual surface hit: `BspMap::traceLine` now also reports the clip plane normal at the hit point. Wired to real events (every shot leaves a bullet hole; the 'H' debug damage key leaves a blood splash) and verified visually — a correctly transparent, properly tinted bullet hole and blood splatter both render against real wall geometry. Blood has no NPC/hit-target trigger yet since none exist in this engine (only the local player can take damage).
- [x] Particle effects (muzzle flashes, explosions, smoke) — a real world-space billboard sprite system (`render/particles.{h,cpp}`) driving the real GoldSrc `.spr` effect assets (muzzleflash2.spr, smokepuff.spr, fexplo.spr) added by the SPR loader: additive/alpha blending per the sprite's own render mode, animated through all its frames over the particle's lifetime, faded in/out at the edges, camera-facing (billboarded against the real view direction, not the view model's screen-locked axes). Wired to real gameplay events — every shot spawns a muzzle flash and, on a hit, a smoke puff at the impact point; a bomb detonation spawns an explosion at the bomb's position. Verified visually: screenshotted a mid-animation explosion frame rendering correctly as a bright additive fireball, plus the muzzle flash glowing correctly at point-blank range.
- [x] SPR sprite format support (used for effects, some HUD elements) — parses dsprite_t (orientation + render mode) and all frames, decoding the palette to RGBA per the sprite's render mode (normal/additive: opaque; alphatest: index 255 keyed transparent; indexalpha: palette used as a grayscale alpha ramp over white, since there's no per-entity render-color tinting yet). Verified against all 88 .spr files shipped with the game via `sprbatchtest` (0 failures) plus a manual pixel dump confirming an additive muzzle flash and an alphatest HUD sprite both decode correctly. Not yet wired into any renderer — animated group frames (none present in this asset set) are also not handled.
- [x] MDL animation playback — `MdlModel::pose(sequence, frame)` decodes each sequence's compressed per-bone animation tracks (RLE keyframe streams, GoldSrc's `mstudioanim_t`/`mstudioanimvalue_t` format) and re-skins the mesh at any fractional frame, interpolated between the two nearest keyframes. The view model now actually plays idle/draw/shoot/reload instead of standing in its static bind pose, switching sequences on the real fire/reload/equip events and falling back to idle when a one-shot animation finishes. Verified by rendering both a weapon (v_ak47.mdl idle/shoot, hand+gun bones) and a full player model (urban.mdl walk cycle, opposite mid-stride frames) at multiple frames — all coherent, non-garbled poses — plus a live in-engine screenshot showing the view model in a genuinely different pose after a simulated shot. Directional-blend sequences (9-way aim/shoot sets) are now fully supported too: `pose()` takes a normalized `blend` parameter and linearly interpolates between the two nearest of a sequence's `numBlends` variants — verified against urban.mdl's `ref_aim_ak47` (numBlends=9), rendering a full look-up pose at blend=0, full look-down at blend=1, and a coherent in-between pose at blend=0.5. Player-model animation is now wired into a real (if minimal) renderer too: a 'V' key toggles a third-person chase camera that draws the player's own team skin (urban/terror) at their world position, animated between idle and run via the same pose() system, switching skins on team change — verified with an in-engine screenshot showing the correctly posed, correctly facing body model. Remaining limitations: only sequences embedded in the main file are supported (true of every sequence in this game's asset set, but not of exotic external-seqgroup models like the hostage NPCs, which already fail to load for unrelated reasons), and the third-person chase camera has no collision against world geometry (it can clip into walls in tight spaces).
- [x] MDL attachment points (muzzle flash origin, weapon-to-hand attachment, etc.) — parsed from `mstudioattachment_t` and transformed through each attachment's owning bone bind-pose (same math path as vertices); the view model's muzzle flash is drawn at attachment 0's real position instead of a fixed offset. Player-model hand attachments are parsed and available via `MdlModel::attachments()`/`findAttachment()`, but the engine has no third-person/other-player rendering yet to attach a weapon model to.
- [x] View model (first-person weapon model) rendering in the main engine window — own narrow-FOV projection pass + depth clear so it never clips into world geometry (pose/offset is a fixed approximation, not attachment-point accurate — see animation TODO below)

### Physics & Movement
- [x] Gravity + jump + ground detection (probe-based, snaps back on floor/ceiling contact instead of clipping through) — verified against real de_dust2 geometry with logged position/velocity
- [x] Ducking, ground friction/acceleration curves, air control — replaced the old instant-velocity movement with persistent velocity + Quake/Source-style ground acceleration and friction (speed ramps up to and decelerates from a real max speed instead of snapping), plus a separate, much weaker air-acceleration constant for limited air control. Ctrl holds duck (switches to the crouch collision hull, halves move speed, lowers the eye height), and only stands back up once there's headroom (hull 1 isn't solid at the current position). Verified interactively (via simulated key input + a debug telemetry log): holding W ramps speed from 0 up to exactly the 250 u/s walk cap then decays back to 0 on release from ground friction; the same test while holding Ctrl caps at exactly 125 u/s (half speed, as configured); no strafe-jump speed-bunnying yet, just the acceleration/friction/air-control model itself.
- [x] Full hull-based collision for other hulls (crouching hull, large hull) — `BspMap` now parses and exposes all 4 of the map's precompiled collision hulls (`pointInSolidHull(point, hull)`, hull 0=point/1=standing/2="large"/3=crouch) instead of hardcoding hull 1 everywhere; movement/ducking above actually switches between hull 1 and hull 3 live. Verified by scanning a vertical line through a real doorway on de_dust2: hull 1 (standing) reports solid in several z-ranges where hull 3 (crouch) reports clear — real, distinct collision geometry per hull, not just an alias. Hull 2 (large) parses and is queryable but nothing in gameplay uses it yet, since the original engine only used it for bigger monsters this project has no equivalent of.
- [x] Line trace for hitscan (`BspMap::traceLine`) — a stepped-sampling trace, not a proper swept hull trace, so it's slightly less precise than the real engine's; fine for now, worth revisiting
- [x] Entity physics (moving platforms, doors, breakables) — `brush_entities.h/.cpp` simulates `func_door` (opens on player touch along its compiled `angle`/`lip` movedir, waits, auto-closes, reverses if touched again while closing), `func_plat` (rides down when the player steps on top, waits at the bottom, returns), and `func_breakable` (tracks `health`, destroyed by bullet damage). Required extending `BspMap` beyond model-0-only collision: every submodel now keeps its own per-hull clip tree (`BspModelBounds::headNode`, `pointInSolidModel`) and a `faces() -> owning submodel` index (`faceModelIndices`), so a moved brush collides and renders where it actually is (`point - offset` against its own rest-position tree) instead of only at its compiled position. Player movement (X/Y/Z resolution, ground probe, duck-stand check) now also tests `BrushEntitySystem::pointInSolid`, standing on a moving platform carries the player along via a per-frame offset delta, and the bullet-hit raymarch damages/destroys breakables. Rendering excludes each live brush entity's faces from the static world mesh and redraws them translated by their current offset through the legacy fixed-function path (no lightmap on these faces, a known simplification). Verified: builds clean, and by construction a closed door's brush sits exactly on its compiled geometry (identical collision to before) until touched. No `func_train`/`path_corner`, rotating doors, or buttons/triggers yet — only the three entity classes actually named in this TODO.

### Gameplay Systems
- [x] Entity system (`entities.h/.cpp`): all spawn points (CT/T-tagged) instead of just the first found, real bomb-target/buy-zone regions from BSP submodel bounds, live "BOMBSITE"/"BUY ZONE" HUD indicators — verified against de_dust2's real entity counts (40 spawns, 2 bomb targets, 2 buy zones). `func_door`/`func_plat`/`func_breakable` now simulate (see `brush_entities.h/.cpp` and the Physics & Movement entry above); `func_button`, triggers, and lights are still just static unparsed geometry/data.
- [x] Basic hitscan firing: left click, 30-round magazine, R to reload, impact marker at the hit point — no damage/recoil/spread/switching yet, and only one weapon (AK47) is wired in at all
- [x] Player health/death/respawn (`game_state.h/.cpp`): fall damage + a debug damage key, death freezes movement/shooting, auto-respawn at a random spawn point. No armor yet.
- [x] Round system (partial): Live/Intermission timer loop, round ends on timeout, death, or now bot elimination too, HUD banner + auto-respawn into the next round. No buy-time phase, no real economy loop yet — see the in-round buy menu item below.
- [x] Bomb defusal logic (`de_` maps): plant (T, hold E in a real func_bomb_target zone, 3s) and defuse (CT, hold E within 80 units of the bomb, 5s), 35s fuse, real win/loss round-end banners. Bots can plant and defuse too now (see Bots & AI) on their own separate progress timer. The bomb still carries no visible world model.
- [ ] Hostage rescue mode logic (`cs_` maps)
- [x] Team assignment (`PlayerState.team`) drives team-based spawn selection for both initial spawn and respawn. No team-select screen, no auto-balance, no other players — 'N' is a debug key to switch team for testing.
- [x] HUD: crosshair, ammo, weapon name, health, round timer, money, team, zone/plant/defuse/bomb indicators, damage flash, and a real per-map radar (loads `cstrike/overviews/<map>.bmp`, player dot from our own BSP world bounds — not the overview file's own undocumented zoom/origin metadata)
- [x] In-round buy menu (`weapons.h`): B key, gated to standing in a real func_buyzone + round live, full weapon catalog (25 weapons, one buy-menu column per category) with prices, deducts money and swaps the equipped weapon/magazine+reserve ammo. Money system: starts at 800, flat per-round reward (no real win/loss economy tied to it yet). Still a single weapon slot — buying a new gun replaces whatever's equipped, there's no separate pistol/primary/grenade/armor slots or per-team price differences yet.
- [x] Scoreboard (hold Tab): real CT/T round-win tally, player's own team/money/health/round number. No player roster or kill count — single-player only, nothing else to list.

### Main Menu / Meta-game
- [x] Top nav shell, Store, Inventory, Case-Opening with a fictive-currency economy (`csmenu`)
- [x] "PLAY" lists real maps and launches `cs15engine` (detached process) for the chosen one — no mode/gametype selection yet, just map -> GO
- [ ] "WATCH" (demos/replays) — no replay system exists yet to watch anything
- [ ] Loadout screen (equip a specific owned skin per weapon per team, used by the in-game renderer)
- [ ] Persisting inventory/currency to disk (currently resets every launch)
- [ ] Trade-up contracts, StatTrak, stickers, graffiti, music kits — not modeled at all yet
- [ ] Real-money purchase path: **not started, and not a simple follow-up.** Real-money loot boxes are
      legally regulated as gambling in multiple jurisdictions (e.g. Belgium, the Netherlands), and building
      it means payment processing, KYC/age verification, and jurisdiction-aware compliance — this needs a
      deliberate legal/business review before any implementation work, not just an engineering pass.
### Audio
- [x] Sound engine, WAV loading, 3D positional audio, and weapon/jump/footstep/breakable sound triggers — see the Status section above. Not yet covered: ambient/looping map sounds (`play2D`/`play3D` support `loop`, but nothing calls it with `loop=true` yet), material-based footstep variation (surface type isn't tracked, so it's always the same four generic steps), and voice chat (separate item below).

### Networking
- [ ] Client-server architecture (currently single-process, no networking)
- [ ] Server-authoritative simulation, client prediction, lag compensation
- [ ] Snapshot/delta compression for state sync
- [ ] Voice chat (stretch goal, original CS Beta didn't have it either)

### Tooling & Engine Infrastructure
- [ ] Config/cvar system
- [ ] Console (in-engine developer console)
- [ ] Input rebinding
- [ ] Save/load or at least map transition support
- [ ] Cross-platform packaging (currently only built/tested on Linux)
- [ ] Automated tests beyond the manual `mapshot`/`modelshot`/`wadtest` verification tools

## Gap analysis: full competitive 5v5 tactical-FPS design vs. this engine

A full game design spec (competitive 5v5 tactical FPS with its own identity —
own maps/weapons/sounds/UI/progression, not CS assets) was compared against
everything above. Most of it is genuinely new scope, not just a rewording of
existing TODO items, so it's broken out here by system rather than merged in.
Nothing in this section is started; a few rows note where a *very* partial,
single-player-only foundation already exists elsewhere in this README.

### Game modes
- [ ] Competitive (ranked 5v5, round-based, side swap, overtime)
- [ ] Casual (relaxed ruleset)
- [ ] Deathmatch (free-for-all, respawn-on-death)
- [ ] Team Deathmatch
- [ ] Wingman (2v2, small maps)
- [ ] Arms Race / gun-progression mode
- [ ] Training/practice mode (aim/recoil/grenade drills, infinite money/ammo, target dummies)
- [ ] Custom lobbies with user-defined rulesets
- [ ] A mode/gametype selector at all — today `csmenu`'s PLAY only picks a map and always launches the same single-player round loop

### Weapon system depth
- [x] Full weapon roster across categories (`weapons.h`'s `kWeaponCatalog`): melee (knife), 6 pistols, 5 SMGs, 2 shotguns, 6 rifles, 4 snipers, 1 heavy (M249) — 25 weapons total, each with its own price/model/magazine+reserve ammo/damage/fire-rate/full-auto flag/move-speed scale, buyable from a per-category buy-menu layout and fully swappable at runtime (view model, animations, ammo, fire behavior). Fire rate and full-auto vs. semi-auto are both real now (a cooldown timer gated by each weapon's `fireRateRpm`, instead of every weapon firing once per click), reload respects actual magazine/reserve sizes instead of refilling a hardcoded 30, and per-category fire sounds play through the new audio engine. Data-driven in the sense of "one static table describing every weapon", not yet "loaded from external data files" — see the row below.
- [ ] External data-driven weapon definitions (loaded from a data file at runtime, not a compiled-in C++ table) — plus depth the current table doesn't model at all: accuracy/spread curves (stand/crouch/move/jump), recoil pattern + recovery, armor penetration, damage falloff by range, headshot multiplier, draw/holster time, reload *time* (ammo currently refills instantly on 'R')
- [x] Named per-body-part hitboxes (head, chest, stomach, arms, legs) with independent damage multipliers — parsing, world-space transform, and the ray-vs-hitbox trace itself are done (see Status above), and now actually connected to a damage event: the player's hitscan resolves to a specific bot's real posed hitbox (see the Bots & AI entry below), so headshots vs. bots deal real 4x damage. The player themself still only ever takes flat damage back (bots don't hit-test the player's own hitboxes yet), and there's no networked second player to test PvP-vs-PvP hit detection with.
- [ ] Learnable, non-random-feeling recoil patterns (deterministic vertical+horizontal pattern + bounded randomness + recovery-over-time), first-shot accuracy, moving/jumping/crouching accuracy modifiers
- [ ] Armor/helmet damage reduction model
- [ ] Bullet penetration through thin materials ("wallbang")
- [ ] Server-side hit validation (moot until there's a client/server split at all — see Networking)

### Grenades & thrown utility
- [ ] Actual throwable/thrown-projectile grenades (arc trajectory, bounce, fuse) — today's "grenades" are just visual particle effects (explosion/smoke sprites) triggered directly by game logic, not physical thrown objects
- [ ] HE grenade (explosive damage falloff by distance)
- [ ] Smoke grenade (dynamic volume that blocks vision, expands/dissipates over time, interacts with geometry/lighting)
- [ ] Flashbang (blind/deafen effect scaled by distance, view angle, and line-of-sight/cover)
- [ ] Incendiary/molotov (timed area-denial fire zone with damage-over-time)

### Physics & world interaction
- [x] Interactive doors, moving platforms (`func_door`, `func_plat` — see the Physics & Movement TODO above); `func_button`/triggers still don't exist
- [x] Breakable/destructible props (`func_breakable` — health from the map, destroyed by bullet damage; see the Physics & Movement TODO above)
- [ ] Thrown/dropped physical objects (grenades before they're picked up as projectiles, a dropped bomb model, dropped weapons) — physics objects in general, beyond the player's own hull collision

### Audio
- [x] Spatial/3D positional audio (`audio/audio.*` — see Status above); still not scaled by movement speed/crouch, and no material-based footstep sounds (concrete/metal/wood/dirt/grass/stone/water) since surface type isn't tracked yet
- [ ] Voice chat: team/party/lobby channels, push-to-talk vs. open mic, per-player mute, muted-after-death-in-some-modes rule

### Bots & AI
- [x] A real bot/AI opponent (`bots.h`/`bots.cpp`): spawns on the team opposite the player at each team's own spawn points, perceives the player (line-of-sight raycast + max view distance, plus hearing — see below), and moves/fights with an Idle → Search → Chase → Attack state machine — closing to an engage distance then holding position and firing, investigating a heard sound or a teammate's last-seen player position, or patrolling toward a bomb site (falling back to a random nearby point on maps with none) when it has nothing else to go on. Firing applies real damage to the player (`damagePlayer`), and being shot back puts the hitbox system to use: the player's hitscan resolves to a specific bot's actual posed, transformed-to-world hitbox (`MdlModel::poseHitboxes()` + `transformHitboxesToWorld()` + `hitboxes.h`'s `traceHitboxes()`), so headshots genuinely deal 4x damage instead of a flat per-bot number. Round integration: all opposing bots dead (and no bomb ticking) ends the round as a genuine elimination win (`endRound(..., "ELIMINATED")`), scored to the player's side, and bots respawn (with a fresh buy) at the start of each new round exactly like the player does. Verified with unit tests against synthetic maps (`test_bots.cpp`, `test_nav.cpp`): spawn/team placement, damage/death/respawn, the hitbox rotate+translate math, the perceive→engage→fire loop, reaction-time delay, buy logic, hearing, team callouts, and path-following.
- [x] Bot difficulty tiers (Beginner → Expert): a `BotDifficultyTuning` table (`bots.h`) per tier controls reaction time, hit chance, view distance, and hearing radius — Beginner reacts slowly and misses often from close range, Expert reacts almost instantly and hits most shots from much further away. Selected via the new `bot_difficulty` cvar (0-3), applied to every bot in the match; no per-bot difficulty mixing within one match yet.
- [x] Fuller bot perception: hearing (gunfire — the player's own and other bots' — reported to `BotSystem::update()` as a position, alerts any bot without a stronger reason to react whose difficulty-tuned hearing radius reaches it, sending it to investigate) and a real reaction-time delay before firing (a freshly-spotted bot enters `Attack` and starts moving/aiming immediately, exactly like before, but won't actually pull the trigger until its difficulty's reaction time elapses). Still no simulated footstep hearing (only gunfire) and no omniscient aim/wallhacking, which there never was.
- [x] Bot navigation — a real polygon navmesh, not a heuristic: `nav.h`/`nav.cpp`'s `NavGraph` extracts every upward-facing BSP face above a minimum size straight from the map's own compiled geometry (`BspMap::faces()`) as a navmesh polygon, connects any two that share an edge (real adjacency, not a guessed sightline), and searches the result with A*. Spawn points, bomb-target/buy-zone centers, and a floor-probed grid sample are stitched in on top via line-of-sight edges, as a fallback layer for gameplay points that don't land exactly on a kept polygon and for maps where too few faces pass the filter. Bots path-find around obstacles instead of steering straight at a target when they can't already see it (`Idle`/`Search` movement; `Chase`/`Attack` still steer directly, since those states only exist because the bot already has a clear sightline). Still not hull-width-aware (a path can graze geometry a clearance-aware navmesh would route further from), no ledge/jump links, no per-node cost besides distance, and nodes are capped for build-time safety, so very large or geometrically complex maps get sparser coverage — see `nav.cpp`'s `kMaxNodes`.
- [x] Bot tactical behavior: buy decisions (`bots.cpp`'s `buyWeapon()` — spends the bot's money on the priciest affordable primary, falling back to the priciest affordable pistol; no eco/force-buy/save reasoning, no armor/utility purchases), a real attacker/defender role split (T bots push their assigned bomb site to plant it and hold a perimeter around the plant once it's down; CT bots hold a perimeter *around* their assigned site instead of standing in the middle of it, matching real defensive positioning more than the old "gather at the center" behavior), site assignment spread round-robin across bots so they don't all stack one site on multi-site maps, and rotations — a CT bot reassigns ("rotates") to whichever site the bomb actually gets planted at and pushes in to defuse range. Bots can now plant and defuse for real: a T bot standing in its site (not busy fighting) and a CT bot within defuse range of a planted bomb (ditto) each accumulate their own plant/defuse progress and complete it exactly like the player would (`round.bombPlanted`/`endRound(..., "BOMB_DEFUSED")`), on a timer entirely separate from the player's own so the two never double-count. Still open: no utility usage (bots don't have or throw grenades — none exist yet, see Grenades & thrown utility) and no explicit team chat beyond the shared "last saw the player here" callout hearing already provides.

### Matchmaking, ranking & social
- [ ] Any matchmaking at all (mode/region/skill-based queue, party-size handling, ping-aware server selection)
- [ ] Skill rating / rank tiers + rank-up/down flow (names/structure to be designed fresh, not copied)
- [ ] Leaderboards (global/region/country/friends; by rating, wins, kills, headshots, matches)
- [ ] Friends list (add/remove/online-status/invite/join lobby/block/report)
- [ ] Lobby/party system (pre-match lobby with ready-check, map/mode selection, invite/kick/promote-leader) — distinct from and prerequisite to matchmaking
- [ ] Player profile (level, rank, W/L, K/D, HS%, favorite weapons/maps, playtime, recent matches)
- [ ] Match history persistence (date, map, mode, result, per-match stats) — currently nothing about a match outcome is saved anywhere
- [ ] Post-match results screen (winner, personal/team stats, rewards, rank delta) beyond the current bare round-win scoreboard tally

### Server architecture & competitive integrity
- [ ] Client/server split of any kind — the engine is a single process with no network layer today, so every item below is currently a hard blocker, not a refinement
- [ ] Dedicated, authoritative server (server owns position/hit/damage/round/economy/objective state; the client only sends inputs)
- [ ] Configurable tick-rate / server simulation frequency
- [ ] Client-side prediction + server reconciliation, interpolation/extrapolation for other players
- [ ] Lag compensation (ping/jitter/packet-loss-aware hit registration)
- [ ] Anti-cheat: server-side plausibility checks (impossible movement/speed/fire-rate/position), detection heuristics for aimbot/triggerbot/wallhack/speedhack/no-recoil/no-spread/automation, file-integrity checks, suspicious-network-pattern flagging, and an escalation path (monitor → remove from match → ban → flag for review)
- [ ] Server-side input validation in general (reject impossible client-claimed state before it affects the match)

### Spectator & replay
- [ ] Spectator mode (follow a player, free camera, first/third person toggle, radar/killfeed/round-info overlay, team-overview mode)
- [ ] Demo/replay recording (positions, shots, hits, kills, grenades, objective actions, round state) and a player (play/pause/FF/rewind, jump to round, follow a player, free camera)
- [ ] A dedicated broadcast/observer mode for tournaments

### HUD, UI & settings
- [ ] Killfeed (attacker → victim, weapon icon, headshot/wallbang/assist/team-kill indicators)
- [ ] Quick team-comms / radio commands (no-mic-required "enemy spotted"/"going A"/"rotate"/etc.)
- [ ] Full main-menu shell beyond Play/Watch/Inventory/Store: Loadout, Profile, Match History, Rank, Leaderboard, Settings, Workshop, Community
- [ ] A real Settings menu: video (resolution/fullscreen/vsync/fps cap/texture-shadow-effects-AA-AO/particle quality), audio (master/music/effects/voice/UI volume), controls (full rebinding, not just movement defaults), mouse (sensitivity, ADS sensitivity, raw input, acceleration), crosshair customization (color/size/thickness/gap/outline/dot/dynamic vs. static)
- [ ] Controller input support (currently keyboard/mouse only)
- [ ] HUD configurability (currently a fixed layout)
- [ ] Minimap info gated by actual game knowledge (only show what a team legitimately knows) — today's single-player radar just shows the map + own dot, there's no "known enemy info" concept to gate yet

### Progression, cosmetics & content pipeline
- [ ] Loadout screen (equip a specific owned skin per weapon per team) — listed above too; repeated here because it's also the entry point for the wider cosmetic system below
- [ ] Skin system with rarity/quality/pattern/wear/float-style metadata that never affects weapon function (today's `csmenu` inventory economy is a reasonable skeleton for this but has no wear/pattern/float model)
- [ ] Persisting inventory/currency/profile to disk (currently resets every launch)
- [ ] Trade-up-style contracts, StatTrak-equivalent counters, stickers, sprays, music kits, agent/character cosmetics
- [ ] Workshop/custom-content support (community maps/skins/sounds/sprays/UI/modes), sandboxed for safety
- [ ] Store front for cosmetics-only purchases (no pay-to-win) — real-money payment rails specifically are already flagged above as needing a legal/compliance pass before any engineering work, independent of the cosmetic system itself

### Moderation & trust
- [ ] Player reporting (cheating/griefing/toxicity/abusive voice/team-killing/exploiting)
- [ ] Admin/moderation tooling (player/match search, report queue, bans, mutes, chat logs, replay access, server logs) with a mandatory audit trail

### Economy depth
- [ ] Loss-bonus streak, plant/defuse/kill economy bonuses beyond the current flat per-round reward
- [ ] The full buy-decision spectrum as a real strategic choice (full buy/force buy/eco/half-buy/save) — today there's one weapon slot and no team-wide economy signal to react to

## Planned future asset source: CS2/CS:GO-style weapon models

Once the MDL pipeline and gameplay are further along, the plan is to switch
the weapon view/world models from the original CS 1.5 (GoldSrc) assets to a
higher-fidelity CS:GO/CS2-style pack:
[`puRe991/CS2GO-Weapons-Pack-RELEASE`](https://github.com/puRe991/CS2GO-Weapons-Pack-RELEASE).

Not integrated yet — nothing has been pulled in. Notes for when we do:
- The repo's default branch only holds a `README.md` and `LICENSE`; the actual
  model/texture files are distributed as GitHub Release downloads, not
  committed to the tree, so switching means fetching a release archive, not
  just cloning the repo.
- Check the model format before assuming our `mdl.cpp` parser can load them
  as-is: a CS:GO/CS2-era pack may use a newer Source-engine model format
  (SMD/VTF/newer MDL versions) rather than GoldSrc's Studio Model v10 — likely
  needs its own loader path, not a drop-in replacement.
- Licensed CC0 1.0 (public domain dedication) — no redistribution restriction,
  but still credit the original authors as a courtesy: Stomatolog (model
  extraction, world model editing for shotguns); x F R 3 N Z Y M 0 V x and
  Volodya (world model rigging); CrazySlavModder (MIGI addons, inspect
  animations, model/texture/particle editing).

### Second candidate source: "CS2 MOD PACK" (GameBanana)

[gamebanana.com/mods/529076](https://gamebanana.com/mods/529076) — a fuller
CS2-style content pack (2,378 files, ~1GB unpacked ~1.9GB): weapon and player
sounds, weapon/player models, materials (models/vgui/weapons/decals/HUD),
overview images, fonts, and a handful of scripts. Plus a small companion
`font_8a987.zip` (CS:GO-style UI fonts: `cslogo.ttf`, `cstrike.ttf`,
`csd.ttf`) — that one *is* committed, at
`external_assets/cs2_mod_pack/font_8a987.zip`.

The main `cs2_mod_pack.zip` is **not** committed to this repo — it was
downloaded and handed to the user directly instead. Reasons:
- **License**: CC BY-NC-ND 4.0 (non-commercial, no derivatives, redistribution
  on other sites requires the author's permission). The user has stated they
  have that permission for this project, but it's still worth knowing the
  terms before touching these assets further.
- **Size**: ~1GB is over GitHub's 100MB hard per-file limit on a plain push.
  Git LFS was set up as the fix, but this fork has GitHub LFS uploads
  disabled (`can not upload new objects to public fork`) — not something
  fixable from here. GitHub Releases (which allow large files without LFS)
  were also considered, but no release-creation/asset-upload tool was
  available in this session.
- If LFS gets enabled for this fork later, or the file should go into a
  GitHub Release instead, re-download from the URL above and add it then.

## Design notes

- Coordinate system: the engine uses GoldSrc's native Z-up convention directly
  (no conversion layer), so map/model/entity coordinates can be used as-is.
- Asset parsing favors matching the original binary formats byte-for-byte
  over reinterpreting them, so real, unmodified CS asset files load without
  any conversion step.
