"use strict";
/* River City Quest, SNES edition, stage 14: landmarks drawn natively at 32 px a cell.

   The web game's landmarks are flat 16 px drawings; through Scale2x they read as 8-bit art made bigger. These are
   the same buildings, the same shapes and colours, drawn again at twice the resolution the way 16-bit games drew
   their towns: dressed stone and brick you can see the courses of, round columns shaded across their width,
   cornices with dentils and a shadow line under them, glass that catches the light, steps with lit treads.
   Light comes from the top left. Each function takes the landmark's box in 32 px-map pixels (twice the web's)
   plus the frame and the landmark record, and draws with R into g. snes_hires.js calls them (LM32) in place of
   the Scale2x'd 16 px drawing, which it cuts out pixel for pixel. Loaded after snes_hires.js. */

const OUTL = '#1a1423';
/* a seven-step ramp: three darker (toward deep violet), the colour, three lighter (toward warm cream) */
function ramp7(c) {
  const [r, g2, b] = hx(c), D = [40, 24, 56], L = [255, 246, 220];
  const mix = (t, to) => toHex(r + (to[0] - r) * t, g2 + (to[1] - g2) * t, b + (to[2] - b) * t);
  return [mix(.66, D), mix(.44, D), mix(.22, D), c, mix(.24, L), mix(.48, L), mix(.72, L)];
}
const R7 = new Map();
function r7(c) { if (!R7.has(c)) R7.set(c, ramp7(c)); return R7.get(c); }

/* dressed stone: courses 8 px high, blocks 16-24 px long, each a touch lighter or darker, lit on top, a dark joint */
function ashlar(X, Y, W, H, c, seed) {
  const r = r7(c);
  R(X, Y, W, H, r[3]);
  for (let y = 0, row = 0; y < H; y += 8, row++) {
    let x = (row & 1) ? -8 : 0, k = 0;
    while (x < W) {
      const q = HH(seed + row, k, 5), len = 16 + (q % 3) * 4, a = Math.max(0, x), e = Math.min(W, x + len);
      if (e - a > 1) {
        const t = q % 7;
        R(X + a, Y + y, e - a, Math.min(8, H - y), t == 0 ? r[4] : t == 1 ? r[2] : r[3]);
        R(X + a, Y + y, e - a, 1, r[5]);
        if (y + 7 < H) R(X + a, Y + y + 7, e - a, 1, r[1]);
        if (x + len < W) R(X + x + len - 1, Y + y, 1, Math.min(7, H - y), r[1]);
        if (t == 3 && e - a > 8) R(X + a + 3 + q % (e - a - 6), Y + y + 3, 2, 1, r[2]);
      }
      x += len; k++;
    }
  }
}
/* brick: 8x4 bricks, three tones, lit tops, dark mortar, a few headers picked out */
function bricks(X, Y, W, H, c, seed) {
  const r = r7(c);
  R(X, Y, W, H, r[3]);
  for (let y = 0, row = 0; y < H; y += 4, row++) {
    for (let x = (row & 1) ? -4 : 0, k = 0; x < W; x += 8, k++) {
      const a = Math.max(0, x), e = Math.min(W, x + 7);
      if (e <= a) continue;
      const t = HH(seed + row, k, 9) % 6;
      R(X + a, Y + y, e - a, Math.min(3, H - y), t == 0 ? r[4] : t == 1 ? r[2] : r[3]);
      R(X + a, Y + y, e - a, 1, t == 1 ? r[3] : r[5]);
      if (x + 7 < W) R(X + x + 7, Y + y, 1, Math.min(3, H - y), r[1]);
    }
    if (y + 3 < H) R(X, Y + y + 3, W, 1, r[1]);
  }
}
/* a round column: shaded across its width (lit just left of middle), a capital and a base */
function column(X, Y, W, H, c) {
  const r = r7(c), band = [r[3], r[5], r[6], r[5], r[4], r[3], r[2], r[1]];
  for (let i = 0; i < W; i++) R(X + i, Y + 6, 1, H - 12, band[Math.min(7, Math.floor(i * 8 / W))]);
  for (let i = 3; i < W - 2; i += 4) R(X + i, Y + 8, 1, H - 16, r[2]);                // flutes
  R(X - 2, Y, W + 4, 4, r[5]); R(X - 2, Y, W + 4, 1, r[6]); R(X - 1, Y + 4, W + 2, 2, r[2]);   // capital
  R(X - 2, Y + H - 6, W + 4, 6, r[4]); R(X - 2, Y + H - 6, W + 4, 1, r[6]); R(X - 2, Y + H - 1, W + 4, 1, r[1]);   // base
}
/* an entablature/cornice: a lit fascia, dentils, a deep shadow line under it */
function cornice(X, Y, W, c, h) {
  const r = r7(c);
  h = h || 12;
  R(X, Y, W, h, r[4]); R(X, Y, W, 2, r[6]); R(X, Y + 2, W, 1, r[5]);
  for (let i = 2; i < W - 2; i += 6) { R(X + i, Y + h - 6, 3, 3, r[5]); R(X + i, Y + h - 3, 3, 1, r[2]); }
  R(X, Y + h - 2, W, 1, r[2]); R(X, Y + h - 1, W, 1, r[0]);
}
/* a window: a lit frame and sill, glass that either glows (lit) or reflects the sky (a diagonal sheen) */
function win32(X, Y, W, H, lit, frame) {
  const f = r7(frame || '#e8dcc0');
  R(X - 2, Y - 2, W + 4, H + 4, f[4]); R(X - 2, Y - 2, W + 4, 1, f[6]); R(X - 2, Y + H + 1, W + 4, 1, f[1]);
  R(X - 3, Y + H + 2, W + 6, 2, f[5]); R(X - 3, Y + H + 4, W + 6, 1, f[1]);            // sill
  if (lit) {
    R(X, Y, W, H, '#f8d868'); R(X, Y, W, 3, '#fff2b0'); R(X, Y + H - 3, W, 3, '#e0a840');
  } else {
    const gl = r7('#2e4a7a');
    for (let j = 0; j < H; j++) R(X, Y + j, W, 1, gl[Math.min(4, 1 + Math.floor(j * 4 / H))]);
    for (let j = 0; j < H; j++) { const a = Math.round(j * .7); if (a < W - 2) R(X + W - 3 - a, Y + j, 2, 1, gl[5]); }
    R(X, Y, W, 1, gl[0]);
  }
  R(X + (W >> 1), Y, 1, H, f[3]); R(X, Y + (H >> 1), W, 1, f[3]);                       // mullions
}
/* steps: lit treads, shaded risers */
function steps(X, Y, W, H, c) {
  const r = r7(c);
  for (let y = 0; y < H; y += 6) { R(X, Y + y, W, 2, r[5]); R(X, Y + y + 2, W, 4, r[2]); R(X, Y + y + 5, W, 1, r[1]); }
}
/* outline a box (1 px, the web's outline colour, softened to a dark violet) */
function box(X, Y, W, H) { R(X, Y, W, H, OUTL); }
/* a 32 px doorway (D cells inside a landmark): an arched opening, its reveal lit on the left, shadow inside */
function door32(X, Y) {
  R(X + 6, Y + 4, 20, 28, OUTL); R(X + 8, Y + 2, 16, 2, OUTL);
  R(X + 8, Y + 6, 16, 26, '#08060c'); R(X + 10, Y + 4, 12, 2, '#08060c');
  R(X + 8, Y + 6, 2, 26, '#2a2034'); R(X + 22, Y + 8, 2, 24, '#100c18');
  R(X + 10, Y + 24, 12, 8, '#181420');
}

const LM32 = {};
/* the web's sign font lacks a few letters its signs use (D, U...): add them */
Object.assign(PXF, Object.fromEntries(Object.entries({ D: '110101101101110', U: '101101101101111', G: '011100101101011', P: '110101110100100',
  J: '001001001101010', Q: '010101101110011', X: '101101010101101', Z: '111001010100111' }).filter(([k]) => !PXF[k])));

/* Virginia Museum of Fine Arts: limestone, a banner frieze in red and gold, a colonnade, a plinth */
LM32.vmfa = (X, Y, W, H) => {
  box(X, Y, W, H);
  ashlar(X + 2, Y + 2, W - 4, H - 4, '#d0ccc2', 3);
  cornice(X + 2, Y + 2, W - 4, '#e0dcd2', 16);
  const b = r7('#6c1c2c');
  R(X + (W >> 1) - 52, Y + 3, 104, 10, b[3]); R(X + (W >> 1) - 52, Y + 3, 104, 1, b[5]); R(X + (W >> 1) - 52, Y + 12, 104, 1, b[1]);
  for (let i = 0; i < 12; i++) R(X + (W >> 1) - 44 + i * 8, Y + 6, 5, 4, i % 3 == 1 ? '#fff2b0' : '#f2c14e');
  for (let i = 20; i < W - 16; i += 32) {                       // the colonnade, each bay's back wall in shadow
    R(X + i - 6, Y + 24, 24, H - 32, r7('#d0ccc2')[1]);
    column(X + i, Y + 22, 12, H - 30, '#ece8e0');
  }
  R(X + 2, Y + H - 10, W - 4, 8, r7('#a8a49c')[3]); R(X + 2, Y + H - 10, W - 4, 2, r7('#a8a49c')[5]); R(X + 2, Y + H - 3, W - 4, 1, r7('#a8a49c')[1]);
};

