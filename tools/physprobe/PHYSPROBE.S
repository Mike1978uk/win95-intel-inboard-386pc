# PHYSPROBE.COM - what answers at the Inboard's reserved 128 KB (0x5E0000-0x5FFFFF)?
#
# SHADRAM.VXD refused all 16 pages at 0x5E0000 on the 5160. This reads the window in real-mode
# DOS through a flat FS (unreal mode) and compares it with the areas it could alias:
#   C0000 video ROM, E0000, F0000 system BIOS, 4E0000 (the same address with A20 off),
#   and 5F0000 (the BIOS-shadow half, known to hold a BIOS copy).
# With A, port A0h bit 7 is set for the duration (interrupts off) and cleared after: per
# UniPCemu it maps the reserved block high, and INBRDPC writes A0h 00/80/00 while sizing memory.
# With W on the command line it also writes one dword in each half, interrupts off, reads it
# back, checks the alias candidates, and restores it at once. The write is skipped if
# 5E0000 and 4E0000 match, since that means A20 is off and the write would land at 4E0000.
#
# Needs real mode (no EMM386) and a 386+. A20 is enabled through XMS when HIMEM is loaded.
# Build: tools/physprobe/build.sh

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
	mov	cl, [0x80]
	xor	ch, ch
	mov	si, 0x81
parse:
	jcxz	parsed
	lodsb
	and	al, 0xDF
	cmp	al, 'A'
	jne	14f
	mov	byte ptr [O(aflag)], 1
14:	cmp	al, 'W'
	jne	parse_next
	mov	byte ptr [O(wflag)], 1
parse_next:
	dec	cx
	jmp	parse
parsed:
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
	cmp	byte ptr [O(aflag)], 0
	je	15f
	mov	al, 0x80			# port A0h bit 7: remap the reserved block high
	out	0xA0, al
15:	wbinvd

# 16 bytes from each address.
	mov	si, O(rdtab)
	mov	di, O(rdbuf)
rd_loop:
	mov	ebx, [si]
	cmp	ebx, -1
	je	rd_done
	mov	cx, 4
rd_dw:
	mov	eax, fs:[ebx]
	mov	[di], eax
	add	ebx, 4
	add	di, 4
	loop	rd_dw
	add	si, 4
	jmp	rd_loop
rd_done:

# Differing dwords over 64 KB for each pair, and FFFFFFFF / zero counts in 5E0000.
	mov	bp, O(cmptab)
	mov	di, O(cmpres)
cmp_loop:
	mov	esi, [bp]
	cmp	esi, -1
	je	cmp_done
	mov	ebx, [bp + 4]
	xor	edx, edx
	mov	ecx, 16384
cmp_dw:
	mov	eax, fs:[esi]
	cmp	eax, fs:[ebx]
	je	3f
	inc	edx
3:	add	esi, 4
	add	ebx, 4
	dec	ecx
	jnz	cmp_dw
	mov	[di], edx
	add	di, 4
	add	bp, 8
	jmp	cmp_loop
cmp_done:
	mov	esi, 0x5E0000
	xor	edx, edx
	xor	ebp, ebp
	mov	ecx, 16384
cnt_dw:
	mov	eax, fs:[esi]
	cmp	eax, -1
	jne	4f
	inc	edx
4:	test	eax, eax
	jnz	5f
	inc	ebp
5:	add	esi, 4
	dec	ecx
	jnz	cnt_dw
	mov	[O(nff)], edx
	mov	[O(nzero)], ebp

# Optional write test, gated on A20 being on (5E0000 differs from 4E0000).
	cmp	byte ptr [O(wflag)], 0
	je	wr_done
	cmp	dword ptr [O(cmpres) + 8], 0
	jne	wr_go
	mov	byte ptr [O(wskip)], 1
	jmp	wr_done
wr_go:
	mov	si, O(wrtab)
	mov	di, O(wrres)
wr_loop:
	mov	ebx, [si]
	cmp	ebx, -1
	je	wr_done
	mov	eax, fs:[ebx]
	mov	[di], eax			# original
	call	snap_alias			# DI+8.. = C0100 E0100 F0100 4E0100, before
	mov	dword ptr fs:[ebx], 0x5A3CC3A5
	wbinvd
	mov	eax, fs:[ebx]
	mov	[di + 4], eax			# read back
	add	di, 16
	call	snap_alias			# after
	sub	di, 16
	mov	eax, [di]
	mov	fs:[ebx], eax			# restore
	wbinvd
	add	di, 40
	add	si, 4
	jmp	wr_loop
wr_done:
	cmp	byte ptr [O(aflag)], 0
	je	16f
	xor	al, al				# INBRDPC leaves A0h at 00
	out	0xA0, al
	wbinvd
16:	sti

	cmp	byte ptr [O(havexms)], 0
	je	6f
	mov	ah, 6				# local disable A20
	.byte	0xFF, 0x1E
	.word	O(xms)
6:

# Report.
	mov	dx, O(msg_hdr)
	call	puts
	cmp	byte ptr [O(havexms)], 0
	je	7f
	mov	dx, O(msg_a20)
	call	puts
	mov	al, [O(a20ok)]
	call	hex8
	call	crlf
7:
	mov	si, O(rdtab)
	mov	bx, O(rdbuf)
