/* 큰 그림자의 방향, 수신 높이, 광원별 차폐와 원화 보존을 검사합니다. */
import assert from 'node:assert/strict';
import fs from 'node:fs';
import {performance} from 'node:perf_hooks';

const workerSource=fs.readFileSync(new URL('./pixel_worker.js',import.meta.url),'utf8');
const create=new Function('self','performance',workerSource+'\nreturn {init,render,castVisibility,boxPlanes,roofPlanes,rayHits};');
const engine=create({postMessage(){}},performance);
const pixel=value=>new Uint8ClampedArray(value).buffer;
const tiny={width:1,height:1,contacts:[],lamps:[],lampOwners:[],casters:[[1,32,32,16,8,8,16,.94]]};
engine.init({scene:tiny,maps:[[90,100,70,255],[90,100,70,255],[128,128,255,255],[0,0,0,255],[4,4,4,255],[0,0,0,255]].map(pixel)});
const sun=(x,y,z,elevation=45,owner=0,direction=-1)=>{
  const angle=elevation*Math.PI/180;
  return engine.castVisibility(x,y,z,direction*Math.cos(angle)*800,0,Math.sin(angle)*800,owner,0,.5);
};
assert(sun(48,32,0)<.1,'태양 반대쪽 지면에 그림자가 있어야 합니다.');
assert.equal(sun(16,32,0),1,'태양 쪽 지면은 밝아야 합니다.');
assert.equal(sun(48,32,0,45,0,1),1,'태양 방향을 반전하면 그림자가 이동해야 합니다.');
assert(sun(88,32,0,15)<.1&&sun(88,32,0,75)===1,'낮은 태양은 긴 그림자를 만들어야 합니다.');
assert(sun(48,32,12,45,2)<.1,'다른 물체의 높은 표면도 그림자를 받아야 합니다.');
assert.equal(sun(48,32,12,45,1),1,'자기 근사 영역으로 스프라이트가 검게 변하면 안 됩니다.');
const edge=sun(48,40,0);
assert(edge>.1&&edge<1,'그림자 경계에 중간 음영이 있어야 합니다.');
assert(engine.castVisibility(16,32,0,64,0,48,0,0,.5)<.1,'점광원도 물체에 막혀야 합니다.');
assert.equal(engine.castVisibility(16,32,0,-16,0,48,0,0,.5),1,'반대편 점광원은 같은 지점을 밝혀야 합니다.');
assert.equal(engine.castVisibility(16,32,0,64,0,48,0,1,.5),1,'건물에 부착한 등불은 자체 근사 영역을 건너뛰어야 합니다.');
assert.equal(engine.castVisibility(16,32,0,16,0,16,0,0,.5),1,'이동광이 근사 영역 안에 들어가도 전체가 검게 변하면 안 됩니다.');
assert.equal(engine.castVisibility(16,32,0,0,0,0,0,0,.5),1,'길이가 없는 광선은 밝기를 유지해야 합니다.');

