; void *memcpy(void *dst, const void *src, size_t n)
;
; rep - repeat smth RCX times
; MOVSB - move byte from address RSI to RDI.
;
global memcpy
memcpy:
    mov rax, rdi ; ret value
    mov rcx, rdx
    rep movsb
    ret

; void *memmove(void *dst, const void *src, size_t n)
global memmove
memmove:
    mov rax, rdi ; ret value
    mov rcx, rdx
    cmp rdi, rsi
    jb .forward

    ; backward copy
    lea rdi, [rdi + rcx - 1]
    lea rsi, [rsi + rcx - 1]
    std
    rep movsb
    cld
    ret
.forward:
    rep movsb
    ret

; void *memset(void *dst, int c, size_t n)
;
; rep - repeat smth RCX times
; STOSB - store AL at address RDI
;
global memset
memset:
    mov r9, rdi ; save ret value
    mov al, sil
    mov rcx, rdx
    rep stosb
    mov rax, r9 ; restore ret value
    ret
