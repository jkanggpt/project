layout(local_size_x=8,local_size_y=8) in;
layout(rgba8,binding=0) uniform image2D outputImage;
uniform sampler2D uSmokeLayer;
void main(){
    ivec2 p=ivec2(gl_GlobalInvocationID.xy);
    if(any(greaterThanEqual(p,imageSize(outputImage))))return;
    vec4 smoke=texelFetch(uSmokeLayer,p,0);
    float opacity=min(smoke.a,.375);
    vec3 ground=imageLoad(outputImage,p).rgb;
    vec3 color=smoke.a>.0001?smoke.rgb*(opacity/smoke.a):vec3(0);
    imageStore(outputImage,p,vec4(color+ground*(1.0-opacity),1));
}
