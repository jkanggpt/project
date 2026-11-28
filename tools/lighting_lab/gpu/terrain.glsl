/* 같은 월드 좌표로 청크의 여유 영역과 본문을 계산합니다. */
layout(std430,binding=3) readonly buffer TerrainCells { uint terrainCells[]; };
layout(std430,binding=4) readonly buffer TerrainPages { uint terrainPages[]; };
uniform ivec2 uMapCells,uFieldChunks;
uniform sampler2D uTerrainFields;
uint terrainAt(ivec2 cell) {
    if(any(lessThan(cell,ivec2(0)))||any(greaterThanEqual(cell,uMapCells)))return 0u;
    return terrainCells[cell.y*uMapCells.x+cell.x];
}
uint terrainKnown(ivec2 cell,uint fallback){
    uint flags=terrainAt(cell);return (flags&128u)!=0u?fallback:flags;
}
float segmentDistance(vec2 p,vec2 a,vec2 b) {
    vec2 d=b-a;
    return length(p-a-d*clamp(dot(p-a,d)/max(dot(d,d),.001),0.0,1.0));
}
void curvedBoundary(vec2 p,vec2 a,vec2 b,vec2 control,inout float best){
    vec2 previous=a;
    for(int i=1;i<=6;i++){
        float t=float(i)/6.0;
        vec2 next=mix(mix(a,control,t),mix(control,b,t),t);
        float distance=segmentDistance(p,previous,next);
        if(distance<best){
            best=distance;
        }
        previous=next;
    }
}
/* 이웃 등고선과 접선이 이어지는 곡선으로 모서리를 둥글게 깎습니다. */
float terrainDistance(vec2 world,uint bit) {
    ivec2 cell=ivec2(floor(world/16.0-.5));
    float best=32.0;
    uint fallback=terrainAt(ivec2(floor(world/16.0)));
    if((fallback&128u)!=0u)fallback=0u;
    bool inside=(terrainKnown(ivec2(floor(world/16.0)),fallback)&bit)!=0u;
    for(int y=-2;y<=2;y++)for(int x=-2;x<=2;x++){
        ivec2 q=cell+ivec2(x,y);
        vec2 base=(vec2(q)+.5)*16.0;
        if(length(max(max(base-world,world-(base+16.0)),vec2(0)))>best)continue;
        bvec4 c=bvec4((terrainKnown(q,fallback)&bit)!=0u,(terrainKnown(q+ivec2(1,0),fallback)&bit)!=0u,
                       (terrainKnown(q+ivec2(1,1),fallback)&bit)!=0u,(terrainKnown(q+ivec2(0,1),fallback)&bit)!=0u);
        vec2 edge[4];int count=0;
        if(c.x!=c.y)edge[count++]=base+vec2(8,0);
        if(c.y!=c.z)edge[count++]=base+vec2(16,8);
        if(c.z!=c.w)edge[count++]=base+vec2(8,16);
        if(c.w!=c.x)edge[count++]=base+vec2(0,8);
        vec2 control=base+8.0;
        if(count==2){
            curvedBoundary(world,edge[0],edge[1],control,best);
        }
        if(count==4){
            if(c.x){
                curvedBoundary(world,edge[0],edge[3],control,best);
                curvedBoundary(world,edge[1],edge[2],control,best);
            }else{
                curvedBoundary(world,edge[0],edge[1],control,best);
                curvedBoundary(world,edge[2],edge[3],control,best);
            }
        }
    }
    bvec4 center=bvec4((terrainKnown(cell,fallback)&bit)!=0u,(terrainKnown(cell+ivec2(1,0),fallback)&bit)!=0u,
        (terrainKnown(cell+ivec2(1,1),fallback)&bit)!=0u,(terrainKnown(cell+ivec2(0,1),fallback)&bit)!=0u);
    int occupied=int(center.x)+int(center.y)+int(center.z)+int(center.w);
    vec2 f=(world-(vec2(cell)+.5)*16.0)/8.0;
    bool corner[4];
    const vec2 corners[4]=vec2[4](vec2(0),vec2(2,0),vec2(2),vec2(0,2));
    for(int k=0;k<4;k++){
        vec2 d=abs(f-corners[k]);
        corner[k]=all(lessThanEqual(d,vec2(1)))&&
            sqrt(max(0.0,1.0-d.x))+sqrt(max(0.0,1.0-d.y))>=1.0;
    }
    if(occupied==0)inside=false;
    else if(occupied==4)inside=true;
    else if(occupied==1)inside=(center.x&&corner[0])||(center.y&&corner[1])||(center.z&&corner[2])||(center.w&&corner[3]);
    else if(occupied==3)inside=!((!center.x&&corner[0])||(!center.y&&corner[1])||(!center.z&&corner[2])||(!center.w&&corner[3]));
    else if(center.x==center.z)inside=center.x?(corner[0]||corner[2]):(corner[1]||corner[3]);
    else{
        vec2 blend=clamp(f*.5,vec2(0),vec2(1));
        float fill=mix(mix(center.x?1.0:0.0,center.y?1.0:0.0,blend.x),
                       mix(center.w?1.0:0.0,center.z?1.0:0.0,blend.x),blend.y);
        inside=fill>=.5;
    }
    return inside?-best:best;
}
vec4 terrainRaw(vec2 world) {
    uint flags=terrainAt(ivec2(floor(world/16.0)));
    vec2 warp=vec2(noiseValue(world/19.0),noiseValue(world/23.0+19.0))-.5;
    if((flags&64u)!=0u)warp=vec2(0);
    vec2 p=world+warp*3.5;
    ivec2 cell=ivec2(floor(world/16.0-.5));vec2 f=fract(world/16.0-.5);
    float road=0.0,stone=0.0;
    uint fallback=(flags&128u)!=0u?0u:flags;
    for(int y=0;y<2;y++)for(int x=0;x<2;x++){
        uint at=terrainKnown(cell+ivec2(x,y),fallback);
        float weight=(x==0?1.0-f.x:f.x)*(y==0?1.0-f.y:f.y);
        if((at&4u)!=0u){
            road+=weight;
            stone+=weight*((at&1u)!=0u?0.0:(at&16u)!=0u?1.4:1.0);
        }
    }
    return vec4(terrainDistance(p,2u),terrainDistance(p,4u),
                terrainDistance(p,8u),road>.001?stone/road:1.0);
}
vec4 terrainField(vec2 world) {
    ivec2 chunk=ivec2(floor(world/128.0));
    if(all(greaterThanEqual(chunk,ivec2(0)))&&all(lessThan(chunk,uFieldChunks))){
        uint page=terrainPages[chunk.y*uFieldChunks.x+chunk.x];
        if(page!=0u){
            int slot=int(page)-1;
            ivec2 at=ivec2(slot%8,slot/8)*192+ivec2(floor(world))-chunk*128+32;
            return texelFetch(uTerrainFields,at,0);
        }
    }
    /* 한 화면에 64청크보다 많이 보이면 같은 식으로 계산하며 자료를 버리지 않습니다. */
    return terrainRaw(floor(world)+.5);
}
