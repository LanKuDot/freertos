#ifndef SEMI_HOST_H
#define SEMI_HOST_H

#include <stddef.h>

/* Semihost system call reasons */
enum semihostCall {
	SYS_SYSTEM = 0x12	/* Pass command to the host. */
};

/* Passes a  command to the host command_line interpreter. */
int semihost_system( const char *cmd, size_t cmd_len );

#endif /* SEMI_HOST_H */
