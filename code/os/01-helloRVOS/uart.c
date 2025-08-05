#include "types.h"
#include "platform.h"

/*
 * The UART control registers are memory-mapped at address UART0. 
 * This macro returns the address of one of the registers.
 */
#define UART_REG(reg) ((volatile uint8_t *)(UART0 + reg))
// UART0 的位置 #define UART0 0x10000000L
// 加上 UART0 的位置當作 reg 的位置

/*
 * Reference
 * [1]: TECHNICAL DATA ON 16550, http://byterunner.com/16550.html
 */

/*
 * UART control registers map. see [1] "PROGRAMMING TABLE"
 * note some are reused by multiple functions
 * 0 (write mode): THR/DLL
 * 1 (write mode): IER/DLM
 */
#define RHR 0	// Receive Holding Register (read mode)
#define THR 0	// Transmit Holding Register (write mode)
#define DLL 0	// LSB of Divisor Latch (write mode)
#define IER 1	// Interrupt Enable Register (write mode)
#define DLM 1	// MSB of Divisor Latch (write mode)
#define FCR 2	// FIFO Control Register (write mode)
#define ISR 2	// Interrupt Status Register (read mode)
#define LCR 3	// Line Control Register
#define MCR 4	// Modem Control Register
#define LSR 5	// Line Status Register
#define MSR 6	// Modem Status Register
#define SPR 7	// ScratchPad Register

/*
 * POWER UP DEFAULTS
 * IER = 0: TX/RX holding register interrupts are both disabled
 * ISR = 1: no interrupt penting
 * LCR = 0
 * MCR = 0
 * LSR = 60 HEX
 * MSR = BITS 0-3 = 0, BITS 4-7 = inputs
 * FCR = 0
 * TX = High
 * OP1 = High
 * OP2 = High
 * RTS = High
 * DTR = High
 * RXRDY = High
 * TXRDY = Low
 * INT = Low
 */

/*
 * LINE STATUS REGISTER (LSR)
 * LSR BIT 0:
 * 0 = no data in receive holding register or FIFO.
 * 1 = data has been receive and saved in the receive holding register or FIFO.
 * ......
 * LSR BIT 5:
 * 0 = transmit holding register is full. 16550 will not accept any data for transmission.
 * 1 = transmitter hold register (or FIFO) is empty. CPU can load the next character.
 * ......
 */
// LSR 的第五個 bit 為 1 的話, cpu 就可以讀下一個字元
#define LSR_RX_READY (1 << 0)
#define LSR_TX_IDLE  (1 << 5)


#define uart_read_reg(reg) (*(UART_REG(reg)))
#define uart_write_reg(reg, v) (*(UART_REG(reg)) = (v))

void uart_init()
{
	/* disable interrupts. */
	/*初始化階段: 在設定 UART 參數時，避免在設定完成前產生不必要的中斷。
	  透過將 IER 暫存器清零，來達到 完全關閉所有 UART 中斷 的目的。
	*/
	uart_write_reg(IER, 0x00);


	/*
	 * Setting baud rate. Just a demo here if we care about the divisor,
	 * but for our purpose [QEMU-virt], this doesn't really do anything.
	 *
	 * Notice that the divisor register DLL (divisor latch least) and DLM (divisor
	 * latch most) have the same base address as the receiver/transmitter and the
	 * interrupt enable register. To change what the base address points to, we
	 * open the "divisor latch" by writing 1 into the Divisor Latch Access Bit
	 * (DLAB), which is bit index 7 of the Line Control Register (LCR).
	 *
	 * Regarding the baud rate value, see [1] "BAUD RATE GENERATOR PROGRAMMING TABLE".
	 * We use 38.4K when 1.8432 MHZ crystal, so the corresponding value is 3.
	 * And due to the divisor register is two bytes (16 bits), so we need to
	 * split the value of 3(0x0003) into two bytes, DLL stores the low byte,
	 * DLM stores the high byte.
	 */

	 /*
	 我們不確定 LCR 的初始值，所以必須先讀取它，以避免覆蓋掉之前設定好的其他參數（例如資料位元數、停止位元數等）。
	 uart_write_reg(LCR, lcr | (1 << 7)); 這行程式碼會設定 LCR 的第 7 個位元 (bit 7) 為 1。這會確保 LCR 的第 7 個位元被設定為 1，同時保留其他位元的值不變。
	 LCR 的第 7 位元稱為 DLAB (Divisor Latch Access Bit)。
	 當 DLAB = 1 時，CPU 讀寫 DL (Divisor Latch) 暫存器時，實際上是在操作 DLL (Divisor Latch Low) 和 DLM (Divisor Latch High) 這兩個暫存器。
     當 DLAB = 0 時，CPU 讀寫 DL 暫存器時，實際上是在操作 IER 和 FCR (FIFO Control Register) 等暫存器。
     因此，這行程式碼的目的是開啟 DLAB，讓後續的 DLL 和 DLM 寫入操作能夠真正影響到波特率的設定。
	 */
	uint8_t lcr = uart_read_reg(LCR);
	uart_write_reg(LCR, lcr | (1 << 7));
	uart_write_reg(DLL, 0x03);
	uart_write_reg(DLM, 0x00);

	/*
	 * Continue setting the asynchronous data communication format.
	 * - number of the word length: 8 bits
	 * - number of stop bits：1 bit when word length is 8 bits
	 * - no parity
	 * - no break control
	 * - disabled baud latch
	 */
	/*
	程式碼 (3 << 0) 的結果是 0x03，其二進位表示為 00000011。
    這會將 LCR 的第 0 和第 1 個位元都設定為 1 (11)。
    根據 LCR 的規範，LCR[1:0] = 11 代表將資料位元數設定為 8 個位元。
	*/
	lcr = 0;
	uart_write_reg(LCR, lcr | (3 << 0));

}
/*

uart_putc 的功能是同步 (synchronous) 地傳送一個字元。它採用了忙碌等待 (busy-waiting) 的方式：
等待：透過一個空迴圈不斷檢查 UART 的狀態。
傳送：當 UART 準備好時，將字元寫入傳送暫存器。
這種方式雖然簡單直觀，但在等待期間，CPU 會一直處於忙碌狀態，無法執行其他任務。在更進階的系統中，通常會使用中斷 (interrupt) 來取代忙碌等待，讓 CPU 在等待時可以執行其他工作，提高系統的效率。
*/
int uart_putc(char ch)
{
	/*
	這個 while 迴圈會持續地、重複地讀取 LSR 暫存器，直到 LSR_TX_IDLE 位元變成 1 為止。這是一個等待的過程，
	確保在寫入字元之前，UART 的傳送緩衝區已經是空的，避免資料遺失。
	*/
	while ((uart_read_reg(LSR) & LSR_TX_IDLE) == 0);

	/*
	THR 是 UART 專門用來存放待傳送資料的暫存器。當你將字元寫入 THR 後，UART 硬體就會自動開始將這個字元轉換為串行訊號並發送出去。
    函式會回傳 uart_write_reg 的執行結果，通常這個函式會回傳 0 或其他代表成功的值
	
	*/
	return uart_write_reg(THR, ch);
}

void uart_puts(char *s)
{
	while (*s) {
		uart_putc(*s++);
	}
}

