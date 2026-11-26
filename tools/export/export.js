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

// The SNES edition's own look (tools/export/snes_style.js): shaded ramps in place of the web game's flat tiles.
if (process.env.SNES_STYLE !== '0') vm.runInContext(fs.readFileSync(path.join(__dirname, 'snes_style.js'), 'utf8'), sb, { filename: 'snes_style.js' });

// Top-level consts (SC, TS, g...) live in the context's shared script scope.
// Scenes for the SNES port so far: the outdoor maps plus every single-room area (interiors, caves).
vm.runInContext(`var SNES_AREAN={};for(const id in SC){const a=SC[id].area;SNES_AREAN[a]=(SNES_AREAN[a]||0)+1;}
  var PORT_AREAS=${JSON.stringify((process.env.DUNGEONS || 'tredegar,quarry,pyramid,poe,carillon,conserv,tunnel').split(','))};
  function portScene(s){return s.ow||SNES_AREAN[s.area]==1||PORT_AREAS.includes(s.area);}`, sb);
vm.runInContext('shadow=()=>{};', sb);   // soft shadows don't survive palette reduction
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
    // Light sources for night and dark rooms (web drawNight/drawLight): wall torches, lit windows, street lamps; pixel centres
    const lights={torches:(s.torches||[]).slice(),wins:Object.values(s.wins||{}).filter(w=>w[2]).map(w=>[w[0]+8,w[1]+9]),
      lamps:(s.lamps||[]).map(([x,y])=>[x*16+8,y*16+4])};
    // Same map with every chest open, for the tiles the ROM swaps in when you open one.
    // Alternate renders for tiles the ROM swaps in at runtime:
    //  alt1: every locked door / gate / cracked wall open, every chest closed (including ones that
    //        appear when a room is solved), boss stairs, bushes cut;  alt2: the same with chests open.
    const bushes=[];
    for(let y=0;y<s.h;y++)for(let x=0;x<s.w;x++)if(s.g[y][x]=='b')bushes.push([x,y]);
    let opened=null,closed=null;
    // Sprouts (web refresh 'grow:' flags): watered, the sprout turns solid and vines bridge the gaps beside it.
    const sprouts=[];
    if(s.interior&&s.g0)s.g0.forEach((r,y)=>r.forEach((c,x)=>{if(c!='z')return;const flag='grow:'+s.id+':'+x+','+y,br=[];
      for(const [dx,dy] of [[1,0],[-1,0],[0,1],[0,-1]]){let X=x+dx,Y=y+dy;while(s.g0[Y]&&s.g0[Y][X]=='_'){br.push([X,Y]);X+=dx;Y+=dy;}}
      sprouts.push({cells:[[x,y]],closed:'z',open:'1',flag});if(br.length)sprouts.push({cells:br,closed:'_',open:'|',flag});}));
    const dynAll=s.dyn.concat(sprouts);
    const dynCells=[];for(const d of dynAll)for(const [x,y] of d.cells)dynCells.push([x,y,d.open]);
    if((s.chests||[]).length||bushes.length||dynCells.length||s.bossT){
      const keep=s.g.map(r=>r.slice());
      const paint=()=>{const c=renderMap(s,0),o2=g;g=c.getContext('2d');try{drawLandmarks(s,0);}catch(e){}try{drawDecor(s);}catch(e){}g=o2;return c;};
      bushes.forEach(([x,y])=>s.g[y][x]=s.base);dynCells.forEach(([x,y,o])=>s.g[y][x]=o);
      if(s.bossT){s.g[8][7]='E';s.g[8][8]='E';}
      (s.braz||[]).forEach(([x,y])=>s.g[y][x]='J');
      (s.chests||[]).forEach(q=>s.g[q.ty][q.tx]='c');closed=paint();
      (s.chests||[]).forEach(q=>s.g[q.ty][q.tx]='o');opened=paint();
      s.g=keep;}
    out.push({id,name:s.name||'',area:s.area||'',ow:!!s.ow,port:portScene(s),w:s.w,h:s.h,ax:s.ax,ay:s.ay,
      grid:s.g.map(r=>Array.isArray(r)?r.join(''):r),canvas:c,opened,closed,
      solve:s.solve||null,bossT:s.bossT||null,entry:!!s.entry,reward:s.reward||null,braz:s.braz||[],lights,lit:!!s.lit,cars:(s.cars||[]).map(c=>({y:c.y,dir:c.dir,kind:c.kind||'car'})),birds:s.birds||[],spots:s.spots||[],murals:s.murals||[],lamps:s.lamps||[],
      pickups:(s.items||[]).filter(it=>['skey','carkey','piece','bib','part'].includes(it.kind)).map(it=>({id:it.id,kind:it.kind,x:it.px||it.tx*16+8,y:it.py||it.ty*16+8})),
      blocks:(s.g0||[]).flatMap((r,y)=>r.map((c,x)=>c=='#'?[x,y]:null).filter(Boolean)),
      switches:(s.g0||[]).flatMap((r,y)=>r.map((c,x)=>c=='^'?[x,y]:null).filter(Boolean)),
      dyn:dynAll.map(d=>({cells:d.cells,closed:d.closed,open:d.open,flag:d.flag||null,solved:d.solved||null})),
      doors:(s.doors||[]).map(d=>({tx:d.tx,ty:d.ty,to:d.to,need:d.need||null,ret:d.ret})),
      chests:(s.chests||[]).map(q=>({id:q.id,tx:q.tx,ty:q.ty,item:q.item,appear:!!q.appear})),bushes});
  }
  return out;
})()`, sb);

const index = [];
for (const m of dump) {
  if (m.err) { console.log('skip', m.id, m.err); continue; }
  const safe = m.id.replace(/[^a-z0-9_-]+/gi, '_');
  fs.writeFileSync(path.join(OUT, 'maps', safe + '.png'), m.canvas.toBuffer('image/png'));
  if (m.opened) fs.writeFileSync(path.join(OUT, 'maps', safe + '.open.png'), m.opened.toBuffer('image/png'));
  if (m.closed) fs.writeFileSync(path.join(OUT, 'maps', safe + '.alt.png'), m.closed.toBuffer('image/png'));
  const { canvas, opened, closed, ...meta } = m;
  index.push({ ...meta, file: 'maps/' + safe + '.png', openFile: opened ? 'maps/' + safe + '.open.png' : null,
    altFile: closed ? 'maps/' + safe + '.alt.png' : null });
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
vm.runInContext('toast=()=>{};tune=()=>{};sfx=()=>{};var SAID=null;say=(l)=>{SAID=l;};p=newPlayer();offer=(n,pre)=>pre;', sb);
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
      const talks=[];const seen=new Set();const gifts=[];
      if(n0.lines&&n0.talk=='folk'){for(const l of n0.lines)talks.push([String(l)]);}
      else{const n=Object.assign({},n0);for(let i=0;i<4;i++){let r;SAID=null;const had=Object.assign({},st.items);try{r=TALK[n.talk](n);if(!r)r=SAID;}
        catch(e){r=null;if(DEBUG)console.log(n.who,String(e));}
        const got=Object.keys(st.items).filter(k=>st.items[k]&&!had[k]);
        if(r&&got.length){const pg=pages(r),k=pg.join('|');if(!seen.has(k)){seen.add(k);talks.push(pg);gifts.push([talks.length-1,got[0]]);}continue;}
        if(!r)continue;const pg=pages(r).filter(x=>x.length),k=pg.join('|');if(pg.length&&!seen.has(k)){seen.add(k);talks.push(pg);}}}
      if(!talks.length&&n0.lines)for(const l of n0.lines)talks.push([String(l)]);
      if(!talks.length&&n0.talk=='shop')talks.push(['What can I get you?']);
      if(!talks.length&&n0.talk=='grocer')talks.push(['Fresh from the farms!']);
      if(!talks.length&&!n0.chal)talks.push(['...']);   // challenge-givers who only offer their challenge say nothing else
      // Trading quest (melon -> sheet music -> river map -> kayak): talks that change what you carry.
      const quest=[];
      for(const need of [null,'melon','music','rivermap']){
        const keep=st;st=newState();st.trade=need;const n=Object.assign({},n0);let r=null;SAID=null;
        try{r=TALK[n.talk](n);if(!r)r=SAID;}catch(e){r=null;}
        const got=st.items.kayak?'kayak':st.trade;
        if(r&&got!==need)quest.push({need:need||'none',give:got,pages:pages(r)});
        st=keep;}
      // What someone says when you come back with the bottle empty or the balls nearly gone.
      let refill=null;
      for(const it of ['bottle','balls']){const keep=st;st=newState();st.items[it]=1;st.bottle=0;st.ammo.balls=0;
        const n=Object.assign({},n0);let r=null;SAID=null;try{r=TALK[n.talk](n);if(!r)r=SAID;}catch(e){r=null;}
        if(r&&(it=='bottle'?st.bottle:st.ammo.balls))refill={item:it,pages:pages(r)};st=keep;}
      // The race bib: the Runner trades his spare Running Shoes for it.
      {const keep=st;st=newState();st.got.bib=1;const n=Object.assign({},n0);let r=null;SAID=null;
        try{r=TALK[n.talk](n);if(!r)r=SAID;}catch(e){r=null;}
        if(r&&st.items.shoes)quest.push({need:'bib',give:'shoes',pages:pages(r)});st=keep;}
      // The frog count: the naturalist in Byrd Park, once you bring five frogs.
      {const keep=st;st=newState();st.frogs=5;const n=Object.assign({},n0);let r=null;SAID=null;
        try{r=TALK[n.talk](n);if(!r)r=SAID;}catch(e){r=null;}
        if(r&&st.f.frogsDone){const after=pages(TALK[n.talk](n));quest.push({need:'frogs',give:'maymont',pages:pages(r),after});}st=keep;}
      // The bateau: the river historian rebuilds it once you bring the oar, the pole and the tiller.
      {const keep=st;st=newState();st.got.oar=st.got.pole=st.got.tiller=1;const n=Object.assign({},n0);let r=null;SAID=null;
        try{r=TALK[n.talk](n);if(!r)r=SAID;}catch(e){r=null;}
        if(r&&st.f.bateau){const after=pages(TALK[n.talk](n));quest.push({need:'parts',give:'bateau',pages:pages(r),after});}st=keep;}
      let sp;try{sp=n0.spr&&n0.spr.down?n0.spr:npcSpr(n0);}catch(e){sp=npcSpr(Object.assign({hat:['#888','#666','#aaa'],shirt:['#888','#666','#aaa']},n0));}
      // A minigame this person runs, and how they offer it (with the gift given and the challenge won).
      const MG={ballplayer:'bat',hoops:'hoops',tennis:'tennis',racer:'derby',cyclist:'trail',racedir:'race'}[n0.talk]||null;let mtext=null;
      if(MG){const keep=st;st=newState();st.items.bike=st.items.balls=1;st.ammo.balls=st.cap.balls;st.pins.flies=1;const n=Object.assign({},n0);SAID=null;let r=null;
        try{r=TALK[n.talk](n);if(!r)r=SAID;}catch(e){r=null;}st=keep;mtext=r?pages(r):null;}
      return {tx:n0.tx,ty:n0.ty,who:n0.who||'',wander:!!n0.wander,chal:n0.chal||null,mini:MG,miniText:mtext,talks,gifts,quest,refill,shop:n0.talk=='shop'?'shop':n0.talk=='grocer'?'market':null,spr:[sp.down.r.c,sp.up.r.c,sp.side.r.c]};
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
    return { tx: n.tx, ty: n.ty, who: n.who, wander: n.wander, chal: n.chal, mini: n.mini, miniText: n.miniText, talks: n.talks, gifts: n.gifts, quest: n.quest, refill: n.refill, shop: n.shop, file };
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
    st=newState();p=newPlayer();cur=SC[id];let L;try{L=give(q.item);}catch(e){L=['You found something!'];}
    gives[q.item]=(L||[]).map(String);}
  const shopOf=(list,title)=>({title,items:list.map(it=>{st=newState();p=newPlayer();st.items.basket=1;st.items.bottle=1;st.items.balls=1;st.items.powder=1;
    let msg='';try{const r=it.buy();msg=r?String(r):'';}catch(e){}
    st=newState();p=newPlayer();let why='';try{why=it.why();}catch(e){}
    return {name:it.name,price:it.price,msg,whyAtStart:why};})});
  return {gives,shops:{shop:shopOf(SHOP,'CARYTOWN GOODS'),market:shopOf(MARKET,'17TH STREET MARKET')}};
})()`, sb);
fs.writeFileSync(path.join(OUT, 'extra.json'), JSON.stringify(extra, null, 1));
// Music and jingles (web music.js SONGS, core.js JING/SECRET, extras.js FANFARE) for tools/mkaudio.py.
fs.writeFileSync(path.join(OUT, 'music.json'), JSON.stringify(vm.runInContext(`({songs:Object.fromEntries(Object.entries(SONGS).map(([k,s])=>[k,{bpm:s.bpm,lt:s.lt||'square',lv:s.lv||.022,lead:s.lead,bass:s.bass,drum:s.drum}])),
  jing:JING,secret:SECRET,fanfare:FANFARE})`, sb), null, 1));
