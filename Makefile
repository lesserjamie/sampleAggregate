# Makefile for Master and Worker

master: master.cpp
	g++ -std=c++11 -g -Wall -o master master.cpp -lpthread

worker: worker.cpp
	g++ -std=c++11 -g -Wall -o worker worker.cpp -lpthread
