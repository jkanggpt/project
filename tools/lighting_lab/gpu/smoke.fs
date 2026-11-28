in vec2 puffUv;
flat in vec4 puffData;
uniform sampler2D uNormalHeight,uPuffs;
uniform vec2 uViewport;
uniform float uAmbient;
uniform int uNatural;
out vec4 finalColor;
void main(){
    float z=texelFetch(uNormalHeight,ivec2(gl_FragCoord.xy),0).w;
    if(puffData.y<z-1.5)discard;
    vec2 uv=(floor(puffUv*16.0)+.5)/16.0;
    vec4 puff;
    if(uNatural!=0){
        vec2 q=(puffUv-.5)*2.0;
        float shape=2.0;
        uint seed=uint(puffData.z*65535.0);
        for(uint i=0u;i<5u;i++){
            uint h=hashWord(seed+i*31u);
            vec2 center=vec2(randomUnit(h)-.5,randomUnit(h+1u)-.5)*.83;
            float radius=.40+randomUnit(h+2u)*.23;
            shape=min(shape,length(q-center)-radius);
        }
        if(shape>0.0)discard;
        float shade=floor((noiseValue(q*2.4+puffData.z*11.0)+.12)*4.0)/4.0;
        puff=vec4(paletteSmoke(.38+shade*.28),1);
    }else puff=texture(uPuffs,vec2((floor(puffData.z*8.0)+uv.x)/8.0,(1.0+uv.y)*.5));
    if(puff.a<.5)discard;
    float opacity=(1.0-puffData.x)*smoothstep(0.0,.08,puffData.x)*(uNatural!=0?.125:.20);
    opacity=floor(opacity*16.0+(uNatural!=0?.5:0.0))/16.0;
    vec3 color=puff.rgb*(.64+clamp(uAmbient,0.0,1.0)*.60);
    finalColor=vec4(color*opacity,opacity);
}
