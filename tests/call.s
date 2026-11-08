; JAL and RET. The function doubles r1.
; expect r1 = 40
; expect r2 = 2
; expect halt
    ADDI  r1, r0, 10
    ADDI  r2, r0, 0          ; call counter
    JAL   c1, double
    JAL   c1, double
    HALT
double:
    ADD   r1, r1, r1
    ADDI  r2, r2, 1
    RET
