#include <memory.h>
#include <string.h>
#include <stdio.h>
#include "_syslog.h"

#include "trace_memory.h"

#include "dom.h"
#include "lownodiacritics_utf8.h"
// #include "lownodiacritics_utf8.cpp"
#include "phrasea_clock_t.h"
#include "indexer.h"

extern CMAP1 cmap_1[128];
extern CMAP2 cmap_2[1920];

extern int narg_stem;
extern const char *arg_stem[];

extern CSyslog zSyslog;


//--------------------------------------------------------------------
// DOMDocument

void CDOMDocument::flushToken()
{
	CIndexer *indexer = (CIndexer *)(this->userData);

	if(this->onKeyword)
	{
		(this->onKeyword)(this, this->lowtokBin, this->lowtokBinLen, this->indexStart, this->tokBinLen, this->wordIndex, "", 0);

		for(int i=0; i<narg_stem; i++)
		{
			if(indexer->stemmer[i])
			{
				const sb_symbol *stemmed = sb_stemmer_stem(indexer->stemmer[i], (const sb_symbol *)(this->lowtokBin), this->lowtokBinLen);
				int stemmedLen = sb_stemmer_length(indexer->stemmer[i]);

				(this->onKeyword)(this, (char *)stemmed, stemmedLen, this->indexStart, this->tokBinLen, this->wordIndex, arg_stem[i], strlen(arg_stem[i]));
			}
		}
	}
	this->wordIndex++;
}


void XMLCALL CDOMDocument::start(void *userData, const char *el, const char **attr)
{
  CDOMDocument *_this = (CDOMDocument *)(userData);

	if(CDOMElement *node = _this->createElement(el))
	{
		if(!_this->documentElement)
		{
			_this->documentElement = node;
		}
		else
		{
			node->parentNode = _this->currentNode;
			if(!_this->currentNode->lastChild)
			{
				_this->currentNode->firstChild = _this->currentNode->lastChild = node;
			}
			else
			{
				for(CDOMNode *n=_this->currentNode->lastChild; n; n=n->previousSibling)
				{
					if(n->nodeType==CDOMNode::XML_ELEMENT_NODE && strcmp(((CDOMElement*)n)->tagName, el)==0)
					{
						node->index = ((CDOMElement*)n)->index+1;
						break;
					}
				}
				_this->currentNode->lastChild->nextSibling = node;
				node->previousSibling = _this->currentNode->lastChild;
				_this->currentNode->lastChild = node;
			}
		}
		_this->currentNode = node;

		int len_el = strlen(el)+1;
		int m;

		node->pathoffset = _this->freepathoffset;
		m = _this->path_msize;
		if(_this->freepathoffset + 1 + len_el > m)
		{
			m = _this->freepathoffset + 1 + len_el;
			_this->path = (char *)_REALLOC(_this->path, m);
		}
		if(_this->path)
		{
			_this->path_msize = m;

			_this->path[_this->freepathoffset] = '/';
			memcpy(_this->path+_this->freepathoffset+1, el, len_el);
			_this->freepathoffset += len_el;
		}

		node->upathoffset = _this->freeupathoffset;
		m = _this->upath_msize;
		if(_this->freeupathoffset + 1 + len_el + 1+33+1 > m)
		{
			m = _this->freeupathoffset + 1 + len_el + 1+33+1;
			_this->upath = (char *)_REALLOC(_this->upath, m);
		}
		if(_this->upath)
		{
			_this->upath_msize = m;

			_this->upath[_this->freeupathoffset] = '/';
			memcpy(_this->upath+_this->freeupathoffset+1, el, len_el);
			_strupr((char *)(_this->upath) + _this->freeupathoffset+1);
			_this->freeupathoffset += len_el;
			_this->freeupathoffset += sprintf((char *)(_this->upath) + _this->freeupathoffset, "[%i]", node->index);
		}

		if(_this->onStart)
			(_this->onStart)(_this, (const char *)el, _this->path, _this->upath);
	}
	_this->depth++;
}


void XMLCALL CDOMDocument::end(void *userData, const char *el)
{
  CDOMDocument *_this = (CDOMDocument *)(userData);
	if(_this->currentNode)
	{
		if(_this->onEnd)
		{
			// ends the node's value ('lowed') properly
			if(_this->currentNode->lowValue)
				_this->currentNode->lowValue[_this->currentNode->lowValue_length] = '\0';
			(_this->onEnd)(_this);		// callback 'end'
		}
		// go back to the parent node
		if(_this->currentNode->parentNode)
		{
			// reset path & upath to parent
			_this->freepathoffset = _this->currentNode->pathoffset;
			_this->freeupathoffset = _this->currentNode->upathoffset;
			// set current node back
			_this->currentNode = _this->currentNode->parentNode;
		}
	}
	_this->depth--;
}

