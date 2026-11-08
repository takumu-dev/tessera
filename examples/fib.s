; Print the first 20 Fibonacci numbers, one on each line.
; Shows function calls (JAL and RET) and decimal output.
;
; expect output "0\n1\n1\n2\n3\n5\n8\n13\n21\n34\n"
; expect output "55\n89\n144\n233\n377\n610\n987\n1597\n2584\n4181\n"
; expect halt

start:
    ADDI  r10, r0, 0         ; a = 0
    ADDI  r11, r0, 1         ; b = 1
    ADDI  r12, r0, 20        ; count
loop:
    MV    r1, r10
    JAL   c1, print_u32
    ADDI  r1, r0, 10         ; '\n'
    JAL   c1, put_char
    ADD   r13, r10, r11      ; t = a + b
    MV    r10, r11
    MV    r11, r13
    ADDI  r12, r12, -1
    BNE   r12, r0, loop
    HALT

; put_char: print the character in r1. Uses r5.
put_char:
    ADDI  r5, r0, 1
    MPUT  0, r5              ; 1 byte
    MPUT  1, r1
    SEND  c8
    RET

; print_u32: print r1 as an unsigned decimal number. Uses r1, r5-r9.
print_u32:
    LI    r5, 1000000000     ; p = the largest power of 10
    ADDI  r6, r0, 0          ; started = 0
    ADDI  r9, r0, 1          ; byte count
pu_loop:
    DIVU  r7, r1, r5         ; d = n / p
    REMU  r1, r1, r5         ; n = n % p
    BNE   r7, r0, pu_print
    BNE   r6, r0, pu_print
    ADDI  r8, r0, 1
    BEQ   r5, r8, pu_print   ; always print the last digit
    J     pu_next
pu_print:
    ADDI  r6, r0, 1
    ADDI  r7, r7, '0'
    MPUT  0, r9
    MPUT  1, r7
    SEND  c8
pu_next:
    ADDI  r8, r0, 10
    DIVU  r5, r5, r8         ; p = p / 10
    BNE   r5, r0, pu_loop
    RET
