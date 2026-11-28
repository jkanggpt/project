layout(local_size_x=8,local_size_y=8) in;
layout(rgba16f,binding=0) writeonly uniform image2D windImage;
layout(rgba16f,binding=1) writeonly uniform image2D waterImage;
uniform vec2 uMap;
uniform float uTime;
layout(std430,binding=3) readonly buffer Terrain { uint terrain[]; };
layout(std430,binding=0) readonly buffer WindActors { Caster windActors[]; };
uniform ivec2 uMapCells;
uniform int uTerrainReady,uWaterEnabled,uWindEnabled,uNatural;
uniform int uWindCasterCount;
float shoreDistance(vec2 world){
    if(uTerrainReady==0)return 32.0;
    ivec2 cell=ivec2(floor(world/16.0));
    float result=32.0;
    for(int y=-1;y<=1;y++)for(int x=-1;x<=1;x++){
        ivec2 at=cell+ivec2(x,y);
        if(any(lessThan(at,ivec2(0)))||any(greaterThanEqual(at,uMapCells)))continue;
        if((terrain[at.y*uMapCells.x+at.x]&2u)!=0u)continue;
        vec2 lo=vec2(at)*16.0,hi=lo+16.0;
        result=min(result,length(max(max(lo-world,world-hi),vec2(0))));
    }
    return result;
}
float wave(vec2 p) {
    vec2 bend=vec2(noiseValue(p*.019+vec2(uTime*.027,-uTime*.019)),noiseValue(p*.023+vec2(-uTime*.013,uTime*.021)))-.5;
    return (noiseValue(p/24.0+bend*.8+vec2(uTime*.13,uTime*.047))-.5)*.62+
           (noiseValue(p/61.0-bend+vec2(-uTime*.071,uTime*.089))-.5)*.95+
           (noiseValue(p/137.0+vec2(uTime*.047,-uTime*.063))-.5)*.15;
}
void main(){
    ivec2 p=ivec2(gl_GlobalInvocationID.xy);
    if(uWindEnabled!=0&&all(lessThan(p,imageSize(windImage)))){
        vec2 world=(vec2(p)+.5)/vec2(imageSize(windImage))*uMap;
        float a=noiseValue(world*.012+vec2(-uTime*.067,uTime*.013));
        float b=noiseValue(world*.041+vec2(-uTime*.11,-uTime*.043));
        vec2 wind=vec2(.55+a*.8,(b-.5)*.65);
        vec2 push=vec2(0);
        if(uNatural!=0)for(int i=0;i<uWindCasterCount;i++){
            Caster c=windActors[i];
            if(c.tags.y==0u||c.tags.z!=1u||c.extent.z>14.0||c.contactProfile.x<.5||c.contactProfile.z<=0.0)continue;
            vec2 d=world-c.contact.xy;float distance=length(d);
            if(distance<14.0)push+=d/max(1.0,distance)*(1.0-distance/14.0);
        }
        imageStore(windImage,p,vec4(wind,clamp(push,vec2(-1),vec2(1))));
    }
    if(uWaterEnabled!=0&&all(lessThan(p,imageSize(waterImage)))){
        vec2 world=(vec2(p)+.5)/vec2(imageSize(waterImage))*uMap;
        float h=wave(world),dx=wave(world+vec2(1,0))-wave(world-vec2(1,0)),dy=wave(world+vec2(0,1))-wave(world-vec2(0,1));
        float shore=uNatural!=0?32.0:shoreDistance(world),damping=smoothstep(0.0,12.0,shore);
        imageStore(waterImage,p,vec4(dx*1.6*damping,dy*1.6*damping,h*damping,clamp(shore/16.0,0.0,1.0)));
    }
}
