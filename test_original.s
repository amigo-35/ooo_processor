.A: 10 20 30
.B: 40 50 60
lw x1, A(x0)
lw x2, B(x0)
addi x3, x0, 5
addi x4, x0, 0
loop:
    add x5, x3, x4
    addi x4, x4, 1
    beq x4, x3, loop
    j end
    addi x6, x0, 99
end:
    addi x7, x0, 1
