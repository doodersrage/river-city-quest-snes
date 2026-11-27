"use strict";
/* River City Quest, SNES edition, stage 13b: maps at 32 px a cell, with ground redrawn natively at that size.

   Every map is painted twice at the web game's 16 px: whole (A), and as bare ground (B: no landmarks, props, decor,
   trees or cast shadows). Comparing the two sorts each pixel into ground, shadow (A is B's shade) or object. Then at
   32 px: the ground cells whose kind has native art are drawn fresh (below), the rest are B through Scale2x;
   shadows are put back as the same shade of whatever is under them now; and the objects, cut out of A, go on top
   through Scale2x (edges against the ground come out smooth because the cut-out is transparent around them).
   Loaded after snes_style.js. */

var HIRES_GROUND = false;   // painting B

/* ---------- Scale2x on RGBA pixels (EPX: each pixel becomes four, corners follow matching neighbours) ---------- */
function scale2xData(src, w, h) {
  const out = new Uint8ClampedArray(w * h * 16), s32 = new Uint32Array(src.buffer, src.byteOffset, w * h), o32 = new Uint32Array(out.buffer);
  const W2 = w * 2;
  for (let y = 0; y < h; y++) for (let x = 0; x < w; x++) {
    const P = s32[y * w + x];
    const A = y > 0 ? s32[(y - 1) * w + x] : P, D = y < h - 1 ? s32[(y + 1) * w + x] : P;
    const C = x > 0 ? s32[y * w + x - 1] : P, B = x < w - 1 ? s32[y * w + x + 1] : P;
    let e0 = P, e1 = P, e2 = P, e3 = P;
    if (C == A && C != D && A != B) e0 = A;
    if (A == B && A != C && B != D) e1 = B;
    if (D == C && D != B && C != A) e2 = C;
    if (B == D && B != A && D != C) e3 = D;
    const i = y * 2 * W2 + x * 2;
    o32[i] = e0; o32[i + 1] = e1; o32[i + W2] = e2; o32[i + W2 + 1] = e3;
  }
  return out;
}

/* ---------- native 32 px ground ---------- */
/* each draws the cell at (X, Y) on the 32 px canvas in g; h is the cell's hash, nb its neighbours, fr the frame */
const NATIVE = {};
const OWNS = 'Tr~';   // native cells that draw their own object (the scale2x'd one in that cell is dropped)

