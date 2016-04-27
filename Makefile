# Makefile for Master and Worker

all: master.cpp worker.cpp master.h worker.h
	g++ -std=c++11 -g -Wall -o master master.cpp -lpthread
	g++ -std=c++11 -g -Wall -o worker worker.cpp -lpthread

master: master.cpp master.h
	g++ -std=c++11 -g -Wall -o master master.cpp -lpthread

worker: worker.cpp worker.h
	g++ -std=c++11 -g -Wall -o worker worker.cpp -lpthread

clean:
	rm master
	rm worker