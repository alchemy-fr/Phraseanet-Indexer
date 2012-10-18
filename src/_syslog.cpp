#ifdef WIN32
#include "../_WIN32 (visual C++ 2008)/syslog_win32.cpp"
#else
#include "xNIX/syslog_xnix.cpp"
#endif
