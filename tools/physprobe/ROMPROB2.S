# ROMPROB2.COM - port 670h bit 0 and the reserved block, without reading never-written RAM.
#
# DRAM with parity must be written before it is read: a read of a never-written location raises
# the Inboard's parity error, which stops the machine. Half of the reserved block (system bank
# E0000, seen at 5E0000) is never written, so it is not read until it has been.
#
# 1. Reads 580000-5DFFFF and 5F0000 with 670h = 1Fh (as INBRDPC leaves it), then with 1Eh
#    (bit 0 clear, same wait states).
# 2. In the 1Eh state writes one dword at 580100 and restores it. Every 64 KB block from 0 to
#    5FFFFF except 5E is read at offset 100h before and after, so any alias shows.
# 3. Only if exactly 080100 and 580100 changed (the mapping predicted in
#    docs/romcache_decode_2026_10_03.md): zeroes 5E0000-5EFFFF, checks the zeroing disturbed no
#    other block, writes each dword's own address, counts mismatches, and zeroes it again.
# 4. Puts 670h back to 1Fh before any DOS call. Interrupts are off throughout.
#
# Needs real mode (no EMM386), a 386+ and HIMEM (A20 through XMS); writes are skipped without it.
# Build: tools/physprobe/build.sh ROMPROB2
	.intel_syntax noprefix
	.code16
	.text
	.set ORG, 0x100
	.set NBLK, 96				# 64 KB blocks from 000000 to 5F0000
	.set NSCAN, 7				# blocks read by scan
	.set BLK5E, 94				# index of the 5E0000 block
_start:
	cld
	smsw	ax
	test	al, 1
	jz	real_ok
	mov	dx, O(msg_v86)
	call	puts
	jmp	quit
real_ok:
	mov	ax, 0x4300
	int	0x2F
	cmp	al, 0x80
	jne	no_xms
	mov	ax, 0x4310
	int	0x2F
	mov	[O(xms)], bx
	mov	[O(xms) + 2], es
	push	cs
	pop	es
	mov	ah, 5				# local enable A20
	.byte	0xFF, 0x1E			# call far [xms]
	.word	O(xms)
	mov	[O(a20ok)], al
	mov	byte ptr [O(havexms)], 1
	jmp	xms_done
no_xms:
	mov	dx, O(msg_noxms)
	call	puts
xms_done:
# Unreal mode: load FS with a flat 4 GB descriptor, return to real mode, keep the limit.
	cli
	xor	eax, eax
	mov	ax, cs
	shl	eax, 4
	add	eax, O(gdt)
	mov	[O(gdtr) + 2], eax
	lgdt	[O(gdtr)]
	mov	eax, cr0
	or	al, 1
	mov	cr0, eax
	jmp	1f
1:	mov	bx, 8
	mov	fs, bx
	and	al, 0xFE
	mov	cr0, eax
	jmp	2f
2:	xor	bx, bx
	mov	fs, bx

	wbinvd
	mov	di, O(res_on)
	call	scan
	mov	dx, 0x670
	mov	al, 0x1E			# 0 wait states, bit 0 (ROMCACHE) clear
	out	dx, al
	wbinvd
	mov	di, O(res_off)
	call	scan

	cmp	byte ptr [O(a20ok)], 1
	jne	wr_skip
	mov	ebx, 0x580100
	mov	di, O(wr1)
	call	wtest
	call	check_map
	cmp	byte ptr [O(mapok)], 1
	jne	wr_end
	call	init5e
	jmp	wr_end
wr_skip:
	mov	byte ptr [O(wskip)], 1
wr_end:
	mov	dx, 0x670
	mov	al, 0x1F			# what INBRDPC leaves
	out	dx, al
	wbinvd
	sti

	cmp	byte ptr [O(havexms)], 0
	je	3f
	mov	ah, 6				# local disable A20
	.byte	0xFF, 0x1E
	.word	O(xms)
3:

# Report.
	mov	dx, O(msg_hdr)
	call	puts
	mov	dx, O(msg_a20)
	call	puts
	mov	al, [O(a20ok)]
	call	hex8
	call	crlf
	mov	dx, O(msg_on)
	call	puts
	mov	bx, O(res_on)
	call	pr_scan
	mov	dx, O(msg_off)
	call	puts
	mov	bx, O(res_off)
	call	pr_scan
	cmp	byte ptr [O(wskip)], 0
	je	4f
	mov	dx, O(msg_wskip)
	call	puts
	jmp	5f
4:	mov	bx, O(wr1)
	call	pr_wr
	cmp	byte ptr [O(mapok)], 1
	je	6f
	mov	dx, O(msg_nomap)
	call	puts
	jmp	5f
