"use strict";
/* River City Quest, SNES edition: a later-SNES look (Chrono Trigger, Secret of Mana) for the tile art.
   Loaded into the exporter after the web game's own scripts, it redraws the ground, trees, roofs and facades with
   shaded colour ramps: light from the top left, shadows that lean purple, highlights that lean warm, soft rounded
   shapes and real texture. Only the SNES export uses it; the web game keeps its own art.
   Colours come from fixed ramps so a map's tiles still fit eight 15-colour palettes. */

/* ---------- ramps ---------- */
const SG = ['#173a2c', '#22553a', '#2f7040', '#428c44', '#5ea84a', '#84c45a', '#b4dc74'];   // grass, dark to light
const SST = ['#2e2838', '#544a58', '#776a72', '#968a8c', '#b4a8a4', '#d2c8bc', '#ece4d4'];  // warm stone
const SRF = ['#1c1a2a', '#2c2a40', '#3e3c56', '#54526c', '#6c6a84', '#8c8aa2'];             // slate roofs
const SWD = ['#2a1810', '#4a2c1a', '#6c4426', '#8e5e34'];                                    // bark and wood
const SSA = ['#9a7e58', '#bea270', '#d8c08c', '#ecdcae', '#f8f0d4'];                          // sand

function hx(c) { return [parseInt(c.slice(1, 3), 16), parseInt(c.slice(3, 5), 16), parseInt(c.slice(5, 7), 16)]; }
function toHex(r, g2, b) { return '#' + [r, g2, b].map(v => Math.max(0, Math.min(255, Math.round(v))).toString(16).padStart(2, '0')).join(''); }
/* a five-step ramp from one base colour: darker steps slide toward deep violet, lighter toward warm cream */
function rampOf(c) {
  const [r, g2, b] = hx(c), D = [40, 24, 56], L = [255, 244, 214];
  const mix = (t, to) => toHex(r + (to[0] - r) * t, g2 + (to[1] - g2) * t, b + (to[2] - b) * t);
  return [mix(.62, D), mix(.34, D), c, mix(.3, L), mix(.6, L)];
}
const RAMPS = new Map();
function ramp(c) { if (!RAMPS.has(c)) RAMPS.set(c, rampOf(c)); return RAMPS.get(c); }

/* ---------- grass: dense tufts in four greens, a few lighter blades catching the sun ---------- */
grass = function (x, y, h) {
  R(x, y, 16, 16, SG[3]);
  for (let i = 0; i < 10; i++) {                           // darker clumps
    const q = HH(h, i, 17), a = x + q % 15, b = y + (q >> 4) % 15;
    R(a, b, 2, 1, SG[2]); R(a + 1, b - 1, 1, 1, SG[2]);
  }
  for (let i = 0; i < 7; i++) {                            // tufts: a dark base, a mid blade, a lit tip
    const q = HH(h, i, 23), a = x + 1 + q % 13, b = y + 3 + (q >> 4) % 12;
    R(a, b, 1, 1, SG[2]); R(a + 1, b, 1, 1, SG[2]);
    R(a, b - 1, 1, 1, SG[4]); R(a + 1, b - 2, 1, 2, SG[4]); R(a + 2, b - 1, 1, 1, SG[4]);
    R(a + 1, b - 3, 1, 1, SG[5]);
  }
  if (h % 4 == 0) { const q = HH(h, 9, 9), a = x + 3 + q % 9, b = y + 4 + (q >> 4) % 8; R(a, b, 3, 1, SG[1]); R(a + 1, b - 1, 1, 1, SG[5]); R(a + 1, b - 2, 1, 1, SG[6]); }
};
flowers = function (x, y, h) {
  grass(x, y, h + 1);
  const cs = [['#f8e060', '#c89830'], ['#f890a8', '#c05070'], ['#ffffff', '#b8b0d0'], ['#b890f8', '#7058b8']];
  for (let i = 0; i < 4; i++) {
    const q = HH(h, i, 3), a = x + 2 + (i & 1) * 8 + q % 4, b = y + 2 + (i >> 1) * 8 + (q >> 3) % 4, c = cs[(q >> 6) % 4];
    R(a, b + 2, 1, 2, SG[1]); R(a - 1, b + 3, 1, 1, SG[4]);
    R(a - 1, b, 3, 2, c[0]); R(a, b - 1, 1, 1, c[0]); R(a + 1, b + 1, 1, 1, c[1]); R(a - 1, b + 1, 1, 1, c[1]);
    R(a, b, 1, 1, '#f8c040');
  }
};
/* cobbles: rounded setts, each lit on top and shaded below, deep joints between */
cobble = function (x, y, h) {
  R(x, y, 16, 16, SST[1]);
  for (let r = 0; r < 4; r++) {
    const segs = r & 1 ? [[0, 4], [4, 8], [12, 4]] : [[0, 8], [8, 8]];
    segs.forEach((sg, i) => {
      const q = HH(h, r, (r & 1 && i == 2) ? 0 : i) % 3, a = x + sg[0], b = y + r * 4, w = sg[1] - 1;
      R(a, b, w, 3, SST[3 + (q == 2 ? 0 : q == 1 ? 1 : 0)]);
      R(a + 1, b, w - 2, 1, SST[5]); R(a, b + 1, 1, 1, SST[4]);
      R(a + 1, b + 2, w - 1, 1, SST[2]); R(a + w - 1, b + 1, 1, 1, SST[2]);
      if (q == 0) R(a + 2 + (h + r) % Math.max(1, w - 3), b + 1, 1, 1, SST[4]);
    });
    R(x, y + r * 4 + 3, 16, 1, SST[0]);
  }
};
/* sidewalk: big slabs with a lit top-left edge, a shaded seam, and a little wear */
sidewalk = function (x, y, h) {
  R(x, y, 16, 16, SST[4]);
  R(x, y, 16, 1, SST[6]); R(x, y, 1, 16, SST[6]); R(x + 1, y + 1, 14, 1, SST[5]); R(x + 1, y + 1, 1, 14, SST[5]);
  R(x, y + 15, 16, 1, SST[2]); R(x + 15, y, 1, 16, SST[2]); R(x + 1, y + 14, 14, 1, SST[3]); R(x + 14, y + 1, 1, 13, SST[3]);
  for (let i = 0; i < 5; i++) { const q = HH(h, i, 9); R(x + 3 + q % 10, y + 3 + (q >> 4) % 10, 1, 1, i & 1 ? SST[3] : SST[5]); }
  if (h % 7 == 0) { R(x + 5, y + 6, 3, 1, SST[2]); R(x + 8, y + 7, 2, 1, SST[2]); R(x + 10, y + 8, 1, 2, SST[2]); R(x + 6, y + 7, 2, 1, SST[5]); }
};
sand = function (x, y, h) {
  R(x, y, 16, 16, SSA[2]);
  for (let i = 0; i < 9; i++) { const q = HH(h, i, 5); R(x + q % 15, y + (q >> 4) % 15, 1 + (q >> 9) % 2, 1, i % 3 == 0 ? SSA[1] : i % 3 == 1 ? SSA[3] : SSA[2]); }
  for (let i = 0; i < 2; i++) { const q = HH(h, i, 13), a = x + q % 10, b = y + 3 + (q >> 4) % 10; R(a, b, 5, 1, SSA[1]); R(a + 1, b - 1, 4, 1, SSA[3]); }
  if (h % 6 == 0) { R(x + 9, y + 9, 3, 2, SSA[4]); R(x + 9, y + 11, 3, 1, SSA[0]); }
};
/* where grass meets paving: a soft, ragged edge with a lit lip and a shadow under it */
fringe = function (x, y, h, nb) {
  for (const [dx, dy] of [[0, -1], [0, 1], [-1, 0], [1, 0]]) {
    if (!GRASSY.includes(nb(dx, dy))) continue;
    for (let i = 0; i < 16; i++) {
      const q = HH(h, i + dx * 3, dy + 5), l = 1 + q % 3 + (i % 5 == 2 ? 1 : 0);
      const a = dx ? (dx < 0 ? 0 : 16 - l) : i, b = dy ? (dy < 0 ? 0 : 16 - l) : i, w = dx ? l : 1, hh = dy ? l : 1;
      R(x + a, y + b, w, hh, q & 8 ? SG[3] : SG[4]);
      const ex = dx ? (dx < 0 ? a + l : a - 1) : a, ey = dy ? (dy < 0 ? b + l : b - 1) : b;
      R(x + ex, y + ey, 1, 1, dy < 0 || dx < 0 ? SST[2] : SG[2]);                     // shadow cast onto the paving
      if (dy > 0) R(x + a, y + b, 1, 1, SG[5]);                                        // lit tips facing the sun
    }
  }
  const G = (dx, dy) => GRASSY.includes(nb(dx, dy));
  if (G(0, -1) && G(-1, 0)) R(x, y, 3, 3, SG[3]);
  if (G(0, -1) && G(1, 0)) R(x + 13, y, 3, 3, SG[3]);
  if (G(0, 1) && G(-1, 0)) R(x, y + 13, 3, 3, SG[3]);
  if (G(0, 1) && G(1, 0)) R(x + 13, y + 13, 3, 3, SG[3]);
};

