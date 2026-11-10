#include "chessMk9.h"
#include "BoardViewUI.h"

#include <FL/Fl_File_Chooser.H>




// //////   global variables   //////

move_t levelsBestMoves[deepestLine][numBestMoves];
// points around king to award higher for attacks
_position_t whiteKingArea, blackKingArea, nullKingArea, workingKingArea;
castlingRights_t castlingRights;
// final move choice and evaluations
move_t evaluatedMoves[maxPossMoves];
Int32 evaluatedMoveNum;

_position_t positionHistory[MAXHIST];
move_t moveHistory[MAXHIST];
uint64_t hashHistory[MAXHIST];
castlingRights_t castlingHistory[MAXHIST];
Int32 takebacks = 0;
Int32 moveNumber = 1;
Int32 pawnFiles[8];
// set up move-steps for *knights*
Int32 xknightSteps[8] = {-2, -2, -1, -1, 1, 1, 2, 2};
Int32 yknightSteps[8] = {-1, +1, -2, +2, -2, +2, -1, +1};

#define HorizonSafeMove 1
#define HorizonShootOut -1
Int32 horizonMethod = HorizonSafeMove;
//Int32 horizonMethod = HorizonShootOut;

// indicator of time out reached
Int32 TimeOut = 0;
#define TIMEOUTFLAG 999999999
double timeoutTimeAllMoves = 10000.; // huge number turns timeout off for the first timedmove

#define MAINGAME 0
#define EARLYENDGAME 1
#define FULLENDGAME 2

Int32 gameStage = MAINGAME; // MAINGAME implies opening or middle game

// hold symbols for pieces (1-6) and their material values
char pieces[8];
Int32 values[8];
move_t lastMove;
Int32 maxShootOutDepth, avShootOutDepth, numShootOuts;
Int32 doUnicodeOrRomanPieces = 1; // 1, or 0 for just P,N,B etc

// unicode pieces
// char *unicode_pieces[] = {".", "♙", "♘", "♗", "♖", "♕", "♔", "♟︎", "♞", "♝", "♜", "♛", "♚"};
Int32 DrawWhiteOnBlack = 1; // change from 1 on a white background for appropriate colouring

// weights for knight positions and pawn positions
_position_t knightWeights, pawnWeights,endgameKing;

// a deliberately invalid move (-1,-1) to (-1,-1)
move_t nullMove;
_position_t emptyBoard; // a board filled with zeroes, for initialising new boards

// externals (opening.c)
extern Int32 maxFirstMoves;

// externals (ZobristHashing.c)
extern uint64_t pieceHash[13][8][8];
extern uint64_t enPassantHash[8][8];
extern uint64_t WhiteCantCastle[2];
extern uint64_t BlackCantCastle[2];
extern int64_t exactHashSearches;
extern int64_t exactHashHits;
extern int64_t orderingHashSearches;
extern int64_t orderingHashHits;

extern Int32 doHashing;
extern Int32 pauseHashing;

extern _position_t DisplayBoard;
extern move_t lastMoveBefore;
extern Int32 FLTKbookOpeningsOn;
extern Int32 FLTKmaxDepth;
extern Int32 FLTKstrategy;
extern Int32 FLTKPLAYER;
extern Int32 FLTKCURRENTSIDE;
extern FILE *FLTKfptr;
extern BoardViewUI *FLTKbrdui;
extern double FLTKavgMoveTime;  // for x SECOND average with TARGETMAXTIME
extern double FLTKallMovesTime; // for x MINUTE limit for with TOTALTIME; aims to use 80% in 36 moves
Int32 FLTKlastDepth = 0;
Int32 FLTKassessment = 0;           // from diff_WmB
char FLTKlastMoveStr[40];           // string containing last move, for display
char FLTKbestlineStr[500] = {'\0'}; // and the latest 'best line'

Int32 FLTK3RepeatsFlag = 0;  // 1 if 3 repeats detected
Int32 FLTKcheckmateFlag = 0; // +1 if 1-0, and -1 if 0-1

// operating limits for the computer (strategy 5 only)
#define FIXEDDEPTH 0
#define TARGETMAXTIME 1
#define ALLMOVESTIME 2
Int32 LIMITMODE = TARGETMAXTIME;

struct timespec start, end; // for timings
time_t dateAndTimeStr;
Int32 verbose = 0; // 2 for full debug, 1 for more info, 0 for quieter

// temp dummy variable
uint64_t DUMMYhash = 0;

// timing
double totalCompTime = 0.;
Int32 totalCompMoves = 0;
Int32 totalCompDepths = 0;
double totalHumanTime = 0.;
Int32 totalHumanMoves = 0;

// normal mouse mode (0) or setup mode (1) - used in handle
Int32 setupMouseMode = 0;

// for counting nodes don, and for timing trigger
int64_t nodeCount = 0;
int64_t timingNodeCount = 0;

// setting values from fltk interface
void setLimitmode(Int32 in) { LIMITMODE = in; }
void setStrategy(Int32 in) { FLTKstrategy = in; } // not actively used unless to try strategy 1 (brute orce)
void setOpeningsOff()
{
  FLTKbookOpeningsOn = 0; // switches off use of openings book
}


Int32 tmpstoremove, tmpstoreplayer, tmpstorecurrentside;
Int32 tmpstoretotalCompMoves,tmpstoretotalHumanMoves;
double tmpstoretotalCompTime, tmpstoretotalHumanTime;
castlingRights_t tmpstorecastling;
_position_t tmpstoreboard;

// for setup position - store for any cancellation
void setupGameInit()
{
  // first store basics allowing a clean cancellation
  tmpstoremove = moveNumber;
  tmpstoreplayer =FLTKPLAYER;
  tmpstorecastling=castlingRights;
  tmpstoreboard = DisplayBoard;
  tmpstorecurrentside = FLTKCURRENTSIDE;
  tmpstoretotalCompMoves =totalCompMoves;
  tmpstoretotalCompTime =totalCompTime;
  tmpstoretotalHumanMoves = totalHumanMoves;
  tmpstoretotalHumanTime = totalHumanTime;
}
void setMouseMode (int32_t val)
{
  setupMouseMode = val;
}
// for setup position - go to original values if a cancellation
void cancelSetup()
{
  moveNumber= tmpstoremove;
  FLTKPLAYER = tmpstoreplayer;
  castlingRights = tmpstorecastling;
  DisplayBoard = tmpstoreboard;
  FLTKCURRENTSIDE = tmpstorecurrentside;
  totalCompMoves=tmpstoretotalCompMoves;
  totalCompTime=tmpstoretotalCompTime;
  totalHumanMoves=tmpstoretotalHumanMoves;
  totalHumanTime=tmpstoretotalHumanTime;
}
void resetAfterSetup(void)
{
  totalCompMoves=0.;
  totalCompTime=0.;
  totalHumanMoves=0.;
  totalHumanTime=0.;
  nodeCount =0; // needed to keep kilo-nodes/sec right
}

// for setup position - set the next side to move
void setNextPlayer(uint32_t nextSide)
{
  printf("\n next to play is %d",nextSide);
  if(nextSide ==1){
    FLTKCURRENTSIDE = 1;
    moveNumber =1; // movenumber to white/black consistent is critical as used in some places
    FLTKPLAYER = 2;//-1;
    FLTKbookOpeningsOn=0;
    
  }
  else {
    FLTKCURRENTSIDE = -1;
    moveNumber =1;
    FLTKPLAYER = 2;//1;
    FLTKbookOpeningsOn=0;
  }
  // set moves 1 & 2 history to new position to allow takebacks
  storeMoveHistory(&DisplayBoard, nullMove, 1, DUMMYhash);
  storeMoveHistory(&DisplayBoard, nullMove, 2, DUMMYhash);

}


// resets all to new game
void ResetGame()
{
  startingBoard(&DisplayBoard);
  moveNumber = 1;
  FLTKbookOpeningsOn = 1;
  FLTKPLAYER = WHITE;
  totalCompTime = 0.;
  totalCompMoves = 0;
  totalCompDepths = 0;
  totalHumanTime = 0.;
  totalHumanMoves = 0;

  // initialise castling right
  castlingRights.whiteKingHasMoved = 0;
  castlingRights.blackKingHasMoved = 0;

  // zero EP (en passant) squres
  for (Int32 ii = 0; ii < 8; ++ii)
    for (Int32 jj = 0; jj < 8; ++jj)
      castlingRights.EPsquares.board[ii][jj] = 0;
}

// fiddly routine o get to nearby 1,2,5 or 10
double roundTo1_2_5or10(double tIn)
{
  Int32 pof10 = 0;
  while (tIn > 0.01)
  {
    tIn /= 10.0;
    ++pof10;
  }
  // is now a number between 0.001 and 0.01 - get it in a 1,2,5,10 form

  if (tIn >= (0.01 - 0.0015)) // half way..
    tIn = 0.01;
  if (tIn <= (0.01 - 0.0015) && tIn >= (0.007 - 0.001))
    tIn = 0.007;
  if (tIn <= (0.007 - 0.001) && tIn >= (0.005 - 0.001))
    tIn = 0.005;
  if (tIn < (0.005 - 0.001) && tIn >= (0.003 - 0.001))
    tIn = 0.003;
  if (tIn < (0.003 - 0.0005) && tIn >= (0.002 - 0.0005))
    tIn = 0.002;
  if (tIn <= (0.002 - 0.0005))
    tIn = 0.001;

  // multiply back up
  for (Int32 ii = 0; ii < pof10; ++ii)
    tIn *= 10.0;
  return (tIn);
}

void setLevel(Int32 sign)
{
  // +ve signe to increase, -ve to decrease
  if (LIMITMODE == FIXEDDEPTH) // just increase or decrease
  {
    if (sign >= 0)
      ++FLTKmaxDepth;
    else
      --FLTKmaxDepth;

    if (FLTKmaxDepth < 3) // play is plain rubbish with less than 3
      FLTKmaxDepth = 3;
    if (FLTKmaxDepth > (deepestLine - 1))
      FLTKmaxDepth = deepestLine - 1;
    return;
  }

  if (LIMITMODE == TARGETMAXTIME) // more complicated go up or down to next of 1,2,3,5,7,10...
  {
    static int shownMsg = 0;
    if (sign >= 0)
      FLTKavgMoveTime *= 1.6;
    else
    {
      FLTKavgMoveTime /= 1.6;
      if (shownMsg == 0 && FLTKavgMoveTime < 0.03)
      {
        fl_message(" minimum search depth of 3 moves may exceed short time limits");
        shownMsg = 1; // stop it showing again
      }
    }

    if (FLTKavgMoveTime < 0.001) // limit to this
      FLTKavgMoveTime = 0.001;

    FLTKavgMoveTime = roundTo1_2_5or10(FLTKavgMoveTime); // rounds it here
  }

  if (LIMITMODE == ALLMOVESTIME) // more complicated go up or down to next of 1,2,5,10...
  {
    if (sign >= 0)
      FLTKallMovesTime *= 1.6;
    else
      FLTKallMovesTime /= 1.6;

    if (FLTKallMovesTime < 0.01)
      FLTKallMovesTime = 0.01;

    FLTKallMovesTime = roundTo1_2_5or10(FLTKallMovesTime); // rounds it here
  }

  // now do the rounding to 1,2,5,10, ...

} // end set limit

double getValToShow(int *dOrf) // for the tLimit_to widget
{
  *dOrf = -1; // flag for a integer
  if (LIMITMODE == FIXEDDEPTH)
    return ((double)FLTKmaxDepth);

  *dOrf = 1; // flag for a double now
  if (LIMITMODE == TARGETMAXTIME)
    return (FLTKavgMoveTime);

  if (LIMITMODE == ALLMOVESTIME)
    return (FLTKallMovesTime);

  return (0.); // to stop compiler warnings
}

void setfltkplayer(Int32 player)
{
  FLTKPLAYER = player;
}

void TakeBackMove()
{

  //if (moveNumber < 3)
  //  return; // no go for < 3
//printf("\n move number was %d side: %d*****",moveNumber,FLTKCURRENTSIDE);
  ++takebacks;
  if(moveNumber >=3)
  {
  moveNumber -= 2;
  }
  else{ // particularly for takebacks after setup
    if(moveNumber>=2)// stop core dumps if 1
       moveNumber -=1;
    FLTKCURRENTSIDE = -FLTKCURRENTSIDE;
  }

  // get back the position and the correct last move (for e.p. and whatever)
  printf("\n last move before");
  printMove(lastMove);
  readMoveHistory(&DisplayBoard, &lastMove, moveNumber);
  printf("\n last move after");
  printMove(lastMove); // seems to get last whites move...

  // unanticipated: need board from before move for algebraic string else a moved bishop (eg) is found to be a '.' (dot) as absent
  _position_t tempBoard;
  move_t tempMove;
  readMoveHistory(&tempBoard, &tempMove, moveNumber - 1);

  moveToAlgerbraic(lastMove, &tempBoard, moveNumber, FLTKlastMoveStr);
  FLTKbrdui->lastmove_to->value(FLTKlastMoveStr);
}

// Int32 LIMITMODE = ALLMOVESTIME;

Int32 doComputerMove(FILE *fptr, Int32 *CURRENTSIDEptr, Int32 PLAYER, uint64_t inHash)
{
  // example of adjusting board from here:
  // FLTKbrdui->zoom->value(1.2);
  clock_gettime(CLOCK_REALTIME, &start); // starting time
  move_t bestLine[deepestLine];
  uint64_t hashval = hashFullPosition(&DisplayBoard);
  if (*CURRENTSIDEptr != WHITE)
    hashval = toggleWhiteToPlayHash(hashval); // toggles each level, so start with opposite one
  printf("\n move %d ", moveNumber);
  // clearHashLedges(); // sets 'num' to 0 on each ledge
  Int32 pruneVintageHashes = 4;
  if (moveNumber % pruneVintageHashes == 0)
    removeOldHashpoints(pruneVintageHashes); // removes older ones

  move_t inMove;
  double secs;
  Int32 diff_WmB = 0;
  char bookName[100];
  if (FLTKbookOpeningsOn == 1)
    FLTKbookOpeningsOn = getBookMove(moveNumber, moveHistory, &inMove, bookName); // returns 0 if no book move found
  // if found
  if (FLTKbookOpeningsOn == 1)
  {
    // printf("\n in book :: %s ", bookName); // print opening name
    // printMove(inMove);
    moveToAlgerbraic(inMove, &DisplayBoard, moveNumber, FLTKlastMoveStr); // for UI
  }
  Int32 depthDone = 0; // just to carry covesCalculated out of braces
  if (FLTKbookOpeningsOn != 1)
  {
    Int32 depth = 0;
    Int32 alpha = -1e8, beta = 1e8;
    maxShootOutDepth = 0;
    avShootOutDepth = 0;
    numShootOuts = 0;
    Int32 movesCalculated;

    diff_WmB = getBestMove(alpha, beta, &DisplayBoard, &inMove, *CURRENTSIDEptr, depth, FLTKmaxDepth,
                           bestLine, FLTKstrategy, hashval, &movesCalculated);

    totalCompDepths += movesCalculated;
    totalCompMoves += 1;
    FLTKlastDepth = movesCalculated;
    FLTKassessment = diff_WmB;
    moveToAlgerbraic(inMove, &DisplayBoard, moveNumber, FLTKlastMoveStr);
    if (horizonMethod == HorizonShootOut)
    {
      printf("\n Max shoot-out depth %d avg depth = %.2f, number =%d", maxShootOutDepth,
             (float)avShootOutDepth / (float)numShootOuts, numShootOuts);
    }
    char tmpstr[100];
    // temp to be removed
    move2string(inMove, tmpstr);
    // printf("\n EARLYmove %ld comps. move coords: ", (int64_t)moveNumber);
    // printf("%s", tmpstr);
    // fflush(stdout);
    if (verbose > 0)
    {

      // print out evaluated moves
      printf("\n level %d main, side: %d maxDepth %d\n moves: { ", depth, *CURRENTSIDEptr, FLTKmaxDepth);
      for (Int32 ii = 0; ii < evaluatedMoveNum; ++ii)
      {
        move2string(evaluatedMoves[ii], tmpstr);
        printf(" %s %d,", tmpstr, evaluatedMoves[ii].evaluation);
      }
      printf(" }");
      printf("\n num of moves = %ld", (int64_t)evaluatedMoveNum);
    } // end if verbose

    // printBestLine(bestLine, movesCalculated, diff_WmB);
    // printHashesBestLine(board1, inMove, movesCalculated, diff_WmB, CURRENTSIDE, hashval);
    HashesToBestline(&DisplayBoard, inMove, bestLine, movesCalculated, diff_WmB, *CURRENTSIDEptr, hashval);
    bestLineToAlgebraicString(&DisplayBoard, bestLine, movesCalculated, diff_WmB, *CURRENTSIDEptr); // for display

    printBestLineAlgebraic(&DisplayBoard, bestLine, movesCalculated, diff_WmB, *CURRENTSIDEptr);
    depthDone = (Int32)movesCalculated; // to carry beyond braces

    move2string(inMove, tmpstr);
    printf("\n move %ld comps. move coords: ", (int64_t)moveNumber);
    printf("%s", tmpstr);
    fflush(stdout);
  } // end if NOT book move

  clock_gettime(CLOCK_REALTIME, &end); // finishing time (for .._withStats() only)
  secs = (double)(end.tv_sec - start.tv_sec) + (1e-9) * (double)(end.tv_nsec - start.tv_nsec);
  printf("\n time taken for move = %.3f sec.", secs);
  // totalCompTime += secs; // done elsewhere
  // totalCompMoves += 1;
  // average depth for game.txt
  double avDepth;
  if (totalCompMoves > 0)
    avDepth = (double)totalCompDepths / ((double)totalCompMoves);
  else
    avDepth = 0.0;

  printf("\n **av depth %f **",avDepth);fflush(stdout);

  storeMoveHistory(&DisplayBoard, inMove, moveNumber, hashval);
  writeMove_withStats(fptr, &DisplayBoard, inMove, moveNumber, diff_WmB, secs, depthDone,avDepth); // write to record file
  doMove(&DisplayBoard, inMove, DUMMYhash);
  lastMoveBefore = inMove; // update for e.p. purposes
  if (abs(diff_WmB) >= values[6] / 10)
    printf("\n *predicted forced checkmate*");

  *CURRENTSIDEptr = -(*CURRENTSIDEptr);
  ++moveNumber;
  return (1);
}

Int32 tryPlayerMove(FILE *fptr, move_t DisplayMove, Int32 *CURRENTSIDEptr, Int32 PLAYER, uint64_t inHash)
{
  move_t inMove;
  _position_t defended = emptyBoard;
  _position_t attacked = emptyBoard;
  move_t movesArray[122];
  Int32 colourValPtr;

  move_t keeplastBefore = lastMoveBefore;
  lastMoveBefore = lastMove; // watch this

  // using BOTH_COLOURS  allows white then blacke then white etc
  if (*CURRENTSIDEptr == PLAYER || PLAYER == BOTH_COLOURS)
  {
    // get legal moves
    // need opposite colours defended squares first, as these are the current colours 'attacked', else will castle in/through check
    getColoursMoves(&DisplayBoard, &defended, &attacked, movesArray, &colourValPtr, -*CURRENTSIDEptr, lastMoveBefore);
    attacked = defended;

    Int32 currentMoves = getColoursMoves(&DisplayBoard, &defended, &attacked, movesArray, &colourValPtr, *CURRENTSIDEptr, lastMoveBefore);

    Int32 isInMoves = isWithinMoves(&DisplayMove, movesArray, currentMoves);
    // printf("\n in moves test = %d, ", isInMoves);
    // fflush(stdout);
    if (isInMoves == 1)
    {
      storeMoveHistory(&DisplayBoard, DisplayMove, moveNumber, inHash);
      writeMove(fptr, &DisplayBoard, DisplayMove, moveNumber); // write to record file
      doMove(&DisplayBoard, DisplayMove, inHash);
      lastMoveBefore = inMove; // must update for e.p.
      // move successful, so change side -> no more player moves until comp moves
      *CURRENTSIDEptr = -(*CURRENTSIDEptr);
      ++moveNumber; // and increment movenumber
                    // printf("\n no more moves til comp moves, movenumber now %d", moveNumber);
                    // fflush(stdout);
      return (1);
    }

    if (1) // very verbose!
    {
      printf("\n Valid moves are: \n {");
      char tempstr[20];
      for (Int32 ii = 0; ii < currentMoves; ++ii)
      {
        move2string(movesArray[ii], tempstr);
        printf("%s, ", tempstr);
      }
      printf("}  \n");
      printf("\n board move: ");
      move2string(DisplayMove, tempstr);
      printf("%s, ", tempstr);
      fflush(stdout);
    }
  }

  // reset if failed
  lastMoveBefore = keeplastBefore; // wach this!
  return (0);
}