function grass32(X, Y, h) {
  R(X, Y, 32, 32, SG[3]);
  for (let i = 0; i < 12; i++) {                 // darker clumps, soft blots of two or three pixels
    const q = HH(h, i, 41), a = X + q % 30, b = Y + (q >> 5) % 30;
    R(a, b, 3, 1, SG[2]); R(a + 1, b - 1, 1, 1, SG[2]); if (q & 0x8000) R(a - 1, b + 1, 2, 1, SG[2]);
  }
  for (let i = 0; i < 8; i++) {                  // tufts: a dark root, three blades in two greens, lit tips
    const q = HH(h, i, 43), a = X + 2 + q % 26, b = Y + 5 + (q >> 5) % 25;
    R(a, b, 5, 1, SG[1]); R(a + 1, b - 1, 3, 1, SG[2]);
    R(a, b - 3, 1, 2, SG[4]); R(a + 2, b - 5, 1, 4, SG[4]); R(a + 4, b - 3, 1, 2, SG[4]);
    R(a + 1, b - 2, 1, 1, SG[3]); R(a + 3, b - 2, 1, 1, SG[3]);
    R(a + 2, b - 6, 1, 1, SG[5]); if (q & 0x10000) R(a, b - 4, 1, 1, SG[5]);
  }
  for (let i = 0; i < 4; i++) {                  // a few sunlit flecks
    const q = HH(h, i, 47); R(X + 1 + q % 30, Y + 1 + (q >> 5) % 30, 1, 1, i & 1 ? SG[5] : SG[6]);
  }
}
function flowers32(X, Y, h) {
  grass32(X, Y, h + 1);
  const cs = [['#f8e060', '#c89830'], ['#f890a8', '#c05070'], ['#ffffff', '#b8b0d0'], ['#b890f8', '#7058b8']];
  for (let i = 0; i < 6; i++) {
    const q = HH(h, i, 49), a = X + 4 + (i % 3) * 10 + q % 4, b = Y + 5 + (i >= 3 ? 14 : 0) + (q >> 4) % 6, c = cs[(q >> 8) % 4];
    R(a, b + 3, 1, 4, SG[1]); R(a - 2, b + 5, 2, 1, SG[4]); R(a + 1, b + 6, 2, 1, SG[4]);           // stem and leaves
    R(a - 1, b - 1, 3, 1, c[0]); R(a - 2, b, 5, 2, c[0]); R(a - 1, b + 2, 3, 1, c[1]);               // petals
    R(a + 1, b + 1, 1, 1, c[1]); R(a - 2, b + 1, 1, 1, c[1]); R(a, b, 1, 1, '#f8c040'); R(a - 1, b - 1, 1, 1, '#ffffff');
  }
}
/* the edge of a lawn onto paving: blades leaning out over it, lit tips, a shadow line under them */
function fringe32(X, Y, h, nb) {
  const G = (dx, dy) => GRASSY.includes(nb(dx, dy));
  for (const [dx, dy] of [[0, -1], [0, 1], [-1, 0], [1, 0]]) {
    if (!G(dx, dy)) continue;
    for (let i = 0; i < 32; i++) {
      const q = HH(h, i + dx * 7, dy + 11), l = 2 + q % 4 + (i % 7 == 3 ? 2 : 0);
      const a = dx ? (dx < 0 ? 0 : 32 - l) : i, b = dy ? (dy < 0 ? 0 : 32 - l) : i, w = dx ? l : 1, hh = dy ? l : 1;
      R(X + a, Y + b, w, hh, q & 16 ? SG[3] : SG[4]);
      const ex = dx ? (dx < 0 ? a + l : a - 1) : a, ey = dy ? (dy < 0 ? b + l : b - 1) : b;
      R(X + ex, Y + ey, 1, 1, dy < 0 || dx < 0 ? SST[2] : SG[2]);
      if (dy > 0 || dx > 0) R(X + a, Y + b, 1, 1, SG[5]);
      else if (q & 32) R(X + (dx ? a + l - 1 : a), Y + (dy ? b + l - 1 : b), 1, 1, SG[5]);
    }
  }
  if (G(0, -1) && G(-1, 0)) R(X, Y, 6, 6, SG[3]);
  if (G(0, -1) && G(1, 0)) R(X + 26, Y, 6, 6, SG[3]);
  if (G(0, 1) && G(-1, 0)) R(X, Y + 26, 6, 6, SG[3]);
  if (G(0, 1) && G(1, 0)) R(X + 26, Y + 26, 6, 6, SG[3]);
}
/* cobbles: rows of worn, rounded setts in soft greys, each lit on top and left, shaded under, in quiet joints */
function cobble32(X, Y, h) {
  R(X, Y, 32, 32, SST[1]);
  for (let r = 0; r < 4; r++) {
    let a = (r & 1) ? -6 : 0, k = 0;
    while (a < 32) {
      const q = HH(h, r * 9 + k, 53), w = 11 + q % 5, b = Y + r * 8, t = (q >> 4) % 6;
      const x0 = Math.max(0, a), x1 = Math.min(32, a + w - 1);
      if (x1 - x0 >= 2) {
        const base = t == 0 ? SST[4] : t == 1 ? SST[2] : SST[3], lit = t == 0 ? SST[5] : SST[4];
        R(X + x0, b + 1, x1 - x0, 5, base); R(X + x0 + 1, b, x1 - x0 - 2, 7, base);            // the sett, corners rounded
        R(X + x0 + 2, b + 1, x1 - x0 - 4, 1, lit); R(X + x0 + 1, b + 2, 1, 2, lit);             // lit top and left
        R(X + x0 + 2, b + 6, x1 - x0 - 3, 1, SST[2]); R(X + x1 - 1, b + 2, 1, 4, SST[2]);       // shaded foot and right
        if (t == 3) { R(X + x0 + 3 + (q >> 8) % Math.max(1, x1 - x0 - 6), b + 3, 2, 1, SST[2]); }   // a chip
        if (t == 4 && x1 - x0 > 6) R(X + x0 + 3, b + 2, 1, 1, SST[6]);                          // a polished glint
      }
      a += w; k++;
    }
  }
}
/* pavement: one big slab a cell, bevelled, lightly worn, now and then a crack */
function sidewalk32(X, Y, h) {
  R(X, Y, 32, 32, SST[4]);
  R(X, Y, 32, 1, SST[6]); R(X, Y, 1, 32, SST[6]); R(X + 1, Y + 1, 30, 1, SST[5]); R(X + 1, Y + 1, 1, 30, SST[5]);
  R(X, Y + 31, 32, 1, SST[2]); R(X + 31, Y, 1, 32, SST[2]); R(X + 1, Y + 30, 30, 1, SST[3]); R(X + 30, Y + 1, 1, 29, SST[3]);
  for (let i = 0; i < 14; i++) { const q = HH(h, i, 57); R(X + 3 + q % 26, Y + 3 + (q >> 5) % 26, 1, 1, i & 1 ? SST[3] : SST[5]); }
  for (let i = 0; i < 3; i++) { const q = HH(h, i, 59), a = X + 4 + q % 20, b = Y + 4 + (q >> 5) % 22; R(a, b, 4, 2, SST[5]); R(a + 1, b + 2, 3, 1, SST[3]); }
  if (h % 5 == 0) {
    const q = HH(h, 1, 61); let a = X + 8 + q % 8, b = Y + 6;
    for (let i = 0; i < 9; i++) { R(a, b, 2, 1, SST[2]); R(a, b + 1, 1, 1, SST[5]); a += (HH(q, i, 3) % 3) - 1; b += 2; }
  }
}
function sand32(X, Y, h) {
  R(X, Y, 32, 32, SSA[2]);
  for (let i = 0; i < 30; i++) { const q = HH(h, i, 63); R(X + q % 31, Y + (q >> 5) % 31, 1 + (q >> 10) % 2, 1, i % 3 == 0 ? SSA[1] : i % 3 == 1 ? SSA[3] : SSA[2]); }
  for (let i = 0; i < 3; i++) {                 // wind ripples: a lit crest over a shaded trough
    const q = HH(h, i, 65), a = X + q % 16, b = Y + 5 + i * 9 + (q >> 5) % 4, w = 10 + (q >> 9) % 8;
    R(a, b, w, 1, SSA[1]); R(a + 1, b - 1, w - 2, 1, SSA[3]); R(a + 3, b - 2, w - 6, 1, SSA[4]);
  }
  if (h % 6 == 0) {                             // a shell
    const a = X + 18, b = Y + 18; R(a, b, 5, 3, SSA[4]); R(a + 1, b - 1, 3, 1, SSA[4]); R(a, b + 3, 5, 1, SSA[0]); R(a + 2, b, 1, 3, SSA[1]);
  }
}
/* floors inside: long boards, or big bevelled flagstones, in the room's own colour */
function floor32(s, X, Y, h) {
  const f = s.fl || ['#4a4458', '#38324a', '#5c566c'], r = ramp(f[0]);
  if (s.wood) {
    R(X, Y, 32, 32, r[2]);
    for (let i = 0; i < 32; i += 8) {
      const q = HH(h, i, 67);
      R(X, Y + i, 32, 1, r[3]); R(X, Y + i + 7, 32, 1, r[0]); R(X, Y + i + 6, 32, 1, r[1]);
      if (q % 3 == 0) { const e = 2 + (q >> 4) % 28; R(X + e, Y + i, 1, 7, r[0]); R(X + e + 1, Y + i, 1, 6, r[3]); }
      R(X + (q >> 2) % 18, Y + i + 3, 6 + (q >> 7) % 8, 1, (i >> 3) & 1 ? r[3] : r[1]);
      if (q % 5 == 1) { R(X + 4 + (q >> 9) % 22, Y + i + 2, 3, 2, r[1]); R(X + 5 + (q >> 9) % 22, Y + i + 2, 1, 1, r[0]); }   // a knot
    }
  } else {
    R(X, Y, 32, 32, r[1]);
    for (const [a, b] of [[0, 0], [16, 0], [-8, 16], [8, 16], [24, 16]]) {
      const x0 = Math.max(0, a), x1 = Math.min(32, a + 16) - 1;
      if (x1 - x0 < 2) continue;
      R(X + x0, Y + b, x1 - x0, 15, r[2]);
      R(X + x0, Y + b, x1 - x0, 1, r[3]); if (a >= 0) R(X + x0, Y + b, 1, 15, r[3]);
      R(X + x0 + 1, Y + b + 14, x1 - x0 - 1, 1, r[1]);
      const q = HH(h, a + 9, b + 3);
      if (q % 4 == 0) { R(X + x0 + 3 + q % 6, Y + b + 4 + (q >> 4) % 6, 2, 1, r[1]); R(X + x0 + 5 + q % 6, Y + b + 5 + (q >> 4) % 6, 1, 1, r[1]); }
    }
  }
}

