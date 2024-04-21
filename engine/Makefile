EXE := patricia

SOURCES := src/patricia.cpp

CXX := clang++

CXXFLAGS := -O3 -march=native -std=c++20 -ffast-math

LINKER :=

SUFFIX :=

ifeq ($(OS), Windows_NT)
	DETECTED_OS := Windows
	SUFFIX := .exe
else
	DETECTED_OS := $(shell uname -s)
	LINKER := -lm
	CXXFLAGS += -pthread
endif

ifneq (,$(findstring clang,$(shell $(CXX) --version)))
    ifeq ($(DETECTED_OS), Windows)
        LINKER += -fuse-ld=lld
    endif
endif

OUT := $(EXE)$(SUFFIX)


$(EXE): $(SOURCES)
	$(CXX) $^ $(CXXFLAGS) -o $(OUT) $(LINKER) 
