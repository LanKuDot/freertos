#include <stddef.h>
#include <stdint.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "string-util.h"

#define ALIGN (sizeof(size_t))
#define ONES ((size_t)-1/UCHAR_MAX)                                                                      
#define HIGHS (ONES * (UCHAR_MAX/2+1))
#define HASZERO(x) ((x)-ONES & ~(x) & HIGHS)

#define SS (sizeof(size_t))
void *memset(void *dest, int c, size_t n)
{
	unsigned char *s = dest;
	c = (unsigned char)c;
	for (; ((uintptr_t)s & ALIGN) && n; n--) *s++ = c;
	if (n) {
		size_t *w, k = ONES * c;
		for (w = (void *)s; n>=SS; n-=SS, w++) *w = k;
		for (s = (void *)w; n; n--, s++) *s = c;
	}
	return dest;
}

void *memcpy(void *dest, const void *src, size_t n)
{
	void *ret = dest;
	
	//Cut rear
	uint8_t *dst8 = dest;
	const uint8_t *src8 = src;
	switch (n % 4) {
		case 3 : *dst8++ = *src8++;
		case 2 : *dst8++ = *src8++;
		case 1 : *dst8++ = *src8++;
		case 0 : ;
	}
	
	//stm32 data bus width
	uint32_t *dst32 = (void *)dst8;
	const uint32_t *src32 = (void *)src8;
	n = n / 4;
	while (n--) {
		*dst32++ = *src32++;
	}
	
	return ret;
}

char *strchr(const char *s, int c)
{
	for (; *s && *s != c; s++);
	return (*s == c) ? (char *)s : NULL;
}

char *strcpy(char *dest, const char *src)
{
	const unsigned char *s = src;
	unsigned char *d = dest;
	while ((*d++ = *s++));
	return dest;
}

char *strncpy(char *dest, const char *src, size_t n)
{
	const unsigned char *s = src;
	unsigned char *d = dest;
	while (n-- && (*d++ = *s++));
	return dest;
}

size_t strlen( const char *str )
{
	const char *a = str;
	const size_t *w;

	for ( ; (uintptr_t) str % ALIGN; str++ )
		if ( !*str ) return ( str - a );

	for ( w = (const void *)str; !HASZERO(*w); w++ )
		;
	for ( str = (const void *)w; *str; str++ )
		;

	return ( str - a );
}

int strncmp( const char *str1, const char *str2, size_t n )
{
	const char *s1 = str1, *s2 = str2;

	for ( ; n > 0; ++s1, ++s2, --n  )
	{
		if ( *s1 != *s2 )
			return *s1 - *s2;
	}

	return 0;
}

char *strcat( char *dest, const char *src )
{
	char *d = dest;
	const char *s = src;
	strcpy( &d[ strlen(d) ], s );
	return dest;
}

/* Write formatted data to string */
int sprintf( char *str, const char *format, ... )
{
	va_list ap;
	char *s = str, *ap_str, itoa_buf[32] = {0}, *ITOA = itoa_buf;
	const char *f = format;

	va_start( ap, format );

	/* Parse the formatted string. */
	while ( *f != '\0' )
	{
		/* Normal text. */
		if ( *f != '%' )
			*s++ = *f++;
		/* Get formatted Indicator and discard %. */
		else
		{
			switch( *++f )
			{
				case 's':	/* String */
					ap_str = va_arg( ap, char * );

					while( *ap_str != '\0' )
						*s++ = *ap_str++;

					break;
				case 'c':	/* Character */
					*s++ = ( char )va_arg( ap, int );
					break;
				case 'u':	/* Unsigned decimal integer */
					itoa( va_arg( ap, unsigned ), itoa_buf, 10 );
					ITOA = itoa_buf;

					while( *ITOA != '\0' )
						*s++ = *ITOA++;

					break;
				default:
					break;
			}	// end of switch ( *++f )

			++f;
		}	// end of else ( *f == '%' )
	}	// end of while( *f )

	va_end( ap );
	return strlen( str );
}

char *itoa( int value, char *str, int base )
{
	static char buf[32] = {0};
	int i = 30, sign = 0;

	/* Deal with 0 */
	if ( value == 0 )
	{
		buf[0] = '0';
		buf[1] = '\0';
		strcpy( str, buf );
		return;
	}

	/* Negative number */
	if ( value < 0 )
	{
		sign = 1;
		value = value * -1;
	}

	/* Converting int to string */
	switch( base )
	{
		case 10:	/* Decimal */
			for ( ; value && i; --i, value /= 10 )
				buf[i] = "0123456789"[ value % 10 ];
			break;
		case 16:	/* Hexdecimal */
			for ( ; value && i; --i, value /= 16 )
				buf[i] = "0123456789ABCDEF"[ value % 16 ];
			break;
		case 8:		/* Optical */
			for( ; value && i; --i, value /= 8 )
				buf[i] = "01234567"[ value % 8 ];
			break;
	}

	/* Append the minus if needs. */
	if ( sign == 1 )
	{
		buf[i] = '-';
		--i;
	}

	strcpy( str, &buf[i+1] );

	return str;
}
