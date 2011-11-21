#ifndef _SYSLOG_OSX_INCLUDED
#define _SYSLOG_OSX_INCLUDED 1

#include <stdio.h>
#include <stdarg.h>
#include <syslog.h>

class CSyslog
{
	private:
		static unsigned short category[13];
		int where;
	public:
		enum { TOTTY, TOLOG };

		enum PHRASEA_LOG_LEVEL {
						  LOGL_PARSE = 0
						, LOGL_ERR = 1
						, LOGL_SQLOK = 2
						, LOGL_ALLOC = 3
						, LOGL_RECORD = 4
						, LOGL_THESAURUS = 5
						, LOGL_INFO = 6
						};
		static const char *libLevel[7];

		enum PHRASEA_LOG_CATEGORY {
						  LOGC_PROG_START   = 0
						, LOGC_PROG_END     = 1
						, LOGC_THREAD_START = 2
						, LOGC_THREAD_END   = 3
						, LOGC_PRELOAD      = 4
						, LOGC_SQLERR       = 5
						, LOGC_XMLERR       = 6
						, LOGC_FLUSH        = 7
						, LOGC_INDEXING     = 8
						, LOGC_SIGNAL       = 9
						, LOGC_THESAURUS    = 10
						, LOGC_STRUCTURE    = 11
						, LOGC_ACNX_OK      = 12
						, LOGC_HASVALUE     = 13
						, LOGC_SQLOK        = 14
						};
		static const char *libCategory[15];

		CSyslog();
		~CSyslog();
		void open(const char *appname, int where);
		void _log(PHRASEA_LOG_LEVEL level, PHRASEA_LOG_CATEGORY category, const char *fmt, ...);
		void close();
};


#endif
