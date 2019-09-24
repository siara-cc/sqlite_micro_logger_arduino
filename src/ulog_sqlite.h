#ifndef __ULOG_SQLITE__
#define __ULOG_SQLITE__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <stdint.h>

typedef unsigned char byte;

enum {ULS_TYPE_INT = 1, ULS_TYPE_REAL, ULS_TYPE_BLOB, ULS_TYPE_TEXT};

enum {ULS_RES_OK = 0, ULS_RES_ERR = -1, ULS_RES_INV_PAGE_SZ = -2, 
      ULS_RES_TOO_LONG = -3, ULS_RES_SEEK_ERR = -4, ULS_RES_READ_ERR = -5,
      ULS_RES_WRITE_ERR = -6, ULS_RES_FLUSH_ERR = -7};

struct ulog_sqlite_context {
  byte *buf;
  byte col_count;
  byte page_size_exp; // 9=512, 10=1024 and so on upto 16=65536
  byte max_pages_exp; // Maximum data pages (as exponent of 2)
                      // after which to roll. 0 means no max.
  byte page_resv_bytes; // Reserved bytes at end of every page (say checksum)
  // Success if return value == len
  int32_t (*read_fn)(struct ulog_sqlite_context *ctx, void *buf, size_t len);
  int32_t (*write_fn)(struct ulog_sqlite_context *ctx, void *buf, size_t len);
  int (*seek_fn)(struct ulog_sqlite_context *ctx, long pos);
  int (*flush_fn)(struct ulog_sqlite_context *ctx);
  // following are running values used internally
  uint32_t cur_page;
  uint32_t cur_rowid;
  byte flush_flag;
  int err_no;
};

int ulog_sqlite_init(struct ulog_sqlite_context *ctx);
int ulog_sqlite_init_with_script(struct ulog_sqlite_context *ctx,
      char *table_name, char *table_script);
int ulog_sqlite_next_row(struct ulog_sqlite_context *ctx);
int ulog_sqlite_set_val(struct ulog_sqlite_context *ctx, int col_idx,
                          int type, const void *val, uint16_t len);
int ulog_sqlite_flush(struct ulog_sqlite_context *ctx);
int ulog_sqlite_finalize(struct ulog_sqlite_context *ctx, void *another_buf);
int ulog_sqlite_check(struct ulog_sqlite_context *ctx);

#ifdef __cplusplus
}
#endif

#endif
