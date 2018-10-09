.text
	addiu	$a0, $0, 1
	sll	$a1, $a1, 2
	srl	$a1, $a1, 2
	and	$a1, $a0, $0
	andi	$a1, $a0, 0
	or	$a1, $a0, $0
	ori	$a1, $a0, 0
	lui	$a1, 3
	slt	$a1, $a0, $0
	sw	$a0, 0($sp)
	addiu	$sp, $sp, 4
	lw	$a0, -4($sp)
	addiu	$sp, $sp, -4
	jal	Mystery
	addi	$0, $0, 0 #unsupported instruction, terminate

Mystery:
	addiu	$v0, $0,0
Loop:
	beq	$a0, $0, Done
	addu	$v0, $v0, $a1
	subu	$a0, $a0, 1
	j	Loop
Done:	
	jr	$ra