/* ---------- trees: a round canopy in five greens with a lit crown and a dark underside ---------- */
const CANOPY = [5, 9, 11, 13, 14, 14, 14, 14, 13, 12, 10, 7];
function canopy(x, y, G, h) {   // G: five greens, dark to light
  CANOPY.forEach((w, i) => { const a = x + 8 - (w >> 1); R(a, y + i, w, 1, G[2]); });
  CANOPY.forEach((w, i) => { const a = x + 8 - (w >> 1); R(a, y + i, 1, 1, G[0]); R(a + w - 1, y + i, 1, 1, G[0]); });
  R(x + 6, y - 1, 5, 1, G[0]); R(x + 5, y + 12, 7, 1, G[0]);
  for (let i = 7; i < 12; i++) { const w = CANOPY[i], a = x + 8 - (w >> 1); R(a + 1, y + i, w - 2, 1, i > 9 ? G[0] : G[1]); }   // shaded underside
  R(x + 4, y + 2, 5, 3, G[3]); R(x + 3, y + 4, 3, 2, G[3]); R(x + 9, y + 3, 2, 2, G[3]);                          // lit crown, top left
  R(x + 5, y + 2, 2, 1, G[4]); R(x + 4, y + 3, 1, 1, G[4]); R(x + 10, y + 3, 1, 1, G[4]);
  for (let i = 0; i < 5; i++) { const q = HH(h, i, 21), a = x + 3 + q % 10, b = y + 3 + (q >> 4) % 6; R(a, b, 2, 1, G[1]); R(a, b - 1, 1, 1, G[3]); }   // leaf clusters
}
blob = function (x, y, mid, dark, out) {   // the web game's tree blob, rebuilt as a shaded canopy from its colour
  const r = ramp(mid);
  canopy(x, y, [out, dark, mid, r[3], r[4]], 0);
};

