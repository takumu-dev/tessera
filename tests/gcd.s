; Euclid's algorithm with REMU.
; expect r1 = 21
    LI    r1, 1071
    LI    r2, 462
loop:
    BEQ   r2, r0, done
    REMU  r3, r1, r2
    MV    r1, r2
    MV    r2, r3
    J     loop
done:
    HALT
