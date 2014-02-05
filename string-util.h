#ifndef STRING_UTIL
#define STRING_UTIL

#include <stddef.h>

void *memset( void *dest, int c, size_t n );
void *memcpy( void *dest, const void *src, size_t n );
char *strchr( const char *s, int c );
char *strcpy( char *dest, const char *src );
char *strncpy( char *dest, const char *src, size_t n );
size_t strlen( const char *str );
int strncmp( const char *str1, const char *str2, size_t n );

#endif /*STRING_UTIL*/
