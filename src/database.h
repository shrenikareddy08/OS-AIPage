#ifndef DATABASE_H
#define DATABASE_H

#include "system_monitor.h"
#include <sqlite3.h>

int database_open(const char *path, sqlite3 **db);

int database_init(sqlite3 *db);

int database_insert_sample(sqlite3 *db,
                           const ProcessInfo *info);

void database_close(sqlite3 *db);

#endif
