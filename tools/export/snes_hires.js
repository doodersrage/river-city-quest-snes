"use strict";
/* River City Quest, SNES edition, stage 13b: maps at 32 px a cell, with ground redrawn natively at that size.

   Every map is painted twice at the web game's 16 px: whole (A), and as bare ground (B: no landmarks, props, decor,
   trees or cast shadows). Comparing the two sorts each pixel into ground, shadow (A is B's shade) or object. Then at
   32 px: the ground cells whose kind has native art are drawn fresh (below), the rest are B through Scale2x;
   shadows are put back as the same shade of whatever is under them now; and the objects, cut out of A, go on top
   through Scale2x (edges against the ground come out smooth because the cut-out is transparent around them).
   Loaded after snes_style.js. */

var HIRES_GROUND = false;   // painting B
var MASKCV = null;          // (one canvas for the landmark masks, reused: node-canvas memory isn't freed promptly)

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
/* R(.., 'shade'): darken what's already there (a shadow drawn into native art) */
const R0 = R;
R = function (x, y, w, h, c) {
  if (c !== 'shade') return R0(x, y, w, h, c);
  SHADEQ.push([x, y, w, h]);   // (applied in one pass on the finished ground: per-call getImageData leaks native memory)
};
var SHADEQ = [];
const OWNS = 'Tr~';   // native cells that draw their own object (the scale2x'd one in that cell is dropped)

/* Stage 14 grass: 16 px quarters, each one of six designs, so the 32 px cell grid doesn't show; and a slow
   "lushness" field across the map (darker clumps where it's lush, sunlit flecks where it's dry) so a lawn has
   patches the way a real one does. Every design shares the base green, so the patches blend without seams. */
