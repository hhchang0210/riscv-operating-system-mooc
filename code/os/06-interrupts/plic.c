#include "os.h"

void plic_init(void)
{
	//取得 hart 值, 這裡是 0
	int hart = r_tp();
  
	/* 
	 * Set priority for UART0.
	 *
	 * Each PLIC interrupt source can be assigned a priority by writing 
	 * to its 32-bit memory-mapped priority register.
	 * The QEMU-virt (the same as FU540-C000) supports 7 levels of priority. 
	 * A priority value of 0 is reserved to mean "never interrupt" and 
	 * effectively disables the interrupt. 
	 * Priority 1 is the lowest active priority, and priority 7 is the highest. 
	 * Ties between global interrupts of the same priority are broken by 
	 * the Interrupt ID; interrupts with the lowest ID have the highest 
	 * effective priority.
	 */
	 // 這些巨集定義在 platform.h
	 /*
	 PLIC_PRIORITY(UART0_IRQ): 這是一個巨集，用於取得 UART0 中斷源的優先級暫存器位址。每個中斷源都有其專屬的優先級暫存器。
	 這裡將 UART0 的優先級設為最低的有效值 1，表示當它發生中斷時，處理器會考慮處理它。
	 PLIC_PRIORITY(UART0_IRQ) 的計算結果就是 0x0c000000L + (10) * 4 = 0x0c000028。
	 將該記憶體位址的值設定為 1
	 */
	*(uint32_t*)PLIC_PRIORITY(UART0_IRQ) = 1;
 
	/*
	 * Enable UART0
	 *
	 * Each global interrupt can be enabled by setting the corresponding 
	 * bit in the enables registers.
	 */

	 /*
	 找到當前核心的中斷啟用暫存器，並將對應於 UART0 中斷 ID 的那個位元設定為 1。
     這是一個啟用 UART0 中斷的關鍵步驟，它告訴 PLIC：「允許 UART0 的中斷傳送到這個 CPU 核心。」
	 這裡說的暫存器其實是記憶體位置。
	 */
	*(uint32_t*)PLIC_MENABLE(hart, UART0_IRQ)= (1 << (UART0_IRQ % 32));

	/* 
	 * Set priority threshold for UART0.
	 *
	 * PLIC will mask all interrupts of a priority less than or equal to threshold.
	 * Maximum threshold is 7.
	 * For example, a threshold value of zero permits all interrupts with
	 * non-zero priority, whereas a value of 7 masks all interrupts.
	 * Notice, the threshold is global for PLIC, not for each interrupt source.
	 */
    
    /*
	將閾值設為 0，是一個常見的初始化步驟，因為：
    它允許所有非零優先級的中斷：根據 PLIC 規範，優先級 0 代表「永不中斷」。因此，將閾值設為 0 就能允許所有優先級為 1 到 7 的中斷請求傳送給 CPU。
\   等同於關閉篩選器：這就像是將所有中斷的閘門都打開，讓任何有實際優先級（非零）的中斷都能被 CPU 接收。
    簡而言之，*(uint32_t*)PLIC_MTHRESHOLD(hart) = 0; 這行程式碼的作用是確保沒有中斷會因為優先級太低而被 PLIC 拒絕，從而允許作業系統的邏輯來決定如何處理它們。
	*/

	*(uint32_t*)PLIC_MTHRESHOLD(hart) = 0;

	/* enable machine-mode external interrupts. */
	//這段程式碼的目的是啟用 RISC-V 機器模式 (Machine Mode) 的外部中斷
	/*
	r_mie()：這是一個函式，用於讀取 mie (Machine Interrupt Enable) 暫存器目前的數值。
    | MIE_MEIE：這是一個位元 OR (bitwise OR) 運算。
    MIE_MEIE 是一個位元遮罩 (bitmask)，它的值只有一個位元是 1，這個位元對應於機器模式外部中斷的開關。
    r_mie() | MIE_MEIE 的結果是將 mie 暫存器的值與這個遮罩進行 OR 運算。這會將外部中斷的開關位元設為 1，同時保留 mie 暫存器中其他位元的原始設定。
	w_mie(reg_t x) 這個函式負責將 C 語言中的值寫入 mie 暫存器。 開啟機器模式的外部中斷。
	機器模式 (Machine Mode) 的外部中斷預設是沒有打開的。機器模式是 RISC-V 最高的特權等級，擁有對所有硬體資源的完全控制權。如果預設就打開外部中斷，
	系統在啟動時可能會因為隨機的中斷而進入不穩定的狀態。
	*/
	w_mie(r_mie() | MIE_MEIE);

	/* enable machine-mode global interrupts. */
	// 啟用 RISC-V 機器模式 (Machine Mode) 的全域中斷
	//mstatus 是一個特權暫存器，用來儲存處理器在機器模式下的各種狀態資訊。
	//MSTATUS_MIE 是一個位元遮罩，它的值只有一個位元是 1，這個位元對應於全域中斷啟用 (Machine Interrupt Enable) 的開關。
	/*
	為什麼需要全域中斷開關？
    在 RISC-V 中，中斷的啟用是分層次的，需要多個開關同時打開才能生效。mstatus 的 MIE 位元就是最頂層的總開關。
    如果 MSTATUS_MIE 為 0：即使 PLIC (平台級中斷控制器) 和 mie 暫存器都啟用了中斷，中斷訊號也無法傳送到處理器核心。
    如果 MSTATUS_MIE 為 1：中斷的啟用取決於 mie 暫存器的設定。例如，只有當 mie 的外部中斷位元也為 1 時，外部中斷才會被處理。
    因此，這行程式碼是啟用中斷的最後一道門，它確保在所有低層次的設定（例如 PLIC 優先級、mie 暫存器）都完成之後，系統才正式進入可以接收和處理中斷的狀態。
	*/

	/*
	全域中斷跟外部中斷差異是？
	這兩個中斷開關的差異在於它們的層次和作用範圍不同。你可以把它們想像成控制中斷的兩道門，必須同時打開，中斷訊號才能抵達 CPU 核心。
	處理器接收中斷⟺(MSTATUS.MIE=1) AND (MIE.MEIE=1)
	假設你正在設置 UART0 中斷，你必須依序開啟三層開關：
    PLIC 層：在 PLIC 中啟用 UART0 中斷並設定其優先級。
    mie 暫存器層：將 mie 中的 MEIE 位元設為 1，打開外部中斷的開關。
    mstatus 暫存器層：將 mstatus 中的 MIE 位元設為 1，打開全域中斷的總開關
	*/
	w_mstatus(r_mstatus() | MSTATUS_MIE);
}