/* 광선 교차를 별도로 투영한 꼭짓점의 볼록 다각형과 비교합니다. */
const cross=(a,b,c)=>(b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0]);
function hull(points){
  const sorted=[...points].sort((a,b)=>a[0]-b[0]||a[1]-b[1]),lower=[],upper=[];
  for(const point of sorted){while(lower.length>1&&cross(lower.at(-2),lower.at(-1),point)<=0)lower.pop();lower.push(point);}
  for(const point of [...sorted].reverse()){while(upper.length>1&&cross(upper.at(-2),upper.at(-1),point)<=0)upper.pop();upper.push(point);}
  return [...lower.slice(0,-1),...upper.slice(0,-1)];
}
const wallVertices=[];
for(const x of [24,40])for(const y of [24,40])for(const z of [0,32])wallVertices.push([x,y,z]);
const roofVertices=[];
for(const x of [24,40])for(const [y,z] of [[24,20],[32,32],[40,20]])roofVertices.push([x,y,z]);
const walls=engine.boxPlanes([24,24,0,40,40,32]),roof=engine.roofPlanes([24,24,40,40,20,32]);
let projectedComparisons=0;
for(const [planes,vertices] of [[walls,wallVertices],[roof,roofVertices]]){
  for(const light of [{sun:[-1,0,1]},{sun:[-.7,-.7,2.6]},{sun:[-.7,-.7,.36]},{point:[-8,8,64]}]){
    const polygon=hull(vertices.map(([x,y,z])=>light.sun?[x-z*light.sun[0]/light.sun[2],y-z*light.sun[1]/light.sun[2]]:[light.point[0]+(x-light.point[0])*light.point[2]/(light.point[2]-z),light.point[1]+(y-light.point[1])*light.point[2]/(light.point[2]-z)]));
    for(let y=-8.375;y<142;y+=3.25)for(let x=-8.375;x<142;x+=3.25){
      const sides=polygon.map((point,i)=>cross(point,polygon[(i+1)%polygon.length],[x,y]));
      if(sides.some(value=>Math.abs(value)<1e-6))continue;
      const expected=sides.every(value=>value>0),delta=light.sun?light.sun.map(value=>value*800):[light.point[0]-x,light.point[1]-y,light.point[2]];
      assert.equal(engine.rayHits(planes,x,y,0,...delta),expected,'벽·지붕의 독립적인 꼭짓점 투영과 광선 교차가 같아야 합니다.');
      projectedComparisons++;
    }
  }
}
assert(engine.rayHits(walls,70,39,0,-800,0,800),'상자의 먼 모서리를 둥글게 잘라내면 안 됩니다.');
assert(!engine.rayHits(walls,70,42,0,-800,0,800),'상자 옆면 밖에는 그림자가 없어야 합니다.');
assert(engine.rayHits(roof,69,32,0,-800,0,800)&&!engine.rayHits(roof,69,24,0,-800,0,800),'지붕 용마루는 처마보다 긴 그림자를 만들어야 합니다.');
assert(engine.rayHits(walls,80,32,0,-88,0,64)&&!engine.rayHits(walls,90,32,0,-98,0,64),'점광원의 원근 투영 길이를 확인합니다.');
assert(!engine.rayHits(walls,0,32,0,16,0,48),'광원 뒤의 상자는 차폐하면 안 됩니다.');
const fixture={owner:7,opacity:.94,walls:[24,24,0,40,40,20],roof:[24,24,40,40,20,32],chimney:[34,28,28,38,32,48]};
engine.init({scene:{...tiny,casters:[],buildings:[fixture]},maps:[[90,100,70,255],[90,100,70,255],[128,128,255,255],[0,0,0,255],[4,4,4,255],[0,0,0,255]].map(pixel)});
assert(engine.castVisibility(69,32,0,-800,0,800,0,0,.5)<.3,'건물 지붕이 빛을 막아야 합니다.');
assert(engine.castVisibility(83,30,0,-800,0,800,0,0,.5)<.3,'굴뚝이 지붕 너머에 별도의 그림자를 만들어야 합니다.');
assert.equal(engine.castVisibility(69,32,0,-800,0,800,7,0,.5),1,'자체 건물은 제외합니다.');
assert.equal(engine.castVisibility(69,32,0,-800,0,800,0,7,.5),1,'소속 건물의 광원은 제외합니다.');
assert.equal(engine.castVisibility(16,32,0,16,0,12,0,0,.5),1,'실내로 이동한 시험 광원은 제외합니다.');
assert.equal(engine.castVisibility(16,32,0,0,0,0,0,0,.5),1,'건물이 있어도 길이가 없는 광선은 안전해야 합니다.');
const report={geometry:'passed',softEdgeVisibility:edge,projectedComparisons};
if(process.argv[2]){
  const directory=process.argv[2];
  const scene=JSON.parse(fs.readFileSync(directory+'/scene.json','utf8'));
  const raw=['original','albedo','normals','surface','material','owners'].map(name=>fs.readFileSync(directory+'/'+name+'.rgba'));
  engine.init({scene,maps:raw.map(data=>data.buffer.slice(data.byteOffset,data.byteOffset+data.byteLength))});
  const base={mode:2,detail:1.25,azimuth:225,elevation:40,ambient:.5,sun:true,lamps:true,windows:true,cursor:false,shadows:true,position:[95,210,95]};
  const render=changes=>engine.render({...base,...changes});
  assert.equal(Buffer.compare(Buffer.from(render({mode:0})),raw[0]),0,'원본 비교 픽셀 보존');
  assert.equal(Buffer.compare(Buffer.from(render({mode:1})),raw[1]),0,'명암 완화 비교 픽셀 보존');
  const clear=render({shadows:false}),shaded=render({});
  let largeGroundPixels=0,receiverPixels=0,minimumGroundRatio=1;
  for(let i=0;i<scene.width*scene.height;i++){
    const p=i*4,a=clear[p]+clear[p+1]+clear[p+2],b=shaded[p]+shaded[p+1]+shaded[p+2];
    if(a===0)continue;
    if(raw[5][p]===0){
      minimumGroundRatio=Math.min(minimumGroundRatio,b/a);
      if(b<a*.9)largeGroundPixels++;
      assert(b/a>.25,'주변광이 남아야 합니다.');
    }else if(b<a*.95)receiverPixels++;
  }
  assert(largeGroundPixels>4000,'건물과 주민의 큰 지면 그림자가 있어야 합니다.');
  assert(receiverPixels>30,'다른 스프라이트도 큰 그림자를 받아야 합니다.');
  const casterGroundPixels={};
  for(const owner of new Set([...scene.casters.map(caster=>caster[0]),...scene.buildings.map(building=>building.owner)])){
    engine.init({scene:{...scene,contacts:[],casters:scene.casters.filter(caster=>caster[0]===owner),buildings:scene.buildings.filter(building=>building.owner===owner)},maps:raw.map(data=>data.buffer.slice(data.byteOffset,data.byteOffset+data.byteLength))});
    const individual=render({lamps:false});
    const individualClear=render({lamps:false,shadows:false});
    let pixels=0;
    for(let i=0;i<scene.width*scene.height;i++)if(raw[5][i*4]===0){
      const p=i*4,a=individualClear[p]+individualClear[p+1]+individualClear[p+2],b=individual[p]+individual[p+1]+individual[p+2];
      if(a>0&&b<a*.95)pixels++;
    }
    assert(pixels>40,'모든 건물·주민·나무·바위가 지면 그림자를 만들어야 합니다: '+owner);
    casterGroundPixels[owner]=pixels;
  }
  engine.init({scene,maps:raw.map(data=>data.buffer.slice(data.byteOffset,data.byteOffset+data.byteLength))});
  /* 차폐를 끄면 이전에 승인한 질감 조명과 결과가 같아야 합니다. */
  if(process.argv[3]){
    const oldSource=fs.readFileSync(process.argv[3],'utf8');
    const prior=new Function('self','performance',oldSource+'\nreturn {init,render};')({postMessage(){}},performance);
    prior.init({scene,maps:raw.slice(0,5).map(data=>data.buffer.slice(data.byteOffset,data.byteOffset+data.byteLength))});
    for(const changes of [{},{sun:false,cursor:true,ambient:.42},{sun:false,ambient:.17}]){
      const state={...base,...changes,shadows:false};
      assert.equal(Buffer.compare(Buffer.from(engine.render(state)),Buffer.from(prior.render(state))),0,'기존 질감 조명 보존');
    }
  }
  if(process.argv[4]){
    const priorSource=fs.readFileSync(process.argv[4],'utf8');
    const prior=new Function('self','performance',priorSource+'\nreturn {init,render};')({postMessage(){}},performance);
    const smallScene={...scene,buildings:[]};
    const copies=()=>raw.map(data=>data.buffer.slice(data.byteOffset,data.byteOffset+data.byteLength));
    engine.init({scene:smallScene,maps:copies()});prior.init({scene:smallScene,maps:copies()});
    for(const changes of [{},{elevation:69},{sun:false,cursor:true,ambient:.42,position:[279,309,95]}]){
      const state={...base,...changes};
      assert.equal(Buffer.compare(Buffer.from(engine.render(state)),Buffer.from(prior.render(state))),0,'작은 물체의 기존 그림자와 표면 조명을 보존합니다.');
    }
    report.smallObjectShadows='exact';
  }
  Object.assign(report,{originalPixels:'exact',albedoPixels:'exact',largeGroundPixels,receiverPixels,minimumGroundRatio,casterGroundPixels});
}
console.log(JSON.stringify(report,null,2));
