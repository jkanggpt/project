layout(local_size_x=64) in;
layout(std430,binding=0) readonly buffer Terrain{uint terrain[];};
layout(std430,binding=1) writeonly buffer Grass{Sprite blades[];};
layout(std430,binding=2) buffer Command{uint vertices;uint instances;uint first;uint base;};
uniform ivec2 uMapCells;
uniform vec2 uOrigin,uViewport;
uniform float uScale,uDensity;
uniform int uLimit;
void main(){
    uint cell=gl_GlobalInvocationID.x;
    if(cell>=uint(uMapCells.x*uMapCells.y)||(terrain[cell]&1u)==0u)return;
    ivec2 tile=ivec2(int(cell)%uMapCells.x,int(cell)/uMapCells.x);
    vec2 tileWorld=vec2(tile)*16.0;
    vec2 end=uOrigin+uViewport/uScale;
    if(tileWorld.x+20<uOrigin.x||tileWorld.y+24<uOrigin.y||tileWorld.x>end.x+8||tileWorld.y>end.y+16)return;
    float clump=.42+noiseValue(tileWorld*.021)*.68;
    for(uint j=0u;j<6u;j++){
        uint seed=hashWord(cell*73856093u+j*19349663u);
        if(randomUnit(seed)>uDensity*clump*.72)continue;
        uint slot=atomicAdd(instances,1u);
        if(slot>=uint(uLimit)){atomicMin(instances,uint(uLimit));return;}
        vec2 root=tileWorld+vec2(randomUnit(seed+1u),randomUnit(seed+2u))*15.0;
        float h=2.8+randomUnit(seed+3u)*3.8,w=2.5+randomUnit(seed+4u)*2.4;
        Sprite s;
        s.destination=vec4(root.x-w*.5,root.y-h,w,h);s.source=vec4(float(seed%8u)/8.0,0,.125,.5);
        s.normalSource=vec4(0);s.tint=vec4(1);s.shape=vec4(root.y,h*.4,1,0);
        s.tags=uvec4(0,8,0,seed);blades[slot]=s;
        blades[slot].contactProfile=vec4(0);
    }
}