void freeHashesAndOpenings()
{
  freeOpenings();
  freeFirstMoves();
  freeHashLedges();
  fclose(FLTKfptr);
}
// initialise() sets up initial values and allocates some arrays
void initialise_chess()
{
  // set up symbols in char array pieces[1]=P, 2=N, .. 6=K  (0  = ' ')
  strcpy(pieces, ".PNBRQK");

  // set piece values (bishop slightly greater than knight to start. Useful to see Noah Caplinger's book
  values[0] = 0;       // not used
  values[1] = 1000;    // pawn
  values[2] = 3000;    // knight
  values[3] = 3300;    // bishop
  values[4] = 5000;    // rook
  values[5] = 9700;    // queen
  values[6] = 1000000; // king

  // fill the 'null king area' with zeroes

  for (Int32 ii = 0; ii < 8; ++ii)
  {
    for (Int32 jj = 0; jj < 8; ++jj)
      nullKingArea.board[ii][jj] = 0;
  }
  // initialise history storage
  //positionHistory = (position_t)calloc(MAXHIST, sizeof(_position_t));

  // set up knight positions
  Int32 line[64] = {-24, -50, -12, -12, -12, -12, -50, -24, -16, -8, 0, 2, 2, 0, -8, -16,
                    -6, 10, 8, 6, 6, 8, 2, -6, -12, 2, 6, 10, 10, 6, 2, -12, -12, 2, 6, 10, 10, 6, 2, -12, -12, 4, 12, 12, 12, 12, 4, -12,
                    -8, -4, 0, 0, 0, 0, -4, -8, -20, -16, -12, -12, -12, -12, -16, -20};
  for (int ii = 0; ii < 8; ++ii)
  {
    for (Int32 jj = 0; jj < 8; ++jj)
      knightWeights.board[ii][jj] = (Int32)(0.8 * movementWeighting * line[ii + jj * 8]); // 0.8 as too keen to swap bishop for knight if 1.0
  }
  // uncomment to see knight weights
  // showBoard1(&knightWeights); exit(0);
  Int32 line2[64] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 10, 4, 8, 8, 4, 10, 0,
                     0, 1, 5, 9, 9, 5, 1, 0, 0, 0, 6, 10, 10, 6, 0, 0,
                      0, 2, 7, 11, 11, 7, 2, 0, 4, 4, 8, 12, 12, 8, 4, 4,
                     5, 7, 9, 13, 13, 9, 7, 5, 6, 8, 10, 14, 14, 10, 8, 6};
  for (int ii = 0; ii < 8; ++ii)
  {
    for (Int32 jj = 0; jj < 8; ++jj)
    {
      pawnWeights.board[ii][jj] = (Int32)(movementWeighting * line2[ii + jj * 8]);
      emptyBoard.board[ii][jj] = 0; // set all to zero
    }
  }
  // uncomment to see pawn weights
  //showBoard1(&pawnWeights); exit(0);
// endgame bonuses for king
  Int32 line3[64] = {0, 0, 4, 6, 6, 4, 0, 0, 0, 4, 6, 8, 8, 6, 4, 0,
    4, 6, 8, 10, 10, 8, 6, 4, 6, 8, 10, 12, 12, 10, 8, 6,
    6, 8, 10, 12, 12, 10, 8, 6, 4, 6, 8, 10, 10, 8, 6, 4,
    0, 4, 6, 8, 8, 6, 4, 0, 0, 0, 4, 6, 6, 4, 0, 0};
for (int ii = 0; ii < 8; ++ii)
{
for (Int32 jj = 0; jj < 8; ++jj)
{
  // need a large weight to encourage king anywhere in endgame, doubled again if FULLENDGAME
endgameKing.board[ii][jj] = (Int32)movementWeighting * 1*( line3[ii + jj * 8]);
}
}
// uncomment to see pawn weights
//showBoard1(&endgameKing); exit(0);


  // set up a null (deliberately invalid) move from (-1,-1)  to (-1,-1)
  nullMove.from[0] = -1;
  nullMove.from[1] = -1;
  nullMove.to[0] = -1;
  nullMove.to[1] = -1;
  nullMove.status = 0;

  // setup Zobrist hash tables
  setupHashTables();

  //  Read openings or set up first moves
  setUpFirstMoves();
  printf("\n got here xxx");
  fflush(stdout);

  // initialise castling right (= to check if a king has moved) - must be done after reading openings
  castlingRights.whiteKingHasMoved = 0;
  castlingRights.blackKingHasMoved = 0;

  // zero EP (en passant) squres
  for (Int32 ii = 0; ii < 8; ++ii)
    for (Int32 jj = 0; jj < 8; ++jj)
      castlingRights.EPsquares.board[ii][jj] = 0;

} // end initialise()

// sets up standard starting position
void startingBoard(position_t newBoard)
{
  zeroPosition(newBoard);
  //  pawns (1)
  for (Int32 ii = 0; ii < 8; ++ii)
  {
    newBoard->board[ii][1] = 1;  // white pawns
    newBoard->board[ii][6] = -1; // black pawns
  }

  // pieces

  // white rooks (4)
  newBoard->board[0][0] = 4;
  newBoard->board[7][0] = 4;
  // black rooks
  newBoard->board[0][7] = -4;
  newBoard->board[7][7] = -4;

  // white and black knights (2)
  newBoard->board[1][0] = 2;
  newBoard->board[6][0] = 2;
  newBoard->board[1][7] = -2;
  newBoard->board[6][7] = -2;

  // white and black bishops (3)
  newBoard->board[2][0] = 3;
  newBoard->board[5][0] = 3;
  newBoard->board[2][7] = -3;
  newBoard->board[5][7] = -3;

  // queens (4)
  newBoard->board[3][0] = 5;
  newBoard->board[3][7] = -5;

  // kings (5)
  newBoard->board[4][0] = 6;
  newBoard->board[4][7] = -6;

  // initialise lastMove
  lastMove.status = 0;
  lastMove.from[0] = 0;
  lastMove.from[1] = 0;
  lastMove.to[0] = 0;
  lastMove.to[1] = 0;
}

// very basic display showing piece indexes, also useful for viewing positional counts such as 'defended' or 'attacking'
void showBoard1(position_t board1)
{
  printf("\n");
  for (Int32 jj = 7; jj >= 0; --jj)
  {
    for (Int32 ii = 0; ii < 8; ++ii)
    {
      printf(" %2d", board1->board[ii][jj]);
    }
    printf("\n");
  }
}

// changes indices 0-6 to ./p/n/b/r/q/k
void index2piece(Int32 pieceIndex, char *str)
{
  // str[1]= pieces[abs(pieceIndex)]; // get the basic piece symbol
  // if (pieceIndex >= 0)
  //   str[0] = ' ';
  // else
  //   str[0] = '-';
  str[0] = ' ';
  if (pieceIndex >= 0)
    str[1] = pieces[pieceIndex];
  else
    str[1] = (char)((Int32)pieces[abs(pieceIndex)] + 32); // change to lower case
  str[2] = '\0';                                          // terminator
}

//  better display showing unicode chess pieces or p,n,b ... etc
void showBoard2(position_t board1, Int32 PLAYER)
{
  char str[10];

  Int32 pieceIndex;

  printf("\n");
  if (PLAYER == BLACK)
  {
    for (Int32 jj = 0; jj < 8; ++jj)
    {
      printf("   %1ld |", (int64_t)jj + 1);
      for (Int32 ii = 7; ii >= 0; --ii)
      {
        pieceIndex = board1->board[ii][jj];
        if (doUnicodeOrRomanPieces == 1)
          index2UnicodePiece(pieceIndex, str, DrawWhiteOnBlack); // unicode chess piece symbol
        else
          index2piece(pieceIndex, str); // basic P, N, B ..

        printf("%s", str);
      }
      printf("\n");
    }
    printf("      ----------------\n");
    printf("      ");
    for (Int32 ii = 7; ii >= 0; --ii)
      printf(" %c", (char)(ii + 97));
  }
  else
  {
    for (Int32 jj = 7; jj >= 0; --jj)
    {
      printf("   %1ld |", (int64_t)jj + 1);
      for (Int32 ii = 0; ii < 8; ++ii)
      {
        pieceIndex = board1->board[ii][jj];
        if (doUnicodeOrRomanPieces == 1)
          index2UnicodePiece(pieceIndex, str, DrawWhiteOnBlack); // unicode chess piece symbol
        else
          index2piece(pieceIndex, str); // basic P, N, B ..

        printf("%s", str);
      }
      printf("\n");
    }
    printf("      ----------------\n");
    printf("      ");
    for (Int32 ii = 0; ii < 8; ++ii)
      printf(" %c", (char)(ii + 97));
  } // end else
}

// tots up the piece values and adds positional considerations (movement, attaccks, doubled pawns etc)
Int32 evaluateBoard(position_t board1, move_t movesArray[maxPossMoves], Int32 *colourValPtr, Int32 COLOUR,
                    Int32 NEXTMOVE, move_t lastMoveBefore)
{
  // COLOUR is +1 for white, -1 for black
  // zero the evaluations first
  *colourValPtr = 0;
  position_t defended = (position_t)calloc(1, sizeof(_position_t));
  position_t attacked = (position_t)calloc(1, sizeof(_position_t));
  zeroPosition(attacked);
  Int32 oppKing = -COLOUR * 6, ixOppKing = 0, iyOppKing = 0;
  Int32 ownKing = -oppKing,    ixOwnKing = 0, iyOwnKing = 0;

  Int32 totalMoves = 0; // total legal moves of COLOUR from the position

  // material totting
  for (Int32 jj = 7; jj >= 0; --jj) // starting at top
  {
    for (Int32 ii = 0; ii < 8; ++ii)
    {
      Int32 piece = board1->board[ii][jj];
      if (piece * COLOUR > 0)                // condition for being same sign/side as 'COLOUR'
        *colourValPtr += values[abs(piece)]; // add to colour's total
      if (piece == oppKing)
      {
        ixOppKing = ii; // record king's position for later
        iyOppKing = jj;
      }

      if (piece == ownKing)
      {
        ixOwnKing = ii; // record king's position for later
        iyOwnKing = jj;
      }

    } // end for ii
  } // end for jj

  // material totted. now need to see what moves possible ..

  // material totting, for whole side (given by 'COLOUR')
  totalMoves = getColoursMoves(board1, defended, attacked, movesArray, colourValPtr, COLOUR, lastMoveBefore);

  // have a 'fresh' defended - these squares are attacked for the other colour, so can weight pressure on opposing king here
  if (COLOUR < 0) // note negated (equal to if( -COLOUR > 0) )
    workingKingArea = whiteKingArea;
  else
    workingKingArea = blackKingArea;

  Int32 kingPressure = 0;
  // narror range for speed 1either side and up to 2 up or down
  Int32 ist = ixOppKing - 1, ifin = ixOppKing + 1, jst = iyOppKing - 2, jfin = iyOppKing + 2;
  if (ist < 0)
    ist = 0;
  if (ifin > 7)
    ifin = 7;
  if (jst < 0)
    jst = 0;
  if (jfin > 7)
    jfin = 7;
  for (Int32 ii = ist; ii <= ifin; ++ii)
  {
    for (Int32 jj = jst; jj <= jfin; ++jj)
    {
      if (workingKingArea.board[ii][jj] > 0 && defended->board[ii][jj] > 0)
      {
        kingPressure += (defended->board[ii][jj] * workingKingArea.board[ii][jj]);
      }
    }
  }
  *colourValPtr += kingPressure * kingPressureBonus;

  // and reward for protection around own king
  if (COLOUR > 0) // note negated compaared with attack bonus
    workingKingArea = whiteKingArea;
  else
    workingKingArea = blackKingArea;

  Int32 kingDefence = 0;
  // narror range for speed 1either side and up to 2 up or down
  ist = ixOwnKing - 1; ifin = ixOwnKing + 1; jst = iyOwnKing - 2; jfin = iyOwnKing + 2;
  if (ist < 0)
    ist = 0;
  if (ifin > 7)
    ifin = 7;
  if (jst < 0)
    jst = 0;
  if (jfin > 7)
    jfin = 7;
  for (Int32 ii = ist; ii <= ifin; ++ii)
  {
    for (Int32 jj = jst; jj <= jfin; ++jj)
    {
      if (workingKingArea.board[ii][jj] > 0 && defended->board[ii][jj] > 0)
      {
        kingDefence += (defended->board[ii][jj] * workingKingArea.board[ii][jj]);
      }
    }
  }
  *colourValPtr += kingDefence * kingDefenceBonus;

 

  // //////   positional changes   //////

  free(defended);
  free(attacked);
  return (totalMoves);

} // end evaluateBoard()

// finds possible moves for one colour with some weighting, also finds 'defended' squares; only uses 'attacked'
Int32 getColoursMoves(position_t board1, position_t defended, position_t attacked, move_t movesArray[maxPossMoves],
                      Int32 *colourValPtr, Int32 COLOUR, move_t lastMoveBefore)
{
  /* if(COLOUR < 0 && moveNumber>=3){
   printf("\n attacked colour %d move %ld",COLOUR, moveNumber);
   showBoard1(attacked);
   exit(0); } */

  // zero count of pawns per file
  for (Int32 ii = 0; ii < 8; ++ii)
    pawnFiles[ii] = 0;

  // get defended squres of side, first set to zero
  zeroPosition(defended);

  Int32 totalMoves = 0;
  Int32 moves;
  Int32 weightedMove;

  for (Int32 jj = 7; jj >= 0; --jj) // starting at top
  {
    for (Int32 ii = 0; ii < 8; ++ii)
    {
      Int32 piece = board1->board[ii][jj];
      if (piece * COLOUR > 0)
      {
        Int32 startMoves = totalMoves;
        // weightedMove is number of moves with extra weighting for position, attacking, piece-type etc
        moves = getSquaresMoves(board1, defended, attacked, movesArray, startMoves, ii, jj, &weightedMove, lastMoveBefore);
        // extras for castling etc (based on one position)
        pieceLocationBonuses(board1, ii, jj, &weightedMove);

        *colourValPtr += weightedMove; //*movementWeighting;
        totalMoves += moves;
      }

    } // end for ii
  } // end for jj

  // pawnFiles[] updated by calls to pieceLocationBonuses() - containes no. of own pawns in each file
  // .. penalise doubled & tripled pawns
  Int32 pawnValue = values[1];
  if (pawnValue != 1000)
  {
    printf("\n problem here, pawn value not 1000 - needs a look! -exiting\n");
    exit(0);
  }
  for (Int32 ii = 0; ii < 8; ++ii)
  {
    if (pawnFiles[ii] == 2)
    {
      *colourValPtr -= 300; // 0.3 pawn value - **assumes 1000 for pawn **
      //  printf("\n doubled pawn"); showBoard1(board1);
    }
    if (pawnFiles[ii] == 3)
    {
      *colourValPtr -= 1000; // full pawn for tripled
                             //  printf("\n tripled pawn"); showBoard1(board1);
    }
  }

  // penalise isolated pawns
  // edge ones first - needs at least 400 milli-pawns per pawn to stop Pxa3 with a choice of Nxa3
  if (pawnFiles[0] > 0 && pawnFiles[1] == 0)
    *colourValPtr -= 400 * pawnFiles[0]; // .3 pawn total for each isolated Rs pawn
  if (pawnFiles[7] > 0 && pawnFiles[6] == 0)
    *colourValPtr -= 400 * pawnFiles[7]; // .3 pawn total for each isolated Rs pawn
  // then centre
  for (Int32 ii = 1; ii < 6; ++ii)
  {
    if (pawnFiles[ii] > 0 && pawnFiles[ii - 1] == 0 && pawnFiles[ii + 1] == 0)
    {
      *colourValPtr -= 400 * pawnFiles[ii]; //
                                            // printf("\n isolated pawn");showBoard1(board1);
    }

    // bonus for first 2 open files (one per flank only)
    // Q side R & N files
    if (pawnFiles[0] == 0 || pawnFiles[1] == 0)
    {
      if (pawnFiles[1] == 0)
        *colourValPtr += 100; // more for the knight's file (0.1 pawn)..
      else
        *colourValPtr += 50; // than the rook file (1/20)
    }
    // K side R & N files
    if (pawnFiles[6] == 0 || pawnFiles[7] == 0)
    {
      if (pawnFiles[6] == 0)
        *colourValPtr += 100; // more for the knight's file ..
      else
        *colourValPtr += 50; // than the rook file
    }

    /// passed pawns could be detected by storing matching versions for white and black
    // .. not doing yet as rarely get there!
  }

  return (totalMoves);
}
// adds bonuses for position of some pieces
void pieceLocationBonuses(position_t board1, Int32 ii, Int32 jj, Int32 *weightedMove)
{
  Int32 piece = (Int32)board1->board[ii][jj];
  Int32 absPiece = abs(piece);

  // ///KING ///
  if (absPiece == 6)
  {
    if(gameStage == MAINGAME)
    {
    // to encourage castling, strategically ..
    // add something if pawns in front
    Int32 forwardStep;
    if (piece > 0) // if white
      forwardStep = 1;
    else
      forwardStep = -1;

    if (forwardStep == 1 && jj != 0)
      forwardStep = 0; // if white must be on 1st rank
    if (forwardStep == -1 && jj != 7)
      forwardStep = 0; // or on eighth rank if black

    if (forwardStep != 0)
    {
      // award being castled-like :pawns in front and away from the centre
      Int32 castlingBonus = 0;

      Int32 ownPawn = forwardStep;
      Int32 bonus = castlingPositionBonuses; // so can use faster shifts for divisions
      // if *own* pawns straight in front, or 1 square either side
      if (board1->board[ii][jj + forwardStep] == ownPawn)
        castlingBonus += bonus;
      if (ii > 0 && board1->board[ii - 1][jj + forwardStep] == ownPawn)
        castlingBonus += bonus;
      if (ii < 7 && board1->board[ii + 1][jj + forwardStep] == ownPawn)
        castlingBonus += bonus;
      // lesser additions for two in front ( 1/4 using >>2)
      forwardStep *= 2;
      if (board1->board[ii][jj + forwardStep] == ownPawn)
        castlingBonus += (bonus >> 2);
      if (ii > 0 && board1->board[ii - 1][jj + forwardStep] == ownPawn)
        castlingBonus += (bonus >> 2);
      if (ii < 7 && board1->board[ii + 1][jj + forwardStep] == ownPawn)
        castlingBonus += (bonus >> 2);
      // printf(" castling bonus %.3f",castlingBonus);

      // encourage king to the sides (needs a consideration of material left on the board later)
      Int32 fileFactor = 8; // effectively *1.0 as divided by 8 before mult
      if (ii == 4 || ii == 3)
        fileFactor = 1; // centre not much good
      else if (ii == 5 || ii == 2)
        fileFactor = 8; // bishops square better
      else if (ii == 6 || ii == 1)
        fileFactor = 11; // .. knight's even better
      else if (ii == 7 || ii == 0)
        fileFactor = 10; // .. Rook's nearly as good

      *weightedMove += ((fileFactor * castlingBonus) >> 3); // div by 8
    } // end if
  } // end if MAINGAME stage

  if(gameStage >= EARLYENDGAME) // encourage the king towards centre
  {
    *weightedMove += endgameKing.board[ii][jj];
    //printf("\n early king %d %d %d",ii,jj,endgameKing.board[ii][jj]);

  }// end if EARLYENDGAME
  // add again if full endgame (equiv to *2 for fullendgame
  if(gameStage >= FULLENDGAME) // encourage the king towards centre
  {
    *weightedMove += endgameKing.board[ii][jj];
    //printf("\n full king %d %d %d",ii,jj,endgameKing.board[ii][jj]);

  }// end if FULLENDGAME
  } // end if King

  // if a pawn
  else if (absPiece == 1)
  {
    // increment count of pawns per file
    pawnFiles[ii] = pawnFiles[ii] + 1;
  }
  // if a bishop
  else if (absPiece == 3)
  {
    // bonus for being twoards ranks 3,4 & 5
    Int32 iy = jj;
    if (piece < 0) // reverse vertical distance if black ..
      iy = 7 - jj;

    if (iy == 0)
      *weightedMove -= 50 * movementWeighting; // discourage staying on back rank
    else if (iy == 1)
      *weightedMove += 0; // 2nd rank ok but tame
    else
      *weightedMove += movementWeighting; // a bit livelier
  }
  // if a Rook
  else if (absPiece == 4)
  {
    if(gameStage == MAINGAME)
    {
    // encourage towards centre files
    if (ii == 1 || ii == 6)
      *weightedMove += movementWeighting;
    else if (ii == 2 || ii == 5)
      *weightedMove += 2 * movementWeighting; // bishop's file
    else if (ii == 3 || ii == 4)
      *weightedMove += 3 * movementWeighting; // central file
    }// end if maingame
    // no particular rook bonuses once towards endgame
  } // end if rook

} // end pieceLocationBonuses()

