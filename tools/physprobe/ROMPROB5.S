# ROMPROB5.COM - prove the window's mapping and make the idle half usable, without a halt.
#
# Same method as INBRDPC.SYS and ROMPROB4: NMI masked (port A0h = 00h), port 62h bit 6 read after
# each access group, latch cleared by pulsing port 61h bit 5, NMI back on (A0h = 80h) at the end.
# With port 670h = 1Eh (bit 0 clear, window open):
# 1. Reads offset 100h of every 64 KB block from 0 to 5FFFFF, recording the parity flag for each.
# 2. Writes one dword at 580100 and restores it; lists every block whose offset 100h changed.
# 3. Only if exactly 080100 and 580100 changed (docs/romcache_decode_2026_10_03.md): copies
#    5E0000-5EFFFF onto itself, as INBRDPC does to conventional memory, so its parity is valid;
#    re-reads it; writes each dword's address and counts mismatches; zeroes it and checks the
#    zeroes. Parity flag after each pass, and any other block changed by these writes.
# 670h is put back to 1Fh before any DOS call. 5E0000 is left zeroed.
#
# Needs real mode (no EMM386), a 386+ and HIMEM (A20 through XMS).
# Build: tools/physprobe/build.sh ROMPROB5
	.intel_syntax noprefix
	.code16
	.text
	.set ORG, 0x100
	.set NBLK, 96				# 64 KB blocks from 000000 to 5F0000
	.set BLK5E, 0x5E
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
	cmp	al, 1
	je	a20_ok
no_xms:
	mov	dx, O(msg_noxms)
	call	puts
	jmp	quit
a20_ok:
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

	mov	al, 0x00			# mask NMI, as INBRDPC does around memory tests
	out	0xA0, al
	call	clrchk
	mov	dx, 0x670
	mov	al, 0x1E			# 0 wait states, bit 0 clear: window open
	out	dx, al
	wbinvd

# 1. Every block, parity flag each.
	mov	di, O(snapA)
	mov	bx, O(chkA)
	call	snapchk

# 2. Write test at 580100.
	mov	ebx, 0x580100
	mov	eax, fs:[ebx]
	mov	[O(worig)], eax
	mov	dword ptr fs:[ebx], 0x5A3CC3A5
	wbinvd
	mov	eax, fs:[ebx]
	mov	[O(wrb)], eax
	mov	di, O(snapB)
	mov	bx, O(chkB)
	call	snapchk
	mov	ebx, 0x580100			# snapchk used BX
	mov	eax, [O(worig)]
	mov	fs:[ebx], eax			# restore
	wbinvd
	call	clrchk
	call	check_map
	cmp	byte ptr [O(mapok)], 1
	jne	3f

# 3. Make 5E0000 usable.
	mov	ebx, 0x5E0000			# copy onto itself
	mov	ecx, 16384
4:	mov	eax, fs:[ebx]
	mov	fs:[ebx], eax
	add	ebx, 4
	dec	ecx
	jnz	4b
	wbinvd
	call	clrchk
	mov	ebx, 0x5E0000			# re-read
	mov	ecx, 16384
5:	mov	eax, fs:[ebx]
	add	ebx, 4
	dec	ecx
	jnz	5b
	call	getchk
	mov	[O(chk1)], al
	mov	ebx, 0x5E0000			# address in data
	mov	ecx, 16384
6:	mov	fs:[ebx], ebx
	add	ebx, 4
	dec	ecx
	jnz	6b
	wbinvd
	mov	ebx, 0x5E0000
	mov	ecx, 16384
	xor	edx, edx
7:	cmp	fs:[ebx], ebx
	je	8f
	inc	edx
8:	add	ebx, 4
	dec	ecx
	jnz	7b
	mov	[O(nbad)], edx
	call	getchk
	mov	[O(chk2)], al
	mov	ebx, 0x5E0000			# zero
	mov	ecx, 16384
	xor	eax, eax
9:	mov	fs:[ebx], eax
	add	ebx, 4
	dec	ecx
	jnz	9b
	wbinvd
	mov	ebx, 0x5E0000
	mov	ecx, 16384
	xor	edx, edx
10:	cmp	dword ptr fs:[ebx], 0
	je	11f
	inc	edx
11:	add	ebx, 4
	dec	ecx
	jnz	10b
	mov	[O(nzero)], edx
	call	getchk
	mov	[O(chk3)], al
	mov	di, O(snapC)
	mov	bx, O(chkC)
	call	snapchk
3:
	mov	dx, 0x670
	mov	al, 0x1F			# what INBRDPC leaves
	out	dx, al
	wbinvd
	call	clrchk
	mov	al, 0x80			# NMI back on
	out	0xA0, al
	sti
	mov	ah, 6				# local disable A20
	.byte	0xFF, 0x1E
	.word	O(xms)

