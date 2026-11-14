# Benchmark choice, 2026-11-14

Research only. Nothing downloaded or run. Search results were thin:
"unverified" means no source found, not that the claim is false.
Checked first-hand from a page: nothing; all facts below are from search
snippets. Fetch of vogons and winworldpc pages returned 403/400.

## Candidates

| Tool | Area | Needs | Comparable results | Source |
|---|---|---|---|---|
| CORETEST (Core Intl, 2.x/3.x) | disk, DOS | DOS; relative index vs IBM 10 MB XT | none found; docs only | [doc](https://discmaster.textfiles.com/view/761/HallofFameCDROM.cdr/open/core28.lzh/CORETEST.DOC), [1988 usenet](https://www.tuhs.org/Usenet/comp.unix.xenix/1988-May/001160.html), [CORE303.EXE note](https://classiccmp.org/mailman3/hyperkitty/list/test-drb@ccmp.vtda.org/message/OGFN5KPDOXL26QZ46QBQSGHVAGDV26C7) |
| SpeedSys 4.78 | CPU/cache/disk, DOS | 386 or higher; cache state swings score | [386 thread](https://www.vogons.org/viewtopic.php?p=599733) (title only seen) | [vogons DOS benchmark archive](https://www.vogons.org/viewtopic.php?p=355951) (listed, link not verified) |
| 3DBench 1.0c | CPU+video, DOS | 486 or slower: 3DBENCH.BAT | [vogons database thread](https://www.vogons.org/viewtopic.php?p=199606): 386SX/16 5.0, 386DX/33 13.3, 386/40 15.8-17.4, DX2-66 43.5. 1.0c unreliable on slow machines | same thread |
| Doom 1.9 `-timedemo demo3` | video+CPU, DOS | 386+, 4 MB | [thandor table](https://thandor.net/benchmark/32) (units unclear), [Sanglard DX2-66: 29 fps](https://www.fabiensanglard.net/2168/doom), [vogons](https://www.vogons.org/viewtopic.php?p=1021410) | shareware 1.9 is the usual copy ([vogons](https://www.vogons.org/viewtopic.php?p=886386)) |
| Ultimate 486 comparison (feipoa) | CPU, 23 DOS/Win tools | 28 Socket 3 CPUs | [thread](https://www.vogons.org/viewtopic.php?p=1260983): ALU/FPU/overall | tool list not read (403) |
| ZD WinBench 96/97 | disk, video, CPU, Win95 | WinBench 97 runs on Win95/NT only, 32-bit ([benchweb](https://www.wotug.org/parallel/nhse/rib/repositories/benchweb/catalog/Asset/winbench.html)); 96 reported OK on 98SE | none found for 486 | WB96+Winstone CD image on archive.org per [vogons](https://www.vogons.org/viewtopic.php?p=259457) (page not opened); winworldpc unverified |
| SiSoft Sandra 97/98 | Win95 | Win95/NT, 32-bit ([Sandra vs SAW](https://discmaster.textfiles.com/file/17089/TechnoGuide.iso/utility/sisandra/data.z/Sandra%20vs%20SAW.doc/Sandra%20vs%20SAW.pdf)); 98 worked where 99/01 crashed ([vogons](https://www.vogons.org/viewtopic.php?p=444825)) | none found | discmaster copies of Sandra (e.g. TechnoGuide link) |
| HD Tach 2.61 | disk, Win9x | uses a kernel VxD; may BSOD on autoinsert notification ([readme](https://discmaster.textfiles.com/view/36898/maximum-cd-2000-09.iso/Benchmarks/HDTach/htach261.exe/data1.cab/Program_Executable_Files/readme.txt)) | none for XT-class | discmaster |
| CrystalMark Retro 2.0 | CPU/disk/video, Win95/98/Me | freeware, covers 95/98/Me ([vogons](https://www.vogons.org/viewtopic.php?p=1113516)); min CPU/RAM not found | newer, few 486 results known | author page not checked |
| CheckIt 3.0 | CPU/disk, DOS | no source for specs | anecdote only ([vogons](https://www.vogons.org/viewtopic.php?p=200864)) | not found |
| Norton SI, Landmark, PCPBench, Quake timedemo | | | no source found in this pass | not found |

Not found for any tool: run length, CompactFlash/XT-IDE behaviour,
hang reports on non-AT hardware (no CMOS/RTC, 8-bit DMA, no second 8259).
The Inboard has none of that on the real bus, so every tool is untested
here until run. PCPBench, SpeedSys and Doom need a 386+, so 286 numbers
for them are derived ([vogons](https://www.vogons.org/viewtopic.php?p=599733)).

## Recommendation (one card session)

1. **Doom `-timedemo demo3`** (DOS). Most published numbers, any 486 ISA
   machine; units differ (realtics vs fps), so quote tics and the card.
   Shows: real game video-write throughput through the Mach8 in a VGA
   mode, which the Mach8 bench of 35 tests (accelerator only) does not.
2. **3DBench** (DOS). Has a score database with 386 and 486 entries.
   Shows: one CPU+video figure on the same scale as 386/486 owners; PAGETIME
   and PERFLOG do not give a cross-machine number. Caveat: 1.0c is
   unreliable on slow machines; a score above 83.5 MHz-class results is suspect.
3. **SpeedSys 4.78** (DOS). Gives CPU, cache and a disk figure in one
   run. Shows: a cache/memory number and a disk transfer rate that other
   people can compare; XTSECT times sectors, not a standard transfer test.
   Caveat: score depends on cache state, record the INBRDPC/cache setting.

Not recommended yet: WinBench 96/97 and Sandra 98 (Win95 disk and graphics
numbers, but no comparable 486 results found and no source page for
downloads); HD Tach (VxD, BSOD note). Next lookup if the owner wants Win95
numbers: WinBench 96 on the archive.org image, with the miniport loaded.
