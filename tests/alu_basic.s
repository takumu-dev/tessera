; ADD, SUB, AND, OR, XOR, the immediate forms, LUI and LI.
; expect r3 = 22
; expect r4 = -2
; expect r5 = 8
; expect r6 = 14
; expect r7 = 6
; expect r8 = -8
; expect r9 = 2
; expect r10 = 0x10B
; expect r11 = 0xF3
; expect r12 = 0xFFFFFC00
; expect r13 = 0x12345678
; expect r0 = 0
; expect halt
    ADDI  r1, r0, 12
    ADDI  r2, r0, 10
    ADD   r3, r1, r2
    SUB   r4, r2, r1
    AND   r5, r1, r2
    OR    r6, r1, r2
    XOR   r7, r1, r2
    ADDI  r8, r1, -20
    ANDI  r9, r2, 0x3
    ORI   r10, r2, 0x101
    XORI  r11, r1, 0xFF
    LUI   r12, 0x3FFFFF
    LI    r13, 0x12345678
    ADDI  r0, r0, 5          ; r0 stays 0
    HALT
