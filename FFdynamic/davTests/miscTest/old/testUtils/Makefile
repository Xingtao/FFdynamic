# Both Intel / Nvidia Gpu Manager
GCC       ?= g++
LDFLAGS   := -lboost_system -lboost_thread -ldl -lpthread
CCFLAGS   := -m64 -g -O2 -Wall -std=c++0x
INCLUDES  := -I../ -I./

all: build

build: parse.out

stringParse.o: ../stringParse.cpp ../stringParse.h
	$(GCC) $(CCFLAGS) $(INCLUDES) -o $@ -c $<

testStringParse.o: testStringParse.cpp 
	$(GCC) $(CCFLAGS) $(INCLUDES) -o $@ -c $<

parse.out: testStringParse.o stringParse.o
	$(GCC) $(CCFLAGS) -o $@ $+ $(LDFLAGS)

clean:
	rm -f *.o parse.out

recomp : clean all
