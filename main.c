#define USE_STDPERIPH_DRIVER
#include "stm32f10x.h"

/* Scheduler includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include <string.h>

/* Filesystem includes */
#include "filesystem.h"
#include "fio.h"

/* String includes */
#include "string-util.h"

#include "semi_host.h"

extern const char _sromfs;

static void setup_hardware();

volatile xSemaphoreHandle serial_tx_wait_sem = NULL;
volatile xQueueHandle serial_rx_queue = NULL;

/* IRQ handler to handle USART2 interruptss (both transmit and receive
 * interrupts). */
void USART2_IRQHandler()
{
	static signed portBASE_TYPE xHigherPriorityTaskWoken;
	char rx_msg;		// Recieve the char from RX

	/* If this interrupt is for a transmit... */
	if (USART_GetITStatus(USART2, USART_IT_TXE) != RESET) {
		/* "give" the serial_tx_wait_sem semaphore to notfiy processes
		 * that the buffer has a spot free for the next byte.
		 */
		xSemaphoreGiveFromISR(serial_tx_wait_sem, &xHigherPriorityTaskWoken);

		/* Diables the transmit interrupt. */
		USART_ITConfig(USART2, USART_IT_TXE, DISABLE);
	}
	/* If this interrupt is for a receive... */
	else if ( USART_GetITStatus( USART2, USART_IT_RXNE ) != RESET )
	{
		/* Receive the char from rx */
		rx_msg = USART_ReceiveData( USART2 );

		/* Queue the received char */
		if ( !xQueueSendToBackFromISR( serial_rx_queue, &rx_msg, \
					&xHigherPriorityTaskWoken ) )
		{
			/*
			 * If there was an error queueing the recieved char,
			 * freeze.
			 */
			while(1);
		}
	}
	else {
		/* Only transmit and receive interrupts should be enabled.
		 * If this is another type of interrupt, freeze.
		 */
		while(1);
	}

	if (xHigherPriorityTaskWoken) {
		taskYIELD();
	}
}

void send_byte(char ch)
{
	/* Wait until the RS232 port can receive another byte (this semaphore
	 * is "given" by the RS232 port interrupt when the buffer has room for
	 * another byte.
	 */
	while (!xSemaphoreTake(serial_tx_wait_sem, portMAX_DELAY));

	/* Send the byte and enable the transmit interrupt (it is disabled by
	 * the interrupt).
	 */
	USART_SendData(USART2, ch);
	USART_ITConfig(USART2, USART_IT_TXE, ENABLE);
}

char recieve_byte()
{
	char msg;

	/* Wait for a byte to be queued by the recieve interrupt handler. */
	while( !xQueueReceive( serial_rx_queue, &msg, portMAX_DELAY ) );
	
	return msg;
}

void print_to_console( const char *str )
{
	/* Send the string to the console.
	 * fd = 1 for stdout. */
	fio_write( 1, str, strlen( str ) );
}

void sPuts( const char *str )
{
	/* Display the string with new line. */
	print_to_console( str );
	print_to_console( "\r\n" );
}

/*
 * Show all tasks and their states that are
 * running on the OS. */
void cmd_ps()
{
	char listBuf[512] = {0};
	sPuts( "Name\t      State Priority   Stack   Num" );
	vTaskList( listBuf );
	print_to_console( listBuf );
}

/*
 * The command would be passed to the host.
 */
void cmd_host( const char *cmd )
{
	/* The command format is :"host <command>" */
	const char *host_cmd = &cmd[5];

	semihost_system( host_cmd, strlen( host_cmd ) );
}

/* Display the content of the file specified. */
void cmd_cat( const char *filename )
{
	char buffer[129];
	int fd = fs_open( filename, 0, O_RDONLY ), count;

	if ( fd < 0 )
	{
		sPuts( "The file dose not exist!" );
		return;
	}

	while( ( count = fio_read( fd, buffer, sizeof(buffer) - 1 ) ) > 0 )
	{
		buffer[ count ] = '\0';
		print_to_console( buffer );
	}

	sPuts( "" );
	fio_close( fd );

	return;
}

