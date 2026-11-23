/* pack_ui_art.c — 생성 원화와 기존 주민의 어깨 위를 게임용 PNG로 묶습니다. */
#include "raylib.h"
#include "storage.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int errors;
static int seam(Image im,int n,int count,int vertical,int start,int end) {
    int extent=vertical?im.width:im.height;
    if(!n)return 0;
    if(n==count)return extent;
    int nominal=n*extent/count,best=nominal,score=2147483647,radius=extent/count/3;
    const Color *pixels=im.data;
    for(int p=nominal-radius;p<=nominal+radius;p++) {
        int found=0;
        for(int q=start;q<end;q++)found+=pixels[vertical?q*im.width+p:p*im.width+q].a>=24;
        int cost=found*100+abs(p-nominal);
        if(cost<score){score=cost;best=p;}
    }
    return best;
}
static Image read_image(const char *dir,const char *name) {
    char path[GAME_PATH_BYTES];int size=0;Image im={0};
    if(!game_join_path(path,sizeof(path),dir,name)){errors++;return im;}
    unsigned char *bytes=game_read_bytes(path,&size);
    if(bytes){im=LoadImageFromMemory(".png",bytes,size);free(bytes);}
    if(!im.data){fprintf(stderr,"Missing image: %s\n",path);errors++;return im;}
    ImageFormat(&im,PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);return im;
}
static void write_image(const char *dir,const char *name,Image im) {
    char path[GAME_PATH_BYTES];int size=0;unsigned char *bytes=ExportImageToMemory(im,".png",&size);
    if(!bytes || !game_join_path(path,sizeof(path),dir,name) || !game_write_bytes(path,bytes,(size_t)size))errors++;
    MemFree(bytes);
}
static void sheet(const char *input,const char *output,const char *name,int columns,int rows,int size,int trim) {
    Image raw=read_image(input,name),atlas=GenImageColor(columns*size,rows*size,BLANK);
    if(!raw.data){UnloadImage(atlas);return;}
    for(int y=0;y<rows;y++)for(int x=0;x<columns;x++) {
        int top=seam(raw,y,rows,0,0,raw.width),bottom=seam(raw,y+1,rows,0,0,raw.width);
        int left=seam(raw,x,columns,1,top,bottom),right=seam(raw,x+1,columns,1,top,bottom);
        Image im=ImageFromImage(raw,(Rectangle){(float)left,(float)top,(float)(right-left),(float)(bottom-top)});
        Color *pixels=im.data;for(int p=0;p<im.width*im.height;p++)if(pixels[p].a<24)pixels[p]=BLANK;
        if(trim)ImageAlphaCrop(&im,.12f);
        float scale=(size-8.0f)/(im.width>im.height?im.width:im.height);
        int w=(int)(im.width*scale+.5f),h=(int)(im.height*scale+.5f);
        ImageResizeNN(&im,w,h);
        ImageDraw(&atlas,im,(Rectangle){0,0,(float)w,(float)h},(Rectangle){x*(float)size+(size-w)/2,y*(float)size+(size-h)/2,(float)w,(float)h},WHITE);
        UnloadImage(im);
    }
    write_image(output,name,atlas);UnloadImage(atlas);UnloadImage(raw);
}
static void portraits(const char *output) {
    Image residents=read_image(output,"villagers_v122.png"),atlas=GenImageColor(512,384,BLANK);
    if(!residents.data){UnloadImage(atlas);return;}
    for(int i=0;i<48;i++) {
        Image front=ImageFromImage(residents,(Rectangle){96,i*96.0f,96,96});
        Rectangle bounds=GetImageAlphaBorder(front,.1f);
        float height=bounds.height*.48f,side=height+4;
        Rectangle crop={48-side*.5f,bounds.y-2,side,side};
        if(crop.x<0)crop.x=0;
        if(crop.y<0)crop.y=0;
        Image face=ImageFromImage(front,crop);ImageResizeNN(&face,60,60);
        ImageDraw(&atlas,face,(Rectangle){0,0,60,60},(Rectangle){(i%8)*64.0f+2,(i/8)*64.0f+2,60,60},WHITE);
        UnloadImage(face);UnloadImage(front);
    }
    write_image(output,"portraits.png",atlas);UnloadImage(atlas);UnloadImage(residents);
}
static void check(const char *dir,const char *name,int columns,int rows,int size) {
    Image im=read_image(dir,name);if(!im.data)return;
    if(im.width!=columns*size || im.height!=rows*size){errors++;UnloadImage(im);return;}
    Color *pixels=im.data;
    for(int r=0;r<rows;r++)for(int c=0;c<columns;c++) {
        int count=0,border=0;
        for(int y=0;y<size;y++)for(int x=0;x<size;x++)if(pixels[(r*size+y)*im.width+c*size+x].a>16){count++;if(x<2||y<2||x>=size-2||y>=size-2)border++;}
        if(!count||border){fprintf(stderr,"Invalid cell %s %d,%d: %d pixels / %d border\n",name,c,r,count,border);errors++;}
    }
    UnloadImage(im);
}
int main(int argc,char **argv) {
    GameArguments arguments;if(!game_arguments(&arguments,argc,argv))return 2;
    argc=arguments.count;argv=arguments.values;
    SetTraceLogLevel(LOG_WARNING);
    const char *out="game/assets/expansion";
    if(argc>1 && !strcmp(argv[1],"--check")) {
        if(argc>2)out=argv[2];
        check(out,"portraits.png",8,6,64);check(out,"combat_fx.png",6,8,128);check(out,"factions.png",3,2,128);
    } else {
        portraits(out);sheet("art/v122_patch1",out,"combat_fx.png",6,8,128,0);sheet("art/v122_patch1",out,"factions.png",3,2,128,1);
    }
    game_arguments_free(&arguments);return errors?1:0;
}
