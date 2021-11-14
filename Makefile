
all:
	gcc -pthread httpserver.c -o httpserver -g -lm -Wall

clean:
	rm -f httpserver httpserver.o
