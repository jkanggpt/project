/* GPU가 없는 대화 미리보기에서 같은 표면·차폐·조명 계산을 수행합니다. */
let scene, maps, cache;
const clamp = value => Math.min(255, Math.max(0, Math.round(value * 255)));
function prepare(data) {
  scene = data.scene;
  maps = data.maps.map(buffer => new Uint8ClampedArray(buffer));
  const count = scene.width * scene.height;
  cache = new Float32Array(count * 15);
  for (let i = 0; i < count; i++) {
    const p = i * 4, k = i * 15;
    if (maps[0][p+3] < 128) continue;
    const height = maps[2][p] + maps[2][p+1] / 256;
    const x = scene.origin[0] + (i % scene.width + .5) / scene.width * scene.size[0];
    const y = scene.origin[1] + (Math.floor(i / scene.width) + .5) / scene.height * scene.size[1] + height;
    let nx = maps[1][p]/127.5-1, ny = maps[1][p+1]/127.5-1, nz = maps[1][p+2]/127.5-1;
    const length = Math.hypot(nx, ny, nz);
    nx /= length; ny /= length; nz /= length;
    cache[k] = x; cache[k+1] = y; cache[k+2] = height;
    cache[k+3] = nx; cache[k+4] = ny; cache[k+5] = nz;
    cache[k+6] = (maps[0][p]/255)**2.2;
    cache[k+7] = (maps[0][p+1]/255)**2.2;
    cache[k+8] = (maps[0][p+2]/255)**2.2;
    cache[k+9] = maps[3][p]/255;
    cache[k+10] = maps[2][p+2]/255;
    if (cache[k+9] < .15) {
      nx += .04*Math.sin(x*.4); ny += .04*Math.cos(y*.3);
      const adjusted = Math.hypot(nx,ny,nz);
      nx /= adjusted; ny /= adjusted; nz /= adjusted;
    }
    cache[k+11] = nx; cache[k+12] = ny; cache[k+13] = nz;
    cache[k+14] = 1;
  }
}
function visible(x,y,z,dx,dy,dz,maximum) {
  const centers = scene.centers, extents = scene.extents;
  const ix = 1/dx, iy = 1/dy, iz = 1/dz;
  for (let i = 0; i < scene.count*4; i += 4) {
    const px=x-centers[i], py=y-centers[i+1], pz=z-centers[i+2];
    const ex=extents[i], ey=extents[i+1], ez=extents[i+2], shape=centers[i+3];
    if (shape === 1) {
      const qx=px/ex, qy=py/ey, qz=pz/ez, rx=dx/ex, ry=dy/ey, rz=dz/ez;
      const aa=rx*rx+ry*ry+rz*rz, bb=qx*rx+qy*ry+qz*rz, cc=qx*qx+qy*qy+qz*qz-1;
      const discriminant=bb*bb-aa*cc;
      if (discriminant < 0) continue;
      const root=Math.sqrt(discriminant), first=(-bb-root)/aa, last=(-bb+root)/aa;
      if (last>Math.max(first,.02) && first<maximum && last>.02) return 0;
      continue;
    }
    let first=-100000, last=maximum;
    if (Math.abs(dx)<1e-7) { if(Math.abs(px)>ex)continue; }
    else {
      const a=(-ex-px)*ix,b=(ex-px)*ix;
      first=Math.max(first,Math.min(a,b));last=Math.min(last,Math.max(a,b));
      if(first>last)continue;
    }
    if (Math.abs(dy)<1e-7) { if(Math.abs(py)>ey)continue; }
    else {
      const a=(-ey-py)*iy,b=(ey-py)*iy;
      first=Math.max(first,Math.min(a,b));last=Math.min(last,Math.max(a,b));
      if(first>last)continue;
    }
    if (shape === 0) {
      if (Math.abs(dz)<1e-7) { if(Math.abs(pz)>ez)continue; }
      else {
        const a=(-ez-pz)*iz,b=(ez-pz)*iz;
        first=Math.max(first,Math.min(a,b));last=Math.min(last,Math.max(a,b));
      }
    } else {
      /* 박공지붕은 바닥 반공간과 양쪽 경사 반공간의 교집합입니다. */
      const slope=ez/ex;
      let a=-dz,b=1+pz;
      if(Math.abs(a)<1e-7){if(b<0)continue;}
      else if(a<0)first=Math.max(first,b/a);else last=Math.min(last,b/a);
      a=slope*dx+dz;b=ez-slope*px-pz;
      if(Math.abs(a)<1e-7){if(b<0)continue;}
      else if(a<0)first=Math.max(first,b/a);else last=Math.min(last,b/a);
      a=-slope*dx+dz;b=ez+slope*px-pz;
      if(Math.abs(a)<1e-7){if(b<0)continue;}
      else if(a<0)first=Math.max(first,b/a);else last=Math.min(last,b/a);
    }
    if(last>Math.max(first,.02) && first<maximum && last>.02) return 0;
  }
  return 1;
}
function lighting(state) {
  const output = new Uint8ClampedArray(scene.width*scene.height*4);
  const az=state.azimuth*Math.PI/180, el=state.elevation*Math.PI/180;
  const sx=Math.cos(az)*Math.cos(el), sy=Math.sin(az)*Math.cos(el), sz=Math.sin(el);
  const sun=state.sun?(state.elevation<25?[2.3,1.1,.45]:[1.65,1.55,1.35]):[0,0,0];
  const lights=[112,124,24,100, 49.5,73,18,92, 168,72,20,84, ...state.position,90];
  const colors=[2.1,.82,.22,state.torch?3.2:0, 2.1,1.05,.4,state.interior?2.4:0,
    1.8,.85,.25,state.interior?2.2:0, .48,1.1,2,state.cursor?3:0];
  const shadows=state.mode===2 || state.mode===5;
  for (let i=0;i<scene.width*scene.height;i++) {
    const p=i*4,k=i*15;
    output[p+3]=255;
    if (!cache[k+14]) { output[p]=6;output[p+1]=10;output[p+2]=15;continue; }
    if (state.mode===0) { output[p]=maps[0][p];output[p+1]=maps[0][p+1];output[p+2]=maps[0][p+2];continue; }
    if (state.mode===3) {
      output[p]=clamp(cache[k+3]*.5+.5);output[p+1]=clamp(cache[k+4]*.5+.5);output[p+2]=clamp(cache[k+5]*.5+.5);continue;
    }
    if (state.mode===4) {
      const h=cache[k+2];output[p]=clamp(h/48);output[p+1]=clamp(.15+h/64);output[p+2]=clamp(.45-h/128);continue;
    }
    const x=cache[k],y=cache[k+1],z=cache[k+2],nx=cache[k+11],ny=cache[k+12],nz=cache[k+13];
    const ox=x+nx*.48,oy=y+ny*.48,oz=z+nz*.48,rough=cache[k+9];
    const ambient=state.ambient*(.55+.45*Math.max(nz,0));
    let rr=ambient*.68,rg=ambient*.82,rb=ambient;
    const diffuse=Math.max(0,nx*sx+ny*sy+nz*sz);
    const sunVisible=shadows && state.sun && (diffuse>0 || state.mode===5)?visible(ox,oy,oz,sx,sy,sz,511.8):1;
    let occlusion=sunVisible;
    let hx=sx,hy=sy+Math.SQRT1_2,hz=sz+Math.SQRT1_2,hLength=Math.hypot(hx,hy,hz);
    const power=120+(12-120)*rough,reflectance=.38+(.025-.38)*rough;
    let spec=diffuse>=.001?Math.max(0,(nx*hx+ny*hy+nz*hz)/hLength)**power*reflectance:0;
    let contribution=sunVisible*(diffuse+spec);
    rr+=sun[0]*contribution;rg+=sun[1]*contribution;rb+=sun[2]*contribution;
    for(let l=0;l<16;l+=4) {
      if(colors[l+3]<=0)continue;
      let dx=lights[l]-x,dy=lights[l+1]-y,dz=lights[l+2]-z;
      const distance=Math.hypot(dx,dy,dz),radius=lights[l+3];
      if(distance>=radius)continue;
      const divisor=Math.max(distance,.01);dx/=divisor;dy/=divisor;dz/=divisor;
      const ndl=Math.max(0,nx*dx+ny*dy+nz*dz);
      const visibility=shadows && (ndl>0 || state.mode===5)?visible(ox,oy,oz,dx,dy,dz,distance-.2):1;
      occlusion=Math.min(occlusion,visibility);
      hx=dx;hy=dy+Math.SQRT1_2;hz=dz+Math.SQRT1_2;hLength=Math.hypot(hx,hy,hz);
      spec=ndl>=.001?Math.max(0,(nx*hx+ny*hy+nz*hz)/hLength)**power*reflectance:0;
      const attenuation=(1-distance/radius)**2*colors[l+3];
      contribution=visibility*(ndl+spec)*attenuation;
      rr+=colors[l]*contribution;rg+=colors[l+1]*contribution;rb+=colors[l+2]*contribution;
    }
    if(state.mode===5){output[p]=output[p+1]=output[p+2]=clamp(.1+.8*occlusion);continue;}
    const emission=cache[k+10]*(cache[k+10]>.8?(state.torch?3.5:0):(state.interior?2.4:0));
    output[p]=clamp((1-Math.exp(-cache[k+6]*(rr+emission)*1.1))**(1/2.2));
    output[p+1]=clamp((1-Math.exp(-cache[k+7]*(rg+emission)*1.1))**(1/2.2));
    output[p+2]=clamp((1-Math.exp(-cache[k+8]*(rb+emission)*1.1))**(1/2.2));
  }
  return output;
}
self.onmessage = event => {
  try {
    if(event.data.type==='init'){prepare(event.data);self.postMessage({type:'ready'});}
    else if(event.data.type==='render'){
      const start=performance.now(),pixels=lighting(event.data.state);
      self.postMessage({type:'frame',id:event.data.id,pixels:pixels.buffer,ms:performance.now()-start},[pixels.buffer]);
    }
  } catch(problem) {self.postMessage({type:'error',message:problem.message});}
};
