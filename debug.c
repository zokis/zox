#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>


void printf_debug(const char *format, va_list args) {
    printf(format, args);
    return ;
}
