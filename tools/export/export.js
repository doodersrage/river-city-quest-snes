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
// Scenes for the SNES port so far: the outdoor maps plus every single-room area (interiors, caves).
vm.runInContext(`var SNES_AREAN={};for(const id in SC){const a=SC[id].area;SNES_AREAN[a]=(SNES_AREAN[a]||0)+1;}
  function portScene(s){return s.ow||SNES_AREAN[s.area]==1;}`, sb);
const dump = vm.runInContext(`(function(){
  st=newState();p=newPlayer();   // a fresh game: chests closed, rubble in place
  const out=[];
  for(const id in SC){
    const s=SC[id];
    try{ if(typeof applyDyn==='function') applyDyn(s); }catch(e){}
    s.torches=[];
    let c; try{ c=renderMap(s,0); const old=g; g=c.getContext('2d');
      try{drawLandmarks(s,0);}catch(e){} try{drawDecor(s);}catch(e){} g=old; }
    catch(e){ out.push({id,err:String(e)}); continue; }
    // Same map with every chest open, for the tiles the ROM swaps in when you open one.
    // ...and with every bush cut (bushes become the map's base ground, as cutAt() does).
    let opened=null;const bushes=[];
    for(let y=0;y<s.h;y++)for(let x=0;x<s.w;x++)if(s.g[y][x]=='b')bushes.push([x,y]);
    if((s.chests||[]).length||bushes.length){const keep=s.chests.map(q=>s.g[q.ty][q.tx]);s.chests.forEach(q=>s.g[q.ty][q.tx]='o');
      bushes.forEach(([x,y])=>s.g[y][x]=s.base);
      opened=renderMap(s,0);const o2=g;g=opened.getContext('2d');try{drawLandmarks(s,0);}catch(e){}try{drawDecor(s);}catch(e){}g=o2;
      s.chests.forEach((q,i)=>s.g[q.ty][q.tx]=keep[i]);bushes.forEach(([x,y])=>s.g[y][x]='b');}
    out.push({id,name:s.name||'',area:s.area||'',ow:!!s.ow,port:portScene(s),w:s.w,h:s.h,ax:s.ax,ay:s.ay,
      grid:s.g.map(r=>Array.isArray(r)?r.join(''):r),canvas:c,opened,
      doors:(s.doors||[]).map(d=>({tx:d.tx,ty:d.ty,to:d.to,need:d.need||null,ret:d.ret})),
      chests:(s.chests||[]).map(q=>({id:q.id,tx:q.tx,ty:q.ty,item:q.item})),bushes});
  }
  return out;
})()`, sb);

const index = [];
for (const m of dump) {
  if (m.err) { console.log('skip', m.id, m.err); continue; }
  const safe = m.id.replace(/[^a-z0-9_-]+/gi, '_');
  fs.writeFileSync(path.join(OUT, 'maps', safe + '.png'), m.canvas.toBuffer('image/png'));
  if (m.opened) fs.writeFileSync(path.join(OUT, 'maps', safe + '.open.png'), m.opened.toBuffer('image/png'));
  const { canvas, opened, ...meta } = m;
  index.push({ ...meta, file: 'maps/' + safe + '.png', openFile: opened ? 'maps/' + safe + '.open.png' : null });
}
fs.writeFileSync(path.join(OUT, 'maps.json'), JSON.stringify(index, null, 1));

// Collision characters, straight from the game.
const SOLID = vm.runInContext('SOLID', sb);
fs.writeFileSync(path.join(OUT, 'solid.json'), JSON.stringify({ solid: SOLID + '#' }));

// Hero walk frames: down, up, side (faces right) x 2 steps, in a 16px strip.
const hero = vm.runInContext('[HERO.down[0],HERO.down[1],HERO.up[0],HERO.up[1],HERO.side[0],HERO.side[1]].map(f=>f.r.c)', sb);
const strip = createCanvas(16 * hero.length, 16), hx = strip.getContext('2d');
// (hero strip continues below)
hero.forEach((c, i) => hx.drawImage(c, i * 16, 16 - c.height));
fs.writeFileSync(path.join(OUT, 'hero.png'), strip.toBuffer('image/png'));
console.log('exported', index.length, 'maps of', dump.length);

