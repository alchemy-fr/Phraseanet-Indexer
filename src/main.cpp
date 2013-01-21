// #define DEBUG_THESAURUS

#include "_version.h"

#include "platform_dependent.h"

#include <stdio.h>
#include <cctype>
#include <locale.h>


#include <stdlib.h>
#include <signal.h>
#include "_syslog.h"
// #include <basetsd.h>

#ifdef WIN32
#include <process.h>
#include "../WIN32/nt_service.h"
#else
#include <time.h>
#include <pthread.h>
#endif

#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

#if defined(LIBXML_XPATH_ENABLED) && defined(LIBXML_SAX1_ENABLED) && defined(LIBXML_OUTPUT_ENABLED)
#else
#error XPath support not compiled in libxml
#endif


#include "consts.h"
#include "phrasea_clock_t.h"
// Source:  http://code.jellycan.com/simpleopt/
#include "../simpleopt/SimpleOpt.h"
// #include "./simpleopt/SimpleGlob.h"


//#include "phrasea_clock_t.h"

#include "dom.h"
#include "connbas_abox.h"
#include "connbas_dbox.h"
#include "sbas.h"

#include "sockets.h"

#include "indexer.h"
#include "../libstemmer_c/libstemmer/modules.h"
#include "stemmer_grp.h"

// prototypes local fcts
THREAD_ENTRYPOINT thread_index(void *parm);


// prototypes external fcts
void saveCterms(CIndexer *indexer);
void callbackRecord(CConnbas_dbox *connbas, unsigned int record_id, char *xml, unsigned long len);


// =============================== true globals =======================
static char *server_args[] = {
	(char *) "this_program", /* this string is not used */
	(char *) "--datadir=.",
	(char *) "--key_buffer_size=32M"
};
static char *server_groups[] = {
	(char *) "embedded",
	(char *) "server",
	(char *) "this_program_SERVER",
	(char *) NULL
};


// ====================== a arg buffer coming from file ==============
char file_buff[1000];


// ============ vars extracted from the command line ===========
const char *arg_host = _T("127.0.0.1");
unsigned int arg_port = MYSQL_PORT; // 3306
const char *arg_base = _T("phrasea");
const char *arg_user = _T("root");
const char *arg_pswd = _T("");
const char *arg_clng = "fr";
int narg_stem = 0;
const char *arg_stem[50];
const char *arg_mycharset = NULL;
unsigned int arg_socket = 0;
int arg_sbas = -1; // WARNING : sbas_id should be unsigned
int debug_flag = 0;
int arg_flush = 50;
bool version_flag = false;
bool help_flag = false;
bool nolog_flag = false;
bool oldsbas_flag = false;
bool quit_flag = false;
int wait_loop;			// check sbas every 10 sec (10...0)

#ifdef INSTALL_AS_NT_SERVICE
bool install_flag = false;
bool remove_flag = false;
bool run_flag = false;
#endif

// ========================== pool of sbas ===========================
CSbasList sbasPool;

// =================== flag to low to stop the pgm ===================
bool running; // lowed by sigint (ctrl-C)



CSyslog zSyslog; // , LOG_PID, LOG_DAEMON);

// ======================================================================




//===================================================================================================================
// MAIN
//===================================================================================================================


// define the ID values to indentify the option

enum
{
	OPT_HELP, OPT_VERSION, OPT_FLAG, OPT_ARG, OPT_HOST, OPT_PORT, OPT_BASE, OPT_USER, OPT_PSWD, OPT_OLDSBAS, OPT_CLNG, OPT_STEM, OPT_NOLOG, OPT_DEBUG, OPT_INSTALL, OPT_REMOVE, OPT_RUN, OPT_SOCKET, OPT_MYCHARSET, OPT_OPTFILE, OPT_FLUSH, OPT_QUIT /*, OPT_SBAS, OPT_FORCEFT, OPT_FORCETH, OPT_FORCE, OPT_LOOP, OPT_UNLOCK */
};

struct stemmer_gmodules {
	void *p;
	int n;
	char *name[8];
};

// show the usage of this program

