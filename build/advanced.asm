.intel_syntax noprefix
.extern printf

.section .rdata,"dr"
zenasm_fmt_i64:
    .asciz "%lld\n"

.text
.globl sum8
sum8:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    sub rsp, 64
    mov qword ptr [rbp - 24], rcx
    mov qword ptr [rbp - 32], rdx
    mov qword ptr [rbp - 40], r8
    mov qword ptr [rbp - 48], r9
    mov r10, qword ptr [rbp + 48]
    mov qword ptr [rbp - 56], r10
    mov r10, qword ptr [rbp + 56]
    mov qword ptr [rbp - 64], r10
    mov r10, qword ptr [rbp + 64]
    mov qword ptr [rbp - 72], r10
    mov r10, qword ptr [rbp + 72]
    mov qword ptr [rbp - 80], r10
sum8.entry.0:
    # line 2:     return a + b + c + d + e + f + g + h
    mov r12, qword ptr [rbp - 24]
    mov rbx, qword ptr [rbp - 32]
    mov r10, r12
    mov r11, rbx
    add r10, r11
    mov r12, r10
    mov rbx, qword ptr [rbp - 40]
    mov r10, r12
    mov r11, rbx
    add r10, r11
    mov r12, r10
    mov rbx, qword ptr [rbp - 48]
    mov r10, r12
    mov r11, rbx
    add r10, r11
    mov r12, r10
    mov rbx, qword ptr [rbp - 56]
    mov r10, r12
    mov r11, rbx
    add r10, r11
    mov r12, r10
    mov rbx, qword ptr [rbp - 64]
    mov r10, r12
    mov r11, rbx
    add r10, r11
    mov r12, r10
    mov rbx, qword ptr [rbp - 72]
    mov r10, r12
    mov r11, rbx
    add r10, r11
    mov r12, r10
    mov rbx, qword ptr [rbp - 80]
    mov r10, r12
    mov r11, rbx
    add r10, r11
    mov rbx, r10
    mov rax, rbx
    jmp sum8.exit
sum8.exit:
    add rsp, 64
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
    push r13
    push r14
    push r15
    sub rsp, 88
main.entry.0:
    # line 5:     let total = 0
    mov rbx, 0
    mov qword ptr [rbp - 48], rbx
    # line 6:     let i = 0
    mov rbx, 0
    mov qword ptr [rbp - 56], rbx
    # line 8:     while i < 10:
    jmp main.while.cond.1
main.while.cond.1:
    mov r12, qword ptr [rbp - 56]
    mov rbx, 10
    mov r10, r12
    mov r11, rbx
    cmp r10, r11
    setl al
    movzx r10, al
    mov rbx, r10
    mov r10, rbx
    cmp r10, 0
    jne main.while.body.2
    jmp main.while.end.3
main.while.body.2:
    # line 9:         i = i + 1
    mov r12, qword ptr [rbp - 56]
    mov rbx, 1
    mov r10, r12
    mov r11, rbx
    add r10, r11
    mov rbx, r10
    mov qword ptr [rbp - 56], rbx
    # line 10:         if i == 2:
    mov r12, qword ptr [rbp - 56]
    mov rbx, 2
    mov r10, r12
    mov r11, rbx
    cmp r10, r11
    sete al
    movzx r10, al
    mov rbx, r10
    mov r10, rbx
    cmp r10, 0
    jne main.if.then.4
    jmp main.if.else.5
main.while.end.3:
    # line 16:     let combined = sum8(total, 1, 2, 3, 4, 5, 6, 7)
    mov r10, qword ptr [rbp - 48]
    mov qword ptr [rbp - 88], r10
    mov r10, 1
    mov qword ptr [rbp - 80], r10
    mov r10, 2
    mov qword ptr [rbp - 72], r10
    mov r15, 3
    mov r14, 4
    mov r13, 5
    mov r12, 6
    mov rbx, 7
    sub rsp, 64
    mov qword ptr [rsp + 32], r14
    mov qword ptr [rsp + 40], r13
    mov qword ptr [rsp + 48], r12
    mov qword ptr [rsp + 56], rbx
    mov rcx, qword ptr [rbp - 88]
    mov rdx, qword ptr [rbp - 80]
    mov r8, qword ptr [rbp - 72]
    mov r9, r15
    call sum8
    add rsp, 64
    mov rbx, rax
    mov qword ptr [rbp - 64], rbx
    # line 17:     print(combined)
    mov rbx, qword ptr [rbp - 64]
    mov rdx, rbx
    lea rcx, [rip + zenasm_fmt_i64]
    xor eax, eax
    call printf
    # line 18:     return combined
    mov rbx, qword ptr [rbp - 64]
    mov rax, rbx
    jmp main.exit
main.if.then.4:
    # line 11:             continue
    jmp main.while.cond.1
main.if.else.5:
    # line 12:         elif i == 9:
    mov r12, qword ptr [rbp - 56]
    mov rbx, 9
    mov r10, r12
    mov r11, rbx
    cmp r10, r11
    sete al
    movzx r10, al
    mov rbx, r10
    mov r10, rbx
    cmp r10, 0
    jne main.if.then.7
    jmp main.if.else.8
main.if.end.6:
    # line 14:         total = total + i
    mov r12, qword ptr [rbp - 48]
    mov rbx, qword ptr [rbp - 56]
    mov r10, r12
    mov r11, rbx
    add r10, r11
    mov rbx, r10
    mov qword ptr [rbp - 48], rbx
    # line 8:     while i < 10:
    jmp main.while.cond.1
main.if.then.7:
    # line 13:             break
    jmp main.while.end.3
main.if.else.8:
    # line 12:         elif i == 9:
    jmp main.if.end.9
main.if.end.9:
    # line 10:         if i == 2:
    jmp main.if.end.6
main.exit:
    add rsp, 88
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret

