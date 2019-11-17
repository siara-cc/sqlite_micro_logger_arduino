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
#include <SD.h>

#define MAX_FILE_NAME_LEN 100
#define MAX_STR_LEN 500

// If you would like to read DBs created with this repo
// with page size more than 4096, change here,
// but ensure as much SRAM can be allocated.
#define BUF_SIZE 4096
byte buf[BUF_SIZE];
char filename[MAX_FILE_NAME_LEN];
extern const char sqlite_sig[];

File myFile;

int32_t read_fn_wctx(struct dblog_write_context *ctx, void *buf, uint32_t pos, size_t len) {
  myFile.seek(pos);
  size_t ret = myFile.read((byte *)buf, len);
  if (ret != len)
    return DBLOG_RES_READ_ERR;
  return ret;
}

int32_t read_fn_rctx(struct dblog_read_context *ctx, void *buf, uint32_t pos, size_t len) {
  myFile.seek(pos);
  size_t ret = myFile.read((byte *)buf, len);
  if (ret != len)
    return DBLOG_RES_READ_ERR;
  return ret;
}

int32_t write_fn(struct dblog_write_context *ctx, void *buf, uint32_t pos, size_t len) {
  myFile.seek(pos);
  size_t ret = myFile.write((byte *)buf, len);
  if (ret != len)
    return DBLOG_RES_ERR;
  myFile.flush();
  return ret;
}

int flush_fn(struct dblog_write_context *ctx) {
  //myFile.flush(); // Anyway being flushed after write
  return DBLOG_RES_OK;
}

