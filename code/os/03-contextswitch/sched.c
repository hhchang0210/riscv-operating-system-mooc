#include "os.h"

/* defined in entry.S */
extern void switch_to(struct context *next);

#define STACK_SIZE 1024
/*
 * In the standard RISC-V calling convention, the stack pointer sp
 * is always 16-byte aligned.
 */
uint8_t __attribute__((aligned(16))) task_stack[STACK_SIZE];
struct context ctx_task; // 在 os.h

static void w_mscratch(reg_t x)
{
	asm volatile("csrw mscratch, %0" : : "r" (x));
}

void user_task0(void);
void sched_init()
{
	/*
	w_mscratch(0); 是一條 RISC-V 組合語言的偽指令，它代表將 mscratch 暫存器的值寫入 0。
    mscratch 是一個 RISC-V 架構中機器模式 (Machine Mode) 下的控制與狀態暫存器 (CSR)。它的主要用途是作為一個臨時儲存空間，
	在處理中斷 (interrupt) 或例外 (exception) 時非常有用。
	mscratch 的作用
    mscratch 的設計目的是為中斷服務程式 (interrupt handler) 提供一個安全的、獨立的暫存器，用來在進入中斷時儲存或交換資料。
    進入中斷：將被使用的通用暫存器，儲存到堆疊 (stack) 上，我們需要一個暫存器來操作堆疊指標 (sp)。這時，mscratch 就可以派上用場。 
    程式可以預先將一個指向核心專屬資料結構的指標儲存在 mscratch 中。這樣，中斷處理程式就可以在不需要依賴通用暫存器的情況下，
    直接透過 mscratch 存取自己的堆疊或資料，從而安全地開始儲存通用暫存器

	*/
	w_mscratch(0);
    // 設定現在的 task 的 sp 跟 ra, 把 ra 指向 user_task0 這個function, 這樣從 entry.S 執行 (switch_to 中的 ret) 這個 task 時會直接跳到  user_task0 執行。
	ctx_task.sp = (reg_t) &task_stack[STACK_SIZE];
	ctx_task.ra = (reg_t) user_task0;
}

void schedule()
{
	struct context *next = &ctx_task;
	switch_to(next);
}

/*
 * a very rough implementaion, just to consume the cpu
 */
void task_delay(volatile int count)
{
	count *= 50000;
	while (count--);
}


void user_task0(void)
{
	uart_puts("Task 0: Created!\n");
	while (1) {
		uart_puts("Task 0: Running...\n");
		task_delay(1000);
	}
}

