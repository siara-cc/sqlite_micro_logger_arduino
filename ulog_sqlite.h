#ifndef __ULOG_SQLITE__
#define __ULOG_SQLITE__

#include <stdlib.h>
#include <stdint.h>

typedef unsigned char byte;
typedef int (*ulog_sqlite_read_fn)(const void *buf, size_t len);
typedef int (*ulog_sqlite_write_fn)(const void *buf, size_t len);
typedef int (*ulog_sqlite_seek_fn)(long pos);
typedef int (*ulog_sqlite_flush_fn)();

enum {ULS_TYPE_INT = 1, ULS_TYPE_REAL, ULS_TYPE_BLOB, ULS_TYPE_TEXT};

enum {ULS_RES_OK = 0, ULS_RES_INV_PAGE_SZ = -1};

struct ulog_sqlite_context {
  void *buf;
  int col_count;
  byte page_size_exp; // 9=512, 10=1024 and so on upto 16=65536
  byte is_row_created;
  uint16_t cur_rec_count;
  uint16_t cur_rec_pos;
  uint16_t cur_rec_hdr_len;
  uint16_t cur_rec_len;
  uint16_t cur_rowid;
  ulog_sqlite_read_fn read_fn;
  ulog_sqlite_write_fn write_fn;
  ulog_sqlite_seek_fn seek_fn;
  ulog_sqlite_flush_fn flush_fn;
};

int ulog_sqlite_init(struct ulog_sqlite_context *ctx);
int ulog_sqlite_init_with_script(struct ulog_sqlite_context *ctx,
      char *table_name, char *table_script);
int ulog_sqlite_new_row(struct ulog_sqlite_context *ctx);
int ulog_sqlite_set_val(struct ulog_sqlite_context *ctx, int col_idx,
                          int type, void *val, int len);
int ulog_sqlite_flush(struct ulog_sqlite_context *ctx);
int ulog_sqlite_finalize(struct ulog_sqlite_context *ctx, void *another_buf);
int ulog_sqlite_recover(struct ulog_sqlite_context *ctx, void *another_buf);

#endif
