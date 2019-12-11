;;kernal.asm
bits 32		;nasm directive - 32 bits
section .text
	;multiboot spec, allows grub multiboot
	align 4
	dd 0x1BADB002	;magic number
	dd 0X00			;flag
	dd - (0x1BADB002 + 0X00) ;checksum, m+f+c should be zero

global start
global keyboard_handler
global read_port
global write_port
global load_idt

extern kmain		;kmain is defined in the c file
extern keyboard_handler_main

read_port:
	mov edx, [esp + 4]
	in al, dx
	ret

write_port:
	mov edx, [esp + 4]
	mov al, [esp + 4 + 4]
	out dx, al
	ret

load_idt:
	mov edx, [esp + 4]
	lidt [edx]
	sti
	ret

;calls a C functions everytime the keyboard is used
keyboard_handler:
	call	keyboard_handler_main
	iretd

start:
	cli		;block interrupts
	mov esp, stack_space	;set stack pointer
	call kmain
	hlt		;halt the CPU

section .bss
resb 8192	;8KB for stack
stack_space:
