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
      ULS_RES_TOO_LONG = -3, ULS_RES_WRITE_ERR = -4, ULS_RES_FLUSH_ERR = -5};

enum {ULS_RES_SEEK_ERR = -6, ULS_RES_READ_ERR = -7, ULS_RES_INVALID_SIG = -8,
      ULS_RES_MALFORMED = -9, ULS_RES_NOT_FOUND = -10, ULS_RES_NOT_FINALIZED = -11};

struct uls_write_context {
  byte *buf;          // working buffer
  byte col_count;     // Number of columns (whether fits into page is not checked extensively)
  byte page_size_exp; // 9=512, 10=1024 and so on upto 16=65536
  byte max_pages_exp; // Maximum data pages (as exponent of 2)
                      // after which to roll. 0 means no max. Not implemented yet.
  byte page_resv_bytes; // Reserved bytes at end of every page (say checksum)
  // read_fn and write_fn should return no. of bytes read or written
  int32_t (*read_fn)(struct uls_write_context *ctx, void *buf, size_t len);
  int32_t (*write_fn)(struct uls_write_context *ctx, void *buf, size_t len);
  int (*seek_fn)(struct uls_write_context *ctx, long pos); // Success if returns 0
  int (*flush_fn)(struct uls_write_context *ctx); // Success if returns 0
  // following are running values used internally
  uint32_t cur_write_page;
  uint32_t cur_write_rowid;
  byte flush_flag;
  int err_no;
};

// logging functions
int uls_write_init(struct uls_write_context *wctx);
int uls_write_init_with_script(struct uls_write_context *wctx,
      char *table_name, char *table_script);
int uls_init_for_append(struct uls_write_context *wctx);
int uls_create_new_row(struct uls_write_context *wctx);
int uls_set_col_val(struct uls_write_context *wctx, int col_idx,
                          int type, const void *val, uint16_t len);
const void *uls_get_col_val(struct uls_write_context *wctx, int col_idx, uint32_t *out_col_type);
int uls_flush(struct uls_write_context *wctx);
int uls_finalize(struct uls_write_context *wctx);
int uls_not_finalized(struct uls_write_context *wctx); // checks finalized or not

struct uls_read_context {
  byte *buf;
  // read_should return no. of bytes read
  int32_t (*read_fn)(struct uls_read_context *ctx, void *buf, size_t len);
  int (*seek_fn)(struct uls_read_context *ctx, long pos); // Success if returns 0
  // following are running values used internally
  uint32_t last_leaf_page;
  uint32_t cur_page;
  uint16_t cur_rec_pos;
  byte page_size_exp;
};

// retrieve functions
int uls_read_init(struct uls_read_context *rctx);
const void *uls_read_col_val(struct uls_read_context *rctx, int col_idx, uint32_t *out_col_type);
uint32_t uls_derive_data_len(uint32_t col_type);
int uls_read_first_row(struct uls_read_context *rctx);
int uls_read_next_row(struct uls_read_context *rctx);
int uls_read_prev_row(struct uls_read_context *rctx);
int uls_read_last_row(struct uls_read_context *rctx);
int uls_bin_srch_row_by_id(struct uls_read_context *rctx, uint32_t rowid);
int uls_bin_srch_row_by_val(struct uls_read_context *rctx, byte *val, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif
