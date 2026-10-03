; ===================================================
; FRACTAL JULIA - Randare in virgula mobila
; Test de stres pentru FPU si jump-uri conditionale
; ===================================================

print "=== FRACTAL JULIA ==="
newline

loop_y:
    ; Resetam X la inceputul fiecarei linii
    movss xmm0, [c_x_start]
    movss [x_val], xmm0
    
loop_x:
    ; Initializam Z = X + iY
    movss xmm0, [x_val]
    movss [zx], xmm0
    movss xmm0, [y_val]
    movss [zy], xmm0
    
    ; iter = 0
    movss xmm0, [zero]
    movss [iter], xmm0
    
loop_iter:
    ; Calculam patratele: zx2 = zx * zx
    movss xmm0, [zx]
    mulss xmm0, [zx]
    movss [zx2], xmm0
    
    ; zy2 = zy * zy
    movss xmm0, [zy]
    mulss xmm0, [zy]
    movss [zy2], xmm0
    
    ; Daca (zx2 + zy2) > 4.0, am scapat din fractal!
    movss xmm0, [zx2]
    addss xmm0, [zy2]
    ucomiss xmm0, [four]
    ja end_loop_iter
    
    ; tmp = 2 * zx * zy + cy (noul zy)
    movss xmm0, [two]
    mulss xmm0, [zx]
    mulss xmm0, [zy]
    addss xmm0, [cy]
    movss [tmp], xmm0
    
    ; zx = zx2 - zy2 + cx (noul zx)
    movss xmm0, [zx2]
    subss xmm0, [zy2]
    addss xmm0, [cx]
    movss [zx], xmm0
    
    ; Actualizam zy cu valoarea salvata
    movss xmm0, [tmp]
    movss [zy], xmm0
    
    ; iter = iter + 1
    movss xmm0, [iter]
    addss xmm0, [one]
    movss [iter], xmm0
    
    ; Daca iter < max_iter, continuam
    ucomiss xmm0, [max_iter]
    jb loop_iter
    
end_loop_iter:
    ; Verificam cum am iesit din bucla
    movss xmm0, [iter]
    ucomiss xmm0, [max_iter]
    jb print_out
    
    ; A ajuns la iteratia maxima -> Este IN fractal (#)
    print [str_in]
    jmp next_x
    
print_out:
    ; A scapat repede -> Este OUT (.)
    print [str_out]
    
next_x:
    ; x_val = x_val + x_step
    movss xmm0, [x_val]
    addss xmm0, [x_step]
    movss [x_val], xmm0
    ucomiss xmm0, [x_max]
    jb loop_x
    
    ; SFÂRȘIT DE RÂND: Folosim un singur newline curat
    newline
    
    ; y_val = y_val + y_step
    movss xmm0, [y_val]
    addss xmm0, [y_step]
    movss [y_val], xmm0
    
    ; Verificăm dacă am ajuns la y_max ÎNAINTE să repetăm bucla y
    ucomiss xmm0, [y_max]
    jb loop_y
    
exit

; --- BAZA DE DATE OPTIMIZATĂ PENTRU 80x25 VGA ---
y_val:
    data -1.0
y_max:
    data 1.0
y_step:
    data 0.08        ; Pasul vertical reglat fin

c_x_start:
    data -1.5
x_val:
    data -1.5
x_max:
    data 1.5
x_step:
    data 0.045       ; ~66 caractere pe rând (fără wrap lateral)
    
; Constanta magică pentru Julia Set (c = -0.8 + 0.156i)
cx:
    data -0.8
cy:
    data 0.156
    
; Variabile de calcul
zx:
    data 0.0
zy:
    data 0.0
zx2:
    data 0.0
zy2:
    data 0.0
tmp:
    data 0.0
iter:
    data 0.0
    
; Constante
zero:
    data 0.0
one:
    data 1.0
two:
    data 2.0
four:
    data 4.0
max_iter:
    data 20.0
    
; Șiruri terminate corect cu zero pentru funcția print
str_in:
    data "#"
    data 0
str_out:
    data "."
    data 0