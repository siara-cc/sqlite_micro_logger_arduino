# sqlite_micro_logger
Lean and Mean Sqlite Data(base) Logger

# Features

- Requires only <em>page size</em> heap and some stack for main logging
- Uses twice the <em>page size</em> heap for "finalizing" the DB
- "Finalize" is optional 
- Can log using Arduino UNO with 512kb page size (2kb RAM)
- Can do binary search on RowID or Timestamp without any index and without finalization
- Recovery possible in case of power failure
- Rolling logs are possible
- Can write to any media or even network filesystem
- DMA writes possible

# Limitations

- Only one table per Sqlite database
- Length of table size limited to (page size - 100) bytes
- Index creation not possible for now
- Index lookup not possible for now
