//
// BoardView class implementation for the Fast Light Tool Kit (FLTK).
//
// Copyright 1998-2021 by Bill Spitzak and others.
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

// Note to editor: the following code can and should be copied
// to the fluid tutorial in 'documentation/src/fluid.dox'
// *without* '#if HAVE_GL' preprocessor statements, leaving
// only those parts where the condition is true.

// [\code in documentation/src/fluid.dox]
#include "BoardViewUI.h"
#include "BoardView.h"
#include "ppmRoutines.h"
#include "./chessMk9.h"
#include <math.h>
#include <GL/glut.h>
ppm_t piecePPMs[13];

// temp for board tesp ***
_position_t DisplayBoard;

static int floatingPiece=2,floatingx=4,floatingy=5;

// not temp
move_t DisplayMove;
Int32 DisplayMoveStatus = 0;
extern Int32 FLTKCURRENTSIDE;
extern Int32 FLTKPLAYER;
extern uint64_t FLTKHASH;
extern FILE *FLTKfptr;
extern Int32 moveNumber;
extern BoardViewUI *FLTKbrdui;
extern Int32 FLTKlastDepth;
extern Int32 FLTKassessment;
extern char FLTKlastMoveStr[40];
extern char FLTKbestlineStr[500];

// timing
extern struct timespec compStart, compEnd; // for timings
extern double totalCompTime;
extern Int32 totalCompMoves;
extern Int32 totalCompDepths;
extern double totalHumanTime;
extern Int32 totalHumanMoves;

// normal (0) or setup mode (1)
extern Int32 setupMouseMode;

//  raises colour component from the black
double lifts(double comp, double lift)
{
  double val = lift * (1.0 - comp) + comp;
  if (val > 1.0)
    val = 1.0;
  if (val < lift)
    val = lift;
  return (val);
}
// tweak rgb to show chosen move
void highlightDisplayMove(int ii, int jj, double *bred, double *bgrn, double *bblu, double *ALPHA)
{
  // swap around if shown from black end
  if (FLTKPLAYER == BLACK)
  {

    if (DisplayMove.from[0] == (7 - (ii + 4)) && DisplayMove.from[1] == (7 - (jj + 4))) // if the 'from' move
    {
      *bred = 0.9;
      *bgrn = 1.0;
      *bblu = 0.9;
      *ALPHA = 0.9;
    }
    return;
  }
  // if player is white
  if (DisplayMove.from[0] == (ii + 4) && DisplayMove.from[1] == (jj + 4)) // if the 'from' move
  {
    *bred = 0.9;
    *bgrn = 1.0;
    *bblu = 0.9;
    *ALPHA = 0.9;
  }
}
// code doesn't seem to find HAVE_GL - this fixes editor fade-out, but comes back further down...
// #ifndef HAVE_GL
// #define HAVE_GL
// #endif

#if HAVE_GL
BoardView::BoardView(int x, int y, int w, int h, const char *l)
    : Fl_Gl_Window(x, y, w, h, l)
#else
BoardView::BoardView(int x, int y, int w, int h, const char *l)
    : Fl_Box(x, y, w, h, l)
#endif /* HAVE_GL */
{
  Fl::use_high_res_GL(1);
  vAng = 0.0;
  hAng = 0.0;
  size = 0.999;
  alpha1 = 0.65;
  yshift = 0.0;

#if !HAVE_GL
  label("OpenGL is required for this demo to operate.");
  align(FL_ALIGN_WRAP | FL_ALIGN_INSIDE);
#endif /* !HAVE_GL */
}

