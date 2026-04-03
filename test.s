lw x1, 0(x0)
lw x2, 3(x0)
addi x3, x0, 5
addi x4, x0, 0
add x5, x3, x4
addi x4, x4, 1
beq x4, x3, -2
j 2
addi x6, x0, 99
addi x7, x0, 1