void XMLCALL CDOMDocument::startCdata(void *userData)
{
  CDOMDocument *_this = (CDOMDocument *)(userData);
	_this->State = CDOMDocument::INTO_CDATA;
}

void XMLCALL CDOMDocument::endCdata(void *userData)
{
  CDOMDocument *_this = (CDOMDocument *)(userData);
	_this->State = CDOMDocument::INTO_UNKNOWN;
}


void XMLCALL CDOMDocument::charHandler(void *userData, const XML_Char *xmls, int len)
{
  CDOMDocument *_this = (CDOMDocument *)(userData);

	if(!_this->parseText)
		return;
//		printf("charHandler @0x%04X  [len=%d] :\n", 888, len);

	XML_Index index = XML_GetCurrentByteIndex(_this->parser);

	if(_this->State != CDOMDocument::INTO_CDATA)
	{
	  int i;
	  unsigned char c0, c;
	  unsigned int  u, msk;
	  unsigned char nBytes;
	  unsigned char nLowBytes;
	  unsigned char *s = (unsigned char *)xmls;
	  char cbreak;
	  register char *p;

		i = 0;
		if(_this->currentNode->index_start == 0)
			_this->currentNode->index_start = index+i;

		while(len >= 0)
		{
			if(len == 0)
			{
				// at the end of data
				nBytes = nLowBytes = 0;
				cbreak = true;
				len--;
			}
			else
			{
				_this->currentNode->index_end = index+i;

				if(_this->indexStart == 0)
					_this->indexStart = index+i;

				cbreak = 0;
				u = 0xFFFFFFFF;
				c0 = *s++;
				len--;
				i++;

				// calculate the 'lowed' value of the char
				if(c0 & 0x80)
				{
					if(c0 & 0x40)
					{
						// 11xxxxxx : multi bytes character
						unsigned char flags = CFLAG_NORMALCHAR;
						_this->tokBin[_this->tokBinLen++] = c0;
						u = ((unsigned int) c0) & 0x0000001F;
						msk = 0xFFFFFF7F;
						nBytes = 1;
						// read max 6 bytes
						while(len && ((c0 <<= 1) & 0x80) && (((c = *s) & 0xC0) == 0x80) && ++nBytes <= 6)
						{
							_this->tokBin[_this->tokBinLen++] = c;
							u = (u<<6 & (msk = (msk<<5) | 0x1F)) | (unsigned int)(c & 0x3F);
							len--;
							i++;
							s++;
							_this->currentNode->index_end++;
						}
						if(nBytes <= 4)
						{
							// char in 2. 3 or 4 bytes
							if(u >= 0x0080  &&  u <= 0x07FF)
							{
								// char on 2 bytes : transcode via look-up table cmap_2
								flags = cmap_2[u - 0x0080].flags;
								cbreak = (flags & CFLAG_ENDCHAR) ? 1 : 0;
								nLowBytes = 0;
								for(p = (char *)(cmap_2[u - 0x0080].s); *p; p++)
								{
									_this->lowtokBin[_this->lowtokBinLen++] = *p;
									nLowBytes++;
									if(_this->getContent)
									{
										_this->currentNode->addLowValueC(*p, cmap_2[u - 0x0080].flags);
									}
								}
							}
							else
							{
								// char on 3 or 4 bytes : don't transcode
								register int j;
								for(j=0, s-=nBytes; j<nBytes; j++, s++)
								{
									_this->lowtokBin[_this->lowtokBinLen++] = *s;
									nLowBytes++;
									if(_this->getContent)
									{
										_this->currentNode->addLowValueC(*s, 0);
									}
								}
							}
						}
						else
						{
							// char on 5 or 6 bytes : skip
							u = 0xFFFFFFFF;
						}

						if(_this->getContent)
						{
							register int j;
							for(j=0, s-=nBytes; j<nBytes; j++)
							{
								// add the byte to the 'value' of the curent node
								_this->currentNode->addValueC(*s++, flags);
							}
						}
					}
					else
					{
						// 10xxxxxx : inproper byte as c0
					}
				}
				else
				{
					// 0xxxxxxx : 1 byte char, transcode via look-up table cmap_1
					unsigned char flags = cmap_1[(int)c0].flags;
					cbreak = (flags & CFLAG_ENDCHAR) ? 1 : 0;

					_this->tokBin[_this->tokBinLen++] = c0;
					_this->lowtokBin[_this->lowtokBinLen++] = cmap_1[(int)c0].c;
					if(_this->getContent)
					{
						// add the byte to the 'value' of the curent node
						_this->currentNode->addValueC(c0, flags);

						// add the transcoded byte to the 'lowed value' of the curent node
						_this->currentNode->addLowValueC(cmap_1[(int)c0].c, flags);
					}
					u = (unsigned int) c0;
					nLowBytes = nBytes = 1;

				}

				_this->indexEnd = index+i;

			}
			if(cbreak || _this->tokBinLen>=400)	// cbreak or buffer full
			{
				if(cbreak)
				{
					_this->tokBinLen -= nBytes;			// remove the cbreak
					_this->lowtokBinLen -= nLowBytes;	// remove the cbreak
				}
				if(_this->tokBinLen > 0)
				{
					if(cbreak)
					{
						_this->indexEnd -= nBytes;		// remove the cbreak
					}
				//	_this->currentNode->addLowValueC('\0', CFLAG_NORMALCHAR);
					_this->flushToken();
				}
				_this->lowtokBinLen = _this->tokBinLen = 0;
				_this->indexStart = 0;
			}
			else	// normal char
			{
			}
		}
	}
}

