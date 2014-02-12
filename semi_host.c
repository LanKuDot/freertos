#include "semi_host.h"

/* Semihost system call parameters */
typedef union param_t {
	int pdInt;
	void *pdVoidPtr;
	char *pdChrPtr;
} param;

static int host_call( enum semihostCall reason, void *arg ) __attribute__ ((naked));
static int host_call( enum semihostCall reason, void *arg )
{
	__asm__( \
			"bkpt	0xAB\n" \
			"nop		\n" \
			"bx		lr	\n" \
			::: );
}

int semihost_system( const char *cmd, size_t cmd_len )
{
	/* Parameters:
	 * Word 1: points to a string to be passed 
	 * 		to the host command-line interpreter.
	 * Word 2: the length of the string.
	 */
	param para[3] = {
		{ .pdChrPtr = cmd },
		{ .pdInt = cmd_len }
	};

	/* The return status. */
	return host_call( SYS_SYSTEM, para );
}
