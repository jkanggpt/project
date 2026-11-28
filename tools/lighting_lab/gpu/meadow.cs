/* 계산·지역 합·그룹 합·채우기를 나누어 실행 순서와 무관한 배치를 만듭니다. */
layout(local_size_x=64) in;
layout(std430,binding=0) buffer Counts { uint counts[]; };
layout(std430,binding=1) buffer Offsets { uint offsets[]; };
layout(std430,binding=2) buffer Command { uint vertices;uint instances;uint first;uint base; };
layout(std430,binding=5) buffer Sums { uint sums[]; };
layout(std430,binding=6) buffer GroupOffsets { uint groupOffsets[]; };
layout(std430,binding=7) writeonly buffer Grass { Sprite blades[]; };
uniform vec2 uOrigin,uViewport;
uniform float uScale,uDensity;
uniform int uPass,uLimit;
shared uint scan[64];
shared uint tieScan[64];
uint rootSeed(ivec2 cell,uint j){return hashWord(hashCell(cell)^((j+1u)*19349663u));}
vec2 rootAt(ivec2 cell,uint j,uint seed){
    vec2 site=vec2(float(j%6u),float(j/6u));
    return floor(vec2(cell)*16.0+(site+vec2(.2)+vec2(randomUnit(seed+1u),randomUnit(seed+2u))*.6)*(16.0/6.0)+.5);
}
bool rootVisible(vec2 root,uint seed,out vec4 field){
    vec2 end=uOrigin+uViewport/uScale;
    if(root.x<uOrigin.x-10.0||root.y<uOrigin.y-1.0||root.x>end.x+10.0||root.y>end.y+25.0)return false;
    uint flags=terrainAt(ivec2(floor(root/16.0)));
    if((flags&1u)==0u||(flags&(128u|16u))!=0u)return false;
    field=terrainField(root+.5);
    if(field.x<1.5||field.y<2.5||field.z<1.0)return false;
    float cluster=noiseValue(root/47.0),gap=noiseValue(root/21.0+vec2(13,9));
    float cover=smoothstep(.36,.65,cluster);
    float density=mix(.018,1.12,cover*cover)*(.42+gap*.66);
    return randomUnit(seed)<uDensity*density;
}
void main(){
    uint cell=gl_GlobalInvocationID.x,lane=gl_LocalInvocationID.x;
    uint cellCount=uint(uMapCells.x*uMapCells.y),groups=(cellCount+63u)/64u;
    uint histogram=groups*2u;
    if(uPass==0){if(cell<258u)sums[histogram+cell]=0u;return;}
    if(uPass==2){
        if(cell!=0u)return;
        uint below=0u,cutoff=256u,remaining=0u;
        for(uint i=0u;i<256u;i++){
            uint count=sums[histogram+i];
            if(below+count>uint(uLimit)){cutoff=i;remaining=uint(uLimit)-below;break;}
            below+=count;
        }
        sums[histogram+256u]=cutoff;sums[histogram+257u]=remaining;
        return;
    }
    if(uPass==5){
        if(cell>=groups)return;
        uint below=0u,ties=0u;
        for(uint i=0u;i<cell;i++){below+=sums[i];ties+=sums[groups+i];}
        groupOffsets[cell]=below;groupOffsets[groups+cell]=ties;
        if(cell+1u==groups){
            uint total=below+sums[cell];
            instances=total+min(sums[histogram+257u],ties+sums[groups+cell]);
        }
        return;
    }
    if(uPass==4){
        scan[lane]=cell<cellCount?counts[cell]:0u;barrier();
        tieScan[lane]=cell<cellCount?counts[cellCount+cell]:0u;barrier();
        for(uint step=1u;step<64u;step*=2u){
            uint add=lane>=step?scan[lane-step]:0u;
            uint addTie=lane>=step?tieScan[lane-step]:0u;barrier();
            scan[lane]+=add;tieScan[lane]+=addTie;barrier();
        }
        if(cell<cellCount){
            offsets[cell]=scan[lane]-counts[cell];
            offsets[cellCount+cell]=tieScan[lane]-counts[cellCount+cell];
        }
        if(lane==63u){sums[gl_WorkGroupID.x]=scan[lane];sums[groups+gl_WorkGroupID.x]=tieScan[lane];}
        return;
    }
    if(cell>=cellCount)return;
    ivec2 tile=ivec2(int(cell)%uMapCells.x,int(cell)/uMapCells.x);
    vec2 tileWorld=vec2(tile)*16.0,end=uOrigin+uViewport/uScale;
    bool visible=tileWorld.x+26>=uOrigin.x&&tileWorld.y+16>=uOrigin.y&&tileWorld.x<=end.x+10&&tileWorld.y<=end.y+25;
    if(!visible||(terrainCells[cell]&1u)==0u){if(uPass==3){counts[cell]=0u;counts[cellCount+cell]=0u;}return;}
    uint below=0u,ties=0u;
    uint cutoff=uPass>2?sums[histogram+256u]:256u;
    uint totalBelow=uPass==6?groupOffsets[groups-1u]+sums[groups-1u]:0u;
    for(uint j=0u;j<36u;j++){
        uint seed=rootSeed(tile,j);vec2 root=rootAt(tile,j,seed);vec4 field;
        if(!rootVisible(root,seed,field))continue;
        uint priority=hashWord(seed+777u)>>24;
        if(uPass==1){atomicAdd(sums[histogram+priority],1u);continue;}
        if(priority>cutoff)continue;
        uint localRank=priority==cutoff?ties++:below++;
        if(uPass==3)continue;
        uint slot;
        if(priority==cutoff){
            uint rank=groupOffsets[groups+cell/64u]+offsets[cellCount+cell]+localRank;
            if(rank>=sums[histogram+257u])continue;
            slot=totalBelow+rank;
        }else slot=groupOffsets[cell/64u]+offsets[cell]+localRank;
        float cluster=noiseValue(root/53.0+7.0),variation=randomUnit(seed+3u);
        int species=cluster<.27?0:cluster<.54?1:cluster<.70?2:3;
        if(field.x<12.0&&noiseValue(root/29.0)>.48)species=4;
        float h=species==0?3.0+variation*3.0:species==1?5.6+variation*7.6:
                species==2?11.0+variation*7.0:species==3?5.0+variation*6.0:14.0+variation*10.0;
        float w=species==0?6.0:species==2?9.0:species==3?10.0:7.0;
        h=floor(h+.5);
        Sprite s;
        s.destination=vec4(root.x-floor(w*.5),root.y-h,w,h);s.source=vec4(0,0,1,1);
        s.normalSource=vec4(0);s.tint=vec4(1);s.shape=vec4(root.y,h*.45,float(species),noiseValue(root/67.0));
        s.tags=uvec4(0,8,0,seed);s.contactProfile=vec4(0);blades[slot]=s;
    }
    if(uPass==3){counts[cell]=below;counts[cellCount+cell]=ties;}
}
