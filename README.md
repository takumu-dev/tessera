# Tessera simulator, milestone 1

This is the first simulator for the Tessera v0.1 architecture. It runs one core (tile 0,0, the supervisor) and includes an assembler, a disassembler and a test runner. All code is C11, with no dependencies.

## Build and test

You need a C compiler and `make`. On Windows, use MSYS2 or WSL.

```
make
make test
```

`make test` assembles and runs each file in `tests/` and `examples/`, and compares the results with the expectations in the file.

## Tools

| Command | What it does |
| --- | --- |
| `./tsim program.s` | Assembles and runs a program. Console output goes to stdout. |
| `./tsim -t program.s` | Same, with a trace line for each instruction (to stderr). |
| `./tsim -d program.s` | Same, with a dump of all registers at the end. |
| `./tasm -l program.s` | Assembles to `program.bin` and prints a listing. |
| `./tdis program.bin` | Disassembles a binary image. |
| `./ttest file.s ...` | Runs test files. |

`tsim` also accepts `-n steps` (step limit) and `-m bytes` (local memory size). It also runs `.bin` images.

Exit status of `tsim`: 0 = halted, 1 = fault, 2 = usage or load error, 3 = unsupported instruction, 4 = step limit.

## What milestone 1 does

- All integer instructions, branches, JAL and JALR.
- JALR checks the target capability: tag, LOCAL space and EXEC.
- Instruction fetch checks pcc: tag, space, EXEC, bounds and alignment.
- HALT, CSRR and BREAK.
- A temporary console: MPUT, MPUTC and SEND to the console endpoint print bytes. SEND does the full capability checks and the SENDCAP check.
- Faults stop the core and print a report.
- The assembler knows all 73 instructions of v0.1.

The simulator stops in the UNSUPPORTED state for:

- Memory and capability instructions (milestone 2).
- RECV, TRYRECV, MGET, MGETC, WAIT, and SEND to a tile (milestone 3).
- DMA (milestone 4).

## Boot state

The simulator runs the supervisor tile, so it has these capabilities at reset:

| Register | Capability |
| --- | --- |
| pcc | LOCAL, all local memory, R + X + LC, cursor 0 |
| c3 | LOCAL data root, R + W + LC + SC |
| c4 | MAILBOX root, mailboxes 0-15, RECV |
| c5 | CHANNEL to the supervisor, mailbox 0 |
| c6 | GLOBAL root |
| c7 | CHANNEL root, all endpoints |
| c8 | CHANNEL to the console |

## Assembler syntax

```
; A comment starts with ; or #
.equ COUNT, 10          ; a constant
start:                  ; a label
    ADDI  r1, r0, COUNT
    LI    r2, 0x12345678
    BNE   r1, r2, start
    LW    r3, 8(c3)     ; offset(capability)
    HALT
```

- Mnemonics and register names: upper or lower case.
- Numbers: decimal, `0x` hex, `0b` binary, `'A'` characters. Simple sums such as `label + 4` are permitted.
- A branch or jump target is a label or an absolute address.
- Directives: `.equ`, `.word`, `.byte`, `.ascii "text"`, `.space n`, `.align n`.
- Pseudo-instructions: `LI rd, imm32`, `MV rd, rs`, `NOP`, `J label`, `RET`.
- The assembler aligns each instruction and `.word` to 4 bytes.

## Test files

A test is an assembly file with expectations in comments:

```
; expect halt                    (default)
; expect fault BOUNDS at 0x14
; expect unsupported at 0x00
; expect r3 = -10
; expect word 0x08 = 0x1C82885A
; expect output "Hi!\n"          (more lines are joined)
; expect limit 5000              (step limit, default 1000000)
```

## Files

```
src/isa.c    instruction table, decoder, disassembler
src/asm.c    two-pass assembler
src/core.c   one core: execute, checks, faults, trace
src/tsim.c   simulator command
src/tasm.c   assembler command
src/tdis.c   disassembler command
src/ttest.c  test runner
tests/       one test for each feature and each fault
examples/    hello.s, fib.s
```

## Next: milestone 2

Add the memory instructions, local memory tags and all capability instructions.
