#include "os.h"

/*
 * Following global vars are defined in mem.S
 */
extern ptr_t TEXT_START;
extern ptr_t TEXT_END;
extern ptr_t DATA_START;
extern ptr_t DATA_END;
extern ptr_t RODATA_START;
extern ptr_t RODATA_END;
extern ptr_t BSS_START;
extern ptr_t BSS_END;
extern ptr_t HEAP_START;
extern ptr_t HEAP_SIZE;

/*
 * _alloc_start points to the actual start address of heap pool
 * _alloc_end points to the actual end address of heap pool
 * _num_pages holds the actual max number of pages we can allocate.
 */
static ptr_t _alloc_start = 0;
static ptr_t _alloc_end = 0;
static uint32_t _num_pages = 0;

#define PAGE_SIZE 4096
/*
PAGE_ORDER 的值為 12，所以頁面大小的計算如下：
頁面大小=2^12=4096 Bytes=4 KB
代表系統中的記憶體頁面大小為 4 KB

使用 PAGE_ORDER 而非直接定義 PAGE_SIZE 4096 有幾個好處：

1.位元運算效率高：在處理記憶體位址時，使用位元左移或右移操作來進行乘除法運算比一般的乘除法快得多。
  要將頁號 (page number) 轉換為實體位址 (physical address)，只需要將頁號左移 PAGE_ORDER 位即可。
  要從一個實體位址中取得頁號，只需要將位址右移 PAGE_ORDER 位即可。
  例如：實體位址 = 頁號 << PAGE_ORDER

2. 彈性：如果未來需要修改頁面大小，例如從 4 KB 變成 8 KB，只需要將 PAGE_ORDER 改成 13 (2^13 = 8192)，而不需要修改所有用到 4096 這個數字的地方。這讓程式碼更容易維護。

在這個例子中，PAGE_ORDER 被用來進行底層、高效能的位元運算，而 PAGE_SIZE 則被用在需要具體大小的場合。這兩個常數互相補充，讓程式碼在不同情境下都保持清晰和高效。

所以，程式碼同時定義 PAGE_ORDER 12 和 PAGE_SIZE 4096 是非常常見的，這是一種優化程式碼設計的慣例。
*/
#define PAGE_ORDER 12 
#define PAGE_TAKEN (uint8_t)(1 << 0)
#define PAGE_LAST  (uint8_t)(1 << 1)

/*
 * Page Descriptor 
 * flags:
 * - bit 0: flag if this page is taken(allocated)
 * - bit 1: flag if this page is the last page of the memory block allocated
 */
struct Page {
	uint8_t flags;
};

static inline void _clear(struct Page *page)
{
	page->flags = 0;
}

static inline int _is_free(struct Page *page)
{
	if (page->flags & PAGE_TAKEN) {
		return 0;
	} else {
		return 1;
	}
}

static inline void _set_flag(struct Page *page, uint8_t flags)
{
	page->flags |= flags;
}

static inline int _is_last(struct Page *page)
{
	if (page->flags & PAGE_LAST) {
		return 1;
	} else {
		return 0;
	}
}

