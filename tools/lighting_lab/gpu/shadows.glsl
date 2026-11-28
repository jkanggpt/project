layout(std430,binding=0) readonly buffer Casters {Caster casters[];};
layout(std430,binding=1) readonly buffer Lights {Light lights[];};
layout(std430,binding=2) buffer Tiles {uint tileWords[];};
layout(std430,binding=4) buffer StaticTiles {uint staticWords[];};
uniform int uCasterCount,uLightCount,uSunEnabled;
uniform vec3 uSunDirection;
uniform vec2 uTileOrigin;
uniform ivec2 uTileSize;
uniform sampler2D uShadowWind;
uniform vec2 uShadowMap;
uniform float uShadowTime;
uniform int uShadowFoliage;
bool slabInterval(float p,float d,float lo,float hi,inout float first,inout float last){
    if(abs(d)<1e-8)return p>=lo&&p<=hi;
    float a=(lo-p)/d,b=(hi-p)/d;
    first=max(first,min(a,b));last=min(last,max(a,b));return first<=last;
}
float clippedRoundedCover(Caster c,vec3 p,vec3 d,float dither){
    vec3 q=(p-c.center.xyz)/c.extent.xyz,v=d/c.extent.xyz;
    float a=dot(v,v);
    if(a<1e-12)return casterCover(c,p,d,dither);
    float center=-dot(q,v)/a,closest=clamp(center,.0001,.9999);
    if(length(q+v*closest)>1.42)return 0.0;
    float originalCoverage=roundedCoverAt(c,q,v,closest,dither);
    if(originalCoverage<=0.0)return 0.0;
    float signX=c.contactProfile.y>.5?-1.0:1.0;
    float x=128.0+2.0*signX*(p.x-c.contact.x)/c.contact.z;
    float dx=2.0*signX*d.x/c.contact.z;
    int column=int(floor(x+dx*closest));
    if(p.y+d.y*closest<=contactFrontAt(c,column))return originalCoverage;
    vec3 nearest=q+v*center;
    float square=2.0164-dot(nearest,nearest);
    if(square<0.0)return 0.0;
    float halfSpan=sqrt(square/a);
    float first=max(.0001,center-halfSpan),last=min(.9999,center+halfSpan);
    if(first>last||!slabInterval(x,dx,0.0,256.0,first,last))return 0.0;
    float startX=x+dx*first;
    int direction=dx<0.0?-1:1;
    column=clamp(dx<0.0?int(ceil(startX))-1:int(floor(startX)),0,255);
    float covered=0.0;
    /* 원화의 아래 윤곽을 세운 절단면으로 둥근 체적의 앞쪽만 잘라 냅니다. */
    for(int step=0;step<256;step++){
        if(column<0||column>255)break;
        float end=last;
        if(abs(dx)>1e-8)end=min(end,(float(column+(direction>0?1:0))-x)/dx);
        float low=first,high=end,front=contactFrontAt(c,column);
        if(d.y>1e-8)high=min(high,(front-p.y)/d.y);
        else if(d.y< -1e-8)low=max(low,(front-p.y)/d.y);
        else if(p.y>front)high=low-1.0;
        if(low<=high){
            covered=max(covered,roundedCoverAt(c,q,v,clamp(center,low,high),dither));
            if(covered>=originalCoverage)return originalCoverage;
        }
        if(end>=last||abs(dx)<1e-8)break;
        first=max(first,end);column+=direction;
    }
    return min(covered,originalCoverage);
}
float upperCover(Caster c,vec3 p,vec3 d,float dither){
    return c.tags.z==1u?clippedRoundedCover(c,p,d,dither):casterCover(c,p,d,dither);
}
float groundedCover(Caster c,vec3 p,vec3 d,float dither){
    if(c.contactProfile.x<.5)return casterCover(c,p,d,dither);
    float first=.00001,last=.99999,height=c.contactProfile.z;
    bool inSlab=height>0.0&&slabInterval(p.z,d.z,0.0,height,first,last)&&
        slabInterval(p.x,d.x,c.contact.x-32.0*c.contact.z,c.contact.x+32.0*c.contact.z,first,last)&&
        slabInterval(p.y,d.y,c.contact.y-48.0*c.contact.w,c.contact.y+1.0,first,last);
    if(inSlab){
        float span=length(d.xy)*(last-first);
        int count=clamp(int(ceil(span/max(.20,min(.5,c.contact.z*.25)))),1,512);
        for(int i=0;i<=512;i++){
            if(i>count)break;
            vec2 at=p.xy+d.xy*mix(first,last,float(i)/float(count));
            if(contactDistance(c,at)<=0.0)return c.extent.w;
        }
    }
    /* 근사 상체는 정확한 하단 층 아래에서 다시 그림자를 만들지 않습니다. */
    if(d.z>0.0){
        float start=clamp((height-p.z)/d.z,0.0,1.0);
        if(start>=1.0)return 0.0;
        return upperCover(c,p+d*start,d*(1.0-start),dither);
    }
    if(p.z<height)return 0.0;
    float end=d.z<0.0?clamp((height-p.z)/d.z,0.0,1.0):1.0;
    return upperCover(c,p,d*end,dither);
}
const int TILE_LIMIT=128;
const int TILE_STRIDE=130;
int tileOffset(vec2 screenWorld){
    ivec2 tile=ivec2(floor((screenWorld-uTileOrigin)/16.0));
    if(any(lessThan(tile,ivec2(0)))||any(greaterThanEqual(tile,uTileSize)))return -1;
    return (tile.y*uTileSize.x+tile.x)*TILE_STRIDE;
}
float shadowVisibility(vec3 p,vec2 screenWorld,int lightIndex,int mode,uint receiver,float dither){
    int offset=tileOffset(screenWorld);
    bool fallback=offset<0;
    if(!fallback)fallback=tileWords[offset+1]!=0u;
    int count=fallback?uCasterCount:int(tileWords[offset]);
    vec3 ray=lightIndex<0?uSunDirection*8192.0:lights[lightIndex].position.xyz-p;
    uint source=lightIndex<0?0u:lights[lightIndex].tags.x;
    uint interior=lightIndex<0?0u:lights[lightIndex].tags.y;
    float covered=0.0;
    for(int i=0;i<count;i++){
        uint index=fallback?uint(i):tileWords[offset+2+i];
        Caster c=casters[index];
        if(c.tags.x==receiver||c.tags.x==source||(interior!=0u&&c.tags.x==interior))continue;
        if(mode==0&&c.tags.y!=0u)continue;
        if(mode==1&&c.tags.y==0u)continue;
        if(uShadowFoliage!=0&&(c.tags.w&2u)!=0u){
            float wind=texture(uShadowWind,c.contact.xy/uShadowMap).x;
            c.center.x+=floor(foliageBend(wind,c.tags.x*7919u,uShadowTime)*c.extent.x*.04+.5);
        }
        covered=max(covered,groundedCover(c,p,ray,dither));
    }
    return 1.0-covered;
}
float contactVisibility(vec2 p,float dither){
    int offset=tileOffset(p);
    bool fallback=offset<0;
    if(!fallback)fallback=tileWords[offset+1]!=0u;
    int count=fallback?uCasterCount:int(tileWords[offset]);
    float covered=0.0;
    for(int i=0;i<count;i++){
        Caster c=casters[fallback?uint(i):tileWords[offset+2+i]];
        if((c.tags.w&1u)==0u)continue;
        float coverage;
        if(c.contactProfile.x>.5){
            float distance=contactDistance(c,p);
            coverage=distance>=3.99*min(c.contact.z,c.contact.w)?0.0:
                (1.0-smoothstep(0.0,2.5,distance))*.24;
        }else if(c.tags.z==0u){
            vec2 distance=max(abs(p-c.center.xy)-c.extent.xy,vec2(0));
            coverage=clamp(1.0-length(distance)/2.5,0.0,1.0)*.24;
        }else{
            float distance=length((p-c.contact.xy)/max(c.contact.zw,vec2(.1)));
            coverage=clamp((1.22-distance)/.65,0.0,1.0)*.27;
        }
        covered=max(covered,floor(coverage*20.0+dither*.5)/20.0);
    }
    return 1.0-covered;
}

/* 이동하는 작은 등불은 지면 거리장 대신 가까운 고정 체적을 검사합니다. */
float localLightVisibility(vec3 p,vec2 screenWorld,int lightIndex,uint receiver,float dither){
    int offset=tileOffset(screenWorld);
    bool fallback=offset<0;
    if(!fallback)fallback=staticWords[offset+1]!=0u;
    int count=fallback?uCasterCount:int(staticWords[offset]);
    Light light=lights[lightIndex];
    vec3 ray=light.position.xyz-p;
    float covered=0.0;
    for(int i=0;i<count;i++){
        Caster c=casters[fallback?uint(i):staticWords[offset+2+i]];
        if(c.tags.y!=0u||c.tags.x==receiver||c.tags.x==light.tags.x||c.tags.x==light.tags.y)continue;
        if(distance(c.center.xy,p.xy)>light.position.w+length(c.extent.xy))continue;
        covered=max(covered,casterCover(c,p,ray,dither));
        if(covered>.94)break;
    }
    return 1.0-covered;
}