/* ---------- roofs: slate rows, a ridge catching the light, eaves in shadow ---------- */
roof = function (x, y, h, nb) {
  R(x, y, 16, 16, SRF[2]);
  for (let r = 0; r < 16; r += 4) {
    const o = (r >> 2) & 1 ? 2 : 0;
    R(x, y + r + 3, 16, 1, SRF[1]);
    for (let i = o; i < 16; i += 4) { R(x + i, y + r, 1, 3, SRF[1]); R(x + i + 1, y + r, 2, 1, SRF[3]); }
    const q = HH(h, r, 6); if (q % 3 == 0) R(x + (q >> 3) % 13, y + r + 1, 3, 1, SRF[4]);
  }
  if (nb(0, -1) != 'B') { R(x, y, 16, 3, SRF[4]); R(x, y, 16, 1, SRF[5]); R(x, y + 3, 16, 1, SRF[0]); }
  if (nb(-1, 0) != 'B') { R(x, y, 2, 16, SRF[4]); R(x + 2, y, 1, 16, SRF[1]); }
  if (nb(1, 0) != 'B') { R(x + 14, y, 2, 16, SRF[1]); R(x + 13, y, 1, 16, SRF[0]); }
  if (nb(0, 1) != 'B') R(x, y + 14, 16, 2, SRF[0]);
  const ft = h % 6;
  if (ft == 0) {   // a chimney with its shadow
    R(x + 9, y + 9, 5, 4, SRF[0]); R(x + 5, y + 3, 6, 8, SWD[1]); R(x + 5, y + 3, 6, 1, '#c47a5a'); R(x + 5, y + 4, 2, 6, '#a85a40');
    R(x + 7, y + 4, 4, 6, '#7a3a2c'); R(x + 5, y + 2, 6, 2, SST[3]); R(x + 5, y + 2, 6, 1, SST[5]);
  } else if (ft == 1) {   // a skylight
    R(x + 4, y + 5, 8, 7, SRF[0]); R(x + 5, y + 5, 6, 6, '#2e4a7a'); R(x + 5, y + 5, 6, 1, '#a8c8e8'); R(x + 5, y + 6, 1, 5, '#5a82b8'); R(x + 4, y + 11, 8, 1, SRF[4]);
  }
};

/* ---------- walls inside: shaded blocks under a dark cornice ---------- */
wallFace = function (s, x, y, h) {
  const c = s.wl || ['#5b5368', '#463f52', '#756c85'], r = ramp(c[0]);
  R(x, y, 16, 16, r[2]);
  for (let row = 1; row < 4; row++) {
    const b = y + row * 4, o = row & 1 ? 4 : 0;
    R(x, b + 3, 16, 1, r[0]);
    for (let i = o; i < 16; i += 8) { R(x + i, b, 1, 3, r[0]); R(x + i + 1, b, 6, 1, r[3]); R(x + i + 1, b + 2, 6, 1, r[1]); }
  }
  R(x, y, 16, 4, '#2a2434'); R(x, y, 16, 1, '#4a445a'); R(x, y + 3, 16, 1, '#14101c'); R(x, y + 13, 16, 3, r[1]); R(x, y + 15, 16, 1, r[0]);
};

/* ---------- facades: every brick shaded, a cream cornice, windows set back into the wall ---------- */
const oldFacade = facade;
facade = function (s, x, y, h, tx, ty, roofAbove, plain) {
  const pl = s.pal || [BRICK], c = pl[(tx + (ty > 5 ? 3 : 0)) % pl.length];
  if (c[3] == 'clap' || s.hs == 'glass') return oldFacade(s, x, y, h, tx, ty, roofAbove, plain);
  const r = ramp(c[0]);
  R(x, y, 16, 16, r[2]);
  for (let row = 0; row < 4; row++) {
    const b = y + row * 4, o = row & 1 ? 4 : 0;
    R(x, b + 3, 16, 1, r[0]);
    for (let i = o - 8; i < 16; i += 8) {
      const a = Math.max(x, x + i), w = Math.min(x + i + 7, x + 16) - a;
      if (w <= 0) continue;
      const q = HH(h, row, i + 8) % 4;
      R(a, b, w, 3, q == 0 ? r[3] : r[2]);
      R(a, b, w, 1, q == 0 ? r[4] : r[3]); R(a, b + 2, w, 1, r[1]);
      if (x + i + 7 < x + 16) R(x + i + 7, b, 1, 3, r[0]);
    }
  }
  R(x + 15, y, 1, 16, r[1]);
  // the cornice (or the shadow of the roof above)
  if (roofAbove) { R(x, y, 16, 2, r[0]); R(x, y + 2, 16, 1, r[1]); }
  else { R(x, y, 16, 3, '#e8dcc0'); R(x, y, 16, 1, '#fff8e8'); R(x, y + 3, 16, 1, '#a08c6c'); for (let i = 1; i < 16; i += 3) R(x + i, y + 2, 1, 1, '#a08c6c'); R(x, y + 4, 16, 1, r[0]); }
  if (isDecor(s, tx, ty) || plain) return;
  const ft = h % 4;
  if (s.hs == 'fan' && ft > 0 && ft < 3) {   // the Fan's bay: three tall windows in a cream surround
    R(x + 2, y + 4, 12, 11, '#c8b898'); R(x + 2, y + 4, 12, 1, '#fff8e8'); R(x + 2, y + 4, 1, 11, '#efe4c8'); R(x + 13, y + 5, 1, 10, '#a08c6c');
    for (const a of [4, 7, 10]) {
      const lit = (h >> (5 + a)) % 3 == 0;
      R(x + a, y + 6, 2, 7, lit ? '#f8d868' : '#1c2a4a'); R(x + a, y + 6, 2, 1, lit ? '#fff6c0' : '#5a82b8');
      if (!lit) { R(x + a, y + 7, 1, 2, '#2e4a7a'); R(x + a + 1, y + 10, 1, 1, '#a8c8e8'); }
      R(x + a, y + 13, 2, 1, '#a08c6c');
    }
    R(x + 2, y + 15, 12, 1, r[0]);
    if (s.ow) (s.wins || (s.wins = {}))[x + ',' + y] = [x, y, (h >> 7) % 3 != 0];
    if (wantsPorch(s, h)) ironPorch(x, y);
    return;
  }
  if (s.hs && s.hs != 'jw' && s.hs != 'fan' && styledFacade(s, x, y, h, tx, ty, c)) { if (wantsPorch(s, h)) ironPorch(x, y); return; }
  if (ft == 0) {   // a door: panelled, under a lit transom, on a stone step
    R(x + 3, y + 4, 10, 12, '#efe4c8'); R(x + 3, y + 4, 10, 1, '#fff8e8'); R(x + 12, y + 5, 1, 11, '#a08c6c');
    R(x + 4, y + 5, 8, 3, '#1c2a4a'); R(x + 5, y + 5, 2, 1, '#f8d868');
    R(x + 4, y + 8, 8, 8, SWD[2]); R(x + 4, y + 8, 8, 1, SWD[3]); R(x + 5, y + 9, 2, 3, SWD[1]); R(x + 9, y + 9, 2, 3, SWD[1]); R(x + 5, y + 13, 6, 2, SWD[1]);
    R(x + 10, y + 12, 1, 1, '#f2c14e'); R(x + 2, y + 15, 12, 1, SST[5]);
  } else if (ft < 3) {   // a window with a lintel, a sill, and the glass in shadow
    const lit = (h >> 5) % 3 == 0;
    if (s.ow) (s.wins || (s.wins = {}))[x + ',' + y] = [x, y, (h >> 7) % 3 != 0];
    R(x + 2, y + 4, 12, 2, '#efe4c8'); R(x + 2, y + 4, 12, 1, '#fff8e8');
    R(x + 3, y + 6, 10, 8, '#c8b898'); R(x + 4, y + 6, 8, 7, lit ? '#f8d868' : '#1c2a4a');
    if (lit) { R(x + 4, y + 6, 8, 1, '#fff6c0'); R(x + 4, y + 7, 2, 2, '#fff6c0'); }
    else { R(x + 4, y + 6, 8, 1, '#5a82b8'); R(x + 5, y + 7, 1, 2, '#2e4a7a'); R(x + 10, y + 10, 1, 1, '#a8c8e8'); }
    R(x + 7, y + 6, 2, 7, '#e8dcc0'); R(x + 4, y + 9, 8, 1, '#e8dcc0');
    R(x + 2, y + 13, 12, 1, '#fff8e8'); R(x + 2, y + 14, 12, 1, '#a08c6c'); R(x + 3, y + 15, 10, 1, r[0]);
  }
  if (s.awn && ft < 3) {
    const ac = [['#e83048', '#ffffff', '#a01c30'], ['#2e9d6b', '#ffffff', '#1c6a48'], ['#3d63c8', '#fff2a8', '#283e88']][(tx >> 1) % 3];
    for (let i = 0; i < 16; i += 2) { R(x + i, y + 3, 2, 3, ac[(i >> 1) & 1]); R(x + i, y + 6, 2, 1, (i >> 1) & 1 ? '#c8c8d0' : ac[2]); }
    R(x, y + 3, 16, 1, '#fff8e8'); R(x, y + 7, 16, 1, r[0]);
  }
  if (s.hs == 'jw' && ft > 0 && wantsPorch(s, h)) ironPorch(x, y);
};

