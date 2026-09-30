mov rax, 5
bucla:
print_rax
sub rax, 1
cmp rax, 0
je gata
jmp bucla
gata:
ret