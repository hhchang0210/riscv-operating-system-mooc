#include "os.h"

#define DELAY 4000

#define USE_LOCK

void user_task0(void)
{
	uart_puts("Task 0: Created!\n");
	while (1) {
	/*
	在這裡可以用 spin_lock 來確保下面 critical section 的部份一定會執行完, 不會被 timer intertrupt 給中斷去執行 task1
	*/	
#ifdef USE_LOCK
		spin_lock();
#endif
		uart_puts("Task 0: Begin ... \n");
		for (int i = 0; i < 5; i++) {
			uart_puts("Task 0: Running... \n");
			task_delay(DELAY);
		}
		uart_puts("Task 0: End ... \n");
#ifdef USE_LOCK
		spin_unlock();
#endif
	}
}

void user_task1(void)
{
	uart_puts("Task 1: Created!\n");
	while (1) {
		// task1 會被 timer interrupt, 然後把執行權交給 task0
		uart_puts("Task 1: Begin ... \n");
		for (int i = 0; i < 5; i++) {
			uart_puts("Task 1: Running... \n");
			task_delay(DELAY);
		}
		uart_puts("Task 1: End ... \n");
	}
}

/* NOTICE: DON'T LOOP INFINITELY IN main() */
void os_main(void)
{
	task_create(user_task0);
	task_create(user_task1);
}