// ---------- Stage 2: things to read and people to talk to, per outdoor map ----------
fs.mkdirSync(path.join(OUT, 'npcs'), { recursive: true });
// Sound and pop-ups have no meaning here; silence them so talk functions run cleanly.
vm.runInContext('toast=()=>{};tune=()=>{};sfx=()=>{};var SAID=null;say=(l)=>{SAID=l;};p=newPlayer();', sb);
vm.runInContext('var DEBUG=' + (process.env.DEBUG ? 1 : 0), sb);
const world = vm.runInContext(`(function(){
  st=newState();
  const out={};
  const pages=r=>Array.isArray(r)?r.map(String):[String(r)];
  for(const id in SC){const s=SC[id];if(!portScene(s))continue;
    const reads=[];
    for(let ty=0;ty<s.h;ty++)for(let tx=0;tx<s.w;tx++){
      const t=s.g[ty][tx];let title=null,text=null;
      const lmk=t=='X'&&(s.lm||[]).find(L=>tx>=L.x&&tx<L.x+L.w&&ty>=L.y&&ty<L.y+L.h);
      const r=s.reads[tx+','+ty]||(s.readsT&&s.readsT[t])||(lmk&&lmk.read);
      if(r&&r[0]=='!'){const x=READX[r];if(x){title=x[0];text=[x[1]];}}
      else if(r&&LOREM[r]){title=LOREM[r].t;text=[LOREM[r].s];}
      else if(t=='e'){title='Mural';text=['A bright mural, painted right onto the wall.'];}
      else if(t=='l'){title='Gas Lamp';text=['A cold gas lamp. The lamplighter could use help.'];}
      if(text)reads.push({tx,ty,title,pages:text,lore:r&&r[0]!='!'?r:null});
    }
    const npcs=(s.npcs||[]).map(n0=>{
      const talks=[];const seen=new Set();
      if(n0.lines&&n0.talk=='folk'){for(const l of n0.lines)talks.push([String(l)]);}
      else{const n=Object.assign({},n0);for(let i=0;i<4;i++){let r;SAID=null;try{r=TALK[n.talk](n);if(!r)r=SAID;}catch(e){r=null;if(DEBUG)console.log(n.who,String(e));}
        if(!r)continue;const pg=pages(r),k=pg.join('|');if(!seen.has(k)){seen.add(k);talks.push(pg);}}}
      if(!talks.length&&n0.lines)for(const l of n0.lines)talks.push([String(l)]);
      if(!talks.length&&n0.talk=='shop')talks.push(['What can I get you?']);
      if(!talks.length&&n0.talk=='grocer')talks.push(['Fresh from the farms!']);
      if(!talks.length)talks.push(['...']);
      let sp;try{sp=n0.spr&&n0.spr.down?n0.spr:npcSpr(n0);}catch(e){sp=npcSpr(Object.assign({hat:['#888','#666','#aaa'],shirt:['#888','#666','#aaa']},n0));}
      return {tx:n0.tx,ty:n0.ty,who:n0.who||'',talks,shop:n0.talk=='shop'?'shop':n0.talk=='grocer'?'market':null,spr:[sp.down.r.c,sp.up.r.c,sp.side.r.c]};
    });
    out[id]={reads,npcs};
  }
  return out;
})()`, sb);
const worldOut = {};
for (const id in world) {
  const w = world[id], safe = id.replace(/[^a-z0-9_-]+/gi, '_');
  worldOut[id] = { reads: w.reads, npcs: w.npcs.map((n, k) => {
    const c = createCanvas(48, 16), x = c.getContext('2d');
    n.spr.forEach((f, i) => x.drawImage(f, i * 16 + Math.floor((16 - f.width) / 2), 16 - f.height));
    const file = 'npcs/' + safe + '_' + k + '.png';
    fs.writeFileSync(path.join(OUT, file), c.toBuffer('image/png'));
    return { tx: n.tx, ty: n.ty, who: n.who, talks: n.talks, shop: n.shop, file };
  }) };
}
fs.writeFileSync(path.join(OUT, 'world.json'), JSON.stringify(worldOut, null, 1));

