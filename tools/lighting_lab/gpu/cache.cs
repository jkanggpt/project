layout(local_size_x=8,local_size_y=8) in;
layout(r8,binding=0) writeonly uniform image2D sunCache;
layout(rgba8,binding=1) writeonly uniform image2D pointCache;
uniform ivec4 uRegion;
void main(){
    ivec2 relative=ivec2(gl_GlobalInvocationID.xy);
    if(any(greaterThanEqual(relative,uRegion.zw)))return;
    ivec2 at=uRegion.xy+relative;
    if(any(lessThan(at,ivec2(0)))||any(greaterThanEqual(at,imageSize(sunCache))))return;
    vec2 world=vec2(at)+.5;
    float dither=bayerValue(at);
    float sun=uSunEnabled!=0?shadowVisibility(vec3(world,0),world,-1,0,0u,dither):1.0;
    vec4 points=vec4(1);
    for(int k=0;k<min(4,uLightCount);k++){
        if((lights[k].tags.z&1u)==0u&&distance(vec3(world,0),lights[k].position.xyz)<lights[k].position.w)
            points[k]=shadowVisibility(vec3(world,0),world,k,0,0u,dither);
    }
    imageStore(sunCache,at,vec4(sun));imageStore(pointCache,at,points);
}
