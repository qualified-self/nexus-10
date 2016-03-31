
CC  = gcc
CXX = g++

# optimization flags: optimize for current cpu
ARCH:=$(shell uname -m)
ifneq (,$(findstring arm,$(ARCH)))
  CFLAGS_ARCH := -mcpu=arm1136j-s -mfpu=vfp -mfloat-abi=softfp
else # assume intel
  CFLAGS_ARCH := -march=native
endif
CFLAGS += $(CFLAGS_ARCH) -g
CFLAGS += -fomit-frame-pointer 

# if we're compiling for the nokia, we only use local libraries
ifneq (,$(findstring arm,$(ARCH)))
  export LOCAL_LIBS=1
endif

# if we're compiling for the Linux CPU cluster, we only use local libraries
# AD !!! quick and dirty fix: fix it nicely to detect Linux HTC CPU cluster
#ifneq (,$(findstring x86_64,$(ARCH)))
#  export LOCAL_LIBS=1
#endif

PWD:=$(shell pwd)

CFLAGS   += -fPIC
#CFLAGS   += -ggdb3 -O0
CFLAGS   += -O3
CXXFLAGS += $(CFLAGS)

INCLUDES = -Iinc
LIBS += -Wl,--copy-dt-needed-entries

ifdef LOCAL_LIBS
  INCLUDES += -I$(PWD)/../edf -I$(PWD)/../bioplot/
  LIBS += -Wl,--rpath=$(PWD)/../edf 
  LIBS += -L$(PWD)/../edf -L$(PWD)/../bioplot/

  LIBDIR = $(PWD)/../edf
  INCDIR = $(PWD)/../edf
  BINDIR = $(PWD)/../bin
else
  LIBDIR = /usr/local/lib
  INCDIR = /usr/local/include
  BINDIR = /usr/local/bin
endif

# special flags for QT
INCLUDE_QT += $(shell pkg-config --cflags QtGui  )
INCLUDE_QT += $(shell pkg-config --cflags QtCore )
LIBS_QT    += $(shell pkg-config --libs   QtGui  )
LIBS_QT    += $(shell pkg-config --libs   QtCore )

