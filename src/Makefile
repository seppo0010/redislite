all: test cli
dependencies:
	cd ../deps/linenoise && $(MAKE)

redislite-cli.o:
	redislite-cli.c ../deps/linenoise/linenoise.h help.h version.h fmacros.h

cli: dependencies
	$(CC) -Wall -I../deps/linenoise -lm -g -o redislite-cli redislite-cli.c memory.c redislite.c util.c page_index.c page.c\
	 page_string.c page_first.c page_freelist.c public_api.c sds.c ../deps/linenoise/linenoise.o

test:
	$(CC) -Wall -lm -g -o test test.c memory.c redislite.c util.c page_index.c page.c page_string.c page_first.c page_freelist.c\
	 public_api.c sds.c

clean:
	rm -rf test test.dSYM redislite-cli
