layout(std430,binding=0) readonly buffer Particles{Particle particles[];};
uniform vec2 uOrigin,uViewport;
uniform float uScale;
uniform int uNatural;
uniform float uPuffScale;
out vec2 puffUv;
out vec3 puffWorld;
flat out vec4 puffData;
void main(){
    Particle p=particles[gl_InstanceID];
    const vec2 corners[6]=vec2[6](vec2(0,0),vec2(1,0),vec2(1,1),vec2(0,0),vec2(1,1),vec2(0,1));
    vec2 uv=corners[gl_VertexID];float age=clamp(p.position.w/max(.1,p.velocity.w),0.0,1.0);
    float size=p.settings.x*(1.0+age*4.2)*uPuffScale;
    vec2 center=vec2(p.position.x,p.position.y-p.position.z*.9);
    vec2 world=center+(uv-.5)*size*2.0;
    vec2 pixel=(world-uOrigin)*uScale;
    gl_Position=p.settings.w==0.0?vec4(3,3,0,1):vec4(pixel.x/uViewport.x*2.0-1.0,1.0-pixel.y/uViewport.y*2.0,0,1);
    puffWorld=vec3(world.x,world.y+p.position.z*.9,p.position.z);
    puffUv=uv;puffData=vec4(age,p.position.z,p.settings.y,size);
}