/* ---------- tree cells: grass, a soft shadow, the trunk, then the canopy ---------- */
const OAK = ['#10301e', '#1a4a2a', '#2a6a36', '#48903e', '#78b858'];
const MAG = ['#0a2418', '#143c28', '#1f5a3a', '#3a8a5a', '#68b080'];
const oldTile = tile;
tile = function (s, ch, tx, ty, fr, plain) {
  if (ch != 'T') return oldTile(s, ch, tx, ty, fr, plain);
  const x = tx * TS, y = ty * TS, h = HH(tx, ty, s.seed);
  const nb = (dx, dy) => { const X = tx + dx, Y = ty + dy; return X < 0 || Y < 0 || X >= s.w || Y >= s.h ? ch : s.g[Y][X]; };
  grass(x, y, h);
  const tk = s.interior ? 'oak' : treeKind(h);
  if (tk == 'dogwood') { oldTile(s, ch, tx, ty, fr, plain); return; }   // the dogwood keeps its seasonal blossom
  const G = tk == 'magnolia' ? MAG : OAK;
  // a wood: canopies run together
  if (nb(-1, 0) == 'T') R(x, y + 1, 8, 12, G[1]); if (nb(1, 0) == 'T') R(x + 8, y + 1, 8, 12, G[1]);
  if (nb(0, -1) == 'T') R(x + 1, y, 14, 8, G[2]); if (nb(0, 1) == 'T') R(x + 1, y + 8, 14, 8, G[1]);
  R(x + 3, y + 13, 11, 2, SG[1]); R(x + 5, y + 15, 7, 1, SG[2]); R(x + 2, y + 14, 1, 1, SG[2]);   // its shadow on the grass
  R(x + 7, y + 10, 3, 5, SWD[2]); R(x + 7, y + 10, 1, 5, SWD[3]); R(x + 9, y + 10, 1, 5, SWD[1]); R(x + 6, y + 14, 5, 1, SWD[1]);
  canopy(x, y, G, h);
  if (tk == 'magnolia' && h % 2) { R(x + 4, y + 5, 2, 2, '#fff8f0'); R(x + 10, y + 7, 2, 2, '#fff8f0'); R(x + 5, y + 6, 1, 1, '#e8c8d8'); }
};

/* ---------- depth: taller buildings and cast shadows ---------- */
/* shadow: each colour in a known ramp steps two shades down its own ramp (so the palettes stay small); anything
   else slides toward deep violet */
