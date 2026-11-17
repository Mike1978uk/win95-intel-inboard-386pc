# KERNEL32 RaiseException entry (entry BFF9A013). Each hit logs the caller
# and the arguments: [esp] return, code, flags, nargs, lpArgs.
set pagination off
set confirm off
set architecture i386
set logging file docs/captures/2026-11-17_raise_gdb/raise_log.txt
set logging overwrite off
set logging enabled on
target remote :12345
hbreak *0xbff9a013
commands
  silent
  printf "RAISE eip=%08x esp=%08x\n", $eip, $esp
  x/6wx $esp
  if *(unsigned int*)($esp+16) != 0
    x/8wx *(unsigned int*)($esp+16)
  end
  continue
end
continue
