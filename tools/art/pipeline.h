#ifndef CASTLE_ART_PIPELINE_H
#define CASTLE_ART_PIPELINE_H
#include "storage.h"
#include "trace.h"
#define game_read_bytes art_read_bytes
#define game_write_bytes art_write_bytes
const char *art_input(const char *relative);
const char *art_stage_output(void);
const char *art_painted_output(void);
const char *art_stage_file(const char *relative);
int art_stage_base(int argc, char **argv);
int art_stage_buildings(int argc, char **argv);
int art_stage_nature(int argc, char **argv);
int art_stage_scenery(int argc, char **argv);
int art_stage_residents(int argc, char **argv);
int art_stage_interface(int argc, char **argv);
int art_stage_services(int argc, char **argv);
int art_stage_production(int argc, char **argv);
#endif
