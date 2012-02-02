
#include "syslog_xnix.h"

#include <string>

#ifndef NULL
	#define NULL 0
#endif
#ifndef FALSE
	#define FALSE 0
#endif
#ifndef TRUE
	#define TRUE 0
#endif


const char *CSyslog::libLevel[7] = {
								  "LOGL_PARSE"
								, "LOGL_ERR"
								, "LOGL_SQLOK"
								, "LOGL_ALLOC"
								, "LOGL_RECORD"
								, "LOGL_THESAURUS"
								, "LOGL_INFO"
							};
const char *CSyslog::libCategory[15] = {
								  "LOGC_PROG_START"
								, "LOGC_PROG_END"
								, "LOGC_THREAD_START"
								, "LOGC_THREAD_END"
								, "LOGC_PRELOAD"
								, "LOGC_SQLERR"
								, "LOGC_XMLERR"
								, "LOGC_FLUSH"
								, "LOGC_INDEXING"
								, "LOGC_SIGNAL"
								, "LOGC_THESAURUS"
								, "LOGC_STRUCTURE"
								, "LOGC_ACNX_OK"
								, "LOGC_HASVALUE"
								, "LOGC_SQLOK"
								};

CSyslog::CSyslog()
{
 //std::string s;
	this->where = TOTTY;
}

CSyslog::~CSyslog()
{
	this->close();
}

void CSyslog::open(const char *ident, int where)
{
	this->close();

	this->where = where;

	if(where == TOLOG)
	{
		openlog("phraseanet_cindexer", LOG_PID, LOG_DAEMON);
	}
}

void CSyslog::_log(PHRASEA_LOG_LEVEL level, PHRASEA_LOG_CATEGORY category, const char *fmt, ...)
{
	extern int debug_flag;
	if(!(debug_flag & (1<<level)))
		return;

	va_list vl;
	char buff[5000];
	va_start(vl, fmt);

	vsnprintf(buff, 5000, fmt, vl);
	std::string aa;

	if(this->where == TOLOG)
	{
		switch(level)
		{
			// case CSyslog::LOGL_DEBUG:
			case CSyslog::LOGL_PARSE:
			case CSyslog::LOGL_SQLOK:
			case CSyslog::LOGL_ALLOC:
				syslog(LOG_DEBUG, "%s", buff); 
				break;
			case CSyslog::LOGL_RECORD:
			case CSyslog::LOGL_THESAURUS:
			case CSyslog::LOGL_INFO:
				syslog(LOG_INFO, "%s", buff); 
				break;
			//case CSyslog::LOGL_WARNING:
			//	syslog(LOG_WARNING, "%s", buff);
			//	break;
			case CSyslog::LOGL_ERR:
				syslog(LOG_ERR, "%s", buff); 
				break;
			default:
				break;	
		}
	}
	else
	{
		// TOTTY
		printf("[%s].[%s] :\n%s\n", this->libLevel[level], this->libCategory[category], buff);
		fflush(stdout);
	}
}

void CSyslog::close()
{
	if(this->where == TOLOG)
		closelog();
	this->where = TOTTY;
}

