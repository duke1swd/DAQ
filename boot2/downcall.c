/*
 * Include this in any module that wants to call functions
 * that are part of boot2.
 */

#define DOWNCALL

/*
 * Globals we share with boot2.
 */
unsigned int sd_lba:24 @ 0;
unsigned char blink_code @ 3;
unsigned char blink_sub_code @ 4;
unsigned char blink_detail @ 5;

#define	VECTOR_BASE	0x820


void
sd_init(void)
{
#asm
	DW __GOTO (VECTOR_BASE)
#endasm
}

void
sd_read(void)
{
#asm
	DW __GOTO (VECTOR_BASE + 4)
#endasm
}

void
waitms(void)
{
#asm
	DW __GOTO (VECTOR_BASE + 8)
#endasm
}

void
blink_debug_x(void)
{
#asm
	DW __GOTO (VECTOR_BASE + 12)
#endasm
}

void
blink_error_code_x(void)
{
#asm
	DW __GOTO (VECTOR_BASE + 16)
#endasm
}
