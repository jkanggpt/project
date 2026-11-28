layout(local_size_x=8,local_size_y=8) in;
layout(rgba8,binding=0) writeonly uniform image2D outputImage;
uniform sampler2D uAlbedo,uNormalHeight,uMaterial,uOwner,uSunCache,uPointCache;
uniform vec2 uOrigin,uViewport,uMap;
uniform float uScale,uAmbient,uElevation;
uniform int uShadows,uView;
uint ownerAt(ivec2 p){uvec4 b=uvec4(round(texelFetch(uOwner,p,0)*255.0));return b.x|(b.y<<8)|(b.z<<16)|(b.w<<24);}
float visible(vec3 world,vec2 screen,int index,uint owner,float dither){
    if(uShadows==0)return 1.0;
    bool ground=owner==0u&&world.z<.01;
    if(!ground)return shadowVisibility(world,screen,index,2,owner,dither);
    float fixedVisibility=1.0;
    ivec2 at=ivec2(floor(world.xy));
    if(all(greaterThanEqual(at,ivec2(0)))&&all(lessThan(at,ivec2(uMap)))){
        if(index<0)fixedVisibility=texelFetch(uSunCache,at,0).r;
        else if(index<4)fixedVisibility=texelFetch(uPointCache,at,0)[index];
        else fixedVisibility=shadowVisibility(world,screen,index,0,0u,dither);
    }
    return min(fixedVisibility,shadowVisibility(world,screen,index,1,0u,dither));
}
void main(){
    ivec2 pixel=ivec2(gl_GlobalInvocationID.xy);
    if(any(greaterThanEqual(pixel,ivec2(uViewport))))return;
    vec4 albedo=texelFetch(uAlbedo,pixel,0);
    if(albedo.a<.05){imageStore(outputImage,pixel,vec4(.07,.09,.10,1));return;}
    vec4 nh=texelFetch(uNormalHeight,pixel,0),material=texelFetch(uMaterial,pixel,0);
    vec3 normal=normalize(nh.xyz);
    int kind=int(round(material.r*255.0));
    if(uView==1){imageStore(outputImage,pixel,vec4(albedo.rgb,1));return;}
    if(uView==2){imageStore(outputImage,pixel,vec4(normal*.5+.5,1));return;}
    if(kind==0){imageStore(outputImage,pixel,vec4(albedo.rgb,1));return;}
    vec2 screen=uOrigin+vec2(float(pixel.x)+.5,uViewport.y-float(pixel.y)-.5)/uScale;
    vec3 world=vec3(screen.x,screen.y+nh.w*.9,nh.w);
    uint owner=ownerAt(pixel);
    float dither=bayerValue(ivec2(floor(screen)));
    float contact=uShadows!=0&&owner==0u?contactVisibility(world.xy,dither):1.0;
    vec3 radiance=vec3(.97,1,.97)*uAmbient*contact;
    float ndl=max(0.0,dot(normal,uSunDirection));ndl=floor(ndl*12.0+dither*.3)/12.0;
    float sunVisibility=1.0;
    if(uSunEnabled!=0){
        sunVisibility=ndl>0.0?visible(world,screen,-1,owner,dither):1.0;
        vec3 color=uElevation<25.0?vec3(.72,.49,.30):vec3(.70,.67,.59);
        radiance+=color*ndl*sunVisibility;
    }
    float specular=0.0,localVisibility=1.0;
    for(int k=0;k<uLightCount;k++){
        Light light=lights[k];vec3 direction=light.position.xyz-world;float range=length(direction);
        if(range>=light.position.w)continue;
        float visibility=visible(world,screen,k,owner,dither);
        localVisibility=min(localVisibility,visibility);
        direction/=max(.01,range);
        float diffuse=max(0.0,dot(normal,direction));diffuse=floor(diffuse*12.0+dither*.3)/12.0;
        float attenuation=pow(1.0-range/light.position.w,2.0)*light.color.w*visibility;
        radiance+=light.color.rgb*diffuse*attenuation;
        vec3 halfway=normalize(direction+vec3(0,.70710678,.70710678));
        float power=kind==3?48.0:kind==5?26.0:36.0;
        specular+=floor(pow(max(0.0,dot(normal,halfway)),power)*4.0)/4.0*(kind==3?.012:.025)*attenuation;
    }
    float lampStrength=clamp((.55-uAmbient)/.4,0.0,1.0);
    vec3 emission=vec3(1,.68,.25)*material.g*(material.g>.8?.40:.28)*lampStrength;
    vec3 color=albedo.rgb*sqrt(clamp(radiance,vec3(.035),vec3(1.55)))+specular*vec3(1,.9,.7)+emission;
    if(uView==3)color=vec3(sunVisibility);
    if(uView==4)color=vec3(localVisibility);
    imageStore(outputImage,pixel,vec4(clamp(color,0.0,1.0),1));
}
