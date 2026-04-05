.intel_syntax noprefix
.extern printf

.section .rdata,"dr"
zenasm_fmt_i64:
    .asciz "%lld\n"

.text
.globl main
main:
    push rbp
    mov rbp, rsp
    push rbx
    sub rsp, 40
main.entry.0:
    # line 4:         total = total + 10
    mov rbx, 40
    mov qword ptr [rbp - 16], rbx
    # line 6:     if total > 20:
    jmp main.if.then.1
main.if.then.1:
    # line 7:         print(total)
    mov rbx, qword ptr [rbp - 16]
    mov rdx, rbx
    lea rcx, [rip + zenasm_fmt_i64]
    xor eax, eax
    call printf
    # line 6:     if total > 20:
    jmp main.if.end.3
main.if.end.3:
    # line 11:     return total
    mov rbx, qword ptr [rbp - 16]
    mov rax, rbx
    jmp main.exit
main.exit:
    add rsp, 40
    pop rbx
    pop rbp
    ret