NATIVE['.'] = (s, X, Y, h) => grass32(X, Y, h);
NATIVE[','] = (s, X, Y, h) => flowers32(X, Y, h);
NATIVE['p'] = (s, X, Y, h, nb) => { cobble32(X, Y, h); fringe32(X, Y, h, nb); };
NATIVE['s'] = (s, X, Y, h, nb) => { sidewalk32(X, Y, h); fringe32(X, Y, h, nb); };
NATIVE['n'] = (s, X, Y, h, nb) => { sand32(X, Y, h); fringe32(X, Y, h, nb); };
NATIVE['F'] = (s, X, Y, h) => floor32(s, X, Y, h);


/* ---------- trees: a broad canopy of leaf clusters, lit from the top left, on a short trunk ---------- */
function canopy32(X, Y, G, h) {   // G: five greens, dark to light; the crown fills the cell's top 26 rows
  const rows = [10, 16, 20, 22, 24, 26, 26, 28, 28, 28, 28, 28, 28, 28, 26, 26, 24, 22, 20, 16, 10];
  rows.forEach((w, i) => { const a = X + 16 - (w >> 1); R(a, Y + 1 + i, w, 1, G[2]); R(a, Y + 1 + i, 1, 1, G[0]); R(a + w - 1, Y + 1 + i, 1, 1, G[0]); });
  R(X + 11, Y, 10, 1, G[0]); R(X + 11, Y + 22, 10, 1, G[0]);
  for (let i = 12; i < 21; i++) { const w = rows[i], a = X + 16 - (w >> 1); R(a + 1, Y + 1 + i, w - 2, 1, i > 17 ? G[0] : G[1]); }   // the shaded underside
  // leaf clusters: each a dark-edged lump with a lit cap, brighter toward the top left
  for (let i = 0; i < 16; i++) {
    const q = HH(h, i, 71), a = X + 6 + q % 18, b = Y + 3 + (q >> 5) % 15, lit = a + b < X + Y + 26;
    R(a, b + 2, 5, 1, G[lit ? 1 : 0]); R(a, b, 5, 2, G[lit ? 3 : 2]); R(a + 1, b - 1, 3, 1, G[lit ? 3 : 2]);
    R(a + 1, b - 1, 2, 1, G[lit ? 4 : 3]);
  }
  R(X + 8, Y + 4, 6, 2, G[3]); R(X + 6, Y + 6, 4, 3, G[3]); R(X + 9, Y + 3, 3, 1, G[4]); R(X + 7, Y + 6, 1, 2, G[4]);   // the sunlit crown
}
const OAK32 = ['#10301e', '#1a4a2a', '#2a6a36', '#48903e', '#78b858'];
const MAG32 = ['#0a2418', '#143c28', '#1f5a3a', '#3a8a5a', '#68b080'];
function tree32(s, X, Y, h, nb) {
  grass32(X, Y, h);
  const tk = s.interior ? 'oak' : treeKind(h);
  const G = tk == 'magnolia' ? MAG32 : OAK32;
  if (nb(-1, 0) == 'T') R(X, Y + 3, 16, 22, G[1]); if (nb(1, 0) == 'T') R(X + 16, Y + 3, 16, 22, G[1]);   // a wood: crowns run together
  if (nb(0, -1) == 'T') R(X + 3, Y, 26, 14, G[2]); if (nb(0, 1) == 'T') R(X + 3, Y + 16, 26, 16, G[1]);
  R(X + 7, Y + 27, 21, 3, SG[1]); R(X + 10, Y + 30, 14, 1, SG[2]); R(X + 5, Y + 28, 2, 1, SG[2]);   // its shadow
  R(X + 13, Y + 20, 6, 10, SWD[2]); R(X + 13, Y + 20, 2, 10, SWD[3]); R(X + 17, Y + 20, 2, 10, SWD[1]);   // the trunk, a root either side
  R(X + 11, Y + 28, 3, 2, SWD[1]); R(X + 18, Y + 28, 3, 2, SWD[1]); R(X + 14, Y + 23, 1, 3, SWD[1]);
  canopy32(X, Y, G, h);
  if (tk == 'dogwood') for (let i = 0; i < 9; i++) { const q = HH(h, i, 73); R(X + 7 + q % 17, Y + 3 + (q >> 5) % 15, 2, 2, '#fff8f0'); R(X + 8 + q % 17, Y + 4 + (q >> 5) % 15, 1, 1, '#e8c8d8'); }
  if (tk == 'magnolia') for (let i = 0; i < 5; i++) { const q = HH(h, i, 75); R(X + 7 + q % 17, Y + 4 + (q >> 5) % 13, 3, 3, '#fff8f0'); R(X + 8 + q % 17, Y + 6 + (q >> 5) % 13, 2, 1, '#e8c8d8'); }
}
NATIVE['T'] = (s, X, Y, h, nb) => tree32(s, X, Y, h, nb);

