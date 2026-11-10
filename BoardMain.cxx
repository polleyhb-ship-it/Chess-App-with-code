//
// BoardView class .
//
// Copyright 1998-2024 by Bill Spitzak and others.
//
// This library is free software. Distribution and use rights are outlined in
// the file "COPYING" which should have been included with this file.  If this
// file is missing or damaged, see the license at:
//
//     https://www.fltk.org/COPYING.php
//
// Please see the following page on how to report bugs and issues:
//
//     https://www.fltk.org/bugs.php
//

#include <FL/Fl.H>
#include <GL/glut.h>
#include "BoardViewUI.h"
#include "BoardView.h"
#include "chessMk9.h"
#include <math.h>
#include <cmath> // avoids unexpected 'exp() not found
// #include "chessGlobals.h"

extern _position_t DisplayBoard;
extern move_t DisplayMove;
extern struct timespec start, end; // for timings
extern time_t dateAndTimeStr;
extern Int32 moveNumber;
extern move_t lastMove;
extern int64_t nodeCount;

Int32 FLTKCURRENTSIDE = WHITE;
Int32 FLTKPLAYER = WHITE;
uint64_t FLTKHASH;
FILE *FLTKfptr;
Int32 FLTKbookOpeningsOn = 1;
Int32 FLTKmaxDepth = 7;

// FLTKsearch strategies: 
// .. 0=brute force, no pruning; 
// .. 1= alpha-beta pruning, prioritising 'takes'; ...
// .. 6 = hash first (one off)
// .. 7 = hash first (incremental)
//    other numbers were tests/experiments, now removed. 7 best
Int32 FLTKstrategy = 7;

double FLTKavgMoveTime = 3.0;   // for 3.0 *seconds* per move, on average
double FLTKallMovesTime = 10.0; // for 10.0 *minutes* time for whole game
extern Int32 LIMITMODE;
struct timespec compStart, compEnd;
struct timespec absStart, absEnd;
BoardViewUI *FLTKbrdui;


move_t lastMoveBefore;
// lets you change the

void idlefunc(void *);

char flstr[200];
int main(int argc, char **argv)
{
  glutInit(&argc, argv); // needed for any glut to work properly
  BoardViewUI *bvui = new BoardViewUI;
  FLTKbrdui = bvui;

  // eg of use
  // FLTKbrdui->zoom->value(1.0);

  // send it so available elsewhere. looks complex so leave it til resize
  // Setivui(brdui);

  // Initial global objects.

  Fl::visual(FL_DOUBLE | FL_INDEX);
  bvui->show(argc, argv);
  // initialising routines here
  FLTKfptr = fopen("game.txt", "w");
  if (FLTKfptr == NULL)
  {
    printf("\n NULL file pointer returned for game.txt - exiting");
    exit(0);
  }

  // send stdout to a file - UNLESS started with any command line argument eg ./chesspresso 0
  if (argc <2) // get 1 just for "./chesspresso" - hedging bets for double-click start
  {
    printf("\n all text output sent to debugOut.txt, start with eg ./chesspresso 0 to see\n");
    fflush(stdout);
    FILE *fp = freopen("debugOut.txt", "w", stdout);
    if (fp == (FILE *)NULL)
      printf("\n WARNING debugOut.txt not opened\n");
  }

  // add starting time to game.txt
  time(&dateAndTimeStr);
  fprintf(FLTKfptr, "%s \n", ctime(&dateAndTimeStr));

  // get pieces and initialise bord and move
  readPiecePPMs();
  startingBoard(&DisplayBoard);
  DisplayMove.from[0] = -1;

  // initialise the chess engine, from old main()
  initialise_chess();

  // find squares near kings
  getKingArea(&DisplayBoard, WHITE);
  getKingArea(&DisplayBoard, BLACK);
  lastMoveBefore = lastMove; // to initialise to status = 0

  // don't think this is actually used as calculated at start of each move
  FLTKHASH = hashFullPosition(&DisplayBoard);
  printf("\n initial hash value = %lu", FLTKHASH);

  clock_gettime(CLOCK_REALTIME, &compStart); // initialise computer starting times
  clock_gettime(CLOCK_REALTIME, &compEnd);
  clock_gettime(CLOCK_REALTIME, &absStart);

  if (LIMITMODE == 0)
  {
    sprintf(flstr, "%d", FLTKmaxDepth);
    FLTKbrdui->tLimit_to->value(flstr); // show time limit
    FLTKbrdui->tLimit_to->label(" - move depth");
  }
  if (LIMITMODE == 1)
  {
    sprintf(flstr, "%.1f", FLTKavgMoveTime);
    FLTKbrdui->tLimit_to->value(flstr); // show time limit
    FLTKbrdui->tLimit_to->label("time per move (sec) ");
  }
  if (LIMITMODE == 2)
  {
    sprintf(flstr, "%d min for game", (Int32)FLTKallMovesTime);
    FLTKbrdui->tLimit_to->value(flstr); // show time limit
    FLTKbrdui->tLimit_to->label("time for game (mins)");
  }

  Fl::add_idle(idlefunc);
  return Fl::run();
}

