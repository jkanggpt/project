/* pack_v121_art.c — 방향 동작·연결 도로·성별 외형과 수목 영역을 패킹합니다. */
#include "raylib.h"
#include "storage.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
static int errors;
static const void *cached_pixels;
static int cached_rows,cached_cuts[65];
static int *row_scores,*row_parent;
static int64_t *row_cost;
static const void *column_pixels;
static int column_top,column_bottom,column_count,column_cuts[65];
static void optimal_cuts(const int *scores,int extent,int count,int *cuts) {
    size_t slots=(size_t)(count+1)*(extent+1);
    int64_t *cost=malloc(slots*sizeof(*cost));int *parent=calloc(slots,sizeof(*parent));
    if(!cost||!parent){errors++;free(cost);free(parent);for(int i=0;i<=count;i++)cuts[i]=i*extent/count;return;}
    for(size_t i=0;i<slots;i++)cost[i]=INT64_MAX/4;
    cost[0]=0;int nominal=extent/count,low=nominal/2,high=nominal*17/10;
    for(int row=1;row<=count;row++)for(int y=row*low;y<=extent && y<=row*high;y++) {
        if(row==count && y!=extent)continue;
        for(int length=low;length<=high && length<=y;length++) {
            int previous=y-length;int64_t value=cost[(size_t)(row-1)*(extent+1)+previous]+(int64_t)(length-nominal)*(length-nominal)/8;
            if(row<count)value+=(int64_t)scores[y]*1000;
            size_t index=(size_t)row*(extent+1)+y;
            if(value<cost[index]){cost[index]=value;parent[index]=previous;}
        }
    }
    int y=extent;cuts[count]=y;
    for(int row=count;row>0;row--){y=parent[(size_t)row*(extent+1)+y];cuts[row-1]=y;}
    free(cost);free(parent);
}
static Image read_png(const char *dir,const char *name) {
    cached_pixels=NULL;
    column_pixels=NULL;
    char path[GAME_PATH_BYTES]; int count = 0; Image im = {0};
    if (!game_join_path(path,sizeof(path),dir,name)) { errors++; return im; }
    unsigned char *p = game_read_bytes(path,&count);
    if (p) { im = LoadImageFromMemory(".png",p,count); free(p); }
    if (!im.data) errors++; else ImageFormat(&im,PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    return im;
}
static void save_png(const char *dir,const char *name,Image im) {
    char path[GAME_PATH_BYTES]; int count = 0; unsigned char *p = ExportImageToMemory(im,".png",&count);
    if (!p || !game_join_path(path,sizeof(path),dir,name) || !game_write_bytes(path,p,(size_t)count)) errors++;
    MemFree(p);
}
static void clean(Image *im) {
    Color *p = im->data;
    for (int i = 0; i < im->width * im->height; i++) if (p[i].a < 96) p[i] = BLANK; else p[i].a = 255;
}
static int seam(Image im,int n,int count,int vertical,int start,int end) {
    int extent = vertical ? im.width : im.height;
    if (!n) return 0;
    if (n == count) return extent;
    if(vertical && count<=64) {
        if(column_pixels!=im.data || column_top!=start || column_bottom!=end || column_count!=count) {
            int *scores=calloc((size_t)im.width+1,sizeof(int));
            if(!scores){errors++;return n*extent/count;}
            const Color *pixels=im.data;
            for(int x=0;x<im.width;x++)for(int y=start;y<end;y++)scores[x]+=pixels[y*im.width+x].a>=96;
            optimal_cuts(scores,im.width,count,column_cuts);free(scores);
            column_pixels=im.data;column_top=start;column_bottom=end;column_count=count;
        }
        return column_cuts[n];
    }
    if (!vertical && count<=64) {
        if(cached_pixels!=im.data || cached_rows!=count) {
            size_t slots=(size_t)(count+1)*(im.height+1);
            free(row_scores);free(row_parent);free(row_cost);
            row_scores=calloc((size_t)im.height+1,sizeof(int));row_parent=malloc(slots*sizeof(int));row_cost=malloc(slots*sizeof(int64_t));
            if(!row_scores||!row_parent||!row_cost){errors++;return n*extent/count;}
            const Color *pixels=im.data;
            for(int y=0;y<im.height;y++)for(int x=0;x<im.width;x++)row_scores[y]+=pixels[y*im.width+x].a>=96;
            for(size_t i=0;i<slots;i++)row_cost[i]=INT64_MAX/4;
            row_cost[0]=0;
            int nominal=im.height/count,low=nominal/2,high=nominal*17/10;
            for(int row=1;row<=count;row++)for(int y=row*low;y<=im.height && y<=row*high;y++) {
                if(row==count && y!=im.height)continue;
                for(int length=low;length<=high && length<=y;length++) {
                    int previous=y-length;int64_t value=row_cost[(size_t)(row-1)*(im.height+1)+previous];
                    value+=(int64_t)(length-nominal)*(length-nominal)/8;
                    if(row<count)value+=(int64_t)row_scores[y]*1000;
                    size_t index=(size_t)row*(im.height+1)+y;
                    if(value<row_cost[index]){row_cost[index]=value;row_parent[index]=previous;}
                }
            }
            int y=im.height;cached_cuts[count]=y;
            for(int row=count;row>0;row--){y=row_parent[(size_t)row*(im.height+1)+y];cached_cuts[row-1]=y;}
            cached_pixels=im.data;cached_rows=count;
        }
        return cached_cuts[n];
    }
    int nominal = n * extent / count, best = nominal, score = INT32_MAX, radius = extent / count / 3;
    Color *p = im.data;
    for (int x = nominal - radius; x <= nominal + radius; x++) {
        int found = 0;
        for (int y = start; y < end; y++) found += p[vertical ? y * im.width + x : x * im.width + y].a >= 96;
        if (found < score || (found == score && abs(x-nominal) < abs(best-nominal))) { best = x; score = found; }
    }
    return best;
}
static Image cut(Image im,int col,int row,int cols,int rows) {
    int top = seam(im,row,rows,0,0,im.width), bottom = seam(im,row+1,rows,0,0,im.width);
    int left = seam(im,col,cols,1,top,bottom), right = seam(im,col+1,cols,1,top,bottom);
    Image result = ImageFromImage(im,(Rectangle){(float)left,(float)top,(float)(right-left),(float)(bottom-top)});
    clean(&result); ImageAlphaCrop(&result,.1f); return result;
}
static void paste(Image *atlas,Image frame,int col,int row,int size,float scale) {
    int w = (int)(frame.width * scale + .5f), h = (int)(frame.height * scale + .5f);
    if (w < 1 || h < 1) { errors++; UnloadImage(frame); return; }
    ImageResizeNN(&frame,w,h);
    ImageDraw(atlas,frame,(Rectangle){0,0,(float)w,(float)h},
        (Rectangle){col*(float)size+(size-w)/2.0f,(row+1)*(float)size-h,(float)w,(float)h},WHITE);
    UnloadImage(frame);
}
static void sequence(Image *atlas,Image src,int source_row,int rows,int target_row,int target_col,int cols,int offset) {
    Image frames[4]; int max = 1;
    for (int f = 0; f < 4; f++) {
        frames[f] = cut(src,f+offset,source_row,cols,rows);
        if (frames[f].width > max) max = frames[f].width;
        if (frames[f].height > max) max = frames[f].height;
    }
    for (int f = 0; f < 4; f++) paste(atlas,frames[f],target_col+f,target_row,96,92.0f/max);
}
static void characters(const char *in,const char *out) {
    Image old = read_png(out,"../painted/villagers.png"), atlas = GenImageColor(1152,1152,BLANK);
    if (old.data) ImageDraw(&atlas,old,(Rectangle){0,0,(float)old.width,(float)old.height},(Rectangle){0,0,(float)old.width,(float)old.height},WHITE);
    const char *names[2] = {"villagers_silver.png","villagers_navy.png"};
    for (int k = 0; k < 2; k++) {
        Image src = read_png(in,names[k]);
        if (src.data) for (int sex = 0; sex < 2; sex++) for (int d = 0; d < 3; d++)
            sequence(&atlas,src,sex*3+d,6,8+k*2+sex,d*4,4,0);
        UnloadImage(src);
    }
    save_png(out,"villagers_v121.png",atlas); UnloadImage(atlas); UnloadImage(old);
}
static void animals(const char *in,const char *out) {
    Image old = read_png(out,"animal_walk.png"), walk = GenImageColor(384,2880,BLANK), attacks = GenImageColor(384,2880,BLANK);
    if (old.data) ImageDraw(&walk,old,(Rectangle){0,0,(float)old.width,(float)old.height},(Rectangle){0,0,(float)old.width,(float)old.height},WHITE);
    Image fight = read_png(in,"animal_attacks.png"), fantasy = read_png(in,"fantasy.png"), raw = read_png(in,"livestock_babies.png");
    const int kinds[9] = {11,12,13,14,15,16,17,19,20};
    if (fight.data) for (int i = 0; i < 9; i++) sequence(&attacks,fight,i,9,kinds[i],0,4,0);
    if (fantasy.data) for (int i = 0; i < 6; i++) {
        sequence(&walk,fantasy,i,6,24+i,0,8,0); sequence(&attacks,fantasy,i,6,24+i,0,8,4);
    }
    Image babies = GenImageColor(384,384,BLANK);
    if (raw.data) for (int i = 0; i < 4; i++) sequence(&babies,raw,i,4,i,0,4,0);
    save_png(out,"animal_walk_v121.png",walk); save_png(out,"animal_attack_v121.png",attacks); save_png(out,"animal_babies.png",babies);
    Image icons = GenImageColor(384,768,BLANK);
    for (int k = 0; k < 30; k++) ImageDraw(&icons,walk,(Rectangle){96,k*96.0f,96,96},(Rectangle){(k%4)*96.0f,(k/4)*96.0f,96,96},WHITE);
    save_png(out,"animals_v121.png",icons);
    UnloadImage(icons); UnloadImage(babies); UnloadImage(raw); UnloadImage(old); UnloadImage(walk); UnloadImage(attacks); UnloadImage(fight); UnloadImage(fantasy);
}
static void raiders(const char *in,const char *out) {
    const char *names[6] = {"raid_bandits.png","raid_monsters.png","raid_undead.png","raid_forest.png","raid_north.png","raid_cult.png"};
    const int counts[6] = {4,5,5,5,4,5};
    Image walk = GenImageColor(1536,2016,BLANK), attack = GenImageColor(1536,2016,BLANK);
    int type = 0;
    for (int k = 0; k < 6; k++) {
        Image src = read_png(in,names[k]);
        if (src.data) for (int n = 0; n < counts[k]; n++,type++) {
            Image frames[24];int max=1;
            for(int d=0;d<3;d++)for(int f=0;f<8;f++) {
                Image im=cut(src,f==7?0:f,n*3+d,8,counts[k]*3);frames[d*8+f]=im;
                if(im.width>max)max=im.width;
                if(im.height>max)max=im.height;
            }
            for(int d=0;d<3;d++)for(int f=0;f<8;f++)paste(f<4?&walk:&attack,frames[d*8+f],(type%4)*4+f%4,(type/4)*3+d,96,92.0f/max);
        }
        UnloadImage(src);
    }
    save_png(out,"raider_walk.png",walk); save_png(out,"raider_attack.png",attack); UnloadImage(walk); UnloadImage(attack);
}
/* 각 행의 연결 성분을 분리해 원본의 불균등한 수목 간격도 안전하게 자릅니다. */
static void trees(const char *out) {
    Image src = read_png("art/v111","trees.png"), atlas = GenImageColor(640,512,BLANK);
    if (!src.data) { UnloadImage(atlas); return; }
    clean(&src); Color *pixels = src.data;
    int cells = src.width * src.height;
    int *labels = calloc((size_t)cells,sizeof(int)), *queue = malloc((size_t)cells * sizeof(int));
    if (!labels || !queue) { errors++; free(labels); free(queue); UnloadImage(src); UnloadImage(atlas); return; }
    for (int row = 0; row < 4; row++) {
        int top = seam(src,row,4,0,0,src.width), bottom = seam(src,row+1,4,0,0,src.width);
        struct Part { int label,area,x0,y0,x1,y1; } parts[2048]; int count = 0;
        for (int y = top; y < bottom; y++) for (int x = 0; x < src.width; x++) {
            int p = y*src.width+x;
            if (labels[p] || !pixels[p].a) continue;
            if (count == 2048) { errors++; continue; }
            int head = 0, tail = 1, label = row*2048+count+1;
            queue[0] = p; labels[p] = label;
            struct Part part = {label,0,x,y,x,y};
            while (head < tail) {
                int q = queue[head++], xx = q%src.width, yy = q/src.width;
                part.area++; if (xx < part.x0) part.x0=xx; if (xx > part.x1) part.x1=xx;
                if (yy < part.y0) part.y0=yy;
                if (yy > part.y1) part.y1=yy;
                for (int dy=-1; dy<=1; dy++) for (int dx=-1; dx<=1; dx++) {
                    int nx=xx+dx,ny=yy+dy;
                    if (nx<0 || nx>=src.width || ny<top || ny>=bottom) continue;
                    int n=ny*src.width+nx;
                    if (!labels[n] && pixels[n].a) { labels[n]=label; queue[tail++]=n; }
                }
            }
            parts[count++]=part;
        }
        if (count < 5) {errors++;continue;}
        int selected[5];
        for (int c=0;c<5;c++) {
            int best=-1;
            for (int n=0;n<count;n++) {
                int used=0;for(int k=0;k<c;k++) used |= selected[k]==n;
                if(!used && (best<0 || parts[n].area>parts[best].area)) best=n;
            }
            selected[c]=best;
        }
        for(int a=0;a<5;a++)for(int b=a+1;b<5;b++)if(parts[selected[a]].x0>parts[selected[b]].x0){int t=selected[a];selected[a]=selected[b];selected[b]=t;}
        for(int c=0;c<5;c++) {
            struct Part body=parts[selected[c]];
            Image frame=GenImageColor(src.width,bottom-top,BLANK); Color *p=frame.data;
            for(int n=0;n<count;n++) {
                struct Part f=parts[n]; int nearest=0,best=INT32_MAX;
                for(int k=0;k<5;k++) {struct Part v=parts[selected[k]];int dx=(f.x0+f.x1-v.x0-v.x1)/2;int d=dx*dx;if(d<best){best=d;nearest=k;}}
                if(nearest!=c || (n!=selected[c] && (f.x1<body.x0-24 || f.x0>body.x1+24)))continue;
                for(int y=f.y0;y<=f.y1;y++)for(int x=f.x0;x<=f.x1;x++)if(labels[y*src.width+x]==f.label)p[(y-top)*src.width+x]=pixels[y*src.width+x];
            }
            ImageAlphaCrop(&frame,.1f); float scale=124.0f/(frame.width>frame.height?frame.width:frame.height);paste(&atlas,frame,c,row,128,scale);
        }
    }
    save_png(out,"trees_v121.png",atlas);free(labels);free(queue);UnloadImage(src);UnloadImage(atlas);
}
static void scenery(const char *in,const char *out) {
    Image raw=read_png(in,"gates.png"),atlas=GenImageColor(512,512,BLANK);
    if(raw.data)for(int r=0;r<4;r++)for(int c=0;c<4;c++){Image f=cut(raw,c,r,4,4);float scale=124.0f/(f.width>f.height?f.width:f.height);paste(&atlas,f,c,r,128,scale);}
    save_png(out,"gates_v121.png",atlas);UnloadImage(atlas);UnloadImage(raw);
    raw=read_png(in,"farms.png");atlas=GenImageColor(1024,1280,BLANK);
    if(raw.data)for(int r=0;r<5;r++)for(int c=0;c<4;c++){Image f=cut(raw,c,r,4,5);ImageResizeNN(&f,256,256);ImageDraw(&atlas,f,(Rectangle){0,0,256,256},(Rectangle){c*256.0f,r*256.0f,256,256},WHITE);UnloadImage(f);}
    save_png(out,"farms_v121.png",atlas);UnloadImage(atlas);UnloadImage(raw);
    raw=read_png(in,"greenhouse_turns.png");atlas=GenImageColor(1024,1024,BLANK);
    if(raw.data)for(int r=0;r<4;r++)for(int c=0;c<4;c++) {
        Image f=cut(raw,c,r,4,4);
        float scale=252.0f/(f.width>f.height?f.width:f.height);
        paste(&atlas,f,c,r,256,scale);
    }
    save_png(out,"greenhouse_turns.png",atlas);UnloadImage(atlas);UnloadImage(raw);
}
static void roads(const char *in,const char *out) {
    Image raw=read_png(in,"road_surfaces.png"),atlas=GenImageColor(1024,3072,BLANK);
    if(raw.data)for(int type=0;type<3;type++) {
        Image tile=ImageFromImage(raw,(Rectangle){type*(raw.width/3.0f),0,raw.width/3.0f,(float)raw.height});ImageResizeNN(&tile,64,64);Color *p=tile.data;
        for(int mask=0;mask<256;mask++)for(int y=0;y<64;y++)for(int x=0;x<64;x++) {
            int on=(x>=8 && x<56 && y>=8 && y<56) || ((mask&1)&&y<32&&x>=8&&x<56) || ((mask&2)&&x>=32&&y>=8&&y<56) || ((mask&4)&&y>=32&&x>=8&&x<56) || ((mask&8)&&x<32&&y>=8&&y<56);
            if(x>=56&&y<8)on=(mask&19)==19;
            if(x>=56&&y>=56)on=(mask&38)==38;
            if(x<8&&y>=56)on=(mask&76)==76;
            if(x<8&&y<8)on=(mask&137)==137;
            if(type==2)on=1;
            Color color=p[y*64+x];color.a=on?255:0;
            ((Color*)atlas.data)[(type*1024+(mask/16)*64+y)*1024+(mask%16)*64+x]=color;
        }
        UnloadImage(tile);
    }
    save_png(out,"road_connections.png",atlas);UnloadImage(raw);UnloadImage(atlas);
}
static void check_assets(const char *dir) {
    const char *names[]={"villagers_v121.png","animal_walk_v121.png","animal_attack_v121.png","animal_babies.png","animals_v121.png","raider_walk.png","raider_attack.png","trees_v121.png","gates_v121.png","farms_v121.png","road_connections.png","greenhouse_turns.png"};
    const int widths[]={1152,384,384,384,384,1536,1536,640,512,1024,1024,1024},heights[]={1152,2880,2880,384,768,2016,2016,512,512,1280,3072,1024};
    for(int i=0;i<12;i++) {
        Image im=read_png(dir,names[i]);
        if(im.data) {
            if(im.width!=widths[i]||im.height!=heights[i])errors++;
            if(i==5||i==6)for(int r=0;r<21;r++)for(int c=0;c<16;c++) {
                int pixels=0;
                for(int y=r*96;y<(r+1)*96;y++)for(int x=c*96;x<(c+1)*96;x++)pixels+=((Color*)im.data)[y*im.width+x].a>0;
                if(pixels<50)errors++;
            }
            if(i==10) {
                Color *p=im.data;
                if(p[0].a || !p[(15*64)*1024+15*64].a)errors++;
                /* 완전 연결 255 마스크는 네 모서리까지 불투명합니다. */
                for(int y=0;y<64;y++)for(int x=0;x<64;x++)if(!p[(15*64+y)*1024+15*64+x].a)errors++;
            }
        }
        UnloadImage(im);
    }
}
int main(int argc,char **argv) {
    GameArguments args;
    if(!game_arguments(&args,argc,argv))return 1;
    argc=args.count;argv=args.values;
    SetTraceLogLevel(LOG_WARNING);
    if(argc!=3){fprintf(stderr,"usage: castle_v121_art <art/v121> <game/assets/expansion>\n");game_arguments_free(&args);return 2;}
    if(strcmp(argv[1],"--check")==0)check_assets(argv[2]);
    else {characters(argv[1],argv[2]);animals(argv[1],argv[2]);raiders(argv[1],argv[2]);trees(argv[2]);scenery(argv[1],argv[2]);roads(argv[1],argv[2]);}
    free(row_scores);free(row_parent);free(row_cost);
    game_arguments_free(&args);
    printf("v1.2.1 art errors=%d\n",errors);return errors?1:0;
}
