/*
  This example demonstrates how the Sqlite Micro Logger library
  can be used to write Analog data into Sqlite database.
  Works on any Arduino compatible microcontroller with SD Card attachment
  having 2kb RAM or more (such as Arduino Uno).

  How Sqlite Micro Logger works:
  https://github.com/siara-in/sqlite_micro_logger

  Copyright 2019 Arundale Ramanathan, Siara Logics (in)

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
#include <SD.h>

File myFile;

// Set the CS Pin connected to the MicroSD
// 8 on most shields such as Sparkfun Micro SD Shield, WeMos D1 Mini
#define SD_CS_PIN 8

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
  return ret;
}

int flush_fn(struct dblog_write_context *ctx) {
  myFile.flush();
  return DBLOG_RES_OK;
}

void printError(int res) {
  Serial.print(F("Err:"));
  Serial.write(res);
  Serial.print(F("\n"));
}

int input_string(char *str, int max_len) {
  max_len--;
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
  return (len == 3 ? 1000 : (len == 2 ? 100 : (len == 1 ? 10 : 0)));
}

void set_part(char *s, int val, uint8_t len) {
  while (len--) {
    *s++ = '0' + val / pow10(len);
    val %= pow10(len);
  }
}

int get_part(char *s, uint8_t len) {
  int i = 0;
  while (len--)
    i += ((*s++ - '0') * pow10(len));
  return i;
}

int update_ts_part(char *ptr, int8_t len, int limit, int ovflw) {
  int8_t is_one_based = (limit == 1000 || limit == 60) || limit == 24 ? 0 : 1;
  int part = get_part(ptr, len) + ovflw - is_one_based;
  ovflw = part / limit;
  part %= limit;
  set_part(ptr, part - is_one_based, len);
  return ovflw;
}

// 012345678901234567890
// YYYY-MM-DD HH:MM:SS.SSS
void updateTimestamp(char *ts, int diff) {
  int ovflw = update_ts_part(ts + 20, 3, 1000, diff); // ms
  if (ovflw) {
    ovflw = update_ts_part(ts + 17, 2, 60, ovflw); // seconds
    if (ovflw) {
      ovflw = update_ts_part(ts + 14, 2, 60, ovflw); // minutes
      if (ovflw) {
        ovflw = update_ts_part(ts + 11, 2, 24, ovflw); // hours
        if (ovflw) {
          int8_t month = get_part(ts + 5, 2);
          int year = get_part(ts, 4);
          int8_t limit = (month == 2 ? (year % 4 ? 27 : 28) : 
            (month == 4 || month == 6 || month == 9 || month == 11 ? 29 : 30));
          ovflw = update_ts_part(ts + 8, 2, limit, ovflw); // day
          if (ovflw) {
            ovflw = update_ts_part(ts + 5, 2, 11, ovflw); // month
            if (ovflw)
              set_part(ts, year + ovflw, 4);
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
        Serial.print(F("TODO"));
        break;
      default: {
        uint32_t col_len = dblog_derive_data_len(col_type);
        for (int j = 0; j < col_len; j++) {
          if (col_type % 2)
            Serial.print(col_val[j]);
          else {
            Serial.print((int)col_val[j]);
            Serial.print(F(" "));
          }
        }
      }
    }
  } while (++i);
}

bool isInited = false;
void setup() {
  // Open serial communications and wait for port to open:
  Serial.begin(9600);
  while (!Serial) {
  }

  Serial.print(F("InitSD..\n"));

  if (!SD.begin(SD_CS_PIN)) {
    Serial.print(F("failed!\n"));
    return;
  }

  Serial.print(F("done.\n"));
  isInited = true;

}

// If you would like to read DBs created with this repo
// with page size more than 512, change here.
#define BUF_SIZE 512
byte buf[BUF_SIZE];
char filename[13];

void inputDBName() {
  Serial.print(F("DB name (max 8.3): "));
  input_string(filename, sizeof(filename));
}

int input_ts(char *datetime) {
  Serial.print(F("\nEnter timestamp (YYYY-MM-DD HH:MM:SS.SSS): "));
  return input_string(datetime, 23);
}

extern const char sqlite_sig[];
void loop() {

  if (!isInited)
    return;

  int num_entries;
  int dly;
  Serial.print(F("\n\nSqlite uLogger\n\n"));
  Serial.print(F("1. Log analog data\n"));
  Serial.print(F("2. Read records using RowID\n"));
  Serial.print(F("3. Recover DB\n\n"));
  Serial.print(F("Enter choice: "));
  int8_t choice = input_num();

  switch (choice) {
    case 1: {
      inputDBName();
      Serial.print(F("\nRecord count (1 to 32767 on UNO): "));
      num_entries = input_num();
      Serial.print(F("\nNumber of analog pins (from A0): "));
      int8_t analog_pin_count = input_num();
      Serial.print(F("\nCurrent time (YYYY-MM-DD HH:MM:SS.000): "));
      char ts[24];
      if (input_ts(ts) < 23) {
        Serial.print(F("Input full timestamp\n"));
        return;
      }
      Serial.print(F("\nDelay(ms): "));
      dly = input_num();

      // open the file. note that only one file can be open at a time,
      // so you have to close this one before opening another.
      SD.remove(filename);
      myFile = SD.open(filename, FILE_WRITE);

      // if the file opened okay, write to it:
      if (myFile) {
        unsigned long start = millis();
        unsigned long last_ms = start;
        struct dblog_write_context ctx;
        ctx.buf = buf;
        ctx.col_count = analog_pin_count + 1;
        ctx.page_resv_bytes = 0;
        ctx.page_size_exp = 9;
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
            updateTimestamp(ts, millis() - last_ms);
            last_ms = millis();
            for (int8_t i = 0; i < analog_pin_count; i++) {
              int val = analogRead(A0 + i);
              res = dblog_set_col_val(&ctx, i, DBLOG_TYPE_INT, &val, sizeof(int));
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
        if (!res)
          res = dblog_finalize(&ctx);
        myFile.close();
        if (res)
          printError(res);
        else {
          Serial.print(F("\nDone. Elapsed time (ms): "));
          Serial.print((millis() - start));
          Serial.print("\n");
        }
      } else {
        // if the file didn't open, print an error:
        Serial.print(F("Open Error\n"));
      }
      break;
    }
    case 2:   // search by row id
    case 3: { // binary search
      inputDBName();
      struct dblog_read_context rctx;
      rctx.page_size_exp = 9;
      rctx.read_fn = read_fn_rctx;
      myFile = SD.open(filename, FILE_READ);
      if (myFile) {
        Serial.print(F("Size:"));
        Serial.print(myFile.size());
        Serial.print(F("\n"));
        rctx.buf = buf;
        int res = dblog_read_init(&rctx);
        if (res) {
          printError(res);
          myFile.close();
          break;
        }
        Serial.print(F("Page size:"));
        Serial.print((int32_t) 1 << rctx.page_size_exp);
        Serial.print(F("\nLast data page:"));
        Serial.print(rctx.last_leaf_page);
        Serial.print(F("\n"));
        if (memcmp(buf, sqlite_sig, 16) || buf[68] != 0xA5) {
          Serial.print(F("Invalid DB. Try recovery.\n"));
          myFile.close();
          break;
        }
        if (BUF_SIZE < (int32_t) 1 << rctx.page_size_exp) {
          Serial.print(F("Buffer size less than Page size. Try increasing if enough SRAM\n"));
          myFile.close();
          break;
        }
        Serial.print(F("\nFirst record:\n"));
        display_row(&rctx);
        uint32_t rowid;
        char srch_datetime[24]; // YYYY-MM-DD HH:MM:SS.SSS
        int8_t dt_len;
        if (choice == 2) {
          Serial.print(F("\nEnter RowID (1 to 32767 on UNO): "));
          uint32_t rowid = input_num();
        } else
          dt_len = input_ts(srch_datetime);
        Serial.print(F("No. of records to display: "));
        num_entries = input_num();
        unsigned long start = millis();
        if ((choice == 2 ? dblog_srch_row_by_id(&rctx, rowid)
             : dblog_bin_srch_row_by_val(&rctx, 0, DBLOG_TYPE_TEXT, srch_datetime, dt_len, 0))) {
            Serial.print(F("Not Found\n"));
        } else {
          Serial.print(F("\nTime taken (ms): "));
          Serial.print((millis() - start));
          Serial.print("\n\n");
          do {
            display_row(&rctx);
          } while (--num_entries && !dblog_read_next_row(&rctx));
        }
        myFile.close();
      } else {
        // if the file didn't open, print an error:
        Serial.print(F("Open Error\n"));
      }
      break;
    }
  }

}