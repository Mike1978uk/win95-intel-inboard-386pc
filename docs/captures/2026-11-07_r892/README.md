# 2026-11-07 r892 against noag, agent-free NOAGDISK, real 5160
`noag`: M8D3D8 (GMEM_FIXED), run 2026-11-06 morning. `r892`: M8D3DD (M8DISP.DRV md5 4263513518f4ed7af787850622650cae, M8DD32.DLL md5 69ff48e5f3946a4bcda63e5408ddab1c), pageable buffers, 892 KB less locked RAM. One run each unless stated, so differences are one run, not a trend. Change = r892 against noag.

## 1. PAGETIME (labels `noag-S28-121800`, `r892-S28-172722`; units as logged)
| Program | open noag | open r892 | % | sw1 noag | sw1 r892 | % | sw2 noag | sw2 r892 | % | sw3 noag | sw3 r892 | % |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| PSP | 15716 | 15171 | -3.5% | 4514 | 5050 | +11.9% | 4724 | 4212 | -10.8% | 3892 | 4156 | +6.8% |
| WordPad | 21053 | 22513 | +6.9% | 3550 | 5633 | +58.7% | 3472 | 3505 | +1.0% | 3596 | 3176 | -11.7% |
| Notepad | 8646 | 7294 | -15.6% | 1755 | 1622 | -7.6% | 1411 | 1905 | +35.0% | 1412 | 1645 | +16.5% |
| Paint | 19207 | 17719 | -7.7% | 2864 | 3060 | +6.8% | 2932 | 3040 | +3.7% | 2859 | 3177 | +11.1% |
| total | 64622 | 62697 | -3.0% | 12683 | 15365 | +21.1% | 12539 | 12662 | +1.0% | 11759 | 12154 | +3.4% |

## 2. PERFLOG (labels `noag-S28-121740`, `r892-S28-172704`)
Counters are cumulative since boot: end = last row, delta = last minus first row. Both runs have 151 rows at a 2 s period (first row 2.0 s, last 298 s), so the period is the same. These labels differ from the PAGETIME ones.
| Counter | noag end | r892 end | % | noag delta | r892 delta | % |
|---|---|---|---|---|---|---|
| VMM cPageIns | 13292 | 13967 | +5.1% | 10619 | 10776 | +1.5% |
| VMM cPageOuts | 4007 | 6646 | +65.9% | 2815 | 4889 | +73.7% |
| VMM cDiscards | 8857 | 7002 | -20.9% | 7666 | 5851 | -23.7% |
| VMM cPageFaults | 28538 | 32461 | +13.7% | 24161 | 27716 | +14.7% |
| SWAPCOMP RawReads | 2758 | 1916 | -30.5% | 2324 | 1610 | -30.7% |
| SWAPCOMP CompressedReads | 5517 | 7727 | +40.1% | 4840 | 6298 | +30.1% |
| SWAPCOMP RawWrites | 592 | 520 | -12.2% | 223 | 197 | -11.7% |
| SWAPCOMP CompressedWrites | 3415 | 6126 | +79.4% | 2592 | 4692 | +81.0% |
| SWAPCOMP SectorsSaved | 17341 | 37128 | +114.1% | 13198 | 28482 | +115.8% |
| SWAPCOMP Contention | 946 | 2204 | +133.0% | 918 | 2141 | +133.2% |
| SWAPCOMP Errors | 0 | 0 | n/a | 0 | 0 | n/a |
| SWAPCOMP Fallback | 0 | 0 | n/a | 0 | 0 | n/a |

PERFLOG data rows have 41 fields under a 40-column header: one unnamed field sits in the SHADRAM block. The SWAPCOMP columns above are counted from the end of the row, which also gives the 918 recorded for `noag` on 11-06. Read by header position they shift by one.
| VMM cpgLocked (level, bytes) | 2953216 | 2441216 | -17.3% | first row 2920448 | first row 1896448 | -35.1% |