/* the Byrd Theatre: brick front, cream coping, the blue blade sign with its gold lights, the marquee chasing */
LM32.byrd = (X, Y, W, H, fr) => {
  bricks(X, Y, W, H, '#a86448', 11);
  const c = r7('#e8dcc0');
  R(X, Y, W, 6, c[4]); R(X, Y, W, 2, c[6]); R(X, Y + 5, W, 1, c[1]);
  for (let i = 8; i < W - 8; i += 40) if (Math.abs(i + 10 - (W >> 1)) > 24) win32(X + i, Y + 14, 16, 18, (i >> 3) % 3 == fr % 3, '#e8dcc0');
  // the blade sign
  const sx = X + (W >> 1) - 16, b = r7('#3d63c8');
  R(sx, Y - 12, 32, H - 22, OUTL); R(sx + 2, Y - 10, 28, H - 26, b[3]); R(sx + 2, Y - 10, 4, H - 26, b[4]); R(sx + 24, Y - 10, 6, H - 26, b[2]);
  for (let i = 0; i < 4; i++) {
    const on = (i + fr) % 2, ly = Y - 5 + i * 14;
    R(sx + 7, ly, 18, 10, on ? '#fff2a8' : '#f8d868'); R(sx + 7, ly, 18, 2, on ? '#ffffff' : '#fff2a8'); R(sx + 7, ly + 9, 18, 1, '#c89830');
  }
  // the marquee: a deep band, a row of bulbs chasing round
  const my = Y + H - 44;
  R(X + 8, my, W - 16, 16, OUTL); R(X + 10, my + 2, W - 20, 12, '#2a1830'); R(X + 10, my + 2, W - 20, 1, '#5a4060');
  for (let i = 12; i < W - 12; i += 6) {
    const on = ((i >> 1) + fr) % 3;
    R(X + i, my + 4, 3, 3, on ? '#fff2a8' : '#e83048'); R(X + i, my + 9, 3, 3, on ? '#e83048' : '#fff2a8');
  }
  R(X + 8, my + 16, W - 16, 3, '#100c18');
};

/* the Capitol: a temple front, the pediment's tympanum shaded under its raking cornice, six columns before a
   portico in shadow, a broad stylobate */
LM32.capitol = (X, Y, W, H) => {
  const s = r7('#ece8de');
  // pediment
  for (let i = 0; i < 28; i++) {
    const ww = Math.round(W * (i + 1) / 28), a = X + ((W - ww) >> 1);
    R(a, Y + i, ww, 1, i < 3 ? s[6] : s[4]);
    R(a, Y + i, 2, 1, OUTL); R(a + ww - 2, Y + i, 2, 1, OUTL);
    if (i > 6) { R(a + 6, Y + i, Math.max(0, ww - 12), 1, i > 20 ? s[2] : s[3]); }
  }
  R(X + (W >> 1) - 20, Y + 16, 40, 5, s[5]); R(X + (W >> 1) - 20, Y + 21, 40, 1, s[2]);   // the oculus band
  R(X, Y + 28, W, 2, OUTL);
  cornice(X + 2, Y + 30, W - 4, '#ece8de', 10);
  R(X, Y + 28, 2, H - 28, OUTL); R(X + W - 2, Y + 28, 2, H - 28, OUTL);
  R(X + 8, Y + 40, W - 16, H - 52, s[0]);                       // the portico, deep in shadow
  R(X + 8, Y + 40, W - 16, 4, '#100c18');
  for (let i = 16; i < W - 16; i += 28) column(X + i, Y + 38, 14, H - 50, '#f4f2ec');
  steps(X + 2, Y + H - 12, W - 4, 12, '#e4e2d8');
};

/* St. John's Church: white clapboard, a grey roof band, the steeple with its louvred belfry, blue window and spire */
LM32.church = (X, Y, W, H, fr) => {
  const by = Y + 56, w7 = r7('#ecebe4'), gr = r7('#5a5662');
  R(X, by, W, H - 56, OUTL);
  for (let y = by + 2; y < Y + H - 2; y += 8) { R(X + 2, y, W - 4, 6, w7[4]); R(X + 2, y, W - 4, 1, w7[6]); R(X + 2, y + 6, W - 4, 2, w7[2]); }
  R(X - 4, by - 12, W + 8, 14, OUTL); R(X, by - 10, W, 10, gr[3]); R(X, by - 10, W, 2, gr[5]); for (let i = 4; i < W; i += 8) R(X + i, by - 8, 1, 8, gr[2]);
  win32(X + 18, by + 26, 14, 26, (fr >> 1) & 1, '#ecebe4'); win32(X + W - 32, by + 26, 14, 26, !((fr >> 1) & 1), '#ecebe4');
  // the tower
  const tx = X + (W >> 1) - 20;
  R(tx, Y + 16, 40, 44, OUTL);
  for (let y = Y + 18; y < Y + 60; y += 8) { R(tx + 2, y, 36, 6, w7[4]); R(tx + 2, y, 36, 1, w7[6]); R(tx + 2, y + 6, 36, 2, w7[2]); }
  R(tx + 8, Y + 24, 24, 16, OUTL); R(tx + 10, Y + 26, 20, 12, '#28406a');
  for (let i = 0; i < 4; i++) R(tx + 10, Y + 27 + i * 3, 20, 1, '#5a82b8');                // louvres
  R(tx + 8, Y, 24, 18, OUTL); R(tx + 10, Y + 2, 20, 16, gr[3]); R(tx + 10, Y + 2, 6, 16, gr[5]); R(tx + 26, Y + 2, 4, 16, gr[1]);
  R(X + (W >> 1) - 2, Y - 12, 4, 14, OUTL); R(X + (W >> 1) - 6, Y - 8, 12, 4, OUTL); R(X + (W >> 1) - 1, Y - 11, 1, 10, '#f2c14e');
};

/* Main Street Station: brick, a red tile roof, arched windows in two storeys, the clock tower */
LM32.station = (X, Y, W, H, fr) => {
  const bw = W - 56, rf = r7('#c0603c');
  bricks(X, Y + 24, bw, H - 24, '#a85840', 21);
  R(X, Y + 16, bw, 12, OUTL);
  for (let i = 0; i < bw - 4; i += 6) { R(X + 2 + i, Y + 18, 5, 8, rf[3]); R(X + 2 + i, Y + 18, 2, 8, rf[5]); R(X + 6 + i, Y + 18, 1, 8, rf[1]); }
  for (let i = 16; i < bw - 16; i += 40) { win32(X + i, Y + 44, 20, 28, ((i >> 4) + (fr >> 1)) % 3 == 0, '#e8dcc0'); win32(X + i, Y + 96, 20, 28, false, '#e8dcc0'); }
  const tx = X + W - 56;
  bricks(tx, Y + 12, 56, H - 12, '#a85840', 23);
  R(tx - 4, Y, 64, 16, OUTL); for (let i = 0; i < 56; i += 6) { R(tx + i, Y + 2, 5, 12, rf[3]); R(tx + i, Y + 2, 2, 12, rf[5]); }
  R(tx + 24, Y - 12, 8, 14, OUTL); R(tx + 26, Y - 10, 4, 10, '#f2c14e');
  // the clock: a white face in a dark ring, its hands at ten past
  R(tx + 10, Y + 28, 36, 36, OUTL); R(tx + 12, Y + 30, 32, 32, '#f4f2ec'); R(tx + 12, Y + 30, 32, 3, '#ffffff'); R(tx + 12, Y + 59, 32, 3, '#c8c4b8');
  for (let k = 0; k < 12; k++) { const a = k * Math.PI / 6; R(tx + 28 + Math.round(Math.sin(a) * 13) - 1, Y + 46 - Math.round(Math.cos(a) * 13) - 1, 2, 2, OUTL); }
  R(tx + 27, Y + 36, 2, 11, OUTL); R(tx + 28, Y + 45, 9, 2, OUTL);
  win32(tx + 18, Y + 84, 20, 28, true, '#e8dcc0');
};

/* the Broad Street department stores: the store's colour in dressed stone, a cream cornice, three storeys of
   windows, the red sign band with its gold letters, a striped awning, display windows */
