/* 휴대 등불·고정 불 아틀라스의 칸 배치 규약입니다. 아트 도구가 이 기준점에 맞춰 굽고 검사하며
   게임은 같은 값으로 손·받침 위치와 광원 위치를 맞춥니다. 좌표는 칸 왼쪽 위 기준 세계 화소입니다. */
#ifndef CASTLE_GAME_FIRE_ATLAS_H
#define CASTLE_GAME_FIRE_ATLAS_H

typedef struct GameFireRow {
    /* height: 굽기 후 내용 높이(화소). base: 받침·손잡이 끝의 칸 좌표. grip: 손이 잡는 높이.
       flame: 여덟 프레임 평균 불꽃 중심(광원 위치). */
    int height, base_x, base_y, grip_y;
    float flame_x, flame_y;
} GameFireRow;

#define GAME_FIRE_FRAMES 8
#define GAME_CARRIED_CELL 16
enum GameCarriedRow { GAME_CARRIED_ROW_TORCH, GAME_CARRIED_ROW_FRONT, GAME_CARRIED_ROW_SIDE, GAME_CARRIED_ROW_BACK };
static const GameFireRow GAME_CARRIED_ROWS[4] = {
    {12, 8, 15, 12, 8.7f, 8.2f},
    {9, 8, 15, 7, 8.5f, 12.9f},
    {9, 8, 15, 7, 7.5f, 13.0f},
    {9, 8, 15, 7, 7.9f, 12.5f},
};

#define GAME_STATIONARY_CELL 24
enum GameStationaryRow {
    GAME_STATIONARY_WALL_TORCH,
    GAME_STATIONARY_BRAZIER,
    GAME_STATIONARY_CAMPFIRE,
    GAME_STATIONARY_FURNACE
};
static const GameFireRow GAME_STATIONARY_ROWS[4] = {
    {14, 12, 23, 23, 16.1f, 15.0f},
    {13, 12, 23, 23, 13.0f, 16.1f},
    {13, 12, 23, 23, 12.7f, 18.6f},
    {8, 12, 23, 23, 12.6f, 21.1f},
};
#endif
