; Division by zero is a fault.
; expect fault DIVZERO at 0x08
    ADDI  r1, r0, 5
    ADDI  r2, r0, 0
    DIV   r3, r1, r2
    HALT