/* ---------- slate roofs: overlapping rows of tiles, a lit ridge, eaves in shadow ---------- */
function roof32(X, Y, h, nb) {
  R(X, Y, 32, 32, SRF[2]);
  for (let r = 0; r < 32; r += 6) {
    const o = ((r / 6) & 1) ? 4 : 0;
    R(X, Y + r + 5, 32, 1, SRF[0]); R(X, Y + r + 4, 32, 1, SRF[1]);
    for (let i = o - 8; i < 32; i += 8) {
      if (i >= 0) R(X + i, Y + r, 1, 5, SRF[1]);
      const a = Math.max(0, i + 1), w = Math.min(32, i + 8) - a;
      if (w > 0) { R(X + a, Y + r, w, 1, SRF[3]); if (HH(h, r, i + 9) % 5 == 0) R(X + a, Y + r + 1, w, 3, SRF[3]); }
    }
  }
  if (nb(0, -1) != 'B') { R(X, Y, 32, 5, SRF[4]); R(X, Y, 32, 1, SRF[5]); R(X, Y + 5, 32, 1, SRF[0]); for (let i = 2; i < 32; i += 6) R(X + i, Y + 2, 3, 1, SRF[5]); }
  if (nb(-1, 0) != 'B') { R(X, Y, 3, 32, SRF[4]); R(X + 3, Y, 1, 32, SRF[1]); }
  if (nb(1, 0) != 'B') { R(X + 28, Y, 4, 32, SRF[1]); R(X + 27, Y, 1, 32, SRF[0]); }
  if (nb(0, 1) != 'B') R(X, Y + 28, 32, 4, SRF[0]);
  const ft = h % 6;
  if (ft == 0) {   // a brick chimney with pots, and its shadow down the slates
    R(X + 18, Y + 18, 9, 7, SRF[0]); R(X + 10, Y + 6, 11, 15, '#7a3a2c'); R(X + 10, Y + 6, 4, 15, '#a85a40'); R(X + 10, Y + 6, 11, 1, '#c47a5a');
    for (let j = 9; j < 21; j += 3) R(X + 10, Y + j, 11, 1, '#5a2a20');
    R(X + 9, Y + 3, 13, 3, SST[3]); R(X + 9, Y + 3, 13, 1, SST[5]); R(X + 9, Y + 5, 13, 1, SST[1]);
    R(X + 11, Y + 1, 3, 2, '#c47a5a'); R(X + 17, Y + 1, 3, 2, '#a85a40'); R(X + 11, Y + 1, 3, 1, '#2a1810'); R(X + 17, Y + 1, 3, 1, '#2a1810');
  } else if (ft == 1) {   // a skylight
    R(X + 8, Y + 9, 16, 14, SRF[0]); R(X + 9, Y + 9, 14, 12, '#2e4a7a'); R(X + 9, Y + 9, 14, 1, '#a8c8e8'); R(X + 9, Y + 10, 2, 11, '#5a82b8');
    R(X + 15, Y + 9, 2, 12, SRF[3]); R(X + 12, Y + 12, 2, 2, '#a8c8e8'); R(X + 8, Y + 22, 16, 1, SRF[4]);
  }
}
/* ---------- brick fronts: courses of shaded bricks, a cream cornice (or the roof's shadow) along the top.
   Windows, doors, porches and awnings are objects, painted at 16 px and laid on through Scale2x. ---------- */
