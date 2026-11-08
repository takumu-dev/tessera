; A reserved opcode (0x3F) causes an ILLEGAL fault.
; expect fault ILLEGAL at 0x04
    NOP
    .word 0xFC000000
