CC=gcc
LINK=gcc

OPTIONS=-g 

LIBS=-lrt -lm -lwiringPi -lpigpio -lpthread 

COMPILE=$(CC) $(OPTIONS) $(INCLUDES)

PROGRAM=my

SOURCES= \
my.c 

HEADERS= 


OBJS= \
my.o 

all: prebuild $(PROGRAM) $(HEADERS) $(SOURCES) 

prebuild:
	rm -f my.o

$(PROGRAM): $(OBJS) 
	$(LINK) -o $(PROGRAM) $(OBJS) $(LIBS)

.c.o:
	$(COMPILE) -c -o $@ $<


clean:
	-rm -f *.o
	-rm -f $(PROGRAM)