const SHADE = new Map();
for (const rp of [SG, SST, SRF, SWD, SSA]) rp.forEach((c, i) => SHADE.set(c.toLowerCase(), rp[Math.max(0, i - 2)]));
function shadeRect(x, y, w, h) {
  const im = g.getImageData(x, y, w, h), d = im.data;
  for (let i = 0; i < d.length; i += 4) {
    if (!d[i + 3]) continue;
    const c = toHex(d[i], d[i + 1], d[i + 2]), sc = SHADE.get(c);
    const [r, g2, b] = sc ? hx(sc) : [d[i] * .62 + 40 * .38, d[i + 1] * .62 + 24 * .38, d[i + 2] * .62 + 56 * .38];
    d[i] = r; d[i + 1] = g2; d[i + 2] = b;
  }
  g.putImageData(im, x, y);
}
/* cobbles, take two: worn, rounded setts in soft greys, lit on top, with quiet joints (a road should sit back) */
cobble = function (x, y, h) {
  R(x, y, 16, 16, SST[2]);
  for (let r = 0; r < 4; r++) {
    let a = (r & 1) ? -3 : 0, k = 0;
    while (a < 16) {
      const q = HH(h, r * 7 + k, 31), w = 6 + q % 3, b = y + r * 4, t = q >> 4;
      const x0 = Math.max(0, a), x1 = Math.min(16, a + w - 1);
      if (x1 - x0 >= 1) {
        R(x + x0, b, x1 - x0, 3, t % 4 == 0 ? SST[4] : SST[3]);
        R(x + x0 + 1, b, Math.max(0, x1 - x0 - 2), 1, t % 4 == 0 ? SST[5] : SST[4]);
        if (a >= 0) R(x + x0, b + 2, 1, 1, SST[2]);
        if (t % 6 == 1) R(x + x0 + 2, b + 1, 2, 1, SST[2]);
      }
      a += w; k++;
    }
    R(x, y + r * 4 + 3, 16, 1, SST[1]);
  }
};
const ROOFED = s => s.roof != 'tin' && s.hs != 'glass';
function upperStorey(s, tx, ty) {   // a B cell just above the ground floor, in a block at least three tall
  const at = (X, Y) => X < 0 || Y < 0 || X >= s.w || Y >= s.h ? '' : s.g[Y][X];
  return at(tx, ty) == 'B' && at(tx, ty + 1) == 'B' && at(tx, ty + 2) != 'B' && at(tx, ty + 2) != '' && at(tx, ty - 1) == 'B' && ty > 0;
}
function storey(s, x, y, h, tx, ty) {   // the upper floor: brick, two sash windows, a cream string course below
  const pl = s.pal || [BRICK], c = pl[(tx + (ty + 1 > 5 ? 3 : 0)) % pl.length];
  facade(s, x, y, h * 7 + 3, tx, ty, true, true);    // the shaded brick (plain: no door or window)
  if (c[3] == 'clap') return;
  for (const a of [2, 9]) {
    R(x + a, y + 3, 5, 9, '#efe4c8'); R(x + a, y + 3, 5, 1, '#fff8e8');
    const lit = HH(h, a, 5) % 4 == 0;
    R(x + a + 1, y + 4, 3, 7, lit ? '#f8d868' : '#1c2a4a'); R(x + a + 1, y + 4, 3, 1, lit ? '#fff6c0' : '#5a82b8');
    R(x + a + 1, y + 7, 3, 1, '#efe4c8'); if (!lit) R(x + a + 3, y + 9, 1, 1, '#a8c8e8');
    R(x + a, y + 12, 5, 1, '#a08c6c');
  }
  if (s.ow) (s.wins || (s.wins = {}))[x + ',' + y] = [x, y, HH(h, 2, 5) % 4 == 0];
  R(x, y + 14, 16, 1, '#e8dcc0'); R(x, y + 15, 16, 1, '#a08c6c');
}
const tileT = tile;
tile = function (s, ch, tx, ty, fr, plain) {
  const x = tx * TS, y = ty * TS, h = HH(tx, ty, s.seed);
  const nb = (dx, dy) => { const X = tx + dx, Y = ty + dy; return X < 0 || Y < 0 || X >= s.w || Y >= s.h ? ch : s.g[Y][X]; };
  if (ch == 'B' && ROOFED(s) && !s.interior) {
    if (upperStorey(s, tx, ty)) { storey(s, x, y, h, tx, ty); return; }
    if (nb(0, 1) == 'B') {
      tileT(s, ch, tx, ty, fr, plain);
      if (upperStorey(s, tx, ty + 1)) {   // the eave over the upper floor
        R(x, y + 12, 16, 2, SRF[4]); R(x, y + 12, 16, 1, SRF[5]); R(x, y + 14, 16, 2, SRF[0]);
      }
      return;
    }
    facade(s, x, y, h, tx, ty, nb(0, -1) == 'B' && !upperStorey(s, tx, ty - 1), plain);
    if (upperStorey(s, tx, ty - 1)) { R(x, y, 16, 1, '#a08c6c'); }
    return;
  }
  tileT(s, ch, tx, ty, fr, plain);
  // shadows thrown by buildings (light from the top left): under them, and on their right
  if ('.,psnF'.includes(ch) && !s.interior) {
    const solidB = c => c == 'B' || c == 'X' || c == 'C' || c == 'S' || c == 'f';
    const up = solidB(nb(0, -1)), left = solidB(nb(-1, 0));
    if (up) { shadeRect(x, y, 16, 6); for (let i = 0; i < 16; i += 2) shadeRect(x + i + 1, y + 6, 1, 1); }
    if (left) { shadeRect(x, y + (up ? 6 : 0), 5, up ? 10 : 16); for (let j = up ? 7 : 1; j < 16; j += 2) shadeRect(x + 5, y + j, 1, 1); }
    else if (solidB(nb(-1, -1)) && !up) { shadeRect(x, y, 5, 6 - 0); }
  }
};

shadeTile = function () {};   // the web game's translucent shadows: drawn above in ramp colours instead

