.intel_syntax noprefix
.extern printf
.extern puts

.section .rdata,"dr"
zenasm_fmt_i64:
    .asciz "%lld\n"
zenasm_str_0:
    .asciz "ZenAsm package ready"

.text
.globl banner
banner:
    push rbp
    mov rbp, rsp
    push rbx
    sub rsp, 8
banner.entry.0:
    # line 2:     return "ZenAsm package ready"
    lea rbx, [rip + zenasm_str_0]
    mov rax, rbx
    jmp banner.exit
banner.exit:
    add rsp, 8
    pop rbx
    pop rbp
    ret

.globl sum_window
sum_window:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    sub rsp, 48
    mov qword ptr [rbp - 24], rcx
    mov qword ptr [rbp - 32], rdx
sum_window.entry.0:
    # line 5:     let total = 0
    mov rbx, 0
    mov qword ptr [rbp - 40], rbx
    # line 6:     for i in range(start, finish):
    mov rbx, qword ptr [rbp - 24]
    mov qword ptr [rbp - 48], rbx
    mov rbx, qword ptr [rbp - 32]
    mov qword ptr [rbp - 56], rbx
    mov rbx, 1
    mov qword ptr [rbp - 64], rbx
    jmp sum_window.for.dispatch.1
sum_window.for.dispatch.1:
    mov r12, qword ptr [rbp - 64]
    mov rbx, 0
    mov r10, r12
    mov r11, rbx
    cmp r10, r11
    setg al
    movzx r10, al
    mov rbx, r10
    mov r10, rbx
    cmp r10, 0
    jne sum_window.for.cond.pos.2
    jmp sum_window.for.cond.neg.3
sum_window.for.cond.pos.2:
    mov r12, qword ptr [rbp - 48]
    mov rbx, qword ptr [rbp - 56]
    mov r10, r12
    mov r11, rbx
    cmp r10, r11
    setl al
    movzx r10, al
    mov rbx, r10
    mov r10, rbx
    cmp r10, 0
    jne sum_window.for.body.4
    jmp sum_window.for.end.6
sum_window.for.cond.neg.3:
    mov r12, qword ptr [rbp - 48]
    mov rbx, qword ptr [rbp - 56]
    mov r10, r12
    mov r11, rbx
    cmp r10, r11
    setg al
    movzx r10, al
    mov rbx, r10
    mov r10, rbx
    cmp r10, 0
    jne sum_window.for.body.4
    jmp sum_window.for.end.6
sum_window.for.body.4:
    # line 7:         if i == 3:
    mov r12, qword ptr [rbp - 48]
    mov rbx, 3
    mov r10, r12
    mov r11, rbx
    cmp r10, r11
    sete al
    movzx r10, al
    mov rbx, r10
    mov r10, rbx
    cmp r10, 0
    jne sum_window.if.then.7
    jmp sum_window.if.else.8
sum_window.for.inc.5:
    # line 6:     for i in range(start, finish):
    mov r12, qword ptr [rbp - 48]
    mov rbx, qword ptr [rbp - 64]
    mov r10, r12
    mov r11, rbx
    add r10, r11
    mov rbx, r10
    mov qword ptr [rbp - 48], rbx
    jmp sum_window.for.dispatch.1
sum_window.for.end.6:
    # line 10:     return total
    mov rbx, qword ptr [rbp - 40]
    mov rax, rbx
    jmp sum_window.exit
sum_window.if.then.7:
    # line 8:             continue
    jmp sum_window.for.inc.5
sum_window.if.else.8:
    # line 7:         if i == 3:
    jmp sum_window.if.end.9
sum_window.if.end.9:
    # line 9:         total = total + i
    mov r12, qword ptr [rbp - 40]
    mov rbx, qword ptr [rbp - 48]
    mov r10, r12
    mov r11, rbx
    add r10, r11
    mov rbx, r10
    mov qword ptr [rbp - 40], rbx
    # line 6:     for i in range(start, finish):
    jmp sum_window.for.inc.5
sum_window.exit:
    add rsp, 48
    pop r12
    pop rbx
    pop rbp
    ret

.globl main
main:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    sub rsp, 64
main.entry.0:
    # line 13:     let total = sum_window(1, 7)
    mov r12, 1
    mov rbx, 7
    mov rcx, r12
    mov rdx, rbx
    call sum_window
    mov rbx, rax
    mov qword ptr [rbp - 24], rbx
    # line 14:     for j in range(10, 4, -2):
    mov rbx, 10
    mov qword ptr [rbp - 32], rbx
    mov rbx, 4
    mov qword ptr [rbp - 40], rbx
    mov rbx, -2
    mov qword ptr [rbp - 48], rbx
    jmp main.for.dispatch.1
main.for.dispatch.1:
    mov rbx, qword ptr [rbp - 48]
    mov r12, 0
    mov r10, rbx
    mov r11, r12
    cmp r10, r11
    setg al
    movzx r10, al
    mov rbx, r10
    mov r10, rbx
    cmp r10, 0
    jne main.for.cond.pos.2
    jmp main.for.cond.neg.3
main.for.cond.pos.2:
    mov r12, qword ptr [rbp - 32]
    mov rbx, qword ptr [rbp - 40]
    mov r10, r12
    mov r11, rbx
    cmp r10, r11
    setl al
    movzx r10, al
    mov rbx, r10
    mov r10, rbx
    cmp r10, 0
    jne main.for.body.4
    jmp main.for.end.6
main.for.cond.neg.3:
    mov r12, qword ptr [rbp - 32]
    mov rbx, qword ptr [rbp - 40]
    mov r10, r12
    mov r11, rbx
    cmp r10, r11
    setg al
    movzx r10, al
    mov rbx, r10
    mov r10, rbx
    cmp r10, 0
    jne main.for.body.4
    jmp main.for.end.6
main.for.body.4:
    # line 15:         total = total + 1
    mov r12, qword ptr [rbp - 24]
    mov rbx, 1
    mov r10, r12
    mov r11, rbx
    add r10, r11
    mov rbx, r10
    mov qword ptr [rbp - 24], rbx
    # line 14:     for j in range(10, 4, -2):
    jmp main.for.inc.5
main.for.inc.5:
    mov r12, qword ptr [rbp - 32]
    mov rbx, qword ptr [rbp - 48]
    mov r10, r12
    mov r11, rbx
    add r10, r11
    mov rbx, r10
    mov qword ptr [rbp - 32], rbx
    jmp main.for.dispatch.1
main.for.end.6:
    # line 17:     asm(total):
    add qword ptr [rbp - 24], 5
    # line 20:     print(banner())
    call banner
    mov rbx, rax
    mov rcx, rbx
    call puts
    # line 21:     print(total)
    mov rbx, qword ptr [rbp - 24]
    mov rdx, rbx
    lea rcx, [rip + zenasm_fmt_i64]
    xor eax, eax
    call printf
    # line 22:     return total
    mov rbx, qword ptr [rbp - 24]
    mov rax, rbx
    jmp main.exit
main.exit:
    add rsp, 64
    pop r12
    pop rbx
    pop rbp
    ret