LM32.store = (X, Y, W, H, fr, s, L) => {
  const c = L.c || '#c8b48c';
  box(X, Y, W, H);
  ashlar(X + 2, Y + 2, W - 4, H - 2, c, 31);
  cornice(X + 2, Y + 2, W - 4, '#efe4c8', 10);
  for (let r = 0; r < 3; r++) for (let i = 16; i < W - 24; i += 36) win32(X + i, Y + 22 + r * 36, 20, 22, ((i + r * 7 + (fr >> 1)) % 5) == 0, '#efe4c8');
  const sb = r7('#6c1c2c'), sy = Y + H - 52;
  R(X + 2, sy, W - 4, 16, sb[3]); R(X + 2, sy, W - 4, 2, sb[5]); R(X + 2, sy + 15, W - 4, 1, sb[0]);
  for (let i = 12; i < W - 12; i += 20) { R(X + i, sy + 4, 12, 8, '#f2c14e'); R(X + i, sy + 4, 12, 2, '#fff2b0'); R(X + i, sy + 11, 12, 1, '#a87820'); }
  for (let i = 2; i < W - 2; i += 8) {                         // the awning, red and white, its scalloped edge in shadow
    const red = (i >> 3) & 1;
    R(X + i, sy + 16, 8, 8, red ? '#e83048' : '#f4f0e8'); R(X + i, sy + 16, 8, 2, red ? '#f87080' : '#ffffff');
    R(X + i + 1, sy + 24, 6, 2, red ? '#a81830' : '#c8c4bc');
  }
  R(X + 2, sy + 26, W - 4, 2, '#100c18');
  const cr = r7('#efe4c8');
  R(X + 2, sy + 28, W - 4, 22, cr[3]); R(X + 2, sy + 28, W - 4, 2, cr[5]);
  for (let i = 12; i < W - 32; i += 44) {
    const gl = r7('#28406a');
    R(X + i, sy + 31, 28, 18, OUTL); R(X + i + 2, sy + 33, 24, 16, gl[2]); R(X + i + 2, sy + 33, 24, 3, gl[4]);
    for (let j = 0; j < 14; j++) R(X + i + 20 - j, sy + 34 + j, 2, 1, gl[5]);
  }
};

/* ---------- house fronts at 32 px: the door, sash windows, the Fan's bay, awnings, iron porches ----------
   Same choices as the 16 px facade (snes_style.js) from the same cell hash, drawn at full resolution. Returns
   true if it drew the cell's front (snes_hires then drops the Scale2x'd one). */
const CRM = r7('#efe4c8');
function sash32(X, Y, W, H, lit) {   // a sash window: cream frame, glass, the meeting rail, a sill
  R(X - 2, Y - 2, W + 4, H + 4, CRM[3]); R(X - 2, Y - 2, W + 4, 1, CRM[6]); R(X + W + 1, Y - 1, 1, H + 3, CRM[1]);
  if (lit) { R(X, Y, W, H, '#f8d868'); R(X, Y, W, 2, '#fff2b0'); R(X, Y + 2, 2, H - 4, '#fff2b0'); R(X, Y + H - 2, W, 2, '#e0a840'); }
  else {
    const gl = r7('#2e4a7a');
    for (let j = 0; j < H; j++) R(X, Y + j, W, 1, gl[Math.min(4, 1 + Math.floor(j * 4 / H))]);
    R(X, Y, W, 1, gl[0]);
    for (let j = 1; j < Math.min(H, W) - 1; j++) R(X + W - 2 - j, Y + j, 1, 1, gl[5]);
    R(X + W - 3, Y + 2, 1, 2, gl[6]);
  }
  R(X, Y + (H >> 1), W, 2, CRM[4]); R(X, Y + (H >> 1) + 1, W, 1, CRM[2]); R(X + (W >> 1), Y, 1, H, CRM[3]);
  R(X - 3, Y + H + 2, W + 6, 2, CRM[5]); R(X - 3, Y + H + 4, W + 6, 1, CRM[0]);
}
function ironPorch32(X, Y) {   // a Jackson Ward iron porch: a rail, posts, a scrolled frieze, balusters
  const I = '#1a1a22', H = '#5a5a6a', M = '#33333f';
  R(X, Y + 10, 32, 4, I); R(X, Y + 10, 32, 1, H);
  R(X + 2, Y + 14, 4, 16, I); R(X + 26, Y + 14, 4, 16, I); R(X + 2, Y + 14, 1, 16, H); R(X + 26, Y + 14, 1, 16, H);
  for (let i = 7; i < 25; i += 6) { R(X + i, Y + 16, 4, 1, I); R(X + i, Y + 17, 1, 2, I); R(X + i + 3, Y + 17, 1, 2, I); R(X + i + 1, Y + 19, 2, 1, M); R(X + i, Y + 16, 2, 1, H); }
  R(X, Y + 22, 32, 2, I); R(X, Y + 22, 32, 1, H); R(X, Y + 29, 32, 2, I);
  for (let i = 1; i < 32; i += 3) { R(X + i, Y + 24, 1, 5, I); if (i % 6 == 1) R(X + i, Y + 24, 1, 2, H); }
}
function facade32(s, X, Y, h, tx, ty, r) {
  const ft = h % 4;
  if (s.hs == 'fan' && ft > 0 && ft < 3) {   // the bay: three tall windows in a cream surround, a lit top, its shadow below
    R(X + 3, Y + 8, 26, 22, CRM[2]); R(X + 3, Y + 8, 26, 2, CRM[6]); R(X + 3, Y + 8, 2, 22, CRM[5]); R(X + 27, Y + 10, 2, 20, CRM[1]);
    for (const a of [7, 14, 21]) sash32(X + a, Y + 12, 4, 14, (h >> (5 + Math.round(a / 2.3))) % 3 == 0);
    R(X + 3, Y + 30, 26, 2, r[0]);
    if (wantsPorch(s, h)) ironPorch32(X, Y);
    return true;
  }
  if (s.hs && s.hs != 'jw' && s.hs != 'fan') return false;   // other districts' styled fronts stay as they are
  if (ft == 0) {   // a panelled door under a lit transom, on a stone step
    R(X + 5, Y + 7, 22, 25, CRM[3]); R(X + 5, Y + 7, 22, 2, CRM[6]); R(X + 25, Y + 9, 2, 23, CRM[1]);
    R(X + 8, Y + 10, 16, 5, '#1c2a4a'); R(X + 10, Y + 11, 4, 2, '#f8d868'); R(X + 15, Y + 11, 4, 2, '#f8d868'); R(X + 8, Y + 10, 16, 1, '#5a82b8');
    const wd = r7(SWD[2]);
    R(X + 8, Y + 16, 16, 15, wd[3]); R(X + 8, Y + 16, 16, 1, wd[5]); R(X + 23, Y + 16, 1, 15, wd[1]);
    for (const [a, b] of [[10, 18], [17, 18], [10, 25], [17, 25]]) { R(X + a, Y + b, 5, 5, wd[2]); R(X + a, Y + b, 5, 1, wd[1]); R(X + a, Y + b + 4, 5, 1, wd[4]); }
    R(X + 21, Y + 24, 2, 2, '#f2c14e');
    R(X + 3, Y + 30, 26, 2, SST[5]); R(X + 3, Y + 31, 26, 1, SST[2]);
  } else if (ft < 3) {
    sash32(X + 9, Y + 11, 14, 15, (h >> 5) % 3 == 0);
    R(X + 6, Y + 7, 20, 3, CRM[4]); R(X + 6, Y + 7, 20, 1, CRM[6]); R(X + 14, Y + 7, 4, 3, CRM[5]);   // lintel and keystone
  }
  if (s.awn && ft < 3) {
    const ac = [['#e83048', '#f4f0e8', '#a01c30'], ['#2e9d6b', '#f4f0e8', '#1c6a48'], ['#3d63c8', '#fff2a8', '#283e88']][(tx >> 1) % 3];
    for (let i = 0; i < 32; i += 4) {
      const c = ac[(i >> 2) & 1];
      R(X + i, Y + 6, 4, 7, c); R(X + i, Y + 6, 4, 1, '#ffffff');
      R(X + i + 1, Y + 13, 2, 2, (i >> 2) & 1 ? '#c8c4bc' : ac[2]);
    }
    R(X, Y + 15, 32, 1, r[0]);
  }
  if (s.hs == 'jw' && ft > 0 && wantsPorch(s, h)) ironPorch32(X, Y);
  return true;
}
function storey32(s, X, Y, h) {   // the upper floor: two sash windows, a cream string course below
  for (const a of [5, 19]) sash32(X + a, Y + 6, 8, 16, HH(h, a == 5 ? 2 : 9, 5) % 4 == 0);
  R(X, Y + 28, 32, 2, CRM[4]); R(X, Y + 28, 32, 1, CRM[6]); R(X, Y + 30, 32, 2, '#a08c6c');
  return true;
}

/* ---------- props at 32 px (stage 14): the web's props, redrawn ----------
   Each draws over its cell's ground (already drawn) at (X, Y); h is the cell's hash, nb its neighbours. */
