#ifndef CASTLE_ART_IMAGE_OPS_H
#define CASTLE_ART_IMAGE_OPS_H
#include "raylib.h"
#include "storage.h"
#include <castle_expansion.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
extern int art_errors;
Image art_read_png(const char *dir, const char *name);
void art_save_png(const char *dir, const char *name, Image image);
void art_clean(Image *image);
int art_seam(Image image, int n, int count, int vertical, int start, int end);
Image art_cut(Image image, int col, int row, int columns, int rows);
void art_paste(Image *target, Image frame, int col, int row, int size, float scale);
void art_sequence(Image *target, Image source, int row, int rows, int target_row, int target_col, int columns,
                  int offset);
void art_image_reset(void);
#endif
