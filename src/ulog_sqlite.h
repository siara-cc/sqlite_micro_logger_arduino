/*
  Sqlite Micro Logger

  Fast and Lean Sqlite database logger targetting
  low RAM systems such as Microcontrollers.

  This Library can work on systems that have as little as 2kb,
  such as the ATMega328 MCU.  It is available for the Arduino platform.

  https://github.com/siara-cc/sqlite_micro_logger

  Copyright @ 2019 Arundale Ramanathan, Siara Logics (cc)

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/

#ifndef __ULOG_SQLITE__
#define __ULOG_SQLITE__

// 0 - No calculation, no checking
// 1 - Calculate, and check header during recovery,
//     skip pages where header checksum don't match
#define DBLOG_CFG_WRITE_CHECKSUM 1

// Not implemented yet
// 0 - No checking
// 1 - Check if page checksum matches everytime page is loaded
//     and whether first record checksum matches during binary search
#define DBLOG_CFG_READ_CHECKSUM 0

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <stdint.h>

typedef unsigned char byte;

enum {DBLOG_TYPE_INT = 1, DBLOG_TYPE_REAL, DBLOG_TYPE_BLOB, DBLOG_TYPE_TEXT};

enum {DBLOG_RES_OK = 0, DBLOG_RES_ERR = -1, DBLOG_RES_INV_PAGE_SZ = -2, 
  DBLOG_RES_TOO_LONG = -3, DBLOG_RES_WRITE_ERR = -4, DBLOG_RES_FLUSH_ERR = -5};

enum {DBLOG_RES_SEEK_ERR = -6, DBLOG_RES_READ_ERR = -7,
  DBLOG_RES_INVALID_SIG = -8, DBLOG_RES_MALFORMED = -9,
  DBLOG_RES_NOT_FOUND = -10, DBLOG_RES_NOT_FINALIZED = -11,
  DBLOG_RES_TYPE_MISMATCH = -12, DBLOG_RES_INV_CHKSUM = -13};

// Write context to be passed to create / append
// a database.  The running values need not be supplied
struct dblog_write_context {
  byte *buf;          // working buffer of size page_size
  byte col_count;     // No. of columns (whether fits into page is not checked)
  byte page_size_exp; // 9=512, 10=1024 and so on upto 16=65536
  byte max_pages_exp; // Maximum data pages (as exponent of 2) after which
                      //   to roll. 0 means no max. Not implemented yet.
  byte page_resv_bytes; // Reserved bytes at end of every page (say checksum)
  // read_fn and write_fn should return no. of bytes read or written
  int32_t (*read_fn)(struct dblog_write_context *ctx, void *buf, uint32_t pos, size_t len);
  int32_t (*write_fn)(struct dblog_write_context *ctx, void *buf, uint32_t pos, size_t len);
  int (*flush_fn)(struct dblog_write_context *ctx); // Success if returns 0
  // following are running values used internally
  uint32_t cur_write_page;
  uint32_t cur_write_rowid;
  byte state;
  int err_no;
};

// Initializes database - writes first page
// and makes it ready for writing data
int dblog_write_init(struct dblog_write_context *wctx);

// Initializes database - writes first page
// and makes it ready for writing data
// Uses the given table name and DDL script
// Table name should match that given in script
int dblog_write_init_with_script(struct dblog_write_context *wctx,
      char *table_name, char *table_script);

// Initalizes database - resets signature on first page
// positions at last page for writing
// If this returns DBLOG_RES_NOT_FINALIZED,
// call dblog_finalize() to first finalize the database
int dblog_init_for_append(struct dblog_write_context *wctx);

// Creates new record with all columns null
// If no more space in page, writes it to disk
// creates new page, and creates a new record
int dblog_append_empty_row(struct dblog_write_context *wctx);

// Creates new record with given column values
// If no more space in page, writes it to disk
// creates new page, and creates a new record
int dblog_append_row_with_values(struct dblog_write_context *wctx,
      uint8_t types[], const void *values[], uint16_t lengths[]);

// Sets value of column in the current record for the given column index
// If no more space in page, writes it to disk
// creates new page, and moves the row to new page
int dblog_set_col_val(struct dblog_write_context *wctx, int col_idx,
                          int type, const void *val, uint16_t len);

// Gets the value of the column for the current record
// Can be used to retrieve the value of the column
// set by dblog_set_col_val
const void *dblog_get_col_val(struct dblog_write_context *wctx, int col_idx, uint32_t *out_col_type);

// Flushes the corrent page to disk
// Page is written only when it becomes full
// If it needs to be written for each record or column,
// this can be used
int dblog_flush(struct dblog_write_context *wctx);

// Flushes data written so far and Updates the last leaf page number
// in the first page to enable Binary Search
int dblog_partial_finalize(struct dblog_write_context *wctx);

// Based on the data written so far, forms Interior B-Tree pages
// according to SQLite format and update the root page number
// in the first page.
int dblog_finalize(struct dblog_write_context *wctx);

// Returns 1 if the database is in unfinalized state
int dblog_not_finalized(struct dblog_write_context *wctx);

// Reads page size from database if not known
int32_t dblog_read_page_size(struct dblog_write_context *wctx);

// Recovers database pointed by given context
// and finalizes it
int dblog_recover(struct dblog_write_context *wctx);

// Read context to be passed to read from a database created using this library.
// The running values need not be supplied
struct dblog_read_context {
  byte *buf;
  // read_fn should return no. of bytes read
  int32_t (*read_fn)(struct dblog_read_context *ctx, void *buf, uint32_t pos, size_t len);
  // following are running values used internally
  uint32_t last_leaf_page;
  uint32_t root_page;
  uint32_t cur_page;
  uint16_t cur_rec_pos;
  byte page_size_exp;
  byte page_resv_bytes;
};

// Reads a database created using this library,
// checks signature and positions at the first record.
// Cannot be used to read SQLite databases
// not created using this library or modified using other libraries
int dblog_read_init(struct dblog_read_context *rctx);

// Returns number of columns in the current record
int dblog_cur_row_col_count(struct dblog_read_context *rctx);

// Returns value of column at given index.
// Also returns type of column in (out_col_type) according to record format
// See https://www.sqlite.org/fileformat.html#record_format
// For text and blob columns, pass the type to dblog_derive_data_len()
// to get the actual length
const void *dblog_read_col_val(struct dblog_read_context *rctx, int col_idx, uint32_t *out_col_type);

// For text and blob columns, pass the out_col_type
// returned by dblog_read_col_val() to get the actual length
uint32_t dblog_derive_data_len(uint32_t col_type);

// Positions current position at first record
int dblog_read_first_row(struct dblog_read_context *rctx);

// Positions current position at next record
int dblog_read_next_row(struct dblog_read_context *rctx);

// Positions current position at previous record
int dblog_read_prev_row(struct dblog_read_context *rctx);

// Positions current position at last record
// The database should have been finalized
// for this function to work
int dblog_read_last_row(struct dblog_read_context *rctx);

// Performs binary search on the inserted records
// using the given Row ID and positions at the record found
// Does not change position if record not found
int dblog_srch_row_by_id(struct dblog_read_context *rctx, uint32_t rowid);

// Performs binary search on the inserted records
// using the given Value and positions at the record found
// Changes current position to closest match, if record not found
// is_rowid = 1 is used to do Binary Search by RowId
int dblog_bin_srch_row_by_val(struct dblog_read_context *rctx, int col_idx,
      int val_type, void *val, uint16_t len, byte is_rowid);

#ifdef __cplusplus
}
#endif

#endif