/* 
 * DESCRIPTION:
 *	Query the PLIC what interrupt we should serve.
 *	Perform an interrupt claim by reading the claim register, which
 *	returns the ID of the highest-priority pending interrupt or zero if there 
 *	is no pending interrupt. 
 *	A successful claim also atomically clears the corresponding pending bit
 *	on the interrupt source.
 * RETURN VALUE:
 *	the ID of the highest-priority pending interrupt or zero if there 
 *	is no pending interrupt.
 */
int plic_claim(void)
{
	//獲取當前發生中斷且優先級最高的中斷源 ID 
	int hart = r_tp();
	int irq = *(uint32_t*)PLIC_MCLAIM(hart);
	return irq;
}

/* 
 * DESCRIPTION:
  *	Writing the interrupt ID it received from the claim (irq) to the 
 *	complete register would signal the PLIC we've served this IRQ. 
 *	The PLIC does not check whether the completion ID is the same as the 
 *	last claim ID for that target. If the completion ID does not match an 
 *	interrupt source that is currently enabled for the target, the completion
 *	is silently ignored.
 * RETURN VALUE: none
 */
void plic_complete(int irq)
{
	//通知 PLIC 該路中斷的處理已經結束 
	int hart = r_tp();
	*(uint32_t*)PLIC_MCOMPLETE(hart) = irq;
}
