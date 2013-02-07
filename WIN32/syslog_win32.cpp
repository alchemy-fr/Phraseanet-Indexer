#include "syslog_win32.h"

#include "messages.h"

#ifndef NULL
	#define NULL 0
#endif

namespace util
{
    //
    // Load a message resource fom the .exe and format it
    // with the passed insertions
    //
    UINT LoadMessage( DWORD dwMsgId, PTSTR pszBuffer, UINT cchBuffer, ... )
    {
        va_list args;
        va_start( args, cchBuffer );

        return FormatMessage(
          FORMAT_MESSAGE_FROM_HMODULE,
          NULL,         // Module (e.g. DLL) to search for the Message. NULL = own .EXE
          dwMsgId,      // Id of the message to look up (aus "Messages.h")
          LANG_NEUTRAL, // Language: LANG_NEUTRAL = current thread's language
          pszBuffer,    // Destination buffer
          cchBuffer,    // Character count of destination buffer
          &args         // Insertion parameters
        );
    }

    //
    // Installs our app as a source of events
    // under the name pszName into the registry
    //

	// PCTSTR undef ?
    void AddEventSource(  const TCHAR *pszName, DWORD dwCategoryCount )
    {
        HKEY    hRegKey = NULL;
        DWORD   dwError = 0;
        TCHAR   szPath[ MAX_PATH ];

        _stprintf( szPath,
          _T("SYSTEM\\CurrentControlSet\\Services\\EventLog\\Application\\%s"),
          pszName );

        // Create the event source registry key
        dwError = RegCreateKey( HKEY_LOCAL_MACHINE, szPath, &hRegKey );

        // Name of the PE module that contains the message resource
        GetModuleFileName( NULL, szPath, MAX_PATH );

        // Register EventMessageFile
        dwError = RegSetValueEx( hRegKey,
                  _T("EventMessageFile"), 0, REG_EXPAND_SZ,
                  (PBYTE) szPath, (_tcslen( szPath) + 1) * sizeof TCHAR );


        // Register supported event types
        DWORD dwTypes = EVENTLOG_ERROR_TYPE |
              EVENTLOG_WARNING_TYPE | EVENTLOG_INFORMATION_TYPE;
        dwError = RegSetValueEx( hRegKey, _T("TypesSupported"), 0, REG_DWORD,
                                (LPBYTE) &dwTypes, sizeof dwTypes );

        // If we want to support event categories,
        // we have also to register the CategoryMessageFile.
        // and set CategoryCount. Note that categories
        // need to have the message ids 1 to CategoryCount!

        if( dwCategoryCount > 0 )
		{

            dwError = RegSetValueEx( hRegKey, _T("CategoryMessageFile"),
                      0, REG_EXPAND_SZ, (PBYTE) szPath,
                      (_tcslen( szPath) + 1) * sizeof TCHAR );

            dwError = RegSetValueEx( hRegKey, _T("CategoryCount"), 0, REG_DWORD,
                      (PBYTE) &dwCategoryCount, sizeof dwCategoryCount );
        }

        RegCloseKey( hRegKey );
    }

}   // namespace util



unsigned short CSyslog::category[16] = {
							  MSGCAT_PROG_START
							, MSGCAT_PROG_END
							, MSGCAT_THREAD_START
							, MSGCAT_THREAD_END
							, MSGCAT_PRELOAD
							, MSGCAT_SQLERR
							, MSGCAT_XMLERR
							, MSGCAT_FLUSH
							, MSGCAT_INDEXING
							, MSGCAT_SIGNAL
							, MSGCAT_THESAURUS
							, MSGCAT_STRUCTURE
							, MSGCAT_ACNX_OK
							, MSGCAT_HASVALUE
							, MSGCAT_SQLOK
							, MSGCAT_ALLOC
					};

char *CSyslog::libLevel[7] = {
								  "LOGL_PARSE"
								, "LOGL_ERR"
								, "LOGL_SQLOK"
								, "LOGL_ALLOC"
								, "LOGL_RECORD"
								, "LOGL_THESAURUS"
								, "LOGL_INFO"
							};