void listDir(int fs_type, const char * dirname) {
  Serial.print(F("Listing directory: "));
  Serial.println(dirname);
  if (fs_type == 1) {
    Dir dir = SPIFFS.openDir("/");
    while (dir.next()) {
        Serial.print(dir.fileName());
        File f = dir.openFile("r");
        Serial.print("\t");
        Serial.println(f.size());
    }
    return;
  }
  File root = SD.open("/");
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

void renameFile(int fs_type, const char *path1, const char *path2) {
  Serial.printf("Renaming file %s to %s\n", path1, path2);
  int res = 0;
  if (fs_type == 1)
    res = SPIFFS.rename(path1, path2);
  if (res) {
    Serial.println(F("File renamed"));
  } else {
    Serial.println(F("Rename failed"));
  }
}

void deleteFile(int fs_type, const char *path) {
  Serial.printf("Deleting file: %s\n", path);
  int res = 0;
  if (fs_type == 1)
    res = SPIFFS.remove(path);
  if (res) {
    Serial.println(F("File deleted"));
  } else {
    Serial.println(F("Delete failed"));
  }
}

enum { CHOICE_LOG_ANALOG_DATA = 1, CHOICE_LOCATE_ROWID, CHOICE_LOCATE_BIN_SRCH, CHOICE_RECOVER_DB,
    CHOICE_LIST_FOLDER, CHOICE_SHOW_FREE_MEM, CHOICE_RENAME_FILE, CHOICE_DELETE_FILE};

int askChoice() {
  Serial.println();
  Serial.println(F("Welcome to SQLite Micro Logger console!!"));
  Serial.println(F("----------------------------------------"));
  Serial.println();
  Serial.println(F("1. Log Analog data"));
  Serial.println(F("2. Locate record using RowID"));
  Serial.println(F("3. Locate record using Binary Search"));
  Serial.println(F("4. Recover database"));
  Serial.println(F("5. List folder contents (SPIFFS only)"));
  Serial.println(F("6. Show free memory"));
  Serial.println(F("7. Rename file (SPIFFS only)"));
  Serial.println(F("8. Delete file (SPIFFS only)"));
  Serial.println();
  Serial.print(F("Enter choice: "));
  return input_num();
}

void displayPrompt(const char *title) {
  Serial.println(F("(prefix /spiffs/ or /sd/ for"));
  Serial.println(F(" SPIFFS or SD_SPI respectively)"));
  Serial.print(F("Enter "));
  Serial.println(title);
}

const char *prefixSPIFFS = "/spiffs/";
const char *prefixSD_SPI = "/sd/";
int ascertainFS(const char *str, int *prefix_len) {
  if (strstr(str, prefixSPIFFS) == str) {
    *prefix_len = strlen(prefixSPIFFS) - 1;
    return 1;
  }
  if (strstr(str, prefixSD_SPI) == str) {
    *prefix_len = strlen(prefixSD_SPI) - 1;
    return 2;
  }
  return 0;
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
  displayPrompt("DB name: ");
  input_string(filename, MAX_FILE_NAME_LEN);
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
  char ts[24];
  if (input_ts(ts) < 23) {
    Serial.print(F("Input full timestamp\n"));
    return;
  }
  Serial.print(F("\nDelay(ms): "));
  dly = input_num();

  if (strstr(filename, prefixSPIFFS) == filename)
    myFile = SPIFFS.open(filename + 7, "w+b");
  else
    myFile = SD.open(filename + 3, FILE_WRITE);

  // if the file opened okay, write to it:
  if (myFile) {
    unsigned long start = millis();
    unsigned long last_ms = start;
    struct dblog_write_context ctx;
    ctx.buf = buf;
    ctx.col_count = 2;
    ctx.page_resv_bytes = 0;
    ctx.page_size_exp = 12;
    ctx.max_pages_exp = 0;
    ctx.read_fn = read_fn_wctx;
    ctx.flush_fn = flush_fn;
    ctx.write_fn = write_fn;
    int val;
    uint8_t types[] = {DBLOG_TYPE_TEXT, DBLOG_TYPE_INT};
    void *values[] = {ts, &val};
    uint16_t lengths[] = {23, sizeof(val)};
    int res = dblog_write_init(&ctx);
    if (!res) {
      while (num_entries--) {
        val = analogRead(A0);
        res = dblog_append_row_with_values(&ctx, types, (const void **) values, lengths);
        if (res)
          break;
        update_ts(ts, (int) (millis() - last_ms));
        last_ms = millis();
        delay(dly);
      }
    }
    Serial.print(F("\nLogging completed. Finalizing...\n"));
    if (!res)
      res = dblog_finalize(&ctx);
    myFile.close();
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
  if (strstr(filename, prefixSPIFFS) == filename)
    myFile = SPIFFS.open(filename + 7, "r+b");
  else
    myFile = SD.open(filename + 3, FILE_READ);
  if (myFile) {
    rctx.buf = buf;
    int res = dblog_read_init(&rctx);
    if (res) {
      print_error(res);
      myFile.close();
      return;
    }
    Serial.print(F("Page size:"));
    Serial.print((int32_t) 1 << rctx.page_size_exp);
    Serial.print(F("\nLast data page:"));
    Serial.print(rctx.last_leaf_page);
    Serial.print(F("\n"));
    if (memcmp(buf, sqlite_sig, 16) || buf[68] != 0xA5) {
      Serial.print(F("Invalid DB. Try recovery.\n"));
      myFile.close();
      return;
    }
    if (BUF_SIZE < (int32_t) 1 << rctx.page_size_exp) {
      Serial.print(F("Buffer size less than Page size. Try increasing if enough SRAM\n"));
      myFile.close();
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
    myFile.close();
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
  if (strstr(filename, prefixSPIFFS) == filename)
    myFile = SPIFFS.open(filename + 7, "r+b");
  else
    myFile = SD.open(filename + 3, FILE_READ);
  if (!myFile) {
    print_error(0);
    return;
  }
  int32_t page_size = dblog_read_page_size(&ctx);
  if (page_size < 512) {
    Serial.print(F("Error reading page size\n"));
    myFile.close();
    return;
  }
  if (dblog_recover(&ctx)) {
    Serial.print(F("Error during recover\n"));
    myFile.close();
    return;
  }
  myFile.close();
}

bool is_inited = false;
void setup() {

  Serial.begin(74880);
  while (!Serial) {
  }

  Serial.print(F("InitSD..\n"));
  SPIFFS.begin();
  SPI.begin();
  if (!SD.begin(4)) {
    Serial.print(F("Failed to mount SD FS\n"));
    return;
  }

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
        displayPrompt("path: ");
        input_string(str, MAX_STR_LEN);
        if (str[0] != 0) {
          int fs_prefix_len = 0;
          int fs_type = ascertainFS(str, &fs_prefix_len);
          if (fs_type != 0) {
            switch (choice) {
              case CHOICE_LIST_FOLDER:
                listDir(fs_type, str + fs_prefix_len);
                break;
              case CHOICE_RENAME_FILE:
                char str1[MAX_FILE_NAME_LEN];
                displayPrompt("path to rename as: ");
                input_string(str1, MAX_STR_LEN);
                if (str1[0] != 0)
                  renameFile(fs_type, str + fs_prefix_len, str1 + fs_prefix_len);
                break;
              case CHOICE_DELETE_FILE:
                deleteFile(fs_type, str + fs_prefix_len);
                break;
            }
          }
        }
        break;
      case CHOICE_SHOW_FREE_MEM:
        //Serial.printf("\nHeap size: %d\n", ESP.getHeapSize());
        Serial.printf("Free Heap: %d\n", ESP.getFreeHeap());
        //Serial.printf("Min Free Heap: %d\n", ESP.getMinFreeBlockSize());
        Serial.printf("Fragmentation: %d\n", ESP.getHeapFragmentation());
        Serial.printf("Max Free Heap: %d\n", ESP.getMaxFreeBlockSize());
        break;
      default:
        Serial.println(F("Invalid choice. Try again."));
  }

}