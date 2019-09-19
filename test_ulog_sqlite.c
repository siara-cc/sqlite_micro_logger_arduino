#ifndef ARDUINO

#include "src/ulog_sqlite.h"
#include <stdio.h>

FILE *file_ptr;

int read_fn(void *buf, size_t len) {
  size_t ret = fread(buf, len, 1, file_ptr);
  if (ret != len)
    return ULS_RES_ERR;
  return ULS_RES_OK;
}

int write_fn(void *buf, size_t len) {
  size_t ret = fwrite(buf, len, 1, file_ptr);
  if (ret != len)
    return ULS_RES_ERR;
  return ULS_RES_OK;
}

int seek_fn(long pos) {
  return fseek(file_ptr, pos, SEEK_SET);
}

int flush_fn() {
  return fflush(file_ptr);
}

int test_basic() {

  byte buf[4096];
  struct ulog_sqlite_context ctx;
  ctx.buf = buf;
  ctx.col_count = 5;
  ctx.page_size_exp = 12;
  ctx.max_pages_exp = 0;
  ctx.read_fn = read_fn;
  ctx.write_fn = write_fn;
  ctx.seek_fn = seek_fn;
  ctx.flush_fn = flush_fn;

  file_ptr = fopen("hello.db", "wb");

  ulog_sqlite_init(&ctx);
  ulog_sqlite_set_val(&ctx, 0, ULS_TYPE_TEXT, "Hello", 5);
  ulog_sqlite_set_val(&ctx, 1, ULS_TYPE_TEXT, "World", 5);
  ulog_sqlite_set_val(&ctx, 2, ULS_TYPE_TEXT, "How", 3);
  ulog_sqlite_set_val(&ctx, 3, ULS_TYPE_TEXT, "Are", 3);
  ulog_sqlite_set_val(&ctx, 4, ULS_TYPE_TEXT, "You", 3);
  ulog_sqlite_new_row(&ctx);
  ulog_sqlite_set_val(&ctx, 0, ULS_TYPE_TEXT, "I", 1);
  ulog_sqlite_set_val(&ctx, 1, ULS_TYPE_TEXT, "am", 2);
  ulog_sqlite_set_val(&ctx, 2, ULS_TYPE_TEXT, "fine", 4);
  ulog_sqlite_set_val(&ctx, 3, ULS_TYPE_TEXT, "Suillus bovinus, the Jersey cow mushroom, is a pored mushroom in the family Suillaceae. A common fungus native to Europe and Asia, it has been introduced to North America and Australia. It was initially described as Boletus bovinus by Carl Linnaeus in 1753, and given its current binomial name by Henri François Anne de Roussel in 1806. It is an edible mushroom, though not highly regarded. The fungus grows in coniferous forests in its native range, and pine plantations elsewhere. It is sometimes parasitised by the related mushroom Gomphidius roseus. S. bovinus produces spore-bearing mushrooms, often in large numbers, each with a convex grey-yellow or ochre cap reaching up to 10 cm (4 in) in diameter, flattening with age. As in other boletes, the cap has spore tubes extending downward from the underside, rather than gills. The pore surface is yellow. The stalk, more slender than those of other Suillus boletes, lacks a ring. (Full article...)", 953);
  //ulog_sqlite_set_val(&ctx, 3, ULS_TYPE_TEXT, "thank", 5);
  ulog_sqlite_set_val(&ctx, 4, ULS_TYPE_TEXT, "you", 3);
  ulog_sqlite_flush(&ctx);

  fclose(file_ptr);

  return 0;

}

int main() {

  test_basic();
  return 0;

}
#endif