void CDOMDocument::dump()
{
	if(this->documentElement)
		this->documentElement->dump();
}

CDOMDocument::CDOMDocument()
{
	this->onKeyword = NULL;
	this->currentNode = this->documentElement = NULL;

	this->path  = NULL;
	this->upath = NULL;
	this->path_msize  = 0;
	this->upath_msize = 0;
	this->onStart = NULL;
	this->onEnd = NULL;
	this->onKeyword = NULL;

	if(	(this->parser = XML_ParserCreate(NULL)) )
	{
		XML_SetUserData(this->parser, this);
		XML_SetElementHandler(this->parser, this->start, this->end );
		XML_SetCharacterDataHandler(this->parser, this->charHandler);
		XML_SetCdataSectionHandler(this->parser, this->startCdata, this->endCdata);
	}
}


bool CDOMDocument::loadXML(char *xml, unsigned long len)
{
	bool ret = TRUE;
//	void *buff;

	if(!this->parser)
		return(FALSE);

	this->depth = -1;
	this->State = CDOMDocument::INTO_UNKNOWN;
	this->indexStart = 0;
	this->indexEnd = 0;
	this->tokBinLen = 0;
	this->lowtokBinLen = 0;
	this->wordIndex = 0;
	this->parseText = true;

	if(this->path)
		_FREE(this->path);
	this->path = (char *)_MALLOC_WHY(200, "dom.cpp:loadXML:path");
	if(this->path)
	{
		this->path_msize = 200;
		this->path[0] = '\0';
		this->freepathoffset = 0;
	}

	if(this->upath)
		_FREE(this->upath);
	this->upath = (char *)_MALLOC_WHY(200, "dom.cpp:loadXML:upath");
	if(this->upath)
	{
		this->upath_msize = 200;
		this->upath[0] = '\0';
		this->freeupathoffset = 0;
	}

	if(XML_Parse(this->parser, xml, len, TRUE) != XML_STATUS_ERROR)
	{
	}
	else
	{
		// handle parse error
		zSyslog._log(CSyslog::LOGL_ERR, CSyslog::LOGC_XMLERR, "Parse error at line %u:\n%s\n",
												  XML_GetCurrentLineNumber(this->parser),
												  XML_ErrorString(XML_GetErrorCode(this->parser)));
		ret = FALSE;
	}

	return(ret);
}


CDOMDocument::~CDOMDocument()
{
	if(this->documentElement)
		delete(this->documentElement);
	if(this->parser)
		XML_ParserFree(this->parser);
	if(this->path)
		_FREE(this->path);
	if(this->upath)
		_FREE(this->upath);
}



//--------------------------------------------------------------------
// DOMElement

