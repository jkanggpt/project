/* 각 칸은 모든 겹치는 광원을 담을 만큼의 공간을 배정받습니다. */
layout(local_size_x=8,local_size_y=8) in;
layout(std430,binding=1) readonly buffer Lights {Light lights[];};
layout(std430,binding=3) writeonly buffer LightTiles {uint lightWords[];};
uniform int uLightCount,uLightTileStride;
uniform ivec2 uTileSize;
uniform vec2 uTileOrigin;
void main(){
    ivec2 tile=ivec2(gl_GlobalInvocationID.xy);
    if(any(greaterThanEqual(tile,uTileSize)))return;
    vec2 lo=uTileOrigin+vec2(tile)*16.0,hi=lo+16.0;
    int offset=(tile.y*uTileSize.x+tile.x)*uLightTileStride;
    uint count=0u;
    for(int i=0;i<uLightCount;i++){
        Light light=lights[i];
        /* 투영 높이를 포함해 지면과 높은 원화 수신면을 함께 보수적으로 묶습니다. */
        vec2 center=vec2(light.position.x,light.position.y-light.position.z*.9);
        float radius=light.position.w;
        vec2 extent=vec2(radius,radius*1.9);
        if(any(greaterThan(center-extent,hi))||any(lessThan(center+extent,lo)))continue;
        lightWords[offset+1+int(count++)]=uint(i);
    }
    lightWords[offset]=count;
}
