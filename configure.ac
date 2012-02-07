#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.60])

AC_INIT([phraseanet_indexer], [3.8.0.0])

AM_INIT_AUTOMAKE(no-dependencies no-define no-dist no-installinfo)

AC_OUTPUT(Makefile)

# AC_CONFIG_SRCDIR([simpleopt/SimpleOpt.h])
# AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CXX
# AC_PROG_CC

XTRAINCPATHS="-I/usr/include/libxml2/ -I/usr/include/malloc/"

CFLAGS="$CFLAGS  $XTRAINCPATHS"
CPPFLAGS="$CPPFLAGS $XTRAINCPATHS"
LDFLAGS="${LDFLAGS}"
AM_CXXFLAGS="$AM_CXXFLAGS"


# Checks for MySQL lib
AC_PATH_PROG([MYSQL_CONFIG], [mysql_config], , $PATH/usr/bin:/usr/local/mysql/bin)
if test "x$MYSQL_CONFIG" = "x"
then
	AC_MSG_ERROR([mysql_config program not found])
	exit 3
else
	IBASE=`$MYSQL_CONFIG --include`
	MYSQLCFLAGS=`$MYSQL_CONFIG --cflags`
	CPPFLAGS="${CPPFLAGS} $MYSQLCFLAGS"
	AM_CXXFLAGS="$AM_CXXFLAGS $IBASE"
fi

AC_MSG_NOTICE([Check MySQL libs in default path])
AC_CHECK_LIB([mysqlclient], [my_init], [LIBMYSQL="OK"], [LIBMYSQL="BAD"])

if test "$LIBMYSQL" = "BAD"
then
	AC_MSG_WARN([cannot link to libmysqlclient in default path])

	LBASE=`$MYSQL_CONFIG --libs`
	AC_MSG_NOTICE([Check MySQL libs in $LBASE])
	LDFLAGS="${LDFLAGS} $LBASE"
	AM_CXXFLAGS="$AM_CXXFLAGS $LBASE"
	
	AC_PATH_PROG([OTOOL], [otool], , $PATH/usr/bin:/usr/local/mysql/bin)
	if test "x$OTOOL" != "x"
	then
		AC_MSG_CHECKING([(OSX) Fetch MySQL lib path])
		OSXMYSQLLIBDIR=`echo "$LBASE"|sed 's#.*-L\(\"/[^\"]*\"\).*#\1#;s#.*-L\(/[^[:space:]]*\).*#\1#;'`
		AC_MSG_RESULT([$OSXMYSQLLIBDIR])
		AC_SUBST(OSXMYSQLLIBDIR)	# pass to Makefile.am

		AC_MSG_CHECKING([(OSX) Fetch MySQL lib arch])
		OSXARCH=`echo "$MYSQLCFLAGS"|grep -- "-arch"|sed 's#.*\(-arch [^[:space:]]*\).*#\1#;'`
		AC_MSG_RESULT([$OSXARCH])
		LDFLAGS="${LDFLAGS} $OSXARCH"
	fi
	AC_CHECK_LIB(mysqlclient,mysql_close, , AC_MSG_ERROR([cannot link to libmysqlclient in $LBASE]))
else
	AC_MSG_NOTICE([link OK to libmysqlclient in default path])
	AC_CHECK_LIB(mysqlclient,mysql_close, , AC_MSG_ERROR([cannot link to libmysqlclient in default path]))
fi


# Checks for other libs
AC_CHECK_LIB(xml2,xmlParseDoc,,AC_MSG_ERROR([cannot link to libxml2]))
AC_CHECK_LIB(expat,XML_Parse,,AC_MSG_ERROR([cannot link to libexpat]))
AC_CHECK_LIB(pthread,pthread_create,,AC_MSG_ERROR([cannot link to pthread]))

AC_CHECK_HEADERS([expat.h fcntl.h limits.h locale.h malloc.h memory.h netinet/in.h stdlib.h string.h sys/socket.h sys/timeb.h unistd.h wchar.h], , [AC_MSG_ERROR([missing header])])
AC_CHECK_HEADER([libxml/tree.h],, [AC_MSG_ERROR([cannot find xml headers])])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_SIZE_T

# Checks for library functions.
#AC_FUNC_ERROR_AT_LINE
#AC_FUNC_MALLOC
#AC_FUNC_REALLOC
AC_CHECK_FUNCS([memset select setlocale socket strcasecmp strchr strrchr])


# AC_CONFIG_FILES([simpleopt/Makefile])


AC_OUTPUT

