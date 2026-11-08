; Spec example 1. The words must match the listing in the spec.
; expect word 0x00 = 0x04400004
; expect word 0x04 = 0x70040000
; expect word 0x08 = 0x1C82885A
; expect word 0x0C = 0x0C880148
; expect word 0x10 = 0x70084000
; expect word 0x14 = 0x70200002
; expect word 0x18 = 0x78000000
; expect output "Hi!\n"
; expect halt
start:
    ADDI  r1, r0, 4          ; 4 bytes
    MPUT  0, r1              ; w0 = byte count
    LI    r2, 0x0A216948     ; "Hi!\n" (LUI + ORI)
    MPUT  1, r2              ; w1 = the 4 bytes
    SEND  c8                 ; send to the console
    HALT