const PROP32 = {};
const IRON = r7('#2a2a34');
function softShadow(X, Y, W, H) { R(X + 2, Y, W - 4, H, 'shade'); R(X, Y + 1, W, H - 2, 'shade'); }
/* a clipped shrub: a dome of leaf clusters, dark underneath, lit on top, a few bright leaves */
function shrub(X, Y, W, H, h, G) {
  G = G || r7('#2f7a3c');
  const rows = [];
  for (let j = 0; j < H; j++) { const t = (j + .5) / H * 2 - 1; rows.push(Math.round(W * Math.sqrt(Math.max(0, 1 - t * t * (j < H / 2 ? 1 : .35))))); }
  rows.forEach((w, j) => { const a = X + ((W - w) >> 1); R(a, Y + j, w, 1, j > H * .7 ? G[1] : j > H * .45 ? G[2] : G[3]); if (w > 2) { R(a, Y + j, 1, 1, G[0]); R(a + w - 1, Y + j, 1, 1, G[0]); } });
  for (let i = 0; i < Math.round(W * H / 40); i++) {
    const q = HH(h, i, 53), a = X + 3 + q % (W - 8), b = Y + 2 + (q >> 5) % Math.max(1, H - 8), lit = (a - X) + (b - Y) * 1.4 < (W + H) * .55;
    R(a, b + 2, 4, 1, G[lit ? 2 : 0]); R(a, b, 4, 2, G[lit ? 4 : 2]); R(a + 1, b - 1, 2, 1, G[lit ? 5 : 3]);
  }
}
PROP32.b = (s, X, Y, h) => { softShadow(X + 2, Y + 24, 28, 6); shrub(X + 2, Y + 3, 28, 24, h); };
PROP32.t = (s, X, Y, h) => {   // a street tree in its pit
  R(X + 4, Y + 24, 24, 6, OUTL); R(X + 6, Y + 25, 20, 4, '#5e4a34'); R(X + 6, Y + 25, 20, 1, '#7a6448');
  softShadow(X + 6, Y + 22, 24, 4);
  R(X + 14, Y + 14, 4, 12, SWD[2]); R(X + 14, Y + 14, 1, 12, SWD[3]); R(X + 17, Y + 14, 1, 12, SWD[1]);
  shrub(X + 3, Y - 2, 26, 18, h, r7('#3a8a40'));
};
PROP32.u = (s, X, Y, h) => {   // a planter box of flowers
  softShadow(X + 4, Y + 26, 26, 4);
  const w = r7('#8a5a34');
  R(X + 4, Y + 16, 24, 12, OUTL); R(X + 6, Y + 18, 20, 9, w[3]); R(X + 6, Y + 18, 20, 2, w[5]); R(X + 6, Y + 25, 20, 2, w[1]);
  for (let i = 8; i < 26; i += 6) R(X + i, Y + 20, 1, 5, w[2]);
  shrub(X + 5, Y + 5, 22, 14, h, r7('#2f7a3c'));
  const c = [['#f08098', '#c05070'], ['#f8e060', '#c89830'], ['#b890f8', '#7058b8']][h % 3];
  for (let i = 0; i < 6; i++) { const q = HH(h, i, 59), a = X + 8 + q % 16, b = Y + 6 + (q >> 4) % 8; R(a, b, 3, 3, c[0]); R(a + 1, b + 1, 1, 1, '#fff6c0'); R(a, b + 2, 3, 1, c[1]); }
};
PROP32.l = (s, X, Y) => {   // a gas lamp: a fluted iron post, the lantern's glass glowing warm, a finial
  softShadow(X + 10, Y + 28, 14, 3);
  R(X + 12, Y + 26, 8, 4, OUTL); R(X + 13, Y + 26, 6, 3, IRON[3]);
  R(X + 14, Y + 10, 4, 18, OUTL); R(X + 15, Y + 10, 1, 18, IRON[5]); R(X + 16, Y + 10, 1, 18, IRON[3]);
  R(X + 10, Y + 2, 12, 10, OUTL); R(X + 11, Y + 3, 10, 7, '#f8d868'); R(X + 12, Y + 4, 3, 5, '#fff6c0'); R(X + 18, Y + 3, 3, 7, '#e0a840');
  R(X + 9, Y + 1, 14, 2, IRON[3]); R(X + 9, Y + 1, 14, 1, IRON[5]); R(X + 15, Y - 2, 2, 3, OUTL); R(X + 11, Y + 10, 10, 2, IRON[2]);
};
PROP32.M = (s, X, Y) => {   // a signpost: a post, a white board with lines of lettering, a dark frame
  softShadow(X + 8, Y + 28, 16, 3);
  R(X + 14, Y + 18, 4, 12, OUTL); R(X + 15, Y + 18, 1, 12, '#5a5a68');
  R(X + 2, Y + 2, 28, 18, OUTL); R(X + 4, Y + 4, 24, 14, '#d8dce4'); R(X + 4, Y + 4, 24, 2, '#ffffff'); R(X + 4, Y + 16, 24, 2, '#9aa0b0');
  R(X + 7, Y + 8, 18, 2, '#3a3a54'); R(X + 7, Y + 12, 13, 2, '#3a3a54');
};
PROP32.i = (s, X, Y) => {   // a park bench: two slats of the seat and the back, iron ends
  softShadow(X + 3, Y + 26, 28, 4);
  const w = r7('#a8743a');
  R(X + 2, Y + 8, 28, 6, OUTL); R(X + 3, Y + 9, 26, 4, w[3]); R(X + 3, Y + 9, 26, 1, w[5]); R(X + 3, Y + 12, 26, 1, w[1]);
  R(X + 2, Y + 15, 28, 8, OUTL); R(X + 3, Y + 16, 26, 3, w[4]); R(X + 3, Y + 16, 26, 1, w[6]); R(X + 3, Y + 19, 26, 3, w[3]); R(X + 3, Y + 21, 26, 1, w[1]);
  for (const a of [3, 25]) { R(X + a, Y + 8, 4, 22, OUTL); R(X + a + 1, Y + 9, 1, 20, IRON[5]); }
};
PROP32['+'] = (s, X, Y) => {   // a granite monument stone
  softShadow(X + 6, Y + 25, 24, 5);
  const g2 = r7('#7a7c88');
  R(X + 9, Y + 6, 14, 22, OUTL); R(X + 11, Y + 8, 10, 19, g2[3]); R(X + 11, Y + 8, 3, 19, g2[5]); R(X + 19, Y + 8, 2, 19, g2[1]);
  R(X + 7, Y + 25, 18, 5, OUTL); R(X + 8, Y + 26, 16, 3, g2[4]); R(X + 8, Y + 26, 16, 1, g2[6]);
  R(X + 13, Y + 12, 6, 1, g2[1]); R(X + 13, Y + 15, 6, 1, g2[1]);
};
PROP32.x = (s, X, Y, h) => {   // a headstone, rounded, lettered, moss at its foot
  softShadow(X + 9, Y + 26, 20, 4);
  const g2 = r7('#a0a4ae');
  R(X + 8, Y + 6, 16, 22, OUTL); R(X + 10, Y + 4, 12, 2, OUTL);
  R(X + 10, Y + 6, 12, 20, g2[3]); R(X + 11, Y + 5, 10, 2, g2[4]); R(X + 10, Y + 6, 3, 20, g2[5]); R(X + 20, Y + 7, 2, 19, g2[1]);
  R(X + 14, Y + 9, 4, 10, g2[1]); R(X + 12, Y + 12, 8, 2, g2[1]);
  R(X + 7, Y + 27, 4, 2, '#2f7a38'); R(X + 21, Y + 27, 4, 2, '#2f7a38'); R(X + 8, Y + 26, 2, 1, '#58a850');
};
PROP32.q = (s, X, Y) => {   // a fire hydrant
  softShadow(X + 10, Y + 27, 14, 3);
  const r = r7('#e83048');
  R(X + 10, Y + 8, 12, 20, OUTL); R(X + 12, Y + 4, 8, 4, OUTL); R(X + 14, Y + 2, 4, 2, OUTL); R(X + 7, Y + 14, 18, 5, OUTL);
  R(X + 11, Y + 9, 10, 18, r[3]); R(X + 11, Y + 9, 3, 18, r[5]); R(X + 19, Y + 9, 2, 18, r[1]);
  R(X + 13, Y + 5, 6, 3, r[4]); R(X + 8, Y + 15, 16, 3, r[3]); R(X + 8, Y + 15, 16, 1, r[5]);
  R(X + 9, Y + 26, 14, 2, r[1]);
};
PROP32.a = (s, X, Y, h) => {   // a café umbrella, striped, on its pole
  softShadow(X + 4, Y + 26, 26, 4);
  R(X + 15, Y + 10, 2, 18, '#5a5662'); R(X + 15, Y + 10, 1, 18, '#8a8692');
  const c = r7(['#e83048', '#2e9d6b', '#3d63c8', '#f2c14e'][h % 4]);
  for (let j = 0; j < 10; j++) {
    const w = 8 + j * 2, a = X + 16 - (w >> 1);
    for (let i = 0; i < w; i++) R(a + i, Y + 2 + j, 1, 1, ((i * 6 / w) | 0) & 1 ? (j < 3 ? '#ffffff' : '#e8e4dc') : (j < 3 ? c[5] : j > 7 ? c[2] : c[3]));
  }
  R(X + 6, Y + 12, 20, 2, OUTL); R(X + 15, Y, 2, 2, OUTL);
};
PROP32.K = (s, X, Y) => {   // a split-rail of upright boards
  const w = r7('#6a4a30');
  softShadow(X, Y + 28, 32, 3);
  for (let i = 2; i < 32; i += 10) { R(X + i, Y + 2, 6, 28, OUTL); R(X + i + 1, Y + 3, 4, 26, w[3]); R(X + i + 1, Y + 3, 1, 26, w[5]); R(X + i + 4, Y + 3, 1, 26, w[1]); R(X + i + 1, Y + 3, 4, 1, w[6]); }
};
PROP32.k = (s, X, Y) => {   // horizontal boards
  const w = r7('#6a4a30');
  for (let i = 2; i < 32; i += 10) { R(X + 2, Y + i, 28, 6, OUTL); R(X + 3, Y + i + 1, 26, 4, w[3]); R(X + 3, Y + i + 1, 26, 1, w[5]); R(X + 3, Y + i + 4, 26, 1, w[1]); }
};
PROP32.f = (s, X, Y, h, nb) => {   // wrought iron: a run of railings with spear finials, or a single post
  if (nb(-1, 0) == 'f' || nb(1, 0) == 'f') {
    softShadow(X, Y + 28, 32, 3);
    R(X, Y + 12, 32, 2, OUTL); R(X, Y + 22, 32, 2, OUTL); R(X, Y + 12, 32, 1, IRON[5]);
    for (let i = 3; i < 32; i += 6) {
      R(X + i, Y + 6, 2, 22, OUTL); R(X + i, Y + 6, 1, 22, IRON[5]);
      R(X + i - 1, Y + 5, 4, 2, OUTL); R(X + i, Y + 3, 2, 2, OUTL); R(X + i, Y + 3, 1, 1, '#f2c14e');
    }
  } else {
    R(X + 19, Y, 2, 32, 'shade');
    R(X + 14, Y, 4, 32, OUTL); R(X + 14, Y, 1, 32, IRON[5]);
    for (let i = 0; i < 32; i += 16) { R(X + 12, Y + i, 8, 4, IRON[3]); R(X + 12, Y + i, 8, 1, IRON[5]); R(X + 14, Y + i - 2, 4, 2, '#f2c14e'); }
  }
};

