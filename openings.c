#include "chessMk9.h"
#include <ctype.h>

extern castlingRights_t castlingRights;

#define QUIET 0
#define VERBOSE 1
#define DebugCHATTY 2

extern uint64_t DUMMYhash;// temp for doMove() changes
extern char FLTKbestlineStr[500];


// set level of 'chat' here
Int32 verboseOpenings = VERBOSE;

// structure to hold first moves
// structure to hold a position and evaluation
typedef struct struct_openings
{
  move_t *opmoves;
  Int32 numMoves;
  float relativeFrequency;
  char name[40];
  char variation[40];
  Int32 definingMove;
  char rawMoves[300]; // for use in displaying opening line
} _openings_t;
typedef _openings_t *openings_t;

Int32 readOpeningsFlag = 0;
Int32 maxFirstMoves = 20; // only 20 for first moves fallback
Int32 maxOpenings = 100;  // more for main openings
Int32 numFirstMoves = 0;
openings_t firstMoves = NULL;
char algMoves[100][10];

//// declarations ////
void allocateFirstMoves();
void freeFirstMoves();

void allocateFirstMoves()
{
  // return(0); this stops the heap prob!
  firstMoves = (openings_t)calloc(maxFirstMoves + 1, sizeof(_openings_t));

  for (Int32 ii = 0; ii < (maxFirstMoves + 1); ++ii)
  {
    firstMoves[ii].opmoves = (move_t *)calloc(2, sizeof(move_t));
  }
}

void allocateOpenings()
{
  // return(0); this stops the heap prob!
  firstMoves = (openings_t)calloc(maxOpenings + 1, sizeof(_openings_t));

  for (Int32 ii = 0; ii < (maxOpenings + 1); ++ii)
  {
    firstMoves[ii].opmoves = NULL;
  }
}

void freeOpenings()
{
  if (firstMoves != NULL)
  {
    for (Int32 ii = 0; ii < (maxOpenings + 1); ++ii)
    {
      if (firstMoves[ii].opmoves != NULL)
      {
        free(firstMoves[ii].opmoves);
        firstMoves[ii].opmoves = NULL;
      }
    }

    free(firstMoves);
    firstMoves = NULL;
  }
}

void freeFirstMoves()
{
  if (firstMoves != NULL)
  {
    for (Int32 ii = 0; ii < (maxFirstMoves + 1); ++ii)
    {
      if (firstMoves[ii].opmoves != NULL)
      {
        free(firstMoves[ii].opmoves);
        firstMoves[ii].opmoves = NULL;
      }
    }

    free(firstMoves);
    firstMoves = NULL;
  }
}

