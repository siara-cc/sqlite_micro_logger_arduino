/*
  This example demonstrates how the Sqlite Micro Logger library
  can be used to write Analog data into Sqlite database.
  Works on any Arduino compatible microcontroller with SD Card attachment
  having 2kb RAM or more (such as Arduino Uno).

  How Sqlite Micro Logger works:
  https://github.com/siara-cc/sqlite_micro_logger_c

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
#include "ulog_sqlite.h"
#include <SPI.h>
#include <FS.h>
#include "SPIFFS.h"
#include "SD_MMC.h"
#include "SD.h"

#define FORMAT_SPIFFS_IF_FAILED true
#define MAX_FILE_NAME_LEN 100
#define MAX_STR_LEN 500

// If you would like to read DBs created with this repo
// with page size more than 4096, change here,
// but ensure as much SRAM can be allocated.
#define BUF_SIZE 4096
byte buf[BUF_SIZE];
char filename[MAX_FILE_NAME_LEN];
extern const char sqlite_sig[];

FILE *myFile;

int32_t read_fn_wctx(struct dblog_write_context *ctx, void *buf, uint32_t pos, size_t len) {
  if (fseek(myFile, pos, SEEK_SET))
    return DBLOG_RES_SEEK_ERR;
  size_t ret = fread(buf, 1, len, myFile);
  if (ret != len)
    return DBLOG_RES_READ_ERR;
  return ret;
}

int32_t read_fn_rctx(struct dblog_read_context *ctx, void *buf, uint32_t pos, size_t len) {
  if (fseek(myFile, pos, SEEK_SET))
    return DBLOG_RES_SEEK_ERR;
  size_t ret = fread(buf, 1, len, myFile);
  if (ret != len)
    return DBLOG_RES_READ_ERR;
  return ret;
}

int32_t write_fn(struct dblog_write_context *ctx, void *buf, uint32_t pos, size_t len) {
  if (fseek(myFile, pos, SEEK_SET))
    return DBLOG_RES_SEEK_ERR;
  size_t ret = fwrite(buf, 1, len, myFile);
  if (ret != len)
    return DBLOG_RES_ERR;
  if (fflush(myFile))
    return DBLOG_RES_FLUSH_ERR;
  fsync(fileno(myFile));
  return ret;
}

int flush_fn(struct dblog_write_context *ctx) {
  return DBLOG_RES_OK;
}

void listDir(fs::FS &fs, const char * dirname) {
  Serial.print(F("Listing directory: "));
  Serial.println(dirname);
  File root = fs.open(dirname);
  if (!root){
    Serial.println(F("Failed to open directory"));
    return;
  }
  if (!root.isDirectory()){
    Serial.println("Not a directory");
    return;
  }
  File file = root.openNextFile();
  while (file) {
    if (file.isDirectory()) {
      Serial.print(" Dir : ");
      Serial.println(file.name());
    } else {
      Serial.print(" File: ");
      Serial.print(file.name());
      Serial.print(" Size: ");
      Serial.println(file.size());
    }
    file = root.openNextFile();
  }
}

void renameFile(fs::FS &fs, const char *path1, const char *path2) {
  Serial.printf("Renaming file %s to %s\n", path1, path2);
  if (fs.rename(path1, path2)) {
    Serial.println(F("File renamed"));
  } else {
    Serial.println(F("Rename failed"));
  }
}

void deleteFile(fs::FS &fs, const char *path) {
  Serial.printf("Deleting file: %s\n", path);
  if (fs.remove(path)) {
    Serial.println(F("File deleted"));
  } else {
    Serial.println(F("Delete failed"));
  }
}

enum { CHOICE_LOG_ANALOG_DATA = 1, CHOICE_LOCATE_ROWID, CHOICE_LOCATE_BIN_SRCH, CHOICE_RECOVER_DB,
    CHOICE_LIST_FOLDER, CHOICE_RENAME_FILE, CHOICE_DELETE_FILE, CHOICE_SHOW_FREE_MEM};

int askChoice() {
  Serial.println();
  Serial.println(F("Welcome to SQLite Micro Logger console!!"));
  Serial.println(F("----------------------------------------"));
  Serial.println();
  Serial.println(F("1. Log Analog data"));
  Serial.println(F("2. Locate record using RowID"));
  Serial.println(F("3. Locate record using Binary Search"));
  Serial.println(F("4. Recover database"));
  Serial.println(F("5. List folder contents"));
  Serial.println(F("6. Rename file"));
  Serial.println(F("7. Delete file"));
  Serial.println(F("8. Show free memory"));
  Serial.println();
  Serial.print(F("Enter choice: "));
  return input_num();
}

void displayPrompt(const char *title) {
  Serial.println(F("(prefix /spiffs/ or /sd/ or /sdcard/ for"));
  Serial.println(F(" SPIFFS or SD_SPI or SD_MMC respectively)"));
  Serial.print(F("Enter "));
  Serial.println(title);
}

const char *prefixSPIFFS = "/spiffs/";
const char *prefixSD_SPI = "/sd/";
const char *prefixSD_MMC = "/sdcard/";
fs::FS *ascertainFS(const char *str, int *prefix_len) {
  if (strstr(str, prefixSPIFFS) == str) {
    *prefix_len = strlen(prefixSPIFFS) - 1;
    return &SPIFFS;
  }
  if (strstr(str, prefixSD_SPI) == str) {
    *prefix_len = strlen(prefixSD_SPI) - 1;
    return &SD;
  }
  if (strstr(str, prefixSD_MMC) == str) {
    *prefix_len = strlen(prefixSD_MMC) - 1;
    return &SD_MMC;
  }
  return NULL;
}

void print_error(int res) {
  Serial.print(F("Err:"));
  Serial.print(res);
  Serial.print(F("\n"));
}

int input_string(char *str, int max_len) {
  int ctr = 0;
  str[ctr] = 0;
  while (str[ctr] != '\n') {
    if (Serial.available()) {
        str[ctr] = Serial.read();
        if (str[ctr] >= ' ' && str[ctr] <= '~')
          ctr++;
        if (ctr >= max_len)
          break;
    }
  }
  str[ctr] = 0;
  Serial.print(str);
  Serial.print(F("\n"));
  return ctr;
}

int input_num() {
  char in[20];
  int ctr = 0;
  in[ctr] = 0;
  while (in[ctr] != '\n') {
    if (Serial.available()) {
      in[ctr] = Serial.read();
      if (in[ctr] >= '0' && in[ctr] <= '9')
        ctr++;
      if (ctr >= sizeof(in))
        break;
    }
  }
  in[ctr] = 0;
  int ret = atoi(in);
  Serial.print(ret);
  Serial.print(F("\n"));
  return ret;
}

int16_t read_int16(const byte *ptr) {
  return (*ptr << 8) | ptr[1];
}

int32_t read_int32(const byte *ptr) {
  int32_t ret;
  ret  = ((int32_t)*ptr++) << 24;
  ret |= ((int32_t)*ptr++) << 16;
  ret |= ((int32_t)*ptr++) << 8;
  ret |= *ptr;
  return ret;
}

int64_t read_int64(const byte *ptr) {
  int64_t ret;
  ret  = ((int64_t)*ptr++) << 56;
  ret |= ((int64_t)*ptr++) << 48;
  ret |= ((int64_t)*ptr++) << 40;
  ret |= ((int64_t)*ptr++) << 32;
  ret |= ((int64_t)*ptr++) << 24;
  ret |= ((int64_t)*ptr++) << 16;
  ret |= ((int64_t)*ptr++) << 8;
  ret |= *ptr;
  return ret;
}

int pow10(int8_t len) {
  return (len == 3 ? 1000 : (len == 2 ? 100 : (len == 1 ? 10 : 1)));
}

void set_ts_part(char *s, int val, int8_t len) {
  while (len--) {
    *s++ = '0' + val / pow10(len);
    val %= pow10(len);
  }
}

int get_ts_part(char *s, int8_t len) {
  int i = 0;
  while (len--)
    i += ((*s++ - '0') * pow10(len));
  return i;
}

int update_ts_part(char *ptr, int8_t len, int limit, int ovflw) {
  int8_t is_one_based = (limit == 1000 || limit == 60 || limit == 24) ? 0 : 1;
  int part = get_ts_part(ptr, len) + ovflw - is_one_based;
  ovflw = part / limit;
  part %= limit;
  set_ts_part(ptr, part - is_one_based, len);
  return ovflw;
}

// 012345678901234567890
// YYYY-MM-DD HH:MM:SS.SSS
void update_ts(char *ts, int diff) {
  int ovflw = update_ts_part(ts + 20, 3, 1000, diff); // ms
  if (ovflw) {
    ovflw = update_ts_part(ts + 17, 2, 60, ovflw); // seconds
    if (ovflw) {
      ovflw = update_ts_part(ts + 14, 2, 60, ovflw); // minutes
      if (ovflw) {
        ovflw = update_ts_part(ts + 11, 2, 24, ovflw); // hours
        if (ovflw) {
          int8_t month = get_ts_part(ts + 5, 2);
          int year = get_ts_part(ts, 4);
          int8_t limit = (month == 2 ? (year % 4 ? 27 : 28) : 
            (month == 4 || month == 6 || month == 9 || month == 11 ? 29 : 30));
          ovflw = update_ts_part(ts + 8, 2, limit, ovflw); // day
          if (ovflw) {
            ovflw = update_ts_part(ts + 5, 2, 11, ovflw); // month
            if (ovflw)
              set_ts_part(ts, year + ovflw, 4); // year
          }
        }
      }
    }
  }
}

void display_row(struct dblog_read_context *ctx) {
  int i = 0;
  do {
    uint32_t col_type;
    const byte *col_val = (const byte *) dblog_read_col_val(ctx, i, &col_type);
    if (!col_val) {
      if (i == 0)
        Serial.print(F("Error reading value\n"));
      Serial.print(F("\n"));
      return;
    }
    if (i)
      Serial.print(F("|"));
    switch (col_type) {
      case 0:
        Serial.print(F("null"));
        break;
      case 1:
        Serial.print(*((int8_t *)col_val));
        break;
      case 2: {
          int16_t ival = read_int16(col_val);
          Serial.print(ival);
        }
        break;
      case 4: {
        int32_t ival = read_int32(col_val);
        Serial.print(ival);
        break;
      }
      // Arduino Serial.print not capable of printing
      // int64_t and double. Need to implement manually
      case 6: // int64_t
      case 7: // double
        Serial.print(F("todo"));
        break;
      default: {
        uint32_t col_len = dblog_derive_data_len(col_type);
        for (int j = 0; j < col_len; j++) {
          if (col_type % 2)
            Serial.print((char)col_val[j]);
          else {
            Serial.print((int)col_val[j]);
            Serial.print(F(" "));
          }
        }
      }
    }
  } while (++i);
}

void input_db_name() {
  displayPrompt("DB name: "));
  input_string(filename, sizeof(filename));
}

int input_ts(char *datetime) {
  Serial.print(F("\nEnter timestamp (YYYY-MM-DD HH:MM:SS.SSS): "));
  return input_string(datetime, 24);
}

void log_analog_data() {
  int num_entries;
  int dly;
  input_db_name();
  Serial.print(F("\nRecord count: "));
  num_entries = input_num();
  // A0 = 36; A3 = 39; A4 = 32; A5 = 33; A6 = 34; A7 = 35; A10 = 4;
  // A11 = 0; A12 = 2; A13 = 15; A14 = 13; A15 = 12; A16 = 14;
  // A17 = 27; A18 = 25; A19 = 26;
  Serial.print(F("\nStarting analog pin (32): "));
  int8_t analog_pin_start = input_num();
  Serial.print(F("\nNo. of pins (5): "));
  int8_t analog_pin_count = input_num();
  char ts[24];
  if (input_ts(ts) < 23) {
    Serial.print(F("Input full timestamp\n"));
    return;
  }
  Serial.print(F("\nDelay(ms): "));
  dly = input_num();

  myFile = fopen(filename, "w+b");

  // if the file opened okay, write to it:
  if (myFile) {
    unsigned long start = millis();
    unsigned long last_ms = start;
    struct dblog_write_context ctx;
    ctx.buf = buf;
    ctx.col_count = analog_pin_count + 1;
    ctx.page_resv_bytes = 0;
    ctx.page_size_exp = 12;
    ctx.max_pages_exp = 0;
    ctx.read_fn = read_fn_wctx;
    ctx.flush_fn = flush_fn;
    ctx.write_fn = write_fn;
    int res = dblog_write_init(&ctx);
    if (!res) {
      while (num_entries--) {
        res = dblog_set_col_val(&ctx, 0, DBLOG_TYPE_TEXT, ts, 23);
        if (res)
          break;
        update_ts(ts, (int) (millis() - last_ms));
        last_ms = millis();
        for (int8_t i = 0; i < analog_pin_count; i++) {
          int val = analogRead(analog_pin_start + i);
          res = dblog_set_col_val(&ctx, i + 1, DBLOG_TYPE_INT, &val, sizeof(int));
          if (res)
            break;
        }
        if (num_entries) {
          res = dblog_append_empty_row(&ctx);
          if (res)
            break;
          delay(dly);
        }
      }
    }
    Serial.print(F("\nLogging completed. Finalizing...\n"));
    if (!res)
      res = dblog_finalize(&ctx);
    fclose(myFile);
    if (res)
      print_error(res);
    else {
      Serial.print(F("\nDone. Elapsed time (ms): "));
      Serial.print((millis() - start));
      Serial.print("\n");
    }
  } else {
    // if the file didn't open, print an error:
    Serial.print(F("Open Error\n"));
  }
}

void locate_records(int8_t choice) {
  int num_entries;
  int dly;
  input_db_name();
  struct dblog_read_context rctx;
  rctx.page_size_exp = 9;
  rctx.read_fn = read_fn_rctx;
  myFile = fopen(filename, "r+b");
  if (myFile) {
    rctx.buf = buf;
    int res = dblog_read_init(&rctx);
    if (res) {
      print_error(res);
      fclose(myFile);
      return;
    }
    Serial.print(F("Page size:"));
    Serial.print((int32_t) 1 << rctx.page_size_exp);
    Serial.print(F("\nLast data page:"));
    Serial.print(rctx.last_leaf_page);
    Serial.print(F("\n"));
    if (memcmp(buf, sqlite_sig, 16) || buf[68] != 0xA5) {
      Serial.print(F("Invalid DB. Try recovery.\n"));
      fclose(myFile);
      return;
    }
    if (BUF_SIZE < (int32_t) 1 << rctx.page_size_exp) {
      Serial.print(F("Buffer size less than Page size. Try increasing if enough SRAM\n"));
      fclose(myFile);
      return;
    }
    Serial.print(F("\nFirst record:\n"));
    display_row(&rctx);
    uint32_t rowid;
    char srch_datetime[24]; // YYYY-MM-DD HH:MM:SS.SSS
    int8_t dt_len;
    if (choice == 2) {
      Serial.print(F("\nEnter RowID: "));
      rowid = input_num();
    } else
      dt_len = input_ts(srch_datetime);
    Serial.print(F("No. of records to display: "));
    num_entries = input_num();
    unsigned long start = millis();
    if (choice == 2)
      res = dblog_srch_row_by_id(&rctx, rowid);
    else
      res = dblog_bin_srch_row_by_val(&rctx, 0, DBLOG_TYPE_TEXT, srch_datetime, dt_len, 0);
    if (res == DBLOG_RES_NOT_FOUND)
      Serial.print(F("Not Found\n"));
    else if (res == 0) {
      Serial.print(F("\nTime taken (ms): "));
      Serial.print((millis() - start));
      Serial.print("\n\n");
      do {
        display_row(&rctx);
      } while (--num_entries && !dblog_read_next_row(&rctx));
    } else
      print_error(res);
    fclose(myFile);
  } else {
    // if the file didn't open, print an error:
    Serial.print(F("Open Error\n"));
  }
}

void recover_db() {
  struct dblog_write_context ctx;
  ctx.buf = buf;
  ctx.read_fn = read_fn_wctx;
  ctx.write_fn = write_fn;
  ctx.flush_fn = flush_fn;
  input_db_name();
  myFile = fopen(filename, "r+b");
  if (!myFile) {
    print_error(0);
    return;
  }
  int32_t page_size = dblog_read_page_size(&ctx);
  if (page_size < 512) {
    Serial.print(F("Error reading page size\n"));
    fclose(myFile);
    return;
  }
  if (dblog_recover(&ctx)) {
    Serial.print(F("Error during recover\n"));
    fclose(myFile);
    return;
  }
  fclose(myFile);
}

bool is_inited = false;
void setup() {

  Serial.begin(115200);
  while (!Serial) {
  }

  Serial.print(F("InitSD..\n"));
  if (!SPIFFS.begin(FORMAT_SPIFFS_IF_FAILED)) {
    Serial.println(F("Failed to mount file Serial"));
    return;
  }
  SPI.begin();
  SD_MMC.begin();
  SD.begin();

  Serial.print(F("done.\n"));
  is_inited = true;

}

char str[MAX_STR_LEN];
void loop() {

  if (!is_inited)
    return;

  int choice = askChoice();
  switch (choice) {
      case CHOICE_LOG_ANALOG_DATA:
        log_analog_data();
        break;
      case CHOICE_LOCATE_ROWID:
      case CHOICE_LOCATE_BIN_SRCH:
        locate_records(choice);
        break;
      case CHOICE_RECOVER_DB:
        recover_db();
        break;
      case CHOICE_LIST_FOLDER:
      case CHOICE_RENAME_FILE:
      case CHOICE_DELETE_FILE:
        fs::FS *fs;
        displayPrompt("path: ");
        input_string(str, MAX_STR_LEN);
        if (str[0] != 0) {
          int fs_prefix_len = 0;
          fs = ascertainFS(str, &fs_prefix_len);
          if (fs != NULL) {
            switch (choice) {
              case CHOICE_LIST_FOLDER:
                listDir(*fs, str + fs_prefix_len);
                break;
              case CHOICE_RENAME_FILE:
                char str1[MAX_FILE_NAME_LEN];
                displayPrompt("path to rename as: ");
                input_string(str1, MAX_STR_LEN);
                if (str1[0] != 0)
                  renameFile(*fs, str + fs_prefix_len, str1 + fs_prefix_len);
                break;
              case CHOICE_DELETE_FILE:
                deleteFile(*fs, str + fs_prefix_len);
                break;
            }
          }
        }
        break;
      case CHOICE_SHOW_FREE_MEM:
        Serial.printf("\nHeap size: %d\n", ESP.getHeapSize());
        Serial.printf("Free Heap: %d\n", esp_get_free_heap_size());
        Serial.printf("Min Free Heap: %d\n", esp_get_minimum_free_heap_size());
        Serial.printf("Largest Free block: %d\n", heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
        break;
      default:
        Serial.println(F("Invalid choice. Try again."));
  }

}