# Noiz2sa makefile(MinGW 2.0.0)
# $Id: Makefile,v 1.4 2003/08/10 09:00:05 kenta Exp $

NAME   = noiz2sa
RM     = rm -f
CC     = gcc
CXX    = g++

PROG        = $(NAME)

DEFAULT_CFLAGS = `sdl2-config --cflags`
LDFLAGS        = `sdl2-config --libs` -L. -lSDL2_mixer -lstdc++

MORE_CFLAGS = -O3

CFLAGS  = $(DEFAULT_CFLAGS) $(MORE_CFLAGS)
CPPFLAGS  = $(DEFAULT_CFLAGS) $(MORE_CFLAGS) -I./bulletml/

OBJS =	$(NAME).o ship.o shot.o frag.o bonus.o \
	foe.o foecommand.o barragemanager.o attractmanager.o \
	background.o letterrender.o \
	screen.o clrtbl.o vector.o degutil.o rand.o soundmanager.o \
	bulletml/bulletmlparser-tinyxml.o \
	bulletml/bulletmlparser.o \
	bulletml/bulletmltree.o \
	bulletml/calc.o \
	bulletml/formula-variables.o \
	bulletml/bulletmlrunner.o \
	bulletml/bulletmlrunnerimpl.o \
	bulletml/tinyxml/tinyxml.cpp \
	bulletml/tinyxml/tinyxmlparser.cpp \
	bulletml/tinyxml/tinyxmlerror.cpp

play: $(PROG)
	cd .. && ./$(PROG) -window

$(PROG): $(OBJS) 
	$(CC) $(CFLAGS) -o $(PROG) $(OBJS) $(LDFLAGS)
	mv $(PROG) ../

clean:
	$(RM) $(PROG) *.o
