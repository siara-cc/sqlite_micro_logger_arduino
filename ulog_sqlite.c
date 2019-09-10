#include "ulog_sqlite.h"

#include <stdlib.h>
#include <string.h>

void writeTwoBytes(byte *ptr, uint16_t bytes) {
  ptr[0] = bytes >> 8;
  ptr[1] = bytes & 0xFF;
}

void writeFourBytes(byte *ptr, uint32_t bytes) {
  ptr[0] = bytes >> 24;
  ptr[1] = (bytes >> 16) & 0xFF;
  ptr[2] = (bytes >> 8) & 0xFF;
  ptr[3] = bytes & 0xFF;
}

void writeVInt1Byte(byte *ptr, byte val) {
  *ptr = val;
}

void writeVInt2Bytes(byte *ptr, uint16_t val) {
  *buf = 0x80 + (val >> 7);
  ptr[1] = val & 0x3F;
}

void writeVInt3Bytes(byte *ptr, uint32_t val) {
  *buf = 0x80 + (val >> 14);
  ptr[1] = 0x80 + ((val >> 7) & 0x3F);
  ptr[2] = val & 0x3F;
}

void writeVInt4Bytes(byte *ptr, uint32_t val) {
  *ptr = 0x80 + (val >> 21);
  ptr[1] = 0x80 + ((val >> 14) & 0x3F);
  ptr[2] = 0x80 + ((val >> 7) & 0x3F);
  ptr[3] = val & 0x3F;
}

uint16_t readHdrVInt(byte *ptr, int *vlen) {
  uint16_t ret = 0;
  *vlen = 2;
  do {
    ret << 7;
    ret += *ptr & 0x3F;
  } while ((*vlen)--);
  *vlen = 2 - *vlen;
  return ret;
}

char default_table_name[] = "t1";

void form_page1(struct ulog_sqlite_context *ctx, int16_t page_size, char *table_name, char *table_script) {

  // 100 byte header - refer https://www.sqlite.org/fileformat.html
  byte *buf = (byte *) ctx->buf;
  memcpy(buf, "SQLite format 3\0", 16);
  writeTwoBytes(buf + 16, page_size);
  buf[18] = 1;
  buf[19] = 1;
  buf[20] = 4; // Provision for checksum
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
  writeOneByte(buf + 107, 0); // Fragmented free bytes

  // write table script record
  int orig_col_count = ctx->col_count;
  ctx->col_count = 5;
  ctx->cur_rec_count = 0;
  ctx->cur_rec_pos = 0;
  writeEmptyRow(ctx);
  ulog_sqlite_set_val(ctx, 0, ULS_TYPE_TEXT, "table", 5);
  if (table_name == NULL)
    table_name = default_table_name;
  ulog_sqlite_set_val(ctx, 1, ULS_TYPE_TEXT, table_name, strlen(table_name));
  ulog_sqlite_set_val(ctx, 2, ULS_TYPE_TEXT, table_name, strlen(table_name));
  int32_t root_page = 0; // TODO: have fixed len for rootpage and save pos
  ulog_sqlite_set_val(ctx, 3, ULS_TYPE_INT, &root_page, 4);
  if (table_script)
    ulog_sqlite_set_val(ctx, 4, ULS_TYPE_TEXT, table_script, strlen(table_script));
  else {
    int script_len = (13 + strlen(table_name) + 2 + 5 * ctx->col_count);
    ulog_sqlite_set_val(ctx, 4, ULS_TYPE_TEXT, buf + 110, script_len);
    byte *script_pos = page_size - buf[20] - script_len;
    memcpy(script_pos, "CREATE TABLE ", 13);
    script_pos += 13;
    memcpy(script_pos, table_name, strlen(table_name));
    script_pos += strlen(table_name);
    *script_pos++ = ' ';
    *script_pos++ = '(';
    script_pos += 2;
    for (int i = 0; i < orig_col_count; ) {
      i++;
      *script_pos++ = 'c';
      *script_pos++ = '0' + (i < 100 ? 0 : (i / 100));
      *script_pos++ = '0' + (i < 10 ? 0 : ((i < 100 ? i : i - 100) / 10));
      *script_pos++ = '0' + (i % 10);
      *script_pos++ = (i == orig_col_count ? ')' : ',');
    }
  }
  ctx->col_count = orig_col_count;
  // write location of last leaf as 0

}

int create_page1(struct ulog_sqlite_context *ctx, char *table_name, char *table_script) {

  if (ctx->page_size_exp < 9 || ctx->page_size_exp > 16)
    return ULS_RES_INV_PAGE_SZ;

  byte *buf = (byte *) ctx->buf;
  uint16_t page_size = 1;
  if (ctx->page_size_exp < 16)
    page_size <<= (ctx->page_size_exp - 1);
  writeTwoBytes(buf + 16, page_size);
  form_page1(ctx, page_size, table_name, table_script);

  ctx->is_row_created = 0;
  ctx->cur_rec_count = 0;
  ctx->cur_rec_pos = 0;
  ctx->cur_rowid = 1;

}

int ulog_sqlite_init_with_script(struct ulog_sqlite_context *ctx, char *table_name,
      char *table_script) {

  int res = create_page1(ctx, table_name, table_script);
  if (!res)
    return res;

}

int ulog_sqlite_init(struct ulog_sqlite_context *ctx) {
  return ulog_sqlite_init_with_script(ctx, 0);
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