// #if HAVE_GL
void BoardView::drawBoard()
{
  // on manual resize need to draw pieces different size (default size presumes 630 x 630 square)
  resizeX = w();
  resizeY = h();

  double ALPHA;
  glShadeModel(GL_FLAT);

  // shade to the edges (the -4.3 to +4.3 box)
  boxv0[2] = 0.; // z=0
  glBegin(GL_QUADS);
  glColor4f(0.5, 0.5, 0.5, 1.0); // R,G,B,alpha
  boxv0[0] = -4.3;
  boxv0[1] = -4.3;
  glVertex3fv(boxv0);
  boxv0[0] = 4.3;
  glVertex3fv(boxv0);
  boxv0[1] = 4.3;
  glVertex3fv(boxv0);
  boxv0[0] = -4.3;
  glVertex3fv(boxv0);
  glEnd();

  // Draw the chequered squares
  glBegin(GL_QUADS);
  for (int ii = -4; ii < 4; ++ii)
  {
    for (int jj = -4; jj < 4; ++jj)
    {
      double sqContrast = 0.9; // from 0.00001(b&w squares same) to 1 (very different)
      double contInv = 1.0 / sqContrast;
      double lift = 0.5;
      double dc = (1.0 - 1 / contInv) / 2.0;
      double bred = dc + (double)((ii + jj + 8) % 2) / contInv;
      double bgrn = dc + (double)((ii + jj + 8) % 2) / contInv;
      double bblu = dc + (double)((ii + jj + 8) % 2) / contInv;
      bred = lifts(bred, lift); // brighten darker squares
      bgrn = lifts(bgrn, lift);
      bblu = lifts(bblu, lift);
      ALPHA = alpha1;
      highlightDisplayMove(ii, jj, &bred, &bgrn, &bblu, &ALPHA); // highlights picked squares
      glColor4f(bred, bgrn, bblu, ALPHA);
      boxv0[0] = ii;
      boxv0[1] = jj;
      boxv0[2] = 0.;
      glVertex3fv(boxv0);
      boxv0[0] = ii + 1;
      boxv0[1] = jj;
      boxv0[2] = 0.;
      glVertex3fv(boxv0);
      boxv0[0] = ii + 1;
      boxv0[1] = jj + 1;
      boxv0[2] = 0.;
      glVertex3fv(boxv0);
      boxv0[0] = ii;
      boxv0[1] = jj + 1;
      boxv0[2] = 0.;
      glVertex3fv(boxv0);
    }
  }
  glEnd();

  float linBright = 0.35; //  brightness of line around squares
  glColor3f(linBright, linBright, linBright);
  glBegin(GL_LINES);
  // vertical lines
  for (int ii = -4; ii <= 4; ++ii)
  {
    boxv0[0] = ii;
    boxv0[1] = -4.;
    boxv0[2] = 0.;
    glVertex3fv(boxv0);
    boxv0[0] = ii;
    boxv0[1] = 4.;
    boxv0[2] = 0.;
    glVertex3fv(boxv0);
  }
  // horizontal lines
  for (int jj = -4; jj <= 4; ++jj)
  {
    boxv0[0] = -4.;
    boxv0[1] = jj;
    boxv0[2] = 0.;
    glVertex3fv(boxv0);
    boxv0[0] = 4.;
    boxv0[1] = jj;
    boxv0[2] = 0.;
    glVertex3fv(boxv0);
  }
  glEnd();

  glPixelZoom(bitmapZoom, bitmapZoom);

  // draw a piece. pieces go from -6 (black king) through black queen (-5),rook, ... white queen (5), white king = 6 ..
  // .. no piece assigned to 0

  int64_t ix, iy, piece;
  for (ix = 0; ix < 8; ++ix)
  {
    if (FLTKPLAYER != BLACK)
    {
      for (iy = 7; iy >= 0; --iy) // best generally from 'other' end
      {
        piece = DisplayBoard.board[ix][iy]; // see whats there
        drawPiece(ix - 4, iy - 4, piece);
      }
    }
    else
    {
      for (iy = 0; iy <= 7; ++iy) // if black draw white pieces first for foreground effect
      {
        piece = DisplayBoard.board[ix][iy];       // see whats there
        drawPiece(7 - ix - 4, 7 - iy - 4, piece); // swap  'x' and 'y' here
      }
    }
  }

  
  if(floatingPiece !=0)
  {

    drawFloatingPiece(floatingx, floatingy, floatingPiece); // swap  'x' and 'y' here

  }

  // add rank and file lettering, first: files a-h
  boxv0[0] = -3.53; // initial position of file 'a' (just before 0.5 for symmetry)
  boxv0[2] = 0.;    // z=0
  char rankOrFile = 'a';
  glColor3f(0.8, 0.8, 0.8);

  for (Int32 ii = -4; ii < 4; ++ii)
  {
    boxv0[1] = -4.2; // y position (lower one)
    glRasterPos3f(boxv0[0], boxv0[1], boxv0[2]);
    glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, (int)rankOrFile);
    boxv0[1] = 4.1; // y position (upper one)
    glRasterPos3f(boxv0[0], boxv0[1], boxv0[2]);
    glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, (int)rankOrFile);

    boxv0[0] += 1.0; // increase position by 1
    rankOrFile += 1; // and the character ..b,c,d,...
  }

  // then ranks 1-8
  float incy = 1.0; // increment for upwards direction
  if (FLTKPLAYER != BLACK)
    boxv0[1] = -3.55; // initial y-position of rank '1'
  else
  {
    boxv0[1] = 3.45; // for drawing upside down
    incy = -1.0;
  }
  rankOrFile = '1';

  for (Int32 ii = -4; ii < 4; ++ii)
  {
    boxv0[0] = -4.2; // x position (LHS one)
    glRasterPos3f(boxv0[0], boxv0[1], boxv0[2]);
    glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, (int)rankOrFile);
    boxv0[0] = 4.1; // y position (RHS one)

    glRasterPos3f(boxv0[0], boxv0[1], boxv0[2]);
    glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, (int)rankOrFile);

    boxv0[1] += incy; // increase y-position by 1
    rankOrFile += 1;  // and the character ..2,3,4,...
  }

  /* char namstr[10]; strcpy(namstr,"a"); // *for more complex stuff*
  Int32 lenGL = strlen(namstr);
  for (Int32 iGL = 0; iGL < lenGL; iGL++)
    glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, (int)namstr[iGL]);
  // _10 , _18 or GLUT_BITMAP_HELVETICA_12  GLUT_BITMAP_TIMES_ROMAN_24 also possible */

} // drawBoard