// finds moves possible for a single piece on a specified square
Int32 getSquaresMoves(position_t board1, position_t defended, position_t attacked, move_t movesArray[maxPossMoves], Int32 startMoves,
                      Int32 ii, Int32 jj, Int32 *weightedMove, move_t lastMoveBefore)
{
  Int32 moves = 0;
  *weightedMove = 0;
  Int32 ix, iy;
  Int32 piece = (Int32)board1->board[ii][jj];
  Int32 absPiece = abs(piece);
  if (absPiece == 1) // if a pawn (w or b)
  {
    // main positional weight from here, (could be in positionalBonuses...)
    ix = ii;
    if (piece > 0) // if white
      iy = jj;
    else
      iy = 7 - jj; // invert for black
    *weightedMove = pawnWeights.board[ix][iy];

    Int32 blockedPawn = 0;
    Int32 step = piece; // starts as 1 (+1 if white or -1 if black)

    Int32 maxMult = 1;
    if ((piece == 1 && jj == 1) || (piece == -1 && jj == 6))
      maxMult = 2; // initial double step condition

    Int32 piece2;
    iy = jj;
    for (Int32 multStep = 1; multStep <= maxMult; ++multStep)
    {
      ///// pawn step ///

      // new position
      ix = ii; // x unchanged
      // iy = jj + step * multStep;
      iy += step;

      // see if valid

      if (ix > 7 || ix < 0 || iy > 7 || iy < 0)
        break; // outside board

      // now see if anything there
      piece2 = (Int32)board1->board[ix][iy]; // no defended count increment for pawn forward move
      if (abs(piece2) > 0)
      {
        // penalty if K or Q ranks completely blocked by piece ( bad if either colour)
        if (ix > 2 && ix < 5 && maxMult == 2 && multStep == 1 && abs(piece2) > 1)
        {
          // blockedPawn = 5 * movementWeighting;
          blockedPawn = ((Int32)movementWeighting) << 2;

          *weightedMove -= blockedPawn; // penalised
        }

        break; // is blocked by something
      }

      Int32 status = 0;
      if (multStep == 2)
        status = 1; // flag for en-passant potential (bit 0)

      // store move before incrementing 'moves', as array starts from zero
      movesArray[startMoves + moves].from[0] = ii;
      movesArray[startMoves + moves].from[1] = jj;
      movesArray[startMoves + moves].to[0] = ix;
      movesArray[startMoves + moves].to[1] = iy;
      movesArray[startMoves + moves].status = status;
      moves += 1;

    } // end for

    // now for take left and right
    // : take left
    // new position
    ix = ii - 1; // left
    iy = jj + step;

    // see if valid...
    Int32 valid = 1; // start with assumption of valid
    if (ix > 7 || ix < 0 || iy > 7 || iy < 0)
      valid = 0; // outside board

    // now see if anything there
    piece2 = 0;
    if (valid == 1)
    {
      piece2 = (Int32)board1->board[ix][iy];
      defended->board[ix][iy] += 1; // increment times defended count

      // if(piece2 ==0 || (sign(piece) == sign(piece2)))// need to be opposite signs (and not zero)
      // if( (piece >0 && piece2 >0) || (piece < 0 && piece2 <0)) // seems to be much slower...
      if (piece * piece2 >= 0) // need to be opposite signs (and not zero)
      {
        valid = 0;                        // is blocked
        *weightedMove += defendWeighting; // something for defending
      }
    }

    // can take something
    if (valid == 1)
    {
      Int32 absPiece2 = abs(piece2);
      Int32 status = 2;  // move takes (bit 1)
      if (absPiece2 > 1) // if a major piece (N, B, R...) (bits 1 & 2)
        status = 14;     // will also be a greater value piece so bits 1,2 & 3

      if (absPiece2 == 6) // if the king (bit 4 also)
        status = 30;

      movesArray[startMoves + moves].from[0] = ii;
      movesArray[startMoves + moves].from[1] = jj;
      movesArray[startMoves + moves].to[0] = ix;
      movesArray[startMoves + moves].to[1] = iy;
      movesArray[startMoves + moves].status = status;
      moves += 1;
      *weightedMove += attackWeighting; // for a threat
    } // end if valid

    // : take right ..
    // new position
    ix = ii + 1; // right
    iy = jj + step;

    // see if valid

    valid = 1; // start with assumption of valid
    if (ix > 7 || ix < 0 || iy > 7 || iy < 0)
      valid = 0; // outside board

    // now see if anything there
    piece2 = 0;
    if (valid == 1)
    {
      piece2 = (Int32)board1->board[ix][iy];
      defended->board[ix][iy] += 1; // increment times defended count

      if (piece * piece2 >= 0) // need to be opposite signs (and not zero)
      {
        valid = 0;                        // is blocked
        *weightedMove += defendWeighting; // something for defending
      }
    }

    // can take something
    if (valid == 1)
    {

      Int32 absPiece2 = abs(piece2);
      Int32 status = 2;  // move takes (bit 1)
      if (absPiece2 > 1) // if a major piece (N, B, R...) (bits 1 & 2)
        status = 14;     // bits 1,2 &3 as greater value

      if (absPiece2 == 6) // if the king (bit 4 also)
        status = 30;

      //  store move before incrementing 'moves', as array starts from zero
      movesArray[startMoves + moves].from[0] = ii;
      movesArray[startMoves + moves].from[1] = jj;
      movesArray[startMoves + moves].to[0] = ix;
      movesArray[startMoves + moves].to[1] = iy;
      movesArray[startMoves + moves].status = status;

      moves += 1;
      *weightedMove += attackWeighting; // as a threat
    } // end if valid

    // now for en-passant:if white pawn and on 5th rank (or black on 4th) and last move was e.p. possible
    Int32 takingRank = 4; // white takes e.p. on the 5th rank (index 4), black on the 4th rank (index 3)
    if (piece < 0)
      takingRank = 3;

    if (absPiece == 1 && jj == takingRank && (lastMoveBefore.status & 1) > 0)
    {
      // n.b this function is sometimes called 'out of turn' to assess defended squares or similar, must check opposite sign pawns (ie piece*piece = -1)
      // adjacent file, correct rank and opposite pawns
      if (abs(ii - lastMoveBefore.to[0]) == 1 && lastMoveBefore.to[1] == takingRank &&
          board1->board[ii][jj] * board1->board[lastMoveBefore.to[0]][lastMoveBefore.to[1]] == -1)
      {
        // can take it
        Int32 status = 2; // move takes (bit 1)
        ix = lastMoveBefore.to[0];
        iy = lastMoveBefore.to[1];
        movesArray[startMoves + moves].from[0] = ii;
        movesArray[startMoves + moves].from[1] = jj;
        movesArray[startMoves + moves].to[0] = ix;
        movesArray[startMoves + moves].to[1] = iy;
        movesArray[startMoves + moves].status = status;

        moves += 1;
        *weightedMove += attackWeighting; // as a threat
      }
    }

  } // end if 1, a pawn
  else if (absPiece == 2) // if a knight (w or b)
  {
    Int32 defendingPiece = 0;
    Int32 attackingPiece = 0;
    for (Int32 k = 0; k < 8; ++k)
    {
      Int32 iStep = (Int32)xknightSteps[k];
      Int32 jStep = (Int32)yknightSteps[k];

      for (Int32 stepMult = 1; stepMult <= 1; ++stepMult)
      {

        ix = ii + iStep; // * stepMult;
        iy = jj + jStep; // * stepMult;

        if (ix > 7 || ix < 0 || iy > 7 || iy < 0)
          break; //  end of that direction run, : next k

        Int32 piece2;
        piece2 = (Int32)board1->board[ix][iy]; // see whats on new square
        defended->board[ix][iy] += 1;          // mark as defended

        Int32 status = 0;
        // not ok is piece2 sign the same as piece (and number +ve)
        if (piece * piece2 > 0)
        {
          defendingPiece = defendWeighting; // but definitely worth a square count
          break;                            // end of run in that direction
        }

        // worth a bonus if attacking a piece?
        if (piece * piece2 < 0)
        {
          attackingPiece = attackWeighting;
          Int32 absPiece2 = abs(piece2);
          status = 2;        // move takes (bit 1)
          if (absPiece2 > 1) // if a major piece (N, B, R...) (bits 1 & 2)
            status = 6;
          if (absPiece2 >= 4) // if a greater or equal value piece (R,Q,K) (bit 1, 2 & 3)
            status = 14;
          if (attacked->board[ix][iy] == 0)
            status = 14;      // apparent 'winning move' if not defended
          if (absPiece2 == 6) // if the king (bit 4 also)
            status = 30;
        }

        // store move before incrementing 'moves', as array starts from zero
        movesArray[startMoves + moves].from[0] = ii;
        movesArray[startMoves + moves].from[1] = jj;
        movesArray[startMoves + moves].to[0] = ix;
        movesArray[startMoves + moves].to[1] = iy;
        movesArray[startMoves + moves].status = status;
        moves += 1;

      } // end for stepmult
      //  worth  something (anyway) if is defending own piece, or else isattacking
      *weightedMove += defendingPiece; // add to sum
      *weightedMove += attackingPiece; // already gets 1.0 from being a valid move

    } // end for k
    // bulk addition for knight
    ix = ii;
    if (piece > 0) // if white
      iy = jj;
    else
      iy = 7 - jj; // for black

    *weightedMove += knightWeights.board[ix][iy];
  }
  // if a bishop
  else if (absPiece == 3)
  {
    Int32 defendingPiece = 0;
    Int32 attackingPiece = 0;
    for (Int32 iStep = -1; iStep <= 1; iStep += 2)
    {
      for (Int32 jStep = -1; jStep <= 1; jStep += 2)
      {
        Int32 stopped = 0;
        Int32 stepMult = 1;
        ix = ii;
        iy = jj;
        while (1) // do until reach end
        {
          // ix = ii + iStep * stepMult;
          // iy = jj + jStep * stepMult;
          ix += iStep;
          iy += jStep;

          if (ix > 7 || ix < 0 || iy > 7 || iy < 0)
            break; // valid = 0; // outside board

          Int32 piece2 = 0;
          Int32 status = 0;

          piece2 = (Int32)board1->board[ix][iy]; // see whats on new square
          defended->board[ix][iy] += 1;          // increment times defended count

          // not ok is piece2 sign the same as piece (and number +ve)
          if (piece * piece2 > 0)
          {
            defendingPiece += defendWeighting; // but definitely worth a square count
            break;
          }
          // worth a bonus if attacking a piece?
          if (piece * piece2 < 0)
          {
            attackingPiece += attackWeighting;
            stopped = 1; // line ends after storing move
            Int32 absPiece2 = abs(piece2);
            status = 2;        // move takes (bit 1)
            if (absPiece2 > 1) // if a major piece (N, B, R...) (bits 1 & 2)
              status = 6;
            if (absPiece2 >= 4) // if a greater value piece (bit 1, 2 & 3)
              status = 14;
            if (attacked->board[ix][iy] == 0)
              status = 14;      // apparent 'winning move' if not defended
            if (absPiece2 == 6) // if the king (bit 4 also)
              status = 30;
          }

          // store move before incrementing 'moves', as array starts from zero
          movesArray[startMoves + moves].from[0] = ii;
          movesArray[startMoves + moves].from[1] = jj;
          movesArray[startMoves + moves].to[0] = ix;
          movesArray[startMoves + moves].to[1] = iy;
          movesArray[startMoves + moves].status = status;
          moves += 1;
          *weightedMove += movementWeighting;

          if (stopped > 0)
            break; // also stopped at attack point

          ++stepMult;
        } // end while

      } // end for jStep
    } // end for iStep

    //  worth  something if is defending own piece, or else isattacking
    *weightedMove += defendingPiece; // add to sum
    *weightedMove += attackingPiece; // already gets 1.0 from being a valid move

  } // end if a bishop

  // if a *rook*
  else if (absPiece == 4)
  {
    Int32 iStep, jStep;
    Int32 defendingPiece = 0;
    Int32 attackingPiece = 0;
    for (Int32 k = 1; k <= 4; ++k)
    {
      if (k == 1)
      {
        iStep = -1;
        jStep = 0;
      }
      if (k == 2)
      {
        iStep = +1;
        jStep = 0;
      }
      if (k == 3)
      {
        iStep = 0;
        jStep = +1;
      }
      if (k == 4)
      {
        iStep = 0;
        jStep = -1;
      }
      Int32 stepMult = 1;
      Int32 stopped = 0;
      ix = ii;
      iy = jj;
      while (1) // do until reach end
      {
        // ix = ii + iStep * stepMult;
        // iy = jj + jStep * stepMult;
        ix += iStep;
        iy += jStep;

        if (ix > 7 || ix < 0 || iy > 7 || iy < 0)
          break; // valid = 0; // outside board

        Int32 piece2 = 0;
        Int32 status = 0;

        piece2 = (Int32)board1->board[ix][iy]; // see whats on new square
        defended->board[ix][iy] += 1;          // increment times defended count

        // not ok is piece2 sign the same as piece (and number +ve)
        if (piece * piece2 > 0)
        {
          defendingPiece += defendWeighting; // but definitely worth a square count
          break;
        }
        // worth a bonus if attacking a piece?
        if (piece * piece2 < 0)
        {
          attackingPiece += attackWeighting;
          stopped = 1; // mark line as ending after recording move
          Int32 absPiece2 = abs(piece2);
          status = 2;        // move takes (bit 1)
          if (absPiece2 > 1) // if a major piece (N, B, R...) (bits 1 & 2)
            status = 6;
          if (absPiece2 >= 5) // if a greater or equal value piece (bit 1, 2 & 3)
            status = 14;
          if (attacked->board[ix][iy] == 0)
            status = 14;      // apparent 'winning move' if not defended
          if (absPiece2 == 6) // if the king (bit 4 also)
            status = 30;
        }

        // store move before incrementing 'moves', as array starts from zero
        movesArray[startMoves + moves].from[0] = ii;
        movesArray[startMoves + moves].from[1] = jj;
        movesArray[startMoves + moves].to[0] = ix;
        movesArray[startMoves + moves].to[1] = iy;
        movesArray[startMoves + moves].status = status;
        moves += 1;
        *weightedMove += movementWeighting;

        if (stopped > 0)
          break; // also stopped at attack point

        ++stepMult;
      } // end while

    } // end for(k=
    //  worth  something (anyway) if is defending own piece, or else isattacking
    *weightedMove += defendingPiece; // add to sum
    *weightedMove += attackingPiece; // already gets 1.0 from being a valid move

  } // end if a rook
  // if a queen
  else if (absPiece == 5)
  {
    Int32 defendingPiece = 0;
    Int32 attackingPiece = 0;
    for (Int32 iStep = -1; iStep <= 1; ++iStep)
    {
      for (Int32 jStep = -1; jStep <= 1; ++jStep)
      {
        Int32 stepMult = 1;
        Int32 stopped = 0;
        ix = ii;
        iy = jj;
        while (1) // do until reach end
        {

          // ix = ii + iStep * stepMult;
          // iy = jj + jStep * stepMult;
          ix += iStep;
          iy += jStep;
          if (iStep == 0 && jStep == 0)
            break; // not moving is not an option

          if (ix > 7 || ix < 0 || iy > 7 || iy < 0)
            break; // outside board

          Int32 piece2 = 0;
          Int32 status = 0;

          piece2 = (Int32)board1->board[ix][iy]; // see whats on new square
          defended->board[ix][iy] += 1;          // increment squres count of times defended

          // not ok is piece2 sign the same as piece (and number +ve)
          if (piece * piece2 > 0)
          {
            defendingPiece += defendWeighting; // but definitely worth a square count
            break;
          }
          // worth a bonus if attacking a piece?
          if (piece * piece2 < 0)
          {
            attackingPiece += attackWeighting;
            stopped = 1; // mark line as ending after recording move
            Int32 absPiece2 = abs(piece2);
            status = 2;        // move takes (bit 1)
            if (absPiece2 > 1) // if a major piece (N, B, R...) (bits 1 & 2)
              status = 6;
            if (absPiece2 >= 6) // if a greater  value piece (bit 1, 2 & 3)
              status = 14;
            if (attacked->board[ix][iy] == 0)
              status = 14;      // apparent 'winning move' if not defended
            if (absPiece2 == 6) // if the king (bit 4 also)
              status = 30;
          }

          // store move before incrementing 'moves', as array starts from zero
          movesArray[startMoves + moves].from[0] = ii;
          movesArray[startMoves + moves].from[1] = jj;
          movesArray[startMoves + moves].to[0] = ix;
          movesArray[startMoves + moves].to[1] = iy;
          movesArray[startMoves + moves].status = status;
          moves += 1;
          *weightedMove += movementWeighting;

          if (stopped > 0)
            break; // also stopped at attack point

          ++stepMult;
        } // end while
        //  worth  something (anyway) if is defending own piece, or else isattacking

      } // end for jStep
    } // end for iStep
    *weightedMove += defendingPiece; // add to sum
    *weightedMove += attackingPiece; // already gets 1.0 from being a valid move

  } // end if a queen

  // if a king
  else if (absPiece == 6)
  {
    Int32 defendingPiece = 0;
    Int32 attackingPiece = 0;
    for (Int32 iStep = -1; iStep <= 1; ++iStep)
    {
      for (Int32 jStep = -1; jStep <= 1; ++jStep)
      {
        Int32 stepMult = 1;

        while (1) // do until stepMult is incremented, as can only move 1 square in any direction
        {

          ix = ii + iStep * stepMult;
          iy = jj + jStep * stepMult;

          if (iStep == 0 && jStep == 0)
            break; // not moving is not a move

          if (ix > 7 || ix < 0 || iy > 7 || iy < 0)
            break; // outside board

          Int32 piece2 = 0;
          Int32 status = 0;

          piece2 = (Int32)board1->board[ix][iy]; // see whats on new square
          defended->board[ix][iy] += 1;          // increment times defended count

          // not ok is piece2 sign the same as piece (and number +ve)
          if (piece * piece2 > 0)
          {
            defendingPiece += defendWeighting; // but definitely worth a square count
            break;
          }
          // worth a bonus if attacking a piece?
          if (piece * piece2 < 0)
          {
            attackingPiece += attackWeighting;
            Int32 absPiece2 = abs(piece2);
            status = 2;        // move takes (bit 1)
            if (absPiece2 > 1) // if a major piece (N, B, R...) (bits 1 & 2)
              status = 6;
            if (attacked->board[ix][iy] == 0)
              status = 14; // apparent 'winning move' if not defended
          }

          // store move before incrementing 'moves', as array starts from zero
          movesArray[startMoves + moves].from[0] = ii;
          movesArray[startMoves + moves].from[1] = jj;
          movesArray[startMoves + moves].to[0] = ix;
          movesArray[startMoves + moves].to[1] = iy;
          movesArray[startMoves + moves].status = status;
          moves += 1;
          *weightedMove += movementWeighting;

          ++stepMult;

          if (stepMult > 1)
            break; // king only moves 1 square in any direction
        } // end while
        //  worth  something (anyway) if is defending own piece, or else isattacking

      } // end for jStep
    } // end for iStep
    *weightedMove += defendingPiece; // add to sum
    *weightedMove += attackingPiece; // already gets 1.0 from being a valid move

    // //////   CASTLING   //////
    // if WHITE and the king is on original square, and not in check and clear between (no check for having previously moved)
    if (piece > 0 && ii == 4 && jj == 0 && attacked->board[ii][jj] == 0 && castlingRights.whiteKingHasMoved == 0)
    {
      // kings side first
      Int32 piece2 = (Int32)board1->board[7][0]; // piece on KR square
      Int32 valid = 1;                           // assume can castle
      if (piece2 != 4)
        valid = 0; // must be WR
      // check squares between
      piece2 = (Int32)board1->board[6][0]; // piece on KN square
      if (piece2 != 0 || attacked->board[6][0] != 0)
        valid = 0;                         // must be clear
      piece2 = (Int32)board1->board[5][0]; // piece on KB square
      if (piece2 != 0 || attacked->board[5][0] != 0)
        valid = 0; // must be clear

      if (valid == 1) // can castle KR side
      {
        // store move before incrementing 'moves', as array starts from zero
        movesArray[startMoves + moves].from[0] = ii;
        movesArray[startMoves + moves].from[1] = jj;
        movesArray[startMoves + moves].to[0] = ii + 2;
        movesArray[startMoves + moves].to[1] = jj;
        movesArray[startMoves + moves].status = 0;
        moves += 1;
        *weightedMove += movementWeighting;
      }

      // queens side next
      piece2 = (Int32)board1->board[0][0]; // piece on QR square
      valid = 1;                           // assume can castle
      if (piece2 != 4)
        valid = 0; // must be WR
      // check squares between
      piece2 = (Int32)board1->board[1][0]; // piece on QN square (ok if attacked here)
      if (piece2 != 0)
        valid = 0;                         // must be clear
      piece2 = (Int32)board1->board[2][0]; // piece on QB square
      if (piece2 != 0 || attacked->board[2][0] != 0)
        valid = 0;                         // must be clear
      piece2 = (Int32)board1->board[3][0]; // piece on Q square
      if (piece2 != 0 || attacked->board[3][0] != 0)
        valid = 0; // must be clear

      if (valid == 1) // can castle QR side
      {
        // store move before incrementing 'moves', as array starts from zero
        movesArray[startMoves + moves].from[0] = ii;
        movesArray[startMoves + moves].from[1] = jj;
        movesArray[startMoves + moves].to[0] = ii - 2;
        movesArray[startMoves + moves].to[1] = jj;
        movesArray[startMoves + moves].status = 0;
        moves += 1;
        *weightedMove += movementWeighting;
      }
    } // end if WHITE

    // if BLACK and the king is on original square, and not in check (no check for having previosly moved)
    if (piece < 0 && ii == 4 && jj == 7 && attacked->board[ii][jj] == 0 && castlingRights.blackKingHasMoved == 0)
    {
      // kings side first
      Int32 piece2 = (Int32)board1->board[7][7]; // piece on KR square
      Int32 valid = 1;                           // assume can castle
      if (piece2 != -4)
        valid = 0; // must be BR
      // check squares between
      piece2 = (Int32)board1->board[6][7]; // piece on KN square
      if (piece2 != 0 || attacked->board[6][7] != 0)
        valid = 0;                         // must be clear
      piece2 = (Int32)board1->board[5][7]; // piece on KB square
      if (piece2 != 0 || attacked->board[5][7] != 0)
        valid = 0; // must be clear

      if (valid == 1) // can castle KR side
      {
        // store move before incrementing 'moves', as array starts from zero
        movesArray[startMoves + moves].from[0] = ii;
        movesArray[startMoves + moves].from[1] = jj;
        movesArray[startMoves + moves].to[0] = ii + 2;
        movesArray[startMoves + moves].to[1] = jj;
        movesArray[startMoves + moves].status = 0;
        moves += 1;
        *weightedMove += movementWeighting;
      }

      // queens side next
      piece2 = (Int32)board1->board[0][7]; // piece on QR square
      valid = 1;                           // assume can castle
      if (piece2 != -4)
        valid = 0; // must be BR
      // check squares between
      piece2 = (Int32)board1->board[1][7]; // piece on QN square (can be attacked here and castle)
      if (piece2 != 0)
        valid = 0;                         // must be clear
      piece2 = (Int32)board1->board[2][7]; // piece on QB square
      if (piece2 != 0 || attacked->board[2][7] != 0)
        valid = 0;                         // must be clear
      piece2 = (Int32)board1->board[3][7]; // piece on Q square
      if (piece2 != 0 || attacked->board[3][7] != 0)
        valid = 0; // must be clear

      if (valid == 1) // can castle QR side
      {
        // store move before incrementing 'moves', as array starts from zero
        movesArray[startMoves + moves].from[0] = ii;
        movesArray[startMoves + moves].from[1] = jj;
        movesArray[startMoves + moves].to[0] = ii - 2;
        movesArray[startMoves + moves].to[1] = jj;
        movesArray[startMoves + moves].status = 0;
        moves += 1;
        *weightedMove += movementWeighting;
      }
    } // end if BLACK

  } // end if a king

  return (moves);
} // end getSquaresMoves()

// converts from index form to more readable e2e4 style
char *move2string(move_t aMove, char *str)
{
  // str need to be at least 5 chars, 8 better potentially
  str[0] = (char)(aMove.from[0] + 97); // 0 to 'a' etc
  str[1] = (char)(aMove.from[1] + 49); // 0 to '1' etc
  str[2] = (char)(aMove.to[0] + 97);   // 0 to 'a' etc
  str[3] = (char)(aMove.to[1] + 49);   // 0 to '1' etc
  str[4] = '\0';                       // and the terminator
  return (str);
}