/* ---------- landmarks, second batch ---------- */
/* the Hippodrome: pale brick, a dark coping, a red blade sign with chasing bulbs, the marquee */
LM32.hippodrome = (X, Y, W, H, fr) => {
  bricks(X, Y, W, H, '#d0c098', 41);
  const cp = r7('#8a7a5a'); R(X, Y, W, 6, cp[3]); R(X, Y, W, 2, cp[5]); R(X, Y + 5, W, 1, cp[0]);
  for (const [a, lit] of [[20, true], [60, false], [100, true]]) win32(X + a, Y + 20, 20, 20, lit, '#efe4c8');
  const sx = X + W - 44, rd = r7('#6c1c2c');
  R(sx, Y, 20, H - 28, OUTL); R(sx + 2, Y + 2, 16, H - 32, rd[3]); R(sx + 2, Y + 2, 4, H - 32, rd[4]); R(sx + 14, Y + 2, 4, H - 32, rd[2]);
  for (let i = 0; i < 5; i++) { const on = (i + fr) % 2; R(sx + 6, Y + 8 + i * 14, 8, 8, on ? '#fff2a8' : '#f2c14e'); R(sx + 6, Y + 8 + i * 14, 8, 2, on ? '#ffffff' : '#fff2a8'); }
  const my = Y + H - 60, mw = W - 80;
  R(X + 16, my, mw, 24, OUTL); R(X + 18, my + 2, mw - 4, 20, '#e8e4dc'); R(X + 18, my + 2, mw - 4, 2, '#ffffff'); R(X + 18, my + 20, mw - 4, 2, '#b8b4ac');
  for (let i = 20; i < mw + 12; i += 8) R(X + i, my - 4, 4, 4, ((i >> 3) + fr) % 2 ? '#fff2a8' : '#e83048');
  R(X + 24, my + 8, mw - 20, 8, rd[3]); R(X + 24, my + 8, mw - 20, 2, rd[5]);
  for (let i = 28; i < mw - 4; i += 10) R(X + i, my + 10, 6, 4, '#f2c14e');
};
/* the 1780 bell tower: brick, a stone cap, the bell in its opening */
LM32.belltower = (X, Y, W, H) => {
  bricks(X, Y + 12, W, H - 12, '#a85840', 51);
  const st = r7('#c8c4b8');
  R(X + 4, Y, W - 8, 14, OUTL); R(X + 6, Y + 2, W - 12, 12, st[3]); R(X + 6, Y + 2, W - 12, 3, st[5]); R(X + 6, Y + 12, W - 12, 2, st[1]);
  R(X + (W >> 1) - 2, Y - 8, 4, 10, OUTL);
  R(X + 14, Y + 26, W - 28, 22, OUTL); R(X + 16, Y + 28, W - 32, 20, '#100c18');
  const b = r7('#f2c14e'); R(X + (W >> 1) - 7, Y + 31, 14, 12, b[3]); R(X + (W >> 1) - 7, Y + 31, 4, 12, b[5]); R(X + (W >> 1) + 4, Y + 31, 3, 12, b[1]);
  R(X + (W >> 1) - 9, Y + 42, 18, 3, b[2]); R(X + (W >> 1) - 1, Y + 44, 3, 3, OUTL);
};
/* the 17th Street market shed: a green standing-seam roof, iron posts, stalls of produce under it */
LM32.market = (X, Y, W, H) => {
  const gr = r7('#2e7d4f');
  R(X, Y, W, 16, OUTL);
  for (let i = 2; i < W - 2; i += 12) { R(X + i, Y + 2, 12, 12, gr[3]); R(X + i, Y + 2, 3, 12, gr[5]); R(X + i + 10, Y + 2, 2, 12, gr[1]); }
  R(X + 2, Y + 2, W - 4, 2, gr[6]); R(X, Y + 16, W, 2, '#100c18');
  for (let i = 8; i < W; i += 52) { R(X + i, Y + 16, 6, H - 16, OUTL); R(X + i + 1, Y + 16, 2, H - 16, '#8a8692'); }
  const cs = ['#e83048', '#f2c14e', '#58a850', '#f08030'];
  for (let i = 20; i < W - 16; i += 32) {
    const w = r7('#8a5a34'), c = r7(cs[(i >> 5) % 4]);
    R(X + i, Y + 36, 20, 16, OUTL); R(X + i + 2, Y + 38, 16, 12, w[3]); R(X + i + 2, Y + 38, 16, 2, w[5]);
    for (let k = 0; k < 6; k++) { const a = X + i + 2 + (k % 3) * 6, b = Y + 30 + (k > 2 ? 4 : 0); R(a, b, 5, 5, c[3]); R(a + 1, b + 1, 2, 2, c[5]); R(a, b + 4, 5, 1, c[1]); }
  }
};
/* the Carillon: a tall tan brick tower, its belfry with bronze bells swinging, slit windows, a copper cap */
LM32.carillon = (X, Y, W, H, fr) => {
  const cx = X + (W >> 1);
  R(cx - 34, Y + 24, 68, H - 24, OUTL); bricks(cx - 32, Y + 26, 64, H - 28, '#c8a078', 61);
  for (let y = Y + 60; y < Y + H - 20; y += 48) R(cx - 32, y, 64, 3, r7('#c8a078')[5]);   // string courses
  R(cx - 24, Y + 8, 48, 20, OUTL); bricks(cx - 22, Y + 10, 44, 18, '#c8a078', 62);
  const cu = r7('#7a8a6a');
  R(cx - 8, Y - 12, 16, 22, OUTL); R(cx - 6, Y - 10, 12, 20, cu[3]); R(cx - 6, Y - 10, 4, 20, cu[5]); R(cx + 3, Y - 10, 3, 20, cu[1]);
  R(cx - 2, Y - 20, 4, 10, OUTL);
  for (let i = -18; i < 18; i += 14) {                       // the belfry openings, a bell in each
    R(cx + i, Y + 32, 10, 28, OUTL); R(cx + i + 2, Y + 34, 6, 26, '#100c18');
    const b = r7((fr >> 1) & 1 ? '#c08840' : '#a87430'), sw = ((fr >> 1) & 1) ? 1 : 0;
    R(cx + i + 2 + sw, Y + 44, 6, 8, b[3]); R(cx + i + 2 + sw, Y + 44, 2, 8, b[5]); R(cx + i + 1 + sw, Y + 51, 8, 2, b[2]);
  }
  for (let i = 0; i < 4; i++) { R(cx - 4, Y + 88 + i * 32, 8, 16, OUTL); R(cx - 2, Y + 90 + i * 32, 4, 12, '#28406a'); R(cx - 2, Y + 90 + i * 32, 4, 2, '#5a82b8'); }
};
/* Maymont: rusticated sandstone, a red tile roof and the tower, windows with stone surrounds */
LM32.mansion = (X, Y, W, H, fr) => {
  R(X, Y + 20, W, H - 20, OUTL); ashlar(X + 2, Y + 22, W - 4, H - 24, '#a89a88', 71);
  const rf = r7('#8a3a2a');
  R(X - 4, Y + 12, W + 8, 12, OUTL); for (let i = 0; i < W; i += 6) { R(X + i, Y + 14, 5, 8, rf[3]); R(X + i, Y + 14, 2, 8, rf[5]); R(X + i + 4, Y + 14, 1, 8, rf[1]); }
  const tx = X + W - 48;
  R(tx, Y - 8, 36, 32, OUTL); ashlar(tx + 2, Y - 6, 32, 30, '#a89a88', 72);
  R(tx - 2, Y - 22, 40, 16, OUTL); for (let i = 0; i < 36; i += 6) { R(tx + i, Y - 20, 5, 12, rf[3]); R(tx + i, Y - 20, 2, 12, rf[5]); }
  win32(tx + 12, Y + 4, 12, 16, true, '#d8ccb0');
  for (let i = 16; i < W - 60; i += 32) win32(X + i, Y + 36, 16, 24, ((i >> 4) + fr) % 3 == 0, '#d8ccb0');
  for (let i = 16; i < W - 24; i += 32) win32(X + i, Y + 80, 16, 24, false, '#d8ccb0');
};
/* the Hollywood pyramid: dry-laid granite in stepped courses, lit on the left face, shaded on the right */
LM32.pyramid = (X, Y, W, H) => {
  const gr = r7('#b0aca0');
  for (let i = 0; i < H; i++) {
    const ww = Math.max(8, Math.round(W * (i + 1) / H)), a = X + ((W - ww) >> 1), half = ww >> 1, course = (i % 10);
    R(a, Y + i, ww, 1, OUTL);
    R(a + 2, Y + i, half - 2, 1, course == 9 ? gr[2] : course < 2 ? gr[6] : gr[5]);
    R(a + half, Y + i, ww - half - 2, 1, course == 9 ? gr[0] : course < 2 ? gr[4] : gr[3]);
    if (course == 5) for (let k = a + 6 + (i % 20 ? 8 : 0); k < a + ww - 6; k += 16) R(k, Y + i - 4, 1, 4, k < a + half ? gr[4] : gr[2]);
  }
};
/* the birdcage memorial: a granite plinth, iron bars and their dome, the stone figure inside */
LM32.birdcage = (X, Y, W, H) => {
  const st = r7('#8a8690');
  R(X + 2, Y + H - 12, W - 4, 12, OUTL); R(X + 4, Y + H - 10, W - 8, 8, st[3]); R(X + 4, Y + H - 10, W - 8, 2, st[6]); R(X + 4, Y + H - 4, W - 8, 2, st[1]);
  const gy = r7('#7a7a84');
  R(X + (W >> 1) - 8, Y + 28, 16, 20, OUTL); R(X + (W >> 1) - 6, Y + 30, 12, 18, gy[3]); R(X + (W >> 1) - 6, Y + 30, 4, 18, gy[5]); R(X + (W >> 1) - 4, Y + 22, 8, 8, gy[4]);
  for (let i = 6; i < W - 4; i += 8) { R(X + i, Y + 12, 2, H - 24, IRON[1]); R(X + i, Y + 12, 1, H - 24, IRON[5]); }
  R(X + 4, Y + 12, W - 8, 2, IRON[1]); R(X + 4, Y + H - 16, W - 8, 2, IRON[1]);
  for (let i = 0; i < 12; i++) R(X + (W >> 1) - i, Y + 12 - i, 2 * i + 2, 1, IRON[1]);
  R(X + (W >> 1) - 2, Y - 4, 4, 6, OUTL);
};
/* the Diamond's grandstand: green tiers, white posts, a crowd of colours that shifts with the frame */
LM32.grandstand = (X, Y, W, H, fr) => {
  R(X, Y, W, H, OUTL);
  const g1 = r7('#2e7d4f'), g2 = r7('#3a9a60');
  for (let r = 0; r < 4; r++) { const g3 = r & 1 ? g1 : g2; R(X + 2, Y + 4 + r * 14, W - 4, 10, g3[3]); R(X + 2, Y + 4 + r * 14, W - 4, 2, g3[5]); R(X + 2, Y + 12 + r * 14, W - 4, 2, g3[0]); }
  for (let i = 8; i < W; i += 48) { R(X + i, Y, 4, H, '#c8ccd4'); R(X + i, Y, 1, H, '#ffffff'); R(X + i + 3, Y, 1, H, '#8a8c94'); }
  R(X, Y, W, 4, '#e8e4dc'); R(X, Y, W, 1, '#ffffff');
  const cols = ['#e83048', '#f2c14e', '#ffffff', '#3d63c8', '#f08098'];
  for (let i = 0; i < 48; i++) { const q = HH(i, fr >> 1, 7), a = X + 6 + q % (W - 12), b = Y + 5 + ((q >> 8) % 4) * 14; R(a, b, 4, 4, cols[q % 5]); R(a, b + 4, 4, 2, '#1a1423'); R(a + 1, b, 2, 1, '#fff8e8'); }
};
/* a GRTC bus shelter: a blue roof, glass panels catching the light, a bench, the sign */
LM32.shelter = (X, Y, W, H) => {
  const b = r7('#3d63c8');
  R(X, Y + 2, W, 8, OUTL); R(X + 2, Y + 4, W - 4, 4, b[3]); R(X + 2, Y + 4, W - 4, 1, b[6]);
  for (const a of [4, W - 8]) { R(X + a, Y + 10, 4, 20, OUTL); R(X + a + 1, Y + 10, 1, 20, '#8a8692'); }
  const gl = r7('#a8dcf4');
  R(X + 8, Y + 10, W - 16, 18, gl[2]); for (let i = 0; i < 16; i++) R(X + 10 + i * 2, Y + 26 - i, 2, 1, gl[5]);
  R(X + 16, Y + 20, W - 32, 4, OUTL); R(X + 17, Y + 20, W - 34, 2, '#8a8692');
  R(X + (W >> 1) - 6, Y - 4, 12, 8, OUTL); R(X + (W >> 1) - 5, Y - 3, 10, 6, '#f2c14e');
};

