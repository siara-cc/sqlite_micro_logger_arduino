#ifndef __ULOG_SQLITE__
#define __ULOG_SQLITE__

#define ULS_CFG_WRITE_CHECKSUM 1
#define ULS_CFG_READ_CHECKSUM 0

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

// Write context to be passed to create / append
// a database.  The running values need not be supplied
struct uls_write_context {
  byte *buf;          // working buffer
  byte col_count;     // Number of columns (whether fits into page is not checked extensively)
  byte page_size_exp; // 9=512, 10=1024 and so on upto 16=65536
  byte max_pages_exp; // Maximum data pages (as exponent of 2)
                      // after which to roll. 0 means no max. Not implemented yet.
  byte page_resv_bytes; // Reserved bytes at end of every page (say checksum)
  // read_fn and write_fn should return no. of bytes read or written
  int32_t (*read_fn)(struct uls_write_context *ctx, void *buf, uint32_t pos, size_t len);
  int32_t (*write_fn)(struct uls_write_context *ctx, void *buf, uint32_t pos, size_t len);
  int (*flush_fn)(struct uls_write_context *ctx); // Success if returns 0
  // following are running values used internally
  uint32_t cur_write_page;
  uint32_t cur_write_rowid;
  byte flush_flag;
  int err_no;
};

// Initializes database - writes first page
// and makes it ready for writing data
int uls_write_init(struct uls_write_context *wctx);

// Initializes database - writes first page
// and makes it ready for writing data
// Uses the given table name and DDL script
// Table name should match that given in script
int uls_write_init_with_script(struct uls_write_context *wctx,
      char *table_name, char *table_script);

// Initalizes database - resets signature on first page
// positions at last page for writing
// If this returns ULS_RES_NOT_FINALIZED,
// call uls_finalize() to first finalize the database
int uls_init_for_append(struct uls_write_context *wctx);

// Creates new record with all columns null
// If no more space in page, writes it to disk
// creates new page, and creates a new record
int uls_create_new_row(struct uls_write_context *wctx);

// Sets value of column in the current record for the given column index
// If no more space in page, writes it to disk
// creates new page, and moves the row to new page
int uls_set_col_val(struct uls_write_context *wctx, int col_idx,
                          int type, const void *val, uint16_t len);

// Gets the value of the column for the current record
// Can be used to retrieve the value of the column
// set by uls_set_col_val
const void *uls_get_col_val(struct uls_write_context *wctx, int col_idx, uint32_t *out_col_type);

// Flushes the corrent page to disk
// Page is written only when it becomes full
// If it needs to be written for each record or column,
// this can be used
int uls_flush(struct uls_write_context *wctx);

// Based on the data written so far, forms Interior B-Tree pages
// according to SQLite format and update the root page number
// in the first page.
int uls_finalize(struct uls_write_context *wctx);

// Returns 1 if the database is in unfinalized state
int uls_not_finalized(struct uls_write_context *wctx);

// Read context to be passed to read from a database created using this library.
// The running values need not be supplied
struct uls_read_context {
  byte *buf;
  // read_fn should return no. of bytes read
  int32_t (*read_fn)(struct uls_read_context *ctx, void *buf, uint32_t pos, size_t len);
  // following are running values used internally
  uint32_t last_leaf_page;
  uint32_t cur_page;
  uint16_t cur_rec_pos;
  byte page_size_exp;
};

// Reads a database created using this library,
// checks signature and positions at the first record.
// Cannot be used to read SQLite databases
// not created using this library or modified using other libraries
int uls_read_init(struct uls_read_context *rctx);

// Returns number of columns in the current record
int uls_cur_row_col_count(struct uls_read_context *rctx);

// Returns value of column at given index.
// Also returns type of column in (out_col_type) according to record format
// See https://www.sqlite.org/fileformat.html#record_format
// For text and blob columns, pass the type to uls_derive_data_len()
// to get the actual length
const void *uls_read_col_val(struct uls_read_context *rctx, int col_idx, uint32_t *out_col_type);

// For text and blob columns, pass the out_col_type
// returned by uls_read_col_val() to get the actual length
uint32_t uls_derive_data_len(uint32_t col_type);

// Positions current position at first record
int uls_read_first_row(struct uls_read_context *rctx);

// Positions current position at next record
int uls_read_next_row(struct uls_read_context *rctx);

// Positions current position at previous record
int uls_read_prev_row(struct uls_read_context *rctx);

// Positions current position at last record
// The database should have been finalized
// for this function to work
int uls_read_last_row(struct uls_read_context *rctx);

// Performs binary search on the inserted records
// using the given Row ID and positions at the record found
// Does not change position if record not found
int uls_bin_srch_row_by_id(struct uls_read_context *rctx, uint32_t rowid);

// Performs binary search on the inserted records
// using the given Value and positions at the record found
// Does not change position if record not found
int uls_bin_srch_row_by_val(struct uls_read_context *rctx, byte *val, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif
