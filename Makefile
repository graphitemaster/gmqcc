# Compilation options:
# * LTO       - Link time optimization       [default=0]
# * ASAN      - Address sanitizer            [default=0]
# * UBSAN     - Undefined behavior sanitizer [default=0]
# * DEBUG     - Debug build                  [default=0]
# * UNUSED    - Remove unused references     [default=1]
# * SRCDIR    - Out of tree builds           [default=./]
LTO ?= 0
ASAN ?= 0
UBSAN ?= 0
DEBUG ?= 0
UNUSED ?= 1
SRCDIR ?= ./

# Determine if we're building for Windows or not so we can set the right file
# extensions for the binaries and exclude the testsuite because it doesn't build
# for that platform.
ifeq ($(OS),Windows_NT)
	GMQCC := gmqcc.exe
	QCVM := qcvm.exe
else
	GMQCC := gmqcc
	QCVM := qcvm
	TESTSUITE := testsuite
endif

# C++ compiler
CXX ?= clang++

# Build artifact directories
OBJDIR := .build/objs
DEPDIR := .build/deps

# Collect all the source files for GMQCC.
GSRCS := ast.cpp
GSRCS += code.cpp
GSRCS += conout.cpp
GSRCS += fold.cpp
GSRCS += ftepp.cpp
GSRCS += intrin.cpp
GSRCS += ir.cpp
GSRCS += lexer.cpp
GSRCS += main.cpp
GSRCS += opts.cpp
GSRCS += parser.cpp
GSRCS += stat.cpp
GSRCS += utf8.cpp
GSRCS += util.cpp

# Collect all the source files for QCVM.
QSRCS := exec.cpp
QSRCS += stat.cpp
QSRCS += util.cpp

# Collect all the source files for TESTSUITE.
TSRCS := conout.cpp
TSRCS += opts.cpp
TSRCS += stat.cpp
TSRCS += test.cpp
TSRCS += util.cpp

#
# Compilation flags
#
CXXFLAGS := -Wall
CXXFLAGS += -Wextra
CXXFLAGS += -Wno-parentheses
CXXFLAGS += -Wno-class-memaccess
CXXFLAGS += -Wno-implicit-fallthrough
CXXFLAGS += -std=c++11

# Disable some unneeded features.
CXXFLAGS += -fno-exceptions
CXXFLAGS += -fno-rtti
CXXFLAGS += -fno-asynchronous-unwind-tables

# Give each function and data it's own section so the linker can remove unused
# references to each, producing smaller, tighter binaries.
ifeq ($(UNUSED),1)
	CXXFLAGS += -ffunction-sections
	CXXFLAGS += -fdata-sections
endif

# Enable link-time optimizations if requested.
ifeq ($(LTO),1)
	CXXFLAGS += -flto
endif

ifeq ($(DEBUG),1)
	# Ensure there is a frame-pointer in debug builds.
	CXXFLAGS += -fno-omit-frame-pointer

	# Disable all optimizations in debug builds.
	CXXFLAGS += -O0

	# Enable debug symbols.
	CXXFLAGS += -g
else
	# Disable all the stack protection features in release builds.
	CXXFLAGS += -fno-stack-protector
	CXXFLAGS += -fno-stack-check

	# Disable frame pointer in release builds when AddressSanitizer isn't present.
	ifeq ($(ASAN),1)
		CXXFLAGS += -fno-omit-frame-pointer
	else
		CXXFLAGS += -fomit-frame-pointer
	endif

	# Highest optimization flag in release builds.
	CXXFLAGS += -O3
endif

# Sanitizer selection
ifeq ($(ASAN),1)
	CXXFLAGS += -fsanitize=address
endif
ifeq ($(UBSAN),1)
	CXXFLAGS += -fsanitize=undefined
endif

#
# Dependency flags
#
DEPFLAGS := -MMD
DEPFLAGS += -MP

#
# Linker flags
#
LDFLAGS :=

# Remove unreferenced sections
ifeq ($(UNUSED),1)
	LDFLAGS += -Wl,--gc-sections
endif

# Enable link-time optimizations if request.
ifeq ($(LTO),1)
	LDFLAGS += -flto
endif

# Sanitizer selection
ifeq ($(ASAN),1)
	LDFLAGS += -fsanitize=address
endif
ifeq ($(UBSAN),1)
	LDFLAGS += -fsanitize=undefined
endif

# Strip the binaries when not a debug build
ifneq (,$(findstring, -g,$(CXXFLAGS)))
	STRIP := true
else
	STRIP := strip
endif

all: $(GMQCC) $(QCVM) $(TESTSUITE)

# Build artifact directories.
$(DEPDIR):
	@mkdir -p $(DEPDIR)
$(OBJDIR):
	@mkdir -p $(OBJDIR)

$(OBJDIR)/%.o: %.cpp $(DEPDIR)/%.d | $(OBJDIR) $(DEPDIR)
	$(CXX) -MT $@ $(DEPFLAGS) -MF $(DEPDIR)/$*.Td $(CXXFLAGS) -c -o $@ $<
	@mv -f $(DEPDIR)/$*.Td $(DEPDIR)/$*.d

$(GMQCC): $(filter %.o,$(GSRCS:%.cpp=$(OBJDIR)/%.o))
	$(CXX) $^ $(LDFLAGS) -o $@
	$(STRIP) $@

$(QCVM): $(filter %.o,$(QSRCS:%.cpp=$(OBJDIR)/%.o))
	$(CXX) $^ $(LDFLAGS) -o $@
	$(STRIP) $@

$(TESTSUITE): $(filter %.o,$(TSRCS:%.cpp=$(OBJDIR)/%.o))
	$(CXX) $^ $(LDFLAGS) -o $@
	$(STRIP) $@

# Determine if the tests should be run.
RUNTESTS := true
ifdef TESTSUITE
	RUNTESTS := ./$(TESTSUITE)
endif

test: $(QCVM) $(TESTSUITE)
	@$(RUNTESTS)

clean:
	rm -rf $(DEPDIR) $(OBJDIR)

.PHONY: test clean $(DEPDIR) $(OBJDIR)

# Dependencies
$(filter %.d,$(GSRCS:%.cpp=$(DEPDIR)/%.d)):
include $(wildcard $@)

$(filter %.d,$(QSRCS:%.cpp=$(DEPDIR)/%.d)):
include $(wildcard $@)

$(filter %.d,$(TSRCS:%.cpp=$(DEPDIR)/%.d)):
include $(wildcard $@)
