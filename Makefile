# Makefile for Master and Worker

sa: main.cpp SampleAggregate.h
	g++ -std=c++11 -g -Wall -o sa main.cpp -lpthread

clean:
	rm sa