//includes character array, support a-z and 0-9, no special characters or ALT, SHIFT, CAPS LOCK
#include "keyboard_map.h"

#define LINES 25
#define COLUMNS_IN_LINE 80
#define BYTES_FOR_EACH_ELEMENT 2
#define SCREENSIZE BYTES_FOR_EACH_ELEMENT * COLUMNS_IN_LINE * LINES

#define KEYBOARD_DATA_PORT 0x60
#define KEYBOARD_STATUS_PORT 0x64
#define IDT_SIZE 256
#define INTERRUPT_GATE 0x8e
#define KERNEL_CODE_SEGMENT_OFFSET 0x08

#define ENTER_KEY_CODE 0x1C

extern unsigned char keyboard_map[128];
extern void keyboard_handler(void);
extern char read_port(unsigned short port);
extern void write_port(unsigned short port, unsigned char data);
extern void load_idt(unsigned long *idt_prt);

//video memory begins
char *vidptr = (char*)0xb8000;

//current cursor loc
unsigned int current_loc = 0;
//Interrupt Descriptor Table
//processor looks this up when it recieves an interrupt signal number
//this is how we'll get keyboard input, via interrupts sent with the PIC
struct IDT_entry{
	unsigned short int offset_lowerbits;
	unsigned short int selector;
	unsigned char zero;
	unsigned char type_attr;
	unsigned short int offset_higherbits;
};

struct IDT_entry IDT[IDT_SIZE];

//initializes and remaps the Interrupt Descriptor Table
void idt_init(void){
	unsigned long keyboard_address;
	unsigned long idt_address;
	unsigned long idt_ptr[2];

	//populate IDT entry of keyboard's interrupt
	keyboard_address = (unsigned long)keyboard_handler;
	IDT[0X21].offset_lowerbits = keyboard_address & 0xffff;
	IDT[0X21].selector = KERNEL_CODE_SEGMENT_OFFSET; //kernel code segment offset
	IDT[0X21].zero = 0;
	IDT[0X21].type_attr = INTERRUPT_GATE; //interrupt gate
	IDT[0X21].offset_higherbits = (keyboard_address & 0xffff0000) >> 16;

	/*		ports
	*			PIC1	PIC2
	*command	0x20	0xA0
	*Data		0x21	0xA1
	*/

	//ICW1 - begin initialization, tells PICs to wait for 3 more initialization words
	write_port(0x20, 0x11);
	write_port(0xA0, 0x11);

	/*ICW2 - remap offset address of IDT
	*
	*In x86 protected mode, we have to remap PICs beyond x20 because Intel have designated the first 32 interrupts as "reserved" for CPU exceptions
	*/

	write_port(0x21, 0x20);
	write_port(0xA1, 0x28);
	
	//ICW3 - setup cascading, won't be using, set to 0
	write_port(0x21, 0x00);
	write_port(0xA1, 0x00);

	//ICW4 - environment info, tell PICs we are running in 80x86
	write_port(0x21, 0x01);
	write_port(0xA1, 0x01);
	//initialization finished

	//mask interrupts
	write_port(0x21, 0xff);
	write_port(0xA1, 0xff);

	//fill the IDT descriptor
	idt_address = (unsigned long)IDT;
	idt_ptr[0] = (sizeof (struct IDT_entry) * IDT_SIZE) + ((idt_address & 0xffff) << 16);
	idt_ptr[1] = idt_address >> 16;

	load_idt(idt_ptr);
}

void kb_init(void){
	//0xFD is 11111101 - enables only IRQ1 (keyboard)
	write_port(0x21, 0xFD);
}

void kprint(const char *str){
	unsigned int i = 0;
	while(str[i] != '\0'){
		vidptr[current_loc++] = str[i++];
		//color of text
		vidptr[current_loc++] = 0x07;
	}
}

void kprint_newline(void){
	unsigned int line_size = BYTES_FOR_EACH_ELEMENT * COLUMNS_IN_LINE;
	current_loc = current_loc + (line_size - current_loc % (line_size));
}

void clear_screen(void){
	unsigned int i = 0;

	while(i < SCREENSIZE){
		vidptr[i++] = ' ';
		//changes color of screen
		vidptr[i++] = 0x07;
	}
}

void keyboard_handler_main(void){
	unsigned char status;
	char keycode;

	//write EOI, End Of Interrupts
	write_port(0x20, 0x20);

	//reads if there is anything in the keyboard buffer
	status = read_port(KEYBOARD_STATUS_PORT);

	//lowest bit of status will be set if buffer is not empty
	if(status & 0x01){
		keycode = read_port(KEYBOARD_DATA_PORT);
		if(keycode < 0){
			return;
		}

		if(keycode == ENTER_KEY_CODE){
			kprint_newline();
			return;
		}
		vidptr[current_loc++] = keyboard_map[(unsigned char)keycode];
		vidptr[current_loc++] = 0x07;
	}
}

void kmain(void){

	//message to be written to the screen
	const char *str = "Wow, a kernel! Now with keyboard!";

	clear_screen();
	kprint(str);
	kprint_newline();
	kprint_newline();

	idt_init();
	kb_init();

	while(1);
}
