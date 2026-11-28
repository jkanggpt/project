in vec2 localUv,worldScreen,sourceUv,normalUv;
flat in vec4 spriteTint,spriteShape,spriteSource,spriteNormalSource;
flat in uvec4 spriteTags;
in float bendSlope;
uniform sampler2D uAlbedo,uNormals,uSurface,uWaves;
uniform vec2 uMap;
uniform float uTime,uDetail;
uniform int uWater,uNatural;
layout(location=0) out vec4 outAlbedo;
layout(location=1) out vec4 outNormalHeight;
layout(location=2) out vec4 outMaterial;
layout(location=3) out vec4 outOwner;
float luminance(vec3 c){return dot(c,vec3(.299,.587,.114));}
vec4 meadowColor(out vec3 bladeNormal) {
    float height=1.0-localUv.y;
    uint seed=spriteTags.w;
    int species=int(spriteShape.z+.5);
    int count=3+int(seed%5u);
    float cover=0.0,lit=0.0;
    for(int i=0;i<7;i++){
        if(i>=count)break;
        uint h=hashWord(seed+uint(i)*113u);
        float root=.12+randomUnit(h)*.76;
        float tip=clamp(root+(randomUnit(h+1u)-.5)*.68,.05,.95);
        float tall=.48+randomUnit(h+2u)*.52;
        float along=height/tall;
        float center=mix(root,tip,along*along);
        float width=(species==3?.105:.073)*(1.0-along*.80);
        if(along<=1.0&&abs(localUv.x-center)<width){
            cover=1.0;lit=randomUnit(h+3u)*.17+height*.13;
        }
    }
    float tone=.28+spriteShape.w*.18+lit+height*.18;
    vec3 color=paletteGrass(tone);
    if(species==4)color=mix(color,paletteEarth(.55),.18);
    if(species==3){
        float stem=abs(localUv.x-.50);
        bool leaf=length((localUv-vec2(.32,.42))/vec2(.22,.19))<1.0||
                  length((localUv-vec2(.65,.64))/vec2(.24,.17))<1.0;
        if(leaf||(stem<.065&&localUv.y>.40)){cover=1.0;color=paletteGrass(.39+height*.30+spriteShape.w*.10);}
    }
    if(species==3&&height>.70&&randomUnit(seed+87u)>.93){
        if(abs(localUv.x-.5)<.18){cover=1.0;color=vec3(.87,.82,.63);}
    }
    bladeNormal=normalize(vec3(-bendSlope,.22+.20*localUv.x,1));
    return vec4(color,cover);
}
void main(){
    uint kind=spriteTags.y;
    /* 공유 모서리의 화소 중심이 아틀라스의 옆 투명 칸을 읽지 않게 합니다. */
    vec2 size=vec2(textureSize(uAlbedo,0));
    vec2 lo=min(spriteSource.xy,spriteSource.xy+spriteSource.zw)+.5/size;
    vec2 hi=max(spriteSource.xy,spriteSource.xy+spriteSource.zw)-.5/size;
    vec2 uv=clamp(sourceUv,lo,hi);
    vec3 tuftNormal=vec3(0,0,1);
    vec4 original=kind==8u&&uNatural!=0?meadowColor(tuftNormal):texture(uAlbedo,uv);
    vec4 color=original*spriteTint;
    if(original.a<.5||color.a<.005)discard;
    float z=max(0.0,(1.0-localUv.y)*spriteShape.y);
    vec3 n=vec3(0,0,1);
    float emission=0.0;
    if(spriteTags.z==1u){
        vec2 normalSize=vec2(textureSize(uNormals,0));
        vec2 normalLo=min(spriteNormalSource.xy,spriteNormalSource.xy+spriteNormalSource.zw)+.5/normalSize;
        vec2 normalHi=max(spriteNormalSource.xy,spriteNormalSource.xy+spriteNormalSource.zw)-.5/normalSize;
        vec2 at=clamp(normalUv,normalLo,normalHi);
        n=normalize(texture(uNormals,at).rgb*2.0-1.0);
        if(spriteSource.z<0.0)n.x=-n.x;
        vec4 surface=texture(uSurface,at);
        z=(surface.r*255.0+surface.g*255.0/256.0)*spriteShape.y;
        emission=surface.b;
    }else if(kind!=8u){
        float dx=luminance(texture(uAlbedo,clamp(uv+vec2(1.0/size.x,0),lo,hi)).rgb)-luminance(texture(uAlbedo,clamp(uv-vec2(1.0/size.x,0),lo,hi)).rgb);
        float dy=luminance(texture(uAlbedo,clamp(uv+vec2(0,1.0/size.y),lo,hi)).rgb)-luminance(texture(uAlbedo,clamp(uv-vec2(0,1.0/size.y),lo,hi)).rgb);
        if(spriteSource.z<0.0)dx=-dx;
        vec3 base=kind==1u?(localUv.y<.48?vec3(0,.42,.91):vec3(0,.95,.31)):
                  kind==2u?normalize(vec3((localUv.x-.5)*1.15,.45,.9)):
                  kind==3u?normalize(vec3((localUv.x-.5)*.9,.7,.7)):
                  kind==6u?normalize(vec3((localUv.x-.5),.35,.9)):vec3(0,0,1);
        n=normalize(base+vec3(-dx,-dy*.65,0)*uDetail*2.8);
        if(kind==4u||kind==5u||kind==7u)z=0.0;
    }
    if(kind==8u)n=uNatural!=0?tuftNormal:normalize(vec3(-bendSlope,.25,1));
    if(uNatural!=0&&kind!=8u){
        float value=luminance(color.rgb);
        if(kind==2u&&color.g>color.r*1.06)color.rgb=mix(color.rgb,paletteGrass(value),.72);
        if(kind==2u&&color.g<=color.r*1.06)color.rgb=mix(color.rgb,paletteWood(value),.60);
        if(kind==6u)color.rgb=mix(color.rgb,paletteStone(value),.72);
        if(kind==1u){
            vec3 ramp=color.r>color.g*1.22?paletteRoof(value):
                      color.r>color.b*1.15&&value<.58?paletteWood(value):palettePlaster(value);
            color.rgb=mix(color.rgb,ramp,.55);
        }
    }
    if(kind==7u&&uWater!=0){
        vec4 wave=texture(uWaves,worldScreen/uMap);
        n=normalize(vec3(-wave.xy,1));z=0.0;
        float bands=floor((wave.z*.55+.5)*7.0)/7.0;
        color.rgb*=.93+bands*.14;
        float glint=step(.16,wave.z)*step(.3,wave.w);
        float shore=step(wave.w,.18)*step(.42,noiseValue(worldScreen*.18+vec2(uTime*.13,uTime*.031)));
        color.rgb+=vec3(.025,.035,.035)*glint+vec3(.04,.06,.055)*shore;
    }
    n=normalize(vec3(n.x+bendSlope*n.z,n.y,n.z));
    if(kind==4u){float macro=noiseValue(worldScreen*.017);color.rgb*=.94+macro*.11;}
    outAlbedo=vec4(color.rgb,color.a);
    outNormalHeight=vec4(n,z);
    outMaterial=vec4(float(kind)/255.0,emission,.75,color.a);
    uint owner=spriteTags.x;
    outOwner=vec4(float(owner&255u),float((owner>>8)&255u),float((owner>>16)&255u),float(owner>>24))/255.0;
}