void BoardView::drawPiece(int64_t ix, int64_t iy, int64_t piece)
{
  if (piece == 0) // empty square, show nothing
    return;

  // printf("\n siz.... %d %d",resizeX, resizeY);
  int64_t imNumber = piece + 6;
  float xoffset = ((float)piecePPMs[imNumber].xwid) / 2000.; // 1 board square ~ 75 pixels, so ...
  float yoffset = ((float)piecePPMs[imNumber].ywid) / 3000.;

  boxv0[0] = (float)ix;
  boxv0[1] = (float)iy;
  boxv0[2] = 0.;
  glPixelZoom((bitmapZoom * (float)resizeX) / 630., (bitmapZoom * (float)resizeY) / 630.);
  glRasterPos3f(boxv0[0] + xoffset, boxv0[1] + yoffset, boxv0[2]);
  glDrawPixels(piecePPMs[imNumber].xwid,
               piecePPMs[0].ywid,
               GL_RGBA, GL_UNSIGNED_BYTE,
               piecePPMs[imNumber].image);
}

// for drawing moving pieces (during setup)
void BoardView::drawFloatingPiece(int64_t mosx, int64_t mosy, int64_t piece)
{
  if (piece == 0) // empty square, show nothing
    return;
// need to convert from scrren coordinates (0 - 630(ish) to 0-7

  // printf("\n siz.... %d %d",resizeX, resizeY);
  int64_t imNumber = piece + 6;
  float xoffset = ((float)piecePPMs[imNumber].xwid) / 2000.; // 1 board square ~ 75 pixels, so ...
  float yoffset = ((float)piecePPMs[imNumber].ywid) / 3000.;


  // -4 for -4 to +4 board, 0.25 to move from bottom corner
  float fmosx = pixelToBoardIndex(mosx, 'x')-4. -0.5; 
  float fmosy = pixelToBoardIndex(mosy, 'y')-4. -0.4;


 //printf("\n before %ld %ld, after mosx mosy = %f %f",mosx,mosy,fmosx,fmosy);
  //   fmosx=-1.;fmosy=2.5; // this shows between d7 and d8
  boxv0[0] = (float)fmosx;
  boxv0[1] = (float)fmosy;
  boxv0[2] = 0.;
  glPixelZoom((bitmapZoom * (float)resizeX) / 630., (bitmapZoom* (float)resizeY) / 630.);
  glRasterPos3f(boxv0[0] + xoffset, boxv0[1] + yoffset, boxv0[2]);
  glDrawPixels(piecePPMs[imNumber].xwid,
               piecePPMs[0].ywid,
               GL_RGBA, GL_UNSIGNED_BYTE,
               piecePPMs[imNumber].image);
}

