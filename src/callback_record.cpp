#include <stdio.h>

#include <stdlib.h>
#include <stdio.h>
#include "_syslog.h"
// #include <basetsd.h>
#include <ctype.h>
#include <string>


#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

#if defined(LIBXML_XPATH_ENABLED) && defined(LIBXML_SAX1_ENABLED) && defined(LIBXML_OUTPUT_ENABLED)
#else
	#error "XPath support not compiled in libxml"
#endif


#include "phrasea_clock_t.h"
#include "dom.h"
#include "connbas_dbox.h"
#include "sbas.h"

#include "indexer.h"

extern char arg_forcedefault;

// prototypes local fcts
void evt_start(CDOMDocument *xmlparser, const char *name, const char *path, const char *upath);
void evt_end(CDOMDocument *xmlparser);
void evt_keyword(CDOMDocument *xmlparser, const char *lowKeyword, unsigned int lowKeywordLen, unsigned int pos, unsigned int len, unsigned int index, const char *lng, unsigned int lngLen);

// prototypes external fcts
extern CSyslog zSyslog; // , LOG_PID, LOG_DAEMON);
extern const char *arg_clng;
void loadThesaurus(CIndexer *indexer);


// ----------------------------------------------
// the parser has seen an  opening tag
// path  : xpath as "/record/description/MotCle"
// upath : xpath as "/RECORD[0]/DESCRIPTION[0]/MOTCLE[4]"
// ----------------------------------------------
void evt_start(CDOMDocument *xmlparser, const char *name, const char *path, const char *upath)
{
	// get data passed to this callback
	CIndexer *indexer = (CIndexer *)(xmlparser->userData);
	// search/create the upath into the xpaths list
	CXPath *xp;
	for(xp=indexer->tXPaths; xp; xp=xp->next)
	{
		if(strcmp((char*)(xp->upath), (char *)upath) == 0)
			break;
	}
	if(!xp)
	{
		if( (xp = new CXPath(upath, strlen((char *)upath))) )
		{
			xp->next = indexer->tXPaths;
			indexer->tXPaths = xp;
		}
	}
	indexer->current_xpath = xp;

	// search the field into the structure
	indexer->xmlparser->parseText = false;
	indexer->xmlparser->getContent = false;

	indexer->xmlparser->onEnd = NULL;
	for(int i=0; i<indexer->nStructFields; i++)
	{
		if(strcmp(path, indexer->tStructField[i].fullpath)==0)
		{
			// the field is known in the structure
			// so we tell it to the current node of the parser
			indexer->xmlparser->currentNode->field = indexer->tStructField + i;

			if(xp)
				xp->field = indexer->tStructField + i;
			indexer->xmlparser->parseText = true;

			if(indexer->tStructField[i].tXPathCtxThesaurus || indexer->tStructField[i].type != CStructField::TYPE_NONE)
			{
				// the current field has at least one xpathcontext to a branch of the thesaurus, or a type
				// ... so we need the content of the field
				// ... so we tell the parser to get the content
				indexer->xmlparser->getContent = true;
				// ... and we ask a callback a the end of the tag
				indexer->xmlparser->onEnd = evt_end;
			}
			break;
		}
	}
}

bool is_integer(char *s)
{
	while(*s && *s>='0' && *s<='9')
		s++;
	return(*s == '\0');
}

bool is_multidigits(char *s)
{
	while(*s && ((*s>='0' && *s<='9') || *s=='_' || isspace(*s)))
		s++;
	return(*s == '\0');
}

bool is_delimdate(char *s)
{
	char buff[90];
	int l;
	int date[6];
	if( (l = strlen(s)) > 89 )
		l = 89;
	memcpy(buff, s, l+1);
	buff[l+1] = '\0';
	while(l >= 0)
	{
		if(buff[l]<'0' || buff[l]>'9')
			buff[l] = ' ';
		l--;
	}
	l = sscanf(buff, "%d %d %d %d %d %d", &date[0], &date[1], &date[2], &date[3], &date[4], &date[5]);
	return(l==3 || l==6);
}


