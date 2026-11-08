; c0 is the null capability. JALR to it is a TAG fault.
; expect fault TAG at 0x00
    JALR  c0, c0, 0
