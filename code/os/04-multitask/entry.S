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
	csrrw	t6, mscratch, t6	# swap t6 and mscratch
	beqz	t6, 1f			# Note: the first time switch_to() is
	                                # called, mscratch is initialized as zero
					# (in sched_init()), which makes t6 zero,
					# and that's the special case we have to
					# handle with t6
	reg_save t6			# save context of prev task
                    
	# Save the actual t6 register, which we swapped into
	# mscratch
	mv	t5, t6			# t5 points to the context of current task
	csrr	t6, mscratch		# read t6 back from mscratch 
	STORE	t6, 30*SIZE_REG(t5)	# save t6 with t5 as base

1:
	# switch mscratch to point to the context of the next task
	csrw	mscratch, a0

	# Restore all GP registers
	# Use t6 to point to the context of the new task
	mv	t6, a0
	reg_restore t6

	# Do actual context switching.
	ret

.end

/*
轉換 1: start_kernel -> task0
這是第一次呼叫 switch_to。此時沒有「上一個」任務，所以程式會跳過保存上下文的步驟。a0 傳入了 task0 的上下文位址 (ctx_task0)。

指令	                     mscratch	  t6	         t5     	說明
csrrw t6, mscratch, t6	    舊 t6 值	  0	        舊 t5 值	將 mscratch 的 0 讀入 t6，並將舊 t6 寫入 mscratch。
beqz t6, 1f	                舊 t6 值      0	        舊 t5 值	t6 為 0，條件成立，跳轉到 1: 標籤。
csrw mscratch, a0	        ctx_task0	  0	        舊 t5 值	將 a0 (ctx_task0) 寫入 mscratch。
mv t6, a0	                ctx_task0	ctx_task0	舊 t5 值	將 a0 (ctx_task0) 複製到 t6，用作 reg_restore 的基底。

/*

/*
轉換 2: task0 -> task1
這是第二次呼叫 switch_to。mscratch 裡現在是 task0 的上下文位址 (ctx_task0)。a0 傳入了 task1 的上下文位址 (ctx_task1)。

指令	                     mscratch	   t6	         t5	                說明
csrrw t6, mscratch, t6	    t6_task0	ctx_task0	   t5_task0     	將 mscratch (ctx_task0) 讀入 t6，並將舊 t6 (t6_task0) 寫入 mscratch。
beqz t6, 1f	                t6_task0	ctx_task0	   t5_task0	        t6 不為 0，條件不成立，繼續執行。
reg_save t6              	t6_task0	ctx_task0	   t5_task0     	使用 t6 (ctx_task0) 作為基底，保存 task0 的暫存器。
mv t5, t6               	t6_task0	ctx_task0	   ctx_task0    	將 t6 (ctx_task0) 複製到 t5，作為保存 t6 的基底位址。
csrr t6, mscratch	        t6_task0	t6_task0	   ctx_task0	    將 mscratch (t6_task0) 讀回 t6，以恢復其真實值。
STORE t6, ... (t5)       	t6_task0	t6_task0	   ctx_task0    	將 t6 (t6_task0) 的值保存到 task0 的上下文中。
上面這些程式都是為了把 task0 存好。可能會有疑問為何要繞一大圈, 寫這樣多程式來存 task0! 直接一開始用 mscratch 就好了？ 
1. mscratch 會用來存下一個要執行的內容(見下一行)
2. 我們利用 t6 來存 task0, 這樣 t6 的內容可能會遺失掉, 所以我們藉由  csrrw t6, mscratch, t6    把 t6 內容放在 mscratch.
所以最後要把 mscratch (這時候代表之前t6的值, t6_task0), 這樣才把 task0的資料完整存在 t5中 

csrw mscratch, a0	        ctx_task1	t6_task0	   ctx_task0	將 a0 (ctx_task1) 寫入 mscratch。
mv t6, a0	                ctx_task1	ctx_task1	   ctx_task0	將 a0 (ctx_task1) 複製到 t6，用作 reg_restore 的基底。


*/