// coverts entered move string to index format (must send addres to alter within structure)
void string2move(char *str, move_t *aMove)
{
  aMove->from[0] = (Int32)str[0] - 97; // 'a' to 0 etc
  aMove->from[1] = (Int32)str[1] - 49; // '1' to 0 etc
  aMove->to[0] = (Int32)str[2] - 97;   // 'a' to 0 etc
  aMove->to[1] = (Int32)str[3] - 49;
  // printf("\n s2m: %s %d %d %d %d",str,str[0],str[1],str[2],str[3]);
  // printf("\n then:  %d %d %d %d",aMove->from[0],aMove->from[1],aMove->to[0],aMove->to[1]);
}

// utility shows indices in a move_t structure
void printMove(move_t aMove)
{
  printf("\n move indices are (%d, %d) to (%d, %d)", aMove.from[0], aMove.from[1], aMove.to[0], aMove.to[1]);
}

// prints out a simple {Nf6} type indicator of move
void shortPrintMove(move_t aMove, position_t aBoard)
{
  //printf("(%d, %d) to (%d, %d)", aMove.from[0], aMove.from[1], aMove.to[0], aMove.to[1]);
  char astr[5];
  Int32 piece = aBoard->board[aMove.from[0]][aMove.from[1]];
  // turn black to white here
  piece = abs(piece);
  astr[0]=pieces[piece]; // for P, N,B,R,Q,K
  astr[1]=aMove.to[0] + 97; // file to a,b,c etc
  astr[2]=aMove.to[1] + 49;// rank to 1,2,3 ...
  astr[3]='\0';
  printf("{%s}",astr);
}



// changes the board according to a move. no checks made here. Fiddly stuff for castling, queening and e.p.
uint64_t doMove(position_t board1, move_t inMove, uint64_t inHash)
{
  uint64_t outHash = inHash;
  if (inMove.from[0] == -1 || inMove.from[1] == -1 || inMove.to[0] == -1 || inMove.to[1] == -1)
  {
    printf("\n warning invalid move");
    printMove(inMove);
    fflush(stdout);
    return (outHash);
  }
  // record last move changed anywhere
  lastMove = inMove;

  // get piece type on 'from' square
  Int32 piece = board1->board[inMove.from[0]][inMove.from[1]];

  if(piece == 0){
   printf("\n *** warning zero piece move ***\n");
   printMove(inMove);
   return(outHash);
  }

  // check if it's castling first (is a king and moves 2
  Int32 step = inMove.to[0] - inMove.from[0];
  Int32 absPiece = abs(piece);

  // note if it's a king move to prevent later castling
  if (absPiece == 6)
  {
    // if just about to lose castling righets, do the castling hash
    if (piece > 0 && castlingRights.whiteKingHasMoved == 0)
      outHash = outHash ^ WhiteCantCastle[0];

    if (piece < 0 && castlingRights.blackKingHasMoved == 0)
      outHash = outHash ^ BlackCantCastle[0];

    if (piece > 0) // if white
      castlingRights.whiteKingHasMoved = 1;
    else
      castlingRights.blackKingHasMoved = 1;
  }

  if (absPiece == 6 && abs(step) == 2)
  {
    uint64_t newhash = pieceHash[piece + 6][inMove.from[0]][inMove.from[1]]; // king, old square
    outHash = outHash ^ newhash;                                             // remove king from old square
    newhash = pieceHash[piece + 6][inMove.to[0]][inMove.to[1]];              // king, new square
    outHash = outHash ^ newhash;                                             // add king

    // put king into 'to' square
    board1->board[inMove.to[0]][inMove.to[1]] = piece;
    // set the 'from' square as vacant
    board1->board[inMove.from[0]][inMove.from[1]] = 0;
    // and move the indicated rook
    if (step > 0) // KR
    {
      Int32 piece2 = board1->board[7][inMove.from[1]]; // should be a rook
      board1->board[7][inMove.from[1]] = 0;            // set to a space
      board1->board[inMove.from[0] + 1][inMove.from[1]] = piece2;

      newhash = pieceHash[piece2 + 6][7][inMove.from[1]];                  // Rook old square
      outHash = outHash ^ newhash;                                         // remove Rook from old square
      newhash = pieceHash[piece2 + 6][inMove.from[0] + 1][inMove.from[1]]; // Rook new square
      outHash = outHash ^ newhash;                                         // add Rook to new square
    }
    else // QR
    {
      Int32 piece2 = board1->board[0][inMove.from[1]]; // should be a rook
      board1->board[0][inMove.from[1]] = 0;            // set to a space
      board1->board[inMove.from[0] - 1][inMove.from[1]] = piece2;

      newhash = pieceHash[piece2 + 6][0][inMove.from[1]];                  // Rook old square
      outHash = outHash ^ newhash;                                         // remove Rook from old square
      newhash = pieceHash[piece2 + 6][inMove.from[0] - 1][inMove.from[1]]; // Rook new square
      outHash = outHash ^ newhash;                                         // add Rook to new square
    }

    // update squares around king
    if (piece > 0)
      getKingArea(board1, WHITE);
    else
      getKingArea(board1, BLACK);

    return (outHash); // return
  }

  // if not castling, check for taking king!
  Int32 piece2 = board1->board[inMove.to[0]][inMove.to[1]];
  if (abs(piece2) == 6) // if a king
  {
    for (Int32 ii = 0; ii < 8; ++ii)
    {
      for (Int32 jj = 0; jj < 8; ++jj)
      {
        if (board1->board[ii][jj] * piece2 > 0) // condition for piece of same colour as the taken king
          board1->board[ii][jj] = 0;            // set it to space (ie remove piece)
      }
    }
    // best do it from scratch for this one
    outHash = hashFullPosition(board1);
  } // end if a taken king

  // if not castling or king taken, do simpler change
  // put it into 'to' square
  // for hashing
  uint64_t newhash;
  piece2 = board1->board[inMove.to[0]][inMove.to[1]];
  if (piece2 != 0) // remove old piece fro hash
  {
    newhash = pieceHash[piece2 + 6][inMove.to[0]][inMove.to[1]];
    outHash = outHash ^ newhash;
  }
  if (piece != 0) // remove current piece from old square hash, and add to new square
  {
    newhash = pieceHash[piece + 6][inMove.from[0]][inMove.from[1]]; // old
    outHash = outHash ^ newhash;

    newhash = pieceHash[piece + 6][inMove.to[0]][inMove.to[1]]; // to
    outHash = outHash ^ newhash;
  }

  board1->board[inMove.to[0]][inMove.to[1]] = piece;

  // set the 'from' square as vacant
  board1->board[inMove.from[0]][inMove.from[1]] = 0;

  // update king area if its a normal king move
  if (absPiece == 6)
  {
    // update squares around king
    if (piece > 0)
      getKingArea(board1, WHITE);
    else
      getKingArea(board1, BLACK);
  }

  // check for queening and en passant
  if (absPiece == 1) // if a pawn
  {
    // ** queening **
    if (piece > 0 && inMove.to[1] == 7)
    { // is white and on top rank for white
      board1->board[inMove.to[0]][inMove.to[1]] = 5;
      newhash = pieceHash[piece + 6][inMove.to[0]][inMove.to[1]]; // white pawn hash
      outHash = outHash ^ newhash;                                // pawn gone
      newhash = pieceHash[5 + 6][inMove.to[0]][inMove.to[1]];     // white queen
      outHash = outHash ^ newhash;                                // queen added
    }
    if (piece < 0 && inMove.to[1] == 0)
    { // is black and on top rank for black
      board1->board[inMove.to[0]][inMove.to[1]] = -5;
      newhash = pieceHash[piece + 6][inMove.to[0]][inMove.to[1]]; // black pawn hash
      outHash = outHash ^ newhash;                                // pawn gone
      newhash = pieceHash[-5 + 6][inMove.to[0]][inMove.to[1]];    // black queen
      outHash = outHash ^ newhash;                                // queen added
    }
  
    //  ** en-passant **
    // if took on same rank, then...
    if (inMove.from[1] == inMove.to[1])
    {
      piece2 = board1->board[inMove.to[0]][inMove.to[1]];
      newhash = pieceHash[piece2 + 6][inMove.to[0]][inMove.to[1]];    // taking pawn hash
      outHash = outHash ^ newhash;                                    // pawn gone
     // newhash = pieceHash[piece + 6][inMove.from[0]][inMove.from[1]]; // taking pawn hash
     // outHash = outHash ^ newhash;                                    // pawn gone

      // take pawn off 'sideways' square
      board1->board[inMove.to[0]][inMove.to[1]] = 0;
      // and add one up ('up' is colour dependent), and the required + or - step is equal to 'piece'
      board1->board[inMove.to[0]][inMove.to[1] + piece] = piece;

      newhash = pieceHash[piece + 6][inMove.to[0]][inMove.to[1] + piece]; // taking pawn hash
      outHash = outHash ^ newhash;                                        // taking pawn added

      // lastly mark en-passant as done with rank &  file hash
      newhash = enPassantHash[inMove.to[0]][inMove.to[1]];
      outHash = outHash ^ newhash; // En-passant marked on taken file & rank

      // and record for later hashing from scratch
      castlingRights.EPsquares.board[inMove.to[0]][inMove.to[1]] = 1;
    } 

  }

  /*
  uint64_t checkhash=hashFullPosition(board1);
  if (checkhash != outHash)
    checkhash = toggleWhiteToPlayHash(checkhash);
    // // note function is deliberately used with a dummy hash sometimes (0), don't trigger for them
  if(checkhash  != outHash && inHash != DUMMYhash) 
  {
     printf("DIFFERENT HASHES, last move:");
    printMove(inMove);
     printf("pce %d",piece0);
     showBoard1(board1);
   }
   else
    printf(" =");

    */

  return (outHash);
} // end doMove()

//  recursive -looks for best move at a branch. The basecase is on the last move..
// ... it does the same but avoids taking anything lower value unless undefended ..
// .. without this computer is grossly over-optimistic or pessimistic depending on ..
// .. who does the last move! ...
// getBestMove_alphaBeta_Ptakes() uses alpha-beta pruning to save searching lines already refuted.
Int32 getBestMove_alphaBeta_Ptakes(Int32 alpha, Int32 beta, position_t board1, move_t *inMove,
                                   Int32 CURRENTSIDE, Int32 depth, Int32 maxDepth, move_t *bestLine, uint64_t hashval)
{
  ++depth;                                // track the levels
  Int32 depthOfSearch = maxDepth - depth; // remaining levels for hash recording
  ++nodeCount;
  ++timingNodeCount;

  // retain the last move before now
  move_t lastMoveBefore = lastMove;
  // and the squares near the kings
  _position_t storeWKing = whiteKingArea, storeBKing = blackKingArea;
  castlingRights_t storeCastling = castlingRights;

  // change the hashvalue colour, and store
  hashval = toggleWhiteToPlayHash(hashval);
  uint64_t storeHash = hashval;
  // see if there's a match from earlier
  // taken out - possibly the cause of occasional suicide moves ..
  // ... as used without testing
  /*
  Int32 alphaVal;
  Int32 gotMatch = getEqualHashPosition(hashval, depthOfSearch, &alphaVal, inMove);
  if (gotMatch > 0)
  {
    return (alphaVal);
  }
    */

  move_t LevelsBestLine[deepestLine];
  move_t movesArray[maxPossMoves];
  Int32 currentVal;

  // base-case:  do last move and fully evaluate each (avoiding 'blind' takes)

  if (depth == (maxDepth))
  {
    
      alpha = evaluateHorizon_playSafe(alpha, beta, board1, inMove, CURRENTSIDE, depth, maxDepth, bestLine, lastMoveBefore, hashval);
    

    // restore king squares
    whiteKingArea = storeWKing;
    blackKingArea = storeBKing;
    castlingRights = storeCastling;
    if (inMove->from[0] > 0) // avoiding nullMove returns
      addHashPoint(hashval, alpha, depthOfSearch, inMove);
    return (alpha);
  } // end of base-case

  // **normal recursive procedure** : faster defining as objects rather than pointers with alloc/free
  _position_t startboard, nextDefended, attacked;

  // record the start
  copyPosition(board1, &startboard);

  // just to get which squares are attacked (opposing player's 'defended' are current players attacked
  // zero 'attacked'
  zeroPosition(&attacked);
  zeroPosition(&nextDefended);
  // then get the 'atacked' squares (needed to check if castling valid)
  getColoursMoves(board1, &nextDefended, &attacked, movesArray, &currentVal, CURRENTSIDE * -1, lastMoveBefore);
  // copy other player's defended to current players 'attacked'
  attacked = nextDefended;
  Int32 moveNum = getColoursMoves(board1, &nextDefended, &attacked, movesArray, &currentVal, CURRENTSIDE, lastMoveBefore);
  if (moveNum == 0) // no moves possible: checkmate as all pieces taken off
  {
    // restore king squares
    whiteKingArea = storeWKing;
    blackKingArea = storeBKing;
    castlingRights = storeCastling;
    return (-10000000); // might just be -1e6 regardless
  }

  prioritiseMoveOrder_Ptakes(movesArray, moveNum);
  // and check all the moves
  Int32 iiBest = -1;

  for (Int32 ii = 0; ii < moveNum; ++ii)
  {
    // pick next move
    move_t aMove = movesArray[ii];
    move_t keepMove = aMove;
    // do the move
    hashval = doMove(&startboard, aMove, hashval);

    // toggle side with *-1  Note aMove gets changed to the best from the next level
    Int32 diff_WmB = -getBestMove_alphaBeta_Ptakes(-beta, -alpha, &startboard, &aMove,
                                                   CURRENTSIDE * (-1), depth, maxDepth, LevelsBestLine, hashval);
    // set board back to original before next try, also the squares near the king
    copyPosition(board1, &startboard);
    whiteKingArea = storeWKing;
    blackKingArea = storeBKing;
    castlingRights = storeCastling;
    hashval = storeHash;

    if (depth == 1)
    {
      evaluatedMoves[ii] = movesArray[ii];
      evaluatedMoves[ii].evaluation = diff_WmB;
      evaluatedMoveNum = moveNum;
    }

    // **  the alpha-beta pruning method ** :
    // If score > beta then opposite side  wouldn't play it as better found, and ..
    // ... this side finds its best move (highest alpha)

    // if (score >= beta) return score; // Beta cut-off
    // if (score > alpha) {
    //    alpha = score;
    //  Record best move...

    if (diff_WmB >= beta)
    {
      // copy  move to inmove: not needed:- *inMove = movesArray[ii];
      whiteKingArea = storeWKing;
      blackKingArea = storeBKing;
      castlingRights = storeCastling;
      return (diff_WmB); // keep move?
    }
    if (diff_WmB > alpha)
    {
      alpha = diff_WmB;
      iiBest = ii;
      for (Int32 k = depth + 2; k <= maxDepth; ++k)
        bestLine[k] = LevelsBestLine[k];
      // both neened ([depth] to catch first move)
      bestLine[depth] = keepMove;
      bestLine[depth + 1] = aMove;
    }
  }

  // this should do check later

  if (iiBest >= 0)
    *inMove = movesArray[iiBest];

  whiteKingArea = storeWKing;
  blackKingArea = storeBKing;
  castlingRights = storeCastling;
  hashval = storeHash;
  addHashPoint(hashval, alpha, depthOfSearch, inMove);
  return (alpha);

} // end of getBestMove_alphaBeta_Ptakes()

//  _PhashFirst() is an advance over getBestMove_alphaBeta_Ptakes() as it stores and uses Zobrist hashes
// getBestMove_alphaBeta_Ptakes() uses alpha-beta pruning to save searching lines already refuted.
Int32 getBestMove_alphaBeta_PhashFirst(Int32 alpha, Int32 beta, position_t board1, move_t *inMove,
                                       Int32 CURRENTSIDE, Int32 depth, Int32 maxDepth, move_t *bestLine, uint64_t hashval)
{
  ++depth;                                // track the levels
  Int32 depthOfSearch = maxDepth - depth; // remaining levels for hash recording
  // retain the last move before now
  move_t lastMoveBefore = lastMove;
  // and the squares around the kings
  _position_t storeWKing = whiteKingArea, storeBKing = blackKingArea;
  castlingRights_t storeCastling = castlingRights;

  // change the hashvalue colour, and store
  hashval = toggleWhiteToPlayHash(hashval);
  uint64_t storeHash = hashval;

  // see if there's a match from earlier

  Int32 alphaVal;
  Int32 gotMatch = getEqualHashPosition(hashval, depthOfSearch, &alphaVal, inMove);
  if (gotMatch > 0)
  {
    return (alphaVal);
  }

  move_t LevelsBestLine[deepestLine];
  move_t movesArray[maxPossMoves];
  Int32 currentVal;

  // base-case:  do last move and fully evaluate each (avoiding 'blind' takes)

  if (depth >= (maxDepth))
  {
    // note the lastMove.status clause speeds it up dramatically! looks promising

      alpha = evaluateHorizon_playSafe(alpha, beta, board1, inMove, CURRENTSIDE, depth, maxDepth, bestLine, lastMoveBefore, hashval);

    // restore king squares
    whiteKingArea = storeWKing;
    blackKingArea = storeBKing;
    castlingRights = storeCastling;

    hashval = storeHash;
    if (inMove->from[0] > 0) // avoiding nullMove returns
      addHashPoint(hashval, alpha, depthOfSearch, inMove);
    return (alpha);
  } // end of base-case

  // **normal recursive procedure** :
  // faster defined as object rather than pointers and allocated/freed
  _position_t startboard, nextDefended, attacked;

  // record the start
  copyPosition(board1, &startboard);

  // just to get which squares are attacked (opposing player's 'defended' are current players attacked
  // zero 'attacked'
  zeroPosition(&attacked);
  zeroPosition(&nextDefended);
  //
  // then get the 'atacked' squares (needed to check if castling valid)
  getColoursMoves(board1, &nextDefended, &attacked, movesArray, &currentVal, CURRENTSIDE * -1, lastMoveBefore);
  // copy other player's defended to current players 'attacked'
  attacked = nextDefended;
  // get current moves
  Int32 moveNum = getColoursMoves(board1, &nextDefended, &attacked, movesArray, &currentVal, CURRENTSIDE, lastMoveBefore);

  if (moveNum == 0) // no moves possible: checkmate as all pieces taken off
  {
    // restore king squares
    whiteKingArea = storeWKing;
    blackKingArea = storeBKing;
    castlingRights = storeCastling;
    *inMove = nullMove;
    return (-10000000); // might just be -1e6 regardless
  }

  // sort in order, hashval, best line, killers, takes, remainder
  prioritiseMoveOrder_PhashFirstV2(movesArray, moveNum, depth, depthOfSearch, hashval);

  Int32 iiBest = -1;

  for (Int32 ii = 0; ii < moveNum; ++ii)
  {
    // pick next move
    move_t aMove = movesArray[ii];
    move_t keepMove = aMove;
    // do the move
    hashval = doMove(&startboard, aMove, hashval);

    Int32 diff_WmB;
    // Late move reduction
    Int32 reducedDepth = maxDepth;
    if (depth > 6 && ii > 6 && aMove.status < 2) // 80 and 10 seem safe for first 2
      --reducedDepth;
    // toggle side with *-1  Note aMove gets changed to the best from the next level
    diff_WmB = -getBestMove_alphaBeta_PhashFirst(-beta, -alpha, &startboard, &aMove,
                                                 CURRENTSIDE * (-1), depth, reducedDepth, LevelsBestLine, hashval);
    // in case it looks good, need to do at full depth
    if (diff_WmB > alpha && reducedDepth < maxDepth)
    {
      // restore situation
      whiteKingArea = storeWKing;
      blackKingArea = storeBKing;
      castlingRights = storeCastling;
      hashval = storeHash;
      aMove = keepMove;
      copyPosition(board1, &startboard);
      hashval = doMove(&startboard, aMove, hashval);
      diff_WmB = -getBestMove_alphaBeta_PhashFirst(-beta, -alpha, &startboard, &aMove,
                                                   CURRENTSIDE * (-1), depth, maxDepth, LevelsBestLine, hashval);
    }
    // set board back to original before next try
    copyPosition(board1, &startboard);
    // restore king squares
    whiteKingArea = storeWKing;
    blackKingArea = storeBKing;
    castlingRights = storeCastling;
    hashval = storeHash;

    if (depth == 1)
    {
      evaluatedMoves[ii] = movesArray[ii];
      evaluatedMoves[ii].evaluation = diff_WmB;
      evaluatedMoveNum = moveNum;
    }

    // **  the alpha-beta pruning method ** :

    if (diff_WmB >= beta)
    {
      // copy  move to inmove: not needed:- *inMove = movesArray[ii];
      addKillerMove(movesArray[ii], depth);
      // addHashPoint(hashval,diff_WmB,depthOfSearch,inMove);
      // restore king squares
      whiteKingArea = storeWKing;
      blackKingArea = storeBKing;
      castlingRights = storeCastling;
      *inMove = movesArray[ii];
      return (diff_WmB); // keep move?
    }
    if (diff_WmB > alpha)
    {
      alpha = diff_WmB;

      // following produces some weird and sometime suicidal moves!!
      // addHashPoint(hashval,alpha,depthOfSearch,inMove);

      iiBest = ii;
      for (Int32 k = depth + 1; k <= maxDepth; ++k)
        bestLine[k] = LevelsBestLine[k];
      // both neened ([depth] to catch first move)
      bestLine[depth] = keepMove;
      bestLine[depth + 1] = aMove;
    }
  }

  // this should do check later

  if (iiBest >= 0)
    *inMove = movesArray[iiBest];
  // restore king squares
  whiteKingArea = storeWKing;
  blackKingArea = storeBKing;
  castlingRights = storeCastling;
  hashval = storeHash;
  addHashPoint(hashval, alpha, depthOfSearch, inMove);
  return (alpha);
} // end of getBestMove_alphaBeta_PhashFirst()