float BoardView::pixelToBoardIndex(int pixIn, char xORy)
{
  // currently 1- 630 maps to -4.3 to 4.3 (glortho); user resizing cause resizeX and resizeY to change from (630,630)
  float val;
  if (xORy == 'y') // y index decreases with increasing pixel position
    val = ((float)(resizeY - pixIn)) * 8.6 / resizeY - 0.3;
  else
    val = ((float)pixIn) * 8.6 / resizeX - 0.3;

  return (val);
}

int BoardView::handle(int event) // doesnt allow screen to be shown without flw, is never called with it!
{
  static int mousex, mousey;
  int newmousex, newmousey; // for moved positions
  int key;
  static float boardx, boardy;
  float newboardx, newboardy; // for moved positions
  static float storeboardx,storeboardy; // to keep the old square position
  static int pickedPiece, newpickedPiece;
  static int lastDeletedPiece = 0;
  static float lastDeletedx=0., lastDeletedy = 0.;

  key = Fl::event_key();
  //printf("\n event %d key %d mode %d*************",event,key,setupMouseMode);


  if (setupMouseMode == 1) // for setup routine only
  {
    switch (event)
    {
      
      case FL_PUSH:
      key = Fl::event_key();
      if(key == FL_Button + 3) // right mouse button - pick for delete up on press
      {
      mousex = Fl::event_x();
      mousey = Fl::event_y();
      boardx = pixelToBoardIndex(mousex, 'x');
      boardy = pixelToBoardIndex(mousey, 'y');
      }

      if(key == FL_Button + 1) // left mouse button - pick for move on press
      {
      mousex = Fl::event_x();
      mousey = Fl::event_y();
      boardx = pixelToBoardIndex(mousex, 'x');
      boardy = pixelToBoardIndex(mousey, 'y');
      pickedPiece = DisplayBoard.board[(Int32)boardx][(Int32)boardy];
      storeboardx=boardx;
      storeboardy=boardy;

      // and for 'floating' display of piece being dragged
      floatingx = mousex;
      floatingy=mousey;
      floatingPiece = pickedPiece;
      //printf("\n event %d key %d picked piece %d*************",event,key,pickedPiece);fflush(stdout);
      }

      return 1; // and back

      case FL_RELEASE:
      key = Fl::event_key();

      if(key == FL_Button + 3) //  do it on right mouse button release
      {
        // before deleting, store the piece
        if(DisplayBoard.board[(Int32)boardx][(Int32)boardy] !=0) // if an actual piece
        {
          lastDeletedPiece = DisplayBoard.board[(Int32)boardx][(Int32)boardy];
          lastDeletedx = boardx;
          lastDeletedy = boardy;
        }
        DisplayBoard.board[(Int32)boardx][(Int32)boardy] =0; // empty square
      }

      if(key == FL_Button + 1) //  move piece on right mouse button release
      {
        newmousex = Fl::event_x();
        newmousey = Fl::event_y();
        newboardx = pixelToBoardIndex(newmousex, 'x');
        newboardy = pixelToBoardIndex(newmousey, 'y');
        // see if any piece on moved to square
        newpickedPiece = DisplayBoard.board[(Int32)newboardx][(Int32)newboardy];

        if(newpickedPiece == 0 && pickedPiece !=0) // if something on old square and nothing on new square
        {
          DisplayBoard.board[(Int32)newboardx][(Int32)newboardy] = pickedPiece;
          DisplayBoard.board[(Int32)storeboardx][(Int32)storeboardy] = 0; // empty old square
        }
        floatingPiece = 0; // and un-select a piece for 'floating' display
      }

      if(key == FL_Button + 2) //  undelete last deleted piece on centre mouse button release
      {
        if(lastDeletedPiece !=0)
        {
          if(DisplayBoard.board[(Int32)lastDeletedx][(Int32)lastDeletedy] ==0) // if old square empty
          { // put it back in original place
            DisplayBoard.board[(Int32)lastDeletedx][(Int32)lastDeletedy] = lastDeletedPiece;
            //lastDeletedPiece = 0; // commented to allow repeated adds
          }
          else // try at current square 
          {
            newmousex = Fl::event_x();
            newmousey = Fl::event_y();
            newboardx = pixelToBoardIndex(newmousex, 'x');
            newboardy = pixelToBoardIndex(newmousey, 'y');
        // see if any piece on new square
           newpickedPiece = DisplayBoard.board[(Int32)newboardx][(Int32)newboardy];
           if(newpickedPiece ==0) // nothing there, so place it there
           {
            DisplayBoard.board[(Int32)newboardx][(Int32)newboardy] = lastDeletedPiece;
            //lastDeletedPiece = 0; commented to allow repeated adds
           }
          }
        }
      }
      return 1;

      case FL_DRAG:  // for floating piece on dragging
      key = Fl::event_key();
      if(key == FL_Button + 1) // left mouse button 
      {
      mousex = Fl::event_x();
      mousey = Fl::event_y();
      //printf("\n drag-left button %d mouse x,y = %d, %d \n", key,mousex,mousey);
      // and for 'floating display
      floatingx = mousex;
      floatingy=mousey;
      }
      return 1;

      default:
      // following is the 'magic'line return anyting else to the base class
      return Fl_Gl_Window::handle(event);
    }
  }
  else // normal behaviour
  {

    switch (event)
    {
      // this works, though FL_KEYBOARD has value 8, which never triggers. 9 used here
      // just for future reference
      #define MY_FL_KEYBOARD 9
      case MY_FL_KEYBOARD: 
        // Fetch the character code or special key code
       key = Fl::event_key(); 
        
        printf("\n key pressed -");fflush(stdout);
        
        // If it's a printable character, show it
        if (key >= 32 && key <= 126) {
          printf("\n -- printable - %d",key);fflush(stdout);

        }
        
        return 1;

    case FL_DRAG:
      // ** mouse drag not used in this app, but code kept as examples**
      /* float dx, dy; static int lastx, lasty;
       key = Fl::event_key(); mousex = Fl::event_x(); mousey = Fl::event_y();
       // translate with left mouse button
       if (key == FL_Button + 1)
       {
         dx = (float)mousex - lastx; dy = (float)mousey - lasty;
         printf("\n drag-left button %d \n", key);
         printf(" mouse x, y %f %f changes x,y %f %f", pixelToBoardIndex(mousex, 'x'), pixelToBoardIndex(mousey, 'y'), dx, dy);
       }
       if (key == FL_Button + 3)
         //  printf("\n drag -right button %d \n", key);
       if (key == FL_Button + 2)
         // printf("\n drag -mid button %d \n", key);
       lastx = mousex; lasty = mousey; redraw();
       return 1;
       */

    case FL_PUSH:
      /*printf("\n push (any button) %d", event);
      mousex = Fl::event_x();
      mousey = Fl::event_y(); */
      return 1;

    case FL_RELEASE:
      // printf("\n release (any button) %d", event);
      key = Fl::event_key();
      mousex = Fl::event_x();
      mousey = Fl::event_y();

      boardx = pixelToBoardIndex(mousex, 'x');
      boardy = pixelToBoardIndex(mousey, 'y');
      if (FLTKPLAYER == BLACK) // swap around if board shown upside-down
      {
        boardx = 8 - boardx;
        boardy = 8 - boardy;
      }
      // printf("\n board position %.4f %.4f",boardx, boardy);fflush(stdout);

      // find appropriate place to put into move from Board
      if (key == FL_Button + 1) // if left button
      {
        if (DisplayMoveStatus == 1) // descending order matters as incremented
        {
          DisplayMove.to[0] = (Int32)boardx;
          DisplayMove.to[1] = (Int32)boardy;
          ++DisplayMoveStatus;
        }
        if (DisplayMoveStatus == 0)
        {
          DisplayMove.from[0] = (Int32)boardx;
          DisplayMove.from[1] = (Int32)boardy;
          DisplayMove.to[0] = -1;
          DisplayMove.to[1] = -1;
          ++DisplayMoveStatus;
        }

        // check its not same square repeated, if so cancel
        if (DisplayMoveStatus >= 2 && DisplayMove.to[0] == DisplayMove.from[0] &&
            DisplayMove.to[1] == DisplayMove.from[1])
        {                           // cancel move
          DisplayMoveStatus = 0;    //  resets
          DisplayMove.from[0] = -1; // signal clear to other sections
          DisplayMove.to[0] = -1;
        }

        if (DisplayMoveStatus >= 2) // have a move to use, as long as it's a piece (!=0)and not taking own colour
        {
          printf("\n move %d %d to %d %d",
                 DisplayMove.from[0], DisplayMove.from[1], DisplayMove.to[0], DisplayMove.to[1]);

          // piece2 must be 0 or opposite sign(=opposite side)
          Int32 piece = DisplayBoard.board[DisplayMove.from[0]][DisplayMove.from[1]]; // 'from' piece
          Int32 piece2 = DisplayBoard.board[DisplayMove.to[0]][DisplayMove.to[1]];    //'to' piece
          printf("- piece = %d", piece);

          if (piece != 0 && piece * piece2 <= 0) // if moving a piece, and not taking own colour
          {
            // plays the move if it's legal, and FLTKCURRENTSIDE flipped and moveNumber incremented
            FLTKHASH = hashFullPosition(&DisplayBoard);
            if (FLTKCURRENTSIDE == WHITE)
              FLTKHASH = toggleWhiteToPlayHash(FLTKHASH); // this doesn't toggle, so start with correct one

            Int32 isGood = tryPlayerMove(FLTKfptr, DisplayMove, &FLTKCURRENTSIDE, FLTKPLAYER, FLTKHASH);
            // printf("\n players: isgood = %d", isGood);
            // fflush(stdout);
            if (isGood == 1)
            {
              // start clock here if on the first move .. doesnt seem to work unless its 2 - not sure why
              if (moveNumber <= 2)
                clock_gettime(CLOCK_REALTIME, &compStart);

              printf("\n movenumber = %d", moveNumber);

              // update times
              clock_gettime(CLOCK_REALTIME, &compEnd); // finishing time
              double secs = (double)(compEnd.tv_sec - compStart.tv_sec) + (1e-9) * (double)(compEnd.tv_nsec - compStart.tv_nsec);
              // printf("\n hdl:time taken for move = %.3f sec.", secs);

              totalHumanTime += secs;
              totalHumanMoves += 1;
              double flmins = totalHumanTime / 60.;
              char flstr[40];
              sprintf(flstr, "%.2f", flmins); // humans total in minutes
              FLTKbrdui->humTot_to->value(flstr);
              double flavg = totalHumanTime / ((double)totalHumanMoves);
              sprintf(flstr, "%.2f", flavg); // comps average per move in seconds
              FLTKbrdui->humAv_to->value(flstr);
              sprintf(flstr, "%.2f", secs); // final comp time for move
              FLTKbrdui->current_to->value(flstr);
              clock_gettime(CLOCK_REALTIME, &compStart); // resets time at end of players's = start of computer's move
            }
            // cant get aything to redraw pieces here (draw(), redraw(), drawBoard() )
            FLTKbrdui->cube->drawBoard();
            ;
            // gets computer's move in an idle function now
          }
          // reset, whether its moved or wasn't a plasible move
          DisplayMoveStatus = 0;    //  resets
          DisplayMove.from[0] = -1; // signal clear to other sections
          DisplayMove.to[0] = -1;
        }
      }
      else
      {
        DisplayMoveStatus = 0;    // mid or right button resets
        DisplayMove.from[0] = -1; // signal clear to other sections
        DisplayMove.to[0] = -1;
      }
      // printf(" position %.3f %.3f", boardx, boardy);
      fflush(stdout);
      redraw();
      return 1;

    default:
      // following is the 'magic'line return anyting else to the base class
      return Fl_Gl_Window::handle(event);

    } // end switch event
  } // end setupmousemode default (normal) else
}

