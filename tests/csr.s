; Control and status registers on tile (0,0).
; expect r1 = 0
; expect r2 = 0x0101
; expect r3 = 65536
; expect r4 = 3
    CSRR  r1, TILEID
    CSRR  r2, GRIDDIM
    CSRR  r3, LOCALSIZE
    CSRR  r4, INSTRET        ; 3 instructions are complete
    HALT