# dependencies are automatically calculated and stored in obj/*.d
SOURCES       := $(wildcard src/*.c src/*.cpp)
DEPENDENCIESS := $(patsubst src/%,obj/%.d,$(basename $(SOURCES)))
-include $(DEPENDENCIES)

#################################################
#################################################
#################################################

.PHONY: all
all: \
	libtmsi libtmsi_bluez libtmsi_wrapper \
	tmsi_server tmsi_client tmsi_clock \
	single_channel multi_channel \
	tms_cfg tms_rd tms32_rd

.PHONY: install
install: all
	install -m 755 -d $(INCDIR)
	install -m 755 -d $(LIBDIR)
	install -m 755 -d $(BINDIR)
	
	for f in tmsi.h tmsi_bluez.h tmsi_wrapper.h tms_ip.h; do \
		cp inc/$$f $(INCDIR); \
		chmod og+r $(INCDIR)/$$f; \
	done
	for f in tmsi edf tmsi_bluez tmsi_wrapper; do \
		cp lib$${f}.so.0 $(LIBDIR); \
		cp lib$${f}.a    $(LIBDIR); \
		chmod og+r $(LIBDIR)/lib$${f}.so.0; \
		chmod og+r $(LIBDIR)/lib$${f}.a; \
		cp lib$${f}.so.0 $(LIBDIR)/lib$${f}.so; \
	done
	install -m 755 \
		tmsi_server tmsi_client tmsi_clock \
		single_channel multi_channel\
		tms_cfg tms_rd\
		$(BINDIR)


.PHONY: clean
clean:
	for f in tmsi edf tmsi_bluez tmsi_wrapper; do \
		rm -f lib$${f}.so.0; \
		rm -f lib$${f}.so; \
		rm -f lib$${f}.a; \
	done
	rm -f tms_cfg tms_rd tms32_rd 
	rm -f multi_channel single_channel 
	rm -f tmsi_server tmsi_client tmsi_clock
	rm -rf obj


#################################################
#################################################
#################################################


obj/%.o: src/%.c
	test -d obj || mkdir obj
	$(CC) $(CFLAGS) $(INCLUDES) $(INCLUDE_QT) -c $< -o $@

obj/%.o: src/%.cpp
	test -d obj || mkdir obj
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(INCLUDE_QT) -c $< -o $@

# automatic dependency generation
obj/%.d: src/%.c
	@test -d obj || mkdir obj
	@echo Calulating dependencies of $<
	@$(CC) -M $(INCLUDES) $(INCLUDE_QT) $< \
	  | awk -F: 'FNR==1 {ORS=""; print "obj/"$$1" $@:"; $$1=""; ORS=RS}; {print}' \
	  > $@

obj/%.d: src/%.cpp
	@test -d obj || mkdir obj
	@echo Calulating dependencies of $<
	@$(CXX) -M $(INCLUDES) $(INCLUDE_QT) $< \
	  | awk -F: 'FNR==1 {ORS=""; print "obj/"$$1" $@:"; $$1=""; ORS=RS}; {print}' \
	  > $@

obj:
	test -d obj || mkdir obj

#################################################

LIBTMSI_objs = obj/tmsi.o

.PHONY: libtmsi
libtmsi: libtmsi.so libtmsi.a
libtmsi.so: libtmsi.so.0
	ln -s $^ $@

libtmsi.so.0: $(LIBTMSI_objs)
	$(CC) $(CFLAGS) $(LIBS) $^ -o $@ -shared -Wl,-soname,libtmsi.so.0 -lm

libtmsi.a: $(LIBTMSI_objs)
	$(AR) rcs $@ $^


#################################################

LIBTMSI_BLUEZ_objs = obj/tmsi_bluez.o

.PHONY: libtmsi_bluez
libtmsi_bluez: libtmsi_bluez.so libtmsi_bluez.a
libtmsi_bluez.so: libtmsi_bluez.so.0
	cp $^ $@

libtmsi_bluez.so.0: $(LIBTMSI_BLUEZ_objs)
	$(CC) $(CFLAGS) $(LIBS) $^ -o $@ -shared -Wl,-soname,libtmsi_bluez.so.0 \
	  -L. -lm -lbluetooth -ltmsi

libtmsi_bluez.a: $(LIBTMSI_BLUEZ_objs)
	$(AR) rcs $@ $^


#################################################

LIBTMSI_WRAPPER_objs = obj/tmsi_wrapper.o obj/Exception.o obj/Server.o obj/Client.o obj/tms_ip.o

.PHONY: libtmsi_wrapper
libtmsi_wrapper: libtmsi_wrapper.so libtmsi_wrapper.a
libtmsi_wrapper.so: libtmsi_wrapper.so.0
	cp $^ $@

libtmsi_wrapper.so.0: $(LIBTMSI_WRAPPER_objs)
	$(CXX) $(CFLAGS) $(LIBS) $^ -o $@ -shared -Wl,-soname,libtmsi_wrapper.so.0 \
	  -L. -lm -ltmsi -ltmsi_bluez -ledf

libtmsi_wrapper.a: $(LIBTMSI_WRAPPER_objs)
	$(AR) rcs $@ $^


#################################################

TMSI_SERVER_objs = obj/Exception.o obj/Server.o obj/tmsi_server.o obj/RunningAverage.o
tmsi_server: $(TMSI_SERVER_objs)
	$(CXX) $(CXXFLAGS) $(LIBS)  $^ -o $@ -rdynamic -L. \
	  -lpthread -lstdc++ -ltmsi -ltmsi_bluez -ltmsi_wrapper -lm -ledf

#################################################

TMSI_CLIENT_objs = obj/Exception.o obj/Client.o obj/tmsi_client.o
tmsi_client: $(TMSI_CLIENT_objs)
	$(CXX) $(CXXFLAGS) $^ -o $@ -rdynamic -L. $(LIBS) \
	  -lstdc++ -lpthread -lbluetooth -ltmsi -ltmsi_bluez -ltmsi_wrapper -lm -lbioplot -lboost_thread

#################################################

single_channel: obj/single_channel.o
	$(CXX) $(CXXFLAGS) $^ -o $@ -rdynamic -L. $(LIBS) \
	  -lpthread -lbluetooth -ltmsi -ltmsi_bluez -ltmsi_wrapper -lm

#################################################

tmsi_clock: obj/tmsi_clock.o
	$(CXX) $(CXXFLAGS) $^ -o $@ -rdynamic -L. $(LIBS) \
	  -lbluetooth -ltmsi -ltmsi_bluez -lm

#################################################

multi_channel: obj/multi_channel.o
	$(CXX) $(CXXFLAGS) $^ -o $@ -rdynamic -L. $(LIBS) \
	  -lpthread -lbluetooth -ltmsi -ltmsi_bluez -ltmsi_wrapper -lm -ledf

#################################################

tms_cfg: obj/tms_cfg.o
	$(CXX) $(CXXFLAGS) $^ -o $@ -rdynamic -L. $(LIBS) -ltmsi

#################################################

tms_rd: obj/tms_rd.o
	$(CXX) $(CXXFLAGS) $^ -o $@ -rdynamic -L. $(LIBS) -ltmsi

#################################################

tms32_rd: obj/tms32_rd.o
	$(CXX) $(CXXFLAGS) $^ -o $@ -rdynamic -L. $(LIBS) -ltmsi

