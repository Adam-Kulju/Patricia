.PHONY : build datagen

EXE := patricia

SOURCES := src/patricia.cpp src/fathom/src/tbprobe.c

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

build: $(SOURCES)
	$(CXX) $^ $(CXXFLAGS) -o $(OUT) $(LINKER) 

default: build

datagen: datagen/datagen.cpp
	$(CXX) $^ $(CXXFLAGS) -o data $(LINKER) 
OUT := $(EXE)$(SUFFIX)


$(EXE): $(SOURCES)
	$(CXX) $^ $(CXXFLAGS) -o $(OUT) $(LINKER) 
