# swapcomp, pages up to 7 sectors stored compressed: bed, 2026-11-14

Bed `vm_5160_page` (port 9870), `86box_auto`. VxD `SWAPZF7C.VXD` (`-DSC_SEVEN` + checksum), md5
`BE561A2120870C330897917A1AEC198C`, read back from `C:\SWAPCOMP.VXD` after the restart (boot 241).
Three PAGETIME loops `s7c1-3`: exit 0 each, 188,837 / 214,417 / 211,807 ms.

Counters, last `PERFLOG.CSV` row, cumulative: CompressedWrites 15,087, RawWrites 220,
CompressedReads 23,253, RawReads 1,660, SectorsSaved 90,546, Errors 0, Fallback 0, Contention 0,
**ChecksumErrors 0**, ZeroReads 178, ZeroWrites 217.

**PASS**: every compressed page read back matched the checksum taken when it was written.

Reading the row: PERFLOG's header has 40 names, each row 44 values. The header lacks the checksum
build's three later counters (ZeroWrites, ZeroReads, ChecksumErrors) and one more earlier in the row.
PERFLOG lists a VxD's counters in reverse of registration (`SWAPCOMP.ASM` `SwapComp_Device_Init`),
so the three sit just before Contention. Header-index reads of this file are shifted; fix PERFLOG.
