; The data root c3 has no EXEC permission, so JALR to it is a fault.
; expect fault PERM at 0x00
    JALR  c0, c3, 0