// The game's display font (Press Start 2P, OFL), ASCII 32-127 as an 8x8 grid.
{
  const { GlobalFonts } = require('@napi-rs/canvas');
  GlobalFonts.registerFromPath(require.resolve('@fontsource/press-start-2p/files/press-start-2p-latin-400-normal.woff2'), 'PS2P');
  const c = createCanvas(128, 48), x = c.getContext('2d');
  x.fillStyle = '#000'; x.fillRect(0, 0, 128, 48); x.fillStyle = '#fff'; x.font = '8px PS2P'; x.textBaseline = 'top';
  for (let i = 0; i < 96; i++) x.fillText(String.fromCharCode(32 + i), (i % 16) * 8, Math.floor(i / 16) * 8);
  fs.writeFileSync(path.join(OUT, 'font.png'), c.toBuffer('image/png'));
}
console.log('world: reads', Object.values(worldOut).reduce((a, w) => a + w.reads.length, 0),
  'npcs', Object.values(worldOut).reduce((a, w) => a + w.npcs.length, 0));

// ---------- Stage 3: combat art and enemy placements ----------
fs.mkdirSync(path.join(OUT, 'combat'), { recursive: true });
const combat = vm.runInContext(`(function(){
  st=newState();p=newPlayer();
  const en={};for(const id in SC){const s=SC[id];if(portScene(s))en[id]=s.en.map(e=>e.slice());}
  // Enemy frames, facing right (the ROM flips them for left).
  const frames=[['rat',SPR.rat.r.c],['squirrel',SPR.squirrel.r.c],['goose',SPR.goose.r.c],['scooter',SPR.scooter.r.c],
    ['crab',SPR.crab.r.c],['bat0',SPR.bat[0].r.c],['bat1',SPR.bat[1].r.c],['ghost',SPR.ghost.r.c]];
  // The paddle at 16 angles, drawn by the game itself around a hero at (20,23) (paddle centre y-3 = 20).
  const paddles=[];
  for(let i=0;i<16;i++){const c=mkCanvas(40,40),old=g;g=c.getContext('2d');p.x=20;p.y=23;paddleAt(i*Math.PI/8,false);g=old;paddles.push(c);}
  return {en,frames,paddles,heart:HEART,heart0:HEART0,coins:[COIN[0].r.c,COIN[1].r.c]};
})()`, sb);
{
  const strip = createCanvas(16 * combat.frames.length, 16), x = strip.getContext('2d');
  combat.frames.forEach(([n, c], i) => x.drawImage(c, i * 16, 16 - c.height));
  fs.writeFileSync(path.join(OUT, 'combat', 'enemies.png'), strip.toBuffer('image/png'));
  const ps = createCanvas(40 * 16, 40), px = ps.getContext('2d');
  combat.paddles.forEach((c, i) => px.drawImage(c, i * 40, 0));
  fs.writeFileSync(path.join(OUT, 'combat', 'paddles.png'), ps.toBuffer('image/png'));
  const hud = createCanvas(9 * 4, 8), hx2 = hud.getContext('2d');
  [combat.heart, combat.heart0, combat.coins[0], combat.coins[1]].forEach((c, i) => hx2.drawImage(c, i * 9, 0));
  fs.writeFileSync(path.join(OUT, 'combat', 'hud.png'), hud.toBuffer('image/png'));
  fs.writeFileSync(path.join(OUT, 'combat', 'combat.json'), JSON.stringify({
    enemies: combat.en, frames: combat.frames.map(f => f[0]) }, null, 1));
  console.log('combat: enemies', Object.values(combat.en).reduce((a, e) => a + e.length, 0));
}

// ---------- Stage 4: what chests give, and the two shops ----------
const extra = vm.runInContext(`(function(){
  const gives={};
  for(const id in SC)for(const q of SC[id].chests||[]){if(gives[q.item])continue;
    st=newState();p=newPlayer();let L;try{L=give(q.item);}catch(e){L=['You found something!'];}
    gives[q.item]=(L||[]).map(String);}
  const shopOf=(list,title)=>({title,items:list.map(it=>{st=newState();p=newPlayer();st.items.basket=1;st.items.bottle=1;st.items.balls=1;st.items.powder=1;
    let msg='';try{const r=it.buy();msg=r?String(r):'';}catch(e){}
    st=newState();p=newPlayer();let why='';try{why=it.why();}catch(e){}
    return {name:it.name,price:it.price,msg,whyAtStart:why};})});
  return {gives,shops:{shop:shopOf(SHOP,'CARYTOWN GOODS'),market:shopOf(MARKET,'17TH STREET MARKET')}};
})()`, sb);
fs.writeFileSync(path.join(OUT, 'extra.json'), JSON.stringify(extra, null, 1));
console.log('chest items', Object.keys(extra.gives).join(' '));