void BoardView::draw()
{
  if (!valid())
  {
    glLoadIdentity();
    glViewport(0, 0, pixel_w(), pixel_h());
    glOrtho(-4.3, 4.3, -4.3, 4.3, -20050, 10000);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  }

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glPushMatrix();
  double xshift = 0.0;
  glTranslatef((GLfloat)xshift, (GLfloat)yshift, 0);
  glRotatef((GLfloat)hAng, 0, 1, 0);
  glRotatef((GLfloat)vAng, 1, 0, 0);
  glScalef(float(size), float(size), float(size));

  // this does the actual drawing
  drawBoard();

  glPopMatrix();
}
// [\endcode in documentation/src/fluid.dox]

// #endif /* HAVE_GL */

void readPiecePPMs()
{
  for (int32_t ii = 0; ii < 13; ++ii)
    piecePPMs[ii].image = NULL;

  char filename[100];
  int64_t cnt = 0;
  // first one mangles (windows only) so first read a one into the spare location ([6])
  strcpy(filename, "./classicPieces/bk.ppm");
  readPPMimage(filename, &piecePPMs[6]);

  strcpy(filename, "./classicPieces/bk.ppm"); // black king
  cnt += readPPMimage(filename, &piecePPMs[0]);
  strcpy(filename, "./classicPieces/bq.ppm"); // .. queen
  cnt += readPPMimage(filename, &piecePPMs[1]);
  strcpy(filename, "./classicPieces/br.ppm"); // ..
  cnt += readPPMimage(filename, &piecePPMs[2]);
  strcpy(filename, "./classicPieces/bb.ppm"); // ..
  cnt += readPPMimage(filename, &piecePPMs[3]);
  strcpy(filename, "./classicPieces/bn.ppm"); // ..
  cnt += readPPMimage(filename, &piecePPMs[4]);
  strcpy(filename, "./classicPieces/bp.ppm"); // ..
  cnt += readPPMimage(filename, &piecePPMs[5]);
  strcpy(filename, "./classicPieces/wp.ppm"); // white pawn
  cnt += readPPMimage(filename, &piecePPMs[7]);
  strcpy(filename, "./classicPieces/wn.ppm"); // white knight
  cnt += readPPMimage(filename, &piecePPMs[8]);
  strcpy(filename, "./classicPieces/wb.ppm"); // ..
  cnt += readPPMimage(filename, &piecePPMs[9]);
  strcpy(filename, "./classicPieces/wr.ppm"); // white rook
  cnt += readPPMimage(filename, &piecePPMs[10]);
  strcpy(filename, "./classicPieces/wq.ppm"); // white quees
  cnt += readPPMimage(filename, &piecePPMs[11]);
  strcpy(filename, "./classicPieces/wk.ppm"); // white king
  cnt += readPPMimage(filename, &piecePPMs[12]);

  printf("\n read %ld piece images", cnt);
  fflush(stdout);
  // add an alpha layer
  for (int64_t ii = 0; ii < 13; ++ii)
    RgbToRGBA(&piecePPMs[ii]);

  // reduce alpha outside the actual piece design
  for (int64_t ii = 0; ii < 13; ++ii)
    softenRGBA(&piecePPMs[ii]);
}

// just here temporarily
// sets up standard starting position
void tempstartingBoard(position_t newBoard)
{
  printf("\n *** temp board routine! ***\n");
  //  pawns (1)
  for (int32_t ii = 0; ii < 8; ++ii)
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
}
