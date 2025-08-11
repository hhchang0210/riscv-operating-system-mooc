#include "os.h"

int spin_lock()
{
	/*
	這行程式碼的作用是關閉機器模式下的全域中斷。MSTATUS_MIE：這是一個位元遮罩，對應於 mstatus 暫存器中用來控制全域中斷的位元 。
	在單核心環境中，關閉全域中斷可以確保當前執行的程式碼不會被任何中斷打斷，從而保護關鍵程式碼區段免受競爭條件的影響 。
	
	將 mstatus 的當前值與反相後的遮罩進行位元 AND 運算。這個運算會將全域中斷啟用位元設為 0，而不會影響 mstatus 暫存器中的其他位元。
	這一行程式碼 w_mstatus(r_mstatus() & ~MSTATUS_MIE); 可以在單核心環境或協同式多工 (cooperative multitasking) 系統中，保證沒有人可以搶走執行權。
	如果是在多核心 (multi-core) 環境下，單純關閉中斷來實現自旋鎖是不足夠的，甚至是有缺陷的。
	*/
	w_mstatus(r_mstatus() & ~MSTATUS_MIE);
	return 0;
}

int spin_unlock()
{
	w_mstatus(r_mstatus() | MSTATUS_MIE);
	return 0;
}
