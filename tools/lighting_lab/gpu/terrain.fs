in vec2 groundWorld;
uniform vec2 uMap;
uniform float uTime;
uniform int uWater;
uniform sampler2D uWaves;
layout(std430,binding=6) readonly buffer GroundCasters { Caster groundCasters[]; };
uniform int uGroundCasterCount;
layout(location=0) out vec4 outAlbedo;
layout(location=1) out vec4 outNormalHeight;
layout(location=2) out vec4 outMaterial;
layout(location=3) out vec4 outOwner;
vec3 paving(vec2 p,out vec3 normal) {
    vec2 grid=p/vec2(7,5);
    ivec2 cell=ivec2(floor(grid));
    float first=100.0,second=100.0;vec2 nearest=vec2(0);uint seed=0u;
    for(int y=-1;y<=1;y++)for(int x=-1;x<=1;x++){
        ivec2 q=cell+ivec2(x,y);uint h=hashCell(q);
        vec2 center=vec2(q)+vec2(.5+float(q.y&1)*.5,.5);
        center+=vec2(randomUnit(h)-.5,randomUnit(h+1u)-.5)*vec2(.42,.25);
        vec2 d=(grid-center)*vec2(7,5);float dist=dot(d,d);
        if(dist<first){second=first;first=dist;nearest=d;seed=h;}
        else second=min(second,dist);
    }
    float edge=(sqrt(second)-sqrt(first))*.5;
    float wear=noiseValue(p/31.0);
    float level=.36+randomUnit(seed)*.31+wear*.12;
    vec3 color=paletteStone(level);
    if(edge<.55){color=paletteEarth(.16);normal=vec3(0,0,1);}
    else{
        float bevel=1.0-smoothstep(.55,1.45,edge);
        normal=normalize(vec3(nearest*bevel*.12,1));
        if(randomUnit(hashCell(ivec2(p))+seed)>.987)color=paletteStone(.29);
    }
    return color;
}
float cellularEdge(vec2 p) {
    ivec2 at=ivec2(floor(p));vec2 f=fract(p);float a=8,b=8;
    for(int y=-1;y<=1;y++)for(int x=-1;x<=1;x++){
        ivec2 q=at+ivec2(x,y);uint h=hashCell(q);
        vec2 d=vec2(x,y)+vec2(randomUnit(h),randomUnit(h+9u))-f;
        float dist=length(d);
        if(dist<a){b=a;a=dist;}else b=min(b,dist);
    }
    return b-a;
}
void main(){
    vec2 p=floor(groundWorld)+.5;
    if(any(lessThan(p,vec2(0)))||any(greaterThanEqual(p,uMap)))discard;
    vec4 field=terrainField(p);float bank=field.x,road=field.y;
    uint flags=terrainAt(ivec2(floor(p/16.0)));
    float macro=noiseValue(p/67.0),cluster=noiseValue(p/19.0);
    float small=noiseValue(p/3.7);
    float level=.51+macro*.25+floor(cluster*4.0)*.035;
    vec3 color=paletteGrass(level),normal=vec3(0,0,1);
    if((flags&16u)!=0u&&(flags&4u)==0u)color=paletteEarth(.37+cluster*.20);
    int material=4;
    /* 짧은 지피 식물도 칸별 그림 대신 월드 군집을 따릅니다. */
    uint dotSeed=hashCell(ivec2(floor(p/vec2(3,5))));
    vec2 blade=mod(p,vec2(3,5));
    if(noiseValue(p/37.0)>.60&&randomUnit(dotSeed)>.66&&blade.x<1.05&&blade.y<2.0+randomUnit(dotSeed+1u)*2.0)
        color=paletteGrass(level-.07);
    if(field.z<3.0){
        vec3 rock=paletteStone(.35+small*.24+macro*.12);
        color=mix(rock,color,smoothstep(-2.0,3.0,field.z));
    }
    float wetGrass=step(.53,noiseValue(p/91.0+5.0));
    float shoreWidth=mix(7.0,3.0,wetGrass);
    if(bank<shoreWidth&&bank>=0.0){
        vec3 sand=paletteEarth(.50+cluster*.25);
        vec3 wet=paletteEarth(.26+cluster*.12);
        if(field.z<4.0||noiseValue(p/91.0+17.0)>.62){
            sand=paletteStone(.45+cluster*.19);
            wet=paletteStone(.25+cluster*.13);
        }
        sand=mix(wet,sand,smoothstep(.5,3.5,bank));
        float pebbles=randomUnit(hashCell(ivec2(p/2.0)));
        if(pebbles>.93&&bank>1.0)sand=paletteStone(.42+pebbles*.15);
        color=mix(sand,color,smoothstep(shoreWidth-2.0,shoreWidth+1.5,bank));
    }
    if(road<4.0&&bank>0.0){
        vec3 stoneNormal;vec3 stone=paving(p,stoneNormal);
        vec3 dirt=paletteEarth(.37+cluster*.17+small*.04);
        stone=mix(dirt,stone,clamp(field.w,0.0,1.0));
        stone=mix(stone,paletteStone(.70),clamp(field.w-1.0,0.0,.4)*.22);
        stoneNormal=normalize(mix(vec3(0,0,1),stoneNormal,clamp(field.w,0.0,1.0)));
        float edge=road+(noiseValue(p/8.0)-.5)*2.0;
        float blend=1.0-smoothstep(-1.5,3.0,edge);
        color=mix(color,stone,blend);normal=normalize(mix(normal,stoneNormal,blend));
        if(blend>.5)material=5;
    }
    if((flags&32u)!=0u)color=mix(color,vec3(.80,.84,.79),.82);
    if(bank<0.0){
        material=7;float depth=-bank;
        vec4 wave=uWater!=0?texture(uWaves,p/uMap):vec4(0);
        float obstacle=1000.0;
        for(int i=0;i<uGroundCasterCount;i++){
            Caster c=groundCasters[i];
            if(c.contactProfile.x<.5||c.contactProfile.z<=0.0||c.extent.z>14.0)continue;
            if(distance(p,c.contact.xy)>36.0*c.contact.z)continue;
            obstacle=min(obstacle,contactDistance(c,p));
        }
        depth=min(depth,max(0.0,obstacle));
        float body=.36+.36*exp(-depth/13.0)+wave.z*.14;
        color=paletteWater(body);
        normal=normalize(vec3(-wave.xy*smoothstep(0.0,10.0,depth),1));
        float time=uWater!=0?uTime:0.0;
        vec2 flow=vec2(time*.31,-time*.17);
        vec2 stream=vec2(noiseValue(p/61.0+time*.013),noiseValue(p/137.0-time*.009))-.5;
        vec2 distort=vec2(noiseValue(p/13.0+flow*.023),noiseValue(p/17.0-flow*.019))-.5;
        float net=cellularEdge(p/24.0+distort*.85+stream*.30+flow*.025);
        float caustic=(1.0-smoothstep(.027,.062,net))*exp(-depth/34.0)*
            step(.23,noiseValue(p/11.0+flow*.12));
        float foamLine=abs(depth-(1.3+sin(time*.83+noiseValue(p/19.0)*4.0)*.65));
        float broken=step(.33,noiseValue(p/7.0+flow*.13));
        float foam=(1.0-smoothstep(.45,1.35,foamLine))*broken;
        color=mix(color,paletteWater(.94),max(foam*.73,caustic*.22));
    }
    outAlbedo=vec4(color,1);outNormalHeight=vec4(normal,0);
    outMaterial=vec4(float(material)/255.0,0,.75,1);outOwner=vec4(0);
}
