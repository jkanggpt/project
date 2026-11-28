/* 원화 알파의 아래 윤곽만 지면에 쓰며 상체를 종이처럼 눕히지 않습니다. */
layout(local_size_x=64) in;
layout(std430,binding=0) buffer Fronts { vec2 fronts[]; };
layout(r8,binding=0) writeonly uniform image2D contactImage;
layout(rgba16f,binding=1) writeonly uniform image2D frontImage;
uniform sampler2D uSourceTexture,uContactPolicy;
uniform vec4 uSource;
uniform ivec2 uPage;
uniform int uPass,uKind;
vec4 policy(){return texelFetch(uContactPolicy,ivec2(clamp(uKind,0,7),0),0)*255.0;}
bool supported(vec2 span){return span.x<1000.0&&(uKind!=3||span.x>=fronts[256].x-2.0);}
void main(){
    uint id=gl_GlobalInvocationID.x;
    vec4 rules=policy();
    if(uPass==0){
        if(id>=256u)return;
        float x=(float(id)+.5)*.5-64.0;
        float front=1024.0;
        if(abs(x)<32.0){
            ivec2 size=textureSize(uSourceTexture,0);
            int height=max(1,int(abs(uSource.w)*float(size.y)+.5));
            float width=max(1.0,abs(uSource.z)*float(size.x));
            float strip=clamp(rules.r/255.0,.04,.6);
            for(int y=height-1;y>=int(float(height)*(1.0-strip));y--){
                vec2 uv=uSource.xy+vec2(x/64.0+.5,(float(y)+.5)/float(height))*uSource.zw;
                if(texture(uSourceTexture,uv).a>=.5){
                    front=(float(y+1)-float(height))*64.0/width;
                    break;
                }
            }
        }
        fronts[id]=vec2(front,front-rules.g);
        return;
    }
    if(uPass==1){
        if(id!=0u)return;
        float lowest=-1024.0;
        for(int x=0;x<256;x++)if(fronts[x].x<1000.0)lowest=max(lowest,fronts[x].x);
        fronts[256]=vec2(lowest);return;
    }
    if(id>=256u*128u)return;
    ivec2 at=ivec2(int(id)%256,int(id)/256);
    if(at.y==0){
        float nearest=1024.0;int closest=257;
        for(int x=0;x<256;x++)if(supported(fronts[x])&&abs(x-at.x)<closest){
            closest=abs(x-at.x);nearest=fronts[x].x;
        }
        imageStore(frontImage,ivec2(at.x,uPage.y/128*16+uPage.x/256),
            vec4(nearest,fronts[at.x].x,supported(fronts[at.x])?1.0:0.0,closest<257?1.0:0.0));
    }
    vec2 p=(vec2(at)+.5)*.5-vec2(64,48);
    float best=4.0;bool inside=false;
    vec2 own=fronts[at.x];
    if(supported(own))inside=p.y<=own.x&&p.y>=own.y;
    for(int dx=-8;dx<=8;dx++){
        int x=at.x+dx;
        if(x<0||x>=256)continue;
        vec2 span=fronts[x];
        if(!supported(span))continue;
        float outside=max(max(span.y-p.y,p.y-span.x),0.0);
        float distance=length(vec2(max(abs(float(dx))*.5-.25,0.0),outside));
        if(inside){
            distance=abs(float(dx))*.5+min(abs(p.y-span.x),abs(p.y-span.y));
            if(x>0&&!supported(fronts[x-1]))distance=min(distance,abs(float(dx))*.5+.25);
            if(x<255&&!supported(fronts[x+1]))distance=min(distance,abs(float(dx))*.5+.25);
        }
        best=min(best,distance);
    }
    float signedDistance=inside?-max(.125,best):max(.125,best);
    imageStore(contactImage,uPage+at,vec4(clamp(.5+signedDistance/8.0,0.0,1.0),0,0,1));
}