# Report.
	mov	dx, O(msg_hdr)
	call	puts
	mov	dx, O(msg_par)
	call	puts
	mov	si, O(chkA)
	call	pr_flag
	mov	dx, O(msg_wr)
	call	puts
	mov	eax, [O(worig)]
	call	hex32
	mov	dx, O(msg_rb)
	call	puts
	mov	eax, [O(wrb)]
	call	hex32
	call	crlf
	mov	dx, O(msg_chg)
	call	puts
	mov	si, O(snapA)
	mov	bx, O(snapB)
	call	pr_chg
	cmp	byte ptr [O(mapok)], 1
	je	12f
	mov	dx, O(msg_nomap)
	call	puts
	jmp	13f
12:	mov	dx, O(msg_init)
	call	puts
	mov	al, [O(chk1)]
	call	hex8
	mov	dx, O(msg_bad)
	call	puts
	mov	eax, [O(nbad)]
	call	hex32
	mov	dx, O(msg_c2)
	call	puts
	mov	al, [O(chk2)]
	call	hex8
	mov	dx, O(msg_zero)
	call	puts
	mov	eax, [O(nzero)]
	call	hex32
	mov	dx, O(msg_c3)
	call	puts
	mov	al, [O(chk3)]
	call	hex8
	call	crlf
	mov	dx, O(msg_chg2)
	call	puts
	mov	si, O(snapA)
	mov	bx, O(snapC)
	call	pr_chg
13:	mov	dx, O(msg_end)
	call	puts
quit:
	mov	ax, 0x4C00
	int	0x21

# Pulse port 61h bit 5 to clear the I/O channel check latch (INBRDPC 0x9C0B).
clrchk:
	push	ax
	in	al, 0x61
	or	al, 0x20
	out	0x61, al
	and	al, 0xDF
	out	0x61, al
	pop	ax
	ret
# AL = port 62h & C0h, then clear the latch.
getchk:
	in	al, 0x62
	and	al, 0xC0
	call	clrchk
	ret

# Offset 100h of every block to [DI], parity flag per block to [BX].
snapchk:
	mov	esi, 0x100
	mov	cx, NBLK
14:	mov	eax, fs:[esi]
	mov	[di], eax
	push	ax
	call	getchk
	mov	[bx], al
	pop	ax
	add	di, 4
	inc	bx
	add	esi, 0x10000
	loop	14b
	ret

# mapok = 1 if exactly blocks 08 and 58 changed between snapA and snapB.
check_map:
	mov	si, O(snapA)
	xor	bp, bp
	xor	cx, cx
	xor	dx, dx				# DL: 08 changed, DH: 58 changed
15:	mov	eax, [si]
	cmp	eax, [si + 4 * NBLK]		# snapB follows snapA
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

# Print the block offset-100h address of every non-zero flag at [SI].
pr_flag:
	mov	eax, 0x100
	mov	cx, NBLK
19:	cmp	byte ptr [si], 0
	je	20f
	push	eax
	call	hex32
	mov	dl, '='
	call	putc
	mov	al, [si]
	call	hex8
	mov	dl, ' '
	call	putc
	pop	eax
20:	add	eax, 0x10000
	inc	si
	loop	19b
	call	crlf
	ret

# Print every block offset-100h address where [SI] and [BX] differ.
pr_chg:
	mov	eax, 0x100
	mov	cx, NBLK
21:	push	eax
	mov	eax, [si]
	cmp	eax, [bx]
	pop	eax
	je	22f
	push	eax
	call	hex32
	mov	dl, ' '
	call	putc
	pop	eax
22:	add	eax, 0x10000
	add	si, 4
	add	bx, 4
	loop	21b
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
23:	rol	eax, 4
	push	eax
	and	al, 0x0F
	call	nib
	pop	eax
	loop	23b
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
	jbe	24f
	add	al, 7
24:	mov	dl, al
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
worig:	.long	0
wrb:	.long	0
nbad:	.long	0
nzero:	.long	0
mapok:	.byte	0
chk1:	.byte	0
chk2:	.byte	0
chk3:	.byte	0
msg_v86:   .ascii "V86 mode - boot without EMM386.\r\n$"
msg_noxms: .ascii "No XMS or A20 not enabled - nothing done.\r\n$"
msg_hdr:   .ascii "ROMPROB5 - 670h=1E, NMI masked, parity polled via port 62h\r\n$"
msg_par:   .ascii "Blocks (offset 100h) with a parity flag on read: $"
msg_wr:    .ascii "Write at 00580100: orig $"
msg_rb:    .ascii "  readback $"
msg_chg:   .ascii "  changed at: $"
msg_nomap: .ascii "Mapping not as predicted - 5E0000 left untouched.\r\n$"
msg_init:  .ascii "5E0000: parity after copy-onto-itself $"
msg_bad:   .ascii "  address-test mismatches $"
msg_c2:    .ascii "  parity $"
msg_zero:  .ascii "  non-zero after zeroing $"
msg_c3:    .ascii "  parity $"
msg_chg2:  .ascii "  blocks changed by the 5E0000 writes: $"
msg_end:   .ascii "670h restored to 1F, NMI re-enabled.\r\n$"
	.balign	4
snapA:	.space	4 * NBLK
snapB:	.space	4 * NBLK
snapC:	.space	4 * NBLK
chkA:	.space	NBLK
chkB:	.space	NBLK
chkC:	.space	NBLK
