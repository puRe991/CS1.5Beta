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
- [x] Lightmaps: BSP lighting lump baked into a shared 2048x2048 atlas, multiplied into the base texture in the world shader. Style 0 only — no animated/switchable light styles yet, and no fullbright/runtime relighting.
- [ ] BSP visibility (PVS) culling for performance on large maps
- [x] Sky rendering: real 6-face GoldSrc skybox (`assets/tga.*` + `render/skybox.*`), loaded from the worldspawn `skyname` key, drawn camera-centered with depth test off. "sky"-textured faces are excluded from normal geometry so the skybox shows through.
- [ ] Decals (bullet holes, blood, etc.)
- [x] Particle effects (muzzle flashes, explosions, smoke) — a real world-space billboard sprite system (`render/particles.{h,cpp}`) driving the real GoldSrc `.spr` effect assets (muzzleflash2.spr, smokepuff.spr, fexplo.spr) added by the SPR loader: additive/alpha blending per the sprite's own render mode, animated through all its frames over the particle's lifetime, faded in/out at the edges, camera-facing (billboarded against the real view direction, not the view model's screen-locked axes). Wired to real gameplay events — every shot spawns a muzzle flash and, on a hit, a smoke puff at the impact point; a bomb detonation spawns an explosion at the bomb's position. Verified visually: screenshotted a mid-animation explosion frame rendering correctly as a bright additive fireball, plus the muzzle flash glowing correctly at point-blank range.
- [x] SPR sprite format support (used for effects, some HUD elements) — parses dsprite_t (orientation + render mode) and all frames, decoding the palette to RGBA per the sprite's render mode (normal/additive: opaque; alphatest: index 255 keyed transparent; indexalpha: palette used as a grayscale alpha ramp over white, since there's no per-entity render-color tinting yet). Verified against all 88 .spr files shipped with the game via `sprbatchtest` (0 failures) plus a manual pixel dump confirming an additive muzzle flash and an alphatest HUD sprite both decode correctly. Not yet wired into any renderer — animated group frames (none present in this asset set) are also not handled.
- [ ] MDL animation playback (currently only the static bind pose renders — no walk/run/shoot animation blending, no sequence system)
- [x] MDL attachment points (muzzle flash origin, weapon-to-hand attachment, etc.) — parsed from `mstudioattachment_t` and transformed through each attachment's owning bone bind-pose (same math path as vertices); the view model's muzzle flash is drawn at attachment 0's real position instead of a fixed offset. Player-model hand attachments are parsed and available via `MdlModel::attachments()`/`findAttachment()`, but the engine has no third-person/other-player rendering yet to attach a weapon model to.
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
- [x] Player health/death/respawn (`game_state.h/.cpp`): fall damage + a debug damage key, death freezes movement/shooting, auto-respawn at a random spawn point. No armor yet.
- [x] Round system (partial): Live/Intermission timer loop, round ends on timeout or death, HUD banner + auto-respawn into the next round. No real win conditions (needs opposing entities/AI), no buy-time phase, no economy loop yet — see the in-round buy menu item below.
- [x] Bomb defusal logic (`de_` maps): plant (T, hold E in a real func_bomb_target zone, 3s) and defuse (CT, hold E within 80 units of the bomb, 5s), 35s fuse, real win/loss round-end banners. Single-player only — no bots to plant/defuse against, and the bomb carries no visible world model yet.
- [ ] Hostage rescue mode logic (`cs_` maps)
- [x] Team assignment (`PlayerState.team`) drives team-based spawn selection for both initial spawn and respawn. No team-select screen, no auto-balance, no other players — 'N' is a debug key to switch team for testing.
- [x] HUD: crosshair, ammo, weapon name, health, round timer, money, team, zone/plant/defuse/bomb indicators, damage flash, and a real per-map radar (loads `cstrike/overviews/<map>.bmp`, player dot from our own BSP world bounds — not the overview file's own undocumented zoom/origin metadata)
- [x] In-round buy menu (`weapons.h`): B key, gated to standing in a real func_buyzone + round live, weapon catalog with prices, deducts money and swaps the equipped weapon/ammo. Money system: starts at 800, flat per-round reward (no real win/loss economy tied to it yet). Only affects the primary weapon slot — no pistol/grenade/armor purchases, no per-team price differences yet.
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