console.log('chest items', Object.keys(extra.gives).join(' '));

// ---------- Stage 5: bosses, projectiles, the hook ----------
const bossArt = vm.runInContext(`(function(){
  const out={};
  // The Iron Foreman: walk (2 steps), wind-up, stunned (2 blinks). 48x48, centre (24,40).
  const fr=[];
  const draw=(state,f)=>{const c=mkCanvas(48,48),old=g;g=c.getContext('2d');frame=f;
    const e={x:24,y:40,state,t:1,hp:9,vx:0,f:1};drawForeman(e,false);g=old;return c;};
  fr.push(draw('walk',0),draw('walk',8),draw('wind',2),draw('stun',4));  // one stun frame: sprite space is tight
  out.foreman=fr;
  // Old Snapper: walk (2 steps, facing right), stunned (2 wiggles). Same 48x48 box.
  const tdraw=(state,f)=>{const c=mkCanvas(48,48),old=g;g=c.getContext('2d');frame=f;
    const e={x:24,y:40,state,t:1,hp:8,vx:1,f:1};drawTurtle(e,false);g=old;return c;};
  out.turtle=[tdraw('walk',0),tdraw('walk',8),tdraw('stun',0),tdraw('stun',4)];
  // The Wraith: solid (visible, can be hit) and faint (its usual state), 2 tendril frames each.
  const wdraw=(vis,f)=>{const c=mkCanvas(48,48),old=g;g=c.getContext('2d');frame=f;
    const e={x:24,y:36,vis:vis?100:0,t:1,hp:6,f:1};const ga=Object.getOwnPropertyDescriptor(Object.getPrototypeOf(g),'globalAlpha');
    drawWraith(e,false);g=old;return c;};
  out.wraith=[wdraw(true,0),wdraw(true,8),wdraw(false,0),wdraw(false,8)];
  // The Raven: body and two wing beats, facing right (web drawEnemy 'raven').
  // The Bronze Bell, on the ground (the ROM lifts it by its height): hanging, and stunned.
  out.bell=['float','stun'].map(stt=>{const c=mkCanvas(48,48),old=g;g=c.getContext('2d');frame=0;
    drawBell({x:24,y:40,z:0,state:stt,t:1},false);g=old;return c;});
  // The Titan Arum (web drawArum): closed bud swaying (2 frames) and in bloom. Centre (24,42).
  out.arum=[['walk',31],['walk',94],['bloom',0]].map(([stt,f])=>{const c=mkCanvas(48,48),old=g;g=c.getContext('2d');frame=f;
    drawArum({x:24,y:37,state:stt,t:1,hp:8,f:1},false);g=old;return c;});
  // The Richmond Vampire (web drawEnemy 'vampire'): one frame facing right, feet at (24,40) like the others.
  out.vampire=[0].map(()=>{const c=mkCanvas(48,48),x=c.getContext('2d');x.drawImage(SPR.vampire.r.c,16,22);return c;});
  // a push block (web drawBlocks, without its soft shadow)
  {const c=mkCanvas(16,16),x=c.getContext('2d'),R3=(a,b,w,h,col)=>{x.fillStyle=col;x.fillRect(a,b,w,h);};
   R3(0,0,16,15,'#1a1423');R3(1,1,14,13,'#8a7a6a');R3(1,1,14,3,'#b4a490');R3(1,11,14,3,'#5e5044');R3(4,5,8,5,'#6e5e4e');R3(5,6,6,3,'#9a8a76');out.block=c;}
  out.raven=[0,1].map(fl=>{const c=mkCanvas(48,48),x=c.getContext('2d'),X=24,Y=38;
    x.drawImage(SPR.raven.r.c,X-12,Y-9);x.drawImage(WINGS[fl].r.c,X-12+4,Y-9+(fl?6:-3));return c;});
  // 16x16 icons: keg, hook, keystones (granite, iron, brick, bronze); explosion 32x32 x2
  const icon=(img)=>{const c=mkCanvas(16,16),x=c.getContext('2d');x.drawImage(img,Math.floor((16-img.width)/2),Math.floor((16-img.height)/2));return c;};
  out.icons=[icon(IC.keg),icon(IC.hook),icon(KSIC.granite),icon(KSIC.iron),icon(KSIC.brick),icon(KSIC.bronze),icon(IC.lantern),icon(IC.bug),icon(IC.key),icon(IC.carkey),icon(IC.piece),icon(IC.can),icon(IC.ball),icon(IC.shoes),icon(IC.bike),icon(IC.bottle),icon(IC.bib),icon(IC.oar),icon(IC.rod),icon(IC.frog)];
  // the bike under the hero (web drawBike), side and front, the hero's feet at (8,6)
  out.bikes=[1,0].map(dir=>{const c=mkCanvas(16,16),old=g,op=p,os=st.riding;g=c.getContext('2d');p={x:8,y:6,dir};st.riding=true;frame=0;
    try{drawBike();}catch(e){}g=old;p=op;st.riding=os;return c;});
  out.boom=[0,1].map(i=>{const c=mkCanvas(32,32),old=g;g=c.getContext('2d');
    const cols=['#ffffff','#fff2a8','#f8e060','#f08020','#e83048'];const r0=i?14:10;
    for(let r=r0;r>0;r-=2){g.fillStyle=cols[Math.min(4,Math.floor((r0-r)/2.5)+i)];g.beginPath();g.arc(16,16,r,0,7);g.fill();}
    g=old;return c;});
  // Town life: three cars (red, yellow, blue), the Pulse bus, a pigeon sitting and flying (2 wing beats).
  // Each in 16-row cells, facing right; the ROM flips them. Car 32 wide, bus 48, pigeon 16 each.
  {const c=mkCanvas(32*3+48+16*3,16),old=g;g=c.getContext('2d');frame=0;
   ['#e83048','#f2c14e','#3d63c8'].forEach((col,i)=>drawCar({x:i*32+16,y:10,len:26,kind:'car',dir:1,col}));
   drawCar({x:96+24,y:11,len:40,kind:'bus',dir:1});
   {const x=152,y=10;R(x-3,y-2,6,4,'#1a1423');R(x-2,y-1,4,2,'#8a8496');R(x+2,y-4,3,3,'#1a1423');R(x+3,y-3,1,1,'#5f566a');}
   [0,1].forEach(w=>{const x=168+w*16,y=9;R(x-2,y-1,4,3,'#1a1423');R(x-1,y,2,1,'#8a8496');R(x-6,y-(w?3:0),4,2,'#5f566a');R(x+2,y-(w?3:0),4,2,'#5f566a');});
   g=old;out.town=c;}
  // Challenge tokens (stage 10): tap, melon, lure, trash, the race flag (2 colours), and the "!" over someone with a challenge.
  {const c=mkCanvas(16*8,16),old=g;g=c.getContext('2d');frame=0;g.drawImage(IC.camera,112+2,2);   // and the camera, for the B icon
   ['tap','melon','lure','trash'].forEach((k,i)=>g.drawImage(IC[k],i*16+2,2));
   [0,1].forEach(f=>{const x=64+f*16+4,y=15;R(x-1,y-14,2,14,'#1a1423');R(x+1,y-14,10,7,'#1a1423');R(x+1,y-13,9,5,f?'#e83048':'#f2c14e');R(x+1,y-13,4,2,'#ffffff');});
   R(96+6,1,5,10,'#1a1423');R(96+7,2,3,7,'#f2c14e');R(96+6,11,5,4,'#1a1423');R(96+7,12,3,2,'#f2c14e');
   g=old;out.tokens=c;}
  // Minigame backgrounds (stage 10): each game's own draw with nothing moving in it. The bat, the racket and the
  // free-throw needle are 2x2 and 3x16 rectangles the ROM draws as sprites, so leave those out; the tennis player
  // moves too. 256x200 like the web game's screen.
  {const R0=R,skip=(w,h)=>(w==2&&h==2)||(w==3&&h==16);
   const bg=(fn)=>{const c=mkCanvas(256,200),old=g;g=c.getContext('2d');frame=0;R=(x,y,w,h,col)=>{if(!skip(w,h))R0(x,y,w,h,col);};
     try{fn();}finally{R=R0;g=old;texts.length=0;}return c;};
   out.minibg={
     bat:bg(()=>drawBat({t:0,n:0,hits:0,hr:0,ball:null,swing:0})),
     hoops:bg(()=>MINI.hoops.draw({n:0,made:0,ph:0,ball:null})),
     pin:bg(()=>{const st0=g.stroke.bind(g);g.stroke=function(){if(g.strokeStyle!='#f2c14e')st0();};   // the flippers move: sprites
       drawPin({t:0,fl:[0,0],lit:[0,0,0,0,0],bumps:[[92,60,9],[164,60,9],[128,92,10],[60,104,7],[196,104,7]],ball:null,score:0,balls:3});}),
     tennis:bg(()=>{const di=g.drawImage.bind(g);g.drawImage=(im,...r)=>{if(im!==HERO.up[0].r.c)di(im,...r);};MINI.tennis.draw({x:128,ball:null,sw:0,ret:0,miss:0});})};}
  // The scrolling-road races (derby, trail, raceway): each course as a 256x512 strip that loops on BG1, its repeats
  // chosen to divide 512 (the web game's 24 px dashes become 32), then the player and the obstacles, 32x32 each.
  {const strip=(fn)=>{const c=mkCanvas(256,512),old=g;g=c.getContext('2d');try{fn();}finally{g=old;}return c;};
   const box=(fn)=>{const c=mkCanvas(32,32),old=g;g=c.getContext('2d');frame=0;try{fn();}finally{g=old;}return c;};
   const H=512;
   out.race={
    derby:{bg:strip(()=>{R(0,0,W,H,'#58a848');for(let y=0;y<H;y+=32){R(0,y,48,30,'#a85040');R(2,y+2,44,4,'#c46a58');R(208,y,48,30,'#94483a');R(210,y+2,44,4,'#b06254');R(10,y+12,10,8,'#f8d868');R(228,y+16,10,8,'#28406a');}
       R(56,0,144,H,'#8a8e9c');R(56,0,4,H,'#e4e0d4');R(196,0,4,H,'#e4e0d4');for(let y=0;y<H;y+=32)R(127,y,2,12,'#f2c14e');}),
     player:box(()=>{const cx=16,cy=18;R(cx-9,cy-10,18,22,'#1a1423');R(cx-8,cy-9,16,20,'#e83048');R(cx-8,cy-9,16,3,'#f890a0');R(cx-11,cy-7,4,6,'#1a1423');R(cx+7,cy-7,4,6,'#1a1423');R(cx-11,cy+5,4,6,'#1a1423');R(cx+7,cy+5,4,6,'#1a1423');g.drawImage(HERO.up[0].r.c,cx-8,cy-8);}),
     obs:{hay:box(()=>{const x=16,y=16;R(x-10,y-6,20,12,'#1a1423');R(x-9,y-5,18,10,'#d8b048');R(x-9,y-2,18,1,'#a88830');}),
          cone:box(()=>{const x=16,y=16;R(x-4,y-6,8,12,'#1a1423');R(x-3,y-5,6,10,'#f08030');R(x-3,y-2,6,2,'#ffffff');}),
          puddle:box(()=>{const x=16,y=16;R(x-12,y-4,24,8,'#2a4a8a');R(x-6,y-2,6,1,'#a8c8f0');})}},
    trail:{bg:strip(()=>{R(0,0,W,H,'#58a848');R(0,0,52,H,'#4a6e62');for(let y=0;y<H;y+=32){R(6,y,10,1,'#b8d4c0');R(26,y+14,12,1,'#7a9e8a');}R(50,0,6,H,'#c8b890');
       for(let y=0;y<H;y+=64){blob(212,y,'#2f7a3c','#1f5a30','#143c22');blob(228,y+32,'#2f7a3c','#1f5a30','#143c22');}
       R(56,0,144,H,'#5e5a62');R(56,0,3,H,'#e8e4dc');R(197,0,3,H,'#e8e4dc');for(let y=0;y<H;y+=32)R(127,y,2,12,'#e8e4dc');}),
     player:box(()=>{const cx=16,cy=18;R(cx-2,cy-6,4,18,'#1a1423');R(cx-1,cy-5,2,16,'#8e909c');R(cx-5,cy+1,10,2,'#e83048');g.drawImage(HERO.up[0].r.c,cx-8,cy-10);}),
     obs:{goose:box(()=>g.drawImage(SPR.goose.r.c,8,7)),
          jogger:box(()=>{const x=16,y=15;R(x-3,y-8,6,6,'#f2c698');R(x-4,y-2,8,7,'#e83048');R(x-3,y+5,2,4,'#3c3c54');R(x+1,y+5,2,4,'#3c3c54');}),
          puddle:box(()=>{const x=16,y=16;R(x-12,y-4,24,8,'#2a4a8a');})}},
    race:{bg:strip(()=>{R(0,0,W,H,'#58a848');R(0,0,48,H,'#c8ccd4');for(let y=0;y<H;y+=8)for(let i=0;i<5;i++){const q=HH(i,y/8|0,3);R(4+i*9,y+2,3,3,['#e83048','#f2c14e','#3d63c8','#ffffff'][q%4]);}R(48,0,6,H,'#ffffff');
       R(54,0,152,H,'#4a4a54');R(206,0,6,H,'#e83048');for(let y=0;y<H;y+=16)R(206,y,6,8,'#ffffff');R(212,0,44,H,'#58a848');for(let y=0;y<H;y+=32){R(104,y,2,12,'#e8e4dc');R(154,y,2,12,'#e8e4dc');}}),
     player:box(()=>{const cx=16,cy=17;R(cx-8,cy-11,16,22,'#1a1423');R(cx-7,cy-10,14,20,'#e83048');R(cx-5,cy-7,10,5,'#a8c8e0');R(cx-7,cy+1,14,2,'#ffffff');R(cx-1,cy+3,2,5,'#ffffff');}),
     obs:{car:box(()=>{const x=16,y=16;R(x-7,y-10,14,20,'#1a1423');R(x-6,y-9,12,18,'#3d63c8');R(x-4,y-6,8,5,'#a8c8e0');R(x-6,y-1,12,2,'#ffffff');}),
          car2:box(()=>{const x=16,y=16;R(x-7,y-10,14,20,'#1a1423');R(x-6,y-9,12,18,'#f2c14e');R(x-4,y-6,8,5,'#a8c8e0');R(x-6,y-1,12,2,'#ffffff');}),
          oil:box(()=>{const x=16,y=16;R(x-11,y-4,22,8,'#14101e');R(x-5,y-2,5,1,'#5a3a8a');})}}};
   out.raceInfo=['derby','trail','race'].map(k=>({k,gold:MINI[k].gold,silver:MINI[k].silver,who:MINI[k].who,name:MINI[k].name,label:MINI[k].label}));}
  // pinball: a lit bumper (radius 9, 10, 7) to lay over the dark one when it's hit
  out.pinlit=(()=>{const c=mkCanvas(96,32),old=g;g=c.getContext('2d');[9,10,7].forEach((r,i)=>{const x=i*32+16,y=16;
    g.fillStyle='#fff2a8';g.beginPath();g.arc(x,y,r,0,6.283);g.fill();g.fillStyle='#ffffff';g.beginPath();g.arc(x-2,y-2,r/2.5,0,6.283);g.fill();});g=old;return c;})();
  // stage 10, part 3: landmark cards, fish, which places are on the James (for sturgeon)
  out.cards=CARDS.map(c=>({id:c.id,name:c.name,fact:c.fact,src:c.src}));out.pack=PACK;out.bosscard=BOSSCARD;
  out.fish=FISH.map(f=>({id:f.id,name:f.name,str:f.str,coins:f.coins,txt:f.txt}));
  out.river=Object.keys(SC).filter(id=>{const o=cur;cur=SC[id];let r=false;try{r=!!riverHere();}catch(e){}cur=o;return r;});
  // The photo album (stage 10): every landmark with a caption, its thumbnail as the web game draws it (thumbOf), and
  // where it stands so the camera can find the nearest.
  out.photos=PHOTOS.map(ph=>{const L=ph.L,s=SC[ph.id],w=L.w*TS+24,h=L.h*TS+20,c=mkCanvas(w,h),old=g;g=c.getContext('2d');
    R(0,0,w,h,'#58a848');for(let i=0;i<w*h/90;i++){const q=HH(i,ph.art.length,5);R(q%w,(q>>8)%h,2,1,'#68b854');}
    try{LMART[L.art](12,10,L.w*TS,L.h*TS,0,s,L);}catch(e){}g=old;
    return {map:ph.id,art:ph.art,cap:PHOTOCAP[ph.art],x:L.x*TS+L.w*8,y:L.y*TS+L.h*8,img:c};});
  out.photoAt=[];for(const id in SC){const s=SC[id];if(!s.ow)continue;for(const L of s.lm||[]){const k=out.photos.findIndex(q=>q.art==L.art);if(k>=0&&PHOTOCAP[L.art])out.photoAt.push({map:id,k,x:L.x*TS+L.w*8,y:L.y*TS+L.h*8});}}
  // stage 11: the title screen (web draw() 'title': the night skyline, the logo, a panel for the save slots),
  // the name screen (a big panel), and the opening's words
  {const T=(x,str,X,Y,col,sz)=>{x.font=sz+'px PS2P';x.textAlign='center';x.textBaseline='top';x.fillStyle=col;x.fillText(str,X,Y);};
   const base=()=>{const c=mkCanvas(256,200),old=g;g=c.getContext('2d');frame=0;try{drawTitleBg();}finally{g=old;}return c;};
   const t=base(),x=t.getContext('2d');
   // the moon sits right where the SNES logo's Y goes: sky over it, and the moon up in the corner instead
   x.putImageData(x.getImageData(150,18,24,22),192,18);
   for(const[a,b,w,h,c]of[[196,22,14,14,'#fff2c8'],[194,25,18,8,'#fff2c8'],[198,20,10,18,'#fff2c8'],[200,26,3,3,'#e8d8a0'],[205,31,2,2,'#e8d8a0']]){x.fillStyle=c;x.fillRect(a+34,b-14,w,h);}
   for(const[dx,dy]of[[-1,0],[1,0],[0,-1],[0,1],[-1,-1],[1,-1],[-1,1],[1,1]]){T(x,'RIVER CITY',128+dx,24+dy,'#1a1423',16);T(x,'QUEST',128+dx,46+dy,'#1a1423',16);}T(x,'RIVER CITY',130,26,'#6a1c2c',16);T(x,'RIVER CITY',128,24,'#f2c14e',16);T(x,'QUEST',130,48,'#6a1c2c',16);T(x,'QUEST',128,46,'#fff2a8',16);
   T(x,'The Tunnel Under Church Hill',129,71,'#1a1423',8);T(x,'The Tunnel Under Church Hill',128,70,'#e8ecf8',8);
   x.fillStyle='rgba(8,10,24,.8)';x.fillRect(14,84,228,58);x.fillStyle='#f2c14e';x.fillRect(14,84,228,1);x.fillRect(14,141,228,1);out.titlebg=t;
   const n=base(),y=n.getContext('2d');y.fillStyle='rgba(8,10,24,.82)';y.fillRect(16,20,224,160);y.fillStyle='#f2c14e';y.fillRect(16,20,224,1);out.namebg=n;}
  out.intro=PANELS.map(P=>{const o=st;st=newState();st.name='\x01';let t='';try{t=P.txt();}catch(e){}st=o;return t;});
  out.chal=Object.entries(CHAL).map(([id,d])=>({id,name:d.name,type:d.type,time:d.time||0,ic:d.ic||null,foe:d.foe||null,n:d.n||0,needs:d.needs||null,intro:d.intro}));
  out.lore=LORE.map(([id,t,s])=>{const sn=s.split('. ')[0];return {id,title:t,quote:sn.length>95?sn.slice(0,92)+'...':sn+'.'};});
  out.keynames=KEYNAME;
  // heart container pickup (16x16, centre 8,9)
  {const c=mkCanvas(16,16),old=g;g=c.getContext('2d');frame=0;const k={kind:'container',x:8,y:9,life:1e9,t:0};
   R(1,1,15,14,'#1a1423');R(2,2,13,12,'#fff2a8');R(3,3,11,10,'#3a1c30');g.drawImage(HEART,4,4);g=old;out.container=c;}
  out.text={foreman:BOSSTXT.foreman,foremanIntro:BOSSINTRO.foreman,turtle:BOSSTXT.turtle,turtleIntro:BOSSINTRO.turtle,wraith:BOSSTXT.wraith,wraithIntro:BOSSINTRO.wraith,raven:BOSSTXT.raven,ravenIntro:BOSSINTRO.raven,bell:BOSSTXT.bell,bellIntro:BOSSINTRO.bell,arum:BOSSTXT.arum,arumIntro:BOSSINTRO.arum,vampireIntro:BOSSINTRO.vampire,
    vampire:['The pale thing shrieks and comes apart like wet newspaper.','Above you the fog thins. One by one the lamps along Broad Street come back on.','Richmond is safe. Somebody owes you a ginger beer.','By morning the fog has burned off the James. Every lamp from Church Hill to Carytown is lit.',"The archivist adds one last story to the journal. It's called Rory and the Fog."],
    relight:RELIGHT.map(r=>[r[0],r[1]]),credits:(()=>{const o=st;st=newState();const c=CREDITS();st=o;return c;})(),can:'The sprout drinks it up and grows!',bib:['A race bib from the Monument Avenue 10K.','A runner in the Fan must be missing it.'],bloom:'The Titan Arum unfurls!'};
  return out;})()`, sb);
{
  const strip = createCanvas(48 * bossArt.foreman.length, 48), x = strip.getContext('2d');
  bossArt.foreman.forEach((c, i) => x.drawImage(c, i * 48, 0));
  fs.writeFileSync(path.join(OUT, 'combat', 'foreman.png'), strip.toBuffer('image/png'));
  fs.writeFileSync(path.join(OUT, 'combat', 'container.png'), bossArt.container.toBuffer('image/png'));
  fs.writeFileSync(path.join(OUT, 'combat', 'block.png'), bossArt.block.toBuffer('image/png'));
  fs.writeFileSync(path.join(OUT, 'combat', 'town.png'), bossArt.town.toBuffer('image/png'));
  fs.writeFileSync(path.join(OUT, 'combat', 'tokens.png'), bossArt.tokens.toBuffer('image/png'));
  for (const k in bossArt.race) {
    const r = bossArt.race[k];
    fs.writeFileSync(path.join(OUT, 'combat', 'race_' + k + '_bg.png'), r.bg.toBuffer('image/png'));
    const names = Object.keys(r.obs), c = createCanvas(32 * (1 + names.length), 32), x = c.getContext('2d');
    x.drawImage(r.player, 0, 0); names.forEach((n, i) => x.drawImage(r.obs[n], 32 * (i + 1), 0));
    fs.writeFileSync(path.join(OUT, 'combat', 'race_' + k + '_spr.png'), c.toBuffer('image/png'));
  }
  fs.writeFileSync(path.join(OUT, 'combat', 'race.json'), JSON.stringify({ info: bossArt.raceInfo, obs: Object.fromEntries(Object.entries(bossArt.race).map(([k, r]) => [k, Object.keys(r.obs)])) }, null, 1));
  fs.writeFileSync(path.join(OUT, 'combat', 'pinlit.png'), bossArt.pinlit.toBuffer('image/png'));
  fs.writeFileSync(path.join(OUT, 'combat', 'title.png'), bossArt.titlebg.toBuffer('image/png'));
  fs.writeFileSync(path.join(OUT, 'combat', 'name.png'), bossArt.namebg.toBuffer('image/png'));
  fs.writeFileSync(path.join(OUT, 'combat', 'intro.json'), JSON.stringify(bossArt.intro, null, 1));
  fs.mkdirSync(path.join(OUT, 'photos'), { recursive: true });
  bossArt.photos.forEach((ph, i) => fs.writeFileSync(path.join(OUT, 'photos', i + '.png'), ph.img.toBuffer('image/png')));
  fs.writeFileSync(path.join(OUT, 'photos', 'photos.json'), JSON.stringify({ photos: bossArt.photos.map(({ img, ...r }) => r), at: bossArt.photoAt }, null, 1));
  for (const k in bossArt.minibg) fs.writeFileSync(path.join(OUT, 'combat', 'mini_' + k + '.png'), bossArt.minibg[k].toBuffer('image/png'));
  for (const name of ['turtle', 'wraith', 'raven', 'bell', 'arum', 'vampire']) {
    const ts = createCanvas(48 * bossArt[name].length, 48), tx2 = ts.getContext('2d');
    bossArt[name].forEach((c, i) => tx2.drawImage(c, i * 48, 0));
    fs.writeFileSync(path.join(OUT, 'combat', name + '.png'), ts.toBuffer('image/png'));
  }
  const ic = createCanvas(16 * bossArt.icons.length, 16), ix = ic.getContext('2d');
  bossArt.icons.forEach((c, i) => ix.drawImage(c, i * 16, 0));
  fs.writeFileSync(path.join(OUT, 'combat', 'icons.png'), ic.toBuffer('image/png'));
  const bk = createCanvas(32, 16), bkx = bk.getContext('2d');
  bossArt.bikes.forEach((c, i) => bkx.drawImage(c, i * 16, 0));
  fs.writeFileSync(path.join(OUT, 'combat', 'bikes.png'), bk.toBuffer('image/png'));
  const bm = createCanvas(64, 32), bx = bm.getContext('2d');
  bossArt.boom.forEach((c, i) => bx.drawImage(c, i * 32, 0));
  fs.writeFileSync(path.join(OUT, 'combat', 'boom.png'), bm.toBuffer('image/png'));
  // spark shot (2 blinks) and the hook head and chain link, 8x8 each, drawn like the web game
  const sm = createCanvas(88, 8), q = sm.getContext('2d');
  const R2 = (X, Y, W, H, C) => { q.fillStyle = C; q.fillRect(X, Y, W, H); };
  for (const [i, c] of [[0, '#f8e060'], [1, '#f08020']]) { const X = i * 8 + 4, Y = 4;
    R2(X - 3, Y - 2, 6, 4, '#1a1423'); R2(X - 2, Y - 3, 4, 6, '#1a1423'); R2(X - 2, Y - 2, 4, 4, c); R2(X - 1, Y - 1, 2, 2, '#ffffff'); }
  { const X = 20, Y = 4; R2(X - 3, Y - 3, 7, 7, '#1a1423'); R2(X - 2, Y - 2, 5, 5, '#d8dce4'); R2(X - 1, Y - 1, 3, 3, '#8e909c'); }
  { const X = 28, Y = 4; R2(X - 1, Y - 1, 3, 3, '#1a1423'); R2(X, Y, 1, 1, '#aeb2c0'); }
  // orb (the Wraith's) and a reflected shot
  for (const [i, c] of [[4, '#e83048'], [5, '#fff2a8'], [6, '#4a4662'], [7, '#f0c070'], [8, '#84cc68']]) { const X = i * 8 + 4, Y = 4;
    R2(X - 3, Y - 2, 6, 4, '#1a1423'); R2(X - 2, Y - 3, 4, 6, '#1a1423'); R2(X - 2, Y - 2, 4, 4, c); R2(X - 1, Y - 1, 2, 2, '#ffffff'); }
  // weather (web drawRain, drawSnow): a slanted raindrop and a snowflake
  R2(72 + 5, 0, 1, 2, '#d8dce4'); R2(72 + 4, 2, 1, 2, '#d8dce4'); R2(72 + 3, 4, 1, 3, '#d8dce4');  // grey: the HUD palette has no blue
  R2(80 + 3, 3, 2, 2, '#ffffff'); R2(80 + 2, 4, 1, 1, '#e8eeff'); R2(80 + 5, 3, 1, 1, '#e8eeff');
  fs.writeFileSync(path.join(OUT, 'combat', 'small.png'), sm.toBuffer('image/png'));
  const intro = vm.runInContext("(function(){try{return Object.entries(BANNERS||{})}catch(e){return null}})()", sb);
  fs.writeFileSync(path.join(OUT, 'combat', 'boss.json'), JSON.stringify({ text: bossArt.text, keynames: bossArt.keynames, chal: bossArt.chal, lore: bossArt.lore, cards: bossArt.cards, pack: bossArt.pack, bosscard: bossArt.bosscard, fish: bossArt.fish, river: bossArt.river }, null, 1));
}
