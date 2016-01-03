UNAME ?= $(shell uname)
CYGWIN = $(findstring CYGWIN, $(UNAME))
MINGW = $(findstring MINGW,  $(UNAME))

ifneq ("$(CYGWIN)", "")
WINDOWS=1
endif
ifneq ("$(MINGW)", "")
WINDOWS=1
endif

CXX ?= clang++
CXXFLAGS = \
	-std=c++11 \
	-Wall \
	-Wextra \
	-fno-exceptions \
	-fno-rtti \
	-MD \
	-g3

CSRCS = \
	ast.cpp \
	code.cpp \
	conout.cpp \
	fold.cpp \
	ftepp.cpp \
	intrin.cpp \
	ir.cpp \
	lexer.cpp \
	main.cpp \
	opts.cpp \
	parser.cpp \
	stat.cpp \
	utf8.cpp \
	util.cpp

TSRCS = \
	conout.cpp \
	opts.cpp \
	stat.cpp \
	test.cpp \
	util.cpp

VSRCS = \
	exec.cpp \
	stat.cpp \
	util.cpp

COBJS = $(CSRCS:.cpp=.o)
TOBJS = $(TSRCS:.cpp=.o)
VOBJS = $(VSRCS:.cpp=.o)

CDEPS = $(CSRCS:.cpp=.d)
TDEPS = $(TSRCS:.cpp=.d)
VDEPS = $(VSRCS:.cpp=.d)

ifndef WINDOWS
CBIN = gmqcc
VBIN = qcvm
TBIN = testsuite
else
CBIN = gmqcc.exe
CVIN = qcvm.exe
endif

ifndef WINDOWS
all: $(CBIN) $(QCVM) $(TBIN)
else
all: $(CBIN) $(QCVM)
endif

$(CBIN): $(COBJS)
	$(CXX) $(COBJS) -o $@

$(VBIN): $(VOBJS)
	$(CXX) $(VOBJS) -o $@

ifndef WINDOWS
$(TBIN): $(TOBJS)
	$(CXX) $(TOBJS) -o $@

test: $(TBIN)
	@./$(TBIN)
endif

.cpp.o:
	$(CXX) -c $(CXXFLAGS) $< -o $@

clean:
	rm -f *.d
	rm -f $(COBJS) $(CDEPS) $(CBIN)
	rm -f $(VOBJS) $(VDEPS) $(VBIN)
ifndef WINDOWS
	rm -f $(TOBJS) $(TDEPS) $(TOBJS)
endif

-include *.d