function brick32(X, Y, h, r, roofAbove) {
  R(X, Y, 32, 32, r[2]);
  for (let row = 0; row < 8; row++) {
    const b = Y + row * 4, o = row & 1 ? 4 : 0;
    R(X, b + 3, 32, 1, r[0]);
    for (let i = o - 8; i < 32; i += 8) {
      const a = Math.max(0, i), w = Math.min(32, i + 7) - a;
      if (w <= 0) continue;
      const q = HH(h, row, i + 8) % 6;
      R(X + a, b, w, 3, q == 0 ? r[3] : q == 1 ? r[1] : r[2]);
      R(X + a, b, w, 1, q == 0 ? r[4] : r[3]); R(X + a, b + 2, w, 1, q == 1 ? r[0] : r[1]);
      if (i + 7 < 32) R(X + i + 7, b, 1, 3, r[0]);
    }
  }
  R(X + 31, Y, 1, 32, r[1]);
  if (roofAbove) { R(X, Y, 32, 4, r[0]); R(X, Y + 4, 32, 1, r[1]); }
  else {
    R(X, Y, 32, 6, '#e8dcc0'); R(X, Y, 32, 1, '#fff8e8'); R(X, Y + 1, 32, 1, '#f4ecd8');
    for (let i = 1; i < 32; i += 4) R(X + i, Y + 4, 2, 2, '#a08c6c');
    R(X, Y + 6, 32, 1, '#a08c6c'); R(X, Y + 7, 32, 1, r[0]);
  }
}
NATIVE['B'] = (s, X, Y, h, nb, fr, tx, ty) => {
  if (s.hs == 'glass' || s.interior) return false;
  const tin = s.roof == 'tin', up = !tin && upperStorey(s, tx, ty);
  if (nb(0, 1) == 'B' && !up) {
    if (tin) return tin32(X, Y, h, nb);
    roof32(X, Y, h, nb);
    if (upperStorey(s, tx, ty + 1)) { R(X, Y + 24, 32, 4, SRF[4]); R(X, Y + 24, 32, 1, SRF[5]); R(X, Y + 28, 32, 4, SRF[0]); }
    return;
  }
  const pl = s.pal || [BRICK], c = pl[(tx + ((up ? ty + 1 : ty) > 5 ? 3 : 0)) % pl.length];
  const above = up || (nb(0, -1) == 'B' && !(!tin && upperStorey(s, tx, ty - 1)));
  if (c[3] == 'clap') { clap32(X, Y, h, c); cornice32(X, Y, ramp(c[0]), above); return; }
  brick32(X, Y, h, ramp(c[0]), above);
};
/* in B, fronts are bare brick: their windows and doors count as objects */
const facadeH = facade, storeyH = storey, styledH = styledFacade;
styledFacade = function (s, x, y, h, tx, ty, c) {
  if (HIRES_GROUND) { if (c[3] == 'clap') { R(x, y, 16, 16, c[0]); for (let j = 1; j < 16; j += 3) { R(x, y + j, 16, 1, c[1]); R(x, y + j + 1, 16, 1, c[2]); } R(x + 15, y, 1, 16, c[1]); } return true; }
  return styledH(s, x, y, h, tx, ty, c);
};
facade = function (s, x, y, h, tx, ty, roofAbove, plain) { return facadeH(s, x, y, h, tx, ty, roofAbove, plain || HIRES_GROUND); };
storey = function (s, x, y, h, tx, ty) { if (HIRES_GROUND) return facade(s, x, y, h * 7 + 3, tx, ty, true, true); return storeyH(s, x, y, h, tx, ty); };

/* ---------- walls inside: big dressed blocks under a dark cornice; the wall tops; torches ---------- */
function wall32(s, X, Y, h) {
  const c = s.wl || ['#5b5368', '#463f52', '#756c85'], r = ramp(c[0]);
  R(X, Y, 32, 32, r[2]);
  for (let row = 0; row < 3; row++) {
    const b = Y + 8 + row * 7, o = row & 1 ? 8 : 0;
    R(X, b + 6, 32, 1, r[0]);
    for (let i = o - 16; i < 32; i += 16) {
      const a = Math.max(0, i), w = Math.min(32, i + 15) - a;
      if (w <= 0) continue;
      R(X + a, b, w, 1, r[3]); R(X + a, b + 5, w, 1, r[1]);
      if (i >= 0) R(X + i, b, 1, 5, r[3]);
      if (i + 15 < 32) R(X + i + 15, b, 1, 6, r[0]);
      if (HH(h, row, i + 20) % 4 == 0) R(X + a + 3, b + 2, 3, 1, r[1]);
    }
  }
  R(X, Y, 32, 8, '#2a2434'); R(X, Y, 32, 1, '#4a445a'); R(X, Y + 1, 32, 1, '#3a3446'); R(X, Y + 7, 32, 1, '#14101c');
  R(X, Y + 28, 32, 4, r[1]); R(X, Y + 31, 32, 1, r[0]);
}
NATIVE['W'] = (s, X, Y, h, nb, fr, tx, ty) => {
  if (nb(0, 1) != 'W') {
    wall32(s, X, Y, h);
    if (ty == 0 && tx % 4 == 1) {   // a torch in its bracket
      R(X + 14, Y + 18, 4, 9, '#3a2a1a'); R(X + 15, Y + 18, 1, 9, '#6a5030'); R(X + 11, Y + 16, 10, 3, '#6a5030'); R(X + 11, Y + 16, 10, 1, '#a08050');
      const f = fr & 1, f2 = fr >> 1;
      R(X + 12 - f2 % 2, Y + 7 - f, 8, 9, '#c04818'); R(X + 13, Y + 6 - f + f2, 6, 9, '#f08020'); R(X + 14 + f, Y + 8, 4, 7, '#f8e060'); R(X + 15, Y + 11, 2, 4, '#ffffff');
    }
  } else {
    R(X, Y, 32, 32, '#2a2434');
    if (nb(0, -1) != 'W') { R(X, Y, 32, 2, '#4a445a'); R(X, Y + 2, 32, 1, '#3a3446'); }
    if (nb(-1, 0) != 'W') R(X, Y, 2, 32, '#4a445a');
    if (nb(1, 0) != 'W') R(X + 30, Y, 2, 32, '#14101c');
    for (let i = 0; i < 3; i++) { const q = HH(h, i, 77); if (q % 3 == 0) R(X + 4 + q % 22, Y + 5 + (q >> 5) % 22, 3, 1, '#3a3446'); }
  }
};

