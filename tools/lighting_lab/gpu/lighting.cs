layout(local_size_x=8,local_size_y=8) in;
layout(rgba8,binding=0) writeonly uniform image2D outputImage;
uniform sampler2D uAlbedo,uNormalHeight,uMaterial,uOwner,uSunCache,uPointCache;
uniform vec2 uOrigin,uViewport,uMap;
uniform float uScale,uAmbient,uElevation;
uniform int uShadows,uView,uLightTileStride;
uniform float uTime,uOvercast;
uniform int uWater;
layout(std430,binding=3) readonly buffer LightTiles {uint lightWords[];};
uint ownerAt(ivec2 p){uvec4 b=uvec4(round(texelFetch(uOwner,p,0)*255.0));return b.x|(b.y<<8)|(b.z<<16)|(b.w<<24);}
float visible(vec3 world,vec2 screen,int index,uint owner,float dither){
    if(uShadows==0)return 1.0;
    if(index>=0&&((lights[index].tags.z&1u)!=0u||index>=16))
        return localLightVisibility(world,screen,index,owner,dither);
    bool ground=owner==0u&&world.z<.01;
    if(!ground)return shadowVisibility(world,screen,index,2,owner,dither);
    float fixedVisibility=1.0;
    ivec2 at=ivec2(floor(world.xy));
    if(all(greaterThanEqual(at,ivec2(0)))&&all(lessThan(at,ivec2(uMap)))){
        if(index<0)fixedVisibility=texelFetch(uSunCache,at,0).r;
        else if(index<4)fixedVisibility=texelFetch(uPointCache,at,0)[index];
        else fixedVisibility=shadowVisibility(world,screen,index,0,0u,dither);
    }
    return min(fixedVisibility,shadowVisibility(world,screen,index,1,0u,dither));
}
void main(){
    ivec2 pixel=ivec2(gl_GlobalInvocationID.xy);
    if(any(greaterThanEqual(pixel,ivec2(uViewport))))return;
    vec4 albedo=texelFetch(uAlbedo,pixel,0);
    if(albedo.a<.05){imageStore(outputImage,pixel,vec4(.07,.09,.10,1));return;}
    vec4 nh=texelFetch(uNormalHeight,pixel,0),material=texelFetch(uMaterial,pixel,0);
    vec3 normal=normalize(nh.xyz);
    int kind=int(round(material.r*255.0));
    if(uView==1){imageStore(outputImage,pixel,vec4(albedo.rgb,1));return;}
    if(uView==2){imageStore(outputImage,pixel,vec4(normal*.5+.5,1));return;}
    if(kind==0){imageStore(outputImage,pixel,vec4(albedo.rgb,1));return;}
    vec2 screen=uOrigin+vec2(float(pixel.x)+.5,uViewport.y-float(pixel.y)-.5)/uScale;
    vec3 world=vec3(screen.x,screen.y+nh.w*.9,nh.w);
    uint owner=ownerAt(pixel);
    float dither=bayerValue(ivec2(floor(screen)));
    float contact=uShadows!=0&&owner==0u?contactVisibility(world.xy,dither):1.0;
    vec3 radiance=vec3(.97,1,.97)*uAmbient*contact*clamp(material.w,.30,1.0);
    float ndl=max(0.0,dot(normal,uSunDirection));ndl=floor(ndl*12.0+dither*.3)/12.0;
    float sunVisibility=1.0;
    if(uSunEnabled!=0){
        sunVisibility=ndl>0.0?visible(world,screen,-1,owner,dither):1.0;
        vec3 color=uElevation<25.0?vec3(.72,.49,.30):vec3(.70,.67,.59);
        radiance+=color*ndl*sunVisibility;
    }
    float specular=0.0,localVisibility=1.0;
    ivec2 tile=ivec2(floor((screen-uTileOrigin)/16.0));
    bool tiled=all(greaterThanEqual(tile,ivec2(0)))&&all(lessThan(tile,uTileSize));
    int lightOffset=tiled?(tile.y*uTileSize.x+tile.x)*uLightTileStride:0;
    int lightCount=tiled?int(lightWords[lightOffset]):uLightCount;
    for(int item=0;item<lightCount;item++){
        int k=tiled?int(lightWords[lightOffset+1+item]):item;
        Light light=lights[k];vec3 direction=light.position.xyz-world;float range=length(direction);
        if(range>=light.position.w)continue;
        float visibility=visible(world,screen,k,owner,dither);
        localVisibility=min(localVisibility,visibility);
        direction/=max(.01,range);
        float diffuse=max(0.0,dot(normal,direction));diffuse=floor(diffuse*12.0+dither*.3)/12.0;
        float attenuation=pow(1.0-range/light.position.w,2.0)*light.color.w*visibility;
        radiance+=light.color.rgb*diffuse*attenuation;
        vec3 halfway=normalize(direction+vec3(0,.70710678,.70710678));
        float roughness=clamp(material.z,.10,1.0);
        float power=mix(76.0,12.0,roughness);
        specular+=floor(pow(max(0.0,dot(normal,halfway)),power)*4.0)/4.0*(.012+(1.0-roughness)*.065)*attenuation;
    }
    float lampStrength=clamp((.55-uAmbient)/.4,0.0,1.0);
    vec3 emissionHue=albedo.rgb/max(.08,max(albedo.r,max(albedo.g,albedo.b)));
    vec3 emission=emissionHue*material.g*(material.g>.8?.40:.28)*lampStrength;
    vec3 color=albedo.rgb*sqrt(clamp(radiance,vec3(.035),vec3(1.55)))+specular*vec3(1,.9,.7)+emission;
    vec3 sunReflection=vec3(0);
    if(kind==7&&uSunEnabled!=0){
        /* 고정 시점·햇빛 방향·물결 법선으로 반짝임을 계산합니다. 미세면은 세계 화소 칸(3×1)에 고정되어
           타일 경계와 화면 이동에 따라 움직이지 않고, 물결 효과가 꺼지면 시간도 멈춥니다. */
        vec3 viewDirection=vec3(0,.70710678,.70710678);
        vec3 halfway=normalize(uSunDirection+viewDirection);
        ivec2 facetCell=ivec2(floor(screen/vec2(3.0,1.0)));
        uint facetSeed=hashCell(facetCell+ivec2(9157,-311));
        float clock=uWater!=0?uTime:0.0;
        float phase=floor(clock*(3.0+randomUnit(facetSeed)*4.0)+randomUnit(facetSeed+7u)*8.0);
        uint twinkle=hashWord(facetSeed^uint(int(phase))*0x27d4eb2du);
        vec2 jitter=vec2(randomUnit(twinkle),randomUnit(twinkle+19u))*2.0-1.0;
        vec2 slope=normal.xy/max(.2,normal.z)*6.0+jitter*.42;
        vec3 facet=normalize(vec3(slope,1.0));
        float aligned=dot(facet,halfway);
        float glint=smoothstep(.955,.985,aligned);
        /* 큰 물결 마루를 따라 짧고 흐린 가로 띠를 더합니다. */
        float crest=noiseValue(vec2(screen.x/9.0+clock*.11,screen.y/1.7-clock*.04));
        float streak=step(.70,crest)*smoothstep(.80,.95,dot(normalize(vec3(normal.xy*6.0,1.0)),halfway))*.45;
        float elevation=smoothstep(4.0,18.0,uElevation);
        float strength=elevation*(1.0-.9*clamp(uOvercast,0.0,1.0))*sunVisibility;
        float level=floor(clamp(max(glint,streak)*strength,0.0,1.0)*4.0+dither*.35)/4.0;
        vec3 sunColor=uElevation<25.0?vec3(1.0,.74,.46):vec3(.95,.97,.90);
        sunReflection=sunColor*level*.38;
        color+=sunReflection;
    }
    if(uView==5)color=sunReflection;
    if(uView==3)color=vec3(sunVisibility);
    if(uView==4)color=vec3(localVisibility);
    imageStore(outputImage,pixel,vec4(clamp(color,0.0,1.0),1));
}
