.intel_syntax noprefix
.extern printf
.extern external_add

.section .rdata,"dr"
zenasm_fmt_i64:
    .asciz "%lld\n"

.text
.globl main
main:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    sub rsp, 48
main.entry.0:
    # line 4:     let base = external_add(20, 22)
    mov r12, 20
    mov rbx, 22
    mov rcx, r12
    mov rdx, rbx
    call external_add
    mov rbx, rax
    mov qword ptr [rbp - 24], rbx
    # line 5:     asm:
    nop
    nop
    # line 8:     print(base)
    mov rbx, qword ptr [rbp - 24]
    mov rdx, rbx
    lea rcx, [rip + zenasm_fmt_i64]
    xor eax, eax
    call printf
    # line 9:     return base
    mov rbx, qword ptr [rbp - 24]
    mov rax, rbx
    jmp main.exit
main.exit:
    add rsp, 48
    pop r12
    pop rbx
    pop rbp
    ret

