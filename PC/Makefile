CC = gcc
CFLAGS = -Wall -g 
SRCS =  synthetizer.c
HEADERS = $(SRCS:.c=.h)
MAIN =  synthetizer

all:    $(MAIN); chmod u+x $(MAIN)


$(MAIN): $(MAIN).c
	$(CC) $(CFLAGS) -o $(MAIN) $(MAIN).c  -lrt -lm -lasound -pthread



clean:
	$(RM) *.o $(MAIN)



