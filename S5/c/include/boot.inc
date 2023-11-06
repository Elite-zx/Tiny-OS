;------------------------
; Author: Xun Morris |
; Time: 2023-11-05 |
;------------------------

;------------------------
; kernel loader attribute
;------------------------
; the location in memory where the loader is loaded
LOADER_START_SECTOR  equ 0x2

; The location of the loader on the hard disk (LBA)
LOADER_BASE_ADDR equ 0x900 


;------------------------
; GDT attribute
;------------------------
; bit G (23) -> Units for segment boundary: 1 byte / 4KB*
DESC_G_4K equ 1_000_0000_0000_0000_0000_0000b

; bit D (22) -> 32bits operand
DESC_D_32 equ 1_00_0000_0000_0000_0000_0000b

; bit L (21) -> 32bits code segment
DESC_L    equ 0_0_0000_0000_0000_0000_0000b

; bit AVL (20) -> the segment is available
DESC_AVL  equ 0_0000_0000_0000_0000_0000b

; bit P (15) -> the segment is present
DESC_P equ 1_000_0000_0000_0000b

; bit DPL(13~14) -> 4 possible values of Descriptor Privilege Level
DESC_DPL_0 equ 00_0_0000_0000_0000b
DESC_DPL_1 equ 01_0_0000_0000_0000b
DESC_DPL_2 equ 10_0_0000_0000_0000b
DESC_DPL_3 equ 11_0_0000_0000_0000b

; bit S (12) -> code/data is not a system segment but sys is 
DESC_S_CODE equ 1_0000_0000_0000b
DESC_S_DATA equ DESC_S_CODE
DESC_S_sys equ 0_0000_0000_0000b

; bit TYPE (8~11) -> type for code(executable)/data(readable) segment  
DESC_TYPE_CODE equ 1000_0000_0000b
DESC_TYPE_DATA equ 0010_0000_0000b

; second part of segment boundary (16~19)-> 4GB
DESC_LIMIT_CODE2 equ 1111_0000_0000_0000_0000b
DESC_LIMIT_DATA2 equ DESC_LIMIT_CODE2
DESC_LIMIT_DISPLAY2 equ 0000_0000_0000_0000_0000b

; high 4 bytes of Descriptor
DESC_CODE_HIGH4 equ (0x00<<24) + DESC_G_4K + DESC_D_32 + DESC_L + \
DESC_AVL + DESC_LIMIT_CODE2 + DESC_P + DESC_DPL_0 + DESC_S_CODE + \
+ DESC_TYPE_CODE + 0x00

DESC_DATA_HIGH4 equ (0x00<<24) + DESC_G_4K + DESC_D_32 + DESC_L + \
DESC_AVL + DESC_LIMIT_DATA2 + DESC_P + DESC_DPL_0 + DESC_S_DATA + \
+ DESC_TYPE_DATA + 0x00

DESC_DISPLAY_HIGH4 equ (0x00<<24) + DESC_G_4K + DESC_D_32 + DESC_L + \
DESC_AVL + DESC_LIMIT_DISPLAY2 + DESC_P + DESC_DPL_0 + DESC_S_DATA + \
+ DESC_TYPE_DATA + 0x0b


;------------------------
; selector attribute
;------------------------
; bit RPL(0~1) -> 4 possible values of RPL
RPL0 equ 00b
RPL1 equ 01b
RPL2 equ 10b
RPL3 equ 11b

; bit TI (2) -> Table indicator
TI_GDT equ 000b
TI_LDT equ 100b

;------------------------------------
; page directory table attribute
;------------------------------------
; The starting address of the page directory table
PAGE_DIR_TABLE_POS equ 0x100000

;------------------------------------
; page table attribute
;------------------------------------
; bit present -> indicates whether the page exists in memory
PG_P equ 1b

; bit read and write 
PG_RW_R equ 00b
PG_RW_W equ 10b

; user and root bit -> whether it can be accessed by programs with privilege level 3
PG_US_S equ 000b
PG_US_U equ 100b

;------------------------------------
; kernel attribute
;------------------------------------
KERNEL_START_SECTOR equ 0x9
KERNEL_BIN_BASE_ADDR equ 0x70000
