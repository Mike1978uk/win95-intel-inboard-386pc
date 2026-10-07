; marks
        dw      046EEh, 0000h
        dw      05AEEh, 0001h
        dw      002E8h, 0071h
        dw      05AEEh, 0002h
        dw      002E8h, 0072h
        dw      05AEEh, 0000h
        dw      002E8h, 0070h
; 4AE8 reload?
        dw      04AE8h, 0002h
        dw      04AE8h, 0006h
        dw      04AE8h, 0002h
; lock/unlock alone
        dw      046EEh, 003Fh
        dw      046EEh, 0000h
; locked set-0 write, unlock
        dw      046EEh, 003Fh
        dw      05AEEh, 0000h
        dw      002E8h, 0080h
        dw      046EEh, 0000h
; mode after
        dw      04AE8h, 0002h
        dw      04AE8h, 0006h
        dw      04AE8h, 0002h
; set1 again, lock cycle
        dw      05AEEh, 0001h
        dw      002E8h, 0091h
        dw      046EEh, 003Fh
        dw      046EEh, 0000h
        dw      04AE8h, 0002h
        dw      0
