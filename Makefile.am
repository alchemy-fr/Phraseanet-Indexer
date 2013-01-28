ACLOCAL_AMFLAGS = -I m4
EXTRA_DIST = autogen.sh

AUTOMAKE_OPTIONS = subdir-objects
AM_CPPFLAGS = -I$(top_srcdir)/libstemmer_c/include
SUBDIRS = libstemmer_c src

bin_PROGRAMS = phraseanet_indexer
phraseanet_indexer_SOURCES = \
    src/main.cpp \
    src/_version.h \
    src/callback_record.cpp \
    src/connbas.cpp \
    src/connbas_abox.cpp \
    src/connbas_dbox.cpp \
    src/dom.cpp \
    src/indexer.cpp \
    src/lownodiacritics_utf8.cpp \
    src/phrasea_clock_t.cpp \
    src/sbas.cpp \
    src/thesaurus.cpp \
    src/trace_memory.cpp \
    src/sockets.cpp \
    xNIX/syslog_xnix.cpp
phraseanet_indexer_CPPFLAGS = -I$(top_srcdir)/libstemmer_c/include
phraseanet_indexer_LDADD = libstemmer_c/libstemmer.a
