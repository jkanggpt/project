/* 칸마다 겹치는 광원 번호 목록을 만듭니다. 목록이 한도를 넘으면 넘침 표식을 세우고,
   조명 단계는 그 칸에서 전체 광원을 순회하므로 어떤 광원도 조용히 빠지지 않습니다. */
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
    int offset=(tile.y*uTileSize.x+tile.x)*uLightTileStride,limit=uLightTileStride-2;
    uint count=0u,overflow=0u;
    for(int i=0;i<uLightCount;i++){
        Light light=lights[i];
        /* 투영 높이를 포함해 지면과 높은 원화 수신면을 함께 보수적으로 묶습니다. */
        vec2 center=vec2(light.position.x,light.position.y-light.position.z*.9);
        float radius=light.position.w;
        vec2 extent=vec2(radius,radius*1.9);
        if(any(greaterThan(center-extent,hi))||any(lessThan(center+extent,lo)))continue;
        if(int(count)<limit)lightWords[offset+2+int(count++)]=uint(i);else overflow=1u;
    }
    lightWords[offset]=count;lightWords[offset+1]=overflow;
}
