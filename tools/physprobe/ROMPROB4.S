# ROMPROB4.COM - the reserved block with port 670h bit 0 clear, parity flagged per block, no halt.
#
# ROMPROBE and ROMPROB2 stopped with PARITY CHECK 2: the card checks parity on every DRAM read
# and raises I/O CH CK, which reaches the NMI. INBRDPC.SYS reads suspect memory the same way it
# is done here: NMI masked (port A0h = 00h), port 62h bit 6 read after each block, the latch
# cleared by pulsing port 61h bit 5, NMI re-enabled (A0h = 80h) at the end (driver 0x9908,
# 0x9BFE-0x9C14, 0xA626-0xA668).
#
# Reads only. For each step: first dword, dwords differing from F0000, and port 62h & C0h
# (40h = I/O channel check, i.e. a parity error on the card; 80h = planar parity).
#   ctl  080000 with 670h = 1Fh (control: written memory, should be clean)
#   0    670h = 1Eh, no read
#   then with 670h = 1Eh: F0000, 5F0000, 580000, 590000, 5A0000-5D0000, 5E0000
# 670h back to 1Fh, latch cleared, NMI on, then the report.
#
# Needs real mode (no EMM386), a 386+ and HIMEM (A20 through XMS).
# Build: tools/physprobe/build.sh ROMPROB4
	.intel_syntax noprefix
	.code16
	.text
	.set ORG, 0x100
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
	wbinvd

	mov	si, O(steps)
	mov	di, O(res)
3:	mov	al, [si]
	test	al, al
	jz	6f
	cmp	al, '0'
	jne	4f
	mov	dx, 0x670			# step 0: clear bit 0, no read
	mov	al, 0x1E
	out	dx, al
	wbinvd
	xor	eax, eax
	mov	[di], eax
	mov	[di + 4], eax
	jmp	5f
4:	mov	ebx, [si + 1]
	call	rdblk
5:	in	al, 0x62
	and	al, 0xC0
	mov	[di + 8], al
	call	clrchk
	add	di, 12
	add	si, 5
	jmp	3b
6:
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
	mov	si, O(steps)
	mov	bx, O(res)
7:	mov	al, [si]
	test	al, al
	jz	8f
	mov	dl, al
	call	putc
	mov	dl, ' '
	call	putc
	mov	eax, [si + 1]
	call	hex32
	mov	dx, O(msg_first)
	call	puts
	mov	eax, [bx]
	call	hex32
	mov	dx, O(msg_diff)
	call	puts
	mov	eax, [bx + 4]
	call	hex32
	mov	dx, O(msg_chk)
	call	puts
	mov	al, [bx + 8]
	call	hex8
	call	crlf
	add	bx, 12
	add	si, 5
	jmp	7b
8:	mov	dx, O(msg_end)
	call	puts
quit:
	mov	ax, 0x4C00
	int	0x21

# Pulse port 61h bit 5 to clear the I/O channel check latch (INBRDPC 0x9C0B).
clrchk:
	in	al, 0x61
	or	al, 0x20
	out	0x61, al
	and	al, 0xDF
	out	0x61, al
	ret

# Read 64 KB at EBX: first dword to [DI], dwords differing from F0000 to [DI+4].
rdblk:
	mov	eax, fs:[ebx]
	mov	[di], eax
	xor	edx, edx
	push	si
	xor	esi, esi
	mov	ecx, 16384
9:	mov	eax, fs:[ebx + esi]
	cmp	eax, fs:[esi + 0xF0000]
	je	10f
	inc	edx
10:	add	esi, 4
	dec	ecx
	jnz	9b
	pop	si
	mov	[di + 4], edx
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
11:	rol	eax, 4
	push	eax
	and	al, 0x0F
	call	nib
	pop	eax
	loop	11b
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
	jbe	12f
	add	al, 7
12:	mov	dl, al
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
steps:	.byte	'c'				# control, 670h still 1Fh
	.long	0x080000
	.byte	'0'
	.long	0
	.byte	'F'
	.long	0x0F0000
	.byte	'5'
	.long	0x5F0000
	.byte	'8'
	.long	0x580000
	.byte	'9'
	.long	0x590000
	.byte	'A'
	.long	0x5A0000
	.byte	'B'
	.long	0x5B0000
	.byte	'C'
	.long	0x5C0000
	.byte	'D'
	.long	0x5D0000
	.byte	'E'
	.long	0x5E0000
	.byte	0
msg_v86:   .ascii "V86 mode - boot without EMM386.\r\n$"
msg_noxms: .ascii "No XMS or A20 not enabled - nothing done.\r\n$"
msg_hdr:   .ascii "ROMPROB4 - step, address, first dword, dwords differing from F0000, port 62h&C0 (40 = card parity)\r\n$"
msg_first: .ascii ": $"
msg_diff:  .ascii "  diffF0000 $"
msg_chk:   .ascii "  62h $"
msg_end:   .ascii "670h restored to 1F, NMI re-enabled.\r\n$"
	.balign	4
res:	.space	12 * 11