/* ---------- water, boardwalks and boulders ---------- */
const SRIVER = ['#1c3430', '#284640', '#345a50', '#447060', '#5e8a78', '#8eb4a0', '#d8eedc'];   // the James, green-grey
const SPOND = ['#14323e', '#1a4656', '#22596a', '#2e6e80', '#46909e', '#86c4cc', '#dcf4f4'];    // ponds and lakes, teal
const SPLANK = ['#3a2214', '#5c3a20', '#7e5430', '#a07040', '#c49058'];
for (const rp of [SRIVER, SPOND, SPLANK]) rp.forEach((c, i) => SHADE.set(c.toLowerCase(), rp[Math.max(0, i - 2)]));
const tileW = tile;
tile = function (s, ch, tx, ty, fr, plain) {
  const x = tx * TS, y = ty * TS; let h = HH(tx, ty, s.seed);
  const nb = (dx, dy) => { const X = tx + dx, Y = ty + dy; return X < 0 || Y < 0 || X >= s.w || Y >= s.h ? ch : s.g[Y][X]; };
  if (ch == 'w') {
    const W = waterOf(s) === WATER.river ? SRIVER : SPOND;
    { const hv = HH(h % 2, 5, 5); h = hv; }   // two patterns, not four: shorelines already make many water tiles
    const land = q => q != 'w' && q != '=' && q != '-';
    R(x, y, 16, 16, W[3]);
    for (let i = 0; i < 4; i++) {          // slow dark swells
      const q = HH(h, i, 11), a = x + q % 12, b = y + 1 + (q >> 4) % 14;
      R(a, b, 4 + (q >> 8) % 4, 1, W[2]); R(a + 1, b + 1, 2 + (q >> 9) % 3, 1, W[2]);
    }
    for (let i = 0; i < 3; i++) {          // wave crests: a lit arc with a darker trough under it
      const q = HH(h, i, 12), a = x + (q % 16 + fr * 2 * (i & 1 ? 1 : -1) + 32) % 12, b = y + 2 + i * 5 + (q >> 4) % 3;
      R(a, b + 1, 5, 1, W[2]); R(a + 1, b, 3, 1, W[4]); R(a + 1, b, 1, 1, W[5]);
    }
    if (h % 3 == 0) { const a = x + 2 + (h >> 4) % 11, b = y + 2 + (h >> 7) % 11; R(a, b, 1, 1, W[6]); }
    if (W === SPOND && h % 13 == 0 && !land(nb(0, -1)) && !land(nb(0, 1)) && !land(nb(-1, 0)) && !land(nb(1, 0))) {   // a lily pad
      const a = x + 3 + (h >> 5) % 6, b = y + 4 + (h >> 8) % 6;
      R(a, b + 1, 7, 4, W[1]); R(a, b, 7, 4, SG[3]); R(a + 1, b - 1, 5, 6, SG[3]); R(a + 1, b - 1, 3, 1, SG[5]); R(a, b + 3, 7, 1, SG[2]);
      R(a + 3, b, 1, 2, W[3]); if (h % 2) { R(a + 5, b - 1, 2, 2, '#f890a8'); R(a + 5, b - 1, 1, 1, '#ffd8e0'); }
    }
    // the banks: shadow where the land is above or to the left (light from the top left), foam where it meets the shore
    if (land(nb(0, -1))) { R(x, y, 16, 3, W[1]); R(x, y + 3, 16, 1, W[2]); for (let i = 0; i < 16; i++) if ((i + tx * 3) % 5 < 2) R(x + i, y + 3, 1, 1, W[5]); }
    if (land(nb(-1, 0))) { R(x, y, 2, 16, W[1]); R(x + 2, y, 1, 16, W[2]); }
    if (land(nb(0, 1))) { for (let i = 0; i < 16; i++) R(x + i, y + 15, 1, 1, (i + tx) % 4 < 2 ? W[6] : W[5]); R(x, y + 14, 16, 1, W[4]); }
    if (land(nb(1, 0))) for (let i = 0; i < 16; i++) R(x + 15, y + i, 1, 1, (i + ty) % 4 < 2 ? W[6] : W[5]);
    if (nb(0, -1) == '=' || nb(0, -1) == '-') { R(x, y, 16, 4, W[1]); R(x, y + 4, 16, 1, W[2]); }   // under a boardwalk
    return;
  }
  if (ch == '=' || ch == '-') {   // boardwalks and bridges: planks lit along their top edge, nails, posts at the water
    const P = SPLANK, along = ch == '=';
    R(x, y, 16, 16, P[3]);
    for (let i = 0; i < 16; i += 4) {
      if (along) { R(x, y + i, 16, 1, P[4]); R(x, y + i + 3, 16, 1, P[1]); R(x, y + i + 2, 16, 1, P[2]); R(x + 2 + HH(h, i, 1) % 11, y + i + 1, 1, 1, P[1]); }
      else { R(x + i, y, 1, 16, P[4]); R(x + i + 3, y, 1, 16, P[1]); R(x + i + 2, y, 1, 16, P[2]); R(x + i + 1, y + 2 + HH(h, i, 1) % 11, 1, 1, P[1]); }
    }
    if (along) {
      if (nb(-1, 0) == 'w') { R(x, y, 3, 16, P[0]); R(x + 1, y, 1, 16, P[2]); R(x, y + 6, 3, 2, P[1]); }
      if (nb(1, 0) == 'w') { R(x + 13, y, 3, 16, P[0]); R(x + 14, y, 1, 16, P[2]); R(x + 13, y + 6, 3, 2, P[1]); }
    } else {
      if (nb(0, -1) == 'w') { R(x, y, 16, 3, P[0]); R(x, y + 1, 16, 1, P[2]); }
      if (nb(0, 1) == 'w') { R(x, y + 13, 16, 3, P[0]); R(x, y + 14, 16, 1, P[2]); }
    }
    return;
  }
  if (ch == 'r') {   // a boulder: a rounded stone in five greys, lit on top, its shadow on the ground
    const gb = '.,psnF'.includes(s.base) ? s.base : '.';
    ground(s, gb, x, y, h);
    R(x + 2, y + 13, 13, 2, SG[1]); R(x + 4, y + 15, 9, 1, SG[2]);
    const rows = [6, 10, 12, 14, 14, 14, 14, 13, 12, 10];
    rows.forEach((w, i) => { const a = x + 8 - (w >> 1); R(a, y + 4 + i, w, 1, SST[3]); R(a, y + 4 + i, 1, 1, SST[0]); R(a + w - 1, y + 4 + i, 1, 1, SST[0]); });
    R(x + 5, y + 3, 6, 1, SST[0]); R(x + 4, y + 14, 8, 1, SST[0]);
    R(x + 3, y + 10, 10, 3, SST[2]); R(x + 4, y + 13, 8, 1, SST[1]);
    R(x + 4, y + 5, 5, 2, SST[5]); R(x + 3, y + 7, 3, 2, SST[4]); R(x + 5, y + 4, 2, 1, SST[6]); R(x + 9, y + 8, 1, 3, SST[1]);
    return;
  }
  tileW(s, ch, tx, ty, fr, plain);
};

