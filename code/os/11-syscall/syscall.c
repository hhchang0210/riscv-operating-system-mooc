#include "os.h"
#include "syscall.h"

int sys_gethid(unsigned int *ptr_hid)
{
	printf("--> sys_gethid, arg0 = %p\n", ptr_hid);
	if (ptr_hid == NULL) {
		return -1;
	} else {
		*ptr_hid = r_mhartid();
		return 0;
	}
}
int sys_sum(int a, int b) {
    // 在核心態下執行加法
    return a + b;
}
void do_syscall(struct context *cxt)
{
	uint32_t syscall_num = cxt->a7;
	int arg1, arg2, result;
	switch (syscall_num) {
	case SYS_gethid:
		cxt->a0 = sys_gethid((unsigned int *)(cxt->a0)); //a0 是個adress, 存放 &hid 的位置
		break;
	case SYS_sum: // <-- 新增的 case
            //[cite_start]// 從 context 中取出使用者傳入的參數 (a0, a1) [cite: 1141]
            arg1 = cxt->a0;
            arg2 = cxt->a1;
            // 呼叫真正的服務函式
            result = sys_sum(arg1, arg2);
            //[cite_start]// 將返回值寫回 context 的 a0 欄位 [cite: 1142]
            cxt->a0 = result;
            break;
	default:
		printf("Unknown syscall no: %d\n", syscall_num);
		cxt->a0 = -1;
	}

	return;
}

