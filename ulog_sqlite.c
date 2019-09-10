#include "ulog_sqlite.h"

#include <string.h>

void writeTwoBytes(byte *buf, uint16_t bytes) {
  buf[0] = bytes >> 8;
  buf[1] = bytes & 0xFF;
}

void writeFourBytes(byte *buf, uint32_t bytes) {
  buf[0] = bytes >> 24;
  buf[1] = (bytes >> 16) & 0xFF;
  buf[2] = (bytes >> 8) & 0xFF;
  buf[3] = bytes & 0xFF;
}

void writeVInt1Byte(byte *buf, byte val) {
  *buf = val;
}

void writeVInt2Bytes(byte *buf, uint16_t val) {
  *buf = 0x80 + (val >> 7);
  buf[1] = val & 0x3F;
}

void writeVInt3Bytes(byte *buf, uint32_t val) {
  *buf = 0x80 + (val >> 14);
  buf[1] = 0x80 + ((val >> 7) & 0x3F);
  buf[2] = val & 0x3F;
}

void writeVInt4Bytes(byte *buf, uint32_t val) {
  *buf = 0x80 + (val >> 21);
  buf[1] = 0x80 + ((val >> 14) & 0x3F);
  buf[2] = 0x80 + ((val >> 7) & 0x3F);
  buf[3] = val & 0x3F;
}

char default_table_name[] = "t1";

void form_header(struct ulog_sqlite_context *ctx, int16_t page_size, char *table_name, char *table_script) {

  // 100 byte header - refer https://www.sqlite.org/fileformat.html
  byte *buf = (byte *) ctx->buf;
  strcpy(buf, "SQLite format 3");
  writeTwoBytes(buf + 16, page_size);
  buf[18] = 1;
  buf[19] = 1;
  buf[20] = 0;
  buf[21] = 64;
  buf[22] = 32;
  buf[23] = 32;
  //writeFourBytes(buf + 24, 0);
  //writeFourBytes(buf + 28, 0);
  //writeFourBytes(buf + 32, 0);
  //writeFourBytes(buf + 36, 0);
  //writeFourBytes(buf + 40, 0);
  memset(buf + 24, '\0', 20); // Set to zero, above 5
  writeFourBytes(buf + 44, 4);
  //writeTwoBytes(buf + 48, 0);
  //writeTwoBytes(buf + 52, 0);
  memset(buf + 48, '\0', 8); // Set to zero, above 2
  writeFourBytes(buf + 56, 1);
  //writeTwoBytes(buf + 60, 0);
  //writeTwoBytes(buf + 64, 0);
  //writeTwoBytes(buf + 68, 0);
  memset(buf + 60, '\0', 32); // Set to zero above 3 + 20 bytes reserved space
  writeFourBytes(buf + 92, 7);
  writeFourBytes(buf + 96, 3028000);
  memset(buf + 100, '\0', page_size - 100); // Set remaing page to zero

  // master table b-tree
  buf[100] = 13; // Leaf table b-tree page
  writeTwoBytes(buf + 101, 0); // No freeblocks
  writeTwoBytes(buf + 103, 1); // Only one record
  writeTwoBytes(buf + 107, 0); // Fragmented free bytes

  // vRecLen + vRowID + vHdrLen(6) + FieldLens + Data
  int rec_hdr_len = 3 + 1 + 1 + (1 + 1 + 1 + 1 + 3);
  int rec_data_len = 5; // type="table"
  if (table_name == NULL)
    table_name = default_table_name;
  rec_data_len += strlen(table_name) * 2;
  rec_data_len += 4; // root page
  int script_len = 0;
  if (table_script != NULL) {
    script_len = strlen(table_script);
    rec_data_len += script_len * 2 + 136;
  } else {
    // create table <table_name> (c01, c02, ...)
    script_len = (13 + strlen(table_name) + 2 + 5 * ctx->col_count);
    rec_data_len += (script_len * 2 + 13);
  }
  int rec_pos = page_size - rec_hdr_len - rec_data_len;
  writeTwoBytes(buf + 105, rec_pos); // Start of cell-content area
  writeVInt3Bytes(buf + rec_pos, rec_hdr_len + rec_data_len); // Rec Len
  writeVInt1Byte(buf + rec_pos + 3, 1); // RowID
  writeVInt1Byte(buf + rec_pos + 4, 6); // Hdr Len
  writeVInt1Byte(buf + rec_pos + 5, 23); // type: "table"  (5 * 2 + 13)
  writeVInt1Byte(buf + rec_pos + 6, strlen(table_name) * 2 + 13);
  writeVInt1Byte(buf + rec_pos + 7, strlen(table_name) * 2 + 13);
  writeVInt3Bytes(buf + rec_pos + 8, script_len * 2 + 13);
  // field values
  memcpy(buf + rec_pos + 11, "table", 5);
  memcpy(buf + rec_pos + 16, table_name, strlen(table_name));
  memcpy(buf + rec_pos + 16 + strlen(table_name), table_name, strlen(table_name));
  writeVInt4Bytes(buf + rec_pos + 16 + strlen(table_name) * 2, 0);
  if (table_script == NULL) {
  } else
    memcpy(buf + rec_pos + 20 + strlen(table_name) * 2, table_script, strlen(table_script));

}

int create_header(struct ulog_sqlite_context *ctx, char *table_script) {

  if (ctx->page_size_exp < 9 || ctx->page_size_exp > 16)
    return ULS_RES_INV_PAGE_SZ;

  byte *buf = (byte *) ctx->buf;
  uint16_t page_size = 1;
  if (ctx->page_size_exp < 16)
    page_size <<= (ctx->page_size_exp - 1);
  writeTwoBytes(buf + 16, page_size);
  formHeader(buf, page_size);

  // write location of last leaf as 0

  ctx->is_row_created = 0;

}

int ulog_sqlite_init(struct ulog_sqlite_context *ctx) {
  return ulog_sqlite_init_with_ddl(ctx, 0);
}

int ulog_sqlite_init_with_script(struct ulog_sqlite_context *ctx, char *table_script) {

  int res = create_header(ctx, table_script);
  if (!res)
    return res;

}

int ulog_sqlite_new_row(struct ulog_sqlite_context *ctx) {

  // vRecLen + vRowID + vHdrLen(6) + FieldLens + Data
  // write empty record into buf from cur pos

  ctx->is_row_created = 1;
  return 0;
}

int ulog_sqlite_set_val(struct ulog_sqlite_context *ctx, int col_idx,
                          int type, void *val, int len) {

  if (!ctx->is_row_created)
    ulog_sqlite_new_row(ctx);

  // identify datatype
  // Calculate new len
  // If overflow, write and move row to new page
  // set col_len
  // set col_val
  // update 

  return 0;
}

int ulog_sqlite_finalize(struct ulog_sqlite_context *ctx, void *another_buf) {
  // close row
  // write last row
  // update last page into first page
  // write parent nodes recursively
  // update root page and db size into first page
  //writeFourBytes(buf + 28, 0); // DB Size
  return 0;
}

int ulog_sqlite_recover(struct ulog_sqlite_context *ctx, void *another_buf) {
  // check if last page number available
  // if not, locate last leaf page from end
  // continue from (3) of finalize
}

int main() {
  return 0;
}