/* ---------- water: long swells, crests that drift with the frame, sun glints; banks shaded, foam at the shore ---------- */
NATIVE['w'] = (s, X, Y, h, nb, fr, tx, ty) => {
  const W = waterOf(s) === WATER.river ? SRIVER : SPOND;
  h = HH(h % 2, 5, 5);
  const land = q => q != 'w' && q != '=' && q != '-';
  R(X, Y, 32, 32, W[3]);
  for (let i = 0; i < 6; i++) { const q = HH(h, i, 81), a = X + q % 24, b = Y + 2 + (q >> 5) % 28; R(a, b, 7 + (q >> 9) % 6, 1, W[2]); R(a + 2, b + 1, 4 + (q >> 10) % 4, 1, W[2]); }
  for (let i = 0; i < 4; i++) {
    const q = HH(h, i, 83), a = X + q % 22, b = Y + 3 + i * 7 + (q >> 5) % 3;
    R(a, b + 1, 9, 1, W[2]); R(a + 1, b, 7, 1, W[4]); R(a + 2, b - 1, 3, 1, W[5]); R(a + 2, b - 1, 1, 1, W[6]);
  }
  // a sun glint that moves from frame to frame, kept to the cell's middle so few tiles animate (VRAM pins them)
  { const q = HH(h, fr, 89), a = X + 9 + q % 12, b = Y + 10 + (q >> 5) % 10; R(a, b, 3, 1, W[6]); R(a + 1, b - 1, 1, 1, W[6]); R(a - 1, b + 1, 5, 1, W[4]); }
  if (land(nb(0, -1))) { R(X, Y, 32, 6, W[1]); R(X, Y + 6, 32, 1, W[2]); for (let i = 0; i < 32; i++) if ((i + tx * 5) % 9 < 3) R(X + i, Y + 6, 1, 1, W[5]); }
  if (land(nb(-1, 0))) { R(X, Y, 4, 32, W[1]); R(X + 4, Y, 1, 32, W[2]); }
  if (land(nb(0, 1))) {   // lapping at the shore: a lit band, then foam of uneven depth
    R(X, Y + 27, 32, 1, W[4]);
    for (let i = 0; i < 32; i++) { const q = HH(i + tx * 32, ty, 87) % 8, d = q < 3 ? 1 : q < 6 ? 2 : 3; R(X + i, Y + 32 - d - 1, 1, 1, W[5]); R(X + i, Y + 32 - d, 1, d, q == 7 ? W[5] : W[6]); }
  }
  if (land(nb(1, 0))) for (let i = 0; i < 32; i++) R(X + 30, Y + i, 2, 1, (i + ty * 3) % 7 < 3 ? W[6] : W[5]);
  if (nb(0, -1) == '=' || nb(0, -1) == '-') { R(X, Y, 32, 8, W[1]); R(X, Y + 8, 32, 1, W[2]); }
  const h0 = HH(tx, ty, s.seed);
  if (h0 % 13 == 0 && !land(nb(0, -1)) && !land(nb(0, 1)) && !land(nb(-1, 0)) && !land(nb(1, 0))) {   // a lily pad, a notch cut in it
    const a = X + 7 + (h0 >> 5) % 10, b = Y + 9 + (h0 >> 8) % 10;
    R(a, b + 2, 14, 7, W[1]); R(a, b, 14, 7, '#3d8a3a'); R(a + 2, b - 2, 10, 11, '#3d8a3a'); R(a + 1, b - 1, 12, 9, '#3d8a3a');
    R(a + 3, b - 1, 6, 2, '#68b854'); R(a + 2, b + 1, 2, 3, '#68b854'); R(a + 6, b + 1, 2, 4, W[3]); R(a + 7, b + 3, 1, 3, W[3]);
    R(a + 1, b + 7, 12, 1, '#2a6a2a');
    if (h0 & 2) { R(a + 9, b - 3, 4, 3, '#f08098'); R(a + 10, b - 4, 2, 1, '#f8b8c8'); R(a + 10, b - 2, 2, 1, '#f8e060'); }
  }
};
/* ---------- boardwalks and bridges: planks with a lit edge, a dark seam, nails; posts where they meet the water ---------- */
function planks32(X, Y, h, nb, along) {
  const P = SPLANK;
  R(X, Y, 32, 32, P[3]);
  for (let i = 0; i < 32; i += 8) {
    const q = HH(h, i, 85);
    if (along) {
      R(X, Y + i, 32, 1, P[4]); R(X, Y + i + 7, 32, 1, P[0]); R(X, Y + i + 6, 32, 1, P[2]);
      R(X + 3 + q % 24, Y + i + 3, 5, 1, P[2]); R(X + 2, Y + i + 3, 1, 1, P[1]); R(X + 29, Y + i + 3, 1, 1, P[1]);
      if (q % 3 == 0) { const e = 6 + (q >> 5) % 20; R(X + e, Y + i, 1, 7, P[1]); }
    } else {
      R(X + i, Y, 1, 32, P[4]); R(X + i + 7, Y, 1, 32, P[0]); R(X + i + 6, Y, 1, 32, P[2]);
      R(X + i + 3, Y + 3 + q % 24, 1, 5, P[2]); R(X + i + 3, Y + 2, 1, 1, P[1]); R(X + i + 3, Y + 29, 1, 1, P[1]);
      if (q % 3 == 0) { const e = 6 + (q >> 5) % 20; R(X + i, Y + e, 7, 1, P[1]); }
    }
  }
  if (along) {
    for (const [d, a] of [[-1, 0], [1, 26]]) if (nb(d, 0) == 'w') { R(X + a, Y, 6, 32, P[0]); R(X + a + 2, Y, 2, 32, P[2]); R(X + a, Y + 12, 6, 4, P[1]); R(X + a + 1, Y + 12, 4, 1, P[4]); }
  } else {
    for (const [d, a] of [[-1, 0], [1, 26]]) if (nb(0, d) == 'w') { R(X, Y + a, 32, 6, P[0]); R(X, Y + a + 2, 32, 2, P[2]); }
  }
}
NATIVE['='] = (s, X, Y, h, nb) => planks32(X, Y, h, nb, true);
NATIVE['-'] = (s, X, Y, h, nb) => planks32(X, Y, h, nb, false);


