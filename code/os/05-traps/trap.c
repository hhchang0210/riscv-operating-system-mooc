#include "os.h"

extern void trap_vector(void);

void trap_init()
{
	/*
	 * set the trap-vector base-address for machine-mode
	 */
	 /*
	 w_mtvec((reg_t)trap_vector); 是一段 C 語言程式碼，用於在 RISC-V 機器模式 (Machine Mode) 下設定中斷向量表 (trap vector table) 的位址。
	 mtvec (Machine Trap-Vector Base-Address Register)：它的作用是儲存中斷處理程式的起始位址。當處理器遇到中斷 (interrupt) 或例外 (exception) 時，
	 它會自動跳轉到 mtvec 所指向的位址，開始執行中斷處理程式。
	 trap_vector： 在 entry.S
     這是一個函式指標或程式碼區塊的名稱。它包含了中斷處理程式的程式碼。
     通常，這個程式碼會負責保存當前執行緒的狀態，判斷中斷的類型，然後跳轉到對應的處理函式。
	 這行程式碼的最終目的是初始化 RISC-V 的中斷系統。它告訴處理器：
    「嘿，如果發生了任何中斷或例外，請不要隨意亂跳，而是去執行 trap_vector 函式所定義的程式碼。」
	 */
	w_mtvec((reg_t)trap_vector); //在 riscv.h
}

reg_t trap_handler(reg_t epc, reg_t cause)
{
	reg_t return_pc = epc; 存著 *(int *)0x00000000 = 100 這行的位置
	reg_t cause_code = cause & MCAUSE_MASK_ECODE; 
	/*
	cause: 0x00000007 (假設是 32 位元系統)，其二進位為 0...0000111。
	MCAUSE_MASK_ECODE：這是一個位元遮罩，它的值通常是 0x7FFFFFFF。這意味著它的最高位元是 0，而其他所有位元都是 1
	運算結果

cause = 00000000000000000000000000000111 
MCAUSE_MASK_ECODE = 01111111111111111111111111111111 
cause_code = 00000000000000000000000000000111 
​
 
最終的結果 cause_code 的值會是 0x00000007。
	*/
	if (cause & MCAUSE_MASK_INTERRUPT) { //MCAUSE_MASK_INTERRUPT：這是一個位元遮罩，它的值是一個只有最高位元 (Most Significant Bit, MSB) 為 1，其餘位元皆為 0 的數字。
		/* Asynchronous trap - interrupt */
		switch (cause_code) {
		case 3:
			uart_puts("software interruption!\n");
			break;
		case 7:
			uart_puts("timer interruption!\n");
			break;
		case 11:
			uart_puts("external interruption!\n");
			break;
		default:
			printf("Unknown async exception! Code = %ld\n", cause_code);
			break;
		}
	} else {
		/*這裡我們觸發 exception*/
		/* Synchronous trap - exception */
		printf("Sync exceptions! Code = %ld\n", cause_code);
		panic("OOPS! What can I do!");

        /*
		這個程式預設是 return *(int *)0x00000000 = 100; 的位置給 mepc, 所以執行後會一直印出 panic! 也就是執行完 trap 後, 
		沒有跳到 *(int *)0x00000000 = 100;的下一行執行		
		要解決這個問題就是下面這一行去掉註解 return_pc +=4, 就是把 *(int *)0x00000000 = 100;的位置+4, 也就是下一行的位置傳給 mepc (跳到 csrw	mepc, a0 這裡)
		
		*/

		// return_pc += 4; 
	}

	return return_pc;
}

void trap_test()
{
	/*
	 * Synchronous exception code = 7
	 * Store/AMO access fault
	 */
	 /*
	 這段程式碼 *(int *)0x00000000 = 100; 試圖寫入記憶體位址 0x00000000。
     這在大多數作業系統中都會導致Store/AMO access fault (儲存/原子操作存取錯誤)，也就是註解中提到的「Synchronous exception code = 7」。
	 
	 這個錯誤之所以發生，有以下幾個原因：
     1.位址 0x00000000 是零位址：這個位址通常被作業系統保留，用來捕捉無效的指標。當程式試圖存取一個空指標（NULL）時，其實際存取的就是零位址。
	 因此，作業系統會將這個位址設定為不可讀寫，以防止程式意外地寫入重要資料或導致未定義行為。
     2. 記憶體區段保護：現代作業系統會將記憶體分成不同的區段，並給予它們不同的權限（例如可讀、可寫、可執行）。
	 位址 0x00000000 通常被標記為不可存取。當程式試圖對這個位址進行寫入 (Store) 操作時，處理器的記憶體管理單元 (MMU) 會偵測到這個違規行為，
	 並立即產生一個例外。

     3. 例外代碼 7：在 RISC-V 架構中，例外代碼 7 專門用來表示「儲存/原子操作存取錯誤」。這就是註解中提到的錯誤代碼，它精確地描述了你程式碼中發生的問題。
	 */
	*(int *)0x00000000 = 100; //跳到 mtvec 暫存器所儲存的位址來執行, 在這裡就是trap_vector
	/*
	硬體處理：處理器會停止執行當前程式碼，並自動執行以下幾個硬體步驟：
    1. 將當前程式計數器 (Program Counter) 的值，儲存到 mepc (Machine Exception Program Counter) 暫存器中。(*(int *)0x00000000 = 100;這一行的位置)
    2. 將例外的原因（在此例中是錯誤代碼 7）儲存到 mcause 暫存器中。
    3. 將發生錯誤的位址（0x00000000）儲存到 mtval 暫存器中。
    4. 跳轉執行：處理器會將程式計數器的值設定為 mtvec 暫存器所儲存的位址。
	*/

	/*
	 * Synchronous exception code = 5
	 * Load access fault
	 */
	//int a = *(int *)0x00000000;

	uart_puts("Yeah! I'm return back from trap!\n");
}

