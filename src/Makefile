release_hdr := $(shell sh -c './mkreleasehdr.sh')

all: test cli
dependencies:
	cd ../deps/linenoise && $(MAKE)

.c.o:
	$(CC) $(ARCH) -c -Wall -W -O0 -g $<

redislite-cli.o:
	$(CC) $(ARCH) -c -Wall -W -O0 -g -I../deps/linenoise redislite-cli.c

libredislite-no-sds.a: memory.o core.o redislite.o util.o page_index.o page.o page_string.o page_first.o page_freelist.o page_list.o page_set.o public_api.o release.o
	ar -cq libredislite-no-sds.a memory.o core.o redislite.o util.o page_index.o page_set.o page.o\
	 page_string.o page_first.o page_freelist.o page_list.o public_api.o release.o

libredislite.a: memory.o core.o redislite.o util.o page_index.o page.o page_string.o page_first.o page_freelist.o page_list.o page_set.o public_api.o sds.o release.o
	ar -cq libredislite.a memory.o core.o redislite.o util.o page_index.o page.o\
	 page_string.o page_first.o page_freelist.o page_list.o page_set.o public_api.o release.o sds.o

cli: dependencies redislite-cli.o libredislite.a
	$(CC) -Wall -lm -O0 -g -o redislite-cli redislite-cli.o libredislite.a ../deps/linenoise/linenoise.o

test: libredislite.a
	$(CC) -Wall -lm -O0 -g -o test test.c libredislite.a

clean:
	rm -rf test test.dSYM redislite-cli *.o *.a redislite-cli.dSYM
