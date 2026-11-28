/* 시야 경계 구름: 보이지 않는 칸(지면 여유 한 칸 포함)은 항상 불투명하게 덮고,
   보이는 칸 안쪽 가장자리만 구름 덩어리 모양으로 풀어 줍니다. 모양은 세계 좌표와 월드 씨앗의
   다중 크기 왜곡 잡음으로 만들고 층마다 다른 속도로 천천히 흐르므로 화면 이동·타일과 무관합니다. */
layout(local_size_x=8,local_size_y=8) in;
layout(std430,binding=3) readonly buffer FogVisibility { uint fogVisibility[]; };
layout(rgba8,binding=0) writeonly uniform image2D outputImage;
uniform ivec2 uMapCells;
uniform vec2 uViewport,uOrigin;
uniform float uScale,uTime,uAmbient,uElevation;
uniform int uSunEnabled;

bool fogHidden(ivec2 cell){
    if(any(lessThan(cell,ivec2(0)))||any(greaterThanEqual(cell,uMapCells)))return true;
    return fogVisibility[cell.y*uMapCells.x+cell.x]<2u;
}
/* 가장 가까운 숨은 칸까지의 거리(화소). 숨은 칸 안은 0, 두 칸 넘게 떨어지면 40입니다. */
float hiddenDistance(vec2 world){
    ivec2 cell=ivec2(floor(world/16.0));
    if(fogHidden(cell))return 0.0;
    float nearest=40.0;
    for(int y=-2;y<=2;y++)for(int x=-2;x<=2;x++){
        ivec2 q=cell+ivec2(x,y);
        if(!fogHidden(q))continue;
        vec2 low=vec2(q)*16.0,high=low+16.0;
        nearest=min(nearest,length(max(max(low-world,world-high),vec2(0))));
    }
    return nearest;
}
/* 지터 격자의 둥근 덩이 높이. 칸마다 반지름·높이·위치가 달라 반복 무늬가 보이지 않습니다. */
float puffLayer(vec2 p,float cell,uint salt){
    vec2 at=p/cell;
    ivec2 base=ivec2(floor(at));
    float best=0.0;
    for(int y=-1;y<=1;y++)for(int x=-1;x<=1;x++){
        ivec2 q=base+ivec2(x,y);
        uint seed=hashCell(q)^salt;
        vec2 center=vec2(q)+.15+vec2(randomUnit(seed),randomUnit(seed+31u))*.70;
        float radius=.48+randomUnit(seed+57u)*.50;
        float d=length((at-center)*vec2(1.0,1.12))/radius;
        if(d<1.0)best=max(best,sqrt(1.0-d*d)*(.72+randomUnit(seed+83u)*.28));
    }
    return best;
}
/* 큰 뭉게구름 위에 작은 덩이를 얹고 서로 다른 속도로 흘립니다. */
float cloudHeight(vec2 p,float t){
    vec2 warp=vec2(noiseValue(p/131.0+vec2(t*.0021,0)),noiseValue(p/117.0+vec2(5.3,t*.0017)))-.5;
    vec2 q=p+warp*34.0;
    float big=puffLayer(q+vec2(t*.27,-t*.08),47.0,0x51a3u);
    float small=puffLayer(q+vec2(-t*.15,t*.11)+vec2(311,97),19.0,0x9e2du);
    float broad=noiseValue(q/83.0+vec2(t*.0026,0));
    return clamp(max(big,big*.55+small*.52)*.86+broad*.14,0.0,1.0);
}
void main(){
    ivec2 pixel=ivec2(gl_GlobalInvocationID.xy);
    if(any(greaterThanEqual(pixel,imageSize(outputImage))))return;
    vec2 world=floor(uOrigin+vec2(float(pixel.x)+.5,uViewport.y-float(pixel.y)-.5)/max(uScale,.001))+.5;
    float distance=hiddenDistance(world);
    if(distance>=39.0){imageStore(outputImage,pixel,vec4(0));return;}
    float t=uTime;
    float h=cloudHeight(world,t);
    /* 구름 덩어리가 보이는 땅으로 0~14화소 밀려 들어오고 그 앞 3~10화소에서 옅어집니다. */
    float intrusion=1.0+13.0*smoothstep(.20,.85,h);
    float width=3.0+7.0*noiseValue(world/23.0+vec2(t*.004,0));
    float dither=bayerValue(ivec2(world));
    float alpha=distance<=0.0?1.0:1.0-smoothstep(intrusion,intrusion+width,distance);
    alpha=floor(alpha*5.0+dither*.95)/5.0;
    if(distance<=0.0)alpha=1.0;
    if(alpha<=0.0){imageStore(outputImage,pixel,vec4(0));return;}
    /* 왼쪽 위의 빛을 받는 볼록면은 밝게, 아래쪽 골은 어둡게 칠합니다. */
    vec2 toLight=vec2(-.62,-.78);
    float lit=clamp(.5+(h-cloudHeight(world+toLight*3.0,t))*3.2,0.0,1.0);
    float tone=clamp(.08+h*.50+(lit-.5)*1.05+.25,0.0,1.0);
    tone=floor(tone*6.0+dither*.85)/6.0;
    float day=clamp((uAmbient-.20)/.30,0.0,1.0);
    float warm=uSunEnabled!=0?1.0-smoothstep(12.0,26.0,uElevation):0.0;
    vec3 shadowDay=vec3(.60,.60,.62),lightDay=vec3(.95,.93,.87);
    vec3 shadowNight=vec3(.22,.23,.34),lightNight=vec3(.50,.50,.66);
    vec3 shadowWarm=vec3(.52,.47,.58),lightWarm=vec3(.98,.78,.66);
    vec3 shadow=mix(mix(shadowNight,shadowDay,day),shadowWarm,warm*day);
    vec3 light=mix(mix(lightNight,lightDay,day),lightWarm,warm*day);
    vec3 color=mix(shadow,light,tone);
    /* 반투명 가장자리는 조금 어둡게 하여 땅 위에 떠 있는 두께를 보입니다. */
    color*=mix(.88,1.0,alpha);
    imageStore(outputImage,pixel,vec4(color,alpha));
}