6:	mov	dx, O(msg_init)
	call	puts
	mov	si, O(snapA)
	mov	bx, O(snapB)
	call	pr_chg
	mov	dx, O(msg_bad)
	call	puts
	mov	eax, [O(nbad)]
	call	hex32
	mov	dx, O(msg_zero)
	call	puts
	mov	eax, [O(nzero)]
	call	hex32
	call	crlf
5:	mov	dx, O(msg_end)
	call	puts
quit:
	mov	ax, 0x4C00
	int	0x21

# For each block in scantab: first dword, dwords differing from F0000, FFFFFFFF dwords.
# 12 bytes per block at DI.
scan:
	mov	si, O(scantab)
7:	mov	ebx, [si]
	cmp	ebx, -1
	je	10f
	mov	eax, fs:[ebx]
	mov	[di], eax
	xor	edx, edx
	xor	ebp, ebp
	push	si
	xor	esi, esi
	mov	ecx, 16384
8:	mov	eax, fs:[ebx + esi]
	cmp	eax, -1
	jne	9f
	inc	ebp
9:	cmp	eax, fs:[esi + 0xF0000]
	je	11f
	inc	edx
11:	add	esi, 4
	dec	ecx
	jnz	8b
	pop	si
	mov	[di + 4], edx
	mov	[di + 8], ebp
	add	di, 12
	add	si, 4
	jmp	7b
10:	ret

# Write test at EBX. DI: +0 address, +4 original, +8 readback, +12 before[NBLK], +12+4*NBLK after[NBLK].
wtest:
	mov	[di], ebx
	mov	eax, fs:[ebx]
	mov	[di + 4], eax
	push	di
	add	di, 12
	call	snap
	pop	di
	mov	dword ptr fs:[ebx], 0x5A3CC3A5
	wbinvd
	mov	eax, fs:[ebx]
	mov	[di + 8], eax
	push	di
	add	di, 12 + 4 * NBLK
	call	snap
	pop	di
	mov	eax, [di + 4]
	mov	fs:[ebx], eax			# restore
	wbinvd
	ret

# Dword at offset 100h of every 64 KB block from 000000 to 5F0000, to [DI]. The 5E block is
# stored as 0 without being read unless snap5e is set (it has been written by then).
snap:
	mov	esi, 0x100
	xor	bp, bp
12:	xor	eax, eax
	cmp	bp, BLK5E
	jne	13f
	cmp	byte ptr [O(snap5e)], 0
	je	14f
13:	mov	eax, fs:[esi]
14:	mov	[di], eax
	add	di, 4
	add	esi, 0x10000
	inc	bp
	cmp	bp, NBLK
	jb	12b
	ret

# mapok = 1 if exactly blocks 08 and 58 changed in the write test.
check_map:
	mov	si, O(wr1) + 12
	xor	bp, bp
	xor	cx, cx
	xor	dx, dx				# DL: 08 changed, DH: 58 changed
15:	mov	eax, [si]
	cmp	eax, [si + 4 * NBLK]
	je	16f
	inc	cx
	cmp	bp, 0x08
	jne	17f
	mov	dl, 1
17:	cmp	bp, 0x58
	jne	16f
	mov	dh, 1
16:	add	si, 4
	inc	bp
	cmp	bp, NBLK
	jb	15b
	cmp	cx, 2
	jne	18f
	cmp	dx, 0x0101
	jne	18f
	mov	byte ptr [O(mapok)], 1
18:	ret

# Zero 5E0000-5EFFFF, snapshot every block before and after, then an address-in-data test, then
# zero again. nbad = dwords that did not read back their address, nzero = dwords not zero at the end.
init5e:
	mov	di, O(snapA)
	call	snap				# snap5e = 0: 5E stored as 0, not read
	mov	ebx, 0x5E0000
	mov	ecx, 16384
	xor	eax, eax
19:	mov	fs:[ebx], eax
	add	ebx, 4
	dec	ecx
	jnz	19b
	wbinvd
	mov	byte ptr [O(snap5e)], 1
	mov	di, O(snapB)
	call	snap
	mov	ebx, 0x5E0000
	mov	ecx, 16384
20:	mov	fs:[ebx], ebx
	add	ebx, 4
	dec	ecx
	jnz	20b
	wbinvd
	mov	ebx, 0x5E0000
	mov	ecx, 16384
	xor	edx, edx
21:	cmp	fs:[ebx], ebx
	je	22f
	inc	edx
22:	add	ebx, 4
	dec	ecx
	jnz	21b
	mov	[O(nbad)], edx
	mov	ebx, 0x5E0000
	mov	ecx, 16384
	xor	eax, eax
23:	mov	fs:[ebx], eax
	add	ebx, 4
	dec	ecx
	jnz	23b
	wbinvd
	mov	ebx, 0x5E0000
	mov	ecx, 16384
	xor	edx, edx
24:	cmp	dword ptr fs:[ebx], 0
	je	25f
	inc	edx
