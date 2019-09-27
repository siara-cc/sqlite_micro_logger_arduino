#ifndef ARDUINO

#include "ulog_sqlite.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

int fd;

int32_t read_fn(struct uls_write_context *ctx, void *buf, size_t len) {
  ssize_t ret = read(fd, buf, len);
  if (ret == -1) {
    ctx->err_no = errno;
    return ULS_RES_READ_ERR;
  }
  return ret;
}

int seek_fn(struct uls_write_context *ctx, long pos) {
  off_t ret = lseek(fd, pos, SEEK_SET);
  if (ret == -1) {
    ctx->err_no = errno;
    return ULS_RES_SEEK_ERR;
  }
  return ULS_RES_OK;
}

int32_t write_fn(struct uls_write_context *ctx, void *buf, size_t len) {
  ssize_t ret = write(fd, buf, len);
  if (ret == -1) {
    ctx->err_no = errno;
    return ULS_RES_WRITE_ERR;
  }
  return ret;
}

int flush_fn(struct uls_write_context *ctx) {
  int ret = fsync(fd);
  if (ret == -1) {
    ctx->err_no = errno;
    return ULS_RES_FLUSH_ERR;
  }
  return ULS_RES_OK;
}

int test_multilevel(char *filename) {

  int32_t page_size = 65536;
  byte buf[page_size];
  struct uls_write_context ctx;
  ctx.buf = buf;
  ctx.col_count = 4;
  ctx.page_size_exp = 16;
  ctx.max_pages_exp = 0;
  ctx.read_fn = read_fn;
  ctx.seek_fn = seek_fn;
  ctx.flush_fn = flush_fn;
  ctx.write_fn = write_fn;

  unlink(filename);
  fd = open(filename, O_CREAT | O_EXCL | O_TRUNC | O_RDWR | O_SYNC, S_IRUSR | S_IWUSR);

  char txt[11];
  uls_write_init(&ctx);
  int32_t max_rows = 1000000;
  for (int32_t i = 0; i < max_rows; i++) {
    double d = i;
    d /= 2;
    uls_set_col_val(&ctx, 0, ULS_TYPE_INT, &i, sizeof(i));
    uls_set_col_val(&ctx, 1, ULS_TYPE_REAL, &d, sizeof(d));
    d = rand();
    d /= 1000;
    uls_set_col_val(&ctx, 2, ULS_TYPE_REAL, &d, sizeof(d));
    int txt_len = rand() % 10;
    for (int j = 0; j < txt_len; j++)
      txt[j] = 'a' + (char)(rand() % 26);
    uls_set_col_val(&ctx, 3, ULS_TYPE_TEXT, txt, txt_len);
    if (i < max_rows - 1)
      uls_create_new_row(&ctx);
  }
  if (uls_finalize(&ctx)) {
    printf("Error during finalize\n");
    return -6;
  }

  close(fd);

  return 0;

}

int test_basic(char *filename) {

  byte buf[512];
  struct uls_write_context ctx;
  ctx.buf = buf;
  ctx.col_count = 5;
  ctx.page_size_exp = 9;
  ctx.max_pages_exp = 0;
  ctx.read_fn = read_fn;
  ctx.seek_fn = seek_fn;
  ctx.flush_fn = flush_fn;
  ctx.write_fn = write_fn;

  unlink(filename);
  fd = open(filename, O_CREAT | O_EXCL | O_TRUNC | O_RDWR | O_SYNC, S_IRUSR | S_IWUSR);

  uls_write_init(&ctx);
  uls_set_col_val(&ctx, 0, ULS_TYPE_TEXT, "Hello", 5);
  uls_set_col_val(&ctx, 1, ULS_TYPE_TEXT, "World", 5);
  uls_set_col_val(&ctx, 2, ULS_TYPE_TEXT, "How", 3);
  uls_set_col_val(&ctx, 3, ULS_TYPE_TEXT, "Are", 3);
  uls_set_col_val(&ctx, 4, ULS_TYPE_TEXT, "You", 3);
  uls_create_new_row(&ctx);
  uls_set_col_val(&ctx, 0, ULS_TYPE_TEXT, "I", 1);
  uls_set_col_val(&ctx, 1, ULS_TYPE_TEXT, "am", 2);
  uls_set_col_val(&ctx, 2, ULS_TYPE_TEXT, "fine", 4);
  //uls_set_col_val(&ctx, 3, ULS_TYPE_TEXT, "Suillus bovinus, the Jersey cow mushroom, is a pored mushroom in the family Suillaceae. A common fungus native to Europe and Asia, it has been introduced to North America and Australia. It was initially described as Boletus bovinus by Carl Linnaeus in 1753, and given its current binomial name by Henri François Anne de Roussel in 1806. It is an edible mushroom, though not highly regarded. The fungus grows in coniferous forests in its native range, and pine plantations elsewhere. It is sometimes parasitised by the related mushroom Gomphidius roseus. S. bovinus produces spore-bearing mushrooms, often in large numbers, each with a convex grey-yellow or ochre cap reaching up to 10 cm (4 in) in diameter, flattening with age. As in other boletes, the cap has spore tubes extending downward from the underside, rather than gills. The pore surface is yellow. The stalk, more slender than those of other Suillus boletes, lacks a ring. (Full article...)", 953);
  uls_set_col_val(&ctx, 3, ULS_TYPE_TEXT, "thank", 5);
  uls_set_col_val(&ctx, 4, ULS_TYPE_TEXT, "you", 3);
  uls_flush(&ctx);

  close(fd);

  return 0;

}