function smoothNoise(x, y, seed) {   // value noise, x and y in 16 px quarters, smooth over about six of them
  const S = 6, gx = Math.floor(x / S), gy = Math.floor(y / S), fx = x / S - gx, fy = y / S - gy;
  const v = (a, b) => (HH(a, b, seed) % 1000) / 1000;
  const s_ = t => t * t * (3 - 2 * t), u = s_(fx), w = s_(fy);
  return (v(gx, gy) * (1 - u) + v(gx + 1, gy) * u) * (1 - w) + (v(gx, gy + 1) * (1 - u) + v(gx + 1, gy + 1) * u) * w;
}
function grass16(X, Y, d, tone) {
  R(X, Y, 16, 16, SG[3]);
  const q = k => HH(d, k, 101 + tone);
  const clumps = tone == 0 ? 5 : tone == 1 ? 3 : 1, flecks = tone == 2 ? 4 : tone == 1 ? 2 : 0;
  for (let i = 0; i < clumps; i++) {          // a dark clump: a soft blot, two shades
    const r = q(i), a = X + 1 + r % 12, b = Y + 2 + (r >> 4) % 12;
    R(a, b, 4, 2, SG[2]); R(a + 1, b - 1, 2, 1, SG[2]); R(a + 1, b + 1, 2, 1, SG[1]);
  }
  for (let i = 0; i < 3; i++) {               // tufts: three blades fanning out, lit tips, a shadow at the root
    const r = q(i + 9), a = X + 2 + r % 11, b = Y + 5 + (r >> 4) % 10;
    R(a - 1, b, 5, 1, SG[2]);
    R(a, b - 2, 1, 2, SG[4]); R(a + 1, b - 4, 1, 4, SG[4]); R(a + 2, b - 3, 1, 3, SG[4]);
    R(a + 1, b - 5, 1, 1, SG[5]); R(a - 1, b - 3, 1, 1, SG[4]); R(a + 3, b - 4, 1, 1, SG[5]);
  }
  for (let i = 0; i < flecks; i++) { const r = q(i + 20); R(X + r % 15, Y + (r >> 4) % 15, 1, 1, i & 1 ? SG[5] : SG[6]); }
}
function grass32(X, Y, h) {
  // (one lushness and one of four quarter layouts a cell: a map's cells then come in a dozen kinds, so its
  // metatiles stay few; the quarters still break up the 32 px grid)
  const n = smoothNoise((X >> 4) + 1, (Y >> 4) + 1, 7), tone = n < .38 ? 0 : n < .68 ? 1 : 2, v = HH(X >> 5, Y >> 5, 13) % 4;
  for (let j = 0; j < 2; j++) for (let i = 0; i < 2; i++) grass16(X + i * 16, Y + j * 16, (v + i + 2 * j) % 4, tone);
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
/* the edge of a lawn onto paving (stage 14): a lumpy lip of grass whose depth wanders smoothly along the edge
   (worked from map position, so neighbouring cells join up), a dark rim and blade tips at its front, and the
   lip's shadow on the paving below it; rounded where two lawn sides meet */
function lip(t, k) { const n = t >> 3, a = HH(n, k, 31) % 5, b = HH((n + 1) & 7, k, 31) % 5, f = (t & 7) / 8; return 2 + Math.round(a * (1 - f) + b * f); }   // (t in 0..63: a loop)
function fringe32(X, Y, h, nb) {
  const G = (dx, dy) => GRASSY.includes(nb(dx, dy));
  for (const [dx, dy] of [[0, -1], [0, 1], [-1, 0], [1, 0]]) {
    if (!G(dx, dy)) continue;
    for (let i = 0; i < 32; i++) {
      // (the wander repeats every 64 px and doesn't depend on the row, so edges reuse a few tiles)
      const along = (dx ? Y + i : X + i) & 63, k = dx ? (dx < 0 ? 1 : 2) : (dy < 0 ? 3 : 4);
      const d = lip(along, k);
      // the lip, from the edge inwards d px, then its rim; tips poke out now and then
      const tip = HH(along, k, 37) % 5 == 0 ? 2 : 0;
      if (dy < 0) { R(X + i, Y, 1, d, SG[3]); R(X + i, Y + d, 1, 1, SG[2]); R(X + i, Y + d + 1, 1, 1, SG[1]); if (tip) { R(X + i, Y + d, 1, 2, SG[4]); R(X + i, Y + d + 2, 1, 1, SG[1]); }
        R(X + i, Y + d + 2 + (tip ? 1 : 0), 1, 2, 'shade'); }
      else if (dy > 0) { R(X + i, Y + 32 - d, 1, d, SG[3]); R(X + i, Y + 31 - d, 1, 1, SG[4]); if (tip) R(X + i, Y + 30 - d, 1, 1, SG[4]); }
      else if (dx < 0) { R(X, Y + i, d, 1, SG[3]); R(X + d, Y + i, 1, 1, SG[1]); if (tip) R(X + d, Y + i, 2, 1, SG[4]); R(X + d + 1, Y + i, 1, 1, 'shade'); }
      else { R(X + 32 - d, Y + i, d, 1, SG[3]); R(X + 31 - d, Y + i, 1, 1, SG[2]); if (tip) R(X + 30 - d, Y + i, 1, 1, SG[4]); }
    }
  }
  const corner = (a, b, x0, y0) => { if (G(a, 0) && G(0, b)) for (let y = 0; y < 7; y++) for (let x = 0; x < 7; x++) if (x * x + y * y < 40) R(X + (a < 0 ? x : 31 - x), Y + (b < 0 ? y : 31 - y), 1, 1, SG[3]); };
  corner(-1, -1); corner(1, -1); corner(-1, 1); corner(1, 1);
  // a tuft on the lip here and there
  for (const [dx, dy] of [[0, -1], [-1, 0], [1, 0]]) if (G(dx, dy) && h % 3 == 0) {
    const i = 6 + h % 20, a = dx ? (dx < 0 ? X + 1 : X + 26) : X + i, b = dx ? Y + i : Y + 1;
    R(a, b + 3, 4, 1, SG[2]); R(a + 1, b, 1, 3, SG[4]); R(a + 2, b - 1, 1, 4, SG[5]); R(a + 3, b + 1, 1, 2, SG[4]);
  }
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
/* pavement (stage 14): 16 px slabs, each bevelled (lit top and left, shaded under), its own shade of stone, worn
   in places: a crack, a dark stain, a patch of grit */
function slab16(X, Y, k) {
  const t = k % 9, base = t == 0 ? '#a89c98' : t == 1 ? '#bcb0aa' : SST[4], lo = SST[3], hi = SST[5];
  R(X, Y, 16, 16, base);
  R(X, Y, 16, 1, hi); R(X, Y, 1, 16, hi);
  R(X + 1, Y + 15, 15, 1, SST[2]); R(X + 15, Y + 1, 1, 15, SST[2]); R(X + 1, Y + 14, 14, 1, lo); R(X + 14, Y + 1, 1, 13, lo);
  for (let i = 0; i < 3; i++) { const r = HH(k, i, 41); R(X + 2 + r % 11, Y + 2 + (r >> 4) % 11, 1, 1, i & 1 ? lo : hi); }
  const f = (k >> 3) % 14;
  if (f == 0) { let a = X + 4 + k % 6, b = Y + 2; for (let i = 0; i < 6; i++) { R(a, b, 1, 2, SST[2]); R(a + 1, b, 1, 1, hi); a += HH(k, i, 3) % 3 - 1; b += 2; } }   // a crack
  else if (f == 1) { R(X + 5, Y + 6, 5, 3, '#a89c98'); R(X + 6, Y + 5, 3, 5, '#a89c98'); }                                                        // a stain
  else if (f == 2) { for (let i = 0; i < 6; i++) { const r = HH(k, i, 47); R(X + 3 + r % 9, Y + 3 + (r >> 4) % 9, 1, 1, SST[2]); } }                  // grit
}
function sidewalk32(X, Y, h) {
  for (let j = 0; j < 2; j++) for (let i = 0; i < 2; i++) slab16(X + i * 16, Y + j * 16, (HH(X >> 5, Y >> 5, 17) % 4) * 2 + ((i + j) & 1) + (i * 3 + j * 5) * 8);
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
    // (stage 14) flagstones: each its own shade, lit along its top and left edges, shaded along the others,
    // dark joints with a little grime in them; now and then a chip, a crack or a worn hollow
    const f7 = typeof r7 == 'function' ? r7(f[0]) : null, t = f7 || [r[0], r[0], r[1], r[2], r[3], r[4], r[4]];
    R(X, Y, 32, 32, t[1]);
    let k = 0;
    for (const [a, b] of [[0, 0], [16, 0], [-8, 16], [8, 16], [24, 16]]) {
      const x0 = Math.max(0, a), x1 = Math.min(32, a + 16) - 1;
      if (x1 - x0 < 2) continue;
      const q = HH(h, a + 9, b + 3), tone = q % 5 == 0 ? t[4] : q % 5 == 1 ? t[2] : t[3];
      R(X + x0, Y + b, x1 - x0, 15, tone);
      R(X + x0, Y + b, x1 - x0, 1, t[5]); if (a >= 0) R(X + x0, Y + b, 1, 15, t[5]);
      R(X + x0 + 1, Y + b + 1, x1 - x0 - 2, 1, tone == t[4] ? t[5] : t[4]);
      R(X + x0 + 1, Y + b + 14, x1 - x0 - 1, 1, t[2]); R(X + x1 - 1, Y + b + 1, 1, 13, t[2]);
      R(X + x0, Y + b + 15, x1 - x0 + 1, 1, t[0]); if (a + 15 < 32) R(X + x1, Y + b, 1, 16, t[0]);
      const v = (q >> 4) % 9;
      if (v == 0 && (q >> 8) % 3 == 0) { let cx = X + x0 + 3 + q % 6, cy = Y + b + 2; for (let i = 0; i < 6; i++) { R(cx, cy, 1, 2, t[0]); R(cx + 1, cy, 1, 1, t[4]); cx += (HH(q, i, 3) % 3) - 1; cy += 2; } }
      else if (v == 1) { R(X + x0 + 4, Y + b + 5, 6, 4, t[2]); R(X + x0 + 5, Y + b + 4, 4, 1, t[2]); R(X + x0 + 5, Y + b + 8, 4, 1, t[4]); }   // a hollow worn by feet
      else if (v == 2) { R(X + x1 - 3, Y + b + 11, 2, 2, t[1]); R(X + x1 - 2, Y + b + 10, 1, 1, t[1]); }                                            // a chipped corner
      else if (v == 3) for (let i = 0; i < 4; i++) { const g3 = HH(q, i, 9); R(X + x0 + 2 + g3 % 11, Y + b + 2 + (g3 >> 4) % 11, 1, 1, t[2]); }
      k++;
    }
  }
}

/* (stage 14) the shadow walls cast on the floor below and beside them: two steps deep under a wall, one beside */
function wallShade(X, Y, nb) {
  const wall = c => c == 'W' || c == 'B' || c == '_';
  if (wall(nb(0, -1))) { R(X, Y, 32, 6, 'shade'); R(X, Y, 32, 3, 'shade'); }
  if (wall(nb(-1, 0))) { R(X, Y, 4, 32, 'shade'); R(X, Y, 2, 32, 'shade'); }
  if (wall(nb(1, 0))) R(X + 30, Y, 2, 32, 'shade');
  if (wall(nb(-1, -1)) && !wall(nb(0, -1)) && !wall(nb(-1, 0))) R(X, Y, 4, 6, 'shade');
}

NATIVE['.'] = (s, X, Y, h) => grass32(X, Y, h);
NATIVE[','] = (s, X, Y, h) => flowers32(X, Y, h);
NATIVE['p'] = (s, X, Y, h, nb) => { cobble32(X, Y, h); fringe32(X, Y, h, nb); };
NATIVE['s'] = (s, X, Y, h, nb) => { sidewalk32(X, Y, h); fringe32(X, Y, h, nb); };
NATIVE['n'] = (s, X, Y, h, nb) => { sand32(X, Y, h); fringe32(X, Y, h, nb); };
NATIVE['_'] = (s, X, Y, h, nb) => {   // (stage 14) a pit: the floor's cut edge at its top, then darkness falling away
  R(X, Y, 32, 32, '#0c0a14');
  for (let i = 0; i < 12; i++) { const q = HH(h, i, 97); R(X + q % 30, Y + 10 + (q >> 5) % 20, 1, 1, '#16121e'); }
  if (nb(0, -1) != '_') { const f = s.fl || ['#4a4458'], t = typeof r7 == 'function' ? r7(f[0]) : ramp(f[0]);
    R(X, Y, 32, 8, t[1]); R(X, Y, 32, 1, t[3]); for (let i = 3; i < 32; i += 7) R(X + i, Y + 1, 1, 7, t[0]); R(X, Y + 8, 32, 3, '#14101c'); }
};
NATIVE['F'] = (s, X, Y, h, nb) => { floor32(s, X, Y, h); wallShade(X, Y, nb); };


/* ---------- trees: a broad canopy of leaf clusters, lit from the top left, on a short trunk ---------- */
function canopy32(X, Y, G, h) {   // G: five greens, dark to light; the crown fills the cell's top 25 rows
  // a crown built of leaf clumps, each a ball lit from the top left; lower clumps sit in front of higher ones,
  // the silhouette is lumpy, the shade between clumps is dark, and tones meet in a dither as 16-bit trees do
  const T = new Int8Array(32 * 26).fill(-1), j = (i, m) => (HH(h, i, 91 + m) % 3) - 1;
  const clumps = [[16, 6, 7], [9, 9, 6], [23, 9, 6], [6, 15, 6], [26, 15, 5], [16, 12, 8], [11, 18, 6], [21, 18, 6], [16, 20, 5]];
  clumps.forEach(([cx, cy, r], i) => {
    cx += j(i, 0); cy += j(i, 1);
    for (let y = cy - r; y <= cy + r; y++) for (let x = cx - r; x <= cx + r; x++) {
      if (x < 1 || x > 30 || y < 0 || y > 24) continue;
      const dx = x - cx, dy = y - cy, d2 = dx * dx + dy * dy;
      if (d2 > r * r + r * .6) continue;
      const lit = (-dx * .7 - dy * .9) / r + (1 - d2 / (r * r)) * .6;   // light from the top left, and the ball's roundness
      let t = lit > .9 ? 4 : lit > .45 ? 3 : lit > -.1 ? 2 : 1;
      if (lit > .45 && lit <= .6 && (x + y) & 1) t = 2;   // dither the edge of the light
      if (lit > .9 && lit <= 1 && (x + y) & 1) t = 3;
      if (lit <= -.1 && lit > -.25 && (x + y) & 1) t = 2;
      T[y * 32 + x] = t;
    }
  });
  for (let y = 0; y < 25; y++) for (let x = 0; x < 32; x++) {
    const t = T[y * 32 + x]; if (t < 0) continue;
    const out = (dx, dy) => { const X2 = x + dx, Y2 = y + dy; return X2 < 0 || X2 > 31 || Y2 < 0 || Y2 > 25 || T[Y2 * 32 + X2] < 0; };
    let c = t;
    if (out(1, 0) || out(0, 1) || out(-1, 0) || out(0, -1)) c = 0;        // the outline
    else if (y > 14 && t > 1 && out(0, 2)) c = 1;                          // the underside in shade
    if (c > 0 && c < 4) { const q = HH(h, x, y * 7 + 5); if (q % 11 == 0) c = c - 1; else if (q % 13 == 0 && c > 1) c = c + 1; }   // leaves
    R(X + x, Y + y, 1, 1, G[c]);
  }
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
  if (tk == 'dogwood') for (let i = 0; i < 8; i++) {   // four-petalled blossoms, pink at the heart
    const q = HH(h, i, 73), a = X + 7 + q % 17, b = Y + 4 + (q >> 5) % 15;
    R(a, b - 1, 1, 3, '#fff8f0'); R(a - 1, b, 3, 1, '#fff8f0'); R(a, b, 1, 1, '#d87898'); R(a + 1, b + 1, 1, 1, '#c8c0c8');
  }
  if (tk == 'magnolia') for (let i = 0; i < 5; i++) {   // big cupped flowers
    const q = HH(h, i, 75), a = X + 7 + q % 16, b = Y + 4 + (q >> 5) % 13;
    R(a, b, 4, 3, '#fff8f0'); R(a + 1, b - 1, 2, 1, '#fff8f0'); R(a, b + 2, 4, 1, '#e8c8d8'); R(a + 3, b, 1, 2, '#f0d8e0'); R(a + 1, b + 1, 2, 1, '#f8e8ec');
  }
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
  // stage 14: its windows, door, bay, awning or porch at 32 px too (the 16 px ones are then dropped)
  if (typeof facade32 == 'undefined' || tin || isDecor(s, tx, ty)) return;
  const hb = HH(tx, ty, s.seed);
  if (up ? storey32(s, X, Y, hb) : facade32(s, X, Y, hb, tx, ty, ramp(c[0]))) {
    if (!up && upperStorey(s, tx, ty - 1)) R(X, Y, 32, 2, '#a08c6c');
    return 'own';
  }
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
  const c = s.wl || ['#5b5368', '#463f52', '#756c85'], r = typeof r7 == 'function' ? r7(c[0]) : ramp(c[0]);
  R(X, Y, 32, 32, r[3]);
  for (let row = 0; row < 3; row++) {   // (stage 14) dressed blocks, each its own shade, lit on top, shaded under
    const b = Y + 8 + row * 7, o = row & 1 ? 8 : 0;
    R(X, b + 6, 32, 1, r[0]);
    for (let i = o - 16; i < 32; i += 16) {
      const a = Math.max(0, i), w = Math.min(32, i + 15) - a;
      if (w <= 0) continue;
      const q = HH(h, row, i + 20) % 6;
      R(X + a, b, w, 6, q == 0 ? r[4] : q == 1 ? r[2] : r[3]);
      R(X + a, b, w, 1, r[5]); R(X + a, b + 5, w, 1, r[1]);
      if (i >= 0) R(X + i, b, 1, 5, r[4]);
      if (i + 15 < 32) R(X + i + 15, b, 1, 6, r[0]);
      if (q == 3) { R(X + a + 4, b + 2, 3, 1, r[1]); R(X + a + 5, b + 3, 2, 1, r[2]); }
    }
  }
  const cp = typeof r7 == 'function' ? r7('#3a3446') : null;   // the coping along the top
  R(X, Y, 32, 8, cp ? cp[2] : '#2a2434'); R(X, Y, 32, 1, cp ? cp[5] : '#4a445a'); R(X, Y + 1, 32, 2, cp ? cp[3] : '#3a3446'); R(X, Y + 7, 32, 1, '#14101c');
  for (let i = 0; i < 32; i += 8) R(X + i, Y + 2, 1, 5, cp ? cp[1] : '#14101c');
  R(X, Y + 29, 32, 3, r[1]); R(X, Y + 29, 32, 1, r[2]); R(X, Y + 31, 32, 1, r[0]);   // the baseboard
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
    for (let j = 0; j < 32; j += 16) for (let i = (j & 16) ? 8 : 0; i < 32; i += 16) { R(X + i, Y + j, 15, 15, '#302a3c'); R(X + i, Y + j, 15, 1, '#3a3446'); R(X + i, Y + j + 14, 15, 1, '#221c2c'); }
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
// stage 14: the 16 px style's shadows (walls and landmarks onto the ground) are left out of the 16 px scene and laid on
// the 32 px ground instead, so they shade its texture; their 1 px dither becomes a finer one
var HR_SCENE = false;
shadeRect = function (x, y, w, h) {
  if (!HR_SCENE) return shadeRect1(x, y, w, h);
  if (!HIRES_GROUND) return;
  if (w == 1 && h == 1) { SHADEQ.push([x * 2, y * 2, 1, 1], [x * 2 + 1, y * 2 + 1, 1, 1]); return; }
  SHADEQ.push([x * 2, y * 2, w * 2, h * 2]);
};
tile = function (s, ch, tx, ty, fr, plain) {
  if (HIRES_GROUND && (ch == 'T' || PROPS.includes(ch) || ch == 'r')) {
    return tileH(s, ch == 'T' ? '.' : propGround(s, tx, ty), tx, ty, fr, plain);
  }
  return tileH(s, ch, tx, ty, fr, plain);
};

/* ---------- stage 14: objects remastered at 32 px ----------
   Scale2x keeps the 16 px art's flat fills and single-pixel steps, which is what reads as 8-bit. At 32 px there's
   room for what 16-bit art does with its extra resolution: each flat area gets a 1 px lit edge along its top and
   left and a 1 px shaded edge along its bottom and right (so shapes look rounded, not cut out), and tall flat
   areas fall off from light at the top to shade at the bottom in three bands. Outlines (the darkest colours) and
   small details are left as they are. Light comes from the top left, as in the rest of the art. */
function lum(r, g2, b) { return r * .3 + g2 * .59 + b * .11; }
function lighter(r, g2, b) { return [r + (255 - r) * .2, g2 + (248 - g2) * .2, b + (230 - b) * .16]; }
function darker(r, g2, b) { return [r * .8 + 40 * .2, g2 * .8 + 26 * .2, b * .8 + 60 * .2]; }
function remaster(O, W, H) {
  const out = new Uint8ClampedArray(O);
  const at = (x, y) => (x < 0 || y < 0 || x >= W || y >= H) ? -1 : (y * W + x) * 4;
  const same = (i, j) => j >= 0 && O[j + 3] && O[i] == O[j] && O[i + 1] == O[j + 1] && O[i + 2] == O[j + 2];
  // how far each pixel's colour runs up and down (for the bands)
  const up = new Uint16Array(W * H), dn = new Uint16Array(W * H);
  for (let x = 0; x < W; x++) {
    for (let y = 0; y < H; y++) { const i = at(x, y); up[y * W + x] = O[i + 3] && same(i, at(x, y - 1)) ? up[(y - 1) * W + x] + 1 : 0; }
    for (let y = H - 1; y >= 0; y--) { const i = at(x, y); dn[y * W + x] = O[i + 3] && same(i, at(x, y + 1)) ? dn[(y + 1) * W + x] + 1 : 0; }
  }
  for (let y = 0; y < H; y++) for (let x = 0; x < W; x++) {
    const i = at(x, y);
    if (!O[i + 3]) continue;
    const L = lum(O[i], O[i + 1], O[i + 2]);
    if (L < 52) continue;                                   // outlines and deep shadows stay
    const p = y * W + x, run = up[p] + dn[p] + 1;
    // edges: is the neighbour another colour? (only edges of areas at least 4 px across, so fine detail stays crisp)
    const big = run >= 4 && (same(i, at(x + 1, y)) || same(i, at(x - 1, y)));
    if (!big) continue;
    let c = null;
    if (!same(i, at(x, y - 1)) || !same(i, at(x - 1, y))) c = lighter(O[i], O[i + 1], O[i + 2]);
    else if (!same(i, at(x, y + 1)) || !same(i, at(x + 1, y))) c = darker(O[i], O[i + 1], O[i + 2]);
    else if (run >= 14) {                                   // a tall flat: light at the top, shade at the bottom
      if (up[p] < run * .22) c = lighter(O[i], O[i + 1], O[i + 2]);
      else if (dn[p] < run * .3) c = darker(O[i], O[i + 1], O[i + 2]);
    }
    if (c) { out[i] = c[0]; out[i + 1] = c[1]; out[i + 2] = c[2]; }
  }
  return out;
}

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
  HR_SCENE = true; SHADEQ = [];
  const a = renderMap1(s, fr), ga = g; g = a.getContext('2d');
  const C = g.getImageData(0, 0, W, H).data;   // the 16 px scene before the landmarks and decor: their shadows are judged against it
  try { drawLandmarks(s, fr); } catch (e) {} try { drawDecor(s); } catch (e) {}
  g = ga;
  HIRES_GROUND = true;
  let b;
  try { b = renderMap1(s, fr); } finally { HIRES_GROUND = false; HR_SCENE = false; }
  const A = a.getContext('2d').getImageData(0, 0, W, H).data, B = b.getContext('2d').getImageData(0, 0, W, H).data;
  // sort the pixels
  const kind = new Uint8Array(W * H), obj = new Uint8ClampedArray(W * H * 4);
  for (let p = 0, i = 0; p < W * H; p++, i += 4) {
    if (A[i] == B[i] && A[i + 1] == B[i + 1] && A[i + 2] == B[i + 2]) continue;
    const sh = shadeOf(B[i], B[i + 1], B[i + 2]);
    if (Math.abs(A[i] - sh[0]) < 9 && Math.abs(A[i + 1] - sh[1]) < 9 && Math.abs(A[i + 2] - sh[2]) < 9) { kind[p] = 1; continue; }
    // a landmark's or a sign's shadow falls on the 16 px ground (a dark wash over it), not on this ground: find it by
    // the same darkening in every channel against the scene without it
    if (A[i] != C[i] || A[i + 1] != C[i + 1] || A[i + 2] != C[i + 2]) {
      const la = A[i] + A[i + 1] + A[i + 2], lc = C[i] + C[i + 1] + C[i + 2], k = la / Math.max(1, lc);
      if (k > .35 && k < .88 && lc > 60) {
        let ok = true;
        for (let c = 0; c < 3; c++) if (Math.abs(A[i + c] - (C[i + c] * k + 8 * (1 - k))) > 14) ok = false;
        if (ok) { kind[p] = 1; continue; }
      }
    }
    kind[p] = 2; obj[i] = A[i]; obj[i + 1] = A[i + 1]; obj[i + 2] = A[i + 2]; obj[i + 3] = 255;
  }
  // stage 14: landmarks with native art (snes_lm32.js): cut their 16 px drawing out of the objects, pixel for pixel
  const lms = NOFX.includes('lm') ? [] : (s.lm || []).filter(L => typeof LM32 != 'undefined' && LM32[L.art] && LMART[L.art]);
  const cv = lms.length ? (MASKCV && MASKCV.width == W && MASKCV.height == H ? MASKCV : (MASKCV = mkCanvas(W, H))) : null;
  for (const L of lms) {
    const gg = g;
    cv.getContext('2d').clearRect(0, 0, W, H);
    g = cv.getContext('2d');
    try { LMART[L.art](L.x * TS, L.y * TS, L.w * TS, L.h * TS, fr, s, L); } catch (e) {} finally { g = gg; }
    const M = cv.getContext('2d').getImageData(0, 0, W, H).data;
    for (let p = 0; p < W * H; p++) if (M[p * 4 + 3] && kind[p] == 2) { kind[p] = 0; obj[p * 4 + 3] = 0; }
  }
  // specks: object bits of four pixels or fewer on open ground (a grass blade by a rock, a fleck of foam) are 16 px
  // texture that Scale2x would only turn into 2x2 blocks; the 32 px ground has its own
  if (!NOFX.includes('speck')) {
    const seen = new Uint8Array(W * H), st = [], comp = [];
    for (let p0 = 0; p0 < W * H; p0++) {
      if (kind[p0] != 2 || seen[p0]) continue;
      st.length = 0; comp.length = 0; st.push(p0); seen[p0] = 1;
      while (st.length) {
        const p = st.pop(); comp.push(p); if (comp.length > 4) continue;
        const x = p % W, y = (p / W) | 0;
        for (const [dx, dy] of [[1, 0], [-1, 0], [0, 1], [0, -1], [1, 1], [-1, -1], [1, -1], [-1, 1]]) {
          const X = x + dx, Y = y + dy; if (X < 0 || Y < 0 || X >= W || Y >= H) continue;
          const q = Y * W + X; if (kind[q] == 2 && !seen[q]) { seen[q] = 1; st.push(q); }
        }
      }
      if (comp.length > 4) continue;
      if (!comp.every(p => '.,_wsS'.includes(s.g[(p / W / TS) | 0][((p % W) / TS) | 0]))) continue;
      for (const p of comp) { kind[p] = 0; obj[p * 4 + 3] = 0; }
    }
  }
  // the 16 px trees' crowns poke a few pixels into the cell above; the 32 px tree is whole in its own cell
  for (let ty = 0; ty < s.h - 1; ty++) for (let tx = 0; tx < s.w; tx++) {
    if (s.g[ty + 1][tx] != 'T' || s.g[ty][tx] == 'T') continue;
    for (let y = ty * TS + TS - 5; y < ty * TS + TS; y++) for (let x = tx * TS; x < tx * TS + TS; x++) { const p = y * W + x; if (kind[p] == 2) { kind[p] = 0; obj[p * 4 + 3] = 0; } }
  }
  const decs = typeof decor32 != 'undefined' && !NOFX.includes('dec') ? decorFor(s) : [];
  for (const d of decs) for (let y = d.y * TS - 2; y < d.y * TS + TS + 2; y++) for (let x = d.x * TS; x < (d.x + d.len) * TS; x++)
    if (y >= 0 && y < H) { const p = y * W + x; if (kind[p] == 2) { kind[p] = 0; obj[p * 4 + 3] = 0; } }
  // the ground at 32 px: B through Scale2x, then the native cells drawn over it
  const out = mkCanvas(W * 2, H * 2), ctx = out.getContext('2d');
  const gi = ctx.createImageData(W * 2, H * 2); gi.data.set(scale2xData(B, W, H)); ctx.putImageData(gi, 0, 0);
  const old = g; g = ctx;
  try {
    for (let ty = 0; ty < s.h; ty++) for (let tx = 0; tx < s.w; tx++) {
      let ch = s.g[ty][tx];
      const ch0 = ch;
      if (PROPS.includes(ch) && (ch != '%' || s.ow)) ch = propGround(s, tx, ty);
      const f = NATIVE[ch];
      if (!f) continue;
      const nb = (dx, dy) => { const X = tx + dx, Y = ty + dy; return X < 0 || Y < 0 || X >= s.w || Y >= s.h ? ch : s.g[Y][X]; };
      let res = f(s, tx * 32, ty * 32, cellHash(s, tx, ty), nb, fr, tx, ty);
      if (res === false) continue;
      // stage 14: a prop with 32 px art stands on the ground just drawn (and its 16 px drawing is dropped)
      if (ch0 != ch && typeof PROP32 != 'undefined' && PROP32[ch0] && !isDecor(s, tx, ty)) {
        const nb0 = (dx, dy) => { const X = tx + dx, Y = ty + dy; return X < 0 || Y < 0 || X >= s.w || Y >= s.h ? ch0 : s.g[Y][X]; };
        PROP32[ch0](s, tx * 32, ty * 32, cellHash(s, tx, ty), nb0, fr);   // (a few variants: tiles repeat)
        res = 'own';
      }
      if (typeof DBG != 'undefined') DBG[String(ch) + ':' + String(res)] = (DBG[String(ch) + ':' + String(res)] || 0) + 1;
      if (OWNS.includes(ch) || res === 'own') for (let y = ty * TS; y < ty * TS + TS; y++) for (let x = tx * TS; x < tx * TS + TS; x++) {
        const p = y * W + x; kind[p] = 0; obj[p * 4 + 3] = 0;
      }
    }
  } finally { g = old; }
  // shadows back on, as the shade of what's there now (and the native art's own shadows)
  const N = ctx.getImageData(0, 0, W * 2, H * 2), nd = N.data;
  const shq = new Uint8Array(W * H * 4);
  for (const [x0, y0, w0, h0] of SHADEQ) for (let y = Math.max(0, y0); y < Math.min(H * 2, y0 + h0); y++) for (let x = Math.max(0, x0); x < Math.min(W * 2, x0 + w0); x++) if (shq[y * W * 2 + x] < 2) shq[y * W * 2 + x]++;
  SHADEQ = [];
  for (let p = 0; p < W * H * 4; p++) for (let n = 0; n < shq[p]; n++) { const i = p * 4, sh = shadeOf(nd[i], nd[i + 1], nd[i + 2]); nd[i] = sh[0]; nd[i + 1] = sh[1]; nd[i + 2] = sh[2]; }
  for (let y = 0; y < H * 2; y++) for (let x = 0; x < W * 2; x++) {
    if (kind[(y >> 1) * W + (x >> 1)] != 1) continue;
    const i = (y * W * 2 + x) * 4, sh = shadeOf(nd[i], nd[i + 1], nd[i + 2]);
    nd[i] = sh[0]; nd[i + 1] = sh[1]; nd[i + 2] = sh[2];
  }
  // the native landmarks, and the doorways in them
  ctx.putImageData(N, 0, 0);
  if (decs.length) { const o = g; g = ctx; try { for (const d of decs) decor32(d); } finally { g = o; } }
  if (lms.length) {
    const old2 = g; g = ctx;
    try {
      for (const L of lms) {
        LM32[L.art](L.x * 32, L.y * 32, L.w * 32, L.h * 32, fr, s, L);
        for (let yy = L.y; yy < L.y + L.h; yy++) for (let xx = L.x; xx < L.x + L.w; xx++) if (s.g[yy][xx] == 'D') door32(xx * 32, yy * 32);
      }
    } finally { g = old2; }
  }
  const N2 = ctx.getImageData(0, 0, W * 2, H * 2), nd2 = N2.data;
  if (typeof DBG != 'undefined') for (let ty = 0; ty < s.h; ty++) for (let tx = 0; tx < s.w; tx++) {
    let n = 0; for (let y = ty * TS; y < ty * TS + TS; y++) for (let x = tx * TS; x < tx * TS + TS; x++) if (obj[(y * W + x) * 4 + 3]) n++;
    if (n) { const k = 'OBJ ' + s.g[ty][tx] + (isDecor(s, tx, ty) ? ' decor' : '') + (typeof DBGXY != 'undefined' ? ' ' + s.id + (s.g[ty][tx] == '.' ? ' ' + tx + ',' + ty : '') : ''); DBG[k] = (DBG[k] || 0) + 1; }
  }
  if (typeof OBJONLY != 'undefined' && OBJONLY) { const q = ctx.getImageData(0, 0, W * 2, H * 2); for (let i = 0; i < q.data.length; i += 4) { q.data[i] = q.data[i] * .3; q.data[i + 1] = q.data[i + 1] * .3; q.data[i + 2] = q.data[i + 2] * .3 + 40; } ctx.putImageData(q, 0, 0); N2.data.set(q.data); }
  // and the objects on top
  const O = NOFX.includes('rm') ? scale2xData(obj, W, H) : remaster(scale2xData(obj, W, H), W * 2, H * 2);
  for (let i = 0; i < nd2.length; i += 4) if (O[i + 3]) { nd2[i] = O[i]; nd2[i + 1] = O[i + 1]; nd2[i + 2] = O[i + 2]; }
  ctx.putImageData(N2, 0, 0);
  return out;
};
