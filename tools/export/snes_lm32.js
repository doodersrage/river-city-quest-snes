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
