/* CPU와 같은 16바이트 정렬 자료를 모든 GPU 단계가 공유합니다. */
struct Sprite { vec4 destination; vec4 source; vec4 tint; vec4 normalSource; vec4 shape; uvec4 tags; vec4 contactProfile; };
struct Caster { vec4 center; vec4 extent; vec4 contact; uvec4 tags; vec4 contactProfile; };
uniform sampler2D uContacts;
uniform sampler2D uContactFront;
float contactFrontAt(Caster c,int column){
    int page=int(c.contactProfile.x+.5)-1;
    vec4 sampleFront=texelFetch(uContactFront,ivec2(clamp(column,0,255),page),0);
    return sampleFront.w>.5?c.contact.y+sampleFront.x*c.contact.w:100000.0;
}
float contactDistance(Caster c,vec2 p){
    vec2 local=(p-c.contact.xy)/max(c.contact.zw,vec2(.001));
    if(c.contactProfile.y>.5)local.x=-local.x;
    if(any(lessThan(local,vec2(-64,-48)))||any(greaterThanEqual(local,vec2(64,16))))return 10000.0;
    int page=int(c.contactProfile.x+.5)-1;
    ivec2 at=ivec2(page%16,page/16)*ivec2(256,128)+ivec2(floor((local+vec2(64,48))*2.0));
    float value=texelFetch(uContacts,at,0).r;
    return value>.999?10000.0:(value-.5)*8.0*min(c.contact.z,c.contact.w);
}
struct Light { vec4 position; vec4 color; uvec4 tags; };
struct Emitter { vec4 position; vec4 settings; };
struct Particle { vec4 position; vec4 velocity; vec4 settings; };
uint hashWord(uint x) { x^=x>>16; x*=0x7feb352du; x^=x>>15; x*=0x846ca68bu; return x^(x>>16); }
uniform uint uWorldSeed;
uint hashCell(ivec2 p) { return hashWord(uint(p.x)*0x9e3779b9u ^ uint(p.y)*0x85ebca6bu ^ uWorldSeed); }
float randomUnit(uint x) { return float(hashWord(x)>>8)*(1.0/16777216.0); }
float foliageBend(float wind,uint seed,float time) {
    float variation=randomUnit(seed);
    return wind-.45+sin(time*(.67+variation*.31)+variation*41.0)*.27;
}
float noiseValue(vec2 p) {
    ivec2 i=ivec2(floor(p)); vec2 f=fract(p); f=f*f*(3.0-2.0*f);
    return mix(mix(randomUnit(hashCell(i)),randomUnit(hashCell(i+ivec2(1,0))),f.x),
               mix(randomUnit(hashCell(i+ivec2(0,1))),randomUnit(hashCell(i+ivec2(1,1))),f.x),f.y);
}
float bayerValue(ivec2 p) {
    const float values[16]=float[16](0,8,2,10,12,4,14,6,3,11,1,9,15,7,13,5);
    return values[(p.y&3)*4+(p.x&3)]/16.0;
}
bool clipPlane(vec3 n,float bound,vec3 p,vec3 d,float margin,inout float first,inout float last) {
    float direction=dot(n,d), distance=bound+margin-dot(n,p);
    if(abs(direction)<1e-8) return distance>=0.0;
    float t=distance/direction;
    if(direction<0.0) first=max(first,t); else last=min(last,t);
    return first<=last;
}
bool convexHit(Caster c,vec3 p,vec3 d,float margin) {
    p-=c.center.xyz;
    float first=.0001,last=.9999;
    if(!clipPlane(vec3(1,0,0),c.extent.x,p,d,margin,first,last))return false;
    if(!clipPlane(vec3(-1,0,0),c.extent.x,p,d,margin,first,last))return false;
    if(!clipPlane(vec3(0,1,0),c.extent.y,p,d,margin,first,last))return false;
    if(!clipPlane(vec3(0,-1,0),c.extent.y,p,d,margin,first,last))return false;
    if(c.tags.z==2u){
        float slope=c.extent.z/max(.01,c.extent.y),length=sqrt(1.0+slope*slope);
        if(!clipPlane(vec3(0,0,-1),0.0,p,d,margin,first,last))return false;
        if(!clipPlane(vec3(0,slope,1)/length,c.extent.z/length,p,d,margin,first,last))return false;
        if(!clipPlane(vec3(0,-slope,1)/length,c.extent.z/length,p,d,margin,first,last))return false;
    }else{
        if(!clipPlane(vec3(0,0,1),c.extent.z,p,d,margin,first,last))return false;
        if(!clipPlane(vec3(0,0,-1),c.extent.z,p,d,margin,first,last))return false;
    }
    return true;
}
float roundedCoverAt(Caster c,vec3 q,vec3 v,float t,float dither){
    float softness=.12+.30*t,distance=length(q+v*t);
    float f=clamp((1.0+softness-distance)/(2.0*softness),0.0,1.0);
    f=f*f*(3.0-2.0*f);
    return clamp(floor(f*8.0+dither)/8.0,0.0,1.0)*c.extent.w;
}
float casterCover(Caster c,vec3 p,vec3 d,float dither) {
    if(c.tags.z==1u){
        vec3 q=(p-c.center.xyz)/c.extent.xyz,v=d/c.extent.xyz;
        float t=clamp(-dot(q,v)/max(dot(v,v),1e-8),.0001,.9999);
        return roundedCoverAt(c,q,v,t,dither);
    }
    const float softness=1.25;
    if(!convexHit(c,p,d,softness))return 0.0;
    if(convexHit(c,p,d,-softness))return c.extent.w;
    float lo=-softness,hi=softness;
    for(int j=0;j<4;j++) {float mid=(lo+hi)*.5;if(convexHit(c,p,d,mid))hi=mid;else lo=mid;}
    float f=clamp(.5-(lo+hi)/(4.0*softness),0.0,1.0);f=f*f*(3.0-2.0*f);
    return clamp(floor(f*8.0+dither)/8.0,0.0,1.0)*c.extent.w;
}
bool insideCaster(Caster c,vec3 p) {
    vec3 q=p-c.center.xyz;
    if(c.tags.z==1u)return dot(q/c.extent.xyz,q/c.extent.xyz)<1.0;
    if(abs(q.x)>c.extent.x||abs(q.y)>c.extent.y)return false;
    if(c.tags.z==2u)return q.z>=0.0&&q.z<=c.extent.z*(1.0-abs(q.y)/c.extent.y);
    return abs(q.z)<=c.extent.z;
}

