; Shifts and compares.
; expect r4 = 20
; expect r5 = 0x3FFFFFFD
; expect r6 = -3
; expect r7 = 0x80000000
; expect r8 = 0xF
; expect r9 = -5
; expect r10 = 1
; expect r11 = 0
; expect r12 = 0
; expect r13 = 1
; expect r15 = 10
    LI    r1, -10
    ADDI  r2, r0, 2
    ADDI  r3, r0, 5
    SLL   r4, r3, r2
    SRL   r5, r1, r2
    SRA   r6, r1, r2
    SLLI  r7, r3, 31
    SRLI  r8, r1, 28
    SRAI  r9, r1, 1
    SLT   r10, r1, r3        ; signed: -10 < 5
    SLTU  r11, r1, r3        ; unsigned: 0xFFFFFFF6 < 5 is false
    SLTI  r12, r1, -20
    SLTI  r13, r3, 6
    ADDI  r14, r0, 33
    SLL   r15, r3, r14       ; only bits 4:0 count: shift by 1
    HALT
