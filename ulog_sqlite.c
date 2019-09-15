#include "ulog_sqlite.h"

#include <stdlib.h>
#include <string.h>

void write_uint8(byte *ptr, uint8_t input) {
  ptr[0] = input;
}

void write_uint16(byte *ptr, uint16_t input) {
  ptr[0] = input >> 8;
  ptr[1] = input & 0xFF;
}

void write_uint32(byte *ptr, uint32_t input) {
  ptr[0] = input >> 24;
  ptr[1] = (input >> 16) & 0xFF;
  ptr[2] = (input >> 8) & 0xFF;
  ptr[3] = input & 0xFF;
}

int write_vint16(byte *ptr, uint16_t vint) {
  int len = get_sizeof_vint16(vint);
  for (int i = len - 1; i > 0; i--)
    *ptr++ = 0x80 + ((vint >> (7 * i)) & 0xFF);
  *ptr = vint & 0xFF;
  return len;
}

int write_vint32(byte *ptr, uint32_t vint) {
  int len = get_sizeof_vint32(vint);
  for (int i = len - 1; i > 0; i--)
    *ptr++ = 0x80 + ((vint >> (7 * i)) & 0xFF);
  *ptr = vint & 0xFF;
  return len;
}

byte get_sizeof_vint16(uint16_t vint) {
  return vint > 16383 ? 3 : (vint > 127 ? 2 : 1);
}

byte get_sizeof_vint32(uint32_t vint) {
  return vint > 268435455 ? 5 : (vint > 2097151 ? 4 
           : (vint > 16383 ? 3 : (vint > 127 ? 2 : 1)));
}

uint16_t read_uint16(byte *ptr) {
  return (*ptr << 8) + ptr[1];
}

uint32_t read_uint32(byte *ptr) {
  return (*ptr << 24) + (ptr[1] << 16)
            + (ptr[2] << 8) + ptr[3];
}

uint16_t read_vint16(byte *ptr, char *vlen) {
  uint16_t ret = 0;
  *vlen = 2;
  do {
    ret << 7;
    ret += *ptr & 0x3F;
  } while ((*ptr & 0x80) == 0 && (*vlen)--);
  *vlen = 2 - *vlen;
  return ret;
}

uint32_t read_vint32(byte *ptr, char *vlen) {
  uint32_t ret = 0;
  *vlen = 4;
  do {
    ret << 7;
    ret += *ptr & 0x3F;
  } while ((*ptr & 0x80) == 0 && (*vlen)--);
  *vlen = 4 - *vlen;
  return ret;
}

uint16_t get_pagesize(byte page_size_exp) {
  uint16_t page_size = 1;
  if (page_size_exp < 16)
    page_size <<= (page_size_exp - 1);
  return page_size;
}

const char col_data_lens[] = {0, 1, 2, 3, 4, 6, 8, 8, 0, 0, 0, 0};
uint16_t get_data_len(uint32_t col_type_or_len) {
  if (col_type_or_len < 12)
    return col_data_lens[col_type_or_len];
  if (col_type_or_len % 2)
    return (col_type_or_len - 13)/2;
  return (col_type_or_len - 12)/2; 
}

char get_hdr_len(uint32_t col_type_or_len) {
  return col_type_or_len < 12 ? 1 : get_sizeof_vint32(col_type_or_len);
}

uint16_t acquire_last_pos(struct ulog_sqlite_context *ctx, byte *ptr) {
  uint16_t last_pos = read_uint16(ptr + 5);
  if (last_pos == 0) {
    ulog_sqlite_new_row(ctx);
    last_pos = read_uint16(ptr + 5);
  }
  return last_pos;
}