extern double totalCompTime;
extern Int32 totalCompMoves;
extern Int32 totalCompDepths;
extern Int32 FLTKlastDepth;
extern Int32 FLTKassessment;
extern char FLTKlastMoveStr[40];
extern char FLTKbestlineStr[500];
extern Int32 FLTK3RepeatsFlag;  // normally 0,  1 if 3 repeats detected
extern Int32 FLTKcheckmateFlag; // normally 0, +1 if 1-0, and -1 if 0-1

void idlefunc(void *)
{
  // if nothing happening is called about every 0.016 sec = 16 msec
  // static int count = 0;

  // clock_gettime(CLOCK_REALTIME, &absEnd); // finishing time
  // double secs = (double)(absEnd.tv_sec - absStart.tv_sec) + (1e-9) * (double)(absEnd.tv_nsec - absStart.tv_nsec);
  // printf("\n in idle %d :time now = %.3f sec.",count, secs);
  //++count;

  // works if (human) player is not the current side, and isn't both colours (play through mode)
  if (FLTKCURRENTSIDE != FLTKPLAYER && FLTKPLAYER != BOTH_COLOURS) 
  {
    // following not used as re-hashed in docomputermove()
    // FLTKHASH = hashFullPosition(&DisplayBoard);
    // if (FLTKCURRENTSIDE != WHITE)
    //  FLTKHASH = toggleWhiteToPlayHash(FLTKHASH); // toggles each level, so start with opposite one

    if (moveNumber > (MAXHIST - 1))
    {
      fl_message("maximum move number reached - increase MAXHIST in code - exiting - bye!");
      freeHashesAndOpenings();
      exit(0);
    }
    // this is the check after players's move
    if (FLTKcheckmateFlag != 0)
    {
      if (FLTKcheckmateFlag > 0)
      {
        fl_message("-Checkmate! : 1 - 0");
        if (FLTKPLAYER == NEITHER_COLOUR) // save automatic run-on in comp v comp
          FLTKPLAYER = WHITE;
      }
      else
      {
        fl_message("-Checkmate! : 0 - 1");
        if (FLTKPLAYER == NEITHER_COLOUR)
          FLTKPLAYER = WHITE;
      }
    }

    if (FLTK3RepeatsFlag != 0)
    {
      if (FLTKPLAYER == NEITHER_COLOUR)
      { // dont work!! attempt to stop it running on in comp v comp mode
        FLTKPLAYER = FLTKCURRENTSIDE;
        void TakeBackMove(); // declaration of function elsewhere
        TakeBackMove();      // go back 2-ply (= 1 traditional chess move)
        fl_message("Draw by 3 repeats : 1/2 - 1/2 - going back 2 moves and you play white");
      }
      else
      {
        fl_message("- Draw by 3 repeats : 1/2 - 1/2");
        printf("\n\n fltkplayer = %d", FLTKPLAYER);
      }
    }

    // need resetting, best here
    FLTK3RepeatsFlag = 0;
    FLTKcheckmateFlag = 0;

    Int32 isGood;
    isGood = doComputerMove(FLTKfptr, &FLTKCURRENTSIDE, FLTKPLAYER, FLTKHASH);
    // printf("\n computers: isgood = %d", isGood);
    // fflush(stdout);

    if (isGood == 1)
    { // computers move should always be valid, but just for luck

      // update times

      clock_gettime(CLOCK_REALTIME, &compEnd); // finishing time
      double secs = (double)(compEnd.tv_sec - compStart.tv_sec) + (1e-9) * (double)(compEnd.tv_nsec - compStart.tv_nsec);
      // printf("\n hdl:time taken for move = %.3f sec.", secs);

      totalCompTime += secs; // includes time for book openings, but this is fairly negligible
      // totalCompMoves += 1;
      double flmins = totalCompTime / 60.;
      char flstr[40];
      sprintf(flstr, "%.2f", flmins); // humans total in minutes
      FLTKbrdui->cmpTot_to->value(flstr);
      double flavg;
      if (totalCompMoves > 0)
        flavg = totalCompTime / ((double)totalCompMoves);
      else
        flavg = totalCompTime / (0.5 * (double)(moveNumber - 1));
      sprintf(flstr, "%.2f", flavg); // comps average per move in seconds
      FLTKbrdui->cmpAv_to->value(flstr);
      sprintf(flstr, "%.2f", secs); // final comp time for move
      FLTKbrdui->current_to->value(flstr);

      // printf("\n ** at ui display depths : %d, moves = %d movenum %d secs %.4f **",totalCompDepths,totalCompMoves, moveNumber,secs);fflush(stdout);
      double flavDepth;
      if (totalCompMoves > 0)
        flavDepth = (double)totalCompDepths / ((double)totalCompMoves);
      else
        flavDepth = 0.0;

      sprintf(flstr, "%.2f", flavDepth); // average search depth
      FLTKbrdui->avdepth_to->value(flstr);
      // double flCoverage = 0.001*exp(flavDepth)/flavg; // .001 to convert 'rate' to per ms, assumes exponential growth per move
      double flCoverage = 0.001 * (double)nodeCount / totalCompTime; // kilo-nodes calculated per sec

      sprintf(flstr, "%.1f", flCoverage); // average search depth
      FLTKbrdui->coverage_to->value(flstr);
      sprintf(flstr, "%d", FLTKlastDepth); // last search depth
      FLTKbrdui->lastdepth_to->value(flstr);
      FLTKbrdui->lastmove_to->value(FLTKlastMoveStr); // last move in algebraic
      sprintf(flstr, "%d", FLTKassessment);           // average search depth
      FLTKbrdui->assessment_to->value(flstr);
      FLTKbrdui->bestline_to->value(FLTKbestlineStr);
      clock_gettime(CLOCK_REALTIME, &compStart); // resets time at end of players's = start of computer's move

      FLTKbrdui->cube->redraw();
      // this is the check after comp's's move

      if (FLTKcheckmateFlag != 0)
      {
        if (FLTKcheckmateFlag > 0)
        {
          fl_message("Checkmate! : 1 - 0");
          if (FLTKPLAYER == NEITHER_COLOUR) // save automatic run-on in comp v comp
            FLTKPLAYER = WHITE;
        }
        else
        {
          fl_message("Checkmate! : 0 - 1");
          if (FLTKPLAYER == NEITHER_COLOUR)
            FLTKPLAYER = WHITE;
        }
      }

      if (FLTK3RepeatsFlag != 0)
      {
        if (FLTKPLAYER == NEITHER_COLOUR)
        { // dont work!! attempt to stop it running on in comp v comp mode
          FLTKPLAYER = FLTKCURRENTSIDE;
          void TakeBackMove(); // declaration of function elsewhere
          TakeBackMove();      // go back 2-ply (= 1 traditional chess move)
          fl_message("Draw by 3 repeats : 1/2 - 1/2 - going back 2 moves and you play white");
        }
        else
        {
          fl_message("-- Draw by 3 repeats : 1/2 - 1/2");
          printf("\n\n fltkplayer = %d", FLTKPLAYER);
        }
      }
      // need resetting, best here
      FLTK3RepeatsFlag = 0;
      FLTKcheckmateFlag = 0;
    }
  }

  FLTKbrdui->cube->redraw();
}
