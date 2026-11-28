/* 원화 표면을 조명하고 건물 윤곽과 둥근 차폐 영역으로 그림자를 계산합니다. */
let scene,maps,normals,macro,heights,kinds,owners,casters,buildings;
const clamp=(v,a,b)=>Math.min(b,Math.max(a,v));
const byte=v=>clamp(Math.round(v),0,255);
const bayer=[0,8,2,10,12,4,14,6,3,11,1,9,15,7,13,5];
function boxPlanes([x0,y0,z0,x1,y1,z1]){
  return [[-1,0,0,-x0],[1,0,0,x1],[0,-1,0,-y0],[0,1,0,y1],[0,0,-1,-z0],[0,0,1,z1]];
}
function roofPlanes([x0,y0,x1,y1,eaves,ridge]){
  const middle=(y0+y1)/2,slope=(ridge-eaves)/((y1-y0)/2),length=Math.hypot(slope,1);
  return [[-1,0,0,-x0],[1,0,0,x1],[0,-1,0,-y0],[0,1,0,y1],[0,0,-1,-eaves],
    [0,slope/length,1/length,(ridge+slope*middle)/length],
    [0,-slope/length,1/length,(ridge-slope*middle)/length]];
}
function insideShape(planes,x,y,z){
  return planes.every(([nx,ny,nz,limit])=>nx*x+ny*y+nz*z<=limit);
}
function rayHits(planes,x,y,z,dx,dy,dz,margin=0){
  let enter=.0001,exit=.9999;
  for(const [nx,ny,nz,limit] of planes){
    const distance=limit+margin-nx*x-ny*y-nz*z,direction=nx*dx+ny*dy+nz*dz;
    if(Math.abs(direction)<1e-9){if(distance<0)return false;continue;}
    const t=distance/direction;
    if(direction<0)enter=Math.max(enter,t);else exit=Math.min(exit,t);
    if(enter>exit)return false;
  }
  return true;
}
function shapeCoverage(planes,x,y,z,dx,dy,dz,dither){
  const softness=1.25;
  if(!rayHits(planes,x,y,z,dx,dy,dz,softness))return 0;
  if(rayHits(planes,x,y,z,dx,dy,dz,-softness))return 1;
  /* 평면을 조금씩 넓혀 경계 거리를 구하므로 벽과 지붕의 직선이 유지됩니다. */
  let low=-softness,high=softness;
  for(let step=0;step<4;step++){
    const middle=(low+high)/2;
    if(rayHits(planes,x,y,z,dx,dy,dz,middle))high=middle;else low=middle;
  }
  let coverage=clamp(.5-(low+high)/(4*softness),0,1);
  coverage=coverage*coverage*(3-2*coverage);
  return clamp(Math.floor(coverage*8+dither)/8,0,1);
}
function init(data) {
  scene=data.scene;maps=data.maps.map(buffer=>new Uint8ClampedArray(buffer));
  const count=scene.width*scene.height;
  normals=new Float32Array(count*3);macro=new Float32Array(count*3);heights=new Float32Array(count);kinds=new Uint8Array(count);owners=new Uint8Array(count);
  casters=scene.casters.map(([owner,x,y,z,rx,ry,rz,opacity])=>({owner,x,y,z,ix:1/rx,iy:1/ry,iz:1/rz,opacity}));
  buildings=(scene.buildings||[]).map(building=>({...building,top:Math.max(building.roof[5],building.chimney[5]),shapes:[boxPlanes(building.walls),roofPlanes(building.roof),boxPlanes(building.chimney)]}));
  for(let i=0;i<count;i++){
    const p=i*4,k=i*3;
    let nx=maps[2][p]/127.5-1,ny=maps[2][p+1]/127.5-1,nz=maps[2][p+2]/127.5-1;
    const length=Math.hypot(nx,ny,nz)||1;
    normals[k]=nx/length;normals[k+1]=ny/length;normals[k+2]=nz/length;
    heights[i]=maps[3][p]+maps[3][p+1]/256;kinds[i]=maps[4][p];owners[i]=maps[5][p];
  }
  for(let y=0;y<scene.height;y++)for(let x=0;x<scene.width;x++){
    const i=y*scene.width+x,k=i*3;
    if(kinds[i]===4||kinds[i]===5){macro[k+2]=1;continue;}
    let nx=0,ny=0,nz=0;
    for(let dy=-3;dy<=3;dy++)for(let dx=-3;dx<=3;dx++){
      const xx=x+dx,yy=y+dy;
      if(xx<0||yy<0||xx>=scene.width||yy>=scene.height)continue;
      const j=yy*scene.width+xx,q=j*3;
      if(kinds[j]!==kinds[i]||normals[k]*normals[q]+normals[k+1]*normals[q+1]+normals[k+2]*normals[q+2]<.55)continue;
      nx+=normals[q];ny+=normals[q+1];nz+=normals[q+2];
    }
    const length=Math.hypot(nx,ny,nz)||1;
    macro[k]=nx/length;macro[k+1]=ny/length;macro[k+2]=nz/length;
  }
}
function contactShadow(x,y,dither){
  let coverage=0;
  for(const [cx,cy,rx,ry,,density] of scene.contacts){
    const distance=Math.hypot((x-cx)/rx,(y-cy)/ry);
    const edge=clamp((1.22-distance)/.65,0,1);
    coverage=Math.max(coverage,density*Math.floor(edge*5+dither*.5)/5);
  }
  for(const building of buildings){
    const [x0,y0,,x1,y1]=building.walls;
    const distance=Math.hypot(Math.max(x0-x,0,x-x1),Math.max(y0-y,0,y-y1));
    const edge=clamp(1-distance/2.5,0,1);
    coverage=Math.max(coverage,.24*Math.floor(edge*5+dither*.5)/5);
  }
  return 1-coverage;
}
function castVisibility(x,y,z,dx,dy,dz,receiverOwner,sourceOwner,dither){
  if(dx*dx+dy*dy+dz*dz<1e-8)return 1;
  let occlusion=0;
  for(const building of buildings){
    if(building.owner===receiverOwner||building.owner===sourceOwner)continue;
    /* 소속 등불과 내부로 이동한 시험 광원은 자체 건물 영역을 제외합니다. */
    if(z+dz<=building.top&&building.shapes.some(planes=>insideShape(planes,x+dx,y+dy,z+dz)))continue;
    let coverage=0;
    for(const planes of building.shapes)coverage=Math.max(coverage,shapeCoverage(planes,x,y,z,dx,dy,dz,dither));
    occlusion=Math.max(occlusion,coverage*building.opacity);
  }
  for(const caster of casters){
    if(caster.owner===receiverOwner||caster.owner===sourceOwner)continue;
    const qx=(x-caster.x)*caster.ix,qy=(y-caster.y)*caster.iy,qz=(z-caster.z)*caster.iz;
    const vx=dx*caster.ix,vy=dy*caster.iy,vz=dz*caster.iz;
    /* 이동광이 차폐 영역 안에 들어가면 해당 근사 영역은 제외합니다. */
    if((qx+vx)**2+(qy+vy)**2+(qz+vz)**2<1)continue;
    const length=vx*vx+vy*vy+vz*vz;
    if(length<1e-8)continue;
    const t=clamp(-(qx*vx+qy*vy+qz*vz)/length,.0001,.9999);
    const softness=.12+.30*t;
    const hx=qx+vx*t,hy=qy+vy*t,hz=qz+vz*t,distanceSquared=hx*hx+hy*hy+hz*hz;
    if(distanceSquared>=(1+softness)**2)continue;
    const distance=Math.sqrt(distanceSquared);
    let coverage=clamp((1+softness-distance)/(2*softness),0,1);
    coverage=coverage*coverage*(3-2*coverage);
    coverage=clamp(Math.floor(coverage*8+dither)/8,0,1);
    occlusion=Math.max(occlusion,coverage*caster.opacity);
  }
  return 1-occlusion;
}
function render(state){
  const count=scene.width*scene.height,output=new Uint8ClampedArray(count*4);
  const az=state.azimuth*Math.PI/180,el=state.elevation*Math.PI/180;
  const sx=Math.cos(az)*Math.cos(el),sy=Math.sin(az)*Math.cos(el),sz=Math.sin(el);
  const sun=state.sun?(state.elevation<25?[.72,.49,.30]:[.70,.67,.59]):[0,0,0];
  const ambient=state.ambient;
  const lights=[];
  const lampStrength=clamp((.55-state.ambient)/.4,0,1);
  if(state.lamps)for(let index=0;index<scene.lamps.length;index++)lights.push([...scene.lamps[index],1.12,.67,.28,.72*lampStrength,scene.lampOwners[index]]);
  if(state.cursor)lights.push([...state.position,230,1.12,1.04,.80,2.4,0]);
  for(let i=0;i<count;i++){
    const p=i*4,k=i*3,x=i%scene.width+.5,y=Math.floor(i/scene.width)+.5;
    output[p+3]=255;
    if(state.mode===0){output[p]=maps[0][p];output[p+1]=maps[0][p+1];output[p+2]=maps[0][p+2];continue;}
    if(state.mode===1){output[p]=maps[1][p];output[p+1]=maps[1][p+1];output[p+2]=maps[1][p+2];continue;}
    if(state.mode===3){output[p]=byte((normals[k]*.5+.5)*255);output[p+1]=byte((normals[k+1]*.5+.5)*255);output[p+2]=byte((normals[k+2]*.5+.5)*255);continue;}
    let nx=macro[k]+(normals[k]-macro[k])*state.detail,ny=macro[k+1]+(normals[k+1]-macro[k+1])*state.detail,nz=macro[k+2]+(normals[k+2]-macro[k+2])*state.detail;
    const length=Math.hypot(nx,ny,nz)||1;nx/=length;ny/=length;nz/=length;
    const z=heights[i],py=y+z*.9,dither=bayer[(Math.floor(y)%4)*4+Math.floor(x)%4]/16;
    let diffuse=Math.max(0,nx*sx+ny*sy+nz*sz);
    diffuse=Math.floor(diffuse*12+dither*.3)/12;
    const contact=state.shadows&&(kinds[i]===4||kinds[i]===5)?contactShadow(x,y,dither):1;
    const sunVisibility=state.shadows&&state.sun&&diffuse>0?castVisibility(x,py,z,sx*800,sy*800,sz*800,owners[i],0,dither):1;
    let rr=ambient*.97*contact+sun[0]*diffuse*sunVisibility,rg=ambient*contact+sun[1]*diffuse*sunVisibility,rb=ambient*.97*contact+sun[2]*diffuse*sunVisibility,specular=0;
    for(const light of lights){
      let dx=light[0]-x,dy=light[1]-py,dz=light[2]-z;
      const distance=Math.hypot(dx,dy,dz);
      if(distance>=light[3])continue;
      const visibility=state.shadows?castVisibility(x,py,z,dx,dy,dz,owners[i],light[8],dither):1;
      dx/=Math.max(distance,.01);dy/=Math.max(distance,.01);dz/=Math.max(distance,.01);
      let ndl=Math.max(0,nx*dx+ny*dy+nz*dz);
      ndl=Math.floor(ndl*12+dither*.3)/12;
      const attenuation=(1-distance/light[3])**2*light[7]*visibility;
      rr+=light[4]*ndl*attenuation;rg+=light[5]*ndl*attenuation;rb+=light[6]*ndl*attenuation;
      const hy=dy+Math.SQRT1_2,hz=dz+Math.SQRT1_2,hLength=Math.hypot(dx,hy,hz);
      const power=kinds[i]===3?48:kinds[i]===5?26:36;
      const highlight=Math.max(0,(nx*dx+ny*hy+nz*hz)/hLength)**power;
      specular+=Math.floor(highlight*4)/4*(kinds[i]===3?.012:.025)*attenuation;
    }
    const emission=maps[3][p+2]/255;
    const glow=(emission>.8?(state.lamps?.40:0):(state.windows?.28:0))*lampStrength;
    const er=glow*emission,eg=er*.68,eb=er*.25;
    output[p]=byte(maps[1][p]*Math.sqrt(clamp(rr,.035,1.55))+255*(specular+er));
    output[p+1]=byte(maps[1][p+1]*Math.sqrt(clamp(rg,.035,1.55))+255*(specular*.9+eg));
    output[p+2]=byte(maps[1][p+2]*Math.sqrt(clamp(rb,.035,1.55))+255*(specular*.7+eb));
  }
  return output;
}
self.onmessage=event=>{
  try{
    if(event.data.type==='init'){init(event.data);self.postMessage({type:'ready'});}
    else if(event.data.type==='render'){
      const start=performance.now(),pixels=render(event.data.state);
      self.postMessage({type:'frame',id:event.data.id,pixels:pixels.buffer,ms:performance.now()-start},[pixels.buffer]);
    }
  }catch(error){self.postMessage({type:'error',message:error.message});}
};
