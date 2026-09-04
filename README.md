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

Controls: WASD to move, mouse to look, Space/Ctrl for up/down, Esc to quit.

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
    mat4.h              # minimal lookAt/perspective matrix math (no GLU/GLM dependency)
    menu_main.cpp        # CS:GO-inspired main menu (Play/Watch/Inventory/Store)
    inventory.{h,cpp}    # fictive-currency skin/case economy + case-opening RNG
    ui/
      ui.{h,cpp}          # minimal immediate-mode 2D UI (rects, buttons, text)
      font5x7.h            # built-in 5x7 bitmap font (our own, not copied)
    assets/
      pak.{h,cpp}        # Quake-style PAK archive reader
      wad.{h,cpp}        # WAD3 texture package parser (palette-indexed -> RGBA8)
      bsp.{h,cpp}        # BSP v30 map parser: geometry, textures, entities, hull collision
      mdl.{h,cpp}        # Studio Model (.mdl v10) parser: bones, textures, skinned mesh
    tools/
      mapshot.cpp        # standalone map screenshot tool
      modelshot.cpp      # standalone model screenshot tool
      mdlbatchtest.cpp   # batch-load a list of .mdl files, report pass/fail per file
```

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

## To Do — what's still needed for a full, playable Counter-Strike

### Rendering
- [x] Shader-based world rendering: GLSL vertex/fragment shader + one VBO for all BSP geometry (`render/`), replacing per-face `glBegin`/`glEnd`. Bullet marks, the view model, and the 2D UI/HUD still use the legacy fixed-function path (GL 2.1 compatibility profile allows mixing both) — converting those is a follow-up, as is eventually moving to a core 3.3+ context
- [ ] Lightmaps (BSP lighting lump is parsed as raw data but not yet applied — faces render unlit/texture-only)
- [ ] BSP visibility (PVS) culling for performance on large maps
- [ ] Sky rendering (skybox/skydome instead of the current flat clear color)
- [ ] Decals (bullet holes, blood, etc.)
- [ ] Particle effects (muzzle flashes, explosions, smoke)
- [ ] SPR sprite format support (used for effects, some HUD elements)
- [ ] MDL animation playback (currently only the static bind pose renders — no walk/run/shoot animation blending, no sequence system)
- [ ] MDL attachment points (muzzle flash origin, weapon-to-hand attachment, etc.)
- [x] View model (first-person weapon model) rendering in the main engine window — own narrow-FOV projection pass + depth clear so it never clips into world geometry (pose/offset is a fixed approximation, not attachment-point accurate — see animation TODO below)

### Physics & Movement
- [x] Gravity + jump + ground detection (probe-based, snaps back on floor/ceiling contact instead of clipping through) — verified against real de_dust2 geometry with logged position/velocity
- [ ] Ducking, ground friction/acceleration curves, air control (movement is instant-velocity, not accelerated — no strafe-jumping, no "Source feel" yet)
- [ ] Full hull-based collision for other hulls (crouching hull, large hull) — only the standard player hull is implemented
- [x] Line trace for hitscan (`BspMap::traceLine`) — a stepped-sampling trace, not a proper swept hull trace, so it's slightly less precise than the real engine's; fine for now, worth revisiting
- [ ] Entity physics (moving platforms, doors, breakables)

### Gameplay Systems
- [x] Entity system (`entities.h/.cpp`): all spawn points (CT/T-tagged) instead of just the first found, real bomb-target/buy-zone regions from BSP submodel bounds, live "BOMBSITE"/"BUY ZONE" HUD indicators — verified against de_dust2's real entity counts (40 spawns, 2 bomb targets, 2 buy zones). Other entity classes (func_door, func_button, breakables, triggers, lights) are still just static unparsed geometry/data.
- [x] Basic hitscan firing: left click, 30-round magazine, R to reload, impact marker at the hit point — no damage/recoil/spread/switching yet, and only one weapon (AK47) is wired in at all
- [ ] Player health/armor/death/respawn
- [ ] Round system: buy time, round win/loss conditions, economy
- [ ] Bomb defusal mode logic (plant/defuse, `de_` maps)
- [ ] Hostage rescue mode logic (`cs_` maps)
- [ ] Team system (T/CT), team-based spawning
- [ ] HUD (crosshair + ammo counter exist; health, money, timer, radar don't)
- [ ] In-round buy menu (distinct from the main-menu Store — this is the classic F-key weapon purchase menu during the buy phase)
- [ ] Scoreboard

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
- [ ] Sound engine (currently no audio playback at all)
- [ ] WAV loading and 3D positional audio
- [ ] Weapon/footstep/ambient sound triggers

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

## Design notes

- Coordinate system: the engine uses GoldSrc's native Z-up convention directly
  (no conversion layer), so map/model/entity coordinates can be used as-is.
- Asset parsing favors matching the original binary formats byte-for-byte
  over reinterpreting them, so real, unmodified CS asset files load without
  any conversion step.
