; Pseudo-instructions expand as the spec says.
; expect word 0x00 = 0x04000000
; expect word 0x04 = 0x04480000
; expect word 0x08 = 0x04C3FFFF
; expect word 0x0C = 0x1D000080
; expect word 0x10 = 0x0D100000
; expect word 0x14 = 0x38000002
; expect word 0x18 = 0x3C040000
; expect word 0x1C = 0x78000000
; expect r3 = -1
; expect r4 = 131072
; expect halt
    NOP                      ; ADDI r0, r0, 0
    MV    r1, r2             ; ADDI r1, r2, 0
    LI    r3, -1             ; fits in 18 bits: ADDI
    LI    r4, 131072         ; too large: LUI + ORI
    J     end                ; JAL c0, end
    RET                      ; JALR c0, c1, 0
end:
    HALT
