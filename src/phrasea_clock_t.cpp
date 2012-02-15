#include <memory.h>
#include "phrasea_clock_t.h"


# ifdef WIN32

void startChrono(CHRONO &chrono)
{
	chrono = GetTickCount();
}
float stopChrono(CHRONO &chrono)
{
	return((float)((GetTickCount()-chrono) / 1000.0));
}


# else

void startChrono(CHRONO &chrono)
{
	gettimeofday(&chrono, NULL);
	return;
}
float stopChrono(CHRONO &chrono)
{
	struct timeval t;
	gettimeofday(&t, NULL);
	t.tv_sec  -= chrono.tv_sec;
	t.tv_usec -= chrono.tv_usec;
	if(t.tv_usec < 0)
	{
		t.tv_sec--;
		t.tv_usec += 1000000;
	}
	return((float)(t.tv_sec) + ((float)(t.tv_usec))/1000000);
}

#endif



#ifndef WIN32
char *_strupr( char *string )
{
	for(register char *p=string; *p; p++)
		*p = (*p>='a' && *p<='z') ? (*p-'a')+'A' : *p;
	return(string);
}
#endif


