#ifndef ARABFS_DIRECTORIES_H
#define ARABFS_DIRECTORIES_H

#include "التعريف_العام.h"
#include "القلب/المكتبات/المكتبات.h"
// دوال المجلدات

int arabfs_mkdir(const char* path);
int arabfs_rmdir(const char* path);
void arabfs_list_path(unsigned int parent,
                      void (*callback)(const char*));
int arabfs_find_entry(const char* name, unsigned int parent);

#endif
