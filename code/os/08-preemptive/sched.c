#include "os.h"

/* defined in entry.S */
extern void switch_to(struct context *next);

#define MAX_TASKS 10
#define STACK_SIZE 1024
/*
 * In the standard RISC-V calling convention, the stack pointer sp
 * is always 16-byte aligned.
 */
uint8_t __attribute__((aligned(16))) task_stack[MAX_TASKS][STACK_SIZE];
struct context ctx_tasks[MAX_TASKS];

/*
 * _top is used to mark the max available position of ctx_tasks
 * _current is used to point to the context of current task
 */
static int _top = 0;
static int _current = -1;

void sched_init()
{
	w_mscratch(0);

	/* enable machine-mode software interrupts. */
	w_mie(r_mie() | MIE_MSIE);
}

/*
 * implment a simple cycle FIFO schedular
 */
void schedule()
{
	if (_top <= 0) {
		panic("Num of task should be greater than zero!");
		return;
	}

	_current = (_current + 1) % _top;
	struct context *next = &(ctx_tasks[_current]);
	switch_to(next);
}

/*
 * DESCRIPTION
 * 	Create a task.
 * 	- start_routin: task routine entry
 * RETURN VALUE
 * 	0: success
 * 	-1: if error occured
 */
int task_create(void (*start_routin)(void))
{
	if (_top < MAX_TASKS) {
		ctx_tasks[_top].sp = (reg_t) &task_stack[_top][STACK_SIZE];
		ctx_tasks[_top].pc = (reg_t) start_routin;
		_top++;
		return 0;
	} else {
		return -1;
	}
}

/*
 * DESCRIPTION
 * 	task_yield()  causes the calling task to relinquish the CPU and a new 
 * 	task gets to run.
 */
void task_yield()
{
	/* trigger a machine-level software interrupt */
	//*(uint32_t*)CLINT_MSIP(id) = 1; 這行程式碼的作用，就是透過寫入一個非零值，明確地發出一個軟體中斷訊號 。
	int id = r_mhartid();
	*(uint32_t*)CLINT_MSIP(id) = 1;
	//*(uint32_t*)CLINT_MSIP(id) = 1; 觸發了一個軟體中斷，導致程式跳轉到**trap_vector** 開始執行中斷處理邏輯。
	//然後跑到 reg_t trap_handler(reg_t epc, reg_t cause) 
}

/*
 * a very rough implementaion, just to consume the cpu
 */
void task_delay(volatile int count)
{
	count *= 50000;
	while (count--);
}

