release_hdr := $(shell sh -c './mkreleasehdr.sh')

# Copyright (C) 2009 Salvatore Sanfilippo <antirez at gmail dot com>
# This file is released under the BSD license, see the COPYING file

release_hdr := $(shell sh -c './mkreleasehdr.sh')
uname_S := $(shell sh -c 'uname -s 2>/dev/null || echo not')
OPTIMIZATION?=-O2

ifeq ($(uname_S),SunOS)
  # make isinf() available
  CFLAGS?=-std=c99 -pedantic $(OPTIMIZATION) -Wall -W -D__EXTENSIONS__ -D_XPG6
  DEBUG?=-g -ggdb 
else
  CFLAGS?=-std=c99 -pedantic $(OPTIMIZATION) -Wall -W $(ARCH) $(PROF)
  DEBUG?=-g -rdynamic -ggdb 
endif

all: cli
dependencies:
	cd ../deps/linenoise && $(MAKE)

.c.o:
	$(CC) $(ARCH) $(DEBUG) $(CFLAGS) -c $<

redislite-cli.o:
	$(CC) $(ARCH) $(DEBUG) $(CFLAGS) -c -I../deps/linenoise redislite-cli.c

libredislite-no-sds.a: memory.o core.o redislite.o util.o page_index.o page.o page_string.o page_first.o page_freelist.o page_list.o page_set.o public_api.o release.o
	ar -cq libredislite-no-sds.a memory.o core.o redislite.o util.o page_index.o page_set.o page.o\
	 page_string.o page_first.o page_freelist.o page_list.o public_api.o release.o

libredislite.a: memory.o core.o redislite.o util.o page_index.o page.o page_string.o page_first.o page_freelist.o page_list.o page_set.o public_api.o sds.o release.o
	ar -cq libredislite.a memory.o core.o redislite.o util.o page_index.o page.o\
	 page_string.o page_first.o page_freelist.o page_list.o page_set.o public_api.o release.o sds.o

cli: dependencies redislite-cli.o libredislite.a
	$(CC) $(DEBUG) $(CFLAGS) -lm -o redislite-cli redislite-cli.o libredislite.a ../deps/linenoise/linenoise.o

clean:
	rm -rf redislite-cli *.o *.a redislite-cli.dSYM