byte *locate_column(byte *rec_ptr, int col_idx, byte **pdata_ptr, 
             uint16_t *prec_len, uint16_t *phdr_len) {
  char vint_len;
  byte *col_ptr = rec_ptr;
  *prec_len = read_vint16(col_ptr, &vint_len);
  col_ptr += vint_len;
  read_vint32(col_ptr, &vint_len);
  col_ptr += vint_len;
  *phdr_len = read_vint16(col_ptr, &vint_len);
  *pdata_ptr = col_ptr + *phdr_len;
  col_ptr += vint_len;
  for (int i = 0; i < col_idx; i++) {
    uint32_t col_type_or_len = read_vint32(col_ptr, &vint_len);
    col_ptr += vint_len;
    (*pdata_ptr) += get_data_len(col_type_or_len);
  }
  return col_ptr;
}

uint32_t derive_col_type_or_len(int type, void *val, int len) {
  uint32_t col_type_or_len = 0;
  if (val != NULL) {
    switch (type) {
      case ULS_TYPE_INT:
        if (len == 1) {
          int8_t *typed_val = (int8_t *) val;
          col_type_or_len = (*typed_val == 0 ? 8 : (*typed_val == 1 ? 9 : 0));
        } else
          col_type_or_len = (len == 2 ? 2 : (len == 4 ? 4 : 6));
        break;
      case ULS_TYPE_REAL:
        col_type_or_len = 7;
        break;
      case ULS_TYPE_BLOB:
        col_type_or_len = len * 2 + 12;
        break;
      case ULS_TYPE_TEXT:
        col_type_or_len = len * 2 + 13;
    }
  }
  return col_type_or_len;    
}

char default_table_name[] = "t1";

void form_page1(struct ulog_sqlite_context *ctx, int16_t page_size, char *table_name, char *table_script) {

  // 100 byte header - refer https://www.sqlite.org/fileformat.html
  byte *buf = (byte *) ctx->buf;
  memcpy(buf, "SQLite format 3\0", 16);
  write_uint16(buf + 16, page_size);
  buf[18] = 1;
  buf[19] = 1;
  buf[20] = 4; // Provision for checksum
  buf[21] = 64;
  buf[22] = 32;
  buf[23] = 32;
  //write_uint32(buf + 24, 0);
  //write_uint32(buf + 28, 0);
  //write_uint32(buf + 32, 0);
  //write_uint32(buf + 36, 0);
  //write_uint32(buf + 40, 0);
  memset(buf + 24, '\0', 20); // Set to zero, above 5
  write_uint32(buf + 44, 4);
  //write_uint16(buf + 48, 0);
  //write_uint16(buf + 52, 0);
  memset(buf + 48, '\0', 8); // Set to zero, above 2
  write_uint32(buf + 56, 1);
  //write_uint16(buf + 60, 0);
  //write_uint16(buf + 64, 0);
  //write_uint16(buf + 68, 0);
  memset(buf + 60, '\0', 32); // Set to zero above 3 + 20 bytes reserved space
  write_uint32(buf + 92, 7);
  write_uint32(buf + 96, 3028000);
  memset(buf + 100, '\0', page_size - 100); // Set remaing page to zero

  // master table b-tree
  init_btree_page(buf + 100);

  // write table script record
  int orig_col_count = ctx->col_count;
  ctx->col_count = 5;
  ulog_sqlite_new_row(ctx);
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
  (ctx->seek_fn)(0);
  (ctx->write_fn)(ctx->buf, page_size);
  ctx->col_count = orig_col_count;
  ctx->cur_page = 1;
  ctx->cur_rowid = 1;
  // write location of last leaf as 0

}

int create_page1(struct ulog_sqlite_context *ctx, 
      char *table_name, char *table_script) {

  if (ctx->page_size_exp < 9 || ctx->page_size_exp > 16)
    return ULS_RES_INV_PAGE_SZ;

  byte *buf = (byte *) ctx->buf;
  uint16_t page_size = get_pagesize(ctx->page_size_exp);
  write_uint16(buf + 16, page_size);
  form_page1(ctx, page_size, table_name, table_script);

  ctx->cur_rowid = 1;

}

void init_btree_page(byte *ptr) {

  ptr[0] = 13; // Leaf table b-tree page
  write_uint16(ptr + 1, 0); // No freeblocks
  write_uint16(ptr + 3, 0); // No records yet
  write_uint16(ptr + 5, 0); // No records yet
  write_uint8(ptr + 7, 0); // Fragmented free bytes

}

int ulog_sqlite_init_with_script(struct ulog_sqlite_context *ctx, 
      char *table_name, char *table_script) {

  int res = create_page1(ctx, table_name, table_script);
  if (!res)
    return res;

}

int ulog_sqlite_init(struct ulog_sqlite_context *ctx) {
  return ulog_sqlite_init_with_script(ctx, 0, 0);
}

int ulog_sqlite_new_row(struct ulog_sqlite_context *ctx) {

  byte *ptr = ctx->buf + (ctx->buf[0] == 13 ? 100 : 0);
  int rec_count = read_uint16(ptr + 3) + 1;
  uint16_t page_size = get_pagesize(ctx->page_size_exp);
  uint16_t new_rec_len = ctx->col_count + get_sizeof_vint32(ctx->cur_rowid);
  new_rec_len += get_sizeof_vint16(new_rec_len);
  uint16_t last_pos = read_uint16(ptr + 5);
  if (last_pos == 0)
    last_pos = ctx->buf + page_size - 4 - new_rec_len;
  else {
    last_pos -= new_rec_len;
    if (last_pos < (ptr - ctx->buf) + 8 + rec_count * 2) {
      (ctx->seek_fn)(ctx->cur_page * page_size);
      (ctx->write_fn)(ctx->buf, page_size);
      init_btree_page(ctx->buf);
      last_pos = ctx->buf + page_size - 4 - new_rec_len;
      rec_count = 1;
    }
  }

  memset(ctx->buf + last_pos, '\0', new_rec_len);
  int vint_len = write_vint16(ctx->buf + last_pos, new_rec_len);
  vint_len = write_vint32(ctx->buf + last_pos + vint_len, ctx->cur_rowid++);
  write_uint16(ptr + 3, rec_count);
  write_uint16(ptr + 5, last_pos);
  write_uint16(ptr + 8 - 2 + (rec_count * 2), last_pos);

  return 0;
}

int ulog_sqlite_set_val(struct ulog_sqlite_context *ctx,
              int col_idx, int type, void *val, int len) {

  byte *ptr = ctx->buf + (ctx->buf[0] == 13 ? 100 : 0);
  int rec_count = read_uint16(ptr + 3);
  uint16_t page_size = get_pagesize(ctx->page_size_exp);
  uint16_t last_pos = acquire_last_pos(ctx, ptr);
  byte *data_ptr;
  uint16_t rec_len;
  uint16_t hdr_len;
  byte *col_ptr = locate_column(ctx->buf + last_pos, col_idx, 
                        &data_ptr, &rec_len, &hdr_len);
  char vint_len;
  uint32_t old_type_or_len = read_vint32(col_ptr, &vint_len);
  char old_hdr_len = get_hdr_len(old_type_or_len);
  uint16_t old_data_len = get_data_len(old_type_or_len);
  uint32_t new_type_or_len = derive_col_type_or_len(type, val, len);
  char new_hdr_len = get_hdr_len(new_type_or_len);
  uint16_t new_data_len = get_data_len(new_type_or_len);
  last_pos += old_hdr_len;
  last_pos += old_data_len;
  last_pos -= new_hdr_len;
  last_pos -= new_data_len;
  if (last_pos < (ptr - ctx->buf) + 8 + rec_count * 2) {
    (ctx->seek_fn)(ctx->cur_page * page_size);
    (ctx->write_fn)(ctx->buf, page_size);
    init_btree_page(ctx->buf);
    rec_len -= old_hdr_len;
    rec_len -= old_data_len;
    rec_len += new_hdr_len;
    rec_len += new_data_len;
    last_pos = ctx->buf + page_size - 4 - rec_len;
    rec_count = 1;
  }
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
  //write_uint32(buf + 28, 0); // DB Size
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
