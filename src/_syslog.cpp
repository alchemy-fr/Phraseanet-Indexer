#ifdef WIN32
#include "../WIN32/syslog_win32.cpp"
#else
#include "xNIX/syslog_xnix.cpp"
#endif