## 3. NOAG.TXT `paged` KB, NOAGDISK block (first block noag, last block r892)
21 points, same order in both, each at the start of the named step (`L` = run label; `bg` = started not waited for; last is finish). Three points per table row.
| Point | noag | r892 | % | Point | noag | r892 | % | Point | noag | r892 | % |
|---|---|---|---|---|---|---|---|---|---|---|---|
| bg PERFLOG.EXE L 300 | 5120 | 5096 | -0.5% | start SLEEP.EXE 10 | 5216 | 5344 | +2.5% | start PAGETIME.EXE L | 6232 | 6164 | -1.1% |
| start WINCLOSE.EXE WAIT PERFLOG | 8844 | 8944 | +1.1% | start M8IDLE.EXE 10 10 600 | 6488 | 6600 | +1.7% | start XTSECT.EXE L | 6632 | 7260 | +9.5% |
| start XTSECT.EXE L | 6068 | 5840 | -3.8% | start XTSECT.EXE L | 5924 | 6148 | +3.8% | start IDLECOST.EXE L | 5968 | 6192 | +3.8% |
| start IDLECOST.EXE L | 5724 | 5668 | -1.0% | start IDLECOST.EXE L | 5736 | 5808 | +1.3% | start LZ4B12.EXE 80 | 5660 | 5812 | +2.7% |
| start LZ4B12I.EXE 80 | 5744 | 5752 | +0.1% | start LZ4B12F.EXE 80 | 5556 | 5656 | +1.8% | start LZ4B12N.EXE 80 | 5556 | 5760 | +3.7% |
| start LZ4B12IN.EXE 80 | 5560 | 5508 | -0.9% | start LZ4B12FN.EXE 80 | 5480 | 5476 | -0.1% | start LZ4B11.EXE 80 | 5564 | 5680 | +2.1% |
| start LZ4B10.EXE 80 | 5368 | 5580 | +3.9% | start LZ4B10FN.EXE 80 | 5572 | 5508 | -1.1% | finished NOAGDISK | 5576 | 5592 | +0.3% |

## 4. XTSECT (3 runs each, 3 passes per run, 500 reads)
Each cell is the minimum over the three runs of that column. Columns: mean us/read, min, max, us/sector. Max is a worst-case spike.
| Pass | Sectors | mean noag | r892 | % | min noag | r892 | % | max noag | r892 | % | us/sec noag | r892 | % |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 1 | 1 | 11534 | 11377 | -1.4% | 9979 | 9987 | +0.1% | 99616 | 57327 | -42.5% | 11534 | 11377 | -1.4% |
| 1 | 8 | 20729 | 20867 | +0.7% | 10761 | 10788 | +0.3% | 30207 | 33902 | +12.2% | 2591 | 2608 | +0.7% |
| 2 | 1 | 11269 | 11356 | +0.8% | 9957 | 9969 | +0.1% | 30409 | 38795 | +27.6% | 11269 | 11356 | +0.8% |
| 2 | 8 | 20762 | 20731 | -0.1% | 10765 | 10774 | +0.1% | 47188 | 47002 | -0.4% | 2595 | 2591 | -0.2% |
| 3 | 1 | 11251 | 11252 | +0.0% | 9961 | 9970 | +0.1% | 30223 | 21863 | -27.7% | 11251 | 11252 | +0.0% |
| 3 | 8 | 20700 | 20733 | +0.2% | 10783 | 10789 | +0.1% | 33409 | 40439 | +21.0% | 2587 | 2591 | +0.2% |

## 5. IDLECOST (3 runs each)
Best of three per column: highest for the passes/s columns, lowest for ns/line. `mean`, `min/s`, `max/s` are the logged passes/s columns.
| KB | mean noag | r892 | % | min/s noag | r892 | % | max/s noag | r892 | % | ns/line noag | r892 | % |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 1 | 33801 | 33740 | -0.2% | 32080 | 32256 | +0.5% | 34448 | 34416 | -0.1% | 462 | 463 | +0.2% |
| 4 | 10649 | 10700 | +0.5% | 10496 | 10624 | +1.2% | 10768 | 10768 | +0.0% | 366 | 365 | -0.3% |
| 8 | 5561 | 5564 | +0.1% | 5536 | 5536 | +0.0% | 5632 | 5600 | -0.6% | 351 | 351 | +0.0% |
| 12 | 3744 | 3760 | +0.4% | 3680 | 3744 | +1.7% | 3792 | 3792 | +0.0% | 347 | 346 | -0.3% |