/* ---------- second pass: clapboard, tin roofs, boulders, rapids ---------- */
function clap32(X, Y, h, c) {   // lapped boards: each lit along its top, shaded under the lap
  const r = ramp(c[0]);
  R(X, Y, 32, 32, r[2]);
  for (let j = 0; j < 32; j += 5) { R(X, Y + j, 32, 1, r[3]); R(X, Y + j + 3, 32, 1, r[1]); R(X, Y + j + 4, 32, 1, r[0]); }
  for (let i = 0; i < 3; i++) { const q = HH(h, i, 91); R(X + 3 + q % 24, Y + 1 + ((q >> 5) % 6) * 5, 4, 1, r[4]); }   // sun on a board or two
  R(X, Y, 2, 32, r[3]); R(X + 30, Y, 2, 32, r[1]);   // corner boards
}
function cornice32(X, Y, r, roofAbove) {
  if (roofAbove) { R(X, Y, 32, 4, r[0]); R(X, Y + 4, 32, 1, r[1]); return; }
  R(X, Y, 32, 6, '#e8dcc0'); R(X, Y, 32, 1, '#fff8e8'); R(X, Y + 1, 32, 1, '#f4ecd8');
  for (let i = 1; i < 32; i += 4) R(X + i, Y + 4, 2, 2, '#a08c6c');
  R(X, Y + 6, 32, 1, '#a08c6c'); R(X, Y + 7, 32, 1, r[0]);
}
function tin32(X, Y, h, nb) {   // standing-seam tin: lit seams, a rust patch now and then
  R(X, Y, 32, 32, '#8a929c');
  for (let i = 2; i < 32; i += 8) { R(X + i, Y, 2, 32, '#c8d0d8'); R(X + i + 2, Y, 1, 32, '#6a727c'); R(X + i + 4, Y, 3, 32, '#7e8690'); }
  for (let j = 7; j < 32; j += 8) R(X, Y + j, 32, 1, '#7a828c');
  if (h % 5 == 0) { const a = X + 4 + h % 18, b = Y + 8 + (h >> 4) % 12; R(a, b, 6, 3, '#a86a4a'); R(a + 1, b + 3, 4, 2, '#8a4a34'); R(a + 1, b, 2, 1, '#c88a5a'); }
  if (nb(0, -1) != 'B') { R(X, Y, 32, 4, '#c8d0d8'); R(X, Y, 32, 1, '#e8eef4'); R(X, Y + 4, 32, 1, '#4a525c'); }
  if (nb(-1, 0) != 'B') R(X, Y, 2, 32, '#c8d0d8');
  if (nb(1, 0) != 'B') R(X + 30, Y, 2, 32, '#4a525c');
  if (nb(0, 1) != 'B') R(X, Y + 29, 32, 3, '#4a525c');
}
function boulder32(X, Y, ox, oy, wid) {   // a rounded stone lit from the top left, a crack, its shadow
  const rows = wid > 20 ? [10, 16, 20, 22, 24, 24, 24, 24, 24, 23, 22, 20, 17, 12] : [6, 10, 12, 14, 14, 14, 13, 12, 10, 6];
  const n = rows.length, cx = X + ox, top = Y + oy;
  R(cx - (rows[n - 1] >> 1) - 2, top + n - 1, rows[n - 1] + 6, 3, SG[1]);
  rows.forEach((w, i) => { const a = cx - (w >> 1); R(a, top + i, w, 1, SST[3]); R(a, top + i, 1, 1, SST[0]); R(a + w - 1, top + i, 1, 1, SST[0]); });
  R(cx - (rows[0] >> 1), top - 1, rows[0], 1, SST[0]); R(cx - (rows[n - 1] >> 1), top + n, rows[n - 1], 1, SST[0]);
  const k = n >> 1;
  for (let i = k; i < n; i++) { const w = rows[i] - 2, a = cx - (w >> 1); R(a, top + i, w, 1, i > n - 3 ? SST[1] : SST[2]); }
  R(cx - (rows[2] >> 1) + 2, top + 1, rows[2] >> 1, 2, SST[5]); R(cx - (rows[3] >> 1) + 1, top + 3, 3, 3, SST[4]); R(cx - (rows[1] >> 1) + 3, top, 3, 1, SST[6]);
  R(cx + 2, top + k - 2, 1, 3, SST[1]); R(cx + 3, top + k + 1, 1, 2, SST[1]); R(cx + 1, top + k - 3, 1, 1, SST[1]);
}
NATIVE['r'] = (s, X, Y, h, nb, fr, tx, ty) => {
  const g0 = propGround(s, tx, ty), f = NATIVE[g0];
  if (f) f(s, X, Y, h, nb, fr, tx, ty); else return false;
  boulder32(X, Y, 16, 7, 24);
};
NATIVE['~'] = (s, X, Y, h, nb, fr) => {
  const W = waterOf(s) === WATER.river ? SRIVER : SPOND;
  R(X, Y, 32, 32, W[4]);
  for (let i = 0; i < 7; i++) {   // standing waves: a dark trough, a green-white face, a foaming crest
    const q = HH(h, i, 93), a = X + q % 22, b = Y + 2 + i * 4 + (q >> 5) % 2, w = 8 + (q >> 9) % 8;
    R(a, b + 2, w, 1, W[2]); R(a + 1, b + 1, w - 2, 1, W[5]); R(a + 2, b, w - 4, 1, W[6]); if (q & 64) R(a + w - 3, b - 1, 2, 1, '#ffffff');
  }
  { const q = HH(h, fr, 95); R(X + 10 + q % 10, Y + 10 + (q >> 5) % 10, 4, 1, '#ffffff'); R(X + 11 + q % 10, Y + 11 + (q >> 5) % 10, 2, 1, W[6]); }   // spray, moving
  for (const [dx, dy] of [[0, -1], [0, 1], [-1, 0], [1, 0]]) { const q = nb(dx, dy); if (q != '~' && q != 'w' && q != '=') R(X + (dx > 0 ? 28 : 0), Y + (dy > 0 ? 28 : 0), dx ? 4 : 32, dy ? 4 : 32, '#ffffff'); }
  if (h % 4 == 0) { boulder32(X, Y, 16, 10, 14); R(X + 6, Y + 21, 20, 2, '#ffffff'); R(X + 8, Y + 23, 16, 1, W[6]); }
};

