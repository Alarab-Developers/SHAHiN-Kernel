#ifndef ARABFS_PATHS_H
#define ARABFS_PATHS_H
#include "القلب/المكتبات/المكتبات.h"

int arabfs_get_parent(const char* path, char* parent_path, char* name);
int arabfs_resolve(const char* path);

#endif