/* ---------- landmarks: the web game's hand-drawn buildings, given later-SNES shading automatically ----------
   Each landmark is drawn alone on a clear layer, then every shape gets a lit top-left edge and a shadow along its
   lower right (colour shifted warm and violet like the ramps), large flat areas a soft light-to-shade fall from top
   to bottom in three steps, and the layer goes onto the map. Outlines and window glass (the darkest colours) stay. */
function shadeLayer(cv, x0, y0, w, h) {
  const ctx = cv.getContext('2d');
  x0 = Math.max(0, x0 - 8); y0 = Math.max(0, y0 - 8);
  w = Math.min(cv.width - x0, w + 16); h = Math.min(cv.height - y0, h + 16);
  const im = ctx.getImageData(x0, y0, w, h), d = im.data, src = new Uint8ClampedArray(d);
  const at = (x, y) => (x < 0 || y < 0 || x >= w || y >= h) ? -1 : (y * w + x) * 4;
  const same = (i, j) => j >= 0 && src[j + 3] && Math.abs(src[i] - src[j]) + Math.abs(src[i + 1] - src[j + 1]) + Math.abs(src[i + 2] - src[j + 2]) < 40;
  const mix = (i, t, to) => { d[i] = src[i] + (to[0] - src[i]) * t; d[i + 1] = src[i + 1] + (to[1] - src[i + 1]) * t; d[i + 2] = src[i + 2] + (to[2] - src[i + 2]) * t; };
  const LIGHT = [255, 244, 214], DARK = [40, 24, 56];
  let bot = 0;
  for (let y = 0; y < h; y++) for (let x = 0; x < w; x++) if (src[at(x, y) + 3]) bot = Math.max(bot, y);
  for (let y = 0; y < h; y++) for (let x = 0; x < w; x++) {
    const i = at(x, y);
    if (!src[i + 3]) continue;
    const luma = src[i] * .3 + src[i + 1] * .5 + src[i + 2] * .2;
    if (luma < 48) continue;                                    // outlines, doors and dark glass stay as drawn
    const upE = !same(i, at(x, y - 1)), leftE = !same(i, at(x - 1, y));
    const downE = !same(i, at(x, y + 1)), rightE = !same(i, at(x + 1, y));
    if (upE || leftE) mix(i, .28, LIGHT);
    else if (downE || rightE) mix(i, .3, DARK);
    else if (bot - y < 3) mix(i, .18, DARK);                     // the foot of the building, where it meets the ground
  }
  ctx.putImageData(im, x0, y0);
}
/* the landmark's silhouette, thrown 5 px down and right onto what's under it, in ramp shadow colours */
function castShadow(main, cv, x0, y0, w, h) {
  const SH = 5;
  x0 = Math.max(0, x0 - 8); y0 = Math.max(0, y0 - 8);
  w = Math.min(cv.width - x0, w + 16 + SH); h = Math.min(cv.height - y0, h + 16 + SH);
  const lay = cv.getContext('2d').getImageData(x0, y0, w, h).data;
  const im = main.getImageData(x0, y0, w, h), d = im.data;
  for (let y = SH; y < h; y++) for (let x = SH; x < w; x++) {
    const i = (y * w + x) * 4, j = ((y - SH) * w + x - SH) * 4;
    if (lay[i + 3] || !lay[j + 3]) continue;
    const c = toHex(d[i], d[i + 1], d[i + 2]), sc = SHADE.get(c);
    const [r, g2, b] = sc ? hx(sc) : [d[i] * .62 + 40 * .38, d[i + 1] * .62 + 24 * .38, d[i + 2] * .62 + 56 * .38];
    d[i] = r; d[i + 1] = g2; d[i + 2] = b;
  }
  main.putImageData(im, x0, y0);
}
drawLandmarks = function (s, fr) {
  const main = g;
  for (const L of s.lm || []) {
    const f = LMART[L.art];
    if (!f) continue;
    const cv = mkCanvas(s.w * TS, s.h * TS);
    g = cv.getContext('2d');
    try { f(L.x * TS, L.y * TS, L.w * TS, L.h * TS, fr, s, L); } catch (e) {} finally { g = main; }
    shadeLayer(cv, L.x * TS, L.y * TS, L.w * TS, L.h * TS);
    castShadow(main, cv, L.x * TS, L.y * TS, L.w * TS, L.h * TS);
    main.drawImage(cv, 0, 0);
    for (let yy = L.y; yy < L.y + L.h; yy++) for (let xx = L.x; xx < L.x + L.w; xx++) {
      const c = s.g[yy][xx];
      if (c == 'D') doorway(xx * TS, yy * TS); else if (c == '%') rubbleAt(xx * TS, yy * TS);
    }
  }
};

