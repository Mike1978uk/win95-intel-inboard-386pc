# ROMPROBE.COM - does port 670h bit 0 open the Inboard's reserved block?
#
# RonnyRoy's U71 equations route 580000-5FFFFF to the card's system bank only while ROMCACHE is
# low, and serve F0000 reads from RAM while it is high. INBRDPC leaves port 670h at 1Fh (0 wait
# states, bit 0 set). This reads the window with 670h = 1Fh, then with 1Eh (bit 0 clear, same wait
# states), and in the 1Eh state writes one dword at 5E0100 and at 580100, restoring each at once.
# Every 64 KB block from 0 to 5FFFFF is read at offset 100h before and after each write, so an
# alias anywhere in that range shows as a changed address. 670h is put back to 1Fh before any DOS
# call; interrupts are off throughout.
#
# Needs real mode (no EMM386), a 386+ and HIMEM (A20 through XMS); the writes are skipped without it.
# Build: tools/physprobe/build.sh ROMPROBE
	.intel_syntax noprefix
	.code16
	.text
	.set ORG, 0x100
	.set NBLK, 96				# 64 KB blocks from 000000 to 5F0000
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
	mov	al, 0x1E			# 0 wait states, bit 0 (ROMCACHE?) clear
	out	dx, al
	wbinvd
	mov	di, O(res_off)
	call	scan

	cmp	byte ptr [O(a20ok)], 1
	jne	wr_skip
	mov	ebx, 0x5E0100
	mov	di, O(wr1)
	call	wtest
	mov	ebx, 0x580100
	mov	di, O(wr2)
	call	wtest
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
	mov	bx, O(wr2)
	call	pr_wr
5:	mov	dx, O(msg_end)
	call	puts
quit:
	mov	ax, 0x4C00
	int	0x21

# For each 64 KB block 580000-5F0000: first dword, dwords differing from F0000, FFFFFFFF dwords.
# 12 bytes per block at DI.
scan:
	mov	ebx, 0x580000
6:	mov	eax, fs:[ebx]
	mov	[di], eax
	xor	edx, edx
	xor	ebp, ebp
	xor	esi, esi
	mov	ecx, 16384
7:	mov	eax, fs:[ebx + esi]
	cmp	eax, -1
	jne	8f
	inc	ebp
8:	cmp	eax, fs:[esi + 0xF0000]
	je	9f
	inc	edx
9:	add	esi, 4
	dec	ecx
	jnz	7b
	mov	[di + 4], edx
	mov	[di + 8], ebp
	add	di, 12
	add	ebx, 0x10000
	cmp	ebx, 0x600000
	jb	6b
	ret

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

# Dword at offset 100h of every 64 KB block from 000000 to 5F0000, to [DI].
snap:
	mov	esi, 0x100
	mov	cx, NBLK
10:	mov	eax, fs:[esi]
	mov	[di], eax
	add	di, 4
	add	esi, 0x10000
	loop	10b
	ret

pr_scan:
	mov	eax, 0x580000
	mov	cx, 8
11:	push	eax
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
	pop	eax
	add	eax, 0x10000
	loop	11b
	ret

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
	mov	dx, O(msg_chg)
	call	puts
	lea	si, [bx + 12]
	mov	eax, 0x100
	mov	cx, NBLK
12:	push	eax
	mov	eax, [si]
	cmp	eax, [si + 4 * NBLK]
	pop	eax
	je	13f
	push	eax
	call	hex32
	mov	dl, ' '
	call	putc
	pop	eax
13:	add	eax, 0x10000
	add	si, 4
	loop	12b
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
14:	rol	eax, 4
	push	eax
	and	al, 0x0F
	call	nib
	pop	eax
	loop	14b
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
	jbe	15f
	add	al, 7
15:	mov	dl, al
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
a20ok:	.byte	0
havexms: .byte	0
wskip:	.byte	0
msg_v86:   .ascii "V86 mode - boot without EMM386.\r\n$"
msg_noxms: .ascii "No XMS - A20 not enabled, writes skipped.\r\n$"
msg_hdr:   .ascii "ROMPROBE - port 670h bit 0 and the 580000-5FFFFF window\r\n$"
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
msg_end:   .ascii "670h restored to 1F.\r\n$"
	.balign	4
res_on:	.space	96
res_off: .space	96
wr1:	.space	12 + 8 * NBLK
wr2:	.space	12 + 8 * NBLK
