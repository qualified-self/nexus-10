
CFLAGS = -O0 -ggdb3
#CFLAGS += -O3
CFLAGS += -Wall -Wextra -Wshadow
#CFLAGS += -Wall -Wextra -fgnu89-inline -Wshadow
CFLAGS += -std=c99 -fPIC

.PHONY: all
all: libedf ant2edf edfsplit edf2ant edf2hdr edfsw edf2rsp edffix

edf.o: edf.c edf.h

LIBEDF_objs = edf.o

.PHONY: libedf
libedf: libedf.so libedf.a
libedf.so: libedf.so.0
	ln -s $^ $@

libedf.so.0: $(LIBEDF_objs)
	$(CC) $(CFLAGS) $(LIBS) $^ -o $@ -shared -Wl,-soname,libedf.so.0 -lm

libedf.a: $(LIBEDF_objs)
	$(AR) rcs $@ $^
	ranlib $@

edf2hdr: edf2hdr.c libedf.a
	$(CC) $(CFLAGS) $^ -o $@ -lm

ant2edf: ant2edf.c libedf.a
	$(CC) $(CFLAGS) $^ -o $@ -lm

edf2ant: edf2ant.c libedf.a
	$(CC) $(CFLAGS) $^ -o $@ -lm

edfsplit: edfsplit.c libedf.a
	$(CC) $(CFLAGS) $^ -o $@ -lm

edf2txt: edf2txt.c libedf.a
	$(CC) $(CFLAGS) $^ -o $@ -lm

edf2rsp: edf2rsp.c libedf.a
	$(CC) $(CFLAGS) $^ -o $@ -lm

edfsw: edfsw.c libedf.a
	$(CC) $(CFLAGS) $^ -o $@ -lm

edffix: edffix.c libedf.a
	$(CC) $(CFLAGS) $^ -o $@ -lm

install: edf2txt edf.h ant2edf edfsplit edf2ant edf2hdr edfsw edf2rsp edffix
ifdef LOCAL_LIBS
	install -d -m755 ../bin
	cp ant2edf ../bin
	cp edf2ant ../bin
	cp edf2txt ../bin
	cp edfsplit ../bin
	cp edf2hdr ../bin
	cp edfsw ../bin
	cp edf2rsp ../bin
	cp edffix ../bin
else
	install -d -m755 /usr/local/bin
	sudo cp ant2edf /usr/local/bin
	sudo cp edf2ant /usr/local/bin
	sudo cp edf2txt /usr/local/bin
	sudo cp edfsplit /usr/local/bin
	sudo cp edf2hdr /usr/local/bin
	sudo cp edf2rsp /usr/local/bin
	sudo cp edffix /usr/local/bin
	sudo cp edfsw /usr/local/bin
	sudo cp edf.h /usr/local/include
	sudo cp libedf.a /usr/local/lib
	sudo cp -d libedf.so* /usr/local/lib
endif
	
.PHONY: clean
clean:
	rm -f edf2txt edfsplit ant2edf edf2ant edf2hdr *.o *.so* libedf.a
