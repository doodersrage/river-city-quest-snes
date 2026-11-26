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
      solve:s.solve||null,bossT:s.bossT||null,entry:!!s.entry,reward:s.reward||null,braz:s.braz||[],
      pickups:(s.items||[]).filter(it=>['skey','carkey','piece','bib'].includes(it.kind)).map(it=>({id:it.id,kind:it.kind,x:it.px||it.tx*16+8,y:it.py||it.ty*16+8})),
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
        if(!r)continue;const pg=pages(r),k=pg.join('|');if(!seen.has(k)){seen.add(k);talks.push(pg);}}}
      if(!talks.length&&n0.lines)for(const l of n0.lines)talks.push([String(l)]);
      if(!talks.length&&n0.talk=='shop')talks.push(['What can I get you?']);
      if(!talks.length&&n0.talk=='grocer')talks.push(['Fresh from the farms!']);
      if(!talks.length)talks.push(['...']);
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
      let sp;try{sp=n0.spr&&n0.spr.down?n0.spr:npcSpr(n0);}catch(e){sp=npcSpr(Object.assign({hat:['#888','#666','#aaa'],shirt:['#888','#666','#aaa']},n0));}
      return {tx:n0.tx,ty:n0.ty,who:n0.who||'',talks,gifts,quest,refill,shop:n0.talk=='shop'?'shop':n0.talk=='grocer'?'market':null,spr:[sp.down.r.c,sp.up.r.c,sp.side.r.c]};
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
    return { tx: n.tx, ty: n.ty, who: n.who, talks: n.talks, gifts: n.gifts, quest: n.quest, refill: n.refill, shop: n.shop, file };
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
  out.icons=[icon(IC.keg),icon(IC.hook),icon(KSIC.granite),icon(KSIC.iron),icon(KSIC.brick),icon(KSIC.bronze),icon(IC.lantern),icon(IC.bug),icon(IC.key),icon(IC.carkey),icon(IC.piece),icon(IC.can),icon(IC.ball),icon(IC.shoes),icon(IC.bike),icon(IC.bottle),icon(IC.bib)];
  // the bike under the hero (web drawBike), side and front, the hero's feet at (8,6)
  out.bikes=[1,0].map(dir=>{const c=mkCanvas(16,16),old=g,op=p,os=st.riding;g=c.getContext('2d');p={x:8,y:6,dir};st.riding=true;frame=0;
    try{drawBike();}catch(e){}g=old;p=op;st.riding=os;return c;});
  out.boom=[0,1].map(i=>{const c=mkCanvas(32,32),old=g;g=c.getContext('2d');
    const cols=['#ffffff','#fff2a8','#f8e060','#f08020','#e83048'];const r0=i?14:10;
    for(let r=r0;r>0;r-=2){g.fillStyle=cols[Math.min(4,Math.floor((r0-r)/2.5)+i)];g.beginPath();g.arc(16,16,r,0,7);g.fill();}
    g=old;return c;});
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
  const sm = createCanvas(72, 8), q = sm.getContext('2d');
  const R2 = (X, Y, W, H, C) => { q.fillStyle = C; q.fillRect(X, Y, W, H); };
  for (const [i, c] of [[0, '#f8e060'], [1, '#f08020']]) { const X = i * 8 + 4, Y = 4;
    R2(X - 3, Y - 2, 6, 4, '#1a1423'); R2(X - 2, Y - 3, 4, 6, '#1a1423'); R2(X - 2, Y - 2, 4, 4, c); R2(X - 1, Y - 1, 2, 2, '#ffffff'); }
  { const X = 20, Y = 4; R2(X - 3, Y - 3, 7, 7, '#1a1423'); R2(X - 2, Y - 2, 5, 5, '#d8dce4'); R2(X - 1, Y - 1, 3, 3, '#8e909c'); }
  { const X = 28, Y = 4; R2(X - 1, Y - 1, 3, 3, '#1a1423'); R2(X, Y, 1, 1, '#aeb2c0'); }
  // orb (the Wraith's) and a reflected shot
  for (const [i, c] of [[4, '#e83048'], [5, '#fff2a8'], [6, '#4a4662'], [7, '#f0c070'], [8, '#84cc68']]) { const X = i * 8 + 4, Y = 4;
    R2(X - 3, Y - 2, 6, 4, '#1a1423'); R2(X - 2, Y - 3, 4, 6, '#1a1423'); R2(X - 2, Y - 2, 4, 4, c); R2(X - 1, Y - 1, 2, 2, '#ffffff'); }
  fs.writeFileSync(path.join(OUT, 'combat', 'small.png'), sm.toBuffer('image/png'));
  const intro = vm.runInContext("(function(){try{return Object.entries(BANNERS||{})}catch(e){return null}})()", sb);
  fs.writeFileSync(path.join(OUT, 'combat', 'boss.json'), JSON.stringify({ text: bossArt.text, keynames: bossArt.keynames }, null, 1));
}
