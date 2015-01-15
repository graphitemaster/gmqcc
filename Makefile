CXX ?= clang++
CXXFLAGS = \
	-std=c++11 \
	-Wall \
	-Wextra \
	-ffast-math \
	-fno-exceptions \
	-fno-rtti \
	-MD

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

CBIN = gmqcc
TBIN = testsuite
VBIN = qcvm

all: $(CBIN) $(TBIN) $(VBIN)

$(CBIN): $(COBJS)
	$(CXX) $(COBJS) -o $@

$(TBIN): $(TOBJS)
	$(CXX) $(TOBJS) -o $@

$(VBIN): $(VOBJS)
	$(CXX) $(VOBJS) -o $@

.cpp.o:
	$(CXX) -c $(CXXFLAGS) $< -o $@

test: $(CBIN) $(TBIN) $(VBIN)
	@./$(TBIN)

clean:
	rm -f *.d
	rm -f $(COBJS) $(CDEPS) $(CBIN)
	rm -f $(TOBJS) $(TDEPS) $(TBIN)
	rm -f $(VOBJS) $(VDEPS) $(VBIN)

-include *.d
