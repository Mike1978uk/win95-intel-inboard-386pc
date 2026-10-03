# ROMPROB3.COM - which step after clearing port 670h bit 0 raises the Inboard's parity error?
#
# ROMPROBE and ROMPROB2 both stopped with PARITY CHECK 2 (the card's I/O channel check) before
# printing. This runs one step at a time, writing a marker on the top text row before each step
# and a '+' after it, so the screen shows the step that faulted when the BIOS halts:
#   0  clear bit 0 (670h = 1Eh), spin ~1 ms, no memory access
#   F  read F0000-FFFFF (the ROM, over the bus, with bit 0 clear)
#   5  read 5F0000 (system F0000, the BIOS copy)
#   8  read 580000 (predicted: conventional 80000)
#   9  read 590000 (predicted: conventional 90000)
#   A-D read 5A0000-5D0000 (predicted: extended 100000-13FFFF)
# then 670h back to 1Fh. Reads only - nothing is written to memory outside the text screen.
# The cursor is parked on row 12 first so the BIOS message does not scroll the markers away.
#
# Needs real mode (no EMM386), a 386+, HIMEM (A20 through XMS) and a colour text screen at B8000.
# Build: tools/physprobe/build.sh ROMPROB3
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
	mov	ax, 0x0003			# 80x25 colour text, clears the screen
	int	0x10
	mov	ah, 2				# cursor to row 12, column 0
	xor	bh, bh
	mov	dx, 0x0C00
	int	0x10
	mov	dx, O(msg_hdr)
	call	puts

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

	mov	edi, 0xB8000			# screen position for the next marker
	mov	al, '0'
	call	mark
	mov	dx, 0x670
	mov	al, 0x1E
	out	dx, al
	wbinvd
	mov	ecx, 100000			# spin, no memory access beyond the loop
3:	dec	ecx
	jnz	3b
	call	done

	mov	si, O(steps)
4:	mov	al, [si]
	test	al, al
	jz	5f
	call	mark
	mov	ebx, [si + 1]
	call	rdblk
	call	done
	add	si, 5
	jmp	4b
5:
	mov	dx, 0x670
	mov	al, 0x1F			# what INBRDPC leaves
	out	dx, al
	wbinvd
	sti
	mov	ah, 6				# local disable A20
	.byte	0xFF, 0x1E
	.word	O(xms)
	mov	dx, O(msg_ok)
	call	puts
quit:
	mov	ax, 0x4C00
	int	0x21

# Marker AL in yellow on blue at FS:[EDI].
mark:
	mov	ah, 0x1E
	mov	fs:[edi], ax
	add	edi, 2
	ret
# '+' in white on green after a step completes, then a space.
done:
	mov	ax, 0x2F2B
	mov	fs:[edi], ax
	add	edi, 4
	ret

# Read 64 KB from EBX (sum discarded).
rdblk:
	mov	ecx, 16384
	xor	edx, edx
6:	add	edx, fs:[ebx]
	add	ebx, 4
	dec	ecx
	jnz	6b
	ret

puts:
	mov	ah, 9
	int	0x21
	ret

	.balign	4
gdt:	.quad	0
	.word	0xFFFF, 0
	.byte	0, 0x92, 0xCF, 0
gdtr:	.word	15
	.long	0
xms:	.long	0
steps:	.byte	'F'
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
	.byte	0
msg_v86:   .ascii "V86 mode - boot without EMM386.\r\n$"
msg_noxms: .ascii "No XMS or A20 not enabled - nothing done.\r\n$"
msg_hdr:   .ascii "ROMPROB3: markers on the top row, '+' = step finished.\r\n$"
msg_ok:    .ascii "All steps finished, 670h restored to 1F.\r\n$"