void ShowUsage(char *app, int oldsbas_flag)
{
	_tprintf((char*) (_T("%s version %s\n")), APPNAME, QUOTE(PHDOTVERSION));
	_tprintf((char*) (_T("Usage : %s <options> \n")), APPNAME);
	_tprintf((char*) (_T("[-?     | --help]                   : this help \n")));
	_tprintf((char*) (_T("[-v     | --version                 : display version and quit \n")));
	_tprintf((char*) (_T("[-h     | --host]=<addr>            : host addr. of applicationBox (default '127.0.0.1') \n")));
	_tprintf((char*) (_T("[-P     | --port]=<port>            : port of applicationBox (default '%d') \n")), MYSQL_PORT);
	_tprintf((char*) (_T("[-b     | --base]=<base>            : database of applicationBox (default 'phrasea') \n")));
	_tprintf((char*) (_T("[-u     | --user]=<user>            : user account for connection to applicationBox (default 'root') \n")));
	_tprintf((char*) (_T("[-p     | --password]=<pwd>         : password for connection to applicationBox (default '') \n")));
	_tprintf((char*) (_T("[-s     | --socket]=<port>          : port for telnet control (default none) \n")));
	_tprintf((char*) (_T("[-f     | --flush]=<n>              : flush every n records (default 50) \n")));
	_tprintf((char*) (_T("[-o     | --old]                    : use old 'sbas' table instead of 'xbas' \n")));
	_tprintf((char*) (_T("[         --quit]                   : index once and quit \n")));
	_tprintf((char*) (_T("[-c     | --clng]=<lng>             : default language for new candidates terms (default 'fr') \n")));
	_tprintf((char*) (_T("[       | --stem]=<lng>,<lng>,..    : stemm for those languages \n")));
	_tprintf((char*) (_T("[-n     | --nolog]                  : do not log, but out to console \n")));
	_tprintf((char*) (_T("[-d     | --debug]=<mask>           : debug mask (to console) \n")));
	_tprintf((char*) (_T("                           1        : xml parsing \n")));
	_tprintf((char*) (_T("                           2        : sql errors \n")));
	_tprintf((char*) (_T("                           4        : sql ok \n")));
	_tprintf((char*) (_T("                           8        : memory alloc. \n")));
	_tprintf((char*) (_T("                          16        : record ops. \n")));
	_tprintf((char*) (_T("                          32        : structure ops. \n")));
	_tprintf((char*) (_T("                          64        : flush ops. \n")));
	_tprintf((char*) (_T("[-@     | --optfile]=<file>         : read (more) arguments from text file (see 'sample_args.txt') \n")));
	_tprintf((char*) (_T("[--default-character-set]=<charset> : charset of applicationBox AND dataBoxes (default none) \n")));
#ifdef INSTALL_AS_NT_SERVICE
	_tprintf((char*) (_T("Windows specific options :\n")));
	_tprintf((char*) (_T("[--install]                         : install as service \n")));
	_tprintf((char*) (_T("[--remove]                          : remove installed service \n")));
	_tprintf((char*) (_T("[--run]                             : run into console \n")));
#endif
	//	printf("[-s | --sbas-id]=<bid>   : sbas-id of dataBox to work in (mandatory) \n");
	//	printf("[     --force-fulltext]  : force fulltext reindex (status-bit[0] = 0, only at first loop) \n");
	//	printf("[     --force-thesaurus] : force thesaurus reindex (status-bit[1] = 0, only at first loop) \n");
	//	printf("[-f | --force]           : force fulltext and thesaurus reindex (status-bits[1,0] = 0, only at first loop) \n");
	//	printf("[-u | --unlock]          : force unlock of locked 'reindexing' records (status-bit[2] = 1, only at first loop) \n");
	//	printf("[-l | --loop]=<n>		 : look for work every <n> seconds (default '0' : single run, no loop) \n\n");

#ifdef INSTALL_AS_NT_SERVICE
	_tprintf((char*) (_T("example:\n %s -h 192.168.0.1 --base dbTest --clng en --nolog --run\n\n")), app);
#else
	_tprintf((char*) (_T("example:\n %s -h=192.168.0.1 --base=dbTest --clng=en --nolog\n\n")), app);
#endif

	CConnbas_abox abox(arg_host, arg_user, arg_pswd, arg_base, arg_port);

	if(abox.isok)
	{
		char buff[4200]; // room for a max of 20 sbas
		abox.listSbas(buff, 4200);

		printf("/----- %s from %s:%d:%s ----\n"
			"|Indexable\n"
			"%s"
			"\\-----------------------------\n", (oldsbas_flag ? "sbas" : "xbas"), arg_host, arg_port, arg_base, buff);
		abox.close();
	}
	else
	{
		printf("/----- %s from %s:%d:%s ----\n"
			"Can't connect to applicationBox\n"
			"\\-----------------------------\n", (oldsbas_flag ? "sbas" : "xbas"), arg_host, arg_port, arg_base);
	}

	CSTEMMER_GRP stemmergrp;
	int i;
	for(i=0; modules[i].name; i++)
	{
		if(modules[i].enc == ENC_UTF_8) //  && strlen(modules[i].name) <= 4)
		{
			stemmergrp.add_module((void *)(modules[i].stem), modules[i].name);
		}
	}
	stemmergrp.dump();
}

