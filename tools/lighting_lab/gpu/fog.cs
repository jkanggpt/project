/* 시야 바깥과 여유 한 칸은 불투명 구름으로 덮고 안쪽 경계만 풀어 줍니다. */
layout(local_size_x=8,local_size_y=8) in;
layout(std430,binding=3) readonly buffer FogVisibility { uint fogVisibility[]; };
layout(rgba8,binding=0) writeonly uniform image2D outputImage;
uniform ivec2 uMapCells;
uniform vec2 uViewport,uOrigin;
uniform float uScale,uTime,uAmbient;

bool fogHidden(ivec2 cell){
    if(any(lessThan(cell,ivec2(0)))||any(greaterThanEqual(cell,uMapCells)))return true;
    return fogVisibility[cell.y*uMapCells.x+cell.x]<2u;
}
float fogInsideDistance(vec2 world){
    ivec2 cell=ivec2(floor(world/16.0));
    if(fogHidden(cell))return 0.0;
    float nearest=32.0;
    for(int y=-1;y<=1;y++)for(int x=-1;x<=1;x++){
        ivec2 q=cell+ivec2(x,y);
        if(!fogHidden(q))continue;
        vec2 low=vec2(q)*16.0,high=low+16.0;
        vec2 d=max(max(low-world,world-high),vec2(0));
        nearest=min(nearest,length(d));
    }
    return nearest;
}
float fogVisibilityBlend(vec2 world){
    vec2 p=world/16.0-.5;
    ivec2 cell=ivec2(floor(p));
    vec2 f=fract(p),one=1.0-f;
    vec4 wx=vec4(one.x*one.x*one.x,3.0*f.x*f.x*f.x-6.0*f.x*f.x+4.0,
                 -3.0*f.x*f.x*f.x+3.0*f.x*f.x+3.0*f.x+1.0,f.x*f.x*f.x)/6.0;
    vec4 wy=vec4(one.y*one.y*one.y,3.0*f.y*f.y*f.y-6.0*f.y*f.y+4.0,
                 -3.0*f.y*f.y*f.y+3.0*f.y*f.y+3.0*f.y+1.0,f.y*f.y*f.y)/6.0;
    float clear=0.0;
    for(int y=0;y<4;y++)for(int x=0;x<4;x++)
        if(!fogHidden(cell+ivec2(x-1,y-1)))clear+=wx[x]*wy[y];
    return clear;
}
float cloudLobes(vec2 p){
    vec2 at=p/vec2(49,43);
    ivec2 cell=ivec2(floor(at));
    float nearest=8.0;
    for(int y=-1;y<=1;y++)for(int x=-1;x<=1;x++){
        ivec2 q=cell+ivec2(x,y);uint seed=hashCell(q);
        vec2 center=vec2(q)+vec2(.12)+vec2(randomUnit(seed),randomUnit(seed+81u))*.76;
        float radius=.51+randomUnit(seed+32u)*.37;
        nearest=min(nearest,length((at-center)*vec2(1,.91))/radius);
    }
    return 1.0-smoothstep(.13,1.45,nearest);
}
void main(){
    ivec2 pixel=ivec2(gl_GlobalInvocationID.xy);
    if(any(greaterThanEqual(pixel,imageSize(outputImage))))return;
    vec2 world=floor(uOrigin+vec2(float(pixel.x)+.5,uViewport.y-float(pixel.y)-.5)/max(uScale,.001))+.5;
    float distance=fogInsideDistance(world);
    if(distance>=14.0){imageStore(outputImage,pixel,vec4(0));return;}
    vec2 drift=vec2(uTime*.58,-uTime*.23);
    vec2 warp=vec2(noiseValue(world/103.0+drift*.011),noiseValue(world/79.0-drift*.014))-.5;
    vec2 p=world+warp*19.0;
    float broad=cloudLobes(p+drift);
    float rolling=noiseValue(p/31.0-drift*.038);
    float detail=noiseValue(p/9.0+drift*.057);
    float width=6.0+8.0*(broad*.66+rolling*.34);
    float alpha=distance<=0.0?1.0:1.0-smoothstep(0.0,width,distance);
    /* 보간한 시야 등고선이 한 칸씩 튀어나온 모서리를 구름 안으로 감춥니다. */
    if(distance>0.0)alpha=max(alpha,1.0-smoothstep(.52,.93,fogVisibilityBlend(world)));
    float shade=clamp(.16+broad*.52+rolling*.21+detail*.11,0.0,1.0);
    shade=floor(shade*7.0)/7.0;
    float day=clamp((uAmbient-.16)/.40,0.0,1.0);
    vec3 low=mix(vec3(.14,.19,.25),vec3(.29,.36,.38),day);
    vec3 high=mix(vec3(.31,.39,.46),vec3(.63,.69,.66),day);
    vec3 color=mix(low,high,shade);
    imageStore(outputImage,pixel,vec4(color,alpha));
}