/* ---------- floors inside: staggered planks or bevelled flagstones, shadowed under the walls ---------- */
floor = function (s, x, y, h) {
  const f = s.fl || ['#4a4458', '#38324a', '#5c566c'], r = ramp(f[0]);
  if (s.wood) {   // long boards: a soft lit edge, a dark seam, now and then a board's end, grain along it
    R(x, y, 16, 16, r[2]);
    for (let i = 0; i < 16; i += 4) {
      const q = HH(h, i, 3);
      R(x, y + i + 3, 16, 1, r[1]);
      if (q % 3 == 0) { const e = (q >> 4) % 14 + 1; R(x + e, y + i, 1, 3, r[1]); }
      R(x + (q >> 2) % 10, y + i + 1, 4 + (q >> 7) % 4, 1, (i >> 2) & 1 ? r[3] : r[1]);
    }
  } else {        // flagstones, two to a tile and staggered, softly bevelled
    R(x, y, 16, 16, r[1]);
    for (const [a, b, w] of [[0, 0, 16], [-8, 8, 16], [8, 8, 16]]) {
      const x0 = Math.max(0, a), x1 = Math.min(16, a + w) - 1;
      R(x + x0, y + b, x1 - x0, 7, r[2]);
      R(x + x0, y + b, x1 - x0, 1, r[3]);
      if (a >= 0) R(x + x0, y + b, 1, 7, r[3]);
    }
    if (h % 5 == 0) { R(x + 3, y + 3, 2, 1, r[1]); R(x + 5, y + 4, 1, 1, r[1]); }
  }
};
const tileF = tile;
tile = function (s, ch, tx, ty, fr, plain) {
  tileF(s, ch, tx, ty, fr, plain);
  if (ch != 'F' || !s.interior && s.ow) return;
  const x = tx * TS, y = ty * TS;
  const at = (dx, dy) => { const X = tx + dx, Y = ty + dy; return X < 0 || Y < 0 || X >= s.w || Y >= s.h ? '' : s.g[Y][X]; };
  const solid = c => c == 'W';   // the walls throw a shadow on the floor (not the furniture)
  if (solid(at(0, -1))) { shadeRect(x, y, 16, 5); for (let i = 0; i < 16; i += 2) shadeRect(x + i + 1, y + 5, 1, 1); }
  if (solid(at(-1, 0))) shadeRect(x, y + (solid(at(0, -1)) ? 5 : 0), 3, solid(at(0, -1)) ? 11 : 16);
};

/* ---------- props: lamps, benches, signs, fences, statues, furniture ----------
   Drawn twice on a scratch layer, whole and as just their ground, so the prop's own pixels can be told apart;
   those get the landmarks' shading, cast a short shadow, and go back on the map. The web game's translucent
   shadows are left out (they come out as odd blends after palette reduction): the cast shadow replaces them. */
const PROPS = 'z1+xfkKM%jJcoYGQVhObg^Rliaquyt';
const tileP = tile;
tile = function (s, ch, tx, ty, fr, plain) {
  if (!PROPS.includes(ch) || (ch == '%' && !s.ow)) return tileP(s, ch, tx, ty, fr, plain);
  const x = tx * TS, y = ty * TS, main = g, oldR = R;
  const base = s.interior ? 'F' : ('.,psnF'.includes(s.base) ? s.base : '.');
  const layer = (c) => { const cv = mkCanvas(32, 32), cx = cv.getContext('2d'); cx.translate(8 - x, 8 - y); g = cx;
    try { tileP(s, c, tx, ty, fr, plain); } finally { g = main; } return cv; };
  R = (a, b, w, h, c) => { if (typeof c == 'string' && c.startsWith('rgba')) return; oldR(a, b, w, h, c); };
  let A, B;
  try { A = layer(ch); B = layer(base); } finally { R = oldR; }
  const a = A.getContext('2d').getImageData(0, 0, 32, 32), b = B.getContext('2d').getImageData(0, 0, 32, 32).data, ad = a.data;
  for (let i = 0; i < ad.length; i += 4)
    if (Math.abs(ad[i] - b[i]) + Math.abs(ad[i + 1] - b[i + 1]) + Math.abs(ad[i + 2] - b[i + 2]) < 6 && b[i + 3]) ad[i + 3] = 0;
  A.getContext('2d').putImageData(a, 0, 0);
  shadeLayer(A, 8, 8, 16, 16);
  // the ground, then the prop's shadow 3 px down and right, then the prop
  tileP(s, base, tx, ty, fr, plain);
  const X0 = Math.max(0, x - 8), Y0 = Math.max(0, y - 8), W = Math.min(main.canvas.width, x + 24) - X0, H = Math.min(main.canvas.height, y + 24) - Y0;
  const lay = A.getContext('2d').getImageData(X0 - (x - 8), Y0 - (y - 8), W, H).data, im = main.getImageData(X0, Y0, W, H), d = im.data;
  for (let yy = 3; yy < H; yy++) for (let xx = 3; xx < W; xx++) {
    const i = (yy * W + xx) * 4, j = ((yy - 3) * W + xx - 3) * 4;
    if (lay[i + 3] || !lay[j + 3]) continue;
    const c = toHex(d[i], d[i + 1], d[i + 2]), sc = SHADE.get(c);
    const [r, g2, bb] = sc ? hx(sc) : [d[i] * .62 + 15, d[i + 1] * .62 + 9, d[i + 2] * .62 + 21];
    d[i] = r; d[i + 1] = g2; d[i + 2] = bb;
  }
  main.putImageData(im, X0, Y0);
  main.drawImage(A, x - 8, y - 8);
};