void signal_sigint(int sig)
{
	zSyslog._log(CSyslog::LOGL_INFO, CSyslog::LOGC_SIGNAL, "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n"
				"'Ctrl-C' received by signal handler\n"
				" --> reseting running flag to 0!\n"
				"!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
	running = false;
	for(CSbas *p = sbasPool.first; p; p = p->next)
		p->status = SBAS_STATUS_TOSTOP;
}


#ifdef INSTALL_AS_NT_SERVICE

void WINAPI ServiceMain(DWORD argc, LPTSTR *argv);
SERVICE_STATUS_HANDLE m_ServiceStatusHandle;

//---------------------------------------------------------------------------
// control status of the service

void WINAPI ServiceCtrlHandler(DWORD Opcode)
{
	SERVICE_STATUS m_ServiceStatus;
	m_ServiceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
	// m_ServiceStatus.dwCurrentState       = SERVICE_STOPPED;
	m_ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP; // | SERVICE_ACCEPT_PAUSE_CONTINUE;
	m_ServiceStatus.dwWin32ExitCode = 0;
	m_ServiceStatus.dwServiceSpecificExitCode = 0;
	m_ServiceStatus.dwCheckPoint = 0;
	m_ServiceStatus.dwWaitHint = 0;
	switch(Opcode)
	{
			/*
			// if the service receive the pause command
			case SERVICE_CONTROL_PAUSE:
				m_ServiceStatus.dwCurrentState = SERVICE_PAUSED;
				SetServiceStatus(m_ServiceStatusHandle, &m_ServiceStatus);
				break;
			case SERVICE_CONTROL_CONTINUE:
				m_ServiceStatus.dwCurrentState = SERVICE_RUNNING;
				SetServiceStatus(m_ServiceStatusHandle, &m_ServiceStatus);
				break;
			 */
			// if the service receive the stop command
		case SERVICE_CONTROL_STOP:
			m_ServiceStatus.dwWin32ExitCode = 0;
			m_ServiceStatus.dwCurrentState = SERVICE_STOP_PENDING;
			m_ServiceStatus.dwCheckPoint = 0;
			m_ServiceStatus.dwWaitHint = 5000;
			SetServiceStatus(m_ServiceStatusHandle, &m_ServiceStatus);
			running = false;
			break;
		case SERVICE_CONTROL_INTERROGATE:
			break;
	}
	return;
}
#endif


CSimpleOpt::SOption g_rgOptions[] = {
	//    { OPT_HOST,  (char *)(_T("-h")),					SO_OPT }, // "-h ARG"
	{ OPT_HOST, (char *) (_T("-h")), SO_REQ_SEP}, // "-h ARG"
	{ OPT_HOST, (char *) (_T("--host")), SO_REQ_SEP}, // "--host ARG"

	{ OPT_PORT, (char *) (_T("-P")), SO_REQ_SEP},
	{ OPT_PORT, (char *) (_T("--port")), SO_REQ_SEP},

	{ OPT_BASE, (char *) (_T("-b")), SO_REQ_SEP},
	{ OPT_BASE, (char *) (_T("--base")), SO_REQ_SEP},

	{ OPT_USER, (char *) (_T("-u")), SO_REQ_SEP},
	{ OPT_USER, (char *) (_T("--user")), SO_REQ_SEP},

	{ OPT_PSWD, (char *) (_T("-p")), SO_REQ_SEP},
	{ OPT_PSWD, (char *) (_T("--password")), SO_REQ_SEP},

	{ OPT_FLUSH, (char *) (_T("-f")), SO_REQ_SEP},
	{ OPT_FLUSH, (char *) (_T("--flush")), SO_REQ_SEP},

	{ OPT_OLDSBAS, (char *) (_T("-o")), SO_NONE},
	{ OPT_OLDSBAS, (char *) (_T("--old")), SO_NONE},

	{ OPT_QUIT, (char *) (_T("--quit")), SO_NONE},

	{ OPT_CLNG, (char *) (_T("-c")), SO_REQ_SEP},
	{ OPT_CLNG, (char *) (_T("--clng")), SO_REQ_SEP},

	{ OPT_STEM, (char *) (_T("--stem")), SO_REQ_SEP},

	{ OPT_NOLOG, (char *) (_T("-n")), SO_NONE},
	{ OPT_NOLOG, (char *) (_T("--nolog")), SO_NONE},

	{ OPT_DEBUG, (char *) (_T("-d")), SO_OPT},
	{ OPT_DEBUG, (char *) (_T("--debug")), SO_OPT},

	{ OPT_SOCKET, (char *) (_T("-s")), SO_REQ_SEP},
	{ OPT_SOCKET, (char *) (_T("--socket")), SO_REQ_SEP},

	{ OPT_MYCHARSET, (char *) (_T("--default-character-set")), SO_REQ_SEP},

#ifdef INSTALL_AS_NT_SERVICE
	{ OPT_INSTALL, (char *) (_T("--install")), SO_NONE},
	{ OPT_REMOVE, (char *) (_T("--remove")), SO_NONE},
	{ OPT_RUN, (char *) (_T("--run")), SO_NONE},
#endif

	{ OPT_HELP, (char *) (_T("-?")), SO_NONE},
	{ OPT_HELP, (char *) (_T("--help")), SO_NONE},

	{ OPT_VERSION, (char *) (_T("-v")), SO_NONE},
	{ OPT_VERSION, (char *) (_T("--version")), SO_NONE},

	{ OPT_OPTFILE, (char *) (_T("--optfile")), SO_REQ_SEP},
	{ OPT_OPTFILE, (char *) (_T("-@")), SO_REQ_SEP},

	SO_END_OF_OPTIONS // END
};

bool parseOptions(int argc, TCHAR * argv[], bool infile = false)
{
	char *p;
	FILE *fp;
	int file_buff_n = 0;
	int file_argc;
	char * file_argv[50];
	//	ShowUsage();
	//	return(0);

	// declare a table of CSimpleOpt::SOption structures. See the SimpleOpt.h header
	// for details of each entry in this structure. In summary they are:
	//  1. ID for this option. This will be returned from OptionId() during processing.
	//     It may be anything >= 0 and may contain duplicates.
	//  2. Option as it should be written on the command line
	//  3. Type of the option. See the header file for details of all possible types.
	//     The SO_REQ_SEP type means an argument is required and must be supplied
	//     separately, e.g. "-f FILE"
	//  4. The last entry must be SO_END_OF_OPTIONS.
	//
	// declare our options parser, pass in the arguments from main
	// as well as our array of valid options.
	CSimpleOpt args(argc, argv, g_rgOptions);
	// while there are arguments left to process
	while(args.Next())
	{
		if(args.LastError() == SO_SUCCESS)
		{
			switch(args.OptionId())
			{
				case OPT_HELP:
					help_flag = true;
					break;
				case OPT_VERSION:
					version_flag = true;
					break;
				case OPT_HOST:
					arg_host = (p = args.OptionArg()) ? p : (char *) ("127.0.0.1");
					break;
				case OPT_PORT:
					arg_port = (p = args.OptionArg()) ? atoi(p) : MYSQL_PORT;
					break;
				case OPT_BASE:
					arg_base = (p = args.OptionArg()) ? p : (char *) ("phrasea");
					break;
				case OPT_USER:
					arg_user = (p = args.OptionArg()) ? p : (char *) ("root");
					break;
				case OPT_PSWD:
					arg_pswd = (p = args.OptionArg()) ? p : (char *) ("");
					break;
				case OPT_CLNG:
					arg_clng = (p = args.OptionArg()) ? p : (char *) ("fr");
					break;
				case OPT_STEM:
					if((p = args.OptionArg()) != NULL)
					{
						char *p0 = NULL;
						char *pl = NULL;
						while(p)
						{
							if(!*p)
								p = NULL;
							if(p && *p!=' ' && *p!=',')
							{
								pl = p;
								if(!p0)
									p0 = p;
							}
							if((!p || *p==',') && p0 && pl)
							{
								if(pl>p0+3)
									pl=p0+3;
								*++pl = '\0';
								if(narg_stem < 50 && p0!=p)
									arg_stem[narg_stem++] = p0;
								pl = p0 = NULL;
							}
							if(p)
								p++;
						}
					}
					break;
				case OPT_SOCKET:
					arg_socket = (p = args.OptionArg()) ? atoi(p) : 0;
					break;
				case OPT_FLUSH:
					arg_flush = (p = args.OptionArg()) ? atoi(p) : 50;
					if(arg_flush < 1)
						arg_flush = 1;
					else if(arg_flush > 1000)
						arg_flush = 1000;
					break;
				case OPT_MYCHARSET:
					arg_mycharset = (p = args.OptionArg()) ? p : NULL;
					break;
				case OPT_OLDSBAS:
					oldsbas_flag = true;
					break;
				case OPT_QUIT:
					quit_flag = true;
					break;
				case OPT_NOLOG:
					nolog_flag = true;
					break;
				case OPT_DEBUG:
					debug_flag = (p = args.OptionArg()) ? atoi(p) : 0;
					break;

				case OPT_OPTFILE:
					printf("OPTIONFILE : '%s'\n", (p = args.OptionArg()) ? p : "NULL");
					if(!infile)
					{
						if((p = args.OptionArg()))
						{
							file_argc = 0;
							int l;
							if((fp = fopen(p, "r")))
							{
								while((file_buff_n < 1990) && fgets(file_buff + file_buff_n, 1000 - file_buff_n, fp))
								{
									//printf("%d : ' %s' \n", __LINE__, file_buff+file_buff_n);
									for(l = strlen(file_buff + file_buff_n); l > 0; l--)
									{
										if(isspace(file_buff[file_buff_n + l - 1]))
											file_buff[file_buff_n + l - 1] = '\0';
										else
											break;
									}
									if(l > 0 && file_buff[file_buff_n] != '#')
									{
										file_argv[file_argc++] = file_buff + file_buff_n;
										file_buff_n += l + 1;
									}
								}
								fclose(fp);
							}
							//for(int i=0; i<file_argc; i++)
							//	printf("%d :  %d:'%s' \n", __LINE__, i, file_argv[i]);
							if(!parseOptions(file_argc, file_argv, true))
								return (false);
						}
					}
					else
					{
						_tprintf(_T("Invalid argument: %s into arguments-file\n"), args.OptionText());
						return false;
					}
					break;

#ifdef INSTALL_AS_NT_SERVICE
				case OPT_INSTALL:
					install_flag = true;
					break;
				case OPT_REMOVE:
					remove_flag = true;
					break;
				case OPT_RUN:
					run_flag = true;
					break;
#endif
			}
		}
		else
		{
			_tprintf(_T("Invalid argument: %s\n"), args.OptionText());
			return false;
		}
	}
	return true;
}


// ------------------------------------------------------------------------
// main job every 10 seconds : scan sbas and lauch threads
// ------------------------------------------------------------------------
#define CNX_STATUS_UNKNOWN 0
#define CNX_STATUS_OK 1
#define CNX_STATUS_BAD 2

extern const char *statlibs[5];

void runThreads(CSbasList *sbasPool, bool oldsbas_flag)
{
	if(!running)
		return;

	static int cnxStatux = CNX_STATUS_UNKNOWN; // status of the last cnx

	// cnx to appbox
	CConnbas_abox abox(arg_host, arg_user, arg_pswd, arg_base, arg_port);

	if(abox.isok)
	{
		CSbasList sbasList;
		CSbas *p;

		if(cnxStatux == CNX_STATUS_UNKNOWN)
			zSyslog._log(CSyslog::LOGL_INFO, CSyslog::LOGC_ACNX_OK, "Connected to appBox %s:%d:%s (user %s)", arg_host, arg_port, arg_base, arg_user);
		else if(cnxStatux == CNX_STATUS_BAD)
			zSyslog._log(CSyslog::LOGL_INFO, CSyslog::LOGC_ACNX_OK, "Re-Connected to appBox %s:%d:%s (user %s)", arg_host, arg_port, arg_base, arg_user);
		cnxStatux = CNX_STATUS_OK;

		//		printf("Connected to appBox %s:%d:%s (user %s)\n", arg_host, arg_port, arg_base, arg_user);
		abox.listSbas2(&sbasList, oldsbas_flag); // list sbas in the temporary list

		// compare list to the pool to know what to add/del

		bool changed = false;

		// flag all pool as "unknown" (unknown sbas-id)
//		sbasPool->dump("++++++++++ 501");
		//		for(p = sbasPool->first; p; p = p->next)
		//		{
		//			if(p->status != SBAS_STATUS_TODELETE)
		//				p->status = SBAS_STATUS_UNKNOWN;
		//		}
		//sbasPool->dump("pool 1");

		// delete unknown sbas_id from the pool
		for(p = sbasPool->first; p; p = p->next)
		{
			bool found = false;
			for(CSbas *l = sbasList.first; l && !found; l = l->next)
			{
				if(*l == *p)
					found = true; // pool entry found in the list
			}
			if(!found)
			{
				if(p->status == SBAS_STATUS_FINISHED)
				{
					// DONT'T delete from pool because the main thread must check 'indexed' (but no problem to leave it)
//					printf("++++ %d ++++ could delete pool entry for sbas_id=%d \n", __LINE__, p->sbas_id);
				}
				else
				{
					// ask this thred to end since sbas has disapeared
					p->status = SBAS_STATUS_TOSTOP;
					changed = true;
				}
			}
		}

//		sbasPool->dump("++++++++++ 534");

		// then merge the list with the pool
		for(CSbas *l = sbasList.first; running && l; l = l->next)
		{
			for(p = sbasPool->first; p; p = p->next)
			{
				if(*p == *l)
					break; // found in the pool
			}
			if(!p)
			{
				// the sbas of the list has not been found in the pool, add it
				p = sbasPool->add(l->sbas_id, l->host, l->port, l->dbname, l->user, l->pwd);
				p->status = SBAS_STATUS_NEW;
//				printf("++++ %d ++++ sbas %d new ! : p->status = SBAS_STATUS_NEW \n", __LINE__, p->sbas_id);

				changed = true;
			}
			else
			{
				p->status = SBAS_STATUS_OLD;
//				printf("++++ %d ++++ sbas %d found ! : p->status = SBAS_STATUS_OLD \n", __LINE__, p->sbas_id);
			}

			if(p->idxthread == (ATHREAD) NULLTHREAD)
			{
//				printf("++++ %d ++++ sbas %d START ! \n", __LINE__, p->sbas_id);
				// there is no thread, create it
				if(THREAD_START(p->idxthread, thread_index, p))
				{
					// the thread will start soon
				}
				else
				{
					// error creating thread
				}
			}
		}

		if(changed)
		{
			abox.close();
		}
	}
	else
	{
		if(cnxStatux == CNX_STATUS_UNKNOWN)
			zSyslog._log(CSyslog::LOGL_ERR, CSyslog::LOGC_ACNX_OK, "Can't connect to appBox %s:%d:%s (user %s)", arg_host, arg_port, arg_base, arg_user);
		else if(cnxStatux == CNX_STATUS_OK)
			zSyslog._log(CSyslog::LOGL_ERR, CSyslog::LOGC_ACNX_OK, "Lost connection to appBox %s:%d:%s (user %s)", arg_host, arg_port, arg_base, arg_user);
		cnxStatux = CNX_STATUS_BAD;
	}
}

int main(int argc, TCHAR * argv[])
{
	// printf("%s[%d] ------------MAIN-----------\n", __FILE__, __LINE__);
	// fflush(stdout);
	if(!parseOptions(argc, argv))
		exit(-1);

	if(help_flag)
	{
		ShowUsage(argv[0], oldsbas_flag);
		exit(0);
	}
	if(version_flag)
	{
		_tprintf(_T("%s version %s\n"), APPNAME, QUOTE(PHDOTVERSION));
		exit(0);
	}

#ifdef INSTALL_AS_NT_SERVICE
	if(install_flag && !remove_flag && !run_flag)
	{
		// let's install as a nt service
		char strDir[1024];
		char strCmd[2048];

		GetCurrentDirectory(1024, strDir); // le path de l'exe
		if(debug_flag)
		{
			sprintf(strCmd, "%s\\%s --host=\"%s\" --port=%d --base=\"%s\" --user=\"%s\" --password=\"%s\" --clng=\"%s\" -d=\"%d\""
					, strDir, argv[0], arg_host, arg_port, arg_base, arg_user, arg_pswd, arg_clng, debug_flag);
		}
		else
		{
			sprintf(strCmd, "%s\\%s --host=\"%s\" --port=%d --base=\"%s\" --user=\"%s\" --password=\"%s\" --clng=\"%s\""
					, strDir, argv[0], arg_host, arg_port, arg_base, arg_user, arg_pswd, arg_clng);
		}
		if(oldsbas_flag)
			strcat(strCmd, " --old");
		if(nolog_flag)
			strcat(strCmd, " --nolog");

		printf("CMD : '%s'\n", strCmd);
		Install(strCmd, NTSERVICENAME, NTSERVICEDESC);
		exit(0);
	}
	if(remove_flag && !install_flag && !run_flag)
	{
		// let's remove the nt service
		UnInstall(NTSERVICENAME);
		exit(0);
	}
	if(run_flag && !install_flag && !remove_flag)
	{
		// let's run the pgm as command-line
		TCHAR * t[] = {argv[0]};
		ServiceMain(1, t);
		exit(0);
	}
	if(run_flag || install_flag || remove_flag)
	{
		printf("options '--install', '--remove', '--run' are mutualy exclusive.\n");
		ShowUsage(argv[0], oldsbas_flag);
		exit(0);
	}

	// declare service and starting in ServiceMain()
	SERVICE_TABLE_ENTRY DispatchTable[] = {
		{ NTSERVICENAME, ServiceMain},
		{ NULL, NULL}
	};
	if(!StartServiceCtrlDispatcher(DispatchTable))
	{
		if(GetLastError() == ERROR_FAILED_SERVICE_CONTROLLER_CONNECT)
		{
			// launched in a terminal ?
			printf("GetLastError() = %d\n", GetLastError());

			printf("to run in a console, use '--run' option.\n");
			exit(0);
		}
	}
}

void WINAPI ServiceMain(DWORD argc, LPTSTR *argv)
{
	DWORD stop_pending_checkpoint = 1;
	SERVICE_STATUS m_ServiceStatus;

	if(!run_flag)
	{
		m_ServiceStatusHandle = RegisterServiceCtrlHandler("NTSERVICENAME", ServiceCtrlHandler);
		if(m_ServiceStatusHandle == (SERVICE_STATUS_HANDLE) 0)
		{
			return;
		}
		m_ServiceStatus.dwServiceType = SERVICE_WIN32;
		m_ServiceStatus.dwCurrentState = SERVICE_START_PENDING;
		m_ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP; // | SERVICE_ACCEPT_PAUSE_CONTINUE;
		m_ServiceStatus.dwWin32ExitCode = 0;
		m_ServiceStatus.dwServiceSpecificExitCode = 0;
		m_ServiceStatus.dwCheckPoint = 0;
		m_ServiceStatus.dwWaitHint = 0;
		m_ServiceStatus.dwCurrentState = SERVICE_RUNNING;
		m_ServiceStatus.dwCheckPoint = 0;
		m_ServiceStatus.dwWaitHint = 0;
		if(!SetServiceStatus(m_ServiceStatusHandle, &m_ServiceStatus))
		{
		}
	}
#endif
	zSyslog.open("phraseanet_cindexer", nolog_flag ? CSyslog::TOTTY : CSyslog::TOLOG);

	zSyslog._log(CSyslog::LOGL_INFO, CSyslog::LOGC_PROG_START, "Program starting");

	if(!mysql_library_init(sizeof (server_args) / sizeof (char *), server_args, server_groups) == 0)
	{
		zSyslog._log(CSyslog::LOGL_ERR, CSyslog::LOGC_SQLERR, "could not initialize MySQL library");
		exit(-1);
	}

	for(int i=0; i<narg_stem; i++)
	{
		struct sb_stemmer *stemmer;
		if( (stemmer = sb_stemmer_new(arg_stem[i], "UTF_8")) )
			sb_stemmer_delete(stemmer);
		else
		{
			char buff[50];
			sprintf(buff, "unknown stemmer '%s'", arg_stem[i]);
			zSyslog._log(CSyslog::LOGL_ERR, CSyslog::LOGC_PROG_START, buff);
			exit(-1);
		}
	}

	int x_errno = 0;

	// on intercepte le ctrl-C
	signal(SIGINT, signal_sigint); // ctrl-c
	signal(SIGTERM, signal_sigint); // kill
#ifdef SIGBREAK
	signal(SIGBREAK, signal_sigint); // win32
#endif
#ifndef WIN32
	// on ignore les fautes de pipe (mysql dead)
	signal(SIGPIPE, SIG_IGN);
#endif

	xmlInitParser();

	//#ifdef WIN32
	SOCKET ListenSocket = -1;
	SOCKADDR_IN InternetAddr;

	CSocketList clientSockets;

	// printf("%s[%d] ------------START-----------\n", __FILE__, __LINE__);
	// fflush(stdout);
	if(arg_socket != 0)
	{
#ifdef WIN32
		WSADATA WSAData;
		WSAStartup(MAKEWORD(1, 0), &WSAData);
#endif
		if((ListenSocket = socket(PF_INET, SOCK_STREAM, 0)) == -1)
		{
			zSyslog._log(CSyslog::LOGL_ERR, CSyslog::LOGC_PROG_START, "sock : socket() failed %d", x_errno);

			ListenSocket = -1;
		}
		else
		{
			// printf("%s[%d] \n", __FILE__, __LINE__);
			// fflush(stdout);
			InternetAddr.sin_family = AF_INET;
			InternetAddr.sin_addr.s_addr = htonl(INADDR_ANY);

			InternetAddr.sin_port = htons(arg_socket);

			// let's try to bind, limit 90 sec.
			int ntry = 30;
			while(ntry > 0)
			{
				// printf("%s[%d] \n", __FILE__, __LINE__);
				// fflush(stdout);
				if(bind(ListenSocket, (SOCKADDR *) & InternetAddr, sizeof (InternetAddr)) != SOCKET_ERROR)
					break;
				SLEEP(3);
				ntry--;
			}

			if(ntry <= 0)
			{
				// printf("%s[%d] \n", __FILE__, __LINE__);
				// fflush(stdout);
#ifdef WIN32
				x_errno = WSAGetLastError();
#else
				x_errno = errno;
#endif
				zSyslog._log(CSyslog::LOGL_ERR, CSyslog::LOGC_PROG_START, "sock : bind() failed 90 sec. (err=%d)", x_errno);

				ListenSocket = -1;
			}


			if(ListenSocket != -1)
			{
				// printf("%s[%d] \n", __FILE__, __LINE__);
				// fflush(stdout);
				if(listen(ListenSocket, 5) == SOCKET_ERROR)
				{
					// printf("%s[%d] \n", __FILE__, __LINE__);
					// fflush(stdout);
#ifdef WIN32
					x_errno = WSAGetLastError();
#else
					x_errno = errno;
#endif
					//						_tprintf(_T("sock : listen failed %d\n"), x_errno);
					zSyslog._log(CSyslog::LOGL_ERR, CSyslog::LOGC_PROG_START, "sock : listen() failed %d", x_errno);

					ListenSocket = -1;
				}
				else
				{
					// Change the socket mode on the listening socket from blocking to non-block
#ifdef WIN32
					ULONG NonBlock = 1;
					if(ioctlsocket(ListenSocket, FIONBIO, &NonBlock) == SOCKET_ERROR)
					{
						x_errno = WSAGetLastError();
						//						_tprintf(_T("sock : ioctlsocket failed %d\n"), x_errno);
						zSyslog._log(CSyslog::LOGL_ERR, CSyslog::LOGC_PROG_START, "sock : ioctlsocket() failed %d", _errno);

						ListenSocket = -1;
					}
#else
					// printf("%s[%d] \n", __FILE__, __LINE__);
					// fflush(stdout);
					if(fcntl(ListenSocket, F_SETFL, O_NONBLOCK) == -1)
					{
						// printf("%s[%d] \n", __FILE__, __LINE__);
						// fflush(stdout);
						x_errno = errno;
						//							_tprintf(_T("sock : fcntl failed %d\n"), x_errno);
						zSyslog._log(CSyslog::LOGL_ERR, CSyslog::LOGC_PROG_START, "sock : fcntl() failed %d", x_errno);

						ListenSocket = -1;
					}
#endif
				}
			}
		}


		// printf("%s[%d] \n", __FILE__, __LINE__);
		// fflush(stdout);
		if(ListenSocket != -1)
		{
			// printf("%s[%d] \n", __FILE__, __LINE__);
			// fflush(stdout);
			zSyslog._log(CSyslog::LOGL_INFO, CSyslog::LOGC_PROG_START, "Control with socket %d", arg_socket);
		}
		// printf("%s[%d] \n", __FILE__, __LINE__);
		// fflush(stdout);
	}
	// printf("%s[%d] \n", __FILE__, __LINE__);
	// fflush(stdout);

	running = true;
	if(arg_socket != 0 && ListenSocket == -1)
	{
		// we failed top open a control socket, we can't run
		running = false;
	}

	//		if(running)
	//		{
	//			umask(0);
	//			if(setsid() >= 0)
	//			{
	//				fclose(stdin);
	//				fclose(stdout);
	//				fclose(stderr);
	//			}
	//		}

#ifndef WIN32
	pid_t ppid = getppid();
#endif
	while(running)
	{
//			printf("++++ %d +++++ running = %d\n", __LINE__, running);
		runThreads(&sbasPool, oldsbas_flag); // scanne xbas (ou sbas) et lance les threads
//			printf("++++ %d +++++ running = %d\n", __LINE__, running);

#ifndef WIN32
		if(ppid != getppid())
			running = false;
#endif
		if(!running)
			break;

		// on attend 10* 1 seconde
		for(wait_loop = 10; running && wait_loop>0; wait_loop--)
		{
			if(quit_flag)
			{
				bool can_quit = true;
				CSbas *p;
				for(p = sbasPool.first; p; p = p->next)
				{
					if(!p->indexed)
						can_quit = false;
				}
				if(can_quit)
				{
					running = false;
					break;
				}
			}


			if(ListenSocket != -1)
			{
				int highsock = 0; // will be ignored by winsock
				fd_set Reader;
				FD_ZERO(&Reader); // liste de sockets � tester en lecture
#ifdef WIN32
				FD_SET(ListenSocket, &Reader); // socket principal
				for(CSocket *s = clientSockets.firstSocket; s; s = s->nextSocket)
					FD_SET(s->socket, &Reader); // sockets clients
#else
				// *nix, calculer le highest socket
				if(ListenSocket > highsock)
					highsock = ListenSocket;
				FD_SET(ListenSocket, &Reader); // socket principal
				for(CSocket *s = clientSockets.firstSocket; s; s = s->nextSocket)
				{
					if(s->socket > highsock)
						highsock = s->socket;
					FD_SET(s->socket, &Reader); // sockets clients
				}
#endif

				DWORD nSocketsChanged;
				TIMEVAL timout = {1, 0}; // 1 sec
				//printf("select\n");
				nSocketsChanged = select(highsock + 1, &Reader, NULL, NULL, &timout);
				//printf("nSocketsChanged == %d\n", nSocketsChanged);
				if(nSocketsChanged == SOCKET_ERROR)
				{
					//printf("nSocketsChanged == SOCKET_ERROR\n");
					SLEEP(1);
					continue;
				}
				if(nSocketsChanged > 0)
				{
					// printf("nSocketsChanged == %d (>0)\n", nSocketsChanged);
					// fflush(stdout);
					if(FD_ISSET(ListenSocket, &Reader))
					{
						nSocketsChanged--;
						SOCKET AcceptSocket;
						// printf("accept FD_ISSET(ListenSocket=%d) \n", ListenSocket);
						// fflush(stdout);
						if((AcceptSocket = accept(ListenSocket, NULL, NULL)) != INVALID_SOCKET)
						{
							//printf("accepted\n", nSocketsChanged);
							// Set the accepted socket to non-blocking mode so the server will
							// not get caught in a blocked condition on WSASends
#ifdef WIN32
							ULONG NonBlock = 1;
							if(ioctlsocket(AcceptSocket, FIONBIO, &NonBlock) != SOCKET_ERROR)
							{
								clientSockets.add(AcceptSocket);
								send(AcceptSocket, "hello, type 'Q <enter>' to quit cindexer\n  ", 41, 0);
								//if (CreateSocketInformation(AcceptSocket) == FALSE)
								//	return;
							}
							else
							{
								// printf("sock error: ioctlsocket() failed with error %d\n", WSAGetLastError());
							}
#else
							//printf("fcntl\n");
							if(fcntl(AcceptSocket, F_SETFL, O_NONBLOCK) != -1)
							{
								//printf("fcntl != -1\n");
								clientSockets.add(AcceptSocket);
								send(AcceptSocket, "hello, type 'Q <enter>' to quit cindexer\n  ", 41, 0);

								//if (CreateSocketInformation(AcceptSocket) == FALSE)
								//	return;
							}
							else
							{
								// printf("sock error: fcntl() failed\n");
							}
#endif
						}
						else
						{
#ifdef WIN32
							if(WSAGetLastError() != WSAEWOULDBLOCK)
							{
								// printf("accept() failed with error %d\n", WSAGetLastError());
							}
#else
							//printf("errno = %d\n", errno);
							if(errno != EWOULDBLOCK)
							{
								//printf("errno != EWOULDBLOCK\n");
								// printf("accept() failed with error %d\n", errno);
							}
#endif
						}
					}
					// Check each socket for Read and Write notification for Total number of sockets

					for(CSocket *s = clientSockets.firstSocket; nSocketsChanged > 0 && s;)
					{
						// If the Reader is marked for this socket then this means data
						// is available to be read on the socket.
						if(FD_ISSET(s->socket, &Reader))
						{
							// printf("accept FD_ISSET(socket=%d) \n", s->socket);
							// fflush(stdout);
							nSocketsChanged--;

							int RecvBytes;
							char buff[201];
							if((RecvBytes = recv(s->socket, buff, 200, 0)) == SOCKET_ERROR)
							{
#ifdef WIN32
								int err;
								if((err = WSAGetLastError()) != WSAEWOULDBLOCK)
								{
									// printf("sock : Receive failed with error %d\n", err);
									s = clientSockets.remove(s);
									continue;
								}
#else
								if(errno != EWOULDBLOCK)
								{
									// printf("sock : Receive failed with error %d\n", err);
									s = clientSockets.remove(s);
									continue;
								}
#endif
							}
							else
							{
								buff[RecvBytes] = '\0';
								// printf("received '%s' (%d bytes)\n", buff, RecvBytes);

								// If zero bytes are received, this indicates connection is closed.
								if(RecvBytes == 0)
								{
									s = clientSockets.remove(s);
									continue;
								}
								else
								{
									if(buff[0] == 'Q')
									{
										send(s->socket, "'Q' received by cindexer...\r\n  ", 28, 0);

										zSyslog._log(CSyslog::LOGL_INFO, CSyslog::LOGC_PROG_END, "'Q' received");

										for(CSbas *p = sbasPool.first; p; p = p->next)
											p->status = SBAS_STATUS_TOSTOP;
										running = false;
									}
								}
							}
						}
						s = s->nextSocket;
					}
				}
				else
				{
					// printf("sock error: select function returned with error %d\n", WSAGetLastError());
				}
			}
			else
			{
				// ici ListenSocket == -1
				SLEEP(1);
			}
		}
	}
	if(ListenSocket != -1)
	{
		char buff[201];
		int RecvBytes;
		int r;
		// printf("%s[%d] \n", __FILE__, __LINE__);
		// fflush(stdout);
		for(CSocket *s = clientSockets.firstSocket; s; s = s->nextSocket)
		{
			// printf("%s[%d] \n", __FILE__, __LINE__);
			// fflush(stdout);
			while(1)
			{
				RecvBytes = recv(s->socket, buff, 200, 0);
				if(RecvBytes == 0 || RecvBytes == SOCKET_ERROR)
					break;
				// printf("%s[%d] \n", __FILE__, __LINE__);
				// fflush(stdout);
			}
			// printf("%s[%d] \n", __FILE__, __LINE__);
			// fflush(stdout);
			// printf("closing socket %d \n", s->socket);
			r = shutdown(s->socket, SD_BOTH);
			// printf("%s[%d] r=%d \n", __FILE__, __LINE__, r);
			// fflush(stdout);
#ifdef WIN32
			closesocket(s->socket);
#else
			r = close(s->socket);
#endif
			// printf("%s[%d] r=%d \n", __FILE__, __LINE__, r);
			// fflush(stdout);
		}
		// printf("%s[%d] \n", __FILE__, __LINE__);
		// fflush(stdout);

		// printf("closing ListenSocket %d \n", ListenSocket);
		r = shutdown(ListenSocket, SD_BOTH);
		// printf("%s[%d] r=%d \n", __FILE__, __LINE__, r);
		// fflush(stdout);

#ifdef WIN32
		closesocket(ListenSocket);
#else
		r = close(ListenSocket);
#endif
		// printf("%s[%d] r=%d \n", __FILE__, __LINE__, r);
		// fflush(stdout);

#ifdef WIN32
			WSACleanup();
#endif
	}

	zSyslog._log(CSyslog::LOGL_INFO, CSyslog::LOGC_THREAD_END, "Waiting threads to end...");

	while(1)
	{
#ifdef INSTALL_AS_NT_SERVICE
		if(!run_flag)
		{
			m_ServiceStatus.dwWin32ExitCode = 0;
			m_ServiceStatus.dwCurrentState = SERVICE_STOP_PENDING;
			m_ServiceStatus.dwCheckPoint = stop_pending_checkpoint++;
			m_ServiceStatus.dwWaitHint = 5000;
			SetServiceStatus(m_ServiceStatusHandle, &m_ServiceStatus);
		}
#endif
		CSbas *p;
		for(p = sbasPool.first; p; p = p->next)
		{
			if(p->idxthread != (ATHREAD) NULLTHREAD)
				break; // un thread tourne encore
		}
		if(!p)
			break;
		SLEEP(1);
	}

#ifdef INSTALL_AS_NT_SERVICE
	if(!run_flag)
	{
		m_ServiceStatus.dwWin32ExitCode = 0;
		m_ServiceStatus.dwCurrentState = SERVICE_STOP_PENDING;
		m_ServiceStatus.dwCheckPoint = stop_pending_checkpoint++;
		m_ServiceStatus.dwWaitHint = 5000;
		SetServiceStatus(m_ServiceStatusHandle, &m_ServiceStatus);
	}
#endif
	xmlCleanupParser();


	mysql_library_end();

	zSyslog._log(CSyslog::LOGL_INFO, CSyslog::LOGC_PROG_END, "Program ended");

	// printf("======= FINI ========\n\n\n");

	zSyslog.close();
#ifdef INSTALL_AS_NT_SERVICE
	if(!run_flag)
	{
		m_ServiceStatus.dwWin32ExitCode = 0;
		m_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
		m_ServiceStatus.dwCheckPoint = stop_pending_checkpoint++;
		m_ServiceStatus.dwWaitHint = 0;
		SetServiceStatus(m_ServiceStatusHandle, &m_ServiceStatus);
	}
#else
	return 0;
#endif
}

// ----------------------------------------------------------------------------
// callback of CConnbas_dbox::scanKwords(...) : global fct
// ----------------------------------------------------------------------------

void cbScanKwords(CConnbas_dbox *connbas, unsigned int kword_id, char *keyword, unsigned long keyword_len, char *lng, unsigned long lng_len)
{
	CIndexer *indexer = (CIndexer *) (connbas->userData);
	unsigned int hash = hashKword(keyword, keyword_len);
	CKword *k;
	if((k = new CKword(keyword, keyword_len, lng, lng_len)) != NULL)
	{
		k->id = kword_id;
		k->next = indexer->tKeywords[hash];
		indexer->tKeywords[hash] = k;
	}
}


// ----------------------------------------------------------------------------
// callback of CConnbas_dbox::scanXpaths(...) : global fct
// ----------------------------------------------------------------------------

void cbScanXpaths(CConnbas_dbox *connbas, unsigned int xpath_id, char *xpath, unsigned long xpath_len)
{
	CIndexer *indexer = (CIndexer *) (connbas->userData);
	CXPath *x;
	// new xpath
	if((x = new CXPath(xpath, xpath_len)) != NULL)
	{
		x->id = xpath_id;
		x->next = indexer->tXPaths;
		indexer->tXPaths = x;
	}
}

THREAD_ENTRYPOINT thread_index(void *parm)
{
	CSbas *sbas = (CSbas *) parm;
	int prefsIndexes_value, prefsIndexes_toReindex = 0;

	zSyslog._log(CSyslog::LOGL_INFO, CSyslog::LOGC_THREAD_START, "#%ld : thread_index START (%s:%ld:%s)", sbas->sbas_id, sbas->host, sbas->port, sbas->dbname);

	mysql_thread_init();

	THREAD_DETACH(sbas->idxthread);

	// create a cnx to the database to index
	CConnbas_dbox dbox(sbas->sbas_id, sbas->host, sbas->user, sbas->pwd, sbas->dbname, sbas->port);

	sbas->indexed = false;

	if(dbox.isok)
	{
		//		xmlInitParser();

		// create an indexer on the base
		CIndexer indexer(&dbox);

		// mem a ref to indexer in the connbas
		dbox.userData = (void *) (&indexer);

		// preload xpath
		int nxp;
		nxp = dbox.scanXPaths(cbScanXpaths);

		// preload keywords
		int nkw;
		nkw = dbox.scanKwords(cbScanKwords);

		zSyslog._log(CSyslog::LOGL_INFO, CSyslog::LOGC_PRELOAD, "#%d : %d xpaths pre-loaded, %d kwords pre-loaded", sbas->sbas_id, nxp, nkw);

		//		sbas->running = true;
//		printf("++++ %d +++++ sbas->status == SBAS_STATUS_TOSTOP ? : %d\n", __LINE__, sbas->status == SBAS_STATUS_TOSTOP);
//		printf("++++ %d +++++ prefsIndexes_toReindex = %d\n", __LINE__, prefsIndexes_toReindex);
//		printf("++++ %d +++++ running = %d\n", __LINE__, running);
		while(!dbox.crashed && running && sbas->status != SBAS_STATUS_TOSTOP && prefsIndexes_toReindex == 0)
		{
			dbox.selectPrefsIndexes(&prefsIndexes_value, &prefsIndexes_toReindex);
//			printf("++++ %d +++++ prefsIndexes_toReindex = %d\n", __LINE__, prefsIndexes_toReindex);
			if(prefsIndexes_toReindex > 0)
			{
				// ask to reindex : truncate indexes, flag records and let the thread end
				zSyslog._log(CSyslog::LOGL_INFO, CSyslog::LOGC_SIGNAL, "#%ld : reindex asked (%s:%ld:%s)", sbas->sbas_id, sbas->host, sbas->port, sbas->dbname);

				dbox.reindexAll();
				// let the thread end

				wait_loop = 0;	// quit main sbas loop faster
			}
			else
			{
//				printf("++++ %d +++++ prefsIndexes_value = %d\n", __LINE__, prefsIndexes_value);
				if(prefsIndexes_value > 0)
				{
					startChrono(dbox.reindexChrono);

					// index every records to index (flush every 50)
					indexer.connbas->scanRecords(callbackRecord, &(sbas->status));

					// flush before quit
					indexer.flush();

					if(indexer.nrecsIndexed > 0)
					{
						zSyslog._log(CSyslog::LOGL_INFO, CSyslog::LOGC_INDEXING, "%d records indexed in %f sec.\n", indexer.nrecsIndexed, stopChrono(dbox.reindexChrono));
						indexer.nrecsIndexed = 0;
					}

					sbas->indexed = true;
				}

				if(quit_flag)
				{
					// run once
					break;
				}
				else
				{
					// sleep for a while
					for(int i = 0; i < 4 && running && sbas->status != SBAS_STATUS_TOSTOP; i++)
						SLEEP(1);
				}
			}

		}

		// close cnx to the dbox
		dbox.close();

		// end libxml
		//		xmlCleanupParser();
//		printf("++++ %d +++++ sbas->indexed = %d\n", __LINE__, sbas->indexed);
	}

	mysql_thread_end();

	zSyslog._log(CSyslog::LOGL_INFO, CSyslog::LOGC_THREAD_END, "#%ld : thread_index END (%s:%ld:%s)", sbas->sbas_id, sbas->host, sbas->port, sbas->dbname);

	// this thread is finished, it says it to the main loop via the pool
	sbas->idxthread = (ATHREAD) NULLTHREAD;
	sbas->status = SBAS_STATUS_FINISHED;

	THREAD_EXIT(NULL); // NULL parameter discarded with win32
}

/* */