CDOMElement::CDOMElement(const char *s, class CDOMDocument *owner)
{
	int l;
	this->ownerDocument = owner;
	this->firstChild = this->lastChild = NULL;
	this->nodeType = CDOMNode::XML_ELEMENT_NODE;
	if( (this->tagName = (char *)_MALLOC_WHY(l = strlen(s)+1, "dom.h:CDOMElement:tagName")) )
		memcpy(this->tagName, s, l);

	this->lowValue = NULL;
	this->lowValue_buffer_size = 0;
	this->lowValue_length = 0;

	this->value = NULL;
	this->value_buffer_size = 0;
	this->value_length = 0;
	this->value_end = 0;

	this->index = 0;
	this->pathoffset = 0;

	this->field = NULL;

	this->index_start = this->index_end = 0;

	this->t0 = this->t1 = this->k0 = this->k1 = -1;

	this->ink = 0;

	this->lastFlags = CFLAG_ENDCHAR;
};

CDOMElement::~CDOMElement()
{
	// printf("~DOMElement(%s)\n", this->tagName);
	CDOMNode *n;
	while( (n = this->firstChild) )
	{
		this->firstChild = n->nextSibling;
		switch(n->nodeType)
		{
			case CDOMNode::XML_ELEMENT_NODE:
				delete ((CDOMElement *)n);
				break;
		}
	}
	if(this->tagName)
		_FREE(this->tagName);

	if(this->lowValue)
		_FREE(this->lowValue);

	if(this->value)
		_FREE(this->value);
};

void CDOMElement::dump(int depth)
{
	int i;
	for(i=0; i<depth; i++)
		putchar('\t');
	if(!this->firstChild)
	{
		printf("<%s/> [%d]\n", this->tagName, this->index);
	}
	else
	{
		CDOMNode *n;
		printf("<%s> [%d]\n", this->tagName, this->index);
		for(n=this->firstChild; n; n=n->nextSibling)
			n->dump(depth+1);
		for(i=0; i<depth; i++)
			putchar('\t');
		printf("</%s>\n", this->tagName);
	}
};

// add a byte to the 'value' of the current field
void CDOMElement::addValueC(char c, unsigned char flags)
{
	// increase size anyway
	if(this->value_length+1 >= this->value_buffer_size)
		this->value = (char *)_REALLOC(this->value, (this->value_buffer_size+=128)) ;

	if(c == '\0')
	{
		// special case : sent by the end of the tag
		this->value[this->value_length = this->value_end] = '\0';	// rtrim
		return;
	}

	// left trim
	if(this->value_length == 0 && flags & CFLAG_SPACECHAR)
		return;

	if(this->value)
	{
		this->value[this->value_length++] = c;
		if(!(flags & CFLAG_SPACECHAR))
			this->value_end = this->value_length;
	}
}


// add a byte to the 'lowed value' of the current field
void CDOMElement::addLowValueC(char c, unsigned char flags)
{
	if(this->lowValue_length+1 >= this->lowValue_buffer_size)
		this->lowValue = (char *)_REALLOC(this->lowValue, (this->lowValue_buffer_size+=128)) ;

	if(c == '\0')
	{
		// special case : sent by the end of the tag
		if(this->lastFlags & CFLAG_ENDCHAR && this->lowValue_length > 0)
		{
			// last char was a endchar : delete it
			this->lowValue[this->lowValue_length-1] = '\0';
		}
		else
		{
			// last char was ok (or value is empty) : add nul at end
			this->lowValue[this->lowValue_length++] = '\0';
		}
		return;
	}

	if(this->ink==0)
	{
		if(c=='(')
			this->ink = 1;
	}
	else
	{
		if(this->ink==1)
		{
			if(c==')')
				this->ink = 2;
		}
	}

	if(flags & CFLAG_ENDCHAR)
	{
		if(this->lastFlags & CFLAG_ENDCHAR)
		{
			// it's a nth endchar : ignore it
		}
		else
		{
			// it's the first endchar : replace it by space
			if(this->lowValue)
				this->lowValue[this->lowValue_length++] = ' ';
		}
	}
	else
	{
		// normal char
		if(this->ink==0)
		{
			if(this->t0 == -1)
				this->t0 = this->lowValue_length;
			this->t1 = this->lowValue_length;
		}
		else if(this->ink==1)
		{
			if(this->k0 == -1)
				this->k0 = this->lowValue_length;
			this->k1 = this->lowValue_length;
		}
		if(this->lowValue)
			this->lowValue[this->lowValue_length++] = c;
	}

	this->lastFlags = flags;
};





