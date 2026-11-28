layout(local_size_x=8,local_size_y=8) in;
layout(rgba16f,binding=0) writeonly uniform image2D fieldImage;
uniform ivec2 uChunk,uPageOffset;
void main(){
    ivec2 p=ivec2(gl_GlobalInvocationID.xy);
    if(any(greaterThanEqual(p,ivec2(192))))return;
    vec2 world=vec2(uChunk*128+p-32)+.5;
    imageStore(fieldImage,uPageOffset+p,terrainRaw(world));
}