/*
 * align the address to the border of page(4K)
 */

 /*
 PAGE_ORDER：這是先前討論過的常數，代表頁面大小的 2 
N
  次方中的 N。在這個例子中，PAGE_ORDER 是 12。

1 << PAGE_ORDER：這是一個位元左移運算，結果是 2 
12
 =4096，也就是 PAGE_SIZE。

... - 1：將 4096 減 1，結果是 4095。

4095 的二進位表示是 0000111111111111（12 個 1）。

所以，order 變數現在儲存了一個位元遮罩，它包含了頁面大小以下的所有位元。
return (address + order) & (~order);這是函式的核心部分，透過位元運算來實現對齊。
address + order：將輸入的位址 address 加上 order (4095)。
這個操作的目的是確保任何位址在加上這個值後，其結果會跨越到下一個頁面邊界。
舉例說明：

如果 address = 4096 (已經對齊)，address + 4095 = 8191。
如果 address = 4097 (未對齊)，address + 4095 = 8192。
如果 address = 8191 (未對齊)，address + 4095 = 12286。
~order：這是位元反相運算 (bitwise NOT)。
order (4095) 的二進位是 ...0000111111111111。
~order 的二進位是 ...1111000000000000。
這個結果是一個遮罩，它包含了所有頁面邊界以上的位元，而將頁面邊界以下的位元全部清零。這個值通常稱為 PAGE_MASK。
有人會定義 #define PAGE_MASK (~(PAGE_SIZE - 1)) // 定義 PAGE_MASK

將 (address + order) 的結果與 ~order 進行位元 AND 運算。
這個 AND 運算會將 (address + order) 結果的所有低 12 位元全部清零。
舉例說明：
如果 (address + order) = 8191 (...0001111111111111)，& (~order) 的結果就是 8192 (...0010000000000000)。
如果 (address + order) = 8192 (...0010000000000000)，& (~order) 的結果就是 8192 (...0010000000000000)。
如果 (address + order) = 12286 (...0010111111111110)，& (~order) 的結果就是 12288 (...0011000000000000)。

這個函式的核心思想是先將位址「向上推進」到可能超過下一個對齊點，然後再將頁面內的偏移量部分「歸零」，從而得到一個對齊到頁面邊界的位址。
 */

static inline ptr_t _align_page(ptr_t address)
{
	ptr_t order = (1 << PAGE_ORDER) - 1;
	return (address + order) & (~order);
}

/*
 *    ______________________________HEAP_SIZE_______________________________
 *   /   ___num_reserved_pages___   ______________num_pages______________   \
 *  /   /                        \ /                                     \   \
 *  |---|<--Page-->|<--Page-->|...|<--Page-->|<--Page-->|......|<--Page-->|---|
 *  A   A                         A                                       A   A
 *  |   |                         |                                       |   |
 *  |   |                         |                                       |   _memory_end
 *  |   |                         |                                       |
 *  |   _heap_start_aligned       _alloc_start                            _alloc_end
 *  HEAP_START(BSS_END)
 *
 *  Note: _alloc_end may equal to _memory_end.
 */
/*
起始點：從程式的 .bss 區段結束處開始。
對齊：將起始位址對齊到頁面邊界。
自我管理：首先保留一小塊區域 (num_reserved_pages)，用來存放管理記憶體本身的元資料（metadata），即每個頁面的狀態結構。用來記錄後面 _alloc_start 到 _alloc_end 裡的資料
可分配區域：剩餘的絕大部分記憶體 (num_pages) 則被劃分出來，作為真正的可分配池。

alloc_start：
這是真正可供程式分配記憶體的起始位址。
它位於 num_reserved_pages 區塊的末尾。
_alloc_start = _heap_start_aligned + num_reserved_pages × PAGE_SIZE。

我卡住的地方是: num_pages 有幾頁, 那 num_reserved_pages 就有幾個! num_reserved_pages 裡面只有uint8_t flags, 其實只有紀錄 num_pages 有沒有被使用!
所以 num_reserved_pages 的記憶體相對只要少少的就好,  而 num_pages 占的記憶體相對比較大 num_reserved_pages × PAGE_SIZE。
*/