char *CSyslog::libCategory[16] = {
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
								, "LOGC_ALLOC"
								};

CSyslog::CSyslog()
{
	this->ident = NULL;
	this->hEventLog = NULL;
	this->where = TOTTY;
}

CSyslog::~CSyslog()
{
	this->close();
}

void CSyslog::open(const TCHAR *ident, int where)
{
	int l;
	this->close();

	this->where = where;

	if(where == TOLOG)
	{
		this->install(ident);

		if(ident && (this->ident = (TCHAR *)(malloc(l=strlen(ident)+1)) ))	// strlen to count bytes !
		{
			memcpy(this->ident, ident, l);
			this->hEventLog = RegisterEventSource( NULL, this->ident);
		}
	}
}

void CSyslog::install(const TCHAR *ident)
{
	util::AddEventSource(ident, 12);
}


void CSyslog::_log(PHRASEA_LOG_LEVEL level, PHRASEA_LOG_CATEGORY category, TCHAR *fmt, ...)
//void CSyslog::_log(PHRASEA_LOG_LEVEL level, PHRASEA_LOG_CATEGORY category, const char *fmt, ...)
{
	extern int debug_flag;
	if(!(debug_flag & (1<<level)))
		return;

	va_list vl;
	char buff[5000];
	va_start(vl, fmt);
#ifdef UNICODE
	_vsnwprintf(buff, 5000, fmt, vl);
#else
	_vsnprintf(buff, 5000, fmt, vl);
#endif

//	printf("%s\n", buff);

	if(where == TOLOG)
	{
		if(!this->hEventLog)
			return;

		const TCHAR *aInsertions[] = { buff };

		switch(level)
		{
			case CSyslog::LOGL_PARSE:
			case CSyslog::LOGL_SQLOK:
			case CSyslog::LOGL_ALLOC:
				ReportEvent(
								this->hEventLog,                  // Handle to the eventlog
								EVENTLOG_INFORMATION_TYPE,      // Type of event
								this->category[category],               // Category (could also be 0)
								EVENT_ALL,               // Event id
								NULL,                       // User's sid (NULL for none)
								1,                          // Number of insertion strings
								0,                          // Number of additional bytes
								aInsertions,                       // Array of insertion strings
								NULL                        // Pointer to additional bytes
							);
				break;
			case CSyslog::LOGL_RECORD:
			case CSyslog::LOGL_THESAURUS:
			case CSyslog::LOGL_INFO:
				ReportEvent(
								this->hEventLog,                  // Handle to the eventlog
								EVENTLOG_WARNING_TYPE,      // Type of event
								this->category[category],               // Category (could also be 0)
								EVENT_ALL,               // Event id
								NULL,                       // User's sid (NULL for none)
								1,                          // Number of insertion strings
								0,                          // Number of additional bytes
								aInsertions,                       // Array of insertion strings
								NULL                        // Pointer to additional bytes
							);
				break;
			case CSyslog::LOGL_ERR:
				ReportEvent(
								this->hEventLog,                  // Handle to the eventlog
								EVENTLOG_ERROR_TYPE,      // Type of event
								this->category[category],               // Category (could also be 0)
								EVENT_ALL,               // Event id
								NULL,                       // User's sid (NULL for none)
								1,                          // Number of insertion strings
								0,                          // Number of additional bytes
								aInsertions,                       // Array of insertion strings
								NULL                        // Pointer to additional bytes
							);
				break;
			default:
				break;
		}
	}
	else
	{
		// TOTTY
		printf("[%s].[%s] :\n%s\n", this->libLevel[level], this->libCategory[category], buff);
//		printf("%s\n", buff);
	}
}

void CSyslog::close()
{
	if(this->hEventLog)
	{
		DeregisterEventSource( this->hEventLog );
		this->hEventLog = NULL;
	}
	if(this->ident)
	{
		free((void *)this->ident);
		this->ident = NULL;
	}
	this->where = TOTTY;
}