vec3 paletteGrass(float value){
    const vec3 ramp[5]=vec3[5](vec3(0.1960784,0.2980392,0.2627451),vec3(0.3137255,0.4196078,0.2901961),vec3(0.4705882,0.5803922,0.3254902),vec3(0.6235294,0.7098039,0.4156863),vec3(0.7960784,0.8352941,0.6039216));
    float at=clamp(value,0.0,1.0)*4.0;int i=min(3,int(at));
    return mix(ramp[i],ramp[i+1],at-float(i));
}

vec3 paletteEarth(float value){
    const vec3 ramp[5]=vec3[5](vec3(0.3019608,0.2509804,0.2196078),vec3(0.4627451,0.3686275,0.2745098),vec3(0.6235294,0.5098039,0.3529412),vec3(0.7411765,0.6470588,0.4431373),vec3(0.8470588,0.7607843,0.5450980));
    float at=clamp(value,0.0,1.0)*4.0;int i=min(3,int(at));
    return mix(ramp[i],ramp[i+1],at-float(i));
}

vec3 paletteWood(float value){
    const vec3 ramp[5]=vec3[5](vec3(0.2862745,0.2313725,0.2117647),vec3(0.4392157,0.3215686,0.2392157),vec3(0.5960784,0.4431373,0.3058824),vec3(0.7372549,0.5843137,0.3921569),vec3(0.8470588,0.7254902,0.5098039));
    float at=clamp(value,0.0,1.0)*4.0;int i=min(3,int(at));
    return mix(ramp[i],ramp[i+1],at-float(i));
}

vec3 paletteStone(float value){
    const vec3 ramp[5]=vec3[5](vec3(0.2784314,0.2784314,0.3137255),vec3(0.4274510,0.4274510,0.4470588),vec3(0.5960784,0.5921569,0.5529412),vec3(0.7254902,0.7098039,0.6313725),vec3(0.8666667,0.8313725,0.7254902));
    float at=clamp(value,0.0,1.0)*4.0;int i=min(3,int(at));
    return mix(ramp[i],ramp[i+1],at-float(i));
}

vec3 paletteRoof(float value){
    const vec3 ramp[5]=vec3[5](vec3(0.3725490,0.2352941,0.2392157),vec3(0.5372549,0.3137255,0.2784314),vec3(0.6862745,0.4274510,0.3215686),vec3(0.7882353,0.5490196,0.4039216),vec3(0.8784314,0.6862745,0.5058824));
    float at=clamp(value,0.0,1.0)*4.0;int i=min(3,int(at));
    return mix(ramp[i],ramp[i+1],at-float(i));
}

vec3 paletteWater(float value){
    const vec3 ramp[5]=vec3[5](vec3(0.1568627,0.3019608,0.3803922),vec3(0.2000000,0.4431373,0.5019608),vec3(0.2941176,0.6117647,0.6313725),vec3(0.5137255,0.7647059,0.7372549),vec3(0.8431373,0.8901961,0.7960784));
    float at=clamp(value,0.0,1.0)*4.0;int i=min(3,int(at));
    return mix(ramp[i],ramp[i+1],at-float(i));
}

vec3 paletteSmoke(float value){
    const vec3 ramp[5]=vec3[5](vec3(0.2941176,0.2980392,0.3137255),vec3(0.4549020,0.4666667,0.4627451),vec3(0.6313725,0.6509804,0.6156863),vec3(0.7686275,0.7921569,0.7372549),vec3(0.8823529,0.8862745,0.8156863));
    float at=clamp(value,0.0,1.0)*4.0;int i=min(3,int(at));
    return mix(ramp[i],ramp[i+1],at-float(i));
}

vec3 palettePlaster(float value){
    const vec3 ramp[5]=vec3[5](vec3(0.4156863,0.3843137,0.3725490),vec3(0.5921569,0.5607843,0.4901961),vec3(0.7411765,0.7019608,0.6000000),vec3(0.8627451,0.8156863,0.6862745),vec3(0.9333333,0.8901961,0.7764706));
    float at=clamp(value,0.0,1.0)*4.0;int i=min(3,int(at));
    return mix(ramp[i],ramp[i+1],at-float(i));
}
