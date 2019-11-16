# Sqlite µLogger for Arduino

Sqlite µLogger is a Fast and Lean database logger that can log data into Sqlite databases even with SRAM as low as 2kb as in an Arduino Uno. The source code can be ported to use with any Microcontroller having at least 2kb RAM.

This repo is an Arduino library that can work with Arduino Uno board or any Arduino board that has minimum 2kb RAM and a SD Shield attached.

It has been tested with Arduino Uno with SparkFun MicroSD Shield, WeMos ESP8266 D1 Mini with WeMos MicroSD Shield and ESP32 SD_MMC breakout board.

![](banner.png?raw=true)

# Features

- Low Memory requirement: `page_size` + some stack
- Can log using Arduino UNO (`2kb` RAM) with 512 bytes page size
- Can do quick binary search on RowID or Timestamp without any index in logarithmic time
- Recovery possible in case of power failure
- Rolling logs are possible (not implemented yet)
- Can use any media using any IO library/API or even network filesystem
- DMA writes possible (not shown)
- Virtually any board and any media can be used as IO is done through callback functions.

# Getting started

The example `Uno_and_above` shows how data read from Analog pins can be stored along with Timestamp into Sqlite database and retrieved by RowId.

Records can also be located using Timestamp in logarithmic time by doing a Binary Search on the data logged.  This is not possible using conventional loggers.

For example, locating any record in a 70 MB db having 1 million records on Arduino UNO with SparkFun microSD Shield took only 1.6 seconds.

The examples `ESP8266_Console` and `ESP32_Console` can be used to log and retrieve from ESP8266 and ESP32 boards respectively on Micro SD and SPIFFS filesystems.

# API

For finding out how the logger works and a complete description of API visit [Sqlite Micro Logger C Library](https://github.com/siara-cc/sqlite_micro_logger_c).

# Ensuring integrity

If there is power failure during logging, the data can be recovered using `Recover database` option in the menu.

# Limitations

Following are limitations of this library:

- Only one table per Sqlite database
- Length of table script limited to (`page size` - 100) bytes
- `Select`, `Insert` are not supported.  Instead C API similar to that of Sqlite API is available.
- Index creation and lookup not possible (as of now)

However, the database created can be copied to a desktop PC and further operations such as index creation and summarization can be carried out from there as though its a regular Sqlite database.  But after doing so, it may not be possible to use it with this library any longer.

# Future plans

- Index creation when finalizing a database
- Allow modification of records
- Rolling logs
- Show how this library can be used in a multi-core, multi-threaded environment

# Support

If you find any issues, please create an issue here or contact the author (Arundale Ramanathan) at arun@siara.cc.
