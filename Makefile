# Makefile for Battleship
# Di Kong
# CSE 271 p10

CC = gcc
FLAGS = -Wall -g


all: battleship

battleship: battleship.c
	$(CC) $(FLAGS) -o battleship battleship.c -lcurses

clean:
	rm -f battleship *~ fifo*

