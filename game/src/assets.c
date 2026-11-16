/* assets.c — 메모리 로더를 써서 한글 경로에서도 리소스를 읽습니다. */
#include "assets.h"
#include "rlgl.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static Texture2D texture(GameAssets *a,const char *dir,const char *name)
{
    char path[GAME_PATH_BYTES]; int n=0; unsigned char *bytes; Image image={0}; Texture2D result={0};
    if (!game_join_path(path,sizeof(path),dir,name)) { a->missing++; return result; }
    bytes=game_read_bytes(path,&n);
    if (bytes) { image=LoadImageFromMemory(".png",bytes,n); free(bytes); }
    if (image.data) { result=LoadTextureFromImage(image); UnloadImage(image); SetTextureFilter(result,TEXTURE_FILTER_POINT); }
    else a->missing++;
    return result;
}
int game_assets_load(GameAssets *a,const char *dir,int audio)
{
    static const char *const buildings[CORE_BLD_COUNT]={"keep","house","farm","gatherer","lumber","quarry","mana_mine","warehouse","market","barracks","tower","wall","gate","healer","mage_tower","road"};
    static const char *const resources[CORE_RES_COUNT]={"gold","mana","food","wood","stone","herbs"};
    static const char *const sounds[SND_COUNT]={
        "audio-interface/Audio/click_002.ogg","audio-interface/Audio/drop_002.ogg","audio-interface/Audio/error_004.ogg",
        "audio-impact/Audio/impactBell_heavy_000.ogg","audio-rpg/Audio/drawKnife1.ogg","audio-impact/Audio/impactSoft_medium_000.ogg",
        "audio-interface/Audio/confirmation_002.ogg","audio-interface/Audio/maximize_006.ogg"};
    char path[GAME_PATH_BYTES],name[160]; int i,n=0,count=0,*codepoints; unsigned char *bytes;
    memset(a,0,sizeof(*a));
    if (!game_join_path(path,sizeof(path),dir,"fonts/Galmuri11.ttf")) return 0;
    bytes=game_read_bytes(path,&n);
    if (!bytes) return 0;
    codepoints=(int *)malloc(12000*sizeof(int));
    if (!codepoints) { free(bytes); return 0; }
    for (i=32;i<127;i++) codepoints[count++]=i;
    codepoints[count++]=0xB7; codepoints[count++]=0xD7;
    codepoints[count++]=0x2190; codepoints[count++]=0x2192;
    for (i=0x3131;i<=0x3163;i++) codepoints[count++]=i;
    for (i=0xAC00;i<=0xD7A3;i++) codepoints[count++]=i;
    /* 한글 완성형 전체를 넣어 주민 이름과 사용자가 붙인 이름도 표시합니다. */
    a->font=LoadFontFromMemory(".ttf",bytes,n,24,codepoints,count);
    free(codepoints); free(bytes);
    if (!a->font.texture.id) return 0;
    SetTextureFilter(a->font.texture,TEXTURE_FILTER_POINT);
    a->terrain=texture(a,dir,"painted/terrain.png"); a->roads=texture(a,dir,"painted/roads.png"); a->snow=texture(a,dir,"painted/snow.png");
    a->props=texture(a,dir,"painted/props.png"); a->construction=texture(a,dir,"painted/construction.png");
    a->villagers=texture(a,dir,"painted/villagers.png"); a->enemies=texture(a,dir,"painted/enemies.png"); a->arrow=texture(a,dir,"painted/arrow.png");
    if (a->villagers.id && (a->villagers.width!=1152 || a->villagers.height!=768)) {
        fprintf(stderr,"Incompatible villager atlas: expected 1152x768\n"); a->missing++;
    }
    for (i=0;i<CORE_BLD_COUNT-1;i++) {
        (void)snprintf(name,sizeof(name),"painted/%s.png",buildings[i]); a->buildings[i]=texture(a,dir,name);
    }
    for (i=0;i<CORE_RES_COUNT;i++) {
        (void)snprintf(name,sizeof(name),"painted/res_%s.png",resources[i]); a->resources[i]=texture(a,dir,name);
    }
    if (audio) {
        InitAudioDevice(); a->audio_ready=IsAudioDeviceReady();
        if (a->audio_ready) for (i=0;i<SND_COUNT;i++) {
            Wave wave={0};
            (void)snprintf(name,sizeof(name),"third_party/kenney/%s",sounds[i]);
            if (!game_join_path(path,sizeof(path),dir,name)) continue;
            bytes=game_read_bytes(path,&n);
            if (bytes) { wave=LoadWaveFromMemory(".ogg",bytes,n); free(bytes); }
            if (wave.data) { a->sounds[i]=LoadSoundFromWave(wave); UnloadWave(wave); }
        }
    }
    if (a->missing) fprintf(stderr,"Missing art assets: %d\n",a->missing);
    return a->missing==0;
}
void game_assets_free(GameAssets *a)
{
    int i;
    if (a->terrain.id) UnloadTexture(a->terrain);
    if (a->roads.id) UnloadTexture(a->roads);
    if (a->snow.id) UnloadTexture(a->snow);
    if (a->props.id) UnloadTexture(a->props);
    if (a->construction.id) UnloadTexture(a->construction);
    if (a->villagers.id) UnloadTexture(a->villagers);
    if (a->enemies.id) UnloadTexture(a->enemies);
    if (a->arrow.id) UnloadTexture(a->arrow);
    for (i=0;i<CORE_BLD_COUNT;i++) if (a->buildings[i].id) UnloadTexture(a->buildings[i]);
    for (i=0;i<CORE_RES_COUNT;i++) if (a->resources[i].id) UnloadTexture(a->resources[i]);
    if (a->font.texture.id) UnloadFont(a->font);
    if (a->audio_ready) {
        for (i=0;i<SND_COUNT;i++) if (a->sounds[i].frameCount) { StopSound(a->sounds[i]); UnloadSound(a->sounds[i]); }
        CloseAudioDevice();
    }
    memset(a,0,sizeof(*a));
}
void game_sound(GameAssets *a,int sound,float volume)
{
    double now,gap=sound==SND_ALARM?1.5:0.08;
    if (!a->audio_ready || sound<0 || sound>=SND_COUNT || !a->sounds[sound].frameCount) return;
    now=GetTime(); if (now-a->last_sound[sound]<gap) return;
    a->last_sound[sound]=now; SetSoundVolume(a->sounds[sound],volume); PlaySound(a->sounds[sound]);
}
int game_screenshot(const char *path)
{
    /* raylib 5.5의 화면 로더는 Windows에서 DPI를 중복 곱합니다.
     * 논리 창 크기 대신 실제 프레임버퍼 크기로 읽어 검은 여백을 피합니다. */
    Image image={0}; int n=0,ok=0; unsigned char *bytes;
    image.width=GetRenderWidth(); image.height=GetRenderHeight(); image.mipmaps=1;
    image.format=PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
    image.data=rlReadScreenPixels(image.width,image.height);
    if (!image.data) return 0;
    bytes=ExportImageToMemory(image,".png",&n);
    if (bytes) { ok=game_write_bytes(path,bytes,(size_t)n); MemFree(bytes); }
    if (!ok) fprintf(stderr,u8"스크린숏 저장 실패: %s\n",path);
    UnloadImage(image); return ok;
}