25:	add	ebx, 4
	dec	ecx
	jnz	24b
	mov	[O(nzero)], edx
	ret

pr_scan:
	mov	si, O(scantab)
26:	mov	eax, [si]
	cmp	eax, -1
	je	27f
	call	hex32
	mov	dx, O(msg_first)
	call	puts
	mov	eax, [bx]
	call	hex32
	mov	dx, O(msg_diff)
	call	puts
	mov	eax, [bx + 4]
	call	hex32
	mov	dx, O(msg_ff)
	call	puts
	mov	eax, [bx + 8]
	call	hex32
	call	crlf
	add	bx, 12
	add	si, 4
	jmp	26b
27:	ret

pr_wr:
	mov	dx, O(msg_wr)
	call	puts
	mov	eax, [bx]
	call	hex32
	mov	dx, O(msg_orig)
	call	puts
	mov	eax, [bx + 4]
	call	hex32
	mov	dx, O(msg_rb)
	call	puts
	mov	eax, [bx + 8]
	call	hex32
	call	crlf
	lea	si, [bx + 12]
	lea	bx, [bx + 12 + 4 * NBLK]
# Print " changed at: " and every block offset 100h where [SI] and [BX] differ.
pr_chg:
	mov	dx, O(msg_chg)
	call	puts
	mov	eax, 0x100
	mov	cx, NBLK
28:	push	eax
	mov	eax, [si]
	cmp	eax, [bx]
	pop	eax
	je	29f
	push	eax
	call	hex32
	mov	dl, ' '
	call	putc
	pop	eax
29:	add	eax, 0x10000
	add	si, 4
	add	bx, 4
	loop	28b
	call	crlf
	ret

puts:
	mov	ah, 9
	int	0x21
	ret
putc:
	push	ax
	mov	ah, 2
	int	0x21
	pop	ax
	ret
crlf:
	push	dx
	mov	dl, 13
	call	putc
	mov	dl, 10
	call	putc
	pop	dx
	ret
hex32:
	push	ecx
	mov	cx, 8
30:	rol	eax, 4
	push	eax
	and	al, 0x0F
	call	nib
	pop	eax
	loop	30b
	pop	ecx
	ret
hex8:
	push	ax
	shr	al, 4
	call	nib
	pop	ax
	push	ax
	and	al, 0x0F
	call	nib
	pop	ax
	ret
nib:
	push	dx
	add	al, '0'
	cmp	al, '9'
	jbe	31f
	add	al, 7
31:	mov	dl, al
	call	putc
	pop	dx
	ret

	.balign	4
gdt:	.quad	0
	.word	0xFFFF, 0
	.byte	0, 0x92, 0xCF, 0
gdtr:	.word	15
	.long	0
xms:	.long	0
nbad:	.long	0
nzero:	.long	0
scantab: .long	0x580000, 0x590000, 0x5A0000, 0x5B0000, 0x5C0000, 0x5D0000, 0x5F0000, -1
a20ok:	.byte	0
havexms: .byte	0
wskip:	.byte	0
mapok:	.byte	0
snap5e:	.byte	0
msg_v86:   .ascii "V86 mode - boot without EMM386.\r\n$"
msg_noxms: .ascii "No XMS - A20 not enabled, writes skipped.\r\n$"
msg_hdr:   .ascii "ROMPROB2 - port 670h bit 0 and the 580000-5FFFFF window (5E read only after zeroing)\r\n$"
msg_a20:   .ascii "XMS local enable A20 returned AX low byte $"
msg_on:    .ascii "670h=1F (as booted): block, first dword, dwords differing from F0000, FFFFFFFF dwords\r\n$"
msg_off:   .ascii "670h=1E (bit 0 clear):\r\n$"
msg_first: .ascii ": $"
msg_diff:  .ascii "  diffF0000 $"
msg_ff:    .ascii "  FF $"
msg_wr:    .ascii "write (670h=1E) at $"
msg_orig:  .ascii "  orig $"
msg_rb:    .ascii "  readback $"
msg_chg:   .ascii "  changed at: $"
msg_wskip: .ascii "Writes skipped: A20 not confirmed on.\r\n$"
msg_nomap: .ascii "Mapping not as predicted - 5E0000 left untouched.\r\n$"
msg_init:  .ascii "5E0000-5EFFFF zeroed and tested. Other blocks changed by the zeroing:\r\n$"
msg_bad:   .ascii "  address-test mismatches $"
msg_zero:  .ascii "  non-zero after re-zeroing $"
msg_end:   .ascii "670h restored to 1F.\r\n$"
	.balign	4
res_on:	.space	12 * NSCAN
res_off: .space	12 * NSCAN
wr1:	.space	12 + 8 * NBLK
snapA:	.space	4 * NBLK
snapB:	.space	4 * NBLK