void evt_end(CDOMDocument *xmlparser)
{
	// get data passed to this callback
	CIndexer *indexer = (CIndexer *)(xmlparser->userData);
	CDOMElement *currentNode = indexer->xmlparser->currentNode;
	CStructField *currentField = currentNode->field;

	currentNode->addLowValueC('\0', CFLAG_NORMALCHAR);
	currentNode->addValueC('\0', CFLAG_NORMALCHAR);

	char *lowValue = currentNode->lowValue;
	char *value = currentNode->value;
	if(lowValue[0] == '\0')
		return;			// champ vide

	char strbuff[1000];
	std::string cstr;

	snprintf(strbuff, 1000, "field <%s> start=%d, end=%d\n", currentField->name, currentNode->index_start, currentNode->index_end);
	cstr += strbuff;

	if(currentField && currentField->type != CStructField::TYPE_NONE)
	{
		// the current field has a type
		CProp *prop = NULL;
		char buff[90];
		double fv;	// float value
		int lv;	// int value
		int l;
		int date[6];
		switch(currentField->type)
		{
			case CStructField::TYPE_FLOAT:
				fv = atof((char *)lowValue);
				l = sprintf(buff, "%f", fv);
				if(l>0 && buff[l-1]=='.')
					buff[--l] = '\0';
				prop = new CProp(buff);

				snprintf(strbuff, 1000, "got prop '%s' of type=TYPE_FLOAT (%d) \n", buff, currentField->type);
				cstr += strbuff;
				break;
			case CStructField::TYPE_INT:
				lv = atol((char *)lowValue);
				l = sprintf(buff, "%d", lv);
				prop = new CProp(buff);

				snprintf(strbuff, 1000, "got prop '%s' of type=TYPE_INT (%d) \n", buff, currentField->type);
				cstr += strbuff;

				break;
			case CStructField::TYPE_DATE:
				if( (l = strlen((char *)lowValue)) > 89 )
					l = 89;
				memcpy(buff, lowValue, l+1);
				buff[l+1] = '\0';
				while(l >= 0)
				{
					if(buff[l]<'0' || buff[l]>'9')
						buff[l] = ' ';
					l--;
				}
				l = sscanf(buff, "%d %d %d %d %d %d", &date[0], &date[1], &date[2], &date[3], &date[4], &date[5]);
				if(l == EOF)
				{
					for(l=0; l<6; l++)
						date[l] = 0;
				}
				else
				{
					while(l < 6)
						date[l++] = 0;
					if(date[0] < 0)
						date[0] = 0;
					else
						if(date[0]<20)
							date[0] += 2000;
						else
							if(date[0] < 100)
								date[0] += 1900;
							else
								if(date[0] >9999)
									date[0] = 9999;
					for(l=1; l<6; l++)
					{
						if(date[l] < 0)
							date[l] = 0;
						else
							if(date[l] > 99)
								date[l] = 99;
					}
				}
				sprintf(buff, "%04d%02d%02d%02d%02d%02d", date[0], date[1], date[2], date[3], date[4], date[5] );

				snprintf(strbuff, 1000, "got prop '%s' of type=TYPE_DATE (%d) \n", buff, currentField->type);
				cstr += strbuff;

				prop = new CProp(buff);
				break;
			case CStructField::TYPE_TEXT:

				snprintf(strbuff, 1000, "got prop '%s' of type=TYPE_TEXT (%d) \n", lowValue, currentField->type);
				cstr += strbuff;

				prop = new CProp(lowValue);
				break;
		}
		if( prop )
		{
			prop->type = currentField->type;
			prop->record_id = indexer->current_rid;
//			prop->pxpath = indexer->current_xpath;
			prop->pfield = indexer->current_xpath->field;
			prop->business = currentField->business;
			prop->next = indexer->firstProp;
			indexer->firstProp = prop;

			currentField->found = TRUE;
		}
	}

	if(currentField && currentField->nXPathCtxThesaurus > 0)
	{
		// there's a least one tbranch, we search in the thesaurus

		// search the value in the thesaurus
		if(lowValue && currentNode->index_end >= currentNode->index_start)
		{
			char *w = NULL;
			char *k = NULL;
			int lw=0, lk=0;
			register int i;

			// delete quotes (simples and doubles) of the lowvalue
			for(i=0; i <= currentNode->value_length; i++)
			{
				if(currentNode->lowValue[i] == '\'' || currentNode->lowValue[i] == '"')
					currentNode->lowValue[i] = ' ';
			}

			if(currentNode->t0 >= 0 && (lw=(currentNode->t1-currentNode->t0+1)) > 0)
			{
				w = currentNode->lowValue + currentNode->t0;
				currentNode->lowValue[currentNode->t1+1] = '\0';
			}
			if(currentNode->k0 >= 0 && (lk=(currentNode->k1-currentNode->k0+1)) > 0)
			{
				k = currentNode->lowValue + currentNode->k0;
				currentNode->lowValue[currentNode->k1+1] = '\0';
			}

			// on cherche dans le thesaurus
			snprintf(strbuff, 1000, "  searching in %d thesaurus branches w='%s' ; k='%s' :\n", currentField->nNodesThesaurus , w?w:(char *)"NULL", k?k:(char *)"NULL");
			cstr += strbuff;

			int nfound = 0;
			for(i=0; i < currentField->nNodesThesaurus; i++)
			{
				CtidSet tids;
				tids.find(currentField->tNodesThesaurus[i], w, k);

				CTHit *thit;
				for(int j=0; j < tids.idNr; j++)
				{
					if(tids.idTab[j] && (thit = new CTHit(tids.idTab[j])) )
					{
						snprintf(strbuff, 1000, "   -> found id='%s'\n", tids.idTab[j]);
						cstr += strbuff;

						thit->record_id = indexer->current_rid;
						thit->pxpath = indexer->current_xpath;
						if(indexer->firstTHit)
							thit->next = indexer->firstTHit;
						indexer->firstTHit = thit;

						thit->hitstart = currentNode->index_start;
						thit->hitlen   = currentNode->index_end - currentNode->index_start + 1;

						thit->business = currentField->business;
					}
				}
				nfound += tids.idNr;
			}

			// if we don't want candidates, return
			if(!currentField->candidatesDates && !currentField->candidatesIntegers && !currentField->candidatesStrings
							&& !currentField->candidatesFirstDigit && !currentField->candidatesMultiDigits)
				return;

			if(nfound == 0)
			{
				// not found in the thesaurus, search in the cterms
				snprintf(strbuff, 1000, "  searching in the cterms branch :\n");
				cstr += strbuff;

				CtidSet tids;
				tids.find(currentField->xmlNodeCterms, w, k);

				for(int j=0; j < tids.idNr; j++)
				{
					CTHit *thit;
					if(tids.idTab[j] && (thit = new CTHit(tids.idTab[j])) )
					{
						snprintf(strbuff, 1000, "   -> found id='%s'\n", tids.idTab[j]);
						cstr += strbuff;

						thit->record_id = indexer->current_rid;
						thit->pxpath = indexer->current_xpath;
						if(indexer->firstTHit)
							thit->next = indexer->firstTHit;
						indexer->firstTHit = thit;

						thit->hitstart = currentNode->index_start;
						thit->hitlen   = currentNode->index_end - currentNode->index_start + 1;

						thit->business = currentField->business;
					}
				}
				nfound += tids.idNr;
			}

			if(nfound == 0 && indexer->xmlNodePtr_deleted)
			{
				// not found in thesaurus neither in cterms : search in deleted (if the 'deleted' branch exists)
				snprintf(strbuff, 1000, "  searching in the deleted branch :\n");
				cstr += strbuff;

				CtidSet tids;
				tids.find(indexer->xmlNodePtr_deleted, w, k);

				for(int j=0; j < tids.idNr; j++)
				{
					CTHit *thit;
					if(tids.idTab[j] && (thit = new CTHit(tids.idTab[j])) )
					{
						snprintf(strbuff, 1000, "   -> found id='%s'\n", tids.idTab[j]);
						cstr += strbuff;

						thit->record_id = indexer->current_rid;
						thit->pxpath    = indexer->current_xpath;
						if(indexer->firstTHit)
							thit->next = indexer->firstTHit;
						indexer->firstTHit = thit;

						thit->hitstart = currentNode->index_start;
						thit->hitlen   = currentNode->index_end - currentNode->index_start + 1;

						thit->business = currentField->business;
					}
				}
				nfound += tids.idNr;
			}

			if(nfound == 0)
			{
				// not found in thesaurus neither in cterms neither in deleted : create in cterms
				// check if this term can be candidate
				bool canBeCandidate = false;
				if(is_delimdate(lowValue))
				{
					if(currentField->candidatesDates)
						canBeCandidate = true;
				}
				else if(is_multidigits(lowValue))
				{
					if(currentField->candidatesMultiDigits)
						canBeCandidate = true;
				}
				else if(is_integer(lowValue))
				{
					if(currentField->candidatesIntegers)
						canBeCandidate = true;
				}
				else if(*lowValue>='0' && *lowValue<='9')
				{
					if(currentField->candidatesFirstDigit)
						canBeCandidate = true;
				}
				else
				{
					if(currentField->candidatesStrings)
						canBeCandidate = true;
				}

				if(!canBeCandidate)
					return;

				xmlNodePtr cbranch = currentField->xmlNodeCterms;

				// get the nextid
				xmlChar *id=NULL, *nextid=NULL;
				if( (id = xmlGetProp(cbranch, (const xmlChar *)"id")) && (nextid = xmlGetProp(cbranch, (const xmlChar *)"nextid")) )
				{
					char *buff;
					int l_id     = strlen((const char *)id);
					int l_nextid = strlen((const char *)nextid);
					if( (buff = (char *)_MALLOC_WHY(l_id + 1 + l_nextid + 2 + 1, "main.cpp:evt_end:buff")) )
					{
						xmlNodePtr te, sy;
						if((te = xmlNewChild(cbranch, NULL, (const xmlChar*)"te", NULL)) != NULL)
						{
							if((sy = xmlNewChild(te, NULL, (const xmlChar*)"sy", NULL)) != NULL)
							{
								memcpy(buff, id, l_id);
								buff[l_id] = '.';
								memcpy(buff+l_id+1, nextid, l_nextid + 1);

								// prop 'id' of new 'te'
								xmlSetProp(te, (const xmlChar*)"id", (const xmlChar *)buff);

								// prop 'nextid' of new te
								xmlSetProp(te, (const xmlChar*)"nextid", (const xmlChar *)"1");

								// prop 'id' of new 'sy'
								memcpy(buff+l_id+1+l_nextid, ".0", 3);
								xmlSetProp(sy, (const xmlChar*)"id", (const xmlChar *)buff);

								// add the thit to the record
								if(CTHit *thit = new CTHit(buff) )
								{
									thit->record_id = indexer->current_rid;
									thit->pxpath = indexer->current_xpath;
									if(indexer->firstTHit)
										thit->next = indexer->firstTHit;
									indexer->firstTHit = thit;

									thit->hitstart = currentNode->index_start;
									thit->hitlen   = currentNode->index_end - currentNode->index_start + 1;

									thit->business = currentField->business;
								}

								// prop 'lng' of the new 'sy'
								xmlSetProp(sy, (const xmlChar*)"lng", (const xmlChar *)arg_clng);

								// prop 'v' of the new 'sy'
								xmlSetProp(sy, (const xmlChar*)"v", (const xmlChar *)value);

								if(currentNode->t0 >= 0)
								{
									// prop 'w' of the new 'sy'
									xmlSetProp(sy, (const xmlChar*)"w", (const xmlChar *)lowValue + currentNode->t0);
								}

								if(currentNode->k0 >= 0)
								{
									// prop 'k' of the new 'sy'
									xmlSetProp(sy, (const xmlChar*)"k", (const xmlChar *)lowValue + currentNode->k0);
								}

								// prop 'nextid' du nvo 'sy'
								xmlSetProp(sy, (const xmlChar*)"nextid", (const xmlChar *)"0");


								// increase nextid
								char ibuff[34];
								sprintf(ibuff, "%d", atoi((const char *)nextid) + 1);
								xmlSetProp(cbranch, (const xmlChar*)"nextid", (const xmlChar *)ibuff );
							}
						}
						_FREE(buff);

						indexer->ctermsChanged = true;

//						saveCterms(indexer);
					}
				}
				if(nextid)
					xmlFree(nextid);
				if(id)
					xmlFree(id);
			}
		}
		else
		{
//				printf("no-value !!\n");
		}
	}
	// printf("evt_end(end)\n");

	zSyslog._log(CSyslog::LOGL_PARSE, CSyslog::LOGC_INDEXING, (TCHAR *)(cstr.c_str()) );
}