/* Parse the command and return the command ID */
int getCommand( const char *cmd )
{
	/* Hello: Print out the welcome message. */
	if ( strncmp( cmd, "Hello", 5 ) == 0 )
		sPuts( "Hi, Welcome to FreeRTOS!" );
	/* ps: Show all the task that are on the OS. */
	else if ( strncmp( cmd, "ps", 2 ) == 0 )
		cmd_ps();
	/* host <command>: The command passed to the host. */
	else if ( strncmp( cmd, "host", 4 ) == 0 )
		cmd_host( cmd );
	/* cat <path/filename>: Display the content of the file. */
	else if ( strncmp( cmd, "cat", 3 ) == 0 )
		cmd_cat( &cmd[4] );
	/* Command not found, show the message. */
	else
		sPuts( "Command Not found!" );
}

#define MAX_SERIAL_LEN	100
/* Function Key ASCII code macro */
#define ESC				27
#define BACKSPACE		127
void shellEnv()
{
	char serial_buf[ MAX_SERIAL_LEN ], ch;
	char prompt[] = "LanKuDot@FreeRTOS~$ ", newLine[] = "\n\r";
	int curr_pos, done;

	/* Infinite loop for running shell environmrnt. */
	while (1)
	{
		/* Show prompt each time waiting for user input. */
		print_to_console( prompt );

		/* Initialize the relatived variable. */
		curr_pos = 0;
		done = 0;

		do
		{
			/* Recieve a byte from the RS232 port ( this call will
			 * block ).
			 */
			ch = recieve_byte();

			/* If the byte is an end-of-line character, than finish
			 * the string and indicate we are done.
			 */
			if ( curr_pos >= MAX_SERIAL_LEN-1 || (ch == '\n') || (ch == '\r') )
			{
				serial_buf[ curr_pos ] = '\0';
				done = -1;
			}
			/* Backspace key pressed */
			else if ( ch == BACKSPACE )
			{
				/* The char is not at the begin of the line. */
				if ( curr_pos != 0 )
				{
					--curr_pos;
					/* Cover the last character with space, and shift the 
					 * cursor left. */
					print_to_console( "\b \b" );
				}
			}
			/* Function/Arrow Key pressed */
			else if ( ch == ESC )
			{
				/* Arrow Key: ESC[A ~ ESC[D 
				 * Function Key: ESC[1~ ~ ESC[6~ */
				ch = recieve_byte();

				if ( ch == '[' )
				{
					ch = recieve_byte();
					if ( ch >= '1' && ch <= '6' )
					{
						/* Discard '~' */
						recieve_byte();
					}

					continue;
				}
			}
			/* Otherwise, add the character to the response string */
			else
			{
				serial_buf[ curr_pos++ ] = ch;
				/* Display the char that just typed */
			//	print_to_console( &ch );		// Bug: Press 1, print a lot...
				fio_write( 1, &ch, 1 );
			}
		} while ( !done );	// end character recieving loop

		/* Direct to the new line */
		print_to_console( newLine );

		/* Deal with the command! */
		getCommand( serial_buf );

	}	// end infinite while loop
}	// end of function shellEnv

int main()
{
	init_rs232();
	enable_rs232_interrupts();
	enable_rs232();
	
	fs_init();
	fio_init();
	
	/* Create the queue used by the serial task.  Messages for write to
	 * the RS232. */
	serial_rx_queue = xQueueCreate( 1, sizeof(char) );

	vSemaphoreCreateBinary(serial_tx_wait_sem);

	/* Create a task that run the sell environment. */
	xTaskCreate( shellEnv, (signed portCHAR *)"Shell",
			512, NULL, tskIDLE_PRIORITY + 5, NULL );

	/* Start running the tasks. */
	vTaskStartScheduler();

	return 0;
}

void vApplicationTickHook()
{
}
