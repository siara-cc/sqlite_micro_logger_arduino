#include "ulog_sqlite.h"
#include <SPI.h>
#include <SD.h>

File myFile;

int32_t read_fn(void *buf, size_t len) {
  size_t ret = myFile.read((byte *)buf, len);
  if (ret != len)
    return ULS_RES_ERR;
  return ret;
}

int seek_fn(long pos) {
  return myFile.seek(pos);
}

int32_t write_fn(void *buf, size_t len) {
  size_t ret = myFile.write((byte *)buf, len);
  if (ret != len)
    return ULS_RES_ERR;
  return ret;
}

int flush_fn() {
  myFile.flush();
  return 0;
}

void setup() {
  // Open serial communications and wait for port to open:
  Serial.begin(9600);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }

  Serial.print("Initializing SD card...");

  if (!SD.begin(4)) {
    Serial.println("initialization failed!");
    return;
  }
  Serial.println("initialization done.");

  // open the file. note that only one file can be open at a time,
  // so you have to close this one before opening another.
  myFile = SD.open("test.db", FILE_WRITE);

  // if the file opened okay, write to it:
  if (myFile) {
    Serial.print("Writing to test.db...");

    byte buf[512];
    struct ulog_sqlite_context ctx;
    ctx.buf = buf;
    ctx.col_count = 5;
    ctx.page_size_exp = 9;
    ctx.max_pages_exp = 0;
    ctx.read_fn = read_fn;
    ctx.seek_fn = seek_fn;
    ctx.flush_fn = flush_fn;
    ctx.write_fn = write_fn;

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
    //ulog_sqlite_set_val(&ctx, 3, ULS_TYPE_TEXT, "Suillus bovinus, the Jersey cow mushroom, is a pored mushroom in the family Suillaceae. A common fungus native to Europe and Asia, it has been introduced to North America and Australia. It was initially described as Boletus bovinus by Carl Linnaeus in 1753, and given its current binomial name by Henri François Anne de Roussel in 1806. It is an edible mushroom, though not highly regarded. The fungus grows in coniferous forests in its native range, and pine plantations elsewhere. It is sometimes parasitised by the related mushroom Gomphidius roseus. S. bovinus produces spore-bearing mushrooms, often in large numbers, each with a convex grey-yellow or ochre cap reaching up to 10 cm (4 in) in diameter, flattening with age. As in other boletes, the cap has spore tubes extending downward from the underside, rather than gills. The pore surface is yellow. The stalk, more slender than those of other Suillus boletes, lacks a ring. (Full article...)", 953);
    ulog_sqlite_set_val(&ctx, 3, ULS_TYPE_TEXT, "thank", 5);
    ulog_sqlite_set_val(&ctx, 4, ULS_TYPE_TEXT, "you", 3);
    ulog_sqlite_flush(&ctx);
    myFile.close();
    Serial.println("done.");

  } else {
    // if the file didn't open, print an error:
    Serial.println("error opening test.db");
  }

}

void loop() {
}