//  The final one, for now! As getBestMove_alphaBeta_PhashFirst() but it checks the time occasionally
// and bales out when limit exceeded so the last depth fully analysed can be used.
Int32 getBestMove_alphaBeta_timeoutPhashFirst(Int32 alpha, Int32 beta, position_t board1, move_t *inMove,
                                              Int32 CURRENTSIDE, Int32 depth, Int32 maxDepth, move_t *bestLine, uint64_t hashval, struct timespec start1, struct timespec end1)
{
  // for fast abort on timeout
  if (TimeOut == 1)
    return (TIMEOUTFLAG);
  ++nodeCount;
  ++timingNodeCount;

  if (timingNodeCount > 10000) // if 10000 nodes done
  {
    timingNodeCount = 0; // zero counter
    // check clock for timeout
    clock_gettime(CLOCK_REALTIME, &end1);
    double secs = (double)(end1.tv_sec - start1.tv_sec) + (1e-9) * (double)(end1.tv_nsec - start1.tv_nsec);
    // peak of 2 x the average seems about right for getting the average, without annoying waits
    if ((LIMITMODE == TARGETMAXTIME && secs > 2.0 * FLTKavgMoveTime) ||
        (LIMITMODE == ALLMOVESTIME && secs > 2.0 * timeoutTimeAllMoves))
    {
      TimeOut = 1; // should all return quickly now


      return (TIMEOUTFLAG);
    }


  }

  ++depth;                                // track the levels
  Int32 depthOfSearch = maxDepth - depth; // remaining levels for hash recording
  // retain the last move before now
  move_t lastMoveBefore = lastMove;
  // and the squares around the kings
  _position_t storeWKing = whiteKingArea, storeBKing = blackKingArea;
  castlingRights_t storeCastling = castlingRights;

  // change the hashvalue colour, and store
  hashval = toggleWhiteToPlayHash(hashval);
  uint64_t storeHash = hashval;
  move_t LevelsBestLine[deepestLine];
  move_t movesArray[maxPossMoves];
  Int32 currentVal;

  // see if there's a equal depth match from earlier

  // following uses a stored move without further testing -
  // ... taken out 8/8/26 as possibly the cause of very occasional suicide moves?
  // tried it again after hash bug fix - no sign its faster than without, so commented out
  // .. possible better after change to split mix randoms, but not obvious...
  
  /*
  Int32 alphaVal;
  Int32 gotMatch = getEqualHashPosition(hashval, depthOfSearch, &alphaVal, inMove);
  if (gotMatch > 0)
  {
    if(board1->board[inMove->from[0]][inMove->from[1]] >0)
      return (alphaVal);
  } */
 



  // base-case:  do last move and fully evaluate each (avoiding 'blind' takes)

  if (depth >= (maxDepth))
  {
    // note the lastMove.status clause speeds it up dramatically! looks promising
    if (horizonMethod == HorizonShootOut && lastMove.status >= 2)
      alpha = evaluateHorizon_shootOut(alpha, beta, board1, inMove, CURRENTSIDE, depth, maxDepth, bestLine, lastMoveBefore, hashval,  start1, end1); // not yet working!
    else
    { // works well for playsafe, probs also for 'shootOut' version -no ...
      if(depth >= 0) { //dodgy sometimes for some unknown reason, switched off
      Int32 alphaVal;
      Int32 gotMatch = getEqualHashPosition(hashval, depthOfSearch, &alphaVal, inMove);
      if (gotMatch > 0)
        return (alphaVal);
      }
      alpha = evaluateHorizon_playSafe(alpha, beta, board1, inMove, CURRENTSIDE, depth, maxDepth, bestLine, lastMoveBefore, hashval);

    }
    // restore king squares
    whiteKingArea = storeWKing;
    blackKingArea = storeBKing;
    castlingRights = storeCastling;

    hashval = storeHash;

    //if (inMove->from[0] > 0) // avoiding nullMove returns, though commenting this 'if' line is interesting
    //if(( inMove->from[0] >=0) &&   (board1->board[inMove->from[0]][inMove->from[1]] !=0))
      addHashPoint(hashval, alpha, depthOfSearch, inMove);
    return (alpha);
  } // end of base-case

  // **normal recursive procedure** :
  // faster defined as object rather than pointers and allocated/freed
  _position_t startboard, nextDefended, attacked;

  // record the start
  copyPosition(board1, &startboard);

  // just to get which squares are attacked (opposing player's 'defended' are current players attacked
  // zero 'attacked'
  zeroPosition(&attacked);
  zeroPosition(&nextDefended);
  //
  // calculate the 'other' players moves first ...
  // ... this is to get the 'atacked' squares (needed to check if castling valid)
  getColoursMoves(board1, &nextDefended, &attacked, movesArray, &currentVal, CURRENTSIDE * -1, lastMoveBefore);
  // copy other player's defended to current players 'attacked'
  attacked = nextDefended;
  // get moves for 'this' player
  Int32 moveNum = getColoursMoves(board1, &nextDefended, &attacked, movesArray, &currentVal, CURRENTSIDE, lastMoveBefore);

  if (moveNum == 0) // no moves possible: checkmate as all pieces taken off, or just possibly stalemate
  {
    // restore king squares
    whiteKingArea = storeWKing;
    blackKingArea = storeBKing;
    castlingRights = storeCastling;
    *inMove = nullMove;
    return (-10000000); // might just be -1e6 regardless
  }

  // sort in order, hashval, killers, winning takes, other takes, remainder
  prioritiseMoveOrder_PhashFirstV2(movesArray, moveNum, depth, depthOfSearch, hashval);

  Int32 iiBest = -1;

  for (Int32 ii = 0; ii < moveNum; ++ii)
  {
    // pick next move
    move_t aMove = movesArray[ii];
    move_t keepMove = aMove;
    // do the move
    hashval = doMove(&startboard, aMove, hashval);

    Int32 diff_WmB;
    // Late move reduction
    Int32 reducedDepth = maxDepth;

    //  ** reduced depth stuff ** for non-taking move
    // is promising with 'break' for depth, but can go awry
    // was depth > 6 for 'on'
    // if (depth > 6 && ii > 4 && aMove.status < 2) // 80 and 10 seem safe for first 2
    //    break;
    // reducedDepth-=1;

    // toggle side with *-1  Note aMove gets changed to the best from the next level
    diff_WmB = -getBestMove_alphaBeta_timeoutPhashFirst(-beta, -alpha, &startboard, &aMove,
                                                        CURRENTSIDE * (-1), depth, reducedDepth, LevelsBestLine, hashval, start1, end1);
    // in case it looks good, need to do at full depth

    // timeout short-circuit
    if (diff_WmB == TIMEOUTFLAG)
      return (TIMEOUTFLAG);

    // set board, and everything else, back to state before move
    copyPosition(board1, &startboard);
    whiteKingArea = storeWKing;
    blackKingArea = storeBKing;
    castlingRights = storeCastling;
    hashval = storeHash;

    if (depth == 1)
    {
      evaluatedMoves[ii] = movesArray[ii];
      evaluatedMoves[ii].evaluation = diff_WmB;
      evaluatedMoveNum = moveNum;
    }

    // **  the alpha-beta pruning method ** :

    if (diff_WmB >= beta)
    {
      addKillerMove(movesArray[ii], depth);
      // restore king squares
      whiteKingArea = storeWKing;
      blackKingArea = storeBKing;
      castlingRights = storeCastling;
      // store hash
      *inMove = movesArray[ii];
      hashval = storeHash;
     // if(( inMove->from[0] >=0) &&   (board1->board[inMove->from[0]][inMove->from[1]] !=0))
        addHashPoint(hashval, diff_WmB, depthOfSearch, inMove);

      return (diff_WmB); // keep move?
    }
    if (diff_WmB > alpha)
    {
      alpha = diff_WmB;

   
      iiBest = ii;
      for (Int32 k = depth + 1; k <= maxDepth; ++k)
        bestLine[k] = LevelsBestLine[k];
      // both neened ([depth] to catch first move)
      bestLine[depth] = keepMove;
      bestLine[depth + 1] = aMove;
    }
  }

  // this should do check later

  if (iiBest >= 0)
    *inMove = movesArray[iiBest];
  // restore king squares
  whiteKingArea = storeWKing;
  blackKingArea = storeBKing;
  castlingRights = storeCastling;
  hashval = storeHash;
  // need this as very occasionaly get a move with no piece at from - v puzzling!
  if(( inMove->from[0] >=0) &&   (board1->board[inMove->from[0]][inMove->from[1]] !=0))
    addHashPoint(hashval, alpha, depthOfSearch, inMove);
  return (alpha);
} // end of getBestMove_alphaBeta_timeoutPhashFirst()

//  recursive -looks for best move at a branch. The basecase is on the last move..
// ... it does the same but avoids taking smaller pieces as response not checked...
// does no alpha-beta pruning, so brute force and generally too slow
Int32 getBestMove_noPrune(position_t board1, move_t *inMove,
                          Int32 CURRENTSIDE, Int32 depth, Int32 maxDepth, move_t *bestLine, uint64_t hashval)
{
  ++depth;                                // track the levels
  Int32 depthOfSearch = maxDepth - depth; // remaining levels for hash recording
  // record last move at this point
  move_t lastMoveBefore = lastMove;
  // and the squares around the kings
  _position_t storeWKing = whiteKingArea, storeBKing = blackKingArea;
  castlingRights_t storeCastling = castlingRights;
  // change the hashvalue colour, and store
  hashval = toggleWhiteToPlayHash(hashval);
  uint64_t storeHash = hashval;

  Int32 alphaVal;
  Int32 gotMatch = getEqualHashPosition(hashval, depthOfSearch, &alphaVal, inMove);
  if (gotMatch > 0)
  {
    return (alphaVal);
  }

  move_t LevelsBestLine[deepestLine];

  Int32 whiteVal, blackVal;
  move_t movesArray[maxPossMoves], dummyMoves[maxPossMoves];
  position_t startboard = (position_t)calloc(1, sizeof(_position_t));
  position_t attacked = (position_t)calloc(1, sizeof(_position_t));

  Int32 currentVal;
  char tempstr[20];

  // base-case:  do last move and fully evaluate each (avoiding 'blind' takes)
  if (depth == (maxDepth))
  {
    position_t defended = (position_t)calloc(1, sizeof(_position_t));
    // controllable print of moves
    Int32 showBCmoves = 0;

    // record the start
    copyPosition(board1, startboard);

    // get the posible moves -
    if (showBCmoves == 1)
    {
      printf("\n b-c board to show depth %d", depth);
      showBoard1(board1);
    }
    zeroPosition(attacked);
    zeroPosition(defended);
    // then get the 'atacked' squares (needed to check if castling valid)
    getColoursMoves(board1, defended, attacked, movesArray, &currentVal, CURRENTSIDE * -1, lastMoveBefore);
    // copy other player's defended to current players 'attacked'
    *attacked = *defended;
    Int32 moveNum = getColoursMoves(board1, defended, attacked, movesArray, &currentVal, CURRENTSIDE, lastMoveBefore);
    // must copy back proper defended
    *defended = *attacked;

    // and check all the moves
    Int32 best = -100000000;
    Int32 iiBest = -1;
    if (showBCmoves == 1)
      printf("\n level %d \n moves: { ", depth);

    for (Int32 ii = 0; ii < moveNum; ++ii)
    {
      // here check if a move is a defended take. dont do it unless higher value as on 'horizon'
      // pick next move, and find target square and what's there
      move_t aMove = movesArray[ii];
      Int32 pieceTaken = board1->board[aMove.to[0]][aMove.to[1]];
      Int32 takenVal = values[abs(pieceTaken)];
      Int32 Mypiece = board1->board[aMove.from[0]][aMove.from[1]];
      Int32 takingVal = values[abs(Mypiece)];

      Int32 fairTake = 0;
      // empty square - no issue
      if (pieceTaken == 0)
        fairTake = 1;
      // worth more: would likely be best to take it
      if ((takenVal - takingVal - 500) > 0)
        fairTake = 1;

      // or if not defended
      if (defended->board[aMove.to[0]][aMove.to[1]] == 0)
        fairTake = 1;

      if (fairTake == 1)
      {
        // do the move
        hashval = doMove(startboard, aMove, hashval);
        // assess result for white THEN black
        Int32 NEXTMOVE = CURRENTSIDE * -1;
        evaluateBoard(startboard, dummyMoves, &whiteVal, WHITE, NEXTMOVE, lastMoveBefore);
        evaluateBoard(startboard, dummyMoves, &blackVal, BLACK, NEXTMOVE, lastMoveBefore);
        // add something for next move aims to discourage wasted moves (eg Nb5...)
        Int32 diff = whiteVal - blackVal + 100 * CURRENTSIDE;
        if (showBCmoves == 1)
        {
          move2string(aMove, tempstr);
          printf(" %s %d,", tempstr, diff);
        }
        // printf("\n level %d %ld move: %s eval %f %f %f", depth, ii, tempstr, diff, whiteVal, blackVal);

        if (diff * CURRENTSIDE > best)
        {
          best = diff * CURRENTSIDE;
          iiBest = ii;
          // store to best line array
          LevelsBestLine[depth + 1] = aMove;
        }

        // set board back to original before next try
        copyPosition(board1, startboard);
        // restore king squares
        whiteKingArea = storeWKing;
        blackKingArea = storeBKing;
        castlingRights = storeCastling;
        hashval = storeHash;

      } // end if valid
    }
    if (showBCmoves == 1)
    {
      printf(" }");
      printf("\n num of possible moves = %ld", (int64_t)moveNum);
    }

    if (iiBest < 0)
    {
      // printf("\n b-c ERROR iiBest = -ve, depth =%d best = %f - exiting", depth, best);
      // printf("\n CURRENTSIDE = %d moves = %ld", CURRENTSIDE, moveNum);
      // showBoard1(board1);
      // exit(0);
      Int32 NEXTMOVE = CURRENTSIDE * -1;
      evaluateBoard(startboard, dummyMoves, &whiteVal, WHITE, NEXTMOVE, lastMoveBefore);
      evaluateBoard(startboard, dummyMoves, &blackVal, BLACK, NEXTMOVE, lastMoveBefore);
      Int32 diff = whiteVal - blackVal;
      // restore king squares
      whiteKingArea = storeWKing;
      blackKingArea = storeBKing;
      castlingRights = storeCastling;
      hashval = storeHash;
      return (diff);
    }

    // copy best move to inmove, includes the status flag
    *inMove = movesArray[iiBest];

    // get best back into +/- format
    free(startboard);
    free(defended);
    // restore king squares
    whiteKingArea = storeWKing;
    blackKingArea = storeBKing;
    castlingRights = storeCastling;
    hashval = storeHash;

    addHashPoint(hashval, best * CURRENTSIDE, depthOfSearch, inMove);
    return (best * CURRENTSIDE);

  } // end of base-case

  // normal recursive procedure

  position_t nextDefended = (position_t)calloc(1, sizeof(_position_t));
  // conditional printout
  Int32 showMAINmoves = 0;
  if (depth == 1)
    showMAINmoves = 1;

  // record the start
  copyPosition(board1, startboard);

  zeroPosition(attacked);
  Int32 moveNum = getColoursMoves(board1, nextDefended, attacked, movesArray, &currentVal, CURRENTSIDE, lastMoveBefore);

  // and check all the moves
  Int32 best = -1000000;
  Int32 iiBest = -1;

  if (showMAINmoves == 1)
    printf("\n level %d main, side: %d maxDepth %d\n moves: { ", depth, CURRENTSIDE, maxDepth);
  for (Int32 ii = 0; ii < moveNum; ++ii)
  {
    // pick next move
    move_t aMove = movesArray[ii];
    move_t keepMove = aMove;
    // do the move
    hashval = doMove(startboard, aMove, hashval);

    // toggle side with *-1  Note aMove gets changed to the best from the next level
    Int32 diff_WmB = getBestMove_noPrune(startboard, &aMove,
                                         CURRENTSIDE * (-1), depth, maxDepth, LevelsBestLine, hashval);

    if (showMAINmoves == 1)
    {
      move2string(movesArray[ii], tempstr);
      printf(" %s %d,", tempstr, diff_WmB);
    }
    // record the best move for current side (most +ve diff_WmB for white, most -ve for black)
    if (diff_WmB * CURRENTSIDE > best)
    {
      best = diff_WmB * CURRENTSIDE;
      iiBest = ii;
      // store 'upstream' to best line array
      for (Int32 k = depth + 2; k <= maxDepth; ++k)
        bestLine[k] = LevelsBestLine[k];
      // both neened ([depth] to catch first move)
      bestLine[depth] = keepMove;
      bestLine[depth + 1] = aMove;
    }

    // set board back to original before next try
    copyPosition(board1, startboard);
    // restore king squares
    whiteKingArea = storeWKing;
    blackKingArea = storeBKing;
    castlingRights = storeCastling;
    hashval = storeHash;
  }

  if (showMAINmoves == 1)
  {
    printf(" }");
    printf("\n num of moves = %ld", (int64_t)moveNum);
  }

  if (iiBest < 0)
  {
    Int32 NEXTMOVE = CURRENTSIDE * -1;
    evaluateBoard(startboard, dummyMoves, &whiteVal, WHITE, NEXTMOVE, lastMoveBefore);
    evaluateBoard(startboard, dummyMoves, &blackVal, BLACK, NEXTMOVE, lastMoveBefore);
    Int32 diff = whiteVal - blackVal;
    return (diff);
    // printf("\n ERROR iiBest = -ve depth = %d, best = %f - exiting", depth, best);
    // exit(0);
  }

  // copy best move to inmove
  *inMove = movesArray[iiBest];

  // get best back into +/- format
  free(startboard);
  free(nextDefended);
  // restore king squares
  whiteKingArea = storeWKing;
  blackKingArea = storeBKing;
  castlingRights = storeCastling;
  hashval = storeHash;
  addHashPoint(hashval, best * CURRENTSIDE, depthOfSearch, inMove);
  return (best * CURRENTSIDE);
}

// simple copy, not relly needed as only 1 line needed
void copyPosition(position_t boarda, position_t boardb)
{
  *boardb = *boarda; // copy the lot in one go
}