void page_init()
{
	/*
	為何需要對齊？
在現代的記憶體管理系統中，對齊是至關重要的步驟。

硬體要求：硬體的分頁機制（例如記憶體管理單元 MMU）是基於頁面工作的。它只能以頁面為單位進行位址轉換和權限檢查。如果一個記憶體區塊沒有對齊到頁面邊界，硬體將無法正確處理它。

簡化管理：對齊後，所有的記憶體操作（分配、釋放、計算偏移量等）都可以直接以頁面為單位進行。這簡化了程式碼邏輯，提高了效率。例如，要從一個位址取得頁號，只需簡單地右移 PAGE_ORDER 位，而不需要處理複雜的邊界情況。

效能優化：處理器在載入或儲存對齊的資料時，通常會比處理未對齊的資料更快。

總結來說，這行程式碼是在準備堆疊區域的第一步。它確保了堆疊的起始位址符合硬體的頁面管理要求，為後續的記憶體分配和管理奠定了基礎。
	*/
	ptr_t _heap_start_aligned = _align_page(HEAP_START); //這行程式碼將 HEAP_START 這個位址對齊到頁面邊界，並將對齊後的結果儲存在 _heap_start_aligned 變數中。

	/* 
	 * We reserved some Pages to hold the Page structures.
	 * The number of reserved pages depends on the LENGTH_RAM.
	 * For simplicity, the space we reserve here is just an approximation,
	 * assuming that it can accommodate the maximum LENGTH_RAM.
	 * We assume LENGTH_RAM should not be too small, ideally no less
	 * than 16M (i.e. PAGE_SIZE * PAGE_SIZE).
	 */
    /*
	程式設計者的捷徑：
為了避免複雜的遞迴計算，程式設計者選擇了一個簡單的、但可能不準確的近似方法。
他們觀察到，如果 LENGTH_RAM 大於或等於 16 MB，LENGTH_RAM / (PAGE_SIZE * PAGE_SIZE) 至少會得到 1。這個結果作為預留頁面數。
這個 16 MB 的值可能代表一個經驗法則，例如，假設每 16 MB 的記憶體只需要一個頁面來儲存其頁面結構。這是一個高度簡化的假設。
	*/
	uint32_t num_reserved_pages = LENGTH_RAM / (PAGE_SIZE * PAGE_SIZE);
    
    /*
	搭配上面的 ASCII 圖一起看
	這行程式碼的目的是計算實際可供分配的頁面數量，也就是圖示中 num_pages 區域的大小。
	這個計算可以拆解成幾個部分來理解：

HEAP_SIZE - (_heap_start_aligned - HEAP_START)

HEAP_SIZE：代表未對齊的堆疊總大小。

_heap_start_aligned - HEAP_START：這是一個偏移量，代表因為對齊操作而損失的記憶體空間。
例如，如果 HEAP_START = 0x80011234，_heap_start_aligned = 0x80012000。
這個差值就是 0x80012000 - 0x80011234 = 0xDCB，也就是 3531 個位元組。
HEAP_SIZE - ...：這部分計算的是經過對齊後，實際可用的堆疊總大小。因為我們只能從 _heap_start_aligned 開始使用記憶體，所以 HEAP_START 到 _heap_start_aligned 之間的空間就無法使用，需要從 HEAP_SIZE 中扣除。

(...) / PAGE_SIZE
這部分是用對齊後的總堆疊大小，除以頁面大小 (PAGE_SIZE)。
這是計算對齊後，整個堆疊區域總共有多少個頁面。
這個數量包含了兩部分：儲存頁面結構的頁面 (num_reserved_pages)，以及實際可供分配的頁面 (_num_pages)。

... - num_reserved_pages
這部分是從總頁面數中，減去已經被保留用來儲存頁面結構的頁面數量。
這樣做就能得到真正可供應用程式或系統分配使用的頁面數量，也就是變數 _num_pages 所代表的值。
	*/


	_num_pages = (HEAP_SIZE - (_heap_start_aligned - HEAP_START))/ PAGE_SIZE - num_reserved_pages;
	printf("HEAP_START = %p(aligned to %p), HEAP_SIZE = 0x%lx,\n"
	       "num of reserved pages = %d, num of pages to be allocated for heap = %d\n",
	       HEAP_START, _heap_start_aligned, HEAP_SIZE,
	       num_reserved_pages, _num_pages);
	
	/*
	 * We use HEAP_START, not _heap_start_aligned as begin address for
	 * allocating struct Page, because we have no requirement of alignment
	 * for position of struct Page.
	 */
    /*
	這是核心原因。程式設計者認為，struct Page 結構體本身在記憶體中的位置 (position)，不需要對齊到頁面邊界。
    也就是說，這個結構體可以從任何位址開始存放，不一定要是 4KB 的倍數。
    為什麼沒有對齊要求？因為 struct Page 是一個軟體定義的資料結構。它不像硬體分頁表那樣有嚴格的對齊要求。它的內容是程式設計師定義的，只要 CPU 能夠讀寫即可，不一定需要頁面邊界的對齊。
    使用 HEAP_START 能夠最大化利用記憶體空間，避免在 HEAP_START 和 _heap_start_aligned 之間的少許空間被浪費。

	而在前面的程式做很多對齊的動作是為了: 實際可分配的記憶體頁面 (Actual Pages)。程式碼 _alloc_start 到 _alloc_end 指向的就是這個區塊。
	*/

	struct Page *page = (struct Page *)HEAP_START;
	for (int i = 0; i < _num_pages; i++) {
		_clear(page);
		page++;	
	}

	_alloc_start = _heap_start_aligned + num_reserved_pages * PAGE_SIZE;
	_alloc_end = _alloc_start + (PAGE_SIZE * _num_pages);

	printf("TEXT:   %p -> %p\n", TEXT_START, TEXT_END);
	printf("RODATA: %p -> %p\n", RODATA_START, RODATA_END);
	printf("DATA:   %p -> %p\n", DATA_START, DATA_END);
	printf("BSS:    %p -> %p\n", BSS_START, BSS_END);
	printf("HEAP:   %p -> %p\n", _alloc_start, _alloc_end);
}

