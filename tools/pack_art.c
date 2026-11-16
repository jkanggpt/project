/* pack_art.c — 생성 원본을 자르고 투명한 발밑 기준점에 맞추는 C 에셋 도구 */
#include "raylib.h"
#include "storage.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;
static const char *const names[16]={"keep","house","farm","gatherer","lumber","quarry","mana_mine","warehouse","market","barracks","tower","wall","gate","healer","mage_tower","construction"};
static const char *const resources[6]={"gold","mana","food","wood","stone","herbs"};
static Image read_image(const char *dir,const char *name)
{
    char path[GAME_PATH_BYTES]; int n=0; unsigned char *bytes; Image result={0};
    if (!game_join_path(path,sizeof(path),dir,name)) return result;
    bytes=game_read_bytes(path,&n);
    if (bytes) { result=LoadImageFromMemory(".png",bytes,n); free(bytes); }
    if (!result.data) { fprintf(stderr,"Missing image: %s\n",path); failures++; }
    else ImageFormat(&result,PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    return result;
}
static void write_image(const char *dir,const char *name,Image image)
{
    char path[GAME_PATH_BYTES]; int n=0; unsigned char *bytes=ExportImageToMemory(image,".png",&n);
    if (!bytes || !game_join_path(path,sizeof(path),dir,name) || !game_write_bytes(path,bytes,(size_t)n)) {
        fprintf(stderr,"Cannot export: %s\n",name); failures++;
    }
    if (bytes) MemFree(bytes);
}
static Image cell(Image source,int col,int row,int cols,int rows)
{
    int x=col*source.width/cols,y=row*source.height/rows;
    return ImageFromImage(source,(Rectangle){(float)x,(float)y,(float)((col+1)*source.width/cols-x),(float)((row+1)*source.height/rows-y)});
}
static void trim(Image *image)
{
    Color *pixels=(Color *)image->data; int i;
    /* 생성기의 아주 옅은 외곽 광택을 지워 픽셀 경계와 투명 여백을 일정하게 합니다. */
    for (i=0;i<image->width*image->height;i++) {
        if (pixels[i].a<96) pixels[i]=(Color){0,0,0,0};
        else pixels[i].a=255;
    }
    ImageAlphaCrop(image,0.1f);
}
static void fit(Image *image,int width,int height)
{
    float scale=(float)width/image->width,vertical=(float)height/image->height;
    int w,h;
    if (vertical<scale) scale=vertical;
    w=(int)(image->width*scale+0.5f); h=(int)(image->height*scale+0.5f);
    ImageResizeNN(image,w>0?w:1,h>0?h:1);
}
static void paste(Image *target,Image image,int x,int y)
{
    ImageDraw(target,image,(Rectangle){0,0,(float)image.width,(float)image.height},(Rectangle){(float)x,(float)y,(float)image.width,(float)image.height},WHITE);
}
static void characters(const char *in,const char *out,const char *name,int rows)
{
    Image source=read_image(in,name),atlas=GenImageColor(4*96,rows*96,BLANK); int x,y;
    if (!source.data) { UnloadImage(atlas); return; }
    for (y=0;y<rows;y++) for (x=0;x<4;x++) {
        Image frame=cell(source,x,y,4,rows); trim(&frame); fit(&frame,96,96);
        paste(&atlas,frame,x*96+(96-frame.width)/2,(y+1)*96-frame.height); UnloadImage(frame);
    }
    write_image(out,name,atlas); UnloadImage(atlas); UnloadImage(source);
}
static void buildings(const char *in,const char *out)
{
    Image source=read_image(in,"buildings.png"); int i;
    if (!source.data) return;
    for (i=0;i<16;i++) {
        Image frame=cell(source,i%4,i/4,4,4); char name[64]; trim(&frame);
        /* 넓은 견본 성벽의 가운데를 한 칸짜리 세로 벽체로 사용합니다. */
        if (i==11) { ImageCrop(&frame,(Rectangle){frame.width*0.25f,0,frame.width*0.5f,(float)frame.height}); trim(&frame); }
        fit(&frame,256,384); (void)snprintf(name,sizeof(name),"%s.png",names[i]);
        write_image(out,name,frame); UnloadImage(frame);
    }
    UnloadImage(source);
}
static void props(const char *in,const char *out)
{
    Image source=read_image(in,"props.png"),atlas=GenImageColor(4*256,3*256,BLANK); int i;
    if (!source.data) { UnloadImage(atlas); return; }
    for (i=0;i<12;i++) {
        /* 원본 생성 결과의 실제 행 여백: 수관 / 광석 / 작은 풀 순서입니다. */
        int top=i<4?0:(i<8?400:700),bottom=i<4?400:(i<8?700:1024);
        Image frame=ImageFromImage(source,(Rectangle){(float)(i%4*source.width/4),top*(float)source.height/1024,source.width/4.0f,(bottom-top)*(float)source.height/1024});
        trim(&frame); fit(&frame,256,256);
        paste(&atlas,frame,i%4*256+(256-frame.width)/2,(i/4+1)*256-frame.height); UnloadImage(frame);
    }
    write_image(out,"props.png",atlas); UnloadImage(atlas); UnloadImage(source);
}
static void ground(const char *in,const char *out)
{
    Image source=read_image(in,"ground.png"),atlas=GenImageColor(256,320,BLANK),snow=GenImageColor(256,64,BLANK); int x,y;
    if (!source.data) { UnloadImage(atlas); UnloadImage(snow); return; }
    for (y=0;y<6;y++) for (x=0;x<4;x++) {
        Image tile=cell(source,x,y,4,6); ImageResizeNN(&tile,64,64);
        if (y<5) paste(&atlas,tile,x*64,y*64); else paste(&snow,tile,x*64,0);
        UnloadImage(tile);
    }
    write_image(out,"terrain.png",atlas); write_image(out,"snow.png",snow);
    UnloadImage(atlas); UnloadImage(snow); UnloadImage(source);
}
static void icons(const char *in,const char *out)
{
    Image source=read_image(in,"icons.png"); int i;
    if (!source.data) return;
    for (i=0;i<7;i++) {
        Image frame=cell(source,i%4,i/4,4,2); char name[64]; trim(&frame);
        fit(&frame,i==6?96:64,i==6?32:64);
        (void)snprintf(name,sizeof(name),i==6?"arrow.png":"res_%s.png",i==6?"":resources[i]);
        write_image(out,name,frame); UnloadImage(frame);
    }
    {
        Image path=cell(source,3,1,4,2),atlas=GenImageColor(128,64,BLANK);
        /* 가장자리 투명 장식 대신 가운데의 연속 포장면을 타일로 사용합니다. */
        ImageCrop(&path,(Rectangle){path.width*0.1f,path.height*0.1f,path.width*0.8f,path.height*0.8f});
        ImageResizeNN(&path,64,64); paste(&atlas,path,0,0); ImageFlipHorizontal(&path); paste(&atlas,path,64,0);
        write_image(out,"roads.png",atlas); UnloadImage(path); UnloadImage(atlas);
    }
    UnloadImage(source);
}
static void check_file(const char *dir,const char *name,int width,int height,int transparent)
{
    Image image=read_image(dir,name); int i,visible=0,clear=0; Color *pixels;
    if (!image.data) return;
    if ((width && image.width!=width) || (height && image.height!=height)) { fprintf(stderr,"Wrong atlas size: %s\n",name); failures++; }
    pixels=(Color *)image.data;
    for (i=0;i<image.width*image.height;i++) { if (pixels[i].a) visible++; else clear++; }
    if (!visible || (transparent && !clear) || (!transparent && clear)) { fprintf(stderr,"Wrong alpha: %s\n",name); failures++; }
    UnloadImage(image);
}
static void check(const char *dir)
{
    int i; char name[64];
    check_file(dir,"villagers.png",384,768,1); check_file(dir,"enemies.png",384,576,1);
    check_file(dir,"props.png",1024,768,1); check_file(dir,"terrain.png",256,320,0);
    check_file(dir,"snow.png",256,64,0); check_file(dir,"roads.png",128,64,0);
    for (i=0;i<16;i++) { (void)snprintf(name,sizeof(name),"%s.png",names[i]); check_file(dir,name,0,0,1); }
    for (i=0;i<6;i++) { (void)snprintf(name,sizeof(name),"res_%s.png",resources[i]); check_file(dir,name,0,0,1); }
    check_file(dir,"arrow.png",0,0,1);
}
int main(int argc,char **argv)
{
    GameArguments args; int result;
    if (!game_arguments(&args,argc,argv)) return 2;
    SetTraceLogLevel(LOG_WARNING);
    if (args.count!=3) { fprintf(stderr,"castle_art <source> <output> | --check <output>\n"); game_arguments_free(&args); return 2; }
    if (!strcmp(args.values[1],"--check")) check(args.values[2]);
    else if (game_make_dirs(args.values[2])) {
        characters(args.values[1],args.values[2],"villagers.png",8); characters(args.values[1],args.values[2],"enemies.png",6);
        buildings(args.values[1],args.values[2]); props(args.values[1],args.values[2]); ground(args.values[1],args.values[2]); icons(args.values[1],args.values[2]); check(args.values[2]);
    } else failures++;
    result=failures?1:0; if (!result) puts("ART PASS: 29 PNG assets, atlas dimensions and alpha checked");
    game_arguments_free(&args); return result;
}
