#include "ulog_sqlite.h"

#include <stdlib.h>
#include <string.h>

int8_t get_sizeof_vint16(uint16_t vint) {
  return vint > 16383 ? 3 : (vint > 127 ? 2 : 1);
}

int8_t get_sizeof_vint32(uint32_t vint) {
  return vint > 268435455 ? 5 : (vint > 2097151 ? 4 
           : (vint > 16383 ? 3 : (vint > 127 ? 2 : 1)));
}

void write_uint8(byte *ptr, uint8_t input) {
  ptr[0] = input;
}

void write_uint16(byte *ptr, uint16_t input) {
  ptr[0] = input >> 8;
  ptr[1] = input & 0xFF;
}

void write_uint32(byte *ptr, uint32_t input) {
  int i = 4;
  while (i--)
    *ptr++ = (input >> (8 * i)) & 0xFF;
}

void write_uint64(byte *ptr, uint64_t input) {
  int i = 8;
  while (i--)
    *ptr++ = (input >> (8 * i)) & 0xFF;
}

int write_vint16(byte *ptr, uint16_t vint) {
  int len = get_sizeof_vint16(vint);
  for (int i = len - 1; i > 0; i--)
    *ptr++ = 0x80 + ((vint >> (7 * i)) & 0xFF);
  *ptr = vint & 0x7F;
  return len;
}

int write_vint32(byte *ptr, uint32_t vint) {
  int len = get_sizeof_vint32(vint);
  for (int i = len - 1; i > 0; i--)
    *ptr++ = 0x80 + ((vint >> (7 * i)) & 0xFF);
  *ptr = vint & 0x7F;
  return len;
}

uint16_t read_uint16(byte *ptr) {
  return (*ptr << 8) + ptr[1];
}

uint32_t read_uint32(byte *ptr) {
  uint32_t ret;
  ret = ((uint32_t)*ptr++) << 24;
  ret += ((uint32_t)*ptr++) << 16;
  ret += ((uint32_t)*ptr++) << 8;
  ret += *ptr;
  //ret = (*ptr << 24) + (ptr[1] << 16)
  //          + (ptr[2] << 8) + ptr[3];
  return ret;
}

uint16_t read_vint16(byte *ptr, int8_t *vlen) {
  uint16_t ret = 0;
  *vlen = 3; // read max 3 bytes
  do {
    ret <<= 7;
    ret += *ptr & 0x7F;
    (*vlen)--;
  } while ((*ptr++ & 0x80) == 0x80 && *vlen);
  *vlen = 3 - *vlen;
  return ret;
}

uint32_t read_vint32(byte *ptr, int8_t *vlen) {
  uint32_t ret = 0;
  *vlen = 5; // read max 5 bytes
  do {
    ret <<= 7;
    ret += *ptr & 0x7F;
    (*vlen)--;
  } while ((*ptr++ & 0x80) == 0x80 && *vlen);
  *vlen = 5 - *vlen;
  return ret;
}

uint16_t get_pagesize(byte page_size_exp) {
  uint16_t page_size = 1;
  if (page_size_exp < 16)
    page_size <<= page_size_exp;
  return page_size;
}

const int8_t col_data_lens[] = {0, 1, 2, 3, 4, 6, 8, 8, 0, 0, 0, 0};
uint16_t get_data_len(uint32_t col_type_or_len) {
  if (col_type_or_len < 12)
    return col_data_lens[col_type_or_len];
  if (col_type_or_len % 2)
    return (col_type_or_len - 13)/2;
  return (col_type_or_len - 12)/2; 
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
  int8_t vint_len;
  byte *hdr_ptr = rec_ptr;
  *prec_len = read_vint16(hdr_ptr, &vint_len);
  hdr_ptr += vint_len;
  read_vint32(hdr_ptr, &vint_len);
  hdr_ptr += vint_len;
  *phdr_len = read_vint16(hdr_ptr, &vint_len);
  *pdata_ptr = hdr_ptr + *phdr_len;
  hdr_ptr += vint_len;
  for (int i = 0; i < col_idx; i++) {
    uint32_t col_type_or_len = read_vint32(hdr_ptr, &vint_len);
    hdr_ptr += vint_len;
    (*pdata_ptr) += get_data_len(col_type_or_len);
  }
  return hdr_ptr;
}

