#include "database.h"

#include <stdio.h>
#include <string.h>

int database_open(const char *path, sqlite3 **db)
{
    if (path == NULL || db == NULL)
        return -1;

    int rc = sqlite3_open(path, db);

    if (rc != SQLITE_OK)
    {
        fprintf(stderr,
                "AIPage: unable to open database: %s\n",
                sqlite3_errmsg(*db));

        sqlite3_close(*db);
        *db = NULL;

        return -1;
    }

    return 0;
}


int database_init(sqlite3 *db)
{
    if (db == NULL)
        return -1;

    const char *sql =
        "CREATE TABLE IF NOT EXISTS process_samples ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,"
        "pid INTEGER NOT NULL,"
        "name TEXT NOT NULL,"
        "state TEXT,"
        "minor_faults INTEGER,"
        "major_faults INTEGER,"
        "minor_faults_per_sec INTEGER,"
        "major_faults_per_sec INTEGER,"
        "vsize_bytes INTEGER,"
        "rss_pages INTEGER,"
        "rss_kb INTEGER,"
        "vm_data_kb INTEGER,"
        "vm_stk_kb INTEGER,"
        "vm_exe_kb INTEGER,"
        "vm_lib_kb INTEGER,"
        "threads INTEGER,"
        "page_size INTEGER,"
        "cpu_percent REAL"
        ");";

    char *error_message = NULL;

    int rc = sqlite3_exec(db,
                          sql,
                          NULL,
                          NULL,
                          &error_message);

    if (rc != SQLITE_OK)
    {
        fprintf(stderr,
                "AIPage: database initialization failed: %s\n",
                error_message);

        sqlite3_free(error_message);

        return -1;
    }

    return 0;
}


int database_insert_sample(sqlite3 *db,
                           const ProcessInfo *info)
{
    if (db == NULL || info == NULL)
        return -1;

    const char *sql =
        "INSERT INTO process_samples ("
        "pid,"
        "name,"
        "state,"
        "minor_faults,"
        "major_faults,"
        "minor_faults_per_sec,"
        "major_faults_per_sec,"
        "vsize_bytes,"
        "rss_pages,"
        "rss_kb,"
        "vm_data_kb,"
        "vm_stk_kb,"
        "vm_exe_kb,"
        "vm_lib_kb,"
        "threads,"
        "page_size,"
        "cpu_percent"
        ") VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?);";

    sqlite3_stmt *stmt = NULL;

    int rc = sqlite3_prepare_v2(db,
                                sql,
                                -1,
                                &stmt,
                                NULL);

    if (rc != SQLITE_OK)
    {
        fprintf(stderr,
                "AIPage: prepare failed: %s\n",
                sqlite3_errmsg(db));

        return -1;
    }

    sqlite3_bind_int(stmt, 1, info->pid);

    sqlite3_bind_text(stmt,
                      2,
                      info->name,
                      -1,
                      SQLITE_TRANSIENT);

    sqlite3_bind_text(stmt,
                      3,
                      info->state,
                      -1,
                      SQLITE_TRANSIENT);

    sqlite3_bind_int64(stmt, 4,
                       (sqlite3_int64)info->minor_faults);

    sqlite3_bind_int64(stmt, 5,
                       (sqlite3_int64)info->major_faults);

    sqlite3_bind_int64(stmt, 6,
                       (sqlite3_int64)info->minor_faults_per_sec);

    sqlite3_bind_int64(stmt, 7,
                       (sqlite3_int64)info->major_faults_per_sec);

    sqlite3_bind_int64(stmt, 8,
                       (sqlite3_int64)info->vsize_bytes);

    sqlite3_bind_int64(stmt, 9,
                       (sqlite3_int64)info->rss_pages);

    sqlite3_bind_int64(stmt, 10,
                       (sqlite3_int64)info->rss_kb);

    sqlite3_bind_int64(stmt, 11,
                       (sqlite3_int64)info->vm_data_kb);

    sqlite3_bind_int64(stmt, 12,
                       (sqlite3_int64)info->vm_stk_kb);

    sqlite3_bind_int64(stmt, 13,
                       (sqlite3_int64)info->vm_exe_kb);

    sqlite3_bind_int64(stmt, 14,
                       (sqlite3_int64)info->vm_lib_kb);

    sqlite3_bind_int(stmt, 15,
                     info->threads);

    sqlite3_bind_int64(stmt, 16,
                       (sqlite3_int64)info->page_size);

    sqlite3_bind_double(stmt, 17,
                        info->cpu_percent);

    rc = sqlite3_step(stmt);

    if (rc != SQLITE_DONE)
    {
        fprintf(stderr,
                "AIPage: insert failed: %s\n",
                sqlite3_errmsg(db));

        sqlite3_finalize(stmt);

        return -1;
    }

    sqlite3_finalize(stmt);

    return 0;
}


void database_close(sqlite3 *db)
{
    if (db != NULL)
        sqlite3_close(db);
}
