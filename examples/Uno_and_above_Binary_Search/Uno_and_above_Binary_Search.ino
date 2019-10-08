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

#define SD_CS_PIN 8

int32_t read_fn(struct dblog_write_context *ctx, void *buf, uint32_t pos, size_t len) {
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
  Serial.println(str);
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
  Serial.println(ret);
  return ret;
}

bool isInited = false;
void setup() {
  // Open serial communications and wait for port to open:
  Serial.begin(9600);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }

  Serial.print(F("InitSD..\n"));

  if (!SD.begin(SD_CS_PIN)) {
    Serial.print(F("failed!\n"));
    return;
  }
  Serial.print(F("done.\n"));
  isInited = true;

}

byte buf[512];

void loop() {

  if (!isInited)
    return;

  char filename[30];
  int num_entries;
  int dly;
  Serial.print(F("\nSqlite uLogger\n"));
  Serial.print(F("DB name: "));
  input_string(filename, sizeof(filename));
  Serial.print(F("\nRecord count: "));
  num_entries = input_num();
  Serial.print(F("\nDelay(ms): "));
  dly = input_num();

  // open the file. note that only one file can be open at a time,
  // so you have to close this one before opening another.
  myFile = SD.open(filename, FILE_WRITE);

  // if the file opened okay, write to it:
  if (myFile) {
    struct dblog_write_context ctx;
    ctx.buf = buf;
    ctx.col_count = 6;
    ctx.page_size_exp = 9;
    ctx.max_pages_exp = 0;
    ctx.read_fn = read_fn;
    ctx.flush_fn = flush_fn;
    ctx.write_fn = write_fn;
    int res = dblog_write_init(&ctx);
    if (!res) {
      while (num_entries--) {
        for (int i = 0; i < 6; i++) {
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
    if (res) {
      Serial.print(F("Err:"));
      Serial.write(res);
      Serial.print(F("\n"));
    } else
      Serial.print(F("done\n"));

  } else {
    // if the file didn't open, print an error:
    Serial.print(F("IO Err\n"));
  }

}
