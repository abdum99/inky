#ifndef __DEBUG_H
#define __DEBUG_H

#include <stdio.h>

#if DEBUG
	#define Debug(fmt,...) printf("%s[%d]:"fmt,__FILE__,__LINE__,##__VA_ARGS__)
#else
	#define Debug(fmt,...) printf(fmt,##__VA_ARGS__)
#endif

#endif