void print_usage() {
  printf("\nSqlite Micro Logger\n");
  printf("-------------------\n\n");
  printf("Sqlite Micro logger is a library that logs records in Sqlite format 3\n");
  printf("using as less memory as possible. This utility is intended for testing it.\n\n");
  printf("Usage\n");
  printf("-----\n\n");
  printf("ulog_sqlite -c <db_name.db> <page_size> <col_count> <csv_1> ... <csv_n>\n");
  printf("    Creates a Sqlite database with the given name and page size\n");
  printf("        and given records in CSV format (no comma in data)\n\n");
  printf("ulog_sqlite -a <db_name.db> <csv_1> ... <csv_n>\n");
  printf("    Appends to a Sqlite database created using -c above\n");
  printf("        with records in CSV format\n\n");
  printf("ulog_sqlite -f <db_name.db>\n");
  printf("    Finalizes DB created to be used as a SQLite database\n\n");
  printf("ulog_sqlite -r\n");
  printf("    Runs pre-defined tests\n\n");
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

int add_col(struct uls_write_context *ctx, int col_idx, char *data, byte isInt, byte isReal) {
  if (isInt) {
    int64_t ival = atoll(data);
    if (ival >= -128 && ival <= 127) {
      int8_t i8val = (int8_t) ival;
      return uls_set_col_val(ctx, col_idx, ULS_TYPE_INT, &i8val, 1);
    } else
    if (ival >= -32768 && ival <= 32767) {
      int16_t i16val = (int16_t) ival;
      return uls_set_col_val(ctx, col_idx, ULS_TYPE_INT, &i16val, 2);
    } else
    if (ival >= -2147483648 && ival <= 2147483647) {
      int32_t i32val = (int32_t) ival;
      return uls_set_col_val(ctx, col_idx, ULS_TYPE_INT, &i32val, 4);
    } else {
      return uls_set_col_val(ctx, col_idx, ULS_TYPE_INT, &ival, 8);
    }
  } else
  if (isReal) {
    //float dval = atof(data);
    double dval = atof(data);
    printf("%lf\n", dval);
    return uls_set_col_val(ctx, col_idx, ULS_TYPE_REAL, &dval, sizeof(dval));
  }
  return uls_set_col_val(ctx, col_idx, ULS_TYPE_TEXT, data, strlen(data));
}

int create_db(int argc, char *argv[]) {
  long page_size = atol(argv[3]);
  byte page_size_exp = validate_page_size(page_size);
  if (!page_size_exp) {
    printf("Page size should be one of 512, 1024, 2048, 4096, 8192, 16384, 32768 or 65536\n");
    return -1;
  }
  byte col_count = atoi(argv[4]);
  byte buf[page_size];
  struct uls_write_context ctx;
  ctx.buf = buf;
  ctx.col_count = col_count;
  ctx.page_size_exp = page_size_exp;
  ctx.max_pages_exp = 0;
  ctx.read_fn = read_fn;
  ctx.seek_fn = seek_fn;
  ctx.flush_fn = flush_fn;
  ctx.write_fn = write_fn;
  unlink(argv[2]);
  fd = open(argv[2], O_CREAT | O_EXCL | O_TRUNC | O_RDWR | O_SYNC, S_IRUSR | S_IWUSR);
  if (fd == -1) {
    perror("Error");
    return -2;
  }
  if (uls_write_init(&ctx)) {
    printf("Error during init\n");
    return -3;
  }
  for (int i = 5; i < argc; i++) {
    char *col_data = argv[i];
    char *chr = col_data;
    int col_idx = 0;
    byte isInt = 1;
    byte isReal = 1;
    while (*chr != '\0') {
      if (*chr == ',') {
        *chr = '\0';
        if (add_col(&ctx, col_idx++, col_data, isInt, isReal)) {
          printf("Error during add col\n");
          return -4;
        }
        chr++;
        col_data = chr;
        isInt = 1;
        isReal = 1;
        continue;
      }
      if ((*chr < '0' || *chr > '9') && *chr != '-' && *chr != '.') {
        isInt = 0;
        isReal = 0;
      } else {
        if (*chr == '.')
          isInt = 0;
      }
      chr++;
    }
    if (add_col(&ctx, col_idx++, col_data, isInt, isReal)) {
      printf("Error during add col\n");
      return -4;
    }
    if (i < argc - 1) {
      if (uls_create_new_row(&ctx)) {
        printf("Error during add col\n");
        return -5;
      }
    }
  }
  if (uls_finalize(&ctx)) {
    printf("Error during finalize\n");
    return -6;
  }
  return 0;
}

int main(int argc, char *argv[]) {

  if (argc > 4 && strcmp(argv[1], "-c") == 0) {
    create_db(argc, argv);
  } else
  if (argc == 2 && strcmp(argv[1], "-r") == 0) {
    test_basic("hello.db");
    test_multilevel("ml.db");
  } else
    print_usage();

  return 0;

}
#endif