Int32 getBookMove(Int32 moveNumber, move_t *moveHistory, move_t *inMove, char *bookName)
{
  Int32 validIndex[200], numValid = 0;
  float validRelFreq[200];
  if (verboseOpenings >= VERBOSE)
    printf("\n openings move %d ***\n", moveNumber);

  // check each opening
  for (Int32 opening = 0; opening < numFirstMoves; ++opening)
  {
    Int32 valid = 0;
    // run through the moves
    if (firstMoves[opening].numMoves >= moveNumber) //  keep in for generic
    {
      valid = 1; // assume valid

      for (Int32 move = 1; move < moveNumber; ++move)
      {
        // note: moveHistory starts from 1, whereas openings_t start from the usual 0
        if (moveHistory[move].from[0] != firstMoves[opening].opmoves[move - 1].from[0] ||
            moveHistory[move].from[1] != firstMoves[opening].opmoves[move - 1].from[1] ||
            moveHistory[move].to[0] != firstMoves[opening].opmoves[move - 1].to[0] ||
            moveHistory[move].to[1] != firstMoves[opening].opmoves[move - 1].to[1])
        {
          valid = 0;
          break;
        }

      } // end for move

    } // end if >= moves in opening

    if (valid == 1)
    {
      validIndex[numValid] = opening; // store it
      validRelFreq[numValid] = firstMoves[opening].relativeFrequency;
      if (verboseOpenings >= DebugCHATTY)
        printf("\nopening %d %s is valid, rel freq %f", opening, firstMoves[opening].name, validRelFreq[numValid]);
      ++numValid;
    }
  } // end for opening

  // now have numValid possible continuations. If zero then return zero to end looking up book moves
  if (numValid == 0)
    return (0);

  if (verboseOpenings >= VERBOSE)
    printf("\n  have %d matching opening lines", numValid);
  //  have at least one move if at this point
  //  get sum of relative frequencies
  float sumRelFreq = 0.;
  for (Int32 ii = 0; ii < numValid; ++ii)
    sumRelFreq += validRelFreq[ii];

  // seeded with srand((uint32_t)time(0)) earlier
  float randomNum = (float)((double)rand() / (double)RAND_MAX); // scale to 0 - 1
  randomNum *= sumRelFreq;                                      // scale to whole range

  // find which range the random number fits into

  float upper, lower;
  Int32 iiLucky = 0; // find the one that wins the random number 'lottery'
  // for the first one
  lower = -1e-7;
  upper = validRelFreq[0];
  if (lower <= randomNum && upper >= randomNum)
  {
    iiLucky = 0; // picked
  }
  // for the rest
  for (Int32 ii = 1; ii < numValid; ++ii)
  {
    lower += validRelFreq[ii - 1];
    upper = lower + validRelFreq[ii];
    if (lower <= randomNum && upper >= randomNum)
    {
      iiLucky = ii;
    }
  }

  *inMove = firstMoves[validIndex[iiLucky]].opmoves[moveNumber - 1];
  strcpy(bookName, firstMoves[validIndex[iiLucky]].name); // and the name (will be replaced by less 'committing' one)

  // now find the most specific variation that matches the chosen move
  /*Int32 mostSpecificVariation = -1;
  Int32 highestMatch = -1;
  for (Int32 ii = 0; ii < numValid; ++ii)
  {
    if ((moveNumber) >= firstMoves[validIndex[ii]].definingMove && firstMoves[validIndex[ii]].definingMove >= highestMatch)
    {
      // must check last move matches
      if ((inMove->from[0] == firstMoves[validIndex[ii]].opmoves[moveNumber - 1].from[0]) &&
          (inMove->from[1] == firstMoves[validIndex[ii]].opmoves[moveNumber - 1].from[1]) &&
          (inMove->to[0] == firstMoves[validIndex[ii]].opmoves[moveNumber - 1].to[0]) &&
          (inMove->to[1] == firstMoves[validIndex[ii]].opmoves[moveNumber - 1].to[1]))
      {
        mostSpecificVariation = ii;
        highestMatch = firstMoves[validIndex[ii]].definingMove;
        if (verboseOpenings >= VERBOSE)
          printf("\n %d match chosen %s", ii, firstMoves[validIndex[ii]].name);
      } // end if
    } // end if
  } // end for
  */

  Int32 mostSpecificVariation = -1;
  Int32 simplestMatch = 10000000;
  for (Int32 ii = 0; ii < numValid; ++ii)
  {
    if ((moveNumber) <= firstMoves[validIndex[ii]].numMoves && firstMoves[validIndex[ii]].numMoves < simplestMatch)
    {
      // must check last move matches
      if ((inMove->from[0] == firstMoves[validIndex[ii]].opmoves[moveNumber - 1].from[0]) &&
          (inMove->from[1] == firstMoves[validIndex[ii]].opmoves[moveNumber - 1].from[1]) &&
          (inMove->to[0] == firstMoves[validIndex[ii]].opmoves[moveNumber - 1].to[0]) &&
          (inMove->to[1] == firstMoves[validIndex[ii]].opmoves[moveNumber - 1].to[1]))
      {
        mostSpecificVariation = ii;
        simplestMatch = firstMoves[validIndex[ii]].numMoves;
        if (verboseOpenings >= VERBOSE)
          printf("\n %d match chosen %s", ii, firstMoves[validIndex[ii]].name);
      } // end if
    } // end if
  } // end for

  if (mostSpecificVariation >= 0) // add on variation if at defining point
  {
    strcpy(bookName, firstMoves[validIndex[mostSpecificVariation]].name); // and the name
    strcat(bookName, firstMoves[validIndex[mostSpecificVariation]].variation);
    if (verboseOpenings >= VERBOSE)
      printf("\n chosen variation name %s, defining move %d", firstMoves[validIndex[mostSpecificVariation]].name, firstMoves[validIndex[mostSpecificVariation]].definingMove);
    
    sprintf(FLTKbestlineStr, "in %s %s:",firstMoves[validIndex[mostSpecificVariation]].name,
           firstMoves[validIndex[mostSpecificVariation]].variation);
    strcat(FLTKbestlineStr,firstMoves[validIndex[mostSpecificVariation]].rawMoves);
    }
  return (1); // success

  return (0); // 0 says no book moves found - will switch off further checking for one
}

