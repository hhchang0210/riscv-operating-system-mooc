#include "syscall.h"

.global gethid
gethid:
	
	
	li a7, SYS_gethid  # 1. 將系統呼叫號載入 a7
	                   # a7 暫存器被用來傳遞系統呼叫號。SYS_gethid 是一個常數（這裡是 1），用來告訴核心：「我想要請求的是『獲取 hart ID』這項服務」。
    ecall              # 2. Environment Call 指令。這是整個流程的觸發點。執行這條指令會故意引發一個同步的 Exception (異常)，CPU 硬體會偵測到這個事件。
	/*
	硬體自動操作:
    當 ecall 被執行，CPU 硬體會自動地
    將當前的權限等級從 U-Mode 提升到 M-Mode。
    將 ecall 指令的位址存入 mepc。
    將 mcause 設為 8 (代表 "Environment call from U-mode")。
    強制跳轉到 mtvec 中預設好的 trap_vector 位址。
	*/

    ret                # 3. 從核心返回後，再返回給呼叫者


.global sum
sum:
	
	
	li a7, SYS_sum  # 1. 將系統呼叫號載入 a7
	                   # a7 暫存器被用來傳遞系統呼叫號。SYS_gethid 是一個常數（這裡是 1），用來告訴核心：「我想要請求的是『獲取 hart ID』這項服務」。
    ecall              # 2. Environment Call 指令。這是整個流程的觸發點。執行這條指令會故意引發一個同步的 Exception (異常)，CPU 硬體會偵測到這個事件。
	/*
	硬體自動操作:
    當 ecall 被執行，CPU 硬體會自動地
    將當前的權限等級從 U-Mode 提升到 M-Mode。
    將 ecall 指令的位址存入 mepc。
    將 mcause 設為 8 (代表 "Environment call from U-mode")。
    強制跳轉到 mtvec 中預設好的 trap_vector 位址。
	*/

    ret                # 3. 從核心返回後，再返回給呼叫者
