#ifndef DOM_INCLUDED
#define DOM_INCLUDED 1

#include <memory.h>
// #include <malloc.h>
#include <string.h>
#include <stdio.h>

#include <expat.h>

#include "trace_memory.h"

// #include <expat_external.h>

#ifdef XML_LARGE_SIZE  // Use large integers for file/stream positions.
#if defined(XML_USE_MSC_EXTENSIONS) && _MSC_VER < 1400
typedef __int64 XML_Index;
typedef unsigned __int64 XML_Size;
#else
typedef long long XML_Index;
typedef unsigned long long XML_Size;
#endif
#else
typedef long XML_Index;
typedef unsigned long XML_Size;
#endif // XML_LARGE_SIZE

//#include <libxml/tree.h>
//#include <libxml/parser.h>
//#include <libxml/xpath.h>
//#include <libxml/xpathInternals.h>

#include "unicode.h"

/*
#ifndef UINT8
	#define UINT8 unsigned char
#endif
#ifndef UINT6
	#define UINT16 unsigned short
#endif
#ifndef UINT32
	#define UINT32 unsigned int
#endif
*/

#ifndef NULL
	#define NULL 0
#endif

class CDOMNode		// abstract class
{
	public:
		enum {XML_UNKNOWN_NODE=0, XML_ELEMENT_NODE=1};
		CDOMNode()
		{
			this->nodeType = CDOMNode::XML_UNKNOWN_NODE;
			this->previousSibling = this->nextSibling = NULL;
		};
		~CDOMNode()
		{
		};
		virtual void dump(int depth=0) = 0;		// pure virtual

		int nodeType;
		class CDOMDocument *ownerDocument;
		class CDOMElement *parentNode;
		class CDOMNode *previousSibling;
		class CDOMNode *nextSibling;
};

class CDOMElement:public CDOMNode
{
	public:
		class CDOMNode *firstChild;
		class CDOMNode *lastChild;
		char *tagName;
		int index;
		size_t pathoffset;
		size_t upathoffset;

		char *valueLC;
		size_t valueLC_length;

		char *valueLCND;		// value lowercase-nodiacritics
		size_t valueLCND_length;

		char *value;
		size_t value_length;
		size_t value_end;			// place for ending \0 (rtrim)

		int index_start;
		int index_end;

		size_t t0, t1;
		size_t k0, k1;

		int ink;				// in-context state (0:before, 1:into, 2:after)

		unsigned char lastFlags;

		class CStructField *field;		// ptr to the field in the structure

		CDOMElement(const char *s, class CDOMDocument *owner);
		~CDOMElement();

		void dump(int depth=0);
		// add a byte to the 'value' of the current field
		void addValueC(char c, unsigned char lastFlags, unsigned char flags);
		// add a byte to the 'lower case value' of the current field
		void addValueLC(char c, unsigned char lastFlags, unsigned char flags);
		// add a byte to the 'lower case value' of the current field
		void addValueLCND(char c, unsigned char lastFlags, unsigned char flags);

	private:
		size_t valueLCND_buffer_size;
		size_t valueLC_buffer_size;
		size_t value_buffer_size;
};

class CDOMDocument
{
	public:
		CDOMDocument();
		~CDOMDocument();
		CDOMElement *documentElement;
		bool load(char *filename);
		bool loadXML(char *xml, size_t len);
		void dump();
		void (*onKeyword)(CDOMDocument *xmlparser, const char *lowKeyword, size_t lowKeywordLen, unsigned int pos, size_t len, unsigned int idx, const char *lng, size_t lngLen);
		void (*onStart)(CDOMDocument *xmlparser, const char *name, const char *path, const char *upath);
		void (*onEnd)(CDOMDocument *xmlparser);
		void *userData;	// to pass/get data to/from callback
		CDOMElement *createElement(const char *name)
		{
			return(new CDOMElement(name, this));
		};
		char *path;
		char *upath;

		bool parseText;
		bool getContent;

		CDOMElement *currentNode;
	private:
		XML_Parser parser;
		static void   start(void *userData, const char *el, const char **attr);
		static void XMLCALL end(void *userData, const char *el);
		static void XMLCALL charHandler(void *userData, const XML_Char *xmls, int len);
		static void XMLCALL startCdata(void *userData);
		static void XMLCALL endCdata(void *userData);

		int depth;
		enum { INTO_UNKNOWN=0, INTO_TEXT, INTO_CDATA };
		int State;

		// malloc size of path and upath
		size_t path_msize;
		size_t upath_msize;

		size_t freepathoffset;
		size_t freeupathoffset;

		unsigned int indexStart;
		unsigned int indexEnd;
		unsigned int wordIndex;

		char token[400+4];
		size_t  tokenLen;

		char tokenLC[400+4];
		size_t  tokenLCLen;

		char tokenLCND[400+4];
		size_t  tokenLCNDLen;

		void flushToken();
};

#endif

