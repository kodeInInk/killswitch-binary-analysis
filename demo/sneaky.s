#sneaky: no libc, no imports, only raw syscalls
#ignores SIGTERM, forks, and the child leaves the process group
.intel_syntax noprefix
.globl _start

.data
act: .quad 1, 0, 0, 0          #sa_handler = SIG_IGN

.text
_start:
    mov eax, 13                #rt_sigaction
    mov edi, 15                #SIGTERM
    lea rsi, [rip + act]
    xor edx, edx
    mov r10d, 8                #sizeof(sigset_t)
    syscall

    mov eax, 57                #fork
    syscall
    test eax, eax
    jnz wait                   #parent skips setsid

    mov eax, 112               #setsid
    syscall

wait:
    mov eax, 34                #pause
    syscall
    jmp wait