// deals with any 'control' entries, but mostly converts familiar nxe5 format to square index format
Int32 parseAlgebraicString(position_t board1, char *instring, move_t *movesArray, Int32 currentMoves, move_t *inMove,
                           Int32 *messageFlag, Int32 CURRENTSIDE)
{
  char tmpStr[40];
  char originalString[50];
  strcpy(originalString, instring);
  move_t tmpMove;
  *messageFlag = 0; // reset needed for some options
  Int32 len = strlen(instring);
  // convert all upper case characters to lower case
  strToLower(instring);

  // first do the 'controls'
  if (strcmp(instring, "q") == 0) // quit program
  {
    *messageFlag = 1;
    return (0);
  }
  if (strcmp(instring, "s") == 0) // message to toggle
  {
    printf("\n ********* swapping sides ********\n");
    *messageFlag = 2;
    return (0);
  }
  if (strcmp(instring, "h") == 0) // gives help info
  {
    *messageFlag = 3;
    return (0);
  }
  if (strcmp(instring, "cc") == 0) // for computer plays computer
  {
    *messageFlag = 4;
    return (0);
  }
  if (strcmp(instring, "w") == 0) // for human as white
  {
    *messageFlag = 5;
    return (0);
  }
  if (strcmp(instring, "b") == 0) // ...
  {
    *messageFlag = 6;
    return (0);
  }
  if (strcmp(instring, "+") == 0) // increase search depth by 1, or time by *2
  {
    *messageFlag = 7;
    return (0);
  }
  if (strcmp(instring, "-") == 0) // decrease search depth by 1, or time by 2
  {
    *messageFlag = 8;
    return (0);
  }
  if (strcmp(instring, "neg") == 0)
  {
    *messageFlag = 9;
    return (0);
  }
  if (strcmp(instring, "u") == 0)
  {
    *messageFlag = 10;
    return (0);
  }
  if (strcmp(instring, "t") == 0)
  {
    *messageFlag = 11;
    return (0);
  }
  if (strcmp(instring, "nb") == 0) // no book openings
  {
    *messageFlag = 12;
    return (0);
  }
  if (strcmp(instring, "hz") == 0) // no book openings
  {
    *messageFlag = 13;
    return (0);
  }

  ///// should be an intended move now /////

  // first check for castling
  // avoid 'not set' warnings
  tmpMove.status = 0;

  while (1) // pseudo while loop to allow use of break command
  {
    if (strcmp(instring, "o-o") == 0 || strcmp(instring, "0-0") == 0)
    {
      if (CURRENTSIDE == WHITE)
        strcpy(instring, "e1g1"); // UCI-ish code for W K-S castling
      else
        strcpy(instring, "e8g8"); // and for black
      break;                      // as its a pseudo while
    } // end if o-o

    if (strcmp(instring, "o-o-o") == 0 || strcmp(instring, "0-0-0") == 0)
    {
      if (CURRENTSIDE == WHITE)
        strcpy(instring, "e1c1"); // UCI-ish code for W K-S castling
      else
        strcpy(instring, "e8c8"); // and for black
      break;
    } // end if o-o-o

    // first take care of 'exd' types -> convert to 'ed'
    if (len == 3 && instring[1] == 'x')
    {
      instring[1] = instring[2]; //
      instring[2] = '\0';
      len = 2; // must drop this one
    }

    if (len == 2) // must be a shorthand pawn move eg e4 or ed
    {
      if (isdigit((int)instring[1]))
      {
        // target of form 'e4' serch known moves for this in the to position
        strcpy(tmpStr, instring);
        strcat(tmpStr, instring);            // for e4e4
        tmpStr[1] = tmpStr[1] - CURRENTSIDE; // for trying e3e4

        string2move(tmpStr, &tmpMove);
        if (verbose > 0)
          printMove(tmpMove);
        if (isWithinMoves(&tmpMove, movesArray, currentMoves) == 1)
        { // gave it
          strcpy(instring, tmpStr);
          break;
        }
        tmpStr[1] = tmpStr[1] - CURRENTSIDE; // for trying e2e4
        string2move(tmpStr, &tmpMove);
        if (verbose > 0)
          printMove(tmpMove);
        if (isWithinMoves(&tmpMove, movesArray, currentMoves) == 1)
        { // gave it

          strcpy(instring, tmpStr);
          break;
        }

      } // end if isdigit
      else // must be of form ed
      {
        // so run up the 'e' file looking for a pawn, no need o check colours as only correct ones in moves
        tmpStr[0] = instring[0];
        tmpStr[1] = '1';
        tmpStr[2] = instring[1];
        tmpStr[3] = '1';
        tmpStr[4] = '\0';

        Int32 gotOne = 0;
        for (Int32 i1 = 0; i1 < 8; ++i1)
        {
          for (Int32 i2 = 0; i2 < 8; ++i2)
          {
            string2move(tmpStr, &tmpMove);
            // check both are pawns of the required opposite colours
            if (board1->board[tmpMove.from[0]][tmpMove.from[1]] == CURRENTSIDE &&
                board1->board[tmpMove.to[0]][tmpMove.to[1]] == -CURRENTSIDE)
            {
              if (isWithinMoves(&tmpMove, movesArray, currentMoves))
              { // gave it

                strcpy(instring, tmpStr);
                gotOne = 1;
                break;
              } // end if a realmove
            } // end if both pawns

            tmpStr[3] = tmpStr[3] + 1;
          }
          tmpStr[3] = '1'; // reset
          tmpStr[1] = tmpStr[1] + 1;
          if (gotOne == 1)
            break;
        }

      } //
    } // end if len = 2

    // len = 3 will be most, eg nf3 bc4 etc, "exd" dealt with earlier (converted to ed)
    // change 'nxd5' to nd5, then dealt with by len==3
    if (len == 4 && instring[1] == 'x')
    {
      instring[1] = instring[2]; // take out the 'x'
      instring[2] = instring[3];
      instring[3] = '\0';
      len = 3; // must drop this one
    }
    if (len == 3)
    { // last 2 digits must contain target square here
      tmpStr[2] = instring[1];
      tmpStr[3] = instring[2];
      tmpStr[4] = '\0';
      // get piece doing it
      char algPiece = instring[0]; // 'p', 'n' etc
      Int32 piece = 99;            // avoids matching empty square
      if (algPiece == 'p')
        piece = 1;
      if (algPiece == 'n')
        piece = 2;
      if (algPiece == 'b')
        piece = 3;
      if (algPiece == 'r')
        piece = 4;
      if (algPiece == 'q')
        piece = 5;
      if (algPiece == 'k')
        piece = 6;

      // printf("\n piece %d", piece);
      //  now find all such pieces on the board
      piece *= CURRENTSIDE; // may be black
      Int32 gotOne = 0;
      for (Int32 ii = 0; ii < 8; ++ii)
      {
        for (Int32 jj = 0; jj < 8; ++jj)
        {
          if (board1->board[ii][jj] == piece)
          {
            string2move(tmpStr, &tmpMove); // get the 'to' bit, the from bit will be random
            tmpMove.from[0] = ii;          // the 'from' bit
            tmpMove.from[1] = jj;
            // printf("\n here");printMove(tmpMove);
            if (isWithinMoves(&tmpMove, movesArray, currentMoves)) // is it a real move?
            {                                                      // gave it
              // this time translate it to internal string form for later simple processing
              move2string(tmpMove, instring);
              gotOne = 1;
              break;
            } // end if a realmove
          }
          if (gotOne == 1)
            break;
        } // end for jj
        if (gotOne == 1)
          break;
      } // end for ii

      // To deal with *ambiguity of bishops and b-file pawns*
      // eg bc3 coulp meab pawn on b-file x C3 or it could mean Bishop x C3
      // if the Bishop assumption didnt work then must be pawn .. (or a typo!)
      if (gotOne == 0 && abs(piece) == 3) // ie if didnt find one for the bishop
      {
        // replace the 'bishop' of bc3 with the bishops pawn, ie pbc3, and let len==4 deal with it
        instring[4] = instring[3]; // the '\0'
        instring[3] = instring[2]; // the '3'
        instring[2] = instring[1]; // the 'c'
        instring[1] = instring[0]; // the 'b'
        instring[0] = 'p';         // and add the pawn prefix
        len = 4;                   // if(len==4) now has a go
      }

    } // end if len==3

    // note 'x' removed earler so 'nxe5' is now just 'ne5'. len==4 must deal with ..
    // ... n3d5 or nbd5 (n3xb5  now changed to n3b3 etc
    if (len == 5 && instring[2] == 'x')
    {
      instring[2] = instring[3]; // take out the 'x'
      instring[3] = instring[4];
      instring[4] = '\0';
      len = 4; // must drop this one
    }
    if (len == 4) // for rank-or-file-specified moves like 'n3d5' or 'nbd3'
    {
      // last 2 digits must contain target square
      tmpStr[2] = instring[2];
      tmpStr[3] = instring[3];
      tmpStr[4] = '\0';
      // get piece doing it
      char algPiece = instring[0]; // 'p', 'n' etc
      Int32 piece = 99;            // avoids matching empty square
      if (algPiece == 'p')
        piece = 1;
      if (algPiece == 'n')
        piece = 2;
      if (algPiece == 'b')
        piece = 3;
      if (algPiece == 'r')
        piece = 4;
      if (algPiece == 'q')
        piece = 5;
      if (algPiece == 'k')
        piece = 6;
      // get rank or file it comes from
      char specifier = instring[1]; // could be a-h or 1-8
      // printf("\n instring char %s %c", instring, specifier);
      Int32 type = 1; // default for is a letter here (presumed 'a' - 'h')
      if (isdigit((int)specifier))
        type = 0; // so presumed a digit character, '1 - '8'
      Int32 iiOrigin = 0, jjOrigin = 0;
      if (type == 0)                      // a digit (rank)
        jjOrigin = (Int32)specifier - 49; // eg '1' to 0
      if (type == 1)                      // a letter (file)
        iiOrigin = (Int32)specifier - 97; // eg 'a' to 0

      // now do as before, but with an extra check on the origin rank or file
      // now find all such pieces on the board
      piece *= CURRENTSIDE; // may be black
      Int32 gotOne = 0;
      for (Int32 ii = 0; ii < 8; ++ii)
      {
        for (Int32 jj = 0; jj < 8; ++jj)
        {
          // extra if , checking for specifier
          if ((type == 0 && jj == jjOrigin) || (type == 1 && ii == iiOrigin))
          {
            if (board1->board[ii][jj] == piece)
            {
              string2move(tmpStr, &tmpMove); // get the 'to' bit, the from bit will be random
              tmpMove.from[0] = ii;          // the 'from' bit
              tmpMove.from[1] = jj;
              // printf("\n here specified");
              if (verbose > 0)
                printMove(tmpMove);
              if (isWithinMoves(&tmpMove, movesArray, currentMoves)) // is it a real move?
              {                                                      // passed it
                // this time translate it to internal string form for later simple processing
                move2string(tmpMove, instring);
                gotOne = 1;
                break;
              } // end if a realmove
            }
          } // end if origin correct
          if (gotOne == 1)
            break;
        } // end for jj
        if (gotOne == 1)
          break;
      } // end for ii

    } // end if len ==4

    break; // as its a pseudo while
  } // end pseudo-while

  // hopefully now a valid move - check against possibles
  // printf("\n board moves match is %s  status: %d", instring, tmpMove.status);
  string2move(instring, inMove);
  if (verbose > 0)
    printMove(*inMove);
  Int32 valid = 0;
  for (Int32 ii = 0; ii < currentMoves; ++ii)
  {
    if (inMove->from[0] == movesArray[ii].from[0] && inMove->from[1] == movesArray[ii].from[1] &&
        inMove->to[0] == movesArray[ii].to[0] && inMove->to[1] == movesArray[ii].to[1])
    {
      valid = 1;
      *inMove = movesArray[ii]; // gets the status string
                                // printf("\n matches move %ld ok , status %d", (int64_t)ii, inMove->status);
    }
  }

  // print possible moves if no match (e.p not yet implemented)
  if (valid == 0)
  {
    printf("\n ******* MOVE \"%s\" not RECOGNISED. Valid moves are: \n {", instring);
    char tempstr[20];
    for (Int32 ii = 0; ii < currentMoves; ++ii)
    {
      move2string(movesArray[ii], tempstr);
      printf("%s, ", tempstr);
    }
    printf("}  \n");
    printf("\n ** ** original input was \"%s\" ** ** \n", originalString);
    *messageFlag = 20;
  }

  return (valid); // returns 1 if move good, else 0
} // end parseAlgebraicString()

// changes a string's upper case to lower case, and returns it - used to simplify parsing routing
char *strToLower(char *instring)
{
  Int32 len = strlen(instring);
  for (Int32 ii = 0; ii < len; ++ii)
    instring[ii] = (char)tolower(instring[ii]);
  return (instring);
} // end strToLower()

// checks a parsed move is within the list found by getColoursMoves(), returns '1' if so and returns it with the .status calculated
Int32 isWithinMoves(move_t *tmpMove, move_t *movesArray, Int32 currentMoves)
{
  Int32 valid = 0;
  for (Int32 ii = 0; ii < currentMoves; ++ii)
  {
    if (tmpMove->from[0] == movesArray[ii].from[0] && tmpMove->from[1] == movesArray[ii].from[1] &&
        tmpMove->to[0] == movesArray[ii].to[0] && tmpMove->to[1] == movesArray[ii].to[1])
    {
      valid = 1;
      *tmpMove = movesArray[ii];
      break;
    }
  }

  return (valid);
}



// write moves individually to a .txt file
void writeMove(FILE *fptr, position_t board1, move_t inMove, Int32 moveNumber)
{
  char str[20];
  if (moveNumber % 2 == 1) // if mve 1, 3, 5 ...
  {
    fprintf(fptr, "\n %ld \t", (int64_t)moveNumber / 2 + 1);
  }
  // get piece
  Int32 ii = inMove.from[0];
  Int32 jj = inMove.from[1];
  Int32 pieceIndex = board1->board[ii][jj];
  str[0] = pieces[abs(pieceIndex)]; // put symbol in op string
  str[1] = '\0';
  Int32 status = inMove.status;
  if ((status & 2) > 0) // AND with bit 1, checking for a take
    strcat(str, "x");
  // then get destination square
  Int32 ii2 = inMove.to[0];
  Int32 jj2 = inMove.to[1];
  Int32 len = strlen(str);
  str[len] = (char)(ii2 + 97);     // eg 0-> 'a'
  str[len + 1] = (char)(jj2 + 49); // eg 0 -> '1'
  str[len + 2] = '\0';
  // just overwrite for castles
  if (str[0] == 'K' && ii == 4 && ii2 == 6)
    strcpy(str, "O-O");
  if (str[0] == 'K' && ii == 4 && ii2 == 2)
    strcpy(str, "O-O-O");

  fprintf(fptr, "%s \t", str);
  fflush(fptr);
  printf(" algebraic: %s (status %d)", str, status);
}
// adds the asssessment and time taken
void writeMove_withStats(FILE *fptr, position_t board1, move_t inMove, Int32 moveNumber, Int32 alpha, double timeTaken, Int32 depthDone,double avDepth)
{
  char str[20];
  if (moveNumber % 2 == 1) // if mve 1, 3, 5 ...
  {
    fprintf(fptr, "\n %ld \t", (int64_t)moveNumber / 2 + 1);
  }
  // get piece
  Int32 ii = inMove.from[0];
  Int32 jj = inMove.from[1];
  Int32 pieceIndex = board1->board[ii][jj];
  str[0] = pieces[abs(pieceIndex)]; // put symbol in op string
  str[1] = '\0';
  Int32 status = inMove.status;
  if ((status & 2) > 0) // AND with bit 1, checking for a take
    strcat(str, "x");
  // then get destination square
  Int32 ii2 = inMove.to[0];
  Int32 jj2 = inMove.to[1];
  Int32 len = strlen(str);
  str[len] = (char)(ii2 + 97);     // eg 0-> 'a'
  str[len + 1] = (char)(jj2 + 49); // eg 0 -> '1'
  str[len + 2] = '\0';
  // just overwrite for castles
  if (str[0] == 'K' && ii == 4 && ii2 == 6)
    strcpy(str, "O-O");
  if (str[0] == 'K' && ii == 4 && ii2 == 2)
    strcpy(str, "O-O-O");

  fprintf(fptr, "%s \t alpha %d \t time %.3f \t depth %d \t avg depth %.2f  ", str, alpha, timeTaken, depthDone,avDepth);
  fflush(fptr);
  printf(" algebraic: %s (status %d)", str, status);
}

void moveToAlgerbraic(move_t inMove, position_t board1, Int32 movenum, char *str)
{
  char frontOfStr[40];
  if (movenum % 2 == 1) // if mve 1, 3, 5 ...
    sprintf(frontOfStr, "%d. ", movenum / 2 + 1);
  else
    sprintf(frontOfStr, "%d. .. , ", movenum / 2);

  // get piece
  Int32 ii = inMove.from[0];
  Int32 jj = inMove.from[1];
  Int32 pieceIndex = board1->board[ii][jj];
  str[0] = pieces[abs(pieceIndex)]; // put symbol in op string
  str[1] = '\0';
  Int32 status = inMove.status;
  if ((status & 2) > 0) // AND with bit 1, checking for a take
    strcat(str, "x");
  // then get destination square
  Int32 ii2 = inMove.to[0];
  Int32 jj2 = inMove.to[1];
  Int32 len = strlen(str);
  str[len] = (char)(ii2 + 97);     // eg 0-> 'a'
  str[len + 1] = (char)(jj2 + 49); // eg 0 -> '1'
  str[len + 2] = '\0';
  // just overwrite for castles
  if (str[0] == 'K' && ii == 4 && ii2 == 6)
    strcpy(str, "O-O");
  if (str[0] == 'K' && ii == 4 && ii2 == 2)
    strcpy(str, "O-O-O");

  strcat(frontOfStr, str);
  strcpy(str, frontOfStr);
}

// converts piece index to unicode chess piece string- defunct with windows board
void index2UnicodePiece(Int32 pieceIndex, char *str, Int32 DrawWhiteOnBlack)
{
  // negates colouring
  if (DrawWhiteOnBlack != 1)
    pieceIndex = -pieceIndex;

  if (pieceIndex < 1)
    pieceIndex = abs(pieceIndex);
  else
    pieceIndex += 6;
  strcpy(str, " ");
  // strcat(str, unicode_pieces[pieceIndex]);
}

// evaluates a position for the FINAL move, avoiding rash captures by checking if defended
Int32 evaluateHorizon_playSafe(Int32 alpha, Int32 beta, position_t board1, move_t *inMove,
                               Int32 CURRENTSIDE, Int32 depth, Int32 maxDepth, move_t *bestLine, move_t lastMoveBefore, uint64_t hashval)
{
  Int32 depthOfSearch = maxDepth - depth; // remaining levels for hash recording
  // Int32 whiteVal, blackVal;
  move_t movesArray[maxPossMoves], dummyMoves[maxPossMoves];
  _position_t startboard; // = (position_t)calloc(1, sizeof(_position_t));
  _position_t defended;   // = (position_t)calloc(1, sizeof(_position_t));
  _position_t attacked;   // = (position_t)calloc(1, sizeof(_position_t));
  Int32 currentVal;
  castlingRights_t storeCastling = castlingRights;

  // no need to change hash colour here
  uint64_t storeHash = hashval;

  // record the start
  copyPosition(board1, &startboard);

  // get the posible moves -

  zeroPosition(&attacked);
  zeroPosition(&defended);
  //  get the 'attacked' squares (ie the defended ones of the opponent (needed to check if castling valid for current side)
  getColoursMoves(board1, &defended, &attacked, movesArray, &currentVal, CURRENTSIDE * -1, lastMoveBefore);
  // copy other player's defended to current players 'attacked'
  attacked = defended;
  Int32 moveNum = getColoursMoves(board1, &defended, &attacked, movesArray, &currentVal, CURRENTSIDE, lastMoveBefore);
  // must copy back proper defended
  defended = attacked;

  prioritiseMoveOrder_PhashFirstV2(movesArray, moveNum, depth, depthOfSearch, hashval);

  // and check all the moves
  //move_t blandMove = nullMove; // fallback move
  Int32 iiBest = -1;

  for (Int32 ii = 0; ii < moveNum; ++ii)
  {
    // here check if a move is a defended take. dont do it unless higher value as on 'horizon'
    // pick next move, and find target square and what's there
    move_t aMove = movesArray[ii];
    Int32 pieceTaken = board1->board[aMove.to[0]][aMove.to[1]];
    Int32 takenVal = values[abs(pieceTaken)];
    Int32 Mypiece = board1->board[aMove.from[0]][aMove.from[1]];
    Int32 takingVal = values[abs(Mypiece)];

    Int32 fairTake = 0;

    // worth more: would likely be best to take it
    if ((takenVal - takingVal - 500) > 0)
      fairTake = 1;

    // two options, 2nd is muchfaster but blandmove often weak
    Int32 option = 1;
    if (option == 1)
    {
      // some promise - only if an undefended take - faster as often results in a 'null move'

      if (defended.board[aMove.to[0]][aMove.to[1]] == 0 && takenVal > 0)
        fairTake = 1;

     // if (defended.board[aMove.to[0]][aMove.to[1]] == 0 && blandMove.from[0] < 0) // find a 'safe' move that fits nothing else
      //{
      //  blandMove = aMove;
      //  fairTake = 1;
     // }
    }
    else
    {
      // or if just not defended
      if (defended.board[aMove.to[0]][aMove.to[1]] == 0)
        fairTake = 1;
    }

    // if took a piece best assume taken back if defended
    Int32 takenBack = 0;
    if (fairTake == 1 && pieceTaken > 0 && defended.board[aMove.to[0]][aMove.to[1]] > 0)
      takenBack = 1;

    Int32 diff_WmB;
    if (fairTake == 1 )
    {
      // do the move
      hashval = doMove(&startboard, aMove, hashval);
      ++nodeCount;

      if (takenBack == 1) // if likely to be taken back then take it off to include in evaluation -not sure its doesn't introduce something odd
      {
        startboard.board[aMove.to[0]][aMove.to[1]] = 0; // set to space
      }
      // assess result for white THEN black
      Int32 NEXTMOVE = CURRENTSIDE * -1; // note NEXTMOVE no longer appears to be used?

      diff_WmB = gradedEvaluateBoard(&startboard, dummyMoves, NEXTMOVE, lastMoveBefore, alpha);

      // need to swap around by currentside for last assessment
      diff_WmB *= CURRENTSIDE;

      if (diff_WmB >= beta)
      {
        *inMove = movesArray[ii];
        hashval=storeHash;
        //addHashPoint(hashval, diff_WmB, depthOfSearch, &aMove); // slows it down

        return (diff_WmB);
      }

      if (diff_WmB > alpha)
      {

        alpha = diff_WmB;
        iiBest = ii;
      }

      // set board back to original before next try
      copyPosition(board1, &startboard);
      // restore castling rights and hashvalue
      hashval = storeHash;
      castlingRights = storeCastling;

    } // end if valid
  }

  castlingRights = storeCastling;
  if (iiBest < 0)
  {
    // can cause crazy moves - not sure why .. maybe found it - keep an eye!
    // no move found (or chosen) just do a 'null move' ie evauate input position
    Int32 NEXTMOVE = CURRENTSIDE * -1;

    alpha = gradedEvaluateBoard(&startboard, dummyMoves, NEXTMOVE, lastMoveBefore, alpha);
    // alpha = LazyEvaluateBoard(&startboard);
    alpha *= CURRENTSIDE;
    //*inMove = blandMove; // 
    *inMove = nullMove; // put something there


    return (alpha); // nothing better found
  }

  // copy best move to inmove, includes the status flag
  *inMove = movesArray[iiBest];

  return (alpha);
}