void setUpFirstMoves()
{
  // seed the random number generator, very randomly
  srand((uint32_t)time(0));

  // allocateFirstMoves(); // do once - creates the space for maxFirstMoves 2 move openings
  allocateOpenings();

  Int32 result = readOpeningsBook();
  // if not found openings book just do first moves version to give variety
  if (result == 1)
    return;

  // result was 0 if here
  // free small first moves allocation
  freeOpenings();
  // and allocate bigger main one
  allocateFirstMoves();

  numFirstMoves = 0;
  move_t move1, move2;

  // standard e4 e5 (indices 4,1 to 4,3 for e4, then 4,6 to 4,4 for e5) ** this is the reference 1.0 for relative frequency **
  move1.from[0] = 4;
  move1.from[1] = 1;
  move1.to[0] = 4;
  move1.to[1] = 3; // e4
  move2.from[0] = 4;
  move2.from[1] = 6;
  move2.to[0] = 4;
  move2.to[1] = 4;
  firstMoves[numFirstMoves].opmoves[0] = move1;
  firstMoves[numFirstMoves].opmoves[1] = move2; // white's move then black's
  firstMoves[numFirstMoves].numMoves = 2;       // 2 moves
  strcpy(firstMoves[numFirstMoves].name, "King's pawn opening");
  firstMoves[numFirstMoves].relativeFrequency = 1.0;
  ++numFirstMoves;

  // French defence e4, e6
  move2.from[0] = 4;
  move2.from[1] = 6;
  move2.to[0] = 4;
  move2.to[1] = 5;
  firstMoves[numFirstMoves].opmoves[0] = move1;
  firstMoves[numFirstMoves].opmoves[1] = move2; // white's move then black's
  firstMoves[numFirstMoves].numMoves = 2;       // 2 moves
  strcpy(firstMoves[numFirstMoves].name, "French defence");
  firstMoves[numFirstMoves].relativeFrequency = 0.2;
  ++numFirstMoves;

  // sicilian e4, c5 - no need to change move1
  move2.from[0] = 2;
  move2.from[1] = 6;
  move2.to[0] = 2;
  move2.to[1] = 4;
  firstMoves[numFirstMoves].opmoves[0] = move1;
  firstMoves[numFirstMoves].opmoves[1] = move2; // white's move then black's
  firstMoves[numFirstMoves].numMoves = 2;       // 2 moves
  strcpy(firstMoves[numFirstMoves].name, "Sicilian defence");
  firstMoves[numFirstMoves].relativeFrequency = 0.6;
  ++numFirstMoves;

  // English - just switch moves around
  firstMoves[numFirstMoves].opmoves[0] = move2;
  firstMoves[numFirstMoves].opmoves[1] = move1; // white's move then black's
  firstMoves[numFirstMoves].numMoves = 2;       // 2 moves
  strcpy(firstMoves[numFirstMoves].name, "English Opening");
  firstMoves[numFirstMoves].relativeFrequency = 0.1;
  ++numFirstMoves;

  // Caro Kann e4, c6
  move2.from[0] = 2;
  move2.from[1] = 6;
  move2.to[0] = 2;
  move2.to[1] = 5;
  firstMoves[numFirstMoves].opmoves[0] = move1;
  firstMoves[numFirstMoves].opmoves[1] = move2; // white's move then black's
  firstMoves[numFirstMoves].numMoves = 2;       // 2 moves
  strcpy(firstMoves[numFirstMoves].name, "Caro Kann");
  firstMoves[numFirstMoves].relativeFrequency = 0.1;
  ++numFirstMoves;

  // Alekhine's defence e4, Nf6
  move2.from[0] = 6;
  move2.from[1] = 7;
  move2.to[0] = 5;
  move2.to[1] = 5;
  firstMoves[numFirstMoves].opmoves[0] = move1;
  firstMoves[numFirstMoves].opmoves[1] = move2; // white's move then black's
  firstMoves[numFirstMoves].numMoves = 2;       // 2 moves
  strcpy(firstMoves[numFirstMoves].name, "Alekhine's defence");
  firstMoves[numFirstMoves].relativeFrequency = 0.1;
  ++numFirstMoves;

  // Centre counter e4, d5
  move2.from[0] = 3;
  move2.from[1] = 7;
  move2.to[0] = 3;
  move2.to[1] = 5;
  firstMoves[numFirstMoves].opmoves[0] = move1;
  firstMoves[numFirstMoves].opmoves[1] = move2; // white's move then black's
  firstMoves[numFirstMoves].numMoves = 2;       // 2 moves
  strcpy(firstMoves[numFirstMoves].name, "Centre counter defence");
  firstMoves[numFirstMoves].relativeFrequency = 0.1;
  ++numFirstMoves;

  // standard d4 d5 (indices 3,1 to 3,3 for d4, then 3,6 to 3,4 for d5)
  move1.from[0] = 3;
  move1.from[1] = 1;
  move1.to[0] = 3;
  move1.to[1] = 3; // e4
  move2.from[0] = 3;
  move2.from[1] = 6;
  move2.to[0] = 3;
  move2.to[1] = 4;
  firstMoves[numFirstMoves].opmoves[0] = move1;
  firstMoves[numFirstMoves].opmoves[1] = move2; // white's move then black's
  firstMoves[numFirstMoves].numMoves = 2;       // 2 moves
  strcpy(firstMoves[numFirstMoves].name, "Queen's-pawn opening");
  firstMoves[numFirstMoves].relativeFrequency = 0.5;
  ++numFirstMoves;

  // other usual Queen's pawn d4, nf6
  move2.from[0] = 6;
  move2.from[1] = 7;
  move2.to[0] = 5;
  move2.to[1] = 5;
  firstMoves[numFirstMoves].opmoves[0] = move1;
  firstMoves[numFirstMoves].opmoves[1] = move2; // white's move then black's
  firstMoves[numFirstMoves].numMoves = 2;       // 2 moves
  strcpy(firstMoves[numFirstMoves].name, "Queen's pawn opening (..Nf6)");
  firstMoves[numFirstMoves].relativeFrequency = 0.6;
  ++numFirstMoves;
  if (verboseOpenings >= VERBOSE)
    printf("added %d first moves", numFirstMoves);
}

