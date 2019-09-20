#ifndef __ULOG_SQLITE__
#define __ULOG_SQLITE__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <stdint.h>

typedef unsigned char byte;
typedef int32_t (*ulog_sqlite_read_fn)(void *buf, size_t len);
typedef int32_t (*ulog_sqlite_write_fn)(void *buf, size_t len);
typedef int (*ulog_sqlite_seek_fn)(long pos);
typedef int (*ulog_sqlite_flush_fn)();

enum {ULS_TYPE_INT = 1, ULS_TYPE_REAL, ULS_TYPE_BLOB, ULS_TYPE_TEXT};

enum {ULS_RES_OK = 0, ULS_RES_ERR = -1, ULS_RES_INV_PAGE_SZ = -2, ULS_RES_TOO_LONG = -3};

struct ulog_sqlite_context {
  byte *buf;
  int col_count;
  byte page_size_exp; // 9=512, 10=1024 and so on upto 16=65536
  byte max_pages_exp; // Maximum data pages (as exponent of 2)
                      // after which to roll. 0 means no max.
  byte page_resv_bytes; // Reserved bytes at end of every page (say checksum)
  ulog_sqlite_read_fn read_fn; // Success if return value == len
  ulog_sqlite_write_fn write_fn; // Success if return value == len
  ulog_sqlite_seek_fn seek_fn;
  ulog_sqlite_flush_fn flush_fn;
  // following are running values used internally
  uint32_t cur_page;
  uint32_t cur_rowid;
  byte flush_flag;
};

int ulog_sqlite_init(struct ulog_sqlite_context *ctx);
int ulog_sqlite_init_with_script(struct ulog_sqlite_context *ctx,
      char *table_name, char *table_script);
int ulog_sqlite_new_row(struct ulog_sqlite_context *ctx);
int ulog_sqlite_set_val(struct ulog_sqlite_context *ctx, int col_idx,
                          int type, void *val, uint16_t len);
int ulog_sqlite_flush(struct ulog_sqlite_context *ctx);
int ulog_sqlite_finalize(struct ulog_sqlite_context *ctx, void *another_buf);
int ulog_sqlite_check(struct ulog_sqlite_context *ctx);

#ifdef __cplusplus
}
#endif

#endif
