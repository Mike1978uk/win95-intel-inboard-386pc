# swapcomp, pages up to 7 sectors stored compressed: bed, 2026-11-14

Bed `vm_5160_page` (port 9870), `86box_auto`. VxD `SWAPZF7C.VXD` (`-DSC_SEVEN` + checksum), md5
`BE561A2120870C330897917A1AEC198C`, read back from `C:\SWAPCOMP.VXD` after the restart (boot 241).
Three PAGETIME loops `s7c1-3`: exit 0 each, 188,837 / 214,417 / 211,807 ms.

Counters, last `PERFLOG.CSV` row, cumulative: CompressedWrites 15,087, RawWrites 220,
CompressedReads 23,253, RawReads 1,660, SectorsSaved 90,546, Errors 0, Fallback 0, Contention 0,
**ChecksumErrors 0**, ZeroReads 178, ZeroWrites 217.

**PASS**: every compressed page read back matched the checksum taken when it was written.

Reading the row: PERFLOG appends, and each run writes its own header; the file's first header
belongs to an earlier, non-checksum run (corrected 2026-11-15). Read each row against the header line
before it: `tools/perflog/perflog_table.py` does.