// ----------------------------------------------
// the parser met a keyword
// ----------------------------------------------
void evt_keyword(CDOMDocument *xmlparser, const char *lowKeyword, unsigned int lowKeywordLen, unsigned int pos, unsigned int len, unsigned int index, const char *lng, unsigned int lngLen)
{
	CIndexer *indexer = (CIndexer *)(xmlparser->userData);

	CStructField *currentField = indexer->xmlparser->currentNode->field;

	if(!currentField || currentField->index==false)
		return;

	unsigned int record_id = indexer->current_rid;

	// on finit notre keyword par '\0'
	char buff[256];
	if(lowKeywordLen > 255)
		lowKeywordLen = 255;
	memcpy(buff, lowKeyword, lowKeywordLen);
	buff[lowKeywordLen] = '\0';

	unsigned int hash = hashKword(lowKeyword, lowKeywordLen);

	// on le cherche dans le tableau de kword
	CKword *k;
	for(k=indexer->tKeywords[hash]; k; k=k->next)
	{
		if(strcmp((char *)(k->kword), buff) == 0 && strcmp(k->lng, lng) == 0)
			break;
	}
	if(!k)
	{
		if(debug_flag && DEBUG_PARSE)
		{
			char buff[100];
			int l = lowKeywordLen;
			if(l>99)
				l=99;
			memcpy(buff, lowKeyword, l);
			buff[l] = '\0';
			zSyslog._log(CSyslog::LOGL_PARSE, CSyslog::LOGC_THESAURUS, "%s(%d) unknown kword('%s'/'%s')\n", __FILE__, __LINE__, buff, lng);
		}
		// new keyword
		if( (k = new CKword(lowKeyword, lowKeywordLen, lng, lngLen)) != NULL)
		{
			k->next = indexer->tKeywords[hash];
			indexer->tKeywords[hash] = k;

			indexer->nNewKeywords++;
		}
	}
	if(k)
	{
		CHit *h;
		if( (h = new CHit(record_id, pos, len, index, currentField->business)) != NULL)
		{
			h->pxpath = indexer->current_xpath;
			h->next = k->firsthit;
			k->firsthit = h;
		}
	}
}