/* ---------- B: bare ground ---------- */
const drawLandmarks1 = drawLandmarks, drawDecor1 = drawDecor, shadeRect1 = shadeRect, tileH = tile;
drawLandmarks = function (s, fr) { if (!HIRES_GROUND) drawLandmarks1(s, fr); };
drawDecor = function (s) { if (!HIRES_GROUND) drawDecor1(s); };
shadeRect = function (x, y, w, h) { if (!HIRES_GROUND) shadeRect1(x, y, w, h); };
tile = function (s, ch, tx, ty, fr, plain) {
  if (HIRES_GROUND && (ch == 'T' || PROPS.includes(ch) || ch == 'r')) {
    return tileH(s, ch == 'T' ? '.' : propGround(s, tx, ty), tx, ty, fr, plain);
  }
  return tileH(s, ch, tx, ty, fr, plain);
};

/* ---------- the 32 px map ---------- */
/* a cell's hash, cut to a few variants like the web art's (export.js VARIANTS) so tiles repeat */
function cellHash(s, tx, ty) { const h = HH(tx, ty, s.seed); return typeof SNES_VARIANTS != 'undefined' && SNES_VARIANTS ? HH(h % SNES_VARIANTS, 7, 7) : h; }
const renderMap1 = renderMap;
function shadeOf(r, g2, b) {
  const sc = SHADE.get(toHex(r, g2, b));
  return sc ? hx(sc) : [r * .62 + 40 * .38, g2 * .62 + 24 * .38, b * .62 + 56 * .38];
}
renderMap = function (s, fr) {
  const W = s.w * TS, H = s.h * TS;
  const a = renderMap1(s, fr), ga = g; g = a.getContext('2d');
  try { drawLandmarks(s, fr); } catch (e) {} try { drawDecor(s); } catch (e) {}
  g = ga;
  HIRES_GROUND = true;
  let b;
  try { b = renderMap1(s, fr); } finally { HIRES_GROUND = false; }
  const A = a.getContext('2d').getImageData(0, 0, W, H).data, B = b.getContext('2d').getImageData(0, 0, W, H).data;
  // sort the pixels
  const kind = new Uint8Array(W * H), obj = new Uint8ClampedArray(W * H * 4);
  for (let p = 0, i = 0; p < W * H; p++, i += 4) {
    if (A[i] == B[i] && A[i + 1] == B[i + 1] && A[i + 2] == B[i + 2]) continue;
    const sh = shadeOf(B[i], B[i + 1], B[i + 2]);
    if (Math.abs(A[i] - sh[0]) < 9 && Math.abs(A[i + 1] - sh[1]) < 9 && Math.abs(A[i + 2] - sh[2]) < 9) { kind[p] = 1; continue; }
    kind[p] = 2; obj[i] = A[i]; obj[i + 1] = A[i + 1]; obj[i + 2] = A[i + 2]; obj[i + 3] = 255;
  }
  // the ground at 32 px: B through Scale2x, then the native cells drawn over it
  const out = mkCanvas(W * 2, H * 2), ctx = out.getContext('2d');
  const gi = ctx.createImageData(W * 2, H * 2); gi.data.set(scale2xData(B, W, H)); ctx.putImageData(gi, 0, 0);
  const old = g; g = ctx;
  try {
    for (let ty = 0; ty < s.h; ty++) for (let tx = 0; tx < s.w; tx++) {
      let ch = s.g[ty][tx];
      if (PROPS.includes(ch) && (ch != '%' || s.ow)) ch = propGround(s, tx, ty);
      const f = NATIVE[ch];
      if (!f) continue;
      const nb = (dx, dy) => { const X = tx + dx, Y = ty + dy; return X < 0 || Y < 0 || X >= s.w || Y >= s.h ? ch : s.g[Y][X]; };
      if (f(s, tx * 32, ty * 32, cellHash(s, tx, ty), nb, fr, tx, ty) === false) continue;
      if (OWNS.includes(ch)) for (let y = ty * TS; y < ty * TS + TS; y++) for (let x = tx * TS; x < tx * TS + TS; x++) {
        const p = y * W + x; kind[p] = 0; obj[p * 4 + 3] = 0;
      }
    }
  } finally { g = old; }
  // shadows back on, as the shade of what's there now
  const N = ctx.getImageData(0, 0, W * 2, H * 2), nd = N.data;
  for (let y = 0; y < H * 2; y++) for (let x = 0; x < W * 2; x++) {
    if (kind[(y >> 1) * W + (x >> 1)] != 1) continue;
    const i = (y * W * 2 + x) * 4, sh = shadeOf(nd[i], nd[i + 1], nd[i + 2]);
    nd[i] = sh[0]; nd[i + 1] = sh[1]; nd[i + 2] = sh[2];
  }
  // and the objects on top
  const O = scale2xData(obj, W, H);
  for (let i = 0; i < nd.length; i += 4) if (O[i + 3]) { nd[i] = O[i]; nd[i + 1] = O[i + 1]; nd[i + 2] = O[i + 2]; }
  ctx.putImageData(N, 0, 0);
  return out;
};