/* ---------- landmarks, third batch: around the rest of the city ---------- */
function domeArc(cx, cy, r, ramp) {   // a dome: shaded in bands, lit toward the top left
  for (let y = -r; y <= 0; y++) {
    const w = Math.round(Math.sqrt(r * r - y * y));
    for (let x = -w; x <= w; x++) {
      const t = (x + r * .35) / r * .8 + (y / r) * .6, k = t < -.55 ? 6 : t < -.25 ? 5 : t < .1 ? 4 : t < .4 ? 3 : t < .7 ? 2 : 1;
      R(cx + x, cy + y, 1, 1, Math.abs(x) >= w - 1 || y == -r ? OUTL : ramp[k]);
    }
  }
}
function colonnade(X, Y, W, H, step, c) {
  const r = r7(c);
  R(X, Y, W, H, r[0]); R(X, Y, W, 4, '#100c18');
  for (let i = 6; i < W - 8; i += step) column(X + i, Y, Math.max(8, step - 12), H, c);
}
LM32.scimuseum = (X, Y, W, H, fr) => {   // the old Broad Street Station: a green dome over a columned front
  const cx = X + (W >> 1), st = r7('#e4dcc8');
  R(X, Y + 36, W, H - 36, OUTL); ashlar(X + 2, Y + 38, W - 4, H - 40, '#e4dcc8', 81);
  domeArc(cx, Y + 46, 60, r7('#7a9aa8'));
  R(cx - 3, Y - 22, 6, 12, OUTL); R(cx - 1, Y - 20, 2, 8, '#f2c14e');
  cornice(X + 2, Y + 46, W - 4, '#e4dcc8', 10);
  colonnade(X + 14, Y + 56, W - 28, H - 70, 28, '#f4f0e4');
  R(cx - 44, Y + 60, 88, 12, OUTL); pxText('SCIENCE', cx, Y + 62, '#f2c14e', 2);
  steps(X + 2, Y + H - 12, W - 4, 12, st[3]);
};
LM32.vmhc = (X, Y, W, H) => {   // the history museum: a low pediment and a long colonnade
  const st = r7('#d8d2c4');
  for (let i = 0; i < 16; i++) { const ww = Math.round(W * (i + 1) / 16), a = X + ((W - ww) >> 1); R(a, Y + i, ww, 1, OUTL); if (ww > 4) R(a + 2, Y + i, ww - 4, 1, i < 3 ? st[6] : st[4]); }
  R(X, Y + 12, W, H - 12, OUTL); cornice(X + 2, Y + 14, W - 4, '#ece6d8', 10);
  colonnade(X + 10, Y + 24, W - 20, H - 36, 26, '#f8f4ea');
  steps(X + 2, Y + H - 12, W - 4, 12, st[3]);
};
LM32.sauers = (X, Y, W, H, fr) => {   // the Sauer's vanilla sign, lit, over the brick works
  bricks(X, Y + 60, W, H - 60, '#a85040', 91);
  R(X, Y + 60, W, 6, '#e8e4dc'); R(X, Y + 60, W, 1, '#ffffff'); R(X, Y + 65, W, 1, '#a8a49c');
  for (let i = 16; i < W - 20; i += 32) win32(X + i, Y + 80, 16, 20, false, '#e8dcc0');
  R(X + 12, Y + 56, 4, 12, OUTL); R(X + W - 16, Y + 56, 4, 12, OUTL);
  const on = (fr >> 1) & 1;
  R(X + 8, Y + 4, W - 16, 52, OUTL); R(X + 10, Y + 6, W - 20, 48, '#2a1830'); R(X + 10, Y + 6, W - 20, 2, '#4a3050');
  pxText("SAUER'S", X + (W >> 1), Y + 10, on ? '#f8e060' : '#c89a30', 4);
  pxText('VANILLA', X + (W >> 1), Y + 34, on ? '#ff9a60' : '#b86a40', 2);
  const bt = r7(on ? '#c87a30' : '#8a5a24');
  R(X + W - 40, Y + 12, 12, 28, OUTL); R(X + W - 38, Y + 16, 8, 22, bt[3]); R(X + W - 38, Y + 16, 2, 22, bt[5]); R(X + W - 36, Y + 12, 4, 4, '#f4f4f8');
};
LM32.ashecenter = (X, Y, W, H) => {   // tennis courts: blue hard court, white lines, the net's shadow
  R(X, Y, W, H, OUTL);
  const bl = r7('#3a7ab0'), cr = r7('#3a8ac8');
  R(X + 2, Y + 2, W - 4, H - 4, bl[3]); for (let i = 0; i < 40; i++) { const q = HH(i, 3, 5); R(X + 4 + q % (W - 8), Y + 4 + (q >> 8) % (H - 8), 2, 1, bl[2]); }
  R(X + 10, Y + 10, W - 20, H - 20, '#e8ecf0'); R(X + 12, Y + 12, W - 24, H - 24, cr[3]);
  R(X + (W >> 1), Y + 12, 2, H - 24, '#e8ecf0'); R(X + 12, Y + (H >> 1), W - 24, 2, OUTL); R(X + 12, Y + (H >> 1) + 2, W - 24, 2, cr[1]);
  for (let i = X + 4; i < X + W - 4; i += 8) R(i, Y, 2, 6, '#5a5662');
  R(X + (W >> 1) - 32, Y + H - 20, 64, 14, OUTL); pxText('ASHE', X + (W >> 1), Y + H - 18, '#f2c14e', 2);
};
LM32.speedway = (X, Y, W, H, fr) => {   // the Raceway: grandstands, the asphalt oval, the infield, cars going round
  R(X, Y, W, H, OUTL); R(X + 2, Y + 2, W - 4, H - 4, '#58a848');
  for (let i = 0; i < 60; i++) { const q = HH(i, 9, 3); R(X + 4 + q % (W - 8), Y + 40 + (q >> 8) % (H - 44), 3, 1, '#4a9a40'); }
  const cx = X + (W >> 1), cy = Y + (H >> 1) + 12;
  g.fillStyle = '#1a1423'; g.beginPath(); g.ellipse(cx, cy, (W >> 1) - 14, (H >> 1) - 18, 0, 0, 6.283); g.fill();
  g.fillStyle = '#5a5662'; g.beginPath(); g.ellipse(cx, cy, (W >> 1) - 16, (H >> 1) - 20, 0, 0, 6.283); g.fill();
  g.fillStyle = '#6a6672'; g.beginPath(); g.ellipse(cx, cy - 2, (W >> 1) - 24, (H >> 1) - 28, 0, 0, 6.283); g.fill();
  g.fillStyle = '#ffffff'; g.beginPath(); g.ellipse(cx, cy, (W >> 1) - 66, (H >> 1) - 54, 0, 0, 6.283); g.fill();
  g.fillStyle = '#58a848'; g.beginPath(); g.ellipse(cx, cy, (W >> 1) - 68, (H >> 1) - 56, 0, 0, 6.283); g.fill();
  for (let i = 0; i < 8; i++) R(cx - 2, Y + H - 58 + i * 4, 4, 2, i & 1 ? OUTL : '#ffffff');
  for (let r = 0; r < 3; r++) { R(X + 8, Y + 8 + r * 12, W - 16, 8, r & 1 ? '#c8ccd4' : '#e8e4dc'); R(X + 8, Y + 14 + r * 12, W - 16, 2, '#8a8c94'); }
  for (let i = 0; i < 70; i++) { const q = HH(i, fr >> 1, 9); R(X + 10 + q % (W - 20), Y + 9 + ((q >> 8) % 3) * 12, 3, 4, ['#e83048', '#f2c14e', '#3d63c8', '#ffffff'][q % 4]); }
  for (let k = 0; k < 3; k++) {
    const a = fr * .35 + k * 2.1, ex = cx + Math.cos(a) * ((W >> 1) - 42), ey = cy + Math.sin(a) * ((H >> 1) - 38), c = r7(['#e83048', '#3d63c8', '#f2c14e'][k]);
    R(ex - 7, ey - 4, 14, 8, OUTL); R(ex - 6, ey - 3, 12, 6, c[3]); R(ex - 6, ey - 3, 12, 2, c[5]); R(ex - 2, ey - 2, 4, 3, '#28406a');
  }
};
LM32.agecroft = (X, Y, W, H, fr) => {   // Agecroft Hall: Tudor half-timbering, gables, leaded casements
  R(X, Y + 24, W, H - 24, OUTL);
  const pl = r7('#efe4c8'), tm = r7('#4a3424');
  R(X + 2, Y + 26, W - 4, H - 28, pl[3]); for (let i = 0; i < 60; i++) { const q = HH(i, 4, 7); R(X + 4 + q % (W - 8), Y + 28 + (q >> 8) % (H - 32), 2, 1, pl[2]); }
  for (let i = X + 4; i < X + W - 4; i += 14) { R(i, Y + 26, 4, H - 28, tm[3]); R(i, Y + 26, 1, H - 28, tm[5]); }
  for (let r = Y + 40; r < Y + H; r += 20) { R(X + 2, r, W - 4, 4, tm[3]); R(X + 2, r, W - 4, 1, tm[5]); }
  for (let i = X + 8; i < X + W - 20; i += 28) { R(i + 2, Y + 48, 10, 10, tm[2]); for (let k = 0; k < 10; k++) R(i + 2 + k, Y + 48 + k, 1, 1, tm[4]); }   // diagonal braces
  for (let k = 0; k < 3; k++) {
    const gx = X + 16 + k * ((W - 32) >> 1), rf = r7('#6a3a2a');
    for (let i = 0; i < 24; i++) { const ww = Math.max(4, i * 2 + 2); R(gx - (ww >> 1), Y + i + 4, ww, 1, i == 23 ? OUTL : (i & 3) == 3 ? rf[1] : rf[3]); R(gx - (ww >> 1), Y + i + 4, 2, 1, OUTL); }
  }
  for (let i = X + 12; i < X + W - 24; i += 52) {
    R(i, Y + 52, 28, 18, OUTL);
    for (let j = 0; j < 3; j++) { const lit = (fr >> 1) & 1 && j == 1; R(i + 2 + j * 9, Y + 54, 7, 14, lit ? '#f8d868' : '#a8c0c8'); for (let k = 0; k < 14; k += 4) R(i + 2 + j * 9, Y + 54 + k, 7, 1, '#5a6a70'); }
  }
};
LM32.siegel = (X, Y, W, H, fr) => {   // the Siegel Center: a dark arena under a domed roof, a gold band, RAMS
  R(X, Y + 16, W, H - 16, OUTL);
  const d = r7('#3a3a44');
  R(X + 2, Y + 18, W - 4, H - 20, d[3]); for (let i = X + 4; i < X + W - 4; i += 6) R(i, Y + 26, 2, H - 30, d[2]);
  R(X + 2, Y + 18, W - 4, 8, '#f2c14e'); R(X + 2, Y + 18, W - 4, 2, '#fff2b0'); R(X + 2, Y + 25, W - 4, 1, '#a87820');
  g.fillStyle = OUTL; g.beginPath(); g.ellipse(X + (W >> 1), Y + 20, (W >> 1) - 4, 18, 0, Math.PI, 0); g.fill();
  g.fillStyle = '#5a5a66'; g.beginPath(); g.ellipse(X + (W >> 1), Y + 20, (W >> 1) - 8, 14, 0, Math.PI, 0); g.fill();
  g.fillStyle = '#7a7a86'; g.beginPath(); g.ellipse(X + (W >> 1) - 20, Y + 16, (W >> 1) - 60, 8, 0, Math.PI, 0); g.fill();
  for (let i = X + 16; i < X + W - 16; i += 28) win32(i, Y + 36, 16, 20, false, '#5a5a66');
  R(X + (W >> 1) - 48, Y + 60, 96, 28, OUTL); pxText('RAMS', X + (W >> 1), Y + 64, (fr >> 1) & 1 ? '#f2c14e' : '#ffffff', 4);
};
LM32.marshall = (X, Y, W, H) => {   // the John Marshall House: brick, a slate roof, chimneys, sash windows
  bricks(X, Y + 16, W, H - 16, '#a85840', 101);
  R(X - 2, Y + 4, W + 4, 14, OUTL); roofBand(X, Y + 6, W, 10);
  for (const a of [12, W - 20]) { R(X + a, Y - 6, 10, 14, OUTL); bricks(X + a + 2, Y - 4, 6, 12, '#a85840', 102); }
  for (let i = 16; i < W - 20; i += 40) { win32(X + i, Y + 28, 18, 22, false, '#efe4c8'); win32(X + i, Y + 72, 18, 22, false, '#efe4c8'); }
};
function roofBand(X, Y, W, H) { const s = r7('#4a4458'); R(X, Y, W, H, s[3]); R(X, Y, W, 2, s[5]); for (let i = 3; i < W; i += 8) R(X + i, Y + 2, 1, H - 2, s[2]); R(X, Y + H - 1, W, 1, s[0]); }
LM32.sallybell = (X, Y, W, H, fr) => {   // Sally Bell's Kitchen: cream brick, the red-and-white awning, the sign, cake boxes
  bricks(X, Y, W, H, '#e0d4b8', 111);
  R(X + 6, Y + 6, W - 12, 24, OUTL); R(X + 8, Y + 8, W - 16, 20, '#3a2840'); pxText('SALLY BELL', X + (W >> 1), Y + 10, '#fff2a8', 2);
  for (let i = 2; i < W - 2; i += 8) { const red = (i >> 3) & 1; R(X + i, Y + 36, 8, 12, red ? '#d87878' : '#f4f0e4'); R(X + i, Y + 36, 8, 2, red ? '#f0a0a0' : '#ffffff'); R(X + i + 1, Y + 48, 6, 2, red ? '#a84848' : '#c8c4bc'); }
  R(X + 2, Y + 50, W - 4, 2, '#100c18');
  const gl = r7('#28406a');
  R(X + 16, Y + H - 48, W - 32, 44, OUTL); R(X + 18, Y + H - 46, W - 36, 42, gl[2]); R(X + 18, Y + H - 46, W - 36, 3, gl[4]);
  for (let i = 0; i < 3; i++) { const bx = X + 26 + i * 26; R(bx, Y + H - 32, 18, 12, '#f4f0e4'); R(bx, Y + H - 32, 18, 2, '#ffffff'); R(bx, Y + H - 26, 18, 2, '#d87878'); R(bx + 8, Y + H - 32, 2, 12, '#d87878'); }
};
LM32.armory = (X, Y, W, H, fr) => {   // the old armory: a brick fortress, crenellations, a gate tower
  bricks(X, Y + 16, W, H - 16, '#a85040', 121);
  for (let i = 0; i < W; i += 24) { R(X + i, Y + 4, 16, 14, OUTL); bricks(X + i + 2, Y + 6, 12, 12, '#a85040', 122 + i); }
  const tx = X + (W >> 1) - 20;
  R(tx, Y - 12, 40, 32, OUTL); bricks(tx + 2, Y - 10, 36, 30, '#a85040', 130);
  for (let i = 0; i < 36; i += 12) { R(tx + 2 + i, Y - 18, 8, 8, OUTL); R(tx + 3 + i, Y - 17, 6, 7, '#a85040'); }
  for (let i = 16; i < W - 24; i += 60) win32(X + i, Y + 44, 16, 28, (fr >> 1) & 1, '#c8b898');
};
LM32.stage = (X, Y, W, H) => {   // the Brown's Island stage: a timber deck, black truss towers, the backdrop
  R(X, Y, W, H, OUTL);
  const wd = r7('#6a4a30');
  R(X + 2, Y + H - 28, W - 4, 26, wd[3]); for (let i = 0; i < 26; i += 6) { R(X + 2, Y + H - 28 + i, W - 4, 1, wd[5]); R(X + 2, Y + H - 23 + i, W - 4, 1, wd[1]); }
  for (const a of [4, W - 12]) { R(X + a, Y + 2, 8, H - 30, '#3c3a44'); for (let j = 4; j < H - 34; j += 8) { R(X + a + 1, Y + 2 + j, 6, 1, '#6a6a76'); R(X + a + 1 + ((j >> 3) & 1) * 5, Y + 2 + j, 1, 8, '#6a6a76'); } }
  R(X + 4, Y + 2, W - 8, 8, '#3c3a44');
  R(X + 16, Y + 10, W - 32, H - 40, '#14101e'); for (let i = 0; i < 24; i++) { const q = HH(i, 1, 3); R(X + 18 + q % (W - 36), Y + 12 + (q >> 8) % (H - 44), 1, 1, '#3a3060'); }
  pxText("BROWN'S ISLAND", X + (W >> 1), Y + H - 20, '#f2c14e', 2);
};
LM32.ruins = (X, Y, W, H) => {   // the Belle Isle ironworks' ruins: broken brick walls, arched openings, ivy
  R(X, Y + 16, W, H - 16, OUTL); bricks(X + 2, Y + 18, W - 4, H - 18, '#8a6a5a', 141);
  for (let i = 0; i < W; i += 20) { const hh = 8 + (i * 7) % 24; R(X + i, Y + 16 - hh, 16, hh + 2, OUTL); bricks(X + i + 2, Y + 18 - hh, 12, hh, '#8a6a5a', 142 + i); }
  for (let i = 24; i < W - 24; i += 44) { R(X + i, Y + 36, 24, H - 36, OUTL); R(X + i + 2, Y + 32, 20, 4, OUTL); R(X + i + 2, Y + 38, 20, H - 38, '#100c18'); }
  for (const [a, b] of [[12, 24], [W - 24, 20]]) shrub(X + a - 4, Y + b, 14, 22, a, r7('#2f7a3c'));
};
LM32.steamboat = (X, Y, W, H, fr) => {   // the river steamboat: white decks, the red paddlewheel turning, stacks
  R(X, Y + 28, W, H - 28, OUTL); R(X + 2, Y + 30, W - 4, H - 34, '#f4f2ec'); R(X + 2, Y + 30, W - 4, 3, '#ffffff'); R(X + 2, Y + H - 8, W - 4, 4, '#e83048');
  R(X + 12, Y + 8, W - 40, 22, OUTL); R(X + 14, Y + 10, W - 44, 18, '#e8e4dc'); R(X + 14, Y + 10, W - 44, 2, '#ffffff');
  for (let i = 20; i < W - 40; i += 16) win32(X + i, Y + 14, 8, 8, false, '#e8e4dc');
  const rd = r7('#8a1424'), px0 = X + W - 32;
  R(px0, Y + 20, 28, H - 24, OUTL); R(px0 + 2, Y + 22, 24, H - 28, rd[3]);
  for (let i = 0; i < 4; i++) { const yy = Y + 24 + ((i * 8 + fr * 4) % 24); R(px0 + 2, yy, 24, 2, '#e83048'); R(px0 + 2, yy + 2, 24, 1, rd[1]); }
  for (const a of [24, 40]) { R(X + a, Y - 16, 8, 26, OUTL); R(X + a + 2, Y - 14, 4, 24, '#3a3a44'); R(X + a, Y - 16, 8, 4, '#f2c14e'); }
  R(X + 26 + (fr & 1) * 2, Y - 24, 6, 6, '#c8c8d0'); R(X + 28 + (fr & 1) * 2, Y - 30, 4, 4, '#e8e8f0');
};
LM32.stonehouse = (X, Y, W, H, fr) => {   // Forest Hill's stone house: fieldstone, a slate roof, a cupola, a porch
  R(X, Y + 20, W, H - 20, OUTL);
  const fs = r7('#9a9aa0'); R(X + 2, Y + 22, W - 4, H - 24, fs[2]);
  for (let r = Y + 24, row = 0; r < Y + H - 2; r += 10, row++) for (let i = X + 2 + (row & 1) * 8, k = 0; i < X + W - 4; i += 18, k++) {
    const q = HH(row, k, 7) % 3, w = Math.min(14 + q * 2, X + W - 2 - i); if (w < 3) continue;
    R(i, r, w, 8, fs[3 + (q == 0 ? 1 : 0)]); R(i, r, w, 1, fs[5]); R(i, r + 7, w, 1, fs[1]);
  }
  R(X - 4, Y + 8, W + 8, 16, OUTL); roofBand(X, Y + 10, W, 12);
  R(X + (W >> 1) - 8, Y - 12, 16, 22, OUTL); R(X + (W >> 1) - 6, Y - 10, 12, 18, '#e8e4dc'); R(X + (W >> 1) - 2, Y - 4, 4, 6, '#c08840');
  const wd = r7('#6a4a30'); R(X + 8, Y + H - 32, W - 16, 6, wd[3]); R(X + 8, Y + H - 32, W - 16, 2, wd[5]);
  for (let i = 16; i < W - 16; i += 40) { R(X + i, Y + H - 28, 4, 26, '#e8e4dc'); R(X + i + 3, Y + H - 28, 1, 26, '#a8a49c'); }
  for (let i = 24; i < W - 24; i += 52) win32(X + i, Y + 36, 16, 20, (fr >> 1) & 1, '#e8e4dc');
};
LM32.trolley = (X, Y, W, H) => {   // a streetcar on display: green and gold, a row of windows, the pole
  R(X, Y + 2, W, H - 4, OUTL);
  const gr = r7('#2e7d4f');
  R(X + 2, Y + 4, W - 4, H - 10, gr[3]); R(X + 2, Y + 4, W - 4, 4, '#f2c14e'); R(X + 2, Y + H - 10, W - 4, 2, gr[1]);
  for (let i = 8; i < W - 8; i += 14) { R(X + i, Y + 10, 10, 8, '#5a82b8'); R(X + i, Y + 10, 10, 2, '#a8c8e8'); R(X + i + 7, Y + 10, 3, 8, '#3a5a90'); }
  for (const a of [8, W - 16]) { R(X + a, Y + H - 6, 8, 6, OUTL); R(X + a + 2, Y + H - 5, 4, 3, '#5a5662'); }
  R(X + (W >> 1), Y - 8, 2, 12, OUTL);
};
