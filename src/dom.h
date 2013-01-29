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

#include "lownodiacritics_utf8.h"

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
#ifndef FALSE
	#define FALSE 0
#endif
#ifndef TRUE
	#define TRUE 0
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
		int pathoffset;
		int upathoffset;

		char *lowValue;
		int lowValue_length;

		char *value;
		int value_length;
		int value_end;			// place for ending \0 (rtrim)

		int index_start;
		int index_end;

		int t0, t1, k0, k1;

		int ink;

		unsigned char lastFlags;

		class CStructField *field;		// ptr to the field in the structure

		CDOMElement(const char *s, class CDOMDocument *owner);
		~CDOMElement();

		void dump(int depth=0);
		// add a byte to the 'value' of the current field
		void addValueC(char c, unsigned char flags);
		// add a byte to the 'lowed value' of the current field
		void addLowValueC(char c, unsigned char flags);

	private:
		int lowValue_buffer_size;
		int value_buffer_size;
};

class CDOMDocument
{
	public:
		CDOMDocument();
		~CDOMDocument();
		CDOMElement *documentElement;
		bool load(char *filename);
		bool loadXML(char *xml, unsigned long len);
		void dump();
		void (*onKeyword)(CDOMDocument *xmlparser, const char *lowKeyword, unsigned int lowKeywordLen, unsigned int pos, unsigned int len, unsigned int idx, const char *lng, unsigned int lngLen);
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
		int path_msize;
		int upath_msize;

		int freepathoffset;
		int freeupathoffset;

		unsigned int indexStart;
		unsigned int indexEnd;
		unsigned int wordIndex;
		char tokBin[400+4];
		int tokBinLen;

		char lowtokBin[400+4];
		int lowtokBinLen;

		void flushToken();
};

#endif

