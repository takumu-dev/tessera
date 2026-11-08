; MUL, MULH, MULHU, DIV, DIVU, REM, REMU and the signed overflow case.
; expect r4 = -70
; expect r5 = -1
; expect r6 = 6
; expect r7 = 3
; expect r8 = -1
; expect r9 = 613566755
; expect r10 = 1
; expect r13 = 0x80000000
; expect r14 = 0
; expect r15 = -2
    LI    r1, -10
    LI    r2, -3
    LI    r3, 7
    MUL   r4, r1, r3         ; -70
    MULH  r5, r1, r3         ; high word of -70 = -1
    MULHU r6, r1, r3         ; 0xFFFFFFF6 * 7 = 0x6_FFFFFFBA
    DIV   r7, r1, r2         ; -10 / -3 = 3
    REM   r8, r1, r2         ; -10 % -3 = -1
    DIVU  r9, r1, r3         ; 0xFFFFFFF6 / 7
    REMU  r10, r1, r3        ; 0xFFFFFFF6 % 7
    LI    r11, 0x80000000
    LI    r12, -1
    DIV   r13, r11, r12      ; overflow: 0x80000000
    REM   r14, r11, r12      ; overflow: 0
    DIV   r15, r3, r2        ; 7 / -3 = -2 (toward zero)
    HALT
