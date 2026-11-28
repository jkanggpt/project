layout(std430,binding=0) readonly buffer Sprites {Sprite sprites[];};
layout(std430,binding=1) readonly buffer Grass {Sprite blades[];};
uniform int uBase,uGrass,uFoliage;
uniform vec2 uOrigin,uViewport,uMap;
uniform float uScale,uTime;
uniform sampler2D uWind;
out vec2 localUv;
out vec2 worldScreen;
out vec2 sourceUv;
out vec2 normalUv;
flat out vec4 spriteTint;
flat out vec4 spriteShape;
flat out vec4 spriteSource;
flat out vec4 spriteNormalSource;
flat out uvec4 spriteTags;
out float bendSlope;
void main(){
    Sprite s=uGrass!=0?blades[gl_InstanceID]:sprites[uBase+gl_InstanceID];
    const vec2 corners[6]=vec2[6](vec2(0,0),vec2(1,0),vec2(1,1),vec2(0,0),vec2(1,1),vec2(0,1));
    int band=gl_VertexID/6;
    vec2 uv=vec2(corners[gl_VertexID%6].x,(float(band)+corners[gl_VertexID%6].y)/8.0);
    vec2 world=s.destination.xy+uv*s.destination.zw;
    if((s.tags.z&8u)!=0u){
        float c=cos(s.shape.z),sn=sin(s.shape.z);
        vec2 offset=(uv-vec2(.5,1))*s.destination.zw;
        world=s.destination.xy+vec2(s.destination.z*.5,s.destination.w)+mat2(c,sn,-sn,c)*offset;
    }
    bendSlope=0.0;
    if((s.tags.y==2u&&uFoliage!=0)||s.tags.y==8u){
        vec4 wind=texture(uWind,vec2(s.destination.x+s.destination.z*.5,s.shape.x)/uMap);
        float tip=s.tags.y==2u?clamp((.70-uv.y)/.70,0.0,1.0):clamp((.80-uv.y)/.80,0.0,1.0);
        float bend=foliageBend(wind.x,s.tags.w,uTime);
        if(s.tags.y==8u)bend+=wind.z*2.3;
        float amplitude=s.destination.w*(s.tags.y==2u?.028:.20);
        float move=bend*amplitude*tip*tip;
        world.x+=floor(move+.5);
        if(s.tags.y==8u)world.y+=floor(length(wind.zw)*tip*2.0+.5);
        bendSlope=bend*tip*.16;
    }
    vec2 screen=(world-uOrigin)*uScale;
    float depth=(s.tags.z&2u)!=0u?.997:s.tags.y==4u||s.tags.y==5u||s.tags.y==7u||s.tags.x==0u?.999:
                clamp(.90-s.shape.x/(max(1.0,uMap.y)*1.2),.02,.95);
    if(s.tags.y==8u)depth=clamp(.90-s.shape.x/(max(1.0,uMap.y)*1.2),.02,.95);
    if((s.tags.z&8u)!=0u)depth=.996;
    gl_Position=vec4(screen.x/uViewport.x*2.0-1.0,1.0-screen.y/uViewport.y*2.0,depth*2.0-1.0,1);
    localUv=uv;worldScreen=world;sourceUv=s.source.xy+uv*s.source.zw;normalUv=s.normalSource.xy+uv*s.normalSource.zw;
    spriteTint=s.tint;spriteShape=s.shape;spriteTags=s.tags;spriteSource=s.source;spriteNormalSource=s.normalSource;
}
