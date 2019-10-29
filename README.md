# Sqlite µLogger

Lean and Mean Sqlite Data(base) Logger

## This library hosts both a C library and an Arduino library.

# Features

- Memory requirement: `page_size` + some stack
- "Finalize" is optional 
- Can log using Arduino UNO (2kb RAM) with 512kb page size
- Can do binary search on RowID or Timestamp without any index
- Recovery possible in case of power failure
- Rolling logs are possible (not implemented yet)
- Can use any media using any IO library/API or even network filesystem
- DMA writes possible

# Ensuring integrity

During finalize:
- If Sqlite format 3 and checksum matches, then all ok
- If header checksum does not match, re-build header from leaf pages
- If leaf page checksum does not match, discard it (optional?)

# Limitations

- Only one table per Sqlite database
- Length of table script limited to (`page size` - 100) bytes
- Index creation and lookup not possible (as of now)

