# Issue #7 live reproduction - 86Box twin of the golden pre-monolith card, 2026-08-23

Machine: ibmxt_inboard386, bios=ibm5160_050986, mach8_vga_isa, display.drv=vga.drv
Stage: Windows 95 first-boot Setup, immediately after 'Programs on the Start menu',
       i.e. entering the 'Windows Help' item. Same point the real 5160 stalls at.

## The guest is ALIVE, not crashed
[modecheck] t+834s CS:PC=0028:C0036E8E A0000_xor16k=00003E8C ring_last=0028:C0036E8E oldpc=C0036E8E
[modecheck] t+835s CS:PC=0028:C0003DA6 A0000_xor16k=00003E8C ring_last=0028:C0003DA6 oldpc=C0003DA6
[modecheck] t+836s CS:PC=0028:C0003249 A0000_xor16k=00003E8C ring_last=0028:C0003249 oldpc=C0003249
[modecheck] t+837s CS:PC=0028:C003713E A0000_xor16k=00003E8C ring_last=0028:C003713E oldpc=C003713E
[modecheck] t+838s CS:PC=0028:C0003244 A0000_xor16k=00003E8C ring_last=0028:C0003244 oldpc=C0003244
[modecheck] t+839s CS:PC=0028:C0036EA9 A0000_xor16k=00003E8C ring_last=0028:C0036EA9 oldpc=C0036EA9
[modecheck] t+840s CS:PC=0028:C00012C7 A0000_xor16k=00003E8C ring_last=0028:C00012C7 oldpc=C00012C7
[modecheck] t+841s CS:PC=0028:C0035C8E A0000_xor16k=00003E8C ring_last=0028:C0035C8E oldpc=C0035C8E
[modecheck] t+842s CS:PC=0028:C0035C94 A0000_xor16k=00003E8C ring_last=0028:C0035C94 oldpc=C0035C94
[modecheck] t+843s CS:PC=0028:C00372A5 A0000_xor16k=00003E8C ring_last=0028:C00372A5 oldpc=C00372A5
[modecheck] t+844s CS:PC=0028:C0035C22 A0000_xor16k=00003E8C ring_last=0028:C0035C22 oldpc=C0035C22
[modecheck] t+845s CS:PC=0028:C0037179 A0000_xor16k=00003E8C ring_last=0028:C0037179 oldpc=C0037179

## Video frozen: A0000 checksum unchanged since t+499s
t+456s A0000_xor16k=00003B12
t+459s A0000_xor16k=000037C4
t+467s A0000_xor16k=00003B12
t+469s A0000_xor16k=00003E8C
t+474s A0000_xor16k=000014D4
t+499s A0000_xor16k=00003E8C

## PIT: Windows' own VTD timer is programmed (mode 3 -> mode 2) and still counting
[pitxstate] ch0 state=4 mode=2 gate=1 ce=7453 clocks=938118041
[pitxstate] ch0 state=4 mode=2 gate=1 ce=10473 clocks=939311053
[pitxstate] ch0 state=4 mode=2 gate=1 ce=14516 clocks=940503042
[pitxstate] ch0 state=4 mode=2 gate=1 ce=15806 clocks=941697784

## Ring-0 PC clusters during the stall (last 200 samples, by page)
     28 C324
     23 C371
     21 C35C
     14 C36E
     13 C372
     11 C12B
      8 C323
      8 C321

## Ruled out: not waiting on keyboard input
Sent ENTER then SPACE to the guest; A0000 checksum stayed 00003E8C across 12s.