/*
 * Allocate a memory block which is composed of contiguous physical pages
 * - npages: the number of PAGE_SIZE pages to allocate
 */
//page_alloc 函式實現了一個首次適配 (first-fit) 記憶體分配演算法，用於在記憶體池中尋找並分配連續的記憶體頁面。
void *page_alloc(int npages)
{
	/* Note we are searching the page descriptor bitmaps. */
	int found = 0;
	struct Page *page_i = (struct Page *)HEAP_START;
	//1. 尋找空閒區塊
	for (int i = 0; i <= (_num_pages - npages); i++) { //依序檢查每一個頁面是否可能是分配區塊的起始點。
		/*
		這個迴圈條件 i <= (_num_pages - npages) 的目的是確保在搜尋過程中，我們總是有足夠的連續空間來滿足分配請求。
		就是確保到最後一個 i 時還有 npages 夠用, 如果不夠 npages 就不對了！
		假設：
        總共有 10 個可分配頁面 (_num_pages = 10)。
        我們需要分配 3 個連續頁面 (npages = 3)。
        那麼，迴圈條件就會是 i <= (10 - 3)，也就是 i <= 7。
        這表示 i 的值可以從 0 到 7。讓我們看看為什麼：
        當 i = 0：我們從第 0 個頁面開始檢查。後續還有 9 個頁面，足夠容納 3 個頁面。
        當 i = 7：我們從第 7 個頁面開始檢查。這時，我們需要檢查第 7、8、9 三個頁面。這仍然是可行的，總共有 3 個頁面可以檢查。
        當 i = 8：如果迴圈條件允許 i=8，我們需要檢查第 8、9、10 三個頁面。但總共只有 10 個頁面，索引最多到 9。這會導致程式碼訪問到不存在的頁面，造成錯誤。
		*/                                               
		if (_is_free(page_i)) {
			/*
			當外部迴圈找到一個空閒頁面 (_is_free(page_i)) 後，內部迴圈會開始檢查後續的 (npages - 1) 個頁面。
            它會確認這些頁面是否也都是空閒的，以確保找到的區塊是連續的。
            如果找到任何一個非空閒的頁面，found 旗標會被設為 0，迴圈中斷，外部迴圈會繼續從下一個頁面尋找。要找到連續的 npages 才行。
			*/
			found = 1;
			/* 
			 * meet a free page, continue to check if following
			 * (npages - 1) pages are also unallocated.
			 */
			struct Page *page_j = page_i + 1;
			for (int j = i + 1; j < (i + npages); j++) {
				/*
				j < (i + npages) 的目的，是讓 j 從 i+1 開始，一路檢查到 i+npages-1 為止。這樣做能確保我們檢查的頁面總數，剛好是從 i 開始的 npages 個連續頁面。
				也可以寫成 for (int j = 1; j < npages; j++)
				*/
				if (!_is_free(page_j)) {
					found = 0;
					break;
				}
				page_j++;
			}
			/*
			 * get a memory block which is good enough for us,
			 * take housekeeping, then return the actual start
			 * address of the first page of this memory block
			 */
			
			if (found) {
			/*
			到這裡表示 metadata 有連續 npages 可以用, 也就是可以配置的記憶體 (num_pages) 也有足夠空間可以配置。
			*/
				struct Page *page_k = page_i;
				for (int k = i; k < (i + npages); k++) {
					_set_flag(page_k, PAGE_TAKEN);
					page_k++;
				}
				page_k--;
				_set_flag(page_k, PAGE_LAST);
				return (void *)(_alloc_start + i * PAGE_SIZE); // 真正回傳的記憶體位置是從 _alloc_start 開始算, 不是從 HEAP_START 算。
			}
		}
		page_i++;
	}
	return NULL;
}

