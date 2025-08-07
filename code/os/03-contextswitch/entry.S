#define LOAD		lw
#define STORE		sw
#define SIZE_REG	4

# Save all General-Purpose(GP) registers to context.
# struct context *base = &ctx_task;
# base->ra = ra;
# ......
# These GP registers to be saved don't include gp
# and tp, because they are not caller-saved or
# callee-saved. These two registers are often used
# for special purpose. For example, in RVOS, 'tp'
# (aka "thread pointer") is used to store hartid,
# which is a global value and would not be changed
# during context-switch.
.macro reg_save base
	STORE ra,   0*SIZE_REG(\base)
	STORE sp,   1*SIZE_REG(\base)
	STORE t0,   4*SIZE_REG(\base)
	STORE t1,   5*SIZE_REG(\base)
	STORE t2,   6*SIZE_REG(\base)
	STORE s0,   7*SIZE_REG(\base)
	STORE s1,   8*SIZE_REG(\base)
	STORE a0,   9*SIZE_REG(\base)
	STORE a1,  10*SIZE_REG(\base)
	STORE a2,  11*SIZE_REG(\base)
	STORE a3,  12*SIZE_REG(\base)
	STORE a4,  13*SIZE_REG(\base)
	STORE a5,  14*SIZE_REG(\base)
	STORE a6,  15*SIZE_REG(\base)
	STORE a7,  16*SIZE_REG(\base)
	STORE s2,  17*SIZE_REG(\base)
	STORE s3,  18*SIZE_REG(\base)
	STORE s4,  19*SIZE_REG(\base)
	STORE s5,  20*SIZE_REG(\base)
	STORE s6,  21*SIZE_REG(\base)
	STORE s7,  22*SIZE_REG(\base)
	STORE s8,  23*SIZE_REG(\base)
	STORE s9,  24*SIZE_REG(\base)
	STORE s10, 25*SIZE_REG(\base)
	STORE s11, 26*SIZE_REG(\base)
	STORE t3,  27*SIZE_REG(\base)
	STORE t4,  28*SIZE_REG(\base)
	STORE t5,  29*SIZE_REG(\base)
	# we don't save t6 here, due to we have used
	# it as base, we have to save t6 in an extra step
	# outside of reg_save
.endm

# restore all General-Purpose(GP) registers from the context
# except gp & tp.
# struct context *base = &ctx_task;
# ra = base->ra;
# ......
.macro reg_restore base
	LOAD ra,   0*SIZE_REG(\base)
	LOAD sp,   1*SIZE_REG(\base)
	LOAD t0,   4*SIZE_REG(\base)
	LOAD t1,   5*SIZE_REG(\base)
	LOAD t2,   6*SIZE_REG(\base)
	LOAD s0,   7*SIZE_REG(\base)
	LOAD s1,   8*SIZE_REG(\base)
	LOAD a0,   9*SIZE_REG(\base)
	LOAD a1,  10*SIZE_REG(\base)
	LOAD a2,  11*SIZE_REG(\base)
	LOAD a3,  12*SIZE_REG(\base)
	LOAD a4,  13*SIZE_REG(\base)
	LOAD a5,  14*SIZE_REG(\base)
	LOAD a6,  15*SIZE_REG(\base)
	LOAD a7,  16*SIZE_REG(\base)
	LOAD s2,  17*SIZE_REG(\base)
	LOAD s3,  18*SIZE_REG(\base)
	LOAD s4,  19*SIZE_REG(\base)
	LOAD s5,  20*SIZE_REG(\base)
	LOAD s6,  21*SIZE_REG(\base)
	LOAD s7,  22*SIZE_REG(\base)
	LOAD s8,  23*SIZE_REG(\base)
	LOAD s9,  24*SIZE_REG(\base)
	LOAD s10, 25*SIZE_REG(\base)
	LOAD s11, 26*SIZE_REG(\base)
	LOAD t3,  27*SIZE_REG(\base)
	LOAD t4,  28*SIZE_REG(\base)
	LOAD t5,  29*SIZE_REG(\base)
	LOAD t6,  30*SIZE_REG(\base)
.endm

# Something to note about save/restore:
# - We use mscratch to hold a pointer to context of current task
# - We use t6 as the 'base' for reg_save/reg_restore, because it is the
#   very bottom register (x31) and would not be overwritten during loading.
#   Note: CSRs(mscratch) can not be used as 'base' due to load/restore
#   instruction only accept general purpose registers.

.text

# void switch_to(struct context *next);
# a0: pointer to the context of the next task
.globl switch_to
.balign 4
switch_to:
    /*
	這段程式碼是 RISC-V 核心在進行行程切換 (context switch) 時的一個重要片段。它利用 mscratch 暫存器來安全地保存和恢復狀態，
	特別是在第一次呼叫時進行特殊處理。
	
	*/
	csrrw	t6, mscratch, t6	# swap t6 and mscratch 目的：在行程切換時，這個操作可以快速地將舊行程的堆疊指標從 mscratch 讀取到 t6，
	                            # 同時將新行程的堆疊指標從 t6 寫入 mscratch，為下一個中斷或例外做準備。
								# atomic operation，交換 mscratch 跟 t6
								# 第一次 mscratch 我們已經設為 0, 所以 t6 也是 0
	beqz	t6, 1f			# Note: the first time switch_to() is
	                                # called, mscratch is initialized as zero
					# (in sched_init()), which makes t6 zero,
					# and that's the special case we have to
					# handle with t6
					# 因為 t6 = 0, 所以跳到 1f
	reg_save t6			# save context of prev task

	# Save the actual t6 register, which we swapped into
	# mscratch
	mv	t5, t6			# t5 points to the context of current task
	csrr	t6, mscratch		# read t6 back from mscratch
	STORE	t6, 30*SIZE_REG(t5)	# save t6 with t5 as base

1:
	# switch mscratch to point to the context of the next task
	# a0 是 RISC-V 組合語言中一個非常重要的暫存器，它有兩個主要用途：
    # 1. 儲存函式的第一個參數
	# 2. 儲存函式的回傳值
	# 簡單來說，可以把 a0 想像成一個多功能的信差，它既能把參數傳遞給函式，也能把函式的結果帶回來。
	csrw	mscratch, a0 # a0 就是 ctx_task 
	                     # 這一行為下一個中斷預先準備好環境。它將新行程的堆疊指標預先存入 mscratch，讓中斷服務程式在任何時候都能快速、安全地找到正確的堆疊。
                         # 但在這裡並沒有中斷, 所以其實把它刪除是沒有影響的
	# Restore all GP registers
	# Use t6 to point to the context of the new task
	mv	t6, a0
	reg_restore t6

	# Do actual context switching.
	/*
	ret 偽指令實際上被組譯成 jalr x0, 0(ra) 這條基本指令。
	jalr x0, 0(ra) 的作用就是無條件地跳轉到 ra 暫存器所指向的位址，而不進行任何連結或儲存回傳位址的操作。
	因為在 sched.c 裡 ctx_task.ra = (reg_t) user_task0;
	所以 ret 這裡會直接跳轉到 user_task0 的位置來執行
	*/
	ret

.end

