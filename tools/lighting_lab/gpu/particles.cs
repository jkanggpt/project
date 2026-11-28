layout(local_size_x=64) in;
layout(std430,binding=0) buffer Particles{Particle particles[];};
layout(std430,binding=1) readonly buffer Emitters{Emitter emitters[];};
uniform int uEmitterCount,uParticleCount;
uniform int uNatural;
uniform int uProbe;
uniform float uDelta,uTime;
uniform vec2 uMap;
uniform sampler2D uWind;
void main(){
    uint id=gl_GlobalInvocationID.x;
    if(id>=uint(uParticleCount))return;
    if(uEmitterCount==0||id>=uint(uEmitterCount)*32u){particles[id].settings.w=0.0;return;}
    if(uProbe!=0){
        Particle probe;
        probe.position=vec4(emitters[0].position.xyz,.45);
        probe.velocity=vec4(0,0,0,1);
        probe.settings=vec4(1.5,.3,emitters[0].settings.x,(uProbe==2||id==0u)?1.0:0.0);
        particles[id]=probe;return;
    }
    uint e=id%uint(uEmitterCount);
    Emitter emitter=emitters[e];Particle p=particles[id];
    uint seed=hashWord((id/uint(uEmitterCount)+uint(emitter.settings.x)*9319u)^uWorldSeed);
    float life=2.8+randomUnit(seed)*3.0;
    if(p.settings.w==0.0||p.position.w>=life||p.settings.z!=emitter.settings.x){
        float age=p.settings.w==0.0?randomUnit(seed+4u)*life:0.0;
        p.position=vec4(emitter.position.xyz,age);
        vec2 wind=texture(uWind,emitter.position.xy/uMap).xy;
        p.velocity=vec4(wind*2.4,(3.3+randomUnit(seed+2u)*2.2)*(uNatural!=0?4.0:1.0),life);
        p.position.xyz+=p.velocity.xyz*age;
        p.settings=vec4(.8+randomUnit(seed+3u)*1.5,randomUnit(seed+5u),emitter.settings.x,1);
    }
    vec2 wind=texture(uWind,p.position.xy/uMap).xy;
    p.velocity.xy=mix(p.velocity.xy,wind*2.8,clamp(uDelta*.8,0.0,1.0));
    p.position.xyz+=p.velocity.xyz*uDelta;
    p.position.w+=uDelta;
    particles[id]=p;
}
