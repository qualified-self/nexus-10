CC  = gcc
CXX = g++
MOC = moc-qt4

# nicer colored error messages from gcc
COLORGCC = $(shell which colorgcc)
ifneq "x$(COLORGCC)" "x"
	CC  = colorgcc
	CXX = colorgcc
endif

# for speed
CFLAGS += -pipe

# nice warnings
CFLAGS += -Wall -Wextra -Wshadow

# optimization flags: optimize for current cpu
ARCH:=$(shell uname -m)
ifneq (,$(findstring arm,$(ARCH)))
  CFLAGS_ARCH := -mcpu=arm1136j-s -mfpu=vfp -mfloat-abi=softfp
else # assume intel
  CFLAGS_ARCH := -march=native
endif
CFLAGS += $(CFLAGS_ARCH)

# in the scratchbox, install everything in ../Nokia_demo.
ifneq (,$(findstring arm,$(ARCH)))
  export LOCAL_LIBS=1
endif


# for debugging: don't optimize, and include debugging info '
#CFLAGS  += -O0
CFLAGS  += -O3
CFLAGS += -ggdb3

# needed to build shared libraries
CFLAGS += -fPIC

# for debugging: add profiling info (SLOW!)
#CFLAGS   += -pg -fprofile-arcs -ftest-coverage
#LDFLAGS  += -pg -fprofile-arcs -ftest-coverage -lgcov

CXXFLAGS := $(CFLAGS)
CFLAGS += -std=c99

# only link the symbols that are actually used
#LIBS += -Wl,--rpath=.
LIBS += -Wl,--as-needed

# special flags for QT
INCLUDE_QT += $(shell pkg-config --cflags QtGui  )
INCLUDE_QT += $(shell pkg-config --cflags QtCore )
LIBS_QT    += $(shell pkg-config --libs   QtGui  )

# default dir to install stuff under

ifdef LOCAL_LIBS
  LIBDIR = ../Nokia_demo/nfb
  INCDIR = ../Nokia_demo/inc
  INCLUDE += -I../Nokia_demo/inc
else
  LIBDIR = /usr/local/lib
  INCDIR = /usr/local/include
endif


PWD := $(shell pwd )

# build the demo application by default
.PHONY: all
all: libbioplot.so

# clean target
.PHONY: clean
clean:
	-rm -f  lib*
	-rm -rf obj moc
	-rm -f  gmon.out *.gcda *.gcno
	-rm -f  tags
	-rm -f  test1

HEADERS = $(wildcard *.h)

.PHONY: install
install: libbioplot.so
	install -m755 -p -d $(LIBDIR)
	install -m755 libbioplot.so libbioplot.so.0 $(LIBDIR)
	install -m755 -p -d $(INCDIR)
	install -m755 $(HEADERS) $(INCDIR)

# general rules for C and C++
obj/%.o: %.c %.h
	-@test -d obj || mkdir obj
	$(CC) $(CFLAGS) $(INCLUDE) -c -o $@ $<

obj/%.o: %.cpp
	-@test -d obj || mkdir obj
	$(CXX) $(CXXFLAGS) $(INCLUDE) $(INCLUDE_QT) -c -o $@ $<

# some QT magic: generate moc_FOO.cpp from FOO.h
moc/moc_%.cpp: %.h 
	-@test -d moc || mkdir moc
	$(MOC) $(INCLUDE) $< -o $@

# build the moc_FOO.cpp files
obj/moc_%.o: moc/moc_%.cpp
	-@test -d obj || mkdir obj
	$(CXX) $(CXXFLAGS) $(INCLUDE) $(INCLUDE_QT) -c -o $@ $<


# automatic dependency generation
obj/%.c.d: %.c
	@test -d obj || mkdir obj
	@test -d obj/moc || mkdir -p obj/moc
	@echo Calulating dependencies of $<
	@$(CC) -M $(INCLUDES) $(INCLUDE_QT) $< \
	  | awk -F: 'FNR==1 {ORS=""; print "obj/"$$1" $@:"; $$1=""; ORS=RS}; {print}' \
	  > $@

obj/%.cpp.d: %.cpp
	@test -d obj || mkdir obj
	@test -d obj/moc || mkdir -p obj/moc
	@echo Calulating dependencies of $<
	@$(CXX) -M $(INCLUDES) $(INCLUDE_QT) $< \
	  | awk -F: 'FNR==1 {ORS=""; print "obj/"$$1" $@:"; $$1=""; ORS=RS}; {print}' \
	  > $@

# ############################################################
# build "libbioplot"
SOURCES_libbioplot += CBioPlot.cpp moc_CBioPlot.cpp
SOURCES_libbioplot += CSpectrumPlot.cpp moc_CSpectrumPlot.cpp
SOURCES_libbioplot += CSpectrumData.cpp
SOURCES_libbioplot += CSquare.cpp moc_CSquare.cpp
SOURCES_libbioplot += CBar.cpp moc_CBar.cpp
SOURCES_libbioplot += CMemButton.cpp moc_CMemButton.cpp
#SOURCES_libbioplot += QwtLogScale.cpp

SOURCES_libbioplot := $(patsubst moc_%, moc/moc_%,  $(SOURCES_libbioplot) )

# dependencies are automatically calculated and stored in obj/*.d
DEPENDENCIES := $(patsubst %,obj/%.d,$(SOURCES_libbioplot))
#DEPENDENCIES := $(patsubst obj/moc/, obj/moc_,  $(DEPENDENCIES) )
-include $(DEPENDENCIES)

OBJS_libbioplot := $(patsubst %.c,       %.o,   $(SOURCES_libbioplot) )
OBJS_libbioplot := $(patsubst %.cpp,     %.o,   $(OBJS_libbioplot) )
OBJS_libbioplot := $(patsubst %,         obj/%, $(OBJS_libbioplot) )
OBJS_libbioplot := $(patsubst obj/moc/%, obj/%, $(OBJS_libbioplot) )

LIBS_libbioplot += -lqwt-qt4 

libbioplot.so: libbioplot.so.0
	ln -fs $^ $@

libbioplot.so.0: $(OBJS_libbioplot)
	$(CXX) $(LDFLAGS) -o $@ -shared -Wl,-soname,$@ $^ $(LIBS) $(LIBS_libbioplot) $(LIBS_QT)



# ###################################################
test1: test/test1.cpp libbioplot.so.0
	gcc -Wall -Wextra -o $@ $^ \
		-l bioplot \
		$(shell pkg-config --libs --cflags QtGui) \
		-L. -I. -L.. -I.. -lboost_thread-mt -Wl,--rpath=$(shell pwd)

