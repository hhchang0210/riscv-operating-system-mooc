#include "os.h"

/*
 * Following functions SHOULD be called ONLY ONE time here,
 * so just declared here ONCE and NOT included in file os.h.
 */
extern void uart_init(void);
extern void page_init(void);

void start_kernel(void)
{
	uart_init();
	uart_puts("Hello, RVOS!\n");

	page_init();
	page_test(); //加上 page_test, 可以體會記憶體配置的狀況

	while (1) {}; // stop here!
}

