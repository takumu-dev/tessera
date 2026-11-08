; Directives: .equ, .word, .byte, .ascii, .align and labels in values.
; expect word 0x00 = 0x78000000
; expect word 0x04 = 100
; expect word 0x08 = 8
; expect word 0x0C = 0x00FF0201
; expect word 0x10 = 0x000A6261
; expect word 0x14 = 0x10
.equ COUNT, 100
start:
    HALT
data:
    .word COUNT
    .word data + 4
    .byte 1, 2, 0xFF
    .align 4
text:
    .ascii "ab\n"
    .word text               ; aligned to 0x14
