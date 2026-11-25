// Runs the browser game's own drawing code headlessly and exports every map as a PNG
// plus its tile grid, so the SNES art pipeline starts from exactly what the web game draws.
// Usage: node export.js <path-to-river-city-quest> <out-dir>
const fs = require('fs'), vm = require('vm'), path = require('path');
const { createCanvas } = require('@napi-rs/canvas');

const ROOT = path.resolve(process.argv[2] || '../../../src/river-city-quest');
const OUT = path.resolve(process.argv[3] || './out');
fs.mkdirSync(path.join(OUT, 'maps'), { recursive: true });

const html = fs.readFileSync(path.join(ROOT, 'index.html'), 'utf8');
const files = [...html.matchAll(/<script src="([^"]+)"><\/script>/g)].map(m => m[1]);

const el = (tag) => {
  if (tag === 'canvas' || tag === undefined) {
    const c = createCanvas(1, 1);
    c.addEventListener = () => {}; c.style = {}; c.classList = { add() {}, remove() {} };
    c.getBoundingClientRect = () => ({ left: 0, top: 0, width: 100, height: 100 });
    c.setPointerCapture = () => {}; c.setAttribute = () => {};
    return c;
  }
  return { style: {}, classList: { add() {}, remove() {} }, addEventListener() {}, setAttribute() {}, getBoundingClientRect: () => ({ left: 0, top: 0, width: 100, height: 100 }) };
};
const sb = {
  document: { getElementById: () => el('canvas'), createElement: el, fonts: null, addEventListener() {}, hidden: false },
  addEventListener() {}, requestAnimationFrame() {}, setInterval() {}, setTimeout() {},
  Math, console, JSON, Set, Map, Object, Array, String, Number, Error, performance,
  navigator: {}, location: { protocol: 'file:', hostname: '' }, localStorage: { getItem: () => null, setItem() {} },
};
sb.window = sb; vm.createContext(sb);
const VARIANTS = +(process.env.VARIANTS || 0);
for (const f of files) {
  let src = fs.readFileSync(path.join(ROOT, f), 'utf8');
  // SNES mode: limit per-cell speckle to a few variants so tiles repeat and fit VRAM.
  if (VARIANTS && f.endsWith('art.js')) src = src.replace('h=HH(tx,ty,s.seed);', 'h=HH(HH(tx,ty,s.seed)%' + VARIANTS + ',7,7);');
  vm.runInContext(src, sb, { filename: f });
}

// Top-level consts (SC, TS, g...) live in the context's shared script scope.
const dump = vm.runInContext(`(function(){
  const out=[];
  for(const id in SC){
    const s=SC[id];
    try{ if(typeof applyDyn==='function') applyDyn(s); }catch(e){}
    s.torches=[];
    let c; try{ c=renderMap(s,0); const old=g; g=c.getContext('2d');
      try{drawLandmarks(s,0);}catch(e){} try{drawDecor(s);}catch(e){} g=old; }
    catch(e){ out.push({id,err:String(e)}); continue; }
    out.push({id,name:s.name||'',area:s.area||'',ow:!!s.ow,w:s.w,h:s.h,ax:s.ax,ay:s.ay,
      grid:s.g.map(r=>Array.isArray(r)?r.join(''):r),canvas:c});
  }
  return out;
})()`, sb);

const index = [];
for (const m of dump) {
  if (m.err) { console.log('skip', m.id, m.err); continue; }
  const safe = m.id.replace(/[^a-z0-9_-]+/gi, '_');
  fs.writeFileSync(path.join(OUT, 'maps', safe + '.png'), m.canvas.toBuffer('image/png'));
  const { canvas, ...meta } = m;
  index.push({ ...meta, file: 'maps/' + safe + '.png' });
}
fs.writeFileSync(path.join(OUT, 'maps.json'), JSON.stringify(index, null, 1));

// Collision characters, straight from the game.
const SOLID = vm.runInContext('SOLID', sb);
fs.writeFileSync(path.join(OUT, 'solid.json'), JSON.stringify({ solid: SOLID + '#' }));

// Hero walk frames: down, up, side (faces right) x 2 steps, in a 16px strip.
const hero = vm.runInContext('[HERO.down[0],HERO.down[1],HERO.up[0],HERO.up[1],HERO.side[0],HERO.side[1]].map(f=>f.r.c)', sb);
const strip = createCanvas(16 * hero.length, 16), hx = strip.getContext('2d');
hero.forEach((c, i) => hx.drawImage(c, i * 16, 16 - c.height));
fs.writeFileSync(path.join(OUT, 'hero.png'), strip.toBuffer('image/png'));
console.log('exported', index.length, 'maps of', dump.length);
