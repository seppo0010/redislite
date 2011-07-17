all: test cli
dependencies:
	cd ../deps/linenoise && $(MAKE)

.c.o:
	$(CC) $(ARCH) -c -Wall -W -O0 -g $<

redislite-cli.o:
	$(CC) $(ARCH) -c -Wall -W -O0 -g -I../deps/linenoise redislite-cli.c

libredislite.a: memory.o core.o redislite.o util.o page_index.o page.o page_string.o page_first.o page_freelist.o page_list.o public_api.o sds.o
	ar -cq libredislite.a memory.o core.o redislite.o util.o page_index.o page.o\
	 page_string.o page_first.o page_freelist.o page_list.o public_api.o sds.o

cli: dependencies redislite-cli.o libredislite.a
	$(CC) -Wall -lm -O0 -g -o redislite-cli redislite-cli.o libredislite.a ../deps/linenoise/linenoise.o

test: libredislite.a
	$(CC) -Wall -lm -O0 -g -o test test.c libredislite.a

clean:
	rm -rf test test.dSYM redislite-cli *.o *.a
