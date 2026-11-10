
#NTTOBJECTS	=	BasicTransformParameters.o CoreTransformParameters.o Menus.o TestFramework.o Modulus.o ModulusSet.o ModulusSet.o Transforms.o

#include ./makeinclude

COPTS   =  -O2 -Wall -lm -static #-fopenmp -pthread   #(last 2 for openMP and threading)
CPPOPTS	=	 -O2 -std=c++0x -O2  -pthread  # -Wall rather noisy!
LINKOPTS = -lm
DEBUGOPTS = -g
PROFOPTS	= -pg
CC        = gcc
CPP				= g++
EXEC      = chesspresso
## FLTK stuff
EXEEXT	=
CXX		= g++ 
MYFLAGS		= -static
OPTIM		=  -Os -Wall -Wunused -Wno-format-y2k  -fno-exceptions -fno-strict-aliasing -ffunction-sections -fdata-sections
CXXFLAGS	=  -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D_THREAD_SAFE -D_REENTRANT  -I/usr/include/freetype2 -I/usr/include/libpng16   -I/usr/include/freetype2   $(FLTKFLAGS) 
MYINCLUDES	=	-I/home/pete/fltk-1.4.5/
LINKFLTK	= /usr/local/lib/libfltk.a
LINKFLTKGL	= /usr/local/lib/libfltk_gl.a
GLDLIBS		= -lGLU -lGL -lXrender -lXcursor -lXfixes -lXft -lfontconfig -pthread -lpthread -lm  -lX11 -lXinerama -lglut
# *important*: needed -lXinerama added to GLDLIBS 2/6/26 **


# important: with g++ compile *all together* or get very silly errors about *undefined reference to*

CHESS_HEADER = chessMk9

OBJECTS	=	BoardMain.cxx BoardView.o BoardViewUI.o ppmRoutines.o chessMainV9.01.o openings.o ZobristHashing.o

## general is
# target : prequisites ..
# <tab>    recipe

TEMPFLAGS	=	


# just 'make' does the first one

$(EXEC)$(EXEEXT): $(OBJECTS)
	echo Linking $@...
	$(CXX)  $(OPTIM) $(CXXFLAGS) $(MYINCLUDES) -o $(EXEC) \
		$(OBJECTS) $(LINKFLTKGL) $(LINKFLTK) $(GLDLIBS)
BoardMain.o: BoardViewUI.h BoardMain.cxx
	$(CXX) $(CXXFLAGS) $(MYINCLUDES) -c BoardMain.cxx -o BoardMain.o $(COPTS)
BoardView.o: BoardView.h BoardView.cxx
	$(CXX) $(CXXFLAGS) $(MYINCLUDES) -c BoardView.cxx -o BoardView.o $(COPTS)
BoardViewUI.o:	BoardViewUI.cxx BoardViewUI.h
	$(CXX) $(CXXFLAGS) $(MYINCLUDES) -c BoardViewUI.cxx -o BoardViewUI.o $(COPTS)
ppmRoutines.o:	ppmRoutines.cxx ppmRoutines.h
	$(CXX) $(CXXFLAGS) $(MYINCLUDES) -c ppmRoutines.cxx -o ppmRoutines.o $(COPTS)
openings.o	:	openings.c $(CHESS_HEADER).h
	$(CXX) -c openings.c $(MYINCLUDES)  -o openings.o $(COPTS) 
ZobristHashing.o	:	ZobristHashing.c $(CHESS_HEADER).h
	$(CXX) -c ZobristHashing.c $(MYINCLUDES)  -o ZobristHashing.o $(COPTS)  
chessMainV9.01.o	:	chessMainV9.01.c $(CHESS_HEADER).h
	$(CXX) -c chessMainV9.01.c $(MYINCLUDES)  -o chessMainV9.01.o $(COPTS) 
BoardViewUI.cxx:	BoardViewUI.fl /home/pete/fltk-1.4.5/fluid/fluid$(EXEEXT)

# note cant mix gcc object files using g++ for linker


clean :
	rm $(EXEC) *.o
debug :
	$(CXX) $(PROFOPTS) $(CXXFLAGS) $(MYINCLUDES) -o $(EXEC)g BoardMain.cxx BoardView.cxx BoardViewUI.cxx ppmRoutines.cxx chessMainV9.01.c openings.c ZobristHashing.c    \
		 $(LINKFLTKGL) $(LINKFLTK) $(GLDLIBS)

# note: use $(DEBUGOPTS) or else $(PROFOPTS) for profiling