pr_rd:
	mov	eax, [si]
	cmp	eax, -1
	je	pr_rd_done
	call	hex32
	mov	dl, ':'
	call	putc
	mov	cx, 16
8:	mov	dl, ' '
	call	putc
	mov	al, [bx]
	call	hex8
	inc	bx
	loop	8b
	call	crlf
	add	si, 4
	jmp	pr_rd
pr_rd_done:
	mov	si, O(cmptab)
	mov	bx, O(cmpres)
pr_cmp:
	mov	eax, [si]
	cmp	eax, -1
	je	pr_cmp_done
	mov	dx, O(msg_diff)
	call	puts
	mov	eax, [si]
	call	hex32
	mov	dx, O(msg_vs)
	call	puts
	mov	eax, [si + 4]
	call	hex32
	mov	dx, O(msg_eq)
	call	puts
	mov	eax, [bx]
	call	hex32
	call	crlf
	add	si, 8
	add	bx, 4
	jmp	pr_cmp
pr_cmp_done:
	mov	dx, O(msg_ff)
	call	puts
	mov	eax, [O(nff)]
	call	hex32
	mov	dx, O(msg_zero)
	call	puts
	mov	eax, [O(nzero)]
	call	hex32
	call	crlf

	cmp	byte ptr [O(wflag)], 0
	je	quit
	cmp	byte ptr [O(wskip)], 0
	je	pr_wr
	mov	dx, O(msg_wskip)
	call	puts
	jmp	quit
pr_wr:
	mov	si, O(wrtab)
	mov	bx, O(wrres)
pr_wr_loop:
	mov	eax, [si]
	cmp	eax, -1
	je	quit
	mov	dx, O(msg_wr)
	call	puts
	mov	eax, [si]
	call	hex32
	call	crlf
	mov	dx, O(msg_wr2)			# orig, readback
	call	puts
	mov	eax, [bx]
	call	hex32
	mov	dl, ' '
	call	putc
	mov	eax, [bx + 4]
	call	hex32
	call	crlf
	mov	dx, O(msg_wr3)			# C0100 E0100 F0100 4E0100 before/after
	call	puts
	xor	di, di
9:	mov	eax, [bx + di + 8]
	call	hex32
	mov	dl, '/'
	call	putc
	mov	eax, [bx + di + 24]
	call	hex32
	mov	dl, ' '
	call	putc
	add	di, 4
	cmp	di, 16
	jb	9b
	call	crlf
	add	si, 4
	add	bx, 40
	jmp	pr_wr_loop

quit:
	mov	ax, 0x4C00
	int	0x21

# Read C0100, E0100, F0100, 4E0100 into [DI+8..DI+23]. Preserves EBX.
snap_alias:
	push	si
	mov	si, O(aliastab)
	push	di
10:	mov	edx, [si]
	cmp	edx, -1
	je	11f
	mov	eax, fs:[edx]
	mov	[di + 8], eax
	add	di, 4
	add	si, 4
	jmp	10b
11:	pop	di
	pop	si
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
12:	rol	eax, 4
	push	eax
	and	al, 0x0F
	call	nib
	pop	eax
	loop	12b
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
	jbe	13f
	add	al, 7
13:	mov	dl, al
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
nff:	.long	0
nzero:	.long	0
rdtab:	.long	0x0C0000, 0x0E0000, 0x0F0000, 0x4E0000, 0x5E0000, 0x5E8000, 0x5F0000, 0x540000, -1
cmptab:	.long	0x5E0000, 0x0C0000
	.long	0x5E0000, 0x0E0000
	.long	0x5E0000, 0x4E0000
	.long	0x5E0000, 0x5F0000
	.long	0x5F0000, 0x0F0000
	.long	-1
wrtab:	.long	0x5E0100, 0x5F0100, -1
aliastab: .long	0x0C0100, 0x0E0100, 0x0F0100, 0x4E0100, -1
wflag:	.byte	0
aflag:	.byte	0
wskip:	.byte	0
havexms: .byte	0
a20ok:	.byte	0

msg_v86:	.ascii	"CPU in V86 mode (EMM386 or Windows): run from plain DOS.\r\n$"
msg_noxms:	.ascii	"No XMS driver: A20 not enabled by this program.\r\n$"
msg_hdr:	.ascii	"PHYSPROBE - Inboard reserved block\r\n$"
msg_a20:	.ascii	"XMS local enable A20 returned AX low byte $"
msg_diff:	.ascii	"differing dwords (of 4000) $"
msg_vs:		.ascii	" vs $"
msg_eq:		.ascii	" = $"
msg_ff:		.ascii	"5E0000 64K: FFFFFFFF dwords $"
msg_zero:	.ascii	"  zero dwords $"
msg_wskip:	.ascii	"Write test skipped: 5E0000 equals 4E0000 (A20 off?).\r\n$"
msg_wr:		.ascii	"write test at $"
msg_wr2:	.ascii	"  original, read back after writing 5A3CC3A5: $"
msg_wr3:	.ascii	"  C0100 E0100 F0100 4E0100 before/after: $"

	.balign	4
rdbuf:	.space	16 * 8
cmpres:	.space	4 * 5
wrres:	.space	40 * 2