// evaluates a position for the FINAL move, avoiding rash captures by capturing to the end...
// ** not working properly as  occasionally causes really bad moves.poss king taking??
Int32 evaluateHorizon_shootOut(Int32 alpha, Int32 beta, position_t board1, move_t *inMove,
                               Int32 CURRENTSIDE, Int32 depth, Int32 maxDepth, move_t *bestLine,
                               move_t lastMoveBefore, uint64_t hashval, struct timespec start1, struct timespec end1)
{
  // and the squares around the kings
  _position_t storeWKing = whiteKingArea, storeBKing = blackKingArea;
  castlingRights_t storeCastling = castlingRights;

  //++nodeCount;
  //++timingNodeCount;
  //++depth;
  Int32 depthOfSearch = maxDepth - depth; // remaining levels for hash recording
  // can curtail shoot-out with a safe move- seems ok

  if (depth > (deepestLine - 2))
  {
    if (depth > maxShootOutDepth)
      maxShootOutDepth = depth;

    avShootOutDepth += depth;
    ++numShootOuts;

    alpha = evaluateHorizon_playSafe(alpha, beta, board1, inMove, CURRENTSIDE, depth, maxDepth, bestLine, lastMoveBefore, hashval);
    // restore king squares
    whiteKingArea = storeWKing;
    blackKingArea = storeBKing;
    castlingRights = storeCastling;
    return (alpha);
  }

  move_t movesArray[maxPossMoves]; //, dummyMoves[maxPossMoves];
  position_t startboard = (position_t)calloc(1, sizeof(_position_t));
  position_t defended = (position_t)calloc(1, sizeof(_position_t));
  position_t attacked = (position_t)calloc(1, sizeof(_position_t));
  Int32 currentVal;
  move_t LevelsBestLine[deepestLine];

  // record the start
  copyPosition(board1, startboard);

  // get the posible moves -
  zeroPosition(attacked); // not worrying about castling here as not a shoot-out thing!
  Int32 moveNum = getColoursMoves(board1, defended, attacked, movesArray, &currentVal, CURRENTSIDE, lastMoveBefore);

  // newly added 14/5
  if (moveNum == 0) // no moves possible: checkmate as all pieces taken off
  {
    // restore king squares
    whiteKingArea = storeWKing;
    blackKingArea = storeBKing;
    castlingRights = storeCastling;
    return (-10000000); // might just be -1e6 regardless
  }
  // sort in order, best line, killers, takes, remainder
  //prioritiseMoveOrder_PbestLineKillers(movesArray, moveNum, depth);
  // sort in order, hashval, killers, winning takes, other takes, remainder
  prioritiseMoveOrder_PhashFirstV2(movesArray, moveNum, depth, depthOfSearch, hashval);

  // and check all the moves
  Int32 iiBest = -1;

  for (Int32 ii = 0; ii < moveNum; ++ii)
  {
    // here check if a move is a defended take. dont do it unless higher value as on 'horizon'
    // pick next move, and find target square and what's there
    move_t aMove = movesArray[ii];

    Int32 status = aMove.status & 4; /// 4 for takes a major piece (N,B,R,...)

    Int32 diff_WmB;
    if (status > 0)
    {
      // printf(" hx ");
      //  do the move
      doMove(startboard, aMove, DUMMYhash);

      // toggle side with *-1  Note aMove gets changed to the best from the next level
      // diff_WmB = -getBestMove_alphaBeta_Ptakes(-beta, -alpha, startboard, &aMove,
      //                                         CURRENTSIDE * (-1), depth, maxDepth + 1, LevelsBestLine);
      //diff_WmB = -getBestMove_alphaBeta_PhashFirst(-beta, -alpha, startboard, &aMove,
      //                                             -CURRENTSIDE, depth, maxDepth + 1, LevelsBestLine, hashval);

      diff_WmB = -getBestMove_alphaBeta_timeoutPhashFirst(-beta, -alpha, startboard, &aMove,
                                                    CURRENTSIDE * (-1), depth, maxDepth +1, LevelsBestLine, hashval, start1, end1);                                        
                                              // set board back to original before next try
      copyPosition(board1, startboard);
      // restore king squares
      whiteKingArea = storeWKing;
      blackKingArea = storeBKing;
      castlingRights = storeCastling;

      // diff_WmB *= CURRENTSIDE;
      // printf("\n hx %.4f %.4f %.4f ",diff_WmB,alpha,beta);
      if (diff_WmB >= beta)
      {
        // added 14/5
        addKillerMove(movesArray[ii], depth);

        free(startboard);
        free(defended);
        free(attacked);
        return (diff_WmB);
      }
      if (diff_WmB > alpha)
      { // printf(" HY");
        alpha = diff_WmB;
        iiBest = ii;
      }
    }
  }
  // no take found so assess incoming position
  if (iiBest < 0)
  {
    if (depth > maxShootOutDepth)
      maxShootOutDepth = depth;

    avShootOutDepth += depth;
    ++numShootOuts;

    alpha = evaluateHorizon_playSafe(alpha, beta, board1, inMove, CURRENTSIDE, depth, maxDepth,
                                     bestLine, lastMoveBefore, hashval);
    free(defended);
    free(startboard);
    return (alpha); // nothing better found
  }

  // copy best move to inmove, includes the status flag
  *inMove = movesArray[iiBest];

  // get best back into +/- format
  free(startboard);
  free(defended);
  free(attacked);
  // return (best * CURRENTSIDE);
  return (alpha);
}

// basic but quite effective move-ordering putting (some)captures near the front
// prioritiseMoveOrder_PhashFirstV2() is generally better if hash available
void prioritiseMoveOrder_Ptakes(move_t movesArray[maxPossMoves], Int32 moveNum)
{
  Int32 maxOrdering = 4; // 4 faster than either 8 or 2 in a few tests
  if (moveNum < maxOrdering)
    maxOrdering = moveNum;

  move_t swap;
  for (Int32 ii = 0; ii < maxOrdering; ++ii)
  {
    for (Int32 jj = ii + 1; jj < moveNum; ++jj)
    {
      if (movesArray[jj].status > movesArray[ii].status)
      {
        swap = movesArray[ii];
        movesArray[ii] = movesArray[jj];
        movesArray[jj] = swap;
      }
    }
  }
}

// organises with 'killers' first. prioritiseMoveOrder_PhashFirstV2() is generally better if hash available
void prioritiseMoveOrder_PbestLineKillers(move_t movesArray[maxPossMoves], Int32 moveNum, Int32 depth)
{
  move_t swap;
  Int32 iGotTo = 0; // track how many added
  for (Int32 jj = 0; jj < numBestMoves; ++jj)
  {
    for (Int32 ii = 0; ii < moveNum; ++ii)
    {
      // see if a move matches the best from levelsBestMoves
      if (levelsBestMoves[depth][jj].from[0] == movesArray[ii].from[0] && levelsBestMoves[depth][jj].from[1] == movesArray[ii].from[1] &&
          levelsBestMoves[depth][jj].to[0] == movesArray[ii].to[0] && levelsBestMoves[depth][jj].to[1] == movesArray[ii].to[1])
      {
        swap = movesArray[iGotTo];
        movesArray[iGotTo] = movesArray[ii];
        movesArray[ii] = swap;
        ++iGotTo;
        break; // found it
      } // end if
    } // end for ii
  } // end for jj

  Int32 maxOrdering = 4 + iGotTo; // 4 faster than either 8 or 2 in a few tests
  if (moveNum < maxOrdering)
    maxOrdering = moveNum;

  // continue to put the takes higher, after the bestline move (if found)
  for (Int32 ii = iGotTo; ii < maxOrdering; ++ii)
  {
    for (Int32 jj = ii + 1; jj < moveNum; ++jj)
    {
      if (movesArray[jj].status > movesArray[ii].status)
      {
        swap = movesArray[ii];
        movesArray[ii] = movesArray[jj];
        movesArray[jj] = swap;
      }
    }
  }
}

// the best move order to-date sorts move by has value first, then 'killer' moves, then winning captures, ..
// .. other captures, then the rest. Order can be changed around safely.
void prioritiseMoveOrder_PhashFirstV2(move_t movesArray[maxPossMoves], Int32 moveNum, Int32 depth, Int32 depthOfSearch, uint64_t hashval)
{
  // need to 1/ see if a sufficient depth hash matches, and 2/ put it top if so
  // should think that bestmoves can be abandoned ...

  move_t copyMoves[maxPossMoves];
  Int32 tags[maxPossMoves];
  for (Int32 ii = 0; ii < moveNum; ++ii)
  {
    //  copyMoves[ii]=nullMove; // not needed
    tags[ii] = 0; // to show if used yet
  }

  Int32 iGotTo = 0; // track how many added

  // squeeze hash in first
  Int32 alphaVal; // dummy, not used
  move_t inMove;

  Int32 gotHash = getPreviousBestHashPosition(hashval, depthOfSearch, &alphaVal, &inMove);
  if (gotHash == 1)
  {
    for (Int32 ii = 0; ii < moveNum; ++ii)
    {
      // see if a move matches the best from levelsBestMoves
      if (movesArray[ii].from[0] == inMove.from[0] && movesArray[ii].from[1] == inMove.from[1] &&
          movesArray[ii].to[0] == inMove.to[0] && movesArray[ii].to[1] == inMove.to[1])
      {
        copyMoves[iGotTo] = movesArray[ii];
        tags[ii] = 1; // show used
        ++iGotTo;
        break; // found it
      } // end if
    } // end for ii
  }

  // next the 'killer moves'
  // start from 1 to avoid 'bestline'
  for (Int32 jj = 1; jj < numBestMoves; ++jj)
  {
    for (Int32 ii = 0; ii < moveNum; ++ii)
    {
      // see if a 'killer' move matches a remaining move
      if (tags[ii] == 0 && levelsBestMoves[depth][jj].from[0] == movesArray[ii].from[0] && levelsBestMoves[depth][jj].from[1] == movesArray[ii].from[1] &&
          levelsBestMoves[depth][jj].to[0] == movesArray[ii].to[0] && levelsBestMoves[depth][jj].to[1] == movesArray[ii].to[1])
      {
        copyMoves[iGotTo] = movesArray[ii];
        tags[ii] = 1; // record as used
        ++iGotTo;
        break; // found it
      } // end if
    } // end for ii
  } // end for jj

  // next the 'winning moves'
  for (Int32 ii = 0; ii < moveNum; ++ii)
  {
    if ((movesArray[ii].status >= 8) && (tags[ii] == 0)) // winning moves and unused
    {
      copyMoves[iGotTo] = movesArray[ii];
      tags[ii] = 1; // record as used
      ++iGotTo;
    }
  }

  // next the other ('losing') captures'
  for (Int32 ii = 0; ii < moveNum; ++ii)
  {
    if ((movesArray[ii].status >= 2) && (tags[ii] == 0)) // if a capture and unused
    {
      copyMoves[iGotTo] = movesArray[ii];
      tags[ii] = 1; // record as used
      ++iGotTo;
    }
  }

  // lastly the remaining moves
  for (Int32 ii = 0; ii < moveNum; ++ii)
  {
    if ((tags[ii] == 0)) // if unused
    {
      copyMoves[iGotTo] = movesArray[ii];
      tags[ii] = 1; // record as used
      ++iGotTo;
    }
  }

  // then copy back to input
  for (Int32 ii = 0; ii < moveNum; ++ii)
  {
    movesArray[ii] = copyMoves[ii];
  }

} // v2

void prioritiseMoveOrder_PhashFirst(move_t movesArray[maxPossMoves], Int32 moveNum, Int32 depth, Int32 depthOfSearch, uint64_t hashval)
{
  // need to 1/ see if a sufficient depth hash matches, and 2/ put it top if so
  // should think that bestmoves can be abandoned ...

  move_t swap;
  Int32 iGotTo = 0; // track how many added

  // squeeze hash in here for now
  Int32 alphaVal; // dummy, not used
  move_t inMove;
  Int32 gotHash = getPreviousBestHashPosition(hashval, depthOfSearch, &alphaVal, &inMove);
  if (gotHash == 1)
  {
    for (Int32 ii = 0; ii < moveNum; ++ii)
    {
      // see if a move matches the best from levelsBestMoves
      if (movesArray[ii].from[0] == inMove.from[0] && movesArray[ii].from[1] == inMove.from[1] &&
          movesArray[ii].to[0] == inMove.to[0] && movesArray[ii].to[1] == inMove.to[1])
      {
        swap = movesArray[0];
        movesArray[0] = movesArray[ii];
        movesArray[ii] = swap;
        ++iGotTo;
        break; // found it
      } // end if
    } // end for ii
  }

  Int32 maxOrdering = 4 + iGotTo; // 4 faster than either 8 or 2 in a few tests
  if (moveNum < maxOrdering)
    maxOrdering = moveNum;

  // continue to put the takes higher, after the bestline move (if found)
  for (Int32 ii = iGotTo; ii < maxOrdering; ++ii)
  {
    for (Int32 jj = ii + 1; jj < moveNum; ++jj)
    {
      if (movesArray[jj].status > movesArray[ii].status)
      {
        swap = movesArray[ii];
        movesArray[ii] = movesArray[jj];
        movesArray[jj] = swap;
        ++iGotTo;
      }
    }
  }

  // start from 1 to avoid 'bestline'
  for (Int32 jj = 1; jj < numBestMoves; ++jj)
  {
    for (Int32 ii = iGotTo; ii < moveNum; ++ii)
    {
      // see if a move matches the best from levelsBestMoves
      if (levelsBestMoves[depth][jj].from[0] == movesArray[ii].from[0] && levelsBestMoves[depth][jj].from[1] == movesArray[ii].from[1] &&
          levelsBestMoves[depth][jj].to[0] == movesArray[ii].to[0] && levelsBestMoves[depth][jj].to[1] == movesArray[ii].to[1])
      {
        swap = movesArray[iGotTo];
        movesArray[iGotTo] = movesArray[ii];
        movesArray[ii] = swap;
        ++iGotTo;
        break; // found it
      } // end if
    } // end for ii
  } // end for jj
}

// checks for position repeated 3 time using position hashes
void checkForRepetition(Int32 atMoveNum)
{
  uint64_t currentHash = hashHistory[atMoveNum];
  Int32 minii = atMoveNum - 10; // if not in last 10 probs not repeated
  if (minii < 1)
    minii = 1;
  Int32 count = 0;
  for (Int32 ii = atMoveNum; ii >= minii; --ii)
  {
    if (hashHistory[ii] == currentHash)
      count += 1;
  }
  if (count >= 3)
    FLTK3RepeatsFlag = 1;
}
// store positions and the move before the human players allowing takeback - last move status needed for e.p.
void storeMoveHistory(position_t board1, move_t inMove, Int32 storedMoveNumber, uint64_t inhash)
{
  // need only store every other position
  // storedMoveNumber /= 2;
  positionHistory[storedMoveNumber] = *board1;
  moveHistory[storedMoveNumber] = inMove;
  hashHistory[storedMoveNumber] = inhash;
  castlingHistory[storedMoveNumber] = castlingRights;

  // check for 3 position repetitions; routine sets a flag if found
  checkForRepetition(storedMoveNumber);
}

// read back an earlier position and last move - 'lastMove' move needed for e.p.
void readMoveHistory(position_t board1, move_t *inMove, Int32 storedMoveNumber)
{
  printf("\n position back to move %ld", (int64_t)storedMoveNumber);
  // storedMoveNumber /= 2; // as every 2nd move stored
  if (storedMoveNumber >= 2)
    lastMove = moveHistory[storedMoveNumber - 1]; // -1 as used for move stored after position reached
  else
    lastMove = nullMove;
  *inMove = lastMove;
  castlingRights = castlingHistory[storedMoveNumber];
  *board1 = positionHistory[storedMoveNumber];
  char tmpstr[20];
  move2string(*inMove, tmpstr);
  printf(" last move coordinates %s, status %d", tmpstr, inMove->status);
  //if (storedMoveNumber < 20)
  //  FLTKbookOpeningsOn = 1; // may have been switched off, relevent for early moves, but complicates
}

// sets a poard to all zeroes - needed before updating 'defended', and sometimes for 'attacked'
void zeroPosition(position_t pos)
{
  for (Int32 ii = 0; ii < 8; ++ii)
  {
    for (Int32 jj = 0; jj < 8; ++jj)
    {
      pos->board[ii][jj] = 0;
    }
  }
}

Int32 getBestMove(Int32 alpha, Int32 beta, position_t board1, move_t *inMove,
                  Int32 CURRENTSIDE, Int32 depth, Int32 maxDepth, move_t *bestLine, Int32 STRATEGY, uint64_t hashval, Int32 *movesCalculated)
{
  // set normal default for moves calculated, it may change with incremental methods e.g strategy = 5
  // note the 'normal' is strategy = 5 or 6
  *movesCalculated = maxDepth;
  Int32 diff_WmB;
  // zero the best line
  for (Int32 mv = 0; mv < deepestLine; ++mv)
    bestLine[mv] = nullMove;
  struct timespec start1, end1; // for timings
  clock_gettime(CLOCK_REALTIME, &start1);

  // see if endgame conditions, sets flag if so
  seeIfEndgame(board1,CURRENTSIDE);
  if (STRATEGY == 0) // brute force search
  {
    diff_WmB = getBestMove_noPrune(board1, inMove, CURRENTSIDE, depth, maxDepth, bestLine, hashval);
    return (diff_WmB);
  }
  else if (STRATEGY == 1) // alpha-beta pruning with some prioritising (takes)
  {
    diff_WmB = getBestMove_alphaBeta_Ptakes(alpha, beta, board1, inMove, CURRENTSIDE, depth, maxDepth, bestLine, hashval);
    return (diff_WmB);
  }
  else if (STRATEGY == 6)
  {
    printf(" .. strategy 6");
    // get an initial best line with reduced depth
    Int32 reducedDepth = maxDepth - 3;
    if (reducedDepth < 2)
      reducedDepth = 2;
    diff_WmB = getBestMove_alphaBeta_Ptakes(alpha, beta, board1, inMove, CURRENTSIDE, depth, reducedDepth, bestLine, hashval);

    // make best line more permanent
    for (Int32 ii = 1; ii <= reducedDepth; ++ii)
      levelsBestMoves[ii][0] = bestLine[ii];
    // use the invalid move (-1,-1) to (-1,-1)

    for (Int32 ii = reducedDepth + 1; ii < deepestLine; ++ii)
      levelsBestMoves[ii][0] = nullMove;
    diff_WmB = getBestMove_alphaBeta_timeoutPhashFirst(alpha, beta, board1, inMove, CURRENTSIDE, depth, maxDepth,
                                                       bestLine, hashval, start1, end1);
    return (diff_WmB);
  }
  else if (STRATEGY == 7)
  {
    printf(" .. strategy 7");

    if (LIMITMODE == TARGETMAXTIME || LIMITMODE == ALLMOVESTIME)
      maxDepth = deepestLine - 1;

    printf("\n max time set at %.4f", FLTKavgMoveTime);
    // get an initial best line with reduced depth
    // make it 3 (or 4 if even) so that increments of 1 or two always reach the final value
    Int32 reducedDepth = 2;
    //if (maxDepth % 2 == 0)
    //  reducedDepth = 2;

    Int32 increment = 1; // only increments of 1 will work properly

    // turn on hashing if wanted (may have been turned off)
    if(doHashing ==1)
      pauseHashing =0;

    diff_WmB = getBestMove_alphaBeta_timeoutPhashFirst(alpha, beta, board1, inMove, CURRENTSIDE, depth,
      reducedDepth, bestLine, hashval, start1, end1);
    // make best line more permanent
    for (Int32 ii = 1; ii <= reducedDepth; ++ii)
      levelsBestMoves[ii][0] = bestLine[ii];
    // use the invalid move (-1,-1) to (-1,-1)
    for (Int32 ii = reducedDepth + 1; ii < deepestLine; ++ii)
      levelsBestMoves[ii][0] = nullMove;

    // for stepping back if timeout happens
    move_t keepMove = *inMove;
    Int32 keepDepth = reducedDepth;

    // printHashSearchStats();
    printf("\n iterating: ");
    while (1)
    {
      reducedDepth += increment;

      // uses GBytes beyond 9, increasing rapidly - so stop it here
      if(reducedDepth >9)
        pauseHashing =1; // stops storage and searching

      if (reducedDepth >= deepestLine) // seg faults past this point
        break;

      diff_WmB = getBestMove_alphaBeta_timeoutPhashFirst(alpha, beta, board1, inMove, CURRENTSIDE, depth,
                                                         reducedDepth, bestLine, hashval, start1, end1);

      clock_gettime(CLOCK_REALTIME, &end1);
      double secs = (double)(end1.tv_sec - start1.tv_sec) + (1e-9) * (double)(end1.tv_nsec - start1.tv_nsec);
      printf(" - depth %d %.2f s: ", reducedDepth, secs);
      shortPrintMove(*inMove, board1);
      if (TimeOut == 1)
      {
        printf(" - timed out****************** %d", diff_WmB);
        *inMove = keepMove;
        reducedDepth = keepDepth;
        *movesCalculated = keepDepth;

        // and restore last best line
        for (Int32 ii = 1; ii <= reducedDepth; ++ii)
          bestLine[ii] = levelsBestMoves[ii][0];
        // use the invalid move (-1,-1) to (-1,-1)
        for (Int32 ii = reducedDepth + 1; ii < deepestLine; ++ii)
          bestLine[ii] = nullMove;
        TimeOut = 0; // reset for next move
        break;
      } // end if timeout

      fflush(stdout);
      // update last ones
      keepMove = *inMove;
      keepDepth = reducedDepth;

      // make best line more permanent
      for (Int32 ii = 1; ii <= reducedDepth; ++ii)
        levelsBestMoves[ii][0] = bestLine[ii];
      // use the invalid move (-1,-1) to (-1,-1)
      for (Int32 ii = reducedDepth + 1; ii < deepestLine; ++ii)
        levelsBestMoves[ii][0] = nullMove;

      *movesCalculated = reducedDepth; // info to return
      // printHashSearchStats();

      // need to stop at max anyway
      if (reducedDepth >= deepestLine)
        break;

      if (LIMITMODE == FIXEDDEPTH)
        if (reducedDepth >= maxDepth)
          break;

      if (LIMITMODE == TARGETMAXTIME)
        if (secs >= FLTKavgMoveTime / 3.) // assume 3-6x longer for next depth step (avg)
          break;

      if (LIMITMODE == ALLMOVESTIME)
      {
        double timeLeft, thisMoveTime;
        Int32 compMovesDone = totalCompMoves;
        if (compMovesDone <= 36) // do 36 moves in 80% of time
        {
          timeLeft = 0.8 * FLTKallMovesTime * 60. - totalCompTime; // allmovestime is in munutes
          thisMoveTime = timeLeft / (37 - totalCompMoves);         // at least 1 on denominator
        }
        else
        {
          timeLeft = FLTKallMovesTime * 60. - totalCompTime;
          thisMoveTime = timeLeft / 20.; // plan to use over 20 moves - reducing!
        }

        // set timeout time
        timeoutTimeAllMoves = thisMoveTime;

        // limit to something reasonable: 4 times the initial average
        if (thisMoveTime > 4 * 0.8 * FLTKallMovesTime * 60. / 36.)
          thisMoveTime = 4 * 0.8 * FLTKallMovesTime * 60. / 36.;

        printf(" target %.1fsec", thisMoveTime);

        if (secs > thisMoveTime / 3.5) //  cut more slack as aiming for average, not peak
          break;
      }
    } // end while 1
    return (diff_WmB);
  }
  else
  {
    printf("\n search strategy %d not valid - exiting\n", STRATEGY);
    exit(0);
  }
}

// inserts new 'killer' move at a point, moving older ones down
inline void addKillerMove(move_t inMove, Int32 depth)
{
  // printf("\n here");
  // if(inMove.status > 1)return; // don't add 'takes' - slows it often

  // check if already in
  for (Int32 ii = 0; ii < numBestMoves; ++ii)
  {
    if (inMove.from[0] == levelsBestMoves[depth][ii].from[0] && inMove.from[1] == levelsBestMoves[depth][ii].from[1] &&
        inMove.to[0] == levelsBestMoves[depth][ii].to[0] && inMove.to[1] == levelsBestMoves[depth][ii].to[1])
      return; // don't add as already there
  } // end for

  //  got here so new tone to add
  Int32 insertAt = 1;
  // will add at 2nd location [1], shift rest down
  for (Int32 ii = numBestMoves - 1; ii >= (insertAt + 1); --ii)
    levelsBestMoves[depth][ii] = levelsBestMoves[depth][ii - 1];

  // and insert new move at [1] (after bestline)
  levelsBestMoves[depth][insertAt] = inMove;

} // end addKillerMove()