// ------------------------------------------------------------------------
// callback called by scanRecords, for each record
// ------------------------------------------------------------------------
void callbackRecord(CConnbas_dbox *connbas, unsigned int record_id, char *xml, unsigned long len)
{
	extern int arg_flush;
	CIndexer *indexer = (CIndexer *)(connbas->userData);

	zSyslog._log(CSyslog::LOGL_RECORD, CSyslog::LOGC_INDEXING, "#%ld : Indexing recordid=%d", connbas->sbas_id, record_id);

	// create a xml parser
	if( (indexer->xmlparser = new CDOMDocument()) )
	{
		// we reload the thesaurus
		loadThesaurus(indexer);

		// let's flag the record as 'indexing' (status bit 2 to '0')
		indexer->connbas->updateRecord_lock(record_id);

		if( CRecord *record = new CRecord )
		{
			record->id = record_id;
			record->next = indexer->tRecord;
			indexer->tRecord = record;
		}

		// tell the indexer who to call when it parses a word in the xml
		indexer->xmlparser->userData = (void *)indexer;
		indexer->xmlparser->onKeyword = evt_keyword;
		indexer->xmlparser->onStart   = evt_start;
		indexer->xmlparser->onEnd	  = NULL;		// it's the opening tag who decides if a close callback is needed (if field with type or thesaurus)

		indexer->current_rid = record_id;

		// mark each field as not found before indexing
		for(int i=0; i<indexer->nStructFields; i++)
			indexer->tStructField[i].found = FALSE;
		
		// load and parse in one shot
		indexer->xmlparser->loadXML(xml, len);

		// fix unfound fields with a default value

		if(arg_forcedefault == 'A' || arg_forcedefault == 'Z')
		{
			for(int i=0; i<indexer->nStructFields; i++)
			{
				if(!indexer->tStructField[i].found && indexer->tStructField[i].type != CStructField::TYPE_NONE)
				{
					CProp *prop;
					switch(indexer->tStructField[i].type)
					{
						case CStructField::TYPE_DATE:
							prop = new CProp(arg_forcedefault=='A' ? "00000000000000" : "99999999999999");
							break;
						case CStructField::TYPE_FLOAT:
							prop = new CProp(arg_forcedefault=='A' ? "0" : "99999999999999");
							break;
						case CStructField::TYPE_INT:
							prop = new CProp(arg_forcedefault=='A' ? "0" : "99999999999999");
							break;
						case CStructField::TYPE_TEXT:
							prop = new CProp(arg_forcedefault=='A' ? "" : "ZZZZZZZZZZZZZZ");
							break;
					}
					if( prop )
					{
						prop->type = indexer->tStructField[i].type;
						prop->record_id = indexer->current_rid;
						// prop->pxpath = NULL;
						prop->pfield = indexer->tStructField + i;
						prop->business = indexer->tStructField[i].business;
						prop->next = indexer->firstProp;
						indexer->firstProp = prop;
					}
				}
			}
		}

		indexer->nrecsInBuff++;

		if(indexer->nrecsInBuff >= arg_flush)		// flush infos every n records
		{
			indexer->flush();

			indexer->nrecsInBuff	= 0;
		}

		// destroy the parser
		delete indexer->xmlparser;

		// flag the record as 'indexed' (status bit 2 to '1')
//		indexer->connbas->updateRecord_unlock(record_id);			// not now ! flush will do the job
	}
}

