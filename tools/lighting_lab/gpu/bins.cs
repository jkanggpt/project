layout(local_size_x=8,local_size_y=8) in;
bool overlaps(vec4 bounds,vec2 lo,vec2 hi){return all(greaterThanEqual(bounds.zw,lo))&&all(lessThanEqual(bounds.xy,hi));}
void main(){
    ivec2 tile=ivec2(gl_GlobalInvocationID.xy);
    if(any(greaterThanEqual(tile,uTileSize)))return;
    int offset=(tile.y*uTileSize.x+tile.x)*TILE_STRIDE;
    vec2 lo=uTileOrigin+vec2(tile)*16.0,hi=lo+16.0;
    uint count=0u,overflow=0u;
    for(int i=0;i<uCasterCount;i++){
        Caster c=casters[i];
        float top=c.tags.z==2u?c.center.z+c.extent.z:c.center.z+c.extent.z;
        vec4 bounds=vec4(c.center.xy-c.extent.xy-vec2(0,.9*top),c.center.xy+c.extent.xy);
        if(c.contactProfile.x>.5&&c.contactProfile.z>0.0){
            bounds.xy=min(bounds.xy,c.contact.xy-vec2(32,48)*c.contact.zw);
            bounds.zw=max(bounds.zw,c.contact.xy+vec2(32,1)*c.contact.zw);
        }
        if(uSunEnabled!=0){
            vec2 push=-uSunDirection.xy/max(.05,uSunDirection.z)*top;
            bounds.xy=min(bounds.xy,c.center.xy-c.extent.xy+push);
            bounds.zw=max(bounds.zw,c.center.xy+c.extent.xy+push);
        }
        for(int k=0;k<uLightCount;k++){
            Light light=lights[k];
            if(distance(c.center.xyz,light.position.xyz)>light.position.w+length(c.extent.xyz))continue;
            vec4 lamp=vec4(light.position.xy-light.position.w*vec2(1,1.35)-vec2(0,.9*light.position.z),light.position.xy+light.position.w*vec2(1,1.35));
            bounds.xy=min(bounds.xy,lamp.xy);bounds.zw=max(bounds.zw,lamp.zw);
        }
        if(!overlaps(bounds+vec4(-3,-3,3,3),lo,hi))continue;
        if(count<uint(TILE_LIMIT))tileWords[offset+2+int(count++)]=uint(i);else overflow=1u;
    }
    tileWords[offset]=count;tileWords[offset+1]=overflow;
}