## 6. LZ4B variants (64 pages, plain LZ4)
The r892 files carry no run label and no time of day. `NOAG.TXT` shows all nine ran in the r892 NOAGDISK block, and the values differ from the morning copies in `docs/captures/2026-11-06_noagent/`, but the logs cannot prove which run wrote them. One run per variant.
| Variant | r892 ratio | r892 comp us | r892 decomp us | morning ratio | morning comp us | morning decomp us | comp % | decomp % |
|---|---|---|---|---|---|---|---|---|
| 10 | 2.24 | 4849.42 | 1106.36 | 2.24 | 4872.58 | 1134.34 | -0.5% | -2.5% |
| 10FN | 2.24 | 4238.55 | 1102.23 | 2.24 | 4228.84 | 1122.02 | +0.2% | -1.8% |
| 11 | 2.25 | 4791.28 | 1110.48 | 2.25 | 4783.92 | 1159.8 | +0.2% | -4.3% |
| 12 | 2.26 | 5101.83 | 1144.2 | 2.26 | 5142.27 | 1113.16 | -0.8% | +2.8% |
| 12F | 2.25 | 4697.08 | 1141.0 | 2.25 | 4737.09 | 1106.41 | -0.8% | +3.1% |
| 12FN | 2.25 | 4296.48 | 1106.56 | 2.25 | 4260.28 | 1105.0 | +0.8% | +0.1% |
| 12I | 2.26 | 5193.27 | 1141.06 | 2.26 | 5176.97 | 1124.39 | +0.3% | +1.5% |
| 12IN | 2.26 | 4787.06 | 1124.75 | 2.26 | 4809.36 | 1141.3 | -0.5% | -1.5% |
| 12N | 2.26 | 4812.67 | 1114.23 | 2.26 | 4822.45 | 1145.47 | -0.2% | -2.7% |

## Swap file, card's WIN386.SWP, read on the host after this run
`tools/swapcomp/sectors.py`, 8,192 slots: 5,082 never written, 2,558 compressed, 552 raw. 82.3 % of used slots decoded. Compressed slots by sectors on disk: 1: 1,363; 2: 258; 3: 323; 4: 345; 5: 157; 6: 112. 775 compressed slots hold one repeated dword, 589 of them all zero. Slots are counted once however often written, and may be stale from earlier boots, so these are not per-I/O rates.

Model per page-in, sample 400 (deflate and lzma CPU are estimates): store 21.56 ms, LZ4 11.36, LZ4-HC 12.52, sector-aware LZ4 12.62, deflate-1 15.65, deflate-9 26.66, lzma 106.42. LZ4 stays the best of these on the model.

Raw slots, had they been compressed with this script's LZ4: 404 over the 6-sector limit, 148 under it (1 sector 49, 2: 25, 3: 11, 4: 10, 5: 21, 6: 32). Whether those 148 were written by SWAPCOMP this boot or are stale is not known.

## Zero pages and the cheaper hash in SWAPCOMP (bed, `SWAPZFC.VXD`, checksum build)

`vm_5160_mcp`, `M8D3DH`, three PAGETIME loops, VxD md5 verified after the restart. Reads back through checksums: 25,016 compressed and raw, 182 zero. **ChecksumErrors 0, Errors 0, Fallback 0.** Bed: no speed from this.

| Counter | Value |
|---|---|
| ZeroWrites | 240 of 15,802 page writes (1.5 %) |
| ZeroReads | 182 of 25,198 page reads (0.7 %) |
| CompressedReads, RawReads | 20,185, 4,831 |
| CompressedWrites, RawWrites | 14,788, 774 |

**Zero pages: under 1 % of paging, not the 25 % `sectors.py` suggested:** that counts each slot once, and zero slots are written once and seldom read. Same 1.5 % as the earlier write counter. PERFLOG's rows have more fields than the CSV's old header: read from the end of the row.

Default `SWAPCOMP.VXD` still builds to 9FDE4934, the card's. `SWAPZF` is 5.6 KB smaller and is the card's next test: `LZ4B10FN`'s 17 % cheaper compress, at ~0.86 ms a page written, is ~4 s of `r892`'s 4,692 writes.
