# VCONN — trace the vendor descriptor with the bridge CONNECTED

The `VT3` capture single-stepped the vendor's `0x4CA3` with the bridge idle, so
the ECR waits never drained; the status check at `4EA6` failed and the routine
branched straight to teardown. It therefore never emitted **count-low** or the
**block command**, which is exactly the part still unconfirmed in
`TRANSPORT_SPEC.md` §11.

`VCONN.SCR` fixes the precondition rather than the symptom:

1. `SD120PPD.SYS` loads at `0100`–`DB85`, so the connect code is assembled at
   `E000` — above the image, below the stack at `FFEE`.
2. The CPP blocks are copied out of `INQ9.SCR` by script and only relocated
   (`0300→E100 0310→E110 0320→E120 03D0→E1D0`). Nothing is retyped.
3. `g=E000` runs unlock + `CPP_CONNECT` and stops on `int 3`.
4. Only then are the globals set and `IP` pointed at `4DA3`.

**No `TEST AL,01 → OR AL,01` patching this time.** That was a tracing aid for a
dead bus; it forces paths the routine would not take against a live peripheral,
which is the opposite of what is wanted here.
