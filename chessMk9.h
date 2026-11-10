#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
//#include <unistd.h>
#include <time.h>
#include <ctype.h>



// can choose int32_t or int64_t here. int32_t big enough, and runes anywhere
#define Int32 int32_t

#define WHITE (Int32)1
#define BLACK (Int32) - 1
#define NEITHER_COLOUR 0
#define BOTH_COLOURS 2  // watch this as negate wont behave sensibly

// structure to hold a position and evaluation
typedef struct struct_position
{
  Int32 board[8][8];
} _position_t;

typedef _position_t *position_t; // set up a pointer type

// structure to hold a move
typedef struct struct_move
{
  Int32 from[2];
  Int32 to[2];
  Int32 status;
  Int32 evaluation;
} move_t;

typedef struct struct_castlingRights
{
  Int32 whiteKingHasMoved, blackKingHasMoved;
  _position_t EPsquares;
} castlingRights_t;




// maximum number of moves at a given position (ignoring blocking), ..
// ... but can be exceeded (and seg faults) if add 3 or 4 queens in setup!
#define maxPossMoves 122
// furthest to record lines - n.b. seg faults if goes beyond
#define deepestLine 18
// number of 'killer moves' to use
#define numBestMoves 3
// history of positions (max 200)
#define MAXHIST 200



// ////////// position assessment modifiers ///////////
// these affect it's playing style very considerably  between conservingpieces and creating space (with movementWeighting)
// and also value of attacking/defending

// weighting for movement to a square in "milli-Pawns" c.f pawn value 1000 suggest ~10-30
// .. looks a little too keen to sac a pawn for space at 20, so a little lower
#define movementWeighting (Int32)14
//  weighting for defending own piece c.f. 1 square of movement. suggest ~20
#define defendWeighting (Int32)20
// additional weighting for attacking piece c.f. 1 square of movement. suggest ~20
#define attackWeighting (Int32)20

// bonuses for kings behind pawns, and away from centre
#define castlingPositionBonuses 100

// bonus for pressure around opposing king
#define kingPressureBonus 20
// and for protecting around own king
#define kingDefenceBonus 15


// //////// function declarations  ////////
void initialise_chess();                                     // sets up piece values etc
void startingBoard(position_t board);                  // sets up standard starting position
void showBoard1(position_t board1);                    // very basic board display, showing indexe
void showBoard2(position_t board1, Int32 PLAYER); // reasonable board display showing unicode or P,N,B .. etc

Int32 evaluateBoard(position_t board1, move_t movesArray[maxPossMoves], Int32 *colourValPtr, Int32 colour, Int32 NEXTMOVE, move_t lastMoveBefore);                            // evaluates a position for w & b
Int32 fullyEvaluateBoard(position_t startboard, move_t *dummyMoves, Int32 NEXTMOVE, move_t lastMoveBefore);
Int32 LazyEvaluateBoard(position_t board1);
Int32 gradedEvaluateBoard(position_t startboard, move_t *dummyMoves, Int32 NEXTMOVE, move_t lastMoveBefore, Int32 bestalpha);
Int32 getSquaresMoves(position_t board1, position_t defended, position_t attacked, move_t movesArray[maxPossMoves],
        Int32 totalMoves, Int32 ii, Int32 jj, Int32 *weightedMove, move_t lastMoveBefore); // find moves possible from position (ii,jj)
void pieceLocationBonuses(position_t board1, Int32 ii, Int32 jj, Int32 *weightedMove);
Int32 getColoursMoves(position_t board1, position_t defended,position_t attacked, move_t movesArray[maxPossMoves], Int32 *colourValPtr, Int32 COLOUR, move_t lastMoveBefore);
char *move2string(move_t aMove, char *str);                                                               // converts from index form to common e2e4 style
void string2move(char *str, move_t *aMove);                                                               // converts from e2e4 style to indices
uint64_t doMove(position_t board1, move_t inMove, uint64_t inHash);                                                           // implements a move on the board
Int32 getBestMove(Int32 alpha, Int32 beta, position_t board1, move_t *inMove,
  Int32 CURRENTSIDE, Int32 depth, Int32 maxDepth, move_t *bestLine, Int32 Strategy, uint64_t hashval, Int32 *movesCalculated);
Int32 getBestMove_noPrune(position_t board1, move_t *inMove, Int32 CURRENTSIDE, Int32 depth,
                         Int32 maxDepth, move_t *bestLine, uint64_t hashval); // as it says!
Int32 getBestMove_alphaBeta_Ptakes(Int32 alpha, Int32 beta, position_t board1, move_t *inMove, Int32 CURRENTSIDE, Int32 depth,
                           Int32 maxDepth, move_t *bestLine, uint64_t hashval);
Int32 getBestMove_alphaBeta_PhashFirst(Int32 alpha, Int32 beta, position_t board1, move_t *inMove,
                            Int32 CURRENTSIDE, Int32 depth, Int32 maxDepth, move_t *bestLine, uint64_t hashval);
Int32 getBestMove_alphaBeta_timeoutPhashFirst(Int32 alpha, Int32 beta, position_t board1, move_t *inMove,
                              Int32 CURRENTSIDE, Int32 depth, Int32 maxDepth, move_t *bestLine, uint64_t hashval,
                              struct timespec start1, struct timespec end1);
