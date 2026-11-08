; Loops and branches, signed and unsigned.
; expect r1 = 5050
; expect r5 = 1
; expect r6 = 1
; expect halt
    ADDI  r1, r0, 0          ; sum
    ADDI  r2, r0, 1          ; i
    ADDI  r3, r0, 100
loop:
    ADD   r1, r1, r2
    ADDI  r2, r2, 1
    BGE   r3, r2, loop       ; while i <= 100
    ADDI  r4, r0, -1         ; the largest unsigned number
    BLTU  r4, r3, wrong      ; must not branch
    BGEU  r4, r3, ok1
    J     wrong
ok1:
    ADDI  r5, r0, 1
    BLT   r4, r3, ok2        ; signed: -1 < 100
    J     wrong
ok2:
    ADDI  r6, r0, 1
    BEQ   r0, r0, done
wrong:
    BREAK
done:
    HALT
