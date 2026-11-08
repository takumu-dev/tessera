; Print "Hi!" on the console. This is example 1 in the spec.
; c8 = console channel (boot state of the supervisor tile)
;
; expect output "Hi!\n"
; expect halt
start:
    ADDI  r1, r0, 4          ; 4 bytes
    MPUT  0, r1              ; w0 = byte count
    LI    r2, 0x0A216948     ; "Hi!\n" as a little-endian word
    MPUT  1, r2              ; w1 = the 4 bytes
    SEND  c8                 ; send to the console
    HALT
