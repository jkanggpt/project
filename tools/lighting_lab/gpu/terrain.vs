uniform vec2 uOrigin,uViewport;
uniform float uScale;
out vec2 groundWorld;
void main(){
    const vec2 corners[3]=vec2[3](vec2(0,0),vec2(2,0),vec2(0,2));
    vec2 p=corners[gl_VertexID];
    groundWorld=uOrigin+p*uViewport/uScale;
    gl_Position=vec4(p.x*2.0-1.0,1.0-p.y*2.0,.998,1);
}