Int32 readOpeningsBook()
{
  FILE *fptr;
  char line[1024];
  int64_t endReached;
  fptr = fopen("OpeningsBook.txt", "r");

  if (fptr == NULL) // if none found, proceed with basic 'first moves'
    return (0);

  Int32 lineCount = 1;
  numFirstMoves = 0;

  // read through the comment lines
  while ((endReached = (int64_t)fgets(line, sizeof(line), fptr)))
  {
    if (strstr(line, "######") != 0)
      break;
    ++lineCount;
  }
  if (verboseOpenings >= DebugCHATTY)
    printf("\ncomments to line %d\n ", lineCount);

  char feature[500];
  Int32 defMove;
  float relFreq;
  // find next opening
  while ((endReached = (int64_t)fgets(line, sizeof(line), fptr)))
  {

    if (strstr(line, "#Opening:") != 0)
      break;
    ++lineCount;
  }
  while (1) // need to detect EOF !!
  {
    if (verboseOpenings >= DebugCHATTY)
      printf("\n\nopening at line %d ", lineCount + 1);
    // replace newline with string terminator
    for (Int32 ii = 0; ii < 100; ++ii)
      if ((Int32)line[ii] == '\n')
        line[ii] = '\0';

    strcpy(feature, &line[10]); // miss trigger bit out
    if (verboseOpenings >= DebugCHATTY)
      printf("- opening %d is %s", numFirstMoves, feature);

    strcpy(firstMoves[numFirstMoves].name, feature);

    // initialise opening
    strcpy(firstMoves[numFirstMoves].variation, "");
    firstMoves[numFirstMoves].definingMove = 100000;
    firstMoves[numFirstMoves].relativeFrequency = 1.0;
    firstMoves[numFirstMoves].numMoves = 0;

    // read through until the next opening
    while ((endReached = (int64_t)fgets(line, sizeof(line), fptr)))
    {

      ++lineCount;

      // replace newline with string terminator
      for (Int32 ii = 0; ii < 500; ++ii)
        if ((Int32)line[ii] == '\n')
          line[ii] = '\0';

      if (strstr(line, "#Opening") != 0) // next opening found
      {
        if (verboseOpenings >= DebugCHATTY)
        {
          printf("\n\n ***FINAL opening %d ***", numFirstMoves);
          printf("\n %s %s \n", firstMoves[numFirstMoves].name, firstMoves[numFirstMoves].variation);
          printf("\n numMoves %d, def move %d  relfreq %f\n", firstMoves[numFirstMoves].numMoves, firstMoves[numFirstMoves].definingMove, firstMoves[numFirstMoves].relativeFrequency);
        }

        if (verboseOpenings >= VERBOSE)
        {
          printf("\n number of openings %d %s - %s ", numFirstMoves + 1,
                 firstMoves[numFirstMoves].name, firstMoves[numFirstMoves].variation);
          if (abs(firstMoves[numFirstMoves].relativeFrequency - 1.0) > .001)
            printf(" (rel. freq %.2f)", firstMoves[numFirstMoves].relativeFrequency);
        }

        ++numFirstMoves;
        break;
      }

      // check for other things: variation
      if (strncmp(line, "variation:", 10) == 0)
      {
        strcpy(feature, &line[10]); // miss trigger bit out
        if (verboseOpenings >= DebugCHATTY)
          printf("\n new variation: %s", feature);
        strcpy(firstMoves[numFirstMoves].variation, "(");
        strcat(firstMoves[numFirstMoves].variation, feature);
        strcat(firstMoves[numFirstMoves].variation, ")");
      }

      if (strncmp(line, "definingMove:", 13) == 0)
      {
        strcpy(feature, &line[13]); // miss trigger bit out
                                    // printf("\n defining move string %s", feature);
        sscanf(feature, "%d", &defMove);
        if (verboseOpenings >= DebugCHATTY)
          printf(", defining move = %d", defMove);
        firstMoves[numFirstMoves].definingMove = defMove;
      }

      if (strncmp(line, "relativeFrequency:", 18) == 0)
      {
        strcpy(feature, &line[18]); // miss trigger bit out
        // printf("\n defining move string x%sx", feature);
        sscanf(feature, "%f", &relFreq);
        if (verboseOpenings >= DebugCHATTY)
          printf(", relfreq = %f", relFreq);
        firstMoves[numFirstMoves].relativeFrequency = relFreq;
      }

      // and the moves
      if (strncmp(line, "moves:", 6) == 0)
      {

        strcpy(feature, &line[6]); // miss trigger bit out
        strncpy(firstMoves[numFirstMoves].rawMoves,feature,298);// less than 300

        // first strip out the ',' and ';', replace with spaces
        for (Int32 ii = 0; ii < 500; ++ii)
        {
          if ((Int32)feature[ii] == ',')
            feature[ii] = ' ';

          if ((Int32)feature[ii] == ';')
            feature[ii] = ' ';
          if ((Int32)feature[ii] == '+') // remove any 'checks'
            feature[ii] = ' ';
        }
        char moveStr[40];
        if (verboseOpenings >= DebugCHATTY)
          printf("\n string of moves x%sx", feature);

#define OUT 0
#define IN 1

        Int32 moveCount = 0;
        Int32 state = OUT; // Initial state is OUT

        Int32 ii = 0;
        while (1)
        {
          // Traverse all characters of the input string
          if (feature[ii] == '\n' || feature[ii] == '\0')
            break;

          // If the current character is a word character
          if (isChessNotation(feature[ii]))
          {
            // If previous state was OUT, increment word count and change state to IN
            if (state == OUT)
            {
              moveCount++;
              state = IN;
              // strncpy(moveStr, &feature[ii], 8);
              for (Int32 jj = 0; jj < 8; ++jj) // avoiding warning from strncpy()
                moveStr[jj] = feature[ii + jj];

              for (Int32 jj = 0; jj < 8; ++jj) // end it at first space
                if (moveStr[jj] == ' ')
                  moveStr[jj] = '\0';
              moveStr[8] = '\0';
              strcpy(&algMoves[moveCount - 1][0], moveStr);
            }
          }
          // If the current character is not a word character
          else
          {
            // Change state to OUT
            state = OUT;
          }
          ++ii;
        } // end while 1

        // printf("\n %d moves read\n", moveCount);
        // for (Int32 ii = 0; ii < moveCount; ++ii)
        // printf("\n after: move %d = x%sx\n", ii, &algMoves[ii][0]);

        // allocate enough space
        firstMoves[numFirstMoves].opmoves = (move_t *)calloc(moveCount + 1, sizeof(move_t));
        firstMoves[numFirstMoves].numMoves = moveCount;

        // now need to play through moves and add to firstMoves[numFirstMoves].opmoves[jj] for jj=0.. moveCount -1

        move_t *newOpening = firstMoves[numFirstMoves].opmoves; // just tidier
        Int32 goodOpening = runThroughMoves(moveCount, newOpening, algMoves);

        if (goodOpening == 1)
        {
          // should be in opmoves
          if (verboseOpenings >= DebugCHATTY)
            printf("- from line %d  : %s", lineCount + 1, feature);
        }
        else
        {
          printf("\n openingsBook.txt problem, line %d \n  :- %s (%s)", lineCount, line, feature);
        }

      } // end if moves:
    }
    // printf("\n end end %ld",endReached);
    if (endReached == 0)
    {
      ++numFirstMoves; // add one on for the last one done
      break;
    }

  } // end of (big) while(1)

  //++numFirstMoves;
  printf("\n ok here");
  fflush(stdout);
  // return (0); // ** TEMP** keep first moves active
  return (1); // read something
} // end readOpeningsBook()