void addKillerMove(move_t inMove,Int32 depth);
void prioritiseMoveOrder_Ptakes( move_t movesArray[maxPossMoves], Int32 moveNum);
void prioritiseMoveOrder_PbestLine(move_t movesArray[maxPossMoves], Int32 moveNum, Int32 depth);
void prioritiseMoveOrder_PbestLineKillers(move_t movesArray[maxPossMoves], Int32 moveNum, Int32 depth);
void prioritiseMoveOrder_PhashFirst(move_t movesArray[maxPossMoves], Int32 moveNum, Int32 depth, Int32 depthOfSearch, uint64_t hashval);
void prioritiseMoveOrder_PhashFirstV2(move_t movesArray[maxPossMoves], Int32 moveNum, Int32 depth, Int32 depthOfSearch, uint64_t hashval);
Int32 evaluateHorizon_playSafe(Int32 alpha, Int32 beta, position_t board1, move_t *inMove,
                            Int32 CURRENTSIDE, Int32 depth, Int32 maxDepth, move_t *bestLine, move_t lastMoveBefore, uint64_t hashval);
Int32 evaluateHorizon_shootOut(Int32 alpha, Int32 beta, position_t board1, move_t *inMove,
                              Int32 CURRENTSIDE, Int32 depth, Int32 maxDepth, move_t *bestLine, move_t lastMoveBefore, uint64_t hashval, struct timespec start1, struct timespec end1);
void copyPosition(position_t boarda, position_t boardb);
Int32 parseCoordinatesString(char *instring, move_t *movesArray, Int32 currentMoves, move_t *inMove, Int32 *messageFlag);
Int32 parseAlgebraicString(position_t board1, char *instring, move_t *movesArray, Int32 currentMoves, move_t *inMove,
                                Int32 *messageFlag, Int32 CURRENTSIDE);
void writeMove(FILE *fptr, position_t board1, move_t inMove, Int32 moveNumber);
void writeMove_withStats(FILE *fptr, position_t board1, move_t inMove, Int32 moveNumber, Int32 alpha, double timeTaken, Int32 depthDone,double avdepth);
void storeMoveHistory(position_t board1,move_t inMove,Int32 moveNumber, uint64_t hashval);
void readMoveHistory(position_t board1,move_t *inMove,Int32 storedMoveNumber);
void index2UnicodePiece(Int32 pieceIndex, char *str, Int32 DrawWhiteOnBlack);
void zeroPosition(position_t pos);
void getKingArea( position_t board1, Int32 COLOUR);
Int32 sign(Int32 x); // returns the sign, +1 or -1
void seeIfEndgame(position_t board1,Int32 CURRENTSIDE);
void countPieceValue(position_t board1,Int32 CURRENTSIDE,Int32 *queenOn, Int32 *pieceValueCount);





// ///////////// ad hoc utility functions ////////////
void printMove(move_t aMove); // utility shows indices in a move_t structure
void shortPrintMove(move_t aMove, position_t board); //  shows indices  very compactly
char *strToLower(char *instring);
Int32 isWithinMoves(move_t *tmpMove, move_t *movesArray, Int32 currentMoves);
void printBestLine(move_t *bestLine, Int32 movesCalculated,Int32 diff_WmB);
void printBestLineAlgebraic(position_t board1, move_t *bestLine, Int32 movesCalculated,Int32 diff_WmB, Int32 CURRENTSIDE);
void printHashesBestLine(position_t board1,move_t inMove, Int32 movesCalculated,Int32 diff_WmB, Int32 CURRENTSIDE, uint64_t hashval);
Int32 onBoard(Int32 i, Int32 j);




// in openings.c
void setUpFirstMoves();
Int32 getBookMove(Int32 moveNumber,move_t *moveHistory, move_t *inMove, char *bookName);
//// declarations ////
void allocateFirstMoves();
void allocateOpenings();
void freeFirstMoves();
Int32 readOpeningsBook();
Int32  runThroughMoves(Int32 wordCount,move_t *newOpening,char algMoves[100][10]);
Int32 isChessNotation(char c);
void freeOpenings();

// in ZobristHashing.c
uint64_t u64rand();
void setupHashTables();
uint64_t hashFullPosition(position_t board);
uint64_t toggleWhiteToPlayHash(uint64_t hashval);
void setupHashLedges();
void freeHashLedges();
void addHashPoint(uint64_t hashval,  Int32 alpha, Int32 depthOfSearch, move_t *inMove);
void clearHashLedges();
Int32 getEqualHashPosition(uint64_t hashval,Int32 depthOfSearch, Int32 *alphaVal, move_t *inMove) ;
Int32 getPreviousBestHashPosition(uint64_t hashval,Int32 depthOfSearch, Int32 *alphaVal, move_t *inMove);
void printHashSearchStats();
void zeroHashStats();
void removeOldHashpoints(Int32 maxKeep);
void HashesToBestline(position_t board1,move_t inMove, move_t *bestLine, Int32 movesCalculated,Int32 diff_WmB,
  Int32 CURRENTSIDE, uint64_t hashval);

// interfaces to Fltk graphics
Int32 tryPlayerMove(FILE *fptr, move_t DisplayMove, Int32 *FLTKCURRENTSIDEptr, Int32 FLTKPLAYER, uint64_t inHash);
Int32 doComputerMove(FILE *FLTKfptr, Int32 *FLTKCURRENTSIDE, Int32 FLTKPLAYER, uint64_t FLTKHASH);
void freeHashesAndOpenings();
void setLimitmode(Int32 in);
void moveToAlgerbraic(move_t inMove, position_t board, Int32 movenum, char *str);
void bestLineToAlgebraicString(position_t board1, move_t *bestLine, Int32 movesCalculated, Int32 diff_WmB, Int32 CURRENTSIDE);















