# River City Quest SNES: Roadmap

A full port of [River City Quest](https://github.com/doodersrage/river-city-quest) to a real
Super Nintendo ROM, built in stages. Each stage ends with a ROM that boots and plays.

## How the port works

- **Code:** C with [PVSnesLib](https://github.com/alekmaul/pvsneslib) (65816), with assembly where speed matters.
  GitHub Actions builds the ROM on every push and publishes it as the `latest` release.
- **Art:** `tools/export` runs the web game's own drawing code headlessly and exports every map and sprite as PNG.
  `tools/snesconv.py` turns those into SNES 4bpp tiles, palettes, tilemaps and collision.
  `tools/preview.py` decodes the SNES data back to PNG, so every conversion can be checked by eye.
- **Source of truth:** the web game. Maps, text and rules are extracted from it, not retyped.

## Hardware budget

| Resource | Limit | Plan |
|---|---|---|
| BG tiles per layer | 1,024 (4bpp) | Speckle limited to 4 variants per tile type. Busiest map: 466 tiles. |
| BG palettes | 8 × 15 colours | Greedy packing per map, global median cut if needed. Palette 0 keeps 3 colours for the text layer (BG3 shares CGRAM 1-3). |
| VRAM | 64 KB | BG1 tiles 0x0000, BG1 map 0x4000 (64×64), BG3 font 0x5000, BG3 map 0x5800, sprites 0x6000 (hero tiles 0-31, NPCs 32-127) |
| Work RAM | 128 KB | Game state, NPCs, enemies; save data to SRAM. WRAM is not cleared at power-on, so every variable is set in code; initialised globals land in bank $7F |
| ROM | 2 MB now, up to 4 MB | Each outdoor map is about 26 KB, each room about 15 KB (one 32 KB bank each) |
| Sprites | 128 on screen, 32 per scanline | 8x8 small (HUD, pickups) and 16x16 large. OBJ tiles: hero 0-31, NPCs 32-127, enemies 128-159, paddle 160-223, HUD 224-255. Palettes: hero 0, NPCs 1-4, enemies 5-6, paddle and HUD 7 |
| Sound | SPC700, 64 KB audio RAM | Chiptunes rebuilt as tracker modules (snesmod) |

## Stages

1. **Walk Richmond** (done in first build). All 24 outdoor neighbourhoods with the web game's art, the hero with walk frames, collision, scrolling and fades between neighbourhoods.
2. **Text and talking** (done). BG3 font (Press Start 2P) and dialogue box, markers, landmarks, murals and lamps, 70 NPCs standing in place with their lines.
   Still to come with later stages: the journal menu for the 66 stories, Spanish text, NPCs walking around, and talk that changes with progress (gifts, trades and challenges currently show their opening lines only).
3. **Combat** (done). Paddle swing (the game's own paddle drawn at 16 angles) and hold-to-charge spin, hearts and coin HUD, the 7 outdoor enemy types with the web game's behaviours, hit stun and knockback, coin and heart drops, waking in Capitol Square.
   Not yet: cutting bushes, hit sparks and puffs, sound, the hero's hurt flash colour, and night enemies (stage 8).
4. **Doors and interiors** (done). Doors into all 24 single-room interiors and caves and back out, rooms centred on screen, bush cutting (from stage 3), the 26 chests with coins and heart charms, Carytown Goods and the 17th Street Market. 2 MB ROM.
   Not yet: the dog and furniture in the row house, cooking, the arcade, and what most shop items do (camera, rod, cards, satchel); they're recorded as bought for the stages that add them.
5. **Dungeon engine and Tredegar** (done). Room-to-room travel inside an area, rooms that stay cleared, chests that appear when a room is solved, small keys per dungeon, locked doors, gates tied to solved rooms, pits and iron posts, the Grappling Hook, projectiles, a 48x48 boss built from 9 sprites in the second OBJ name table, boss stairs and heart containers, waking at the dungeon entrance. Tredegar Iron Works and the Iron Foreman are complete.
6. **Items and the other dungeons, together (done).** Each dungeon needs the item it hides, so they come in pairs: the Quarry and Blasting Powder (done: cracked walls, rubble, Old Snapper, the Granite Keystone, SELECT to switch items, keg drops), the Pyramid and the Gas Lantern (done: the Archivist's gift, braziers and lamp-lit rooms, the Cast-Iron Shield, reflected shots, the Wraith, the Iron Keystone; dark rooms come with lighting in stage 8), the Poe cellar (done: the Gold-Bug, pickups lying in rooms, the Raven, the Brick Keystone; boss sprite cells are now packed so four bosses fit the second OBJ table), the Carillon (done: the trading quest and kayak, river crossing, the Carillon Key, push blocks and switches, the Bronze Bell, the Bronze Keystone, heart pieces), the Conservatory (done: the Watering Can, sprouts and vine bridges, the Titan Arum; a grown sprout is walkable here, since in the web game the stalk blocks the only way onto its vine), the item menu and gear (done: START menu, Baseballs, Running Shoes and the race bib, the Bicycle at about 2 px a frame everywhere rather than faster on paths, Ginger Beer and refills, shop restocks and the Big Satchel; the Fishing Rod and Camera come with the minigames and town life), and the Church Hill Tunnel finale (done: the keystone door, the Flooded Siding, the Signal Room, the Richmond Vampire, and the ending: the lamps come back on in five neighbourhoods, then the credits. The relight is shown by daylight for now; lamps lit at night come with lighting in stage 8. There's no name entry yet, so the credits star Rory).
7. **Music and sound (done).** Every web game track rebuilt for the SPC700, plus sound effects. tools/mkaudio.py reads the web game's own songs (lead, bass and drum lines) and writes one Impulse Tracker module per song, with looped single-cycle square and triangle samples and one-shot drums, one row per eighth note; SNESMOD (PVSnesLib's driver) plays them from a soundbank in ROM banks 52 and up. Sixteen sound effects (swing, hit, clank, hurt, defeat, coin, heart, talk, menu, fuse, explosion, item jingle, secret jingle, boss shot, slam, fanfare) are the web game's sfx() and tune() calls rendered to samples. Each place plays the song the web game picks: the town theme outdoors and in lit buildings, its own theme south of the river, the forge in the Tredegar works, the dungeon theme in dungeons and caves, the boss theme during a fight, and the victory theme over the ending. The title and night themes are in the soundbank for stages 8 and 11. Changing song uploads a module to the sound chip, which takes a moment; it happens during the screen fade between places.
8. **Day, night and weather.** Palette fades and colour math for night, lamp and window glow, rain and snow on a sprite or BG3 overlay, fog via colour math.
9. **Town life.** Walking townspeople, buses and cars, pigeons, the trading and bateau quests.
10. **Challenges and minigames.** The 14 neighbourhood challenges, fishing, landmark cards, heart pieces.
11. **Saves and polish.** Battery-backed SRAM, three save slots, title screen, name entry, credits, a real-hardware test on a flash cart.

## Known gaps to revisit

- **Palette quality:** busy maps such as Broad Street lose some hues (green planters go grey-teal) when cut to 8 palettes.
  Fix options: per-tile palette choice weighted by visibility, or moving small details to sprites or a second BG layer.
- **Variety:** 4 speckle variants per tile type loses a little variety (fewer red-berry trees). 6 to 8 variants may still fit.
- **Animated tiles:** water and flame frames from the web game are not exported yet (frames 1 to 3).

## Performance notes

- Every sprite goes through `sprPut` / `sprHide` (main.c), which skip OAM calls for sprites that stay hidden. Calling `oamSetEx` and `oamSet` for every slot every frame cost about 20% of the frame budget.
- The HUD redraws only when health, coins or the B item change.
- The play test measures nothing yet, but `frameCount` against emulator frames is the check to use: 120/120 outdoors and in boss rooms as of the Quarry.
