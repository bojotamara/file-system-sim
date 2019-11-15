CC      = g++
CFLAGS  = -std=c++11 -Wall -O2
SOURCES = $(wildcard *.cc)
OBJECTS = $(SOURCES:%.cc=%.o)

all: fs

fs: $(OBJECTS)
	$(CC) -o fs $(OBJECTS)

compile: $(OBJECTS)

%.o: %.cc
	${CC} ${CFLAGS} -c $^

clean:
	@rm -f *.o fs

compress:
	zip fs-sim.zip README.md Makefile *.cc *.h

