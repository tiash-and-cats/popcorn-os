bits 64
default rel
global pop_main

section .text
pop_main:
    ; Align stack and reserve shadow space
    sub rsp, 40          ; 32 bytes shadow + 8 for alignment

    call myfn            ; myfn(rcx <svc>)

    add rsp, 40          ; restore stack
    mov rax, 0           ; return 0
    ret                  ; return

myfn:
    ; Align stack and reserve shadow space
    sub rsp, 40          ; 32 bytes shadow + 8 for alignment
    
    mov rbx, rcx         ; rbx = rcx <svc>
    
    ; call svc->print
    mov rax, [rbx]       ; rax = rbx->print
    mov rcx, rbx         ; rcx = rbx
    lea rdx, [msg]       ; rdx = &msg
    call rax             ; rax(rbx, rdx);

    add rsp, 40          ; restore stack
    mov rax, 0           ; return 0
    ret                  ; return

section .data
msg: dw 'H','e','l','l','o',' ','f','r','o','m',' ','N','A','S','M',' ','a','p','p','!',13,10,0