uint32_t derive_col_type_or_len(int type, const void *val, int len) {
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

void init_bt_tbl_leaf(byte *ptr) {

  ptr[0] = 13; // Leaf table b-tree page
  write_uint16(ptr + 1, 0); // No freeblocks
  write_uint16(ptr + 3, 0); // No records yet
  write_uint16(ptr + 5, 0); // No records yet
  write_uint8(ptr + 7, 0); // Fragmented free bytes

}

void init_bt_tbl_inner(byte *ptr) {

  ptr[0] = 5; // Interior table b-tree page
  write_uint16(ptr + 1, 0); // No freeblocks
  write_uint16(ptr + 3, 0); // No records yet
  write_uint16(ptr + 5, 0); // No records yet
  write_uint8(ptr + 7, 0); // Fragmented free bytes

}

int add_rec_to_inner_tbl(struct ulog_sqlite_context *ctx, byte *another_buf, 
      uint32_t rowid, uint32_t cur_level_pos, byte is_last) {

  uint16_t page_size = get_pagesize(ctx->page_size_exp);
  uint16_t last_pos = read_uint16(another_buf + 5);
  int rec_count = read_uint16(another_buf + 3) + 1;
  byte rec_len = 4 + get_sizeof_vint32(rowid);

  if (last_pos == 0)
    last_pos = page_size - rec_len;
  else {
    // 12 = header, 5 = space for last rowid
    if (last_pos - rec_len < 12 + 5 + rec_count * 2)
      last_pos = 0;
    else
      last_pos -= rec_len;
  }

  if (is_last)
    last_pos = 0;
  if (last_pos) {
    write_uint32(another_buf + last_pos, cur_level_pos);
    write_vint32(another_buf + last_pos + 4, rowid);
    write_uint16(another_buf + 3, rec_count);
    write_uint32(another_buf + 5, last_pos);
  } else {
    write_uint32(another_buf + 8, cur_level_pos);
    write_vint32(another_buf + 12 + rec_count * 2, rowid);
    return 1;
  }

  return 0;

}

void write_rec_len_rowid_hdr_len(byte *ptr, uint16_t rec_len, uint32_t rowid, uint16_t hdr_len) {
  // write record len
  *ptr++ = 0x80 + (rec_len >> 14);
  *ptr++ = 0x80 + ((rec_len >> 7) & 0x7F);
  *ptr++ = rec_len & 0x7F;
  // write row id
  ptr += write_vint32(ptr, rowid);
  // write header len
  *ptr++ = 0x80 + (hdr_len >> 7);
  *ptr = hdr_len & 0x7F;
}

char default_table_name[] = "t1";

void form_page1(struct ulog_sqlite_context *ctx, int16_t page_size, char *table_name, char *table_script) {

  // 100 byte header - refer https://www.sqlite.org/fileformat.html
  byte *buf = (byte *) ctx->buf;
  //memcpy(buf, "uLogSQLite xxxx\0", 16);
  memcpy(buf, "SQLite format 3\0", 16);
  write_uint16(buf + 16, page_size);
  buf[18] = 1;
  buf[19] = 1;
  buf[20] = ctx->page_resv_bytes;
  buf[21] = 64;
  buf[22] = 32;
  buf[23] = 32;
  //write_uint32(buf + 24, 0);
  //write_uint32(buf + 28, 0);
  //write_uint32(buf + 32, 0);
  //write_uint32(buf + 36, 0);
  //write_uint32(buf + 40, 0);
  memset(buf + 24, '\0', 20); // Set to zero, above 5
  write_uint32(buf + 28, 2); // TODO: Update during finalize
  write_uint32(buf + 44, 4);
  //write_uint16(buf + 48, 0);
  //write_uint16(buf + 52, 0);
  memset(buf + 48, '\0', 8); // Set to zero, above 2
  write_uint32(buf + 56, 1);
  //write_uint16(buf + 60, 0);
  //write_uint16(buf + 64, 0);
  //write_uint16(buf + 68, 0);
  memset(buf + 60, '\0', 32); // Set to zero above 3 + 20 bytes reserved space
  write_uint32(buf + 92, 105);
  write_uint32(buf + 96, 3016000);
  memset(buf + 100, '\0', page_size - 100); // Set remaing page to zero

  // master table b-tree
  init_bt_tbl_leaf(buf + 100);

  // write table script record
  int orig_col_count = ctx->col_count;
  ctx->cur_page = 0;
  ctx->col_count = 5;
  ulog_sqlite_new_row(ctx);
  ulog_sqlite_set_val(ctx, 0, ULS_TYPE_TEXT, "table", 5);
  if (table_name == NULL)
    table_name = default_table_name;
  ulog_sqlite_set_val(ctx, 1, ULS_TYPE_TEXT, table_name, strlen(table_name));
  ulog_sqlite_set_val(ctx, 2, ULS_TYPE_TEXT, table_name, strlen(table_name));
  int32_t root_page = 2;
  ulog_sqlite_set_val(ctx, 3, ULS_TYPE_INT, &root_page, 4);
  // TODO: check whether will fit
  if (table_script)
    ulog_sqlite_set_val(ctx, 4, ULS_TYPE_TEXT, table_script, strlen(table_script));
  else {
    int table_name_len = strlen(table_name);
    int script_len = (13 + table_name_len + 2 + 5 * ctx->col_count);
    ulog_sqlite_set_val(ctx, 4, ULS_TYPE_TEXT, buf + 110, script_len);
    byte *script_pos = buf + page_size - buf[20] - script_len;
    memcpy(script_pos, "CREATE TABLE ", 13);
    script_pos += 13;
    memcpy(script_pos, table_name, table_name_len);
    script_pos += table_name_len;
    *script_pos++ = ' ';
    *script_pos++ = '(';
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
  ctx->cur_rowid = 0;
  init_bt_tbl_leaf(ctx->buf);
  ulog_sqlite_new_row(ctx);

}

int create_page1(struct ulog_sqlite_context *ctx, 
      char *table_name, char *table_script) {
  if (ctx->page_size_exp < 9 || ctx->page_size_exp > 16)
    return ULS_RES_INV_PAGE_SZ;
  byte *buf = (byte *) ctx->buf;
  uint16_t page_size = get_pagesize(ctx->page_size_exp);
  write_uint16(buf + 16, page_size);
  ctx->cur_rowid = 0;
  form_page1(ctx, page_size, table_name, table_script);
  return ULS_RES_OK;
}

int ulog_sqlite_init_with_script(struct ulog_sqlite_context *ctx, 
      char *table_name, char *table_script) {
  return create_page1(ctx, table_name, table_script);
}

int ulog_sqlite_init(struct ulog_sqlite_context *ctx) {
  return ulog_sqlite_init_with_script(ctx, 0, 0);
}

#define FIXED_LEN_OF_REC_LEN 3
#define FIXED_LEN_OF_HDR_LEN 2
int ulog_sqlite_new_row(struct ulog_sqlite_context *ctx) {

  ctx->cur_rowid++;
  byte *ptr = ctx->buf + (ctx->buf[0] == 13 ? 0 : 100);
  int rec_count = read_uint16(ptr + 3) + 1;
  uint16_t page_size = get_pagesize(ctx->page_size_exp);
  uint16_t len_of_rec_len_rowid = FIXED_LEN_OF_REC_LEN + get_sizeof_vint32(ctx->cur_rowid);
  uint16_t new_rec_len = ctx->col_count;
  new_rec_len += FIXED_LEN_OF_HDR_LEN;
  uint16_t last_pos = read_uint16(ptr + 5);
  if (last_pos == 0)
    last_pos = page_size - ctx->page_resv_bytes - new_rec_len - len_of_rec_len_rowid;
  else {
    last_pos -= new_rec_len;
    last_pos -= len_of_rec_len_rowid;
    if (last_pos < (ptr - ctx->buf) + 8 + rec_count * 2) {
      (ctx->seek_fn)(ctx->cur_page * page_size);
      (ctx->write_fn)(ctx->buf, page_size);
      ctx->cur_page++;
      init_bt_tbl_leaf(ctx->buf);
      last_pos = page_size - ctx->page_resv_bytes - new_rec_len - len_of_rec_len_rowid;
      rec_count = 1;
    }
  }

  memset(ctx->buf + last_pos, '\0', new_rec_len + len_of_rec_len_rowid);
  write_rec_len_rowid_hdr_len(ctx->buf + last_pos, new_rec_len, 
                              ctx->cur_rowid, ctx->col_count + FIXED_LEN_OF_HDR_LEN);
  write_uint16(ptr + 3, rec_count);
  write_uint16(ptr + 5, last_pos);
  write_uint16(ptr + 8 - 2 + (rec_count * 2), last_pos);
  ctx->flush_flag = 0xA5;

  return 0;
}

int ulog_sqlite_set_val(struct ulog_sqlite_context *ctx,
              int col_idx, int type, const void *val, uint16_t len) {

  byte *ptr = ctx->buf + (ctx->buf[0] == 13 ? 0 : 100);
  uint16_t page_size = get_pagesize(ctx->page_size_exp);
  uint16_t last_pos = acquire_last_pos(ctx, ptr);
  int rec_count = read_uint16(ptr + 3);
  byte *data_ptr;
  uint16_t rec_len;
  uint16_t hdr_len;
  byte *hdr_ptr = locate_column(ctx->buf + last_pos, col_idx, 
                        &data_ptr, &rec_len, &hdr_len);
  int8_t cur_len_of_len;
  uint16_t cur_len = get_data_len(read_vint32(hdr_ptr, &cur_len_of_len));
  uint16_t new_len = type == ULS_TYPE_REAL ? 8 : len;
  int32_t diff = new_len - cur_len;
  if (rec_len + diff + 2 > page_size - ctx->page_resv_bytes)
    return ULS_RES_TOO_LONG;
  uint16_t new_last_pos = last_pos + cur_len - new_len - FIXED_LEN_OF_HDR_LEN;
  if (new_last_pos < (ptr - ctx->buf) + 8 + rec_count * 2) {
    (ctx->seek_fn)(ctx->cur_page * page_size);
    (ctx->write_fn)(ctx->buf, page_size);
    ctx->cur_page++;
    init_bt_tbl_leaf(ctx->buf);
    memmove(ctx->buf + page_size - ctx->page_resv_bytes - rec_len,
            ctx->buf + last_pos, rec_len);
    hdr_ptr -= last_pos;
    data_ptr -= last_pos;
    last_pos = page_size - ctx->page_resv_bytes - rec_len - FIXED_LEN_OF_REC_LEN;
    hdr_ptr += last_pos;
    data_ptr += last_pos;
    rec_count = 1;
  }

  // make (or reduce) space and copy data
  new_last_pos = last_pos - diff;
  memmove(ctx->buf + new_last_pos, ctx->buf + last_pos,
          data_ptr - ctx->buf - last_pos);
  data_ptr -= diff;
  if (type == ULS_TYPE_INT) {
    switch (len) {
      case 1:
        write_uint8(data_ptr, *((int8_t *) val));
        break;
      case 2:
        write_uint16(data_ptr, *((int16_t *) val));
        break;
      case 4:
        write_uint32(data_ptr, *((int32_t *) val));
        break;
      case 8:
        write_uint64(data_ptr, *((int64_t *) val));
        break;
    }
  } else
  if (type == ULS_TYPE_REAL && len == 4) {
    // Assume float and double are already IEEE-754 format
    union FPSinglePrecIEEE754 {
      struct {
        uint32_t mantissa : 23;
        uint8_t exponent : 8;
        uint8_t sign : 1;
      } bits;
      byte raw[4];
    } fnumber;
    union FPDoublePrecIEEE754 {
      struct {
        uint64_t mantissa : 52;
        uint16_t exponent : 11;
        uint8_t sign : 1;
      } bits;
      byte raw[8];
    } dnumber;
    memcpy(&fnumber, val, 4);
    dnumber.bits.mantissa = fnumber.bits.mantissa;
    dnumber.bits.exponent = fnumber.bits.exponent;
    dnumber.bits.sign = fnumber.bits.sign;
    memcpy(data_ptr, &dnumber, 8);
  } else
    memcpy(data_ptr, val, len);

  // make (or reduce) space and copy len
  uint32_t new_type_or_len = derive_col_type_or_len(type, val, new_len);
  int8_t new_len_of_len = get_sizeof_vint32(new_type_or_len);
  int8_t hdr_diff = new_len_of_len -  cur_len_of_len;
  diff += hdr_diff;
  if (hdr_diff) {
    memmove(ctx->buf + new_last_pos - hdr_diff, ctx->buf + new_last_pos,
          hdr_ptr - ctx->buf - last_pos);
  }
  write_vint32(hdr_ptr - diff, new_type_or_len);

  new_last_pos -= hdr_diff;
  write_rec_len_rowid_hdr_len(ctx->buf + new_last_pos, rec_len + diff,
                              ctx->cur_rowid, hdr_len + hdr_diff);
  write_uint16(ptr + 5, new_last_pos);
  rec_count--;
  write_uint16(ptr + 8 + rec_count * 2, new_last_pos);
  ctx->flush_flag = 0xA5;

  return ULS_RES_OK;
}

int ulog_sqlite_flush(struct ulog_sqlite_context *ctx) {
  uint16_t page_size = get_pagesize(ctx->page_size_exp);
  (ctx->seek_fn)(ctx->cur_page * page_size);
  (ctx->write_fn)(ctx->buf, page_size);
  int ret = ctx->flush_fn();
  if (!ret)
    ctx->flush_flag = 0;
  return ret;
}

// page_count = 2 and prefix = "uLogSQLite xxxx" -> logging data
// page_count = x and prefix = "uLogSQLite xxxx" -> finalizing, x is last data page
// page_count = x and prefix = "SQLite format 3" -> finalize complete
int ulog_sqlite_finalize(struct ulog_sqlite_context *ctx, void *another_buf) {

  uint16_t page_size = get_pagesize(ctx->page_size_exp);
  if (ctx->flush_flag == 0xA5) {
    ulog_sqlite_flush(ctx);
    ctx->flush_flag = 0xA5;
  }

  (ctx->seek_fn)(0);
  (ctx->read_fn)(ctx->buf, page_size);
  if (memcmp(ctx->buf, "SQLite format 3", 15) == 0)
    return ULS_RES_OK;

  if (ctx->flush_flag == 0xA5) {
    write_uint32(ctx->buf + 28, ctx->cur_page + 1);
    (ctx->seek_fn)(0);
    (ctx->write_fn)(ctx->buf, page_size);
  }

  uint32_t next_level_cur_pos = ctx->cur_page + 1;
  uint32_t next_level_begin_pos = next_level_cur_pos;
  uint32_t cur_level_pos = 1;
  do {
    init_bt_tbl_inner(another_buf);
    while (cur_level_pos < next_level_begin_pos) {
      (ctx->seek_fn)(cur_level_pos * page_size);
      int32_t bytes_read = (ctx->read_fn)(ctx->buf, page_size);
      if (bytes_read != page_size) {
        cur_level_pos++;
        break;
      }
      uint32_t rowid;
      if (ctx->buf[0] == 13) {
        uint16_t rec_count = read_uint16(ctx->buf + 3) - 1;
        uint16_t rec_pos = 8 + rec_count * 2;
        int8_t vint_len;
        read_vint16(ctx->buf + rec_pos, &vint_len);
        rowid = read_vint32(ctx->buf + rec_pos + 3, &vint_len);
      } else {
        int8_t vint_len;
        rowid = read_vint32(ctx->buf + 12 + read_uint16(ctx->buf + 3) * 2, &vint_len);
      }
      byte is_last = (cur_level_pos + 1 == next_level_begin_pos ? 1 : 0);
      if (add_rec_to_inner_tbl(ctx, another_buf, rowid, cur_level_pos, is_last)) {
        (ctx->seek_fn)(next_level_cur_pos * page_size);
        next_level_cur_pos++;
        (ctx->write_fn)(ctx->buf, page_size);
        init_bt_tbl_inner(another_buf);
      }
      cur_level_pos++;
    }
    if (next_level_begin_pos == next_level_cur_pos - 1)
      break;
    else {
      cur_level_pos = next_level_begin_pos;
      next_level_cur_pos++;
      next_level_begin_pos = next_level_cur_pos;
    }
  } while (1);

  (ctx->seek_fn)(0);
  (ctx->read_fn)(ctx->buf, page_size);
  byte *data_ptr;
  uint16_t rec_len;
  uint16_t hdr_len;
  locate_column(ctx->buf + read_uint16(ctx->buf + 105), 3, &data_ptr, &rec_len, &hdr_len);
  write_uint32(data_ptr, next_level_begin_pos); // update root_page
  write_uint32(ctx->buf + 28, next_level_begin_pos); // update page_count
  memcpy(ctx->buf, "SQLite format 3", 16);
  (ctx->seek_fn)(0);
  (ctx->write_fn)(ctx->buf, page_size);

  return ULS_RES_OK;
}

int ulog_sqlite_check(struct ulog_sqlite_context *ctx) {
  // check if last page number available
  // if not, locate last leaf page from end
  // continue from (3) of finalize
  return 0;
}
