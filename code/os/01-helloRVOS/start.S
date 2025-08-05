#include "platform.h"

	# size of each hart's stack is 1024 bytes
	.equ	STACK_SIZE, 1024

	.global	_start

	.text
_start:
	# park harts with id != 0
	// 在單核心處理器中, mhartid 為 0
	csrr	t0, mhartid		# read current hart id
	mv	tp, t0			# keep CPU's hartid in its tp for later usage.// 這裡就是將 mhartid 存起來，供後續使用。
	bnez	t0, park		# if we're not on the hart 0 //這個步驟是整個啟動代碼的關鍵，它有效地將核心 0 和其他核心分開。

					# we park the hart
	# Setup stacks, the stack grows from bottom to top, so we put the
	# stack pointer to the very end of the stack range.
	slli	t0, t0, 10		# shift left the hart id by 1024 //所以這條指令是在計算 hart_id * 1024，也就是每個核心專屬的 1KB 堆疊空間。因為核心 0 的 ID 是 0，所以偏移量為 0。
	la	sp, stacks + STACK_SIZE	# set the initial stack pointer
					# to the end of the first stack space //這條指令將堆疊空間的基地址加上預設的堆疊大小 (STACK_SIZE)，得到第一個堆疊的末尾地址，並將其載入到**sp (Stack Pointer)** 暫存器。
	/*
	程式將剛剛計算出的堆疊偏移量 t0 (此時為 0) 加到 sp 上。
    這行程式碼確保每個核心都有自己獨立的堆疊空間。因為核心 0 的偏移量是 0，所以 sp 保持不變，指向第一個堆疊的末尾。
	*/
	add	sp, sp, t0		# move the current hart stack pointer
					# to its place in the stack space

	j	start_kernel		# hart 0 jump to c

park:
	wfi
	j	park

	# In the standard RISC-V calling convention, the stack pointer sp
	# is always 16-byte aligned.
.balign 16
stacks:
	.skip	STACK_SIZE * MAXNUM_CPU # allocate space for all the harts stacks

	.end				# End of file
