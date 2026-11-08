; Code that runs past the end of local memory stops with a BOUNDS fault.
; expect fault BOUNDS at 0x10000
    J     0x10000
