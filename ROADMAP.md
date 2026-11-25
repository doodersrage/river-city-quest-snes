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
| Work RAM | 128 KB | Game state, NPCs, enemies; save data to SRAM |
| ROM | 1 MB now, up to 4 MB | Each outdoor map is about 26 KB |
| Sprites | 128 on screen, 32 per scanline | Hero, enemies, NPCs, pickups, effects |
| Sound | SPC700, 64 KB audio RAM | Chiptunes rebuilt as tracker modules (snesmod) |

## Stages

1. **Walk Richmond** (done in first build). All 24 outdoor neighbourhoods with the web game's art, the hero with walk frames, collision, scrolling and fades between neighbourhoods.
2. **Text and talking** (done). BG3 font (Press Start 2P) and dialogue box, markers, landmarks, murals and lamps, 70 NPCs standing in place with their lines.
   Still to come with later stages: the journal menu for the 66 stories, Spanish text, NPCs walking around, and talk that changes with progress (gifts, trades and challenges currently show their opening lines only).
3. **Combat.** Paddle swing and spin attack, hearts, enemies from the web game with their behaviour, pickups, death and continue.
4. **Doors and interiors.** Building entrances, the 11 interiors, the row house and the dog, shops and rupees.
5. **Dungeons.** Small keys, locked doors, push blocks, braziers, cracked walls. One dungeon end to end first (Tredegar), then the other six and the finale.
6. **Items.** Gas Lantern, Baseballs, Blasting Powder, Gold-Bug, Running Shoes, Grappling Hook, Fishing Rod, Watering Can, Ginger Beer, shield, kayak.
7. **Music and sound.** Every web game track rebuilt for the SPC700, plus sound effects.
8. **Day, night and weather.** Palette fades and colour math for night, lamp and window glow, rain and snow on a sprite or BG3 overlay, fog via colour math.
9. **Town life.** Walking townspeople, buses and cars, pigeons, the trading and bateau quests.
10. **Challenges and minigames.** The 14 neighbourhood challenges, fishing, landmark cards, heart pieces.
11. **Saves and polish.** Battery-backed SRAM, three save slots, title screen, name entry, credits, a real-hardware test on a flash cart.

## Known gaps to revisit

- **Palette quality:** busy maps such as Broad Street lose some hues (green planters go grey-teal) when cut to 8 palettes.
  Fix options: per-tile palette choice weighted by visibility, or moving small details to sprites or a second BG layer.
- **Variety:** 4 speckle variants per tile type loses a little variety (fewer red-berry trees). 6 to 8 variants may still fit.
- **Animated tiles:** water and flame frames from the web game are not exported yet (frames 1 to 3).
