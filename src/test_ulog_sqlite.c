#ifndef ARDUINO

#include "ulog_sqlite.h"
#include <stdio.h>
#include <string.h>

FILE *file_ptr;

int32_t read_fn(struct ulog_sqlite_context *ctx, void *buf, size_t len) {
  size_t ret = fread(buf, len, 1, file_ptr);
  if (ret != len)
    return ULS_RES_ERR;
  return ret;
}

int seek_fn(struct ulog_sqlite_context *ctx, long pos) {
  return fseek(file_ptr, pos, SEEK_SET);
}

int32_t write_fn(struct ulog_sqlite_context *ctx, void *buf, size_t len) {
  size_t ret = fwrite(buf, len, 1, file_ptr);
  if (ret != len)
    return ULS_RES_ERR;
  return ret;
}

int flush_fn(struct ulog_sqlite_context *ctx) {
  return fflush(file_ptr);
}

int test_basic() {

  byte buf[512];
  struct ulog_sqlite_context ctx;
  ctx.buf = buf;
  ctx.col_count = 5;
  ctx.page_size_exp = 12;
  ctx.max_pages_exp = 0;
  ctx.read_fn = read_fn;
  ctx.seek_fn = seek_fn;
  ctx.flush_fn = flush_fn;
  ctx.write_fn = write_fn;

  file_ptr = fopen("hello.db", "wb");

  ulog_sqlite_init(&ctx);
  ulog_sqlite_set_val(&ctx, 0, ULS_TYPE_TEXT, "Hello", 5);
  ulog_sqlite_set_val(&ctx, 1, ULS_TYPE_TEXT, "World", 5);
  ulog_sqlite_set_val(&ctx, 2, ULS_TYPE_TEXT, "How", 3);
  ulog_sqlite_set_val(&ctx, 3, ULS_TYPE_TEXT, "Are", 3);
  ulog_sqlite_set_val(&ctx, 4, ULS_TYPE_TEXT, "You", 3);
  ulog_sqlite_next_row(&ctx);
  ulog_sqlite_set_val(&ctx, 0, ULS_TYPE_TEXT, "I", 1);
  ulog_sqlite_set_val(&ctx, 1, ULS_TYPE_TEXT, "am", 2);
  ulog_sqlite_set_val(&ctx, 2, ULS_TYPE_TEXT, "fine", 4);
  //ulog_sqlite_set_val(&ctx, 3, ULS_TYPE_TEXT, "Suillus bovinus, the Jersey cow mushroom, is a pored mushroom in the family Suillaceae. A common fungus native to Europe and Asia, it has been introduced to North America and Australia. It was initially described as Boletus bovinus by Carl Linnaeus in 1753, and given its current binomial name by Henri François Anne de Roussel in 1806. It is an edible mushroom, though not highly regarded. The fungus grows in coniferous forests in its native range, and pine plantations elsewhere. It is sometimes parasitised by the related mushroom Gomphidius roseus. S. bovinus produces spore-bearing mushrooms, often in large numbers, each with a convex grey-yellow or ochre cap reaching up to 10 cm (4 in) in diameter, flattening with age. As in other boletes, the cap has spore tubes extending downward from the underside, rather than gills. The pore surface is yellow. The stalk, more slender than those of other Suillus boletes, lacks a ring. (Full article...)", 953);
  ulog_sqlite_set_val(&ctx, 3, ULS_TYPE_TEXT, "thank", 5);
  ulog_sqlite_set_val(&ctx, 4, ULS_TYPE_TEXT, "you", 3);
  ulog_sqlite_flush(&ctx);

  fclose(file_ptr);

  return 0;

}

void print_usage() {
  printf("\nSqlite Micro Logger\n");
  printf("-------------------\n\n");
  printf("Sqlite Micro logger is a low memory usage logger that logs records in Sqlite format 3\n\n");
  printf("Usage\n");
  printf("-----\n\n");
  printf("ulog_sqlite -c <db_name.db> <page_size> <col_count> <csv_1> ... <csv_n>\n");
  printf("    Creates a Sqlite database with the given name and page size\n");
  printf("        and given records in CSV format\n\n");
  printf("ulog_sqlite -a <db_name.db> <csv_1> ... <csv_n>\n");
  printf("    Appends to a Sqlite database created using -c above\n");
  printf("        with records in CSV format\n\n");
  printf("ulog_sqlite -f <db_name.db>\n");
  printf("    Finalizes DB created to be used as a SQLite database\n\n");
  printf("ulog_sqlite -r\n");
  printf("    Runs in-built tests\n\n");
}

byte validate_page_size(long page_size) {
  switch (page_size) {
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
    case 65536:
      return 16;
  }
  return 0;
}

int main(int argc, char *argv[]) {

  if (argc > 4 && strcmp(argv[1], "-c") != 0) {
    long page_size = atol(argv[3]);
    byte page_size_exp = validate_page_size(page_size);
    if (!page_size_exp) {
      printf("Page size should be one of 512, 1024, 2048, 4096, 8192, 16384, 32768 or 65536\n");
      return 0;
    }
    byte col_count = atoi(argv[4]);
    byte buf[page_size];
    struct ulog_sqlite_context ctx;
    ctx.buf = buf;
    ctx.col_count = col_count;
    ctx.page_size_exp = page_size_exp;
    ctx.max_pages_exp = 0;
    ctx.read_fn = read_fn;
    ctx.seek_fn = seek_fn;
    ctx.flush_fn = flush_fn;
    ctx.write_fn = write_fn;
    file_ptr = fopen(argv[2], "wb");
    if (file_ptr == NULL) {
      perror("Error: ");
      return -1;
    }
    ulog_sqlite_init(&ctx);
    ulog_sqlite_set_val(&ctx, 0, ULS_TYPE_TEXT, "Hello", 5);
  } else
  if (argc == 2 && strcmp(argv[1], "-r") != 0) {
    test_basic();
  }

  return 0;

}
#endif