/*
 * Free the memory block
 * - p: start address of the memory block
 */
void page_free(void *p)
{
	/*
	 * Assert (TBD) if p is invalid
	 */
	if (!p || (ptr_t)p >= _alloc_end) {
		return;
	}
	/* get the first page descriptor of this memory block */
	struct Page *page = (struct Page *)HEAP_START;
	page += ((ptr_t)p - _alloc_start)/ PAGE_SIZE;
	/* loop and clear all the page descriptors of the memory block */
	// 其實真正清除記憶體是在 metadata 部份, 而真正配置的記憶體不用特地清除，就每次使用時都覆蓋過去。
	while (!_is_free(page)) {
		if (_is_last(page)) {
			_clear(page);
			break;
		} else {
			_clear(page);
			page++;;
		}
	}
}

void page_test()
{
/*
我的qemu跑出來是
HEAP_START = 0x800033f4(aligned to 0x80004000), HEAP_SIZE = 0x07ffcc0c,
num of reserved pages = 8, num of pages to be allocated for heap = 32756
TEXT:   0x80000000 -> 0x80002d28
RODATA: 0x80002d28 -> 0x80002ee5
DATA:   0x80003000 -> 0x80003000
BSS:    0x80003000 -> 0x800033f4
HEAP:   0x8000c000 -> 0x88000000 // 這裡我建議改成 aligned to 0x80004000 -> 0x88000000! 0x8000c000 其實是印出開始可以 allocated 的記憶體位置，所以會跟 p 的位置一樣!
                                 // 不然會覺得奇怪, HEAP 怎馬上接著  _num_page 的位置, 也就是 _alloc_start. 這中間應該有一塊 reserved_page 的空間
								 // 其實由於 num_reserved_pages 很小, 不需要太大空間. 因為是 metadata, 只記錄 flag 代表有沒有用過. 
								 // 我跑出來是 num of reserved pages = 8，sizeof(struct Page) 假設為 16 位元組（一個常見的結構體大小），這塊空間是 8 * 16 = 128 位元組。
p = 0x8000c000 //0x80004000 + 8 * 0x1000 (4KB (0x1000))
p2 = 0x8000e000 // p 占了兩個 page 
p3 = 0x8000e000




*/


	void *p = page_alloc(2);
	printf("p = %p\n", p);
	//page_free(p);

	void *p2 = page_alloc(7);
	printf("p2 = %p\n", p2);
	page_free(p2);

	void *p3 = page_alloc(4);
	printf("p3 = %p\n", p3);
}

