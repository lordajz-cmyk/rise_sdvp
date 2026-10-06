#ifndef CONF_GENERAL_H_
#define CONF_GENERAL_H_
#include "datatypes.h"
extern MAIN_CONFIG main_config;
void conf_general_read_main_conf(MAIN_CONFIG *conf);
void conf_general_sanitize_main_config(MAIN_CONFIG *conf);
bool conf_general_store_main_config(MAIN_CONFIG *conf);
#endif
