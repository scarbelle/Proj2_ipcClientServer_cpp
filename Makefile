#
# CSE 691 Adv Systems Programming
# Homework #7
#
# Scarlett J. Davidson
#
# Makefile to build main.c
#
#
PROG=myproj2
OPTIONS= -lrt -pthread

$(PROG):  $(PROG).c 
	gcc -o $(PROG) $(PROG).c $(OPTIONS)

clean:
	rm $(PROG) $(PROG).o

test:
	./$(PROG)