// returns 1 if all good, 0 of a problem
Int32 runThroughMoves(Int32 moveCount, move_t *newOpening, char algMoves[100][10])
{
  castlingRights_t storeCastling = castlingRights;
  position_t testBoard, defended, attacked;
  move_t movesArray[maxPossMoves];
  Int32 colourValPtr = 0;
  move_t lastMoveBefore;
  testBoard = (position_t)calloc(1, sizeof(_position_t));
  defended = (position_t)calloc(1, sizeof(_position_t));
  attacked = (position_t)calloc(1, sizeof(_position_t));
  startingBoard(testBoard); // set to starting positions
  Int32 CURRENTSIDE = WHITE;
  for (Int32 moveNo = 0; moveNo < moveCount; ++moveNo)
  {
    Int32 currentMoves = getColoursMoves(testBoard, defended, attacked, movesArray, &colourValPtr, CURRENTSIDE, lastMoveBefore);
    char instring[20];
    char origInstring[20];
    strcpy(instring, &algMoves[moveNo][0]);
    strcpy(origInstring, instring);
    move_t inMove;
    Int32 messageFlag;
    Int32 validMove = parseAlgebraicString(testBoard, instring, movesArray, currentMoves, &inMove, &messageFlag, CURRENTSIDE);
    if (validMove == 1) // is ok
    {
      newOpening[moveNo] = inMove;
    }
    else
    {
      printf("\n openings problem with %s", origInstring);
      free(testBoard);
      free(defended);
      free(attacked);
      castlingRights = storeCastling;
      return (0); // failed
    }

    doMove(testBoard, inMove, DUMMYhash);
    CURRENTSIDE = -CURRENTSIDE; // and swap sides

  } // end for moveNo

  free(testBoard);
  free(defended);
  free(attacked);
  castlingRights = storeCastling;
  return (1); // 1 for passed
}

Int32 isChessNotation(char c)
{
  if (isalnum(c) > 0)
    return (1);
  if (c == '+')
    return (1);
  if (c == '-') // needed for castling 0-0
    return (1);
  if (c == 'x') // needed for taking
    return (1);

  return (0); // not chess notation, except space, \n etc
}