void getKingArea(position_t board1, Int32 COLOUR)
{
  Int32 ii, jj, ix, iy;
  Int32 colourKing = 6 * COLOUR; /// index of the colour's king
  Int32 step = COLOUR;
  int32_t weight;
  workingKingArea = nullKingArea; // zero everywhere

  for (ii = 0; ii < 8; ++ii)
  {
    for (jj = 0; jj < 8; ++jj)
    {
      if (board1->board[ii][jj] == colourKing) // if the right king
      {
        // for king's row and 2 in front
        for (Int32 mult = 1; mult < 3; ++mult) // slows it right down so only 1 or 2 rows sensible
        {
          iy = jj + mult * step;
          for (ix = ii - 1; ix <= ii + 1; ++ix)
          {
            if (ix >= 0 && ix < 8 && iy >= 0 && iy < 8)
            {
              if (mult == 1)
                weight = 2;
              else
                weight = 1;

              workingKingArea.board[ix][iy] = weight;
            }
          }
        } // end k

        if (COLOUR > 0)
          whiteKingArea = workingKingArea;
        else
          blackKingArea = workingKingArea;

        // if(COLOUR>0){showBoard1(&workingKingArea);printf(" king area");}

        return;
      }
    } // end for jj
  } // end for ii
}

// returns the sign, as efficiently as possible!
inline Int32 sign(Int32 x)
{
  if (x > 0)
    return 1;
  if (x < 0)
    return -1;
  return 0;
  // return( (x>0) - (x<0)); // other is faster? .. unclear different every run!
}

void printBestLine(move_t *bestLine, Int32 movesCalculated, Int32 diff_WmB)
{
  char instring[50];
  // print best line
  printf("\n best line: {");
  for (Int32 ii = 1; ii <= movesCalculated; ++ii)
  {
    move2string(bestLine[ii], instring);
    printf("%s", instring);
    if (ii < movesCalculated)
      printf(",");
  }
  printf("} ( %d deep, %d) ", movesCalculated, diff_WmB);
}

Int32 onBoard(Int32 i, Int32 j)
{
  if (i >= 0 && i < 8 && j >= 0 && j < 8)
    return (1);
  else
    return (0);
}

void bestLineToAlgebraicString(position_t board1, move_t *bestLine, Int32 movesCalculated, Int32 diff_WmB, Int32 CURRENTSIDE)
{
  Int32 addOn = 0; // a shift of where the semi-colons go
  if (CURRENTSIDE == 1)
    addOn = 1;
  // store these as changed by doMove()
  _position_t storeWKing = whiteKingArea, storeBKing = blackKingArea;
  castlingRights_t storeCastling = castlingRights;
  _position_t startingBoard;
  copyPosition(board1, &startingBoard);
  uint64_t dummyHash = 0;
  strcpy(FLTKbestlineStr, "best line: {");
  for (Int32 mv = 1; mv <= movesCalculated; ++mv) // note starts at array index 1
  {
    char str[20];
    strcpy(str, "null");

    // get piece
    Int32 ii = bestLine[mv].from[0];
    Int32 jj = bestLine[mv].from[1];
    if (onBoard(ii, jj)) // tests if indices are on board, proceeds if so
    {
      Int32 pieceIndex = board1->board[ii][jj];
      str[0] = pieces[abs(pieceIndex)]; // put symbol in op string (P,B,N etc)
      str[1] = '\0';

      Int32 status = bestLine[mv].status;
      if ((status & 2) > 0) // AND with bit 1, checking for a take
        strcat(str, "x");
      // then get destination square
      Int32 ii2 = bestLine[mv].to[0];
      Int32 jj2 = bestLine[mv].to[1];
      Int32 len = strlen(str);
      str[len] = (char)(ii2 + 97);     // eg 0-> 'a'
      str[len + 1] = (char)(jj2 + 49); // eg 0 -> '1'
      str[len + 2] = '\0';
      // just overwrite for castles
      if (str[0] == 'K' && ii == 4 && ii2 == 6)
        strcpy(str, "O-O");
      if (str[0] == 'K' && ii == 4 && ii2 == 2)
        strcpy(str, "O-O-O");

      // if white king taken on 2nd move
      // this one wrks (the computers)
      if (FLTKPLAYER != CURRENTSIDE && FLTKPLAYER != NEITHER_COLOUR)
      {
        // printf("\n **  not same %d %d %d %d",mv,board1->board[ii2][jj2],FLTKPLAYER,CURRENTSIDE);
        if (mv == 3 && board1->board[ii2][jj2] == 6 && CURRENTSIDE == BLACK)
          FLTKcheckmateFlag = -1; // black has won
        if (mv == 3 && board1->board[ii2][jj2] == -6 && CURRENTSIDE == WHITE)
          FLTKcheckmateFlag = +1;                    // white has won
        if (mv == 2 && board1->board[ii2][jj2] == 6) // not sure if need extra condition here ...
          FLTKcheckmateFlag = -1;                    // black has won
        if (mv == 2 && board1->board[ii2][jj2] == -6)
          FLTKcheckmateFlag = +1; // white has won
      }
      if (FLTKPLAYER == NEITHER_COLOUR)
      {
        // printf("\n **  not same %d %d %d %d",mv,board1->board[ii2][jj2],FLTKPLAYER,CURRENTSIDE);
        if (mv == 3 && board1->board[ii2][jj2] == 6 && CURRENTSIDE == BLACK)
          FLTKcheckmateFlag = -1; // black has won
        if (mv == 3 && board1->board[ii2][jj2] == -6 && CURRENTSIDE == WHITE)
          FLTKcheckmateFlag = +1;                                            // white has won
        if (mv == 2 && board1->board[ii2][jj2] == 6 && CURRENTSIDE == BLACK) // not sure if need extra condition here ...
          FLTKcheckmateFlag = -1;                                            // black has won
        if (mv == 2 && board1->board[ii2][jj2] == -6 && CURRENTSIDE == WHITE)
          FLTKcheckmateFlag = +1; // white has won
      }

    } // end if on board

    // add to display string
    strcat(FLTKbestlineStr, str);

    if (mv < movesCalculated)
    {
      if ((mv + addOn) % 2 == 0)
        strcat(FLTKbestlineStr, ","); // after white move
      else
        strcat(FLTKbestlineStr, "; "); // after black move
    }

    // then move on
    if (onBoard(ii, jj))
      doMove(board1, bestLine[mv], dummyHash);

  } // end for mv

  strcat(FLTKbestlineStr, "}"); // end stuff
  // interesting to see comps view
  // Int32 vivid = 1;
  // if(vivid==1)
  //  showBoard2(board1,WHITE);

  copyPosition(&startingBoard, board1); // back to starting values

  whiteKingArea = storeWKing;
  blackKingArea = storeBKing;
  castlingRights = storeCastling;
}

void printBestLineAlgebraic(position_t board1, move_t *bestLine, Int32 movesCalculated, Int32 diff_WmB, Int32 CURRENTSIDE)
{
  Int32 addOn = 0; // a shift of where the semi-colons go
  if (CURRENTSIDE == 1)
    addOn = 1;
  // store these as changed by doMove()
  _position_t storeWKing = whiteKingArea, storeBKing = blackKingArea;
  castlingRights_t storeCastling = castlingRights;
  _position_t startingBoard;
  copyPosition(board1, &startingBoard);
  uint64_t dummyHash = 0;
  printf("\n best line: {");
  for (Int32 mv = 1; mv <= movesCalculated; ++mv) // note starts at array index 1
  {
    char str[20];
    strcpy(str, "null");

    // get piece
    Int32 ii = bestLine[mv].from[0];
    Int32 jj = bestLine[mv].from[1];
    if (onBoard(ii, jj)) // tests if indices are on board, proceeds if so
    {
      Int32 pieceIndex = board1->board[ii][jj];
      str[0] = pieces[abs(pieceIndex)]; // put symbol in op string (P,B,N etc)
      str[1] = '\0';

      Int32 status = bestLine[mv].status;
      if ((status & 2) > 0) // AND with bit 1, checking for a take
        strcat(str, "x");
      // then get destination square
      Int32 ii2 = bestLine[mv].to[0];
      Int32 jj2 = bestLine[mv].to[1];
      Int32 len = strlen(str);
      str[len] = (char)(ii2 + 97);     // eg 0-> 'a'
      str[len + 1] = (char)(jj2 + 49); // eg 0 -> '1'
      str[len + 2] = '\0';
      // just overwrite for castles
      if (str[0] == 'K' && ii == 4 && ii2 == 6)
        strcpy(str, "O-O");
      if (str[0] == 'K' && ii == 4 && ii2 == 2)
        strcpy(str, "O-O-O");

    } // end if on board

    // output to sreen
    printf("%s", str);
    if (mv < movesCalculated)
    {
      if ((mv + addOn) % 2 == 0)
        printf(","); // after white move
      else
        printf("; "); // after black move
    }

    // then move on
    if (onBoard(ii, jj))
      doMove(board1, bestLine[mv], dummyHash);

  } // end for mv

  printf("} ( %d deep, %d) ", movesCalculated, diff_WmB); // end stuff
  fflush(stdout);
  // interesting to see comps view
  // Int32 vivid = 1;
  // if(vivid==1)
  //  showBoard2(board1,WHITE);

  copyPosition(&startingBoard, board1); // back to starting values

  whiteKingArea = storeWKing;
  blackKingArea = storeBKing;
  castlingRights = storeCastling;
}

void printHashesBestLine(position_t board1, move_t inMove, Int32 movesCalculated, Int32 diff_WmB,
                         Int32 CURRENTSIDE, uint64_t hashval)
{
  Int32 addOn = 0; // a shift of where the semi-colons go
  if (CURRENTSIDE == 1)
    addOn = 1;
  // store these as changed by doMove()
  _position_t storeWKing = whiteKingArea, storeBKing = blackKingArea;
  castlingRights_t storeCastling = castlingRights;
  _position_t startingBoard;
  copyPosition(board1, &startingBoard);

  uint64_t storeHash = hashval;
  hashval = toggleWhiteToPlayHash(hashval);

  Int32 dummyAlpha = 0;
  move_t nextMove = inMove;
  Int32 gotOne;

  // printf("\n before anything");
  // showBoard2(board1,1);

  Int32 mv;
  printf("\n best line: {");
  for (mv = 1; mv <= movesCalculated; ++mv) // note starts at array index 1
  {
    char str[20];

    // get piece
    Int32 ii = nextMove.from[0];
    Int32 jj = nextMove.from[1];
    Int32 pieceIndex = board1->board[ii][jj];
    str[0] = pieces[abs(pieceIndex)]; // put symbol in op string (P,B,N etc)
    str[1] = '\0';
    Int32 status = nextMove.status;
    if ((status & 2) > 0) // AND with bit 1, checking for a take
      strcat(str, "x");
    // then get destination square
    Int32 ii2 = nextMove.to[0];
    Int32 jj2 = nextMove.to[1];
    Int32 len = strlen(str);
    str[len] = (char)(ii2 + 97);     // eg 0-> 'a'
    str[len + 1] = (char)(jj2 + 49); // eg 0 -> '1'
    str[len + 2] = '\0';
    // just overwrite for castles
    if (str[0] == 'K' && ii == 4 && ii2 == 6)
      strcpy(str, "O-O");
    if (str[0] == 'K' && ii == 4 && ii2 == 2)
      strcpy(str, "O-O-O");

    // output to sreen
    printf("%s", str);
    if (mv < movesCalculated)
    {
      if ((mv + addOn) % 2 == 0)
        printf(","); // after white move
      else
        printf("; "); // after black move
    }

    // then move on and change the hashvalue colour
    hashval = doMove(board1, inMove, hashval);
    hashval = toggleWhiteToPlayHash(hashval);

    gotOne = getPreviousBestHashPosition(hashval, 0, &dummyAlpha, &nextMove); //-10 to remove limits

    // copy back for next iteration
    inMove = nextMove;

    // printf("\n gotone = %d",gotOne);fflush(stdout);
    if (gotOne == 0)
    {
      break;
    }

  } // end for mv

  printf("} ( %d deep, %d) ", movesCalculated, diff_WmB); // end stuff
  printf("\n (ran out at mv = %d)", mv);

  // interesting to see comps view
  //  showBoard2(board1,WHITE);

  copyPosition(&startingBoard, board1); // back to starting values

  whiteKingArea = storeWKing;
  blackKingArea = storeBKing;
  castlingRights = storeCastling;
  hashval = storeHash;
}

void HashesToBestline(position_t board1, move_t inMove, move_t *bestLine, Int32 movesCalculated, Int32 diff_WmB,
                      Int32 CURRENTSIDE, uint64_t hashval)
{

  // store these as changed by doMove()
  _position_t storeWKing = whiteKingArea, storeBKing = blackKingArea;
  castlingRights_t storeCastling = castlingRights;
  _position_t startingBoard;
  copyPosition(board1, &startingBoard);

  uint64_t storeHash = hashval;
  hashval = toggleWhiteToPlayHash(hashval);

  Int32 dummyAlpha = 0;
  move_t nextMove = inMove;
  Int32 gotOne;
  Int32 mv;

  for (mv = 1; mv <= movesCalculated; ++mv) // note starts at array index 1; zero first
  {
    bestLine[mv] = nullMove;
  }
  for (mv = 1; mv <= movesCalculated; ++mv) // note starts at array index 1
  {
    bestLine[mv] = inMove;
    // then move on and change the hashvalue colour
    hashval = doMove(board1, inMove, hashval);
    hashval = toggleWhiteToPlayHash(hashval);

    gotOne = getPreviousBestHashPosition(hashval, 0, &dummyAlpha, &nextMove); //-10 to remove limits

    // copy back for next iteration
    if (gotOne == 1)
      inMove = nextMove;
    else
    {
      inMove = nullMove;
    }

    // printf("\n gotone = %d",gotOne);fflush(stdout);
    if (gotOne == 0)
    {
      break;
    }

  } // end for mv

  // interesting to see comps view
  //  showBoard2(board1,WHITE);

  copyPosition(&startingBoard, board1); // back to starting values

  whiteKingArea = storeWKing;
  blackKingArea = storeBKing;
  castlingRights = storeCastling;
  hashval = storeHash;
}

Int32 fullyEvaluateBoard(position_t startboard, move_t *dummyMoves, Int32 NEXTMOVE, move_t lastMoveBefore)
{
  // return(LazyEvaluateBoard(startboard));
  Int32 whiteVal, blackVal;
  evaluateBoard(startboard, dummyMoves, &whiteVal, WHITE, NEXTMOVE, lastMoveBefore);
  evaluateBoard(startboard, dummyMoves, &blackVal, BLACK, NEXTMOVE, lastMoveBefore);
  return (whiteVal - blackVal);
}

// just the pieces
Int32 LazyEvaluateBoard(position_t board1)
{
  // zero the evaluations first
  Int32 colourVal = 0;

  // material totting
  for (Int32 jj = 7; jj >= 0; --jj) // starting at top
  {
    for (Int32 ii = 0; ii < 8; ++ii)
    {
      Int32 piece = board1->board[ii][jj];
      if (piece > 0)                // if white piece
        colourVal += values[piece]; // add to colour's total
      if (piece < 0)
        colourVal -= values[-piece]; // if black then subtract (do nothing for zero)

    } // end for ii
  } // end for jj

  // material totted.

  return (colourVal);
}

Int32 gradedEvaluateBoard(position_t startboard, move_t *dummyMoves, Int32 NEXTMOVE, move_t lastMoveBefore, Int32 bestalpha)
{
  // not sure this behaves well
  /*
  Int32 lazyEval = LazyEvaluateBoard(startboard);
  if (lazyEval < (bestalpha - 1000)) // 1000 milli-pawns is the usual max positional score
    return (lazyEval);
    */

  Int32 fullEval = fullyEvaluateBoard(startboard, dummyMoves, NEXTMOVE, lastMoveBefore);
  return (fullEval);
}

// store game position and associated states
Int32 writeGame(void)
{
  // char fname[2000];
  // strcpy(fname,"gamex.game");

  char *fname;
  fname = fl_file_chooser("Save game (.game)", "*.game", "a.game");
  printf("\n return name is %s", fname);
  fflush(stdout);
  if (strcmp(fname, "") == 0)
  {
    fname = (char *)calloc(200, sizeof(char));
    strcpy(fname, "tempname");
  }

  if (strstr(fname, ".game") == (char *)NULL)
    strcat(fname, ".game");
  printf("\n name after tweaks = %s", fname);

  FILE *fptr = fopen(fname, "wb");
  if (fptr == NULL)
  {
    printf("\n *******ERROR couldn't write file %s *****", fname);
    return (0);
  } // failure

  fwrite(&DisplayBoard, sizeof(_position_t), 1, fptr);        // send position
  fwrite(&FLTKPLAYER, sizeof(int32_t), 1, fptr);              // send colour of player
  fwrite(&FLTKCURRENTSIDE, sizeof(int32_t), 1, fptr);         // side to play next
  fwrite(&moveNumber, sizeof(int32_t), 1, fptr);              // move number
  fwrite(&castlingRights, sizeof(castlingRights_t), 1, fptr); // castling & ep rights
  fwrite(&lastMove, sizeof(move_t), 1, fptr);
  fwrite(&lastMoveBefore, sizeof(move_t), 1, fptr);

  // and history
  fwrite(&moveHistory, MAXHIST * sizeof(move_t), 1, fptr);
  fwrite(&positionHistory, MAXHIST * sizeof(position_t), 1, fptr);
  fwrite(&castlingHistory, MAXHIST * sizeof(castlingRights_t), 1, fptr);

  fl_message(" Game saved to %s", fname);
  fclose(fptr);
  return (1); // success
}
// read in a stored game and associated states
Int32 readGame(void)
{

  char *fname;
  fname = fl_file_chooser("Load game (.game)", "*.game", "");
  printf("\n return name is %s", fname);
  fflush(stdout);

  if (strstr(fname, ".game") == (char *)NULL)
  { // no valid file returned
    fl_message(" No \".game\" file selected - can't open");
    return (0);
  }

  FILE *fptr = fopen(fname, "rb");
  if (fptr == NULL)
  {
    printf("\n *******ERROR couldn't open file %s *****", fname);
    return (0);
  } // failure

  size_t success = fread(&DisplayBoard, sizeof(_position_t), 1, fptr); // read board position
  printf("\n success =%ld", success);
  success = fread(&FLTKPLAYER, sizeof(int32_t), 1, fptr);
  printf("\n success =%ld", success);
  success = fread(&FLTKCURRENTSIDE, sizeof(int32_t), 1, fptr);
  printf("\n success =%ld", success);
  success = fread(&moveNumber, sizeof(int32_t), 1, fptr);
  printf("\n success =%ld", success);
  success = fread(&castlingRights, sizeof(castlingRights_t), 1, fptr);
  printf("\n success =%ld", success);
  success = fread(&lastMove, sizeof(move_t), 1, fptr);
  printf("\n success =%ld", success);
  success = fread(&lastMoveBefore, sizeof(move_t), 1, fptr);
  printf("\n success =%ld", success);

  // and history
  success = fread(&moveHistory, MAXHIST * sizeof(move_t), 1, fptr);
  printf("\n success =%ld", success);
  success = fread(&positionHistory, MAXHIST * sizeof(position_t), 1, fptr);
  printf("\n success =%ld", success);
  success = fread(&castlingHistory, MAXHIST * sizeof(castlingRights_t), 1, fptr);
  printf("\n success =%ld", success);

  fclose(fptr);

  return (1); // success
}

// sums the *major* piece values for the opposite side, for determining if endgame conditions
void countPieceValue(position_t board1,Int32 CURRENTSIDE,Int32 *queenOn, Int32 *pieceValueCount)
{
  // zero
  *queenOn =0;
  *pieceValueCount =0;

  Int32 OPPCOLOUR = -CURRENTSIDE; // get count for opposite colour

  for (Int32 jj = 7; jj >= 0; --jj) // starting at top
  {
    for (Int32 ii = 0; ii < 8; ++ii)
    {
      Int32 piece = board1->board[ii][jj];
      if (piece * OPPCOLOUR > 0)
      {
        Int32 abspiece = abs(piece);

        // if between a Knight and a queen
        if( (abspiece > 1) && (abspiece < 6))
        {
        Int32 pieceVal = values[abspiece]; // tot the values, in millipawns
        *pieceValueCount += pieceVal; // add to total
        if(abspiece == 5)
          *queenOn =1; // the queen lives still!
        }

      }

    } // end for ii
  } // end for jj
  return;
}

void seeIfEndgame(position_t board1,Int32 CURRENTSIDE)
{
  // get *major* piece value count, and if queen present, of OTHER side (negation in routine)
  Int32 queenOn,pieceValueCount;
  countPieceValue(board1,CURRENTSIDE,&queenOn, &pieceValueCount);
  printf("\n major piece values sum: %d, queen on = %d ",pieceValueCount,queenOn);

  // and decide if have reached endgame .. used in positional enumeration
  if(pieceValueCount < 15000) // less than eg 2rooks + 2 bishops
    gameStage = EARLYENDGAME; // value 1
  if(pieceValueCount < 11000) //  less than 1 rook and 2 knights
    gameStage = FULLENDGAME; // value 2

  printf("- gamestage %d",gameStage);
}

void aboutMessage()
{
  fl_message("Chesspresso V1.0 - a quick fix of chess \nPete Olley, 2026");
}

void acknowlMessage()
{
  fl_message("Acknowledgements are gratefully given to \n -FLTK (Fast Light Tool Kit) for windowing libraries\n -the author of the chess piece images (origin forgotten)");
}