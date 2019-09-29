#include "ulog_sqlite.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
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
    *ptr++ = 0x80 + ((vint >> (7 * i)) & 0x7F);
  *ptr = vint & 0x7F;
  return len;
}

int write_vint32(byte *ptr, uint32_t vint) {
  int len = get_sizeof_vint32(vint);
  for (int i = len - 1; i > 0; i--)
    *ptr++ = 0x80 + ((vint >> (7 * i)) & 0x7F);
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

int32_t get_pagesize(byte page_size_exp) {
  return (int32_t) 1 << page_size_exp;
}

uint16_t acquire_last_pos(struct uls_write_context *wctx, byte *ptr) {
  uint16_t last_pos = read_uint16(ptr + 5);
  if (last_pos == 0) {
    uls_create_new_row(wctx);
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
  rec_ptr = *pdata_ptr; // re-position to check for corruption below
  hdr_ptr += vint_len;
  for (int i = 0; i < col_idx; i++) {
    uint32_t col_type_or_len = read_vint32(hdr_ptr, &vint_len);
    hdr_ptr += vint_len;
    (*pdata_ptr) += uls_derive_data_len(col_type_or_len);
    if (hdr_ptr > rec_ptr + *phdr_len)
      return NULL;
  }
  return hdr_ptr;
}

uint32_t derive_col_type_or_len(int type, const void *val, int len) {
  uint32_t col_type_or_len = 0;
  if (val != NULL) {
    switch (type) {
      case ULS_TYPE_INT:
        col_type_or_len = (len == 1 ? 1 : (len == 2 ? 2 : (len == 4 ? 4 : 6)));
        //if (len == 1) {
        //  int8_t *typed_val = (int8_t *) val;
        //  col_type_or_len = (*typed_val == 0 ? 8 : (*typed_val == 1 ? 9 : 0));
        //} else
        //  col_type_or_len = (len == 2 ? 2 : (len == 4 ? 4 : 6));
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

int add_rec_to_inner_tbl(struct uls_write_context *wctx, byte *parent_buf, 
      uint32_t rowid, uint32_t cur_level_pos, byte is_last) {

  int32_t page_size = get_pagesize(wctx->page_size_exp);
  uint16_t last_pos = read_uint16(parent_buf + 5);
  int rec_count = read_uint16(parent_buf + 3) + 1;
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
  cur_level_pos++;
  if (last_pos) {
    write_uint32(parent_buf + last_pos, cur_level_pos);
    write_vint32(parent_buf + last_pos + 4, rowid);
    write_uint16(parent_buf + 3, rec_count--);
    write_uint16(parent_buf + 12 + rec_count * 2, last_pos);
    write_uint16(parent_buf + 5, last_pos);
  } else {
    write_uint32(parent_buf + 8, cur_level_pos);
    write_vint32(parent_buf + 12 + rec_count * 2, rowid);
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

int write_bytes(struct uls_write_context *wctx, byte *buf, long pos, int32_t size) {
  if ((wctx->seek_fn)(wctx, pos))
    return ULS_RES_SEEK_ERR;
  if ((wctx->write_fn)(wctx, buf, size) != size)
    return ULS_RES_WRITE_ERR;
  return ULS_RES_OK;
}

int read_bytes_wctx(struct uls_write_context *wctx, byte *buf, long pos, int32_t size) {
  if ((wctx->seek_fn)(wctx, pos))
    return ULS_RES_SEEK_ERR;
  if ((wctx->read_fn)(wctx, buf, size) != size)
    return ULS_RES_READ_ERR;
  return ULS_RES_OK;
}

int read_bytes_rctx(struct uls_read_context *rctx, byte *buf, long pos, int32_t size) {
  if ((rctx->seek_fn)(rctx, pos))
    return ULS_RES_SEEK_ERR;
  if ((rctx->read_fn)(rctx, buf, size) != size)
    return ULS_RES_READ_ERR;
  return ULS_RES_OK;
}

const char sqlite_sig[] = "SQLite format 3";
const char uls_sig[]    = "SQLite3 uLogger";

char default_table_name[] = "t1";
int form_page1(struct uls_write_context *wctx, int32_t page_size, char *table_name, char *table_script) {

  // 100 byte header - refer https://www.sqlite.org/fileformat.html
  byte *buf = (byte *) wctx->buf;
  memcpy(buf, uls_sig, 16);
  //memcpy(buf, "SQLite format 3\0", 16);
  write_uint16(buf + 16, page_size == 65536 ? 1 : (uint16_t) page_size);
  buf[18] = 1;
  buf[19] = 1;
  buf[20] = wctx->page_resv_bytes;
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
  // User version initially 0, set to table leaf count
  // used to locate last leaf page for binary search
  // and move to last page.
  write_uint32(buf + 60, 0);
  write_uint32(buf + 64, 0);
  // App ID - set to 0xA5xxxxxx where A5 is signature
  // last 5 bits = wctx->max_pages_exp - set to 0 currently
  // till it is implemented
  write_uint32(buf + 68, 0xA5000000);
  memset(buf + 72, '\0', 20); // reserved space
  write_uint32(buf + 92, 105);
  write_uint32(buf + 96, 3016000);
  memset(buf + 100, '\0', page_size - 100); // Set remaing page to zero

  // master table b-tree
  init_bt_tbl_leaf(buf + 100);

  // write table script record
  int orig_col_count = wctx->col_count;
  wctx->cur_write_page = 0;
  wctx->col_count = 5;
  uls_create_new_row(wctx);
  uls_set_col_val(wctx, 0, ULS_TYPE_TEXT, "table", 5);
  if (table_name == NULL)
    table_name = default_table_name;
  uls_set_col_val(wctx, 1, ULS_TYPE_TEXT, table_name, strlen(table_name));
  uls_set_col_val(wctx, 2, ULS_TYPE_TEXT, table_name, strlen(table_name));
  int32_t root_page = 2;
  uls_set_col_val(wctx, 3, ULS_TYPE_INT, &root_page, 4);
  if (table_script) {
    uint16_t script_len = strlen(table_script);
    if (script_len > page_size - 100 - wctx->page_resv_bytes - 8 - 10)
      return ULS_RES_TOO_LONG;
    uls_set_col_val(wctx, 4, ULS_TYPE_TEXT, table_script, script_len);
  } else {
    int table_name_len = strlen(table_name);
    int script_len = (13 + table_name_len + 2 + 5 * orig_col_count);
    if (script_len > page_size - 100 - wctx->page_resv_bytes - 8 - 10)
      return ULS_RES_TOO_LONG;
    uls_set_col_val(wctx, 4, ULS_TYPE_TEXT, buf + 110, script_len);
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
  int res = write_bytes(wctx, wctx->buf, 0, page_size);
  if (res)
    return res;
  wctx->col_count = orig_col_count;
  wctx->cur_write_page = 1;
  wctx->cur_write_rowid = 0;
  init_bt_tbl_leaf(wctx->buf);
  uls_create_new_row(wctx);

  return ULS_RES_OK;

}

int create_page1(struct uls_write_context *wctx, 
      char *table_name, char *table_script) {
  if (wctx->page_size_exp < 9 || wctx->page_size_exp > 16)
    return ULS_RES_INV_PAGE_SZ;
  byte *buf = (byte *) wctx->buf;
  int32_t page_size = get_pagesize(wctx->page_size_exp);
  wctx->cur_write_rowid = 0;
  return form_page1(wctx, page_size, table_name, table_script);
}

uint32_t get_last_rowid(struct uls_write_context *wctx, uint32_t pos, int32_t page_size, uint32_t *out_rowid) {
  byte src_buf[12];
  int res = read_bytes_wctx(wctx, src_buf, pos * page_size, 12);
  if (res)
    return res;
  uint16_t last_pos = read_uint16(src_buf + 5);
  uint8_t page_type = *src_buf;
  res = read_bytes_wctx(wctx, src_buf, pos * page_size + last_pos, 12);
  if (res)
    return res;
  int8_t vint_len;
  *out_rowid = read_vint32(src_buf + (page_type == 13 ? 3 : 4), &vint_len);
  return ULS_RES_OK;
}

const void *get_col_val(byte *buf, uint16_t rec_data_pos, int col_idx, uint32_t *out_col_type) {
  byte *data_ptr;
  uint16_t rec_len;
  uint16_t hdr_len;
  byte *hdr_ptr = locate_column(buf + rec_data_pos, col_idx, &data_ptr, &rec_len, &hdr_len);
  if (!hdr_ptr)
    return NULL;
  int8_t cur_len_of_len;
  *out_col_type = read_vint32(hdr_ptr, &cur_len_of_len);
  return data_ptr;
}

int check_signature(byte *buf) {
  if (memcmp(buf, uls_sig, 16) && memcmp(buf, sqlite_sig, 16))
    return ULS_RES_INVALID_SIG;
  if (read_uint32(buf + 68) != 0xA5000000)
    return ULS_RES_INVALID_SIG;
  return ULS_RES_OK;
}

byte get_page_size_exp(int32_t page_size) {
  switch (page_size) {
    case 1:
      return 16;
    case 512:
      return 9;
    case 1024:
      return 10;
    case 2048:
      return 11;
    case 4096:
      return 12;
    case 8192:
      return 13;
    case 16384:
      return 14;
    case 32768:
      return 15;
  }
  return 0;
}

int uls_write_init_with_script(struct uls_write_context *wctx, 
      char *table_name, char *table_script) {
  return create_page1(wctx, table_name, table_script);
}

int uls_write_init(struct uls_write_context *wctx) {
  return uls_write_init_with_script(wctx, 0, 0);
}

#define LEN_OF_REC_LEN 3
#define LEN_OF_HDR_LEN 2
int uls_create_new_row(struct uls_write_context *wctx) {

  wctx->cur_write_rowid++;
  byte *ptr = wctx->buf + (wctx->buf[0] == 13 ? 0 : 100);
  int rec_count = read_uint16(ptr + 3) + 1;
  int32_t page_size = get_pagesize(wctx->page_size_exp);
  uint16_t len_of_rec_len_rowid = LEN_OF_REC_LEN + get_sizeof_vint32(wctx->cur_write_rowid);
  uint16_t new_rec_len = wctx->col_count;
  new_rec_len += LEN_OF_HDR_LEN;
  uint16_t last_pos = read_uint16(ptr + 5);
  if (last_pos == 0)
    last_pos = page_size - wctx->page_resv_bytes - new_rec_len - len_of_rec_len_rowid;
  else {
    last_pos -= new_rec_len;
    last_pos -= len_of_rec_len_rowid;
    if (last_pos < (ptr - wctx->buf) + 9 + rec_count * 2) {
      int res = write_bytes(wctx, wctx->buf, wctx->cur_write_page * page_size, page_size);
      if (res)
        return res;
      wctx->cur_write_page++;
      init_bt_tbl_leaf(wctx->buf);
      last_pos = page_size - wctx->page_resv_bytes - new_rec_len - len_of_rec_len_rowid;
      rec_count = 1;
    }
  }

  memset(wctx->buf + last_pos, '\0', new_rec_len + len_of_rec_len_rowid);
  write_rec_len_rowid_hdr_len(wctx->buf + last_pos, new_rec_len, 
                              wctx->cur_write_rowid, wctx->col_count + LEN_OF_HDR_LEN);
  write_uint16(ptr + 3, rec_count);
  write_uint16(ptr + 5, last_pos);
  write_uint16(ptr + 8 - 2 + (rec_count * 2), last_pos);
  wctx->flush_flag = 0xA5;

  return ULS_RES_OK;
}

int uls_set_col_val(struct uls_write_context *wctx,
              int col_idx, int type, const void *val, uint16_t len) {

  byte *ptr = wctx->buf + (wctx->buf[0] == 13 ? 0 : 100);
  int32_t page_size = get_pagesize(wctx->page_size_exp);
  uint16_t last_pos = acquire_last_pos(wctx, ptr);
  int rec_count = read_uint16(ptr + 3);
  byte *data_ptr;
  uint16_t rec_len;
  uint16_t hdr_len;
  byte *hdr_ptr = locate_column(wctx->buf + last_pos, col_idx, 
                        &data_ptr, &rec_len, &hdr_len);
  if (hdr_ptr == NULL)
    return ULS_RES_MALFORMED;
  int8_t cur_len_of_len;
  uint16_t cur_len = uls_derive_data_len(read_vint32(hdr_ptr, &cur_len_of_len));
  uint16_t new_len = type == ULS_TYPE_REAL ? 8 : len;
  int32_t diff = new_len - cur_len;
  if (rec_len + diff + 2 > page_size - wctx->page_resv_bytes)
    return ULS_RES_TOO_LONG;
  uint16_t new_last_pos = last_pos + cur_len - new_len - LEN_OF_HDR_LEN;
  if (new_last_pos < (ptr - wctx->buf) + 9 + rec_count * 2) {
    uint16_t prev_last_pos = read_uint16(ptr + 8 + (rec_count - 2) * 2);
    write_uint16(ptr + 3, rec_count - 1);
    write_uint16(ptr + 5, prev_last_pos);
    int res = write_bytes(wctx, wctx->buf, wctx->cur_write_page * page_size, page_size);
    if (res)
      return res;
    wctx->cur_write_page++;
    init_bt_tbl_leaf(wctx->buf);
    int8_t len_of_rowid;
    read_vint32(wctx->buf + last_pos + 3, &len_of_rowid);
    memmove(wctx->buf + page_size - wctx->page_resv_bytes 
            - len_of_rowid - rec_len - LEN_OF_REC_LEN,
            wctx->buf + last_pos, len_of_rowid + rec_len + LEN_OF_REC_LEN);
    hdr_ptr -= last_pos;
    data_ptr -= last_pos;
    last_pos = page_size - wctx->page_resv_bytes - len_of_rowid - rec_len - LEN_OF_REC_LEN;
    hdr_ptr += last_pos;
    data_ptr += last_pos;
    rec_count = 1;
    write_uint16(ptr + 3, rec_count);
    write_uint16(ptr + 5, last_pos);
  }

  // make (or reduce) space and copy data
  new_last_pos = last_pos - diff;
  memmove(wctx->buf + new_last_pos, wctx->buf + last_pos,
          data_ptr - wctx->buf - last_pos);
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
    // Assumes float is represented in IEEE-754 format
    uint32_t bytes = *((uint32_t *) val);
    uint8_t exp8 = (bytes >> 23) & 0xFF;
    uint16_t exp11 = exp8;
    if (exp11 != 0) {
      if (exp11 < 127)
        exp11 = 1023 - (127 - exp11);
      else
        exp11 = 1023 + (exp11 - 127);
    }
    uint64_t bytes64 = ((uint64_t)(bytes >> 31) << 63) 
       | ((uint64_t)exp11 << 52)
       | ((uint64_t)(bytes & 0x7FFFFF) << (52-23) );
    write_uint64(data_ptr, bytes64);
  } else
  if (type == ULS_TYPE_REAL && len == 8) {
    // Assumes double is represented in IEEE-754 format
    uint64_t bytes = *((uint64_t *) val);
    write_uint64(data_ptr, bytes);
  } else
    memcpy(data_ptr, val, len);

  // make (or reduce) space and copy len
  uint32_t new_type_or_len = derive_col_type_or_len(type, val, new_len);
  int8_t new_len_of_len = get_sizeof_vint32(new_type_or_len);
  int8_t hdr_diff = new_len_of_len -  cur_len_of_len;
  diff += hdr_diff;
  if (hdr_diff) {
    memmove(wctx->buf + new_last_pos - hdr_diff, wctx->buf + new_last_pos,
          hdr_ptr - wctx->buf - last_pos);
  }
  write_vint32(hdr_ptr - diff, new_type_or_len);

  new_last_pos -= hdr_diff;
  write_rec_len_rowid_hdr_len(wctx->buf + new_last_pos, rec_len + diff,
                              wctx->cur_write_rowid, hdr_len + hdr_diff);
  write_uint16(ptr + 5, new_last_pos);
  rec_count--;
  write_uint16(ptr + 8 + rec_count * 2, new_last_pos);
  wctx->flush_flag = 0xA5;

  return ULS_RES_OK;
}

const void *uls_get_col_val(struct uls_write_context *wctx,
        int col_idx, uint32_t *out_col_type) {
  uint16_t last_pos = read_uint16(wctx->buf + 5);
  if (last_pos == 0)
    return NULL;
  return get_col_val(wctx->buf, last_pos, col_idx, out_col_type);
}

int uls_flush(struct uls_write_context *wctx) {
  int32_t page_size = get_pagesize(wctx->page_size_exp);
  int res = write_bytes(wctx, wctx->buf, wctx->cur_write_page * page_size, page_size);
  if (res)
    return res;
  int ret = wctx->flush_fn(wctx);
  if (!ret)
    wctx->flush_flag = 0;
  return ret;
}

// page_count = 2 and prefix = "uLogSQLite xxxx" -> logging data
// page_count = x and prefix = "uLogSQLite xxxx" -> finalizing, x is last data page
// page_count = x and prefix = "SQLite format 3" -> finalize complete
int uls_finalize(struct uls_write_context *wctx) {

  int32_t page_size = get_pagesize(wctx->page_size_exp);
  if (wctx->flush_flag == 0xA5) {
    uls_flush(wctx);
    wctx->flush_flag = 0xA5;
  }

  int res = read_bytes_wctx(wctx, wctx->buf, 0, page_size);
  if (res)
    return res;
  if (memcmp(wctx->buf, sqlite_sig, 16) == 0)
    return ULS_RES_OK;

  // There was a flush just now, so update the last page in first page
  if (wctx->flush_flag == 0xA5) {
    write_uint32(wctx->buf + 60, wctx->cur_write_page);
    res = write_bytes(wctx, wctx->buf, 0, page_size);
    if (res)
      return res;
  }

  uint32_t next_level_cur_pos = wctx->cur_write_page + 1;
  uint32_t next_level_begin_pos = next_level_cur_pos;
  uint32_t cur_level_pos = 1;
  do {
    init_bt_tbl_inner(wctx->buf);
    while (cur_level_pos < next_level_begin_pos) {
      uint32_t rowid;
      if (get_last_rowid(wctx, cur_level_pos, page_size, &rowid)) {
        cur_level_pos++;
        break;
      }
      byte is_last = (cur_level_pos + 1 == next_level_begin_pos ? 1 : 0);
      if (add_rec_to_inner_tbl(wctx, wctx->buf, rowid, cur_level_pos, is_last)) {
        res = write_bytes(wctx, wctx->buf, next_level_cur_pos * page_size, page_size);
        if (res)
          return res;
        next_level_cur_pos++;
        init_bt_tbl_inner(wctx->buf);
      }
      cur_level_pos++;
    }
    if (next_level_begin_pos == next_level_cur_pos - 1)
      break;
    else {
      cur_level_pos = next_level_begin_pos;
      next_level_begin_pos = next_level_cur_pos;
    }
  } while (1);

  res = read_bytes_wctx(wctx, wctx->buf, 0, page_size);
  if (res)
    return res;
  byte *data_ptr;
  uint16_t rec_len;
  uint16_t hdr_len;
  if (!locate_column(wctx->buf + read_uint16(wctx->buf + 105), 3,
         &data_ptr, &rec_len, &hdr_len))
    return ULS_RES_MALFORMED;
  write_uint32(data_ptr, next_level_cur_pos); // update root_page
  write_uint32(wctx->buf + 28, next_level_cur_pos); // update page_count
  memcpy(wctx->buf, sqlite_sig, 16);
  res = write_bytes(wctx, wctx->buf, 0, page_size);
  if (res)
    return res;

  return ULS_RES_OK;
}

int uls_not_finalized(struct uls_write_context *wctx) {
  int res = read_bytes_wctx(wctx, wctx->buf, 0, 72);
  if (res)
    return res;
  if (memcmp(wctx->buf, sqlite_sig, 16) == 0)
    return ULS_RES_OK;
  return ULS_RES_NOT_FINALIZED;
}

int uls_init_for_append(struct uls_write_context *wctx) {
  int res = read_bytes_wctx(wctx, wctx->buf, 0, 72);
  if (res)
    return res;
  if (check_signature(wctx->buf))
    return ULS_RES_INVALID_SIG;
  int32_t page_size = read_uint16(wctx->buf + 16);
  if (page_size == 1)
    page_size = 65536;
  wctx->page_size_exp = get_page_size_exp(page_size);
  if (!wctx->page_size_exp)
    return ULS_RES_MALFORMED;
  res = read_bytes_wctx(wctx, wctx->buf, 0, page_size);
  if (res)
    return res;
  wctx->flush_flag = 0;
  wctx->cur_write_page = read_uint32(wctx->buf + 60);
  if (wctx->cur_write_page == 0)
    return ULS_RES_NOT_FINALIZED;
  memcpy(wctx->buf, uls_sig, 16);
  write_uint32(wctx->buf + 60, 0);
  res = write_bytes(wctx, wctx->buf, 0, page_size);
  if (res)
    return res;
  res = get_last_rowid(wctx, wctx->cur_write_page, page_size, &wctx->cur_write_rowid);
  if (res)
    return res;
  res = read_bytes_wctx(wctx, wctx->buf, wctx->cur_write_page, page_size);
  if (res)
    return res;
  return ULS_RES_OK;
}

int read_cur_page(struct uls_read_context *rctx) {
  int32_t page_size = get_pagesize(rctx->page_size_exp);
  int res = read_bytes_rctx(rctx, rctx->buf, rctx->cur_page * page_size, page_size);
  if (res)
    return res;
  if (rctx->buf[0] != 13)
    return ULS_RES_NOT_FOUND;
  return ULS_RES_OK;
}

int uls_read_init(struct uls_read_context *rctx) {
  int res = read_bytes_rctx(rctx, rctx->buf, 0, 72);
  if (res)
    return res;
  if (check_signature(rctx->buf))
    return ULS_RES_INVALID_SIG;
  int32_t page_size = read_uint16(rctx->buf + 16);
  rctx->page_size_exp = get_page_size_exp(page_size);
  if (!rctx->page_size_exp)
    return ULS_RES_INVALID_SIG;
  rctx->last_leaf_page = read_uint32(rctx->buf + 60);
  rctx->cur_page = 0;
  return ULS_RES_OK;
}

const void *uls_read_col_val(struct uls_read_context *rctx,
     int col_idx, uint32_t *out_col_type) {
  if (rctx->cur_page == 0)
    uls_read_first_row(rctx);
  return get_col_val(rctx->buf, 
    read_uint16(rctx->buf + 8 - 2 + rctx->cur_rec_pos * 2), 
    col_idx, out_col_type);
}

const int8_t col_data_lens[] = {0, 1, 2, 3, 4, 6, 8, 8};
uint32_t uls_derive_data_len(uint32_t col_type_or_len) {
  if (col_type_or_len >= 12) {
    if (col_type_or_len % 2)
      return (col_type_or_len - 13)/2;
    return (col_type_or_len - 12)/2; 
  } else
  if (col_type_or_len < 8)
    return col_data_lens[col_type_or_len];
  return 0;
}

int uls_read_first_row(struct uls_read_context *rctx) {
  rctx->cur_page = 1;
  if (read_cur_page(rctx))
    return ULS_RES_NOT_FOUND;
  rctx->cur_rec_pos = 0;
  return ULS_RES_OK;
}

int uls_read_next_row(struct uls_read_context *rctx) {
  uint16_t rec_count = read_uint16(rctx->buf + 3);
  rctx->cur_rec_pos++;
  if (rctx->cur_rec_pos == rec_count) {
    int32_t page_size = get_pagesize(rctx->page_size_exp);
    rctx->cur_page++;
    if (read_cur_page(rctx))
      return ULS_RES_NOT_FOUND;
    rctx->cur_rec_pos = 0;
  }
  return ULS_RES_OK;
}

int uls_read_prev_row(struct uls_read_context *rctx) {
  if (rctx->cur_rec_pos == 0) {
    if (rctx->cur_page == 1)
      return ULS_RES_NOT_FOUND;
    rctx->cur_page--;
    if (read_cur_page(rctx))
      return ULS_RES_NOT_FOUND;
    rctx->cur_rec_pos = read_uint16(rctx->buf + 3);
  }
  rctx->cur_rec_pos--;
  return ULS_RES_OK;
}

int uls_read_last_row(struct uls_read_context *rctx) {
  if (rctx->last_leaf_page == 0)
    return ULS_RES_NOT_FINALIZED;
  rctx->cur_page = rctx->last_leaf_page;
  if (read_cur_page(rctx))
    return ULS_RES_NOT_FOUND;
  rctx->cur_rec_pos = read_uint16(rctx->buf + 3) - 1;
  return ULS_RES_OK;
}

int read_last_rowid(struct uls_read_context *rctx, uint32_t pos, int32_t page_size, uint32_t *out_rowid, uint16_t *out_data_pos) {
  byte src_buf[12];
  int res = read_bytes_rctx(rctx, src_buf, pos * page_size, 12);
  if (res)
    return res;
  if (*src_buf != 13)
    return ULS_RES_MALFORMED;
  *out_data_pos = read_uint16(src_buf + 5);
  res = read_bytes_rctx(rctx, src_buf, pos * page_size + *out_data_pos, 12);
  if (res)
    return res;
  int8_t vint_len;
  *out_rowid = read_vint32(src_buf + 3, &vint_len);
  return ULS_RES_OK;
}

uint32_t read_rowid_at(struct uls_read_context *rctx, uint32_t rec_pos, uint16_t *out_data_pos) {
  int8_t vint_len;
  *out_data_pos = read_uint16(rctx->buf + 8 - 2 + rec_pos * 2);
  return read_vint32(rctx->buf + *out_data_pos + LEN_OF_REC_LEN, &vint_len);
}

int uls_bin_srch_row_by_id(struct uls_read_context *rctx, uint32_t rowid) {
  int32_t page_size = get_pagesize(rctx->page_size_exp);
  if (rctx->last_leaf_page == 0)
    return ULS_RES_NOT_FINALIZED;
  uint32_t middle, first, size;
  uint16_t rec_data_pos;
  int res;
  first = 1;
  size = rctx->last_leaf_page + 1;
  while (first < size) {
    middle = (first + size) >> 1;
    uint32_t rowid_at;
    res = read_last_rowid(rctx, middle, page_size, &rowid_at, &rec_data_pos);
    if (res)
      return res;
    if (rowid_at < rowid)
      first = middle + 1;
    else if (rowid_at > rowid)
      size = middle;
    else {
      rctx->cur_page = middle;
      rctx->cur_rec_pos = rec_data_pos;
      return ULS_RES_OK;
    }
  }
  uint32_t found_at_page = size;
  res = read_bytes_rctx(rctx, rctx->buf, size * page_size, page_size);
  if (res)
    return res;
  first = 0;
  size = read_uint16(rctx->buf + 3);
  while (first < size) {
    middle = (first + size) >> 1;
    uint32_t rowid_at = read_rowid_at(rctx, middle, &rec_data_pos);
    if (res)
      return res;
    if (rowid_at < rowid)
      first = middle + 1;
    else if (rowid_at > rowid)
      size = middle;
    else {
      rctx->cur_page = found_at_page;
      rctx->cur_rec_pos = rec_data_pos;
      return ULS_RES_OK;
    }
  }
  return ULS_RES_NOT_FOUND;
}

int uls_bin_srch_row_by_val(struct uls_read_context *rctx, byte *val, uint16_t len) {
  return ULS_RES_OK;
}
