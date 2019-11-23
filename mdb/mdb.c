#include "mdb.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#define MDB_PTR_SIZE 4
#define MDB_DATALEN_SIZE 4
#define MDB_HASH_SIZE 4

typedef uint32_t mdb_size_t;
typedef uint32_t mdb_ptr_t;

typedef struct {
  char *db_name;

  FILE *fp_superblock;
  FILE *fp_index;
  FILE *fp_data;

  mdb_options_t options;

  uint32_t index_record_size;
} mdb_int_t;

static mdb_status_t mdb_status(uint8_t code, const char *desc);
static mdb_int_t *mdb_alloc();
static void mdb_free(mdb_int_t *db);
static uint32_t mdb_hash(const char *key);

static char pathbuf[4096];

mdb_status_t mdb_open(mdb_t *handle, const char *path) {
  mdb_int_t *db = mdb_alloc();
  if (db == NULL) {
    return mdb_status(MDB_ERR_ALLOC,
                      "failed allocating memory buffer for database");
  }

  strcpy(pathbuf, path);
  strcat(pathbuf, ".db.super");
  db->fp_superblock = fopen(pathbuf, "r");
  if (db->fp_superblock == NULL) {
    mdb_free(db);
    return mdb_status(MDB_ERR_OPEN_FILE, "cannot open superblock file as read");
  }

  fscanf(db->fp_superblock, "%s", db->db_name);
  fscanf(db->fp_superblock, "%hu", &(db->options.key_size_max));
  fscanf(db->fp_superblock, "%u", &(db->options.data_size_max));
  fscanf(db->fp_superblock, "%u", &(db->options.hash_buckets));
  fscanf(db->fp_superblock, "%u", &(db->options.items_max));

  db->index_record_size =
      db->options.key_size_max
      + MDB_PTR_SIZE * 2
      + MDB_DATALEN_SIZE;

  if (ferror(db->fp_superblock)) {
    mdb_free(db);
    return mdb_status(MDB_ERR_READ, "read error when parsing superblock");
  }

  strcpy(pathbuf, path);
  strcat(pathbuf, ".db.index");
  db->fp_index = fopen(pathbuf, "r+");
  if (db->fp_index == NULL) {
    mdb_free(db);
    return mdb_status(MDB_ERR_OPEN_FILE, "cannot open index file as readwrite");
  }

  strcpy(pathbuf, path);
  strcat(pathbuf, ".db.data");
  db->fp_data = fopen(pathbuf, "r+");
  if (db->fp_index == NULL) {
    mdb_free(db);
    return mdb_status(MDB_ERR_OPEN_FILE, "cannot open data file as readwrite");
  }

  *handle = (mdb_t)db;
  return mdb_status(MDB_OK, NULL);
}

mdb_status_t mdb_read(mdb_t handle, const char *key, char *buf, size_t bufsiz) {
  (void)buf;
  (void)bufsiz;

  mdb_int_t *db = (mdb_int_t*)handle;
  uint32_t hash = mdb_hash(key);
  mdb_size_t bucket = hash % db->options.hash_buckets;

  uint32_t ptr;
  if (fseek(db->fp_index, (bucket + 1) * MDB_PTR_SIZE, SEEK_SET) != 0) {
    return mdb_status(MDB_ERR_SEEK, "cannot seek to bucket");
  }
  if (fread(&ptr, MDB_PTR_SIZE, 1, db->fp_index) != MDB_PTR_SIZE) {
    return mdb_status(MDB_ERR_READ, "cannot read bucket");
  }

  if (fseek(db->fp_index, ptr, SEEK_SET) != 0) {
    return mdb_status(MDB_ERR_SEEK, "cannot seek to index record");
  }

  mdb_ptr_t next_ptr;
  char *key_buffer = (char*)malloc(db->options.key_size_max + 1);
  mdb_ptr_t value_ptr;
  mdb_size_t value_size;
  if (fread(&next_ptr, MDB_PTR_SIZE, 1, db->fp_index) != MDB_PTR_SIZE) {
    return mdb_status(MDB_ERR_READ, "cannot read next ptr");
  }
  if (fread(&key_buffer, 1, db->options.key_size_max + 1, db->fp_index)
      != db->options.key_size_max + 1) {
    return mdb_status(MDB_ERR_READ, "cannot read key");
  }
  if (fread(&value_ptr, MDB_PTR_SIZE, 1, db->fp_index) != MDB_PTR_SIZE) {
    return mdb_status(MDB_ERR_READ, "cannot read value ptr");
  }
  if (fread(&value_size, MDB_DATALEN_SIZE, 1, db->fp_index)
      != MDB_DATALEN_SIZE) {
    return mdb_status(MDB_ERR_READ, "cannot read value length");
  }

  return mdb_status(MDB_ERR_UNIMPLEMENTED, NULL);
}

mdb_options_t mdb_get_options(mdb_t handle) {
  mdb_int_t *db = (mdb_int_t*)handle;
  return db->options;
}

static mdb_status_t mdb_status(uint8_t code, const char *desc) {
  mdb_status_t s;
  s.code = code;
  s.desc = desc;
  return s;
}

static mdb_int_t *mdb_alloc() {
  mdb_int_t *ret = (mdb_int_t*)malloc(sizeof(mdb_int_t));
  if (!ret) {
    return NULL;
  }
  ret->db_name = (char*)malloc(DB_NAME_MAX + 1);
  if (!ret->db_name) {
    free(ret);
    return NULL;
  }
  ret->options.db_name = ret->db_name;
  return ret;
}

static void mdb_free(mdb_int_t *db) {
  if (db->fp_superblock != NULL) {
    fclose(db->fp_superblock);
  }
  if (db->fp_index != NULL) {
    fclose(db->fp_index);
  }
  if (db->fp_data != NULL) {
    fclose(db->fp_data);
  }
  free(db->db_name);
  free(db);
}

static uint32_t mdb_hash(const char *key) {
  uint32_t ret = 0;
  for (uint32_t i = 0; *key != '\0'; ++key, ++i) {
    ret += *key * i;
  }
  return ret;
}
