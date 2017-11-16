

DUKTAPE_ROOT =		deps/duktape
PYTHON =		python2.7

DUKTAPE_DIR =		duk

CLOBBER_FILES +=	$(DUKTAPE_DIR)

CC =			gcc
CFLAGS =		-gdwarf-2 -std=c99 -D__EXTENSIONS__ \
			-Wall -Wextra -Werror \
			-Wno-unused-parameter \
			-I./duk
LIBS =			-lm -lnvpair -L/usr/lib/fm -R/usr/lib/fm -lfmd_msg

PROG =			nvtool


all: $(PROG)

$(DUKTAPE_DIR)/duktape.c:
	rm -rf $(@D)
	@echo -------- CONFIGURING DUKTAPE -----------------
	$(PYTHON) $(DUKTAPE_ROOT)/tools/configure.py \
	    --define=DUK_USE_FATAL_HANDLER=fatal_handler \
	    --output-directory=$(@D)
	/usr/bin/sed -i '' -e 's/ast.endian/endian/' \
	    $(DUKTAPE_DIR)/duk_config.h

CLEAN_FILES +=		duktape.o
duktape.o: $(DUKTAPE_DIR)/duktape.c
	$(CC) -o $@ -c $(CFLAGS) -Wno-implicit-function-declaration $^

CLEAN_FILES +=		nvtool
nvtool: nvtool.c duktape.o
	$(CC) -o $@ $(CFLAGS) $^ $(LIBS)


clean:
	rm -rf $(CLEAN_FILES)

clobber: clean
	rm -rf $(CLOBBER_FILES)

