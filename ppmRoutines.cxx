#include "ppmRoutines.h"
#include <stdlib.h>

int testmain()
{
  ppm_t testImage;
  testImage.image = NULL;
  char fname[50];
  strcpy(fname, "./classicPieces/bb.ppm");
  readPPMimage(fname, &testImage);

  strcpy(fname, "testbb.ppm");
  printf("\n got %s %ld %ld %ld", fname, testImage.xwid, testImage.ywid, testImage.ncol);

  writePPMimage(fname, testImage);

  freePPMimage(testImage);
  printf("/n here");
  return (0);
}

int64_t readPPMimage(char *filename, ppm_t *InImage)
{

  FILE *fin;
  char instring[200];
  int64_t ival, AsciiFormat;
  int64_t in, inmax, incol;

  if (InImage->image != (unsigned char *)NULL)
  {
    printf("\n WARNING, input image storage not NULL");
    return (0); // already got it
  }

  fin = fopen(filename, "rb");
  if (fin == (FILE *)NULL)
  {
    printf("\n image file %s not found", filename);
    return (0);
  }

  char *ret = fgets(instring, 199, fin);
  if (ret == (char *)0)
    printf("\n oh dear!");
  if (instring[0] == 80 && instring[1] == 50) // P2
  {
    InImage->ncol = 1;
    incol = 1;
    AsciiFormat = 1;
  }
  else if (instring[0] == 80 && instring[1] == 51) // P3
  {
    InImage->ncol = 3;
    incol = 3;
    AsciiFormat = 1;
  }
  else if (instring[0] == 80 && instring[1] == 53) // P5
  {
    InImage->ncol = 1;
    incol = 1;
    AsciiFormat = 0;
  }
  else if (instring[0] == 80 && instring[1] == 54) // P6
  {
    InImage->ncol = 3;
    incol = 3;
    AsciiFormat = 0;
    // fprintf(debugfile," have P6 ncol=3\n");fflush(debugfile);
  }
  else
  {
    printf("%s is not a P2 or P5pgm  or a P3 or P6 PPM\n\n", filename);
    return (0);
  }

  ret = fgets(instring, 199, fin);
  while (instring[0] == '#')
    ret = fgets(instring, 199, fin);
  int64_t v1, v2;
  sscanf(instring, "%ld %ld", &v1, &v2);

  printf("\n warning temp limit on v1 v2 in ppm read");
  if(v1 < 20) v1=20;
  if(v1>150)v1=150;
  if(v2<20)v2=20;
  if(v2>150)v2=150;
  
  InImage->xwid = (int64_t)v1;
  InImage->ywid = (int64_t)v2; // not storing correctly in 1st one read in Windows...
  // and get maximum
  ret = fgets(instring, 199, fin);
  int64_t mx;
  sscanf(instring, "%ld", &mx);

  // printf("\n %ld %ld %ld\n",InImage->xwid,InImage->ywid, InImage->ncol);

  inmax = (int64_t)v1 * v2;
  InImage->image = (unsigned char *)malloc(incol * inmax * sizeof(unsigned char));

  int64_t resread = 0;
  if (AsciiFormat == 1)
  {

    for (in = 0; in < inmax * (incol); in++)
    {
      resread = fscanf(fin, "%ld", &ival);
      InImage->image[in] = (unsigned char)ival;
    }
    if (resread == 0)
      printf("\n oh dear ..!");
  }
  else
  { // binary format
    for (in = 0; in < inmax * (incol); in++)
    {
      resread = fread(&InImage->image[in], 1, 1, fin);
    }
    if (resread == 0)
      printf("\n oh dear ...!");
  }
  fclose(fin);

  // flip image vertically
  unsigned char *im2 = (unsigned char *)malloc((incol)*inmax * sizeof(unsigned char));

  int64_t iix, iiy, xwid2, ywid2;
  xwid2 = InImage->xwid;
  ywid2 = InImage->ywid;
  for (iiy = 0; iiy < InImage->ywid; iiy++)
  {
    for (iix = 0; iix < 3 * InImage->xwid; iix++)
    {
      im2[iix + 3 * xwid2 * (ywid2 - 1 - iiy)] = InImage->image[iix + 3 * xwid2 * iiy]; // fiddly!
    }
  }
  // free previous 'mallocation', and copy in new
  free(InImage->image);
  InImage->image = im2;

  // convert to RGBA
  // RgbToRGBA(InImage);

  return (1);

} // end readPPMimage()

// ads an alpha layer to normal RGB
void RgbToRGBA(ppm_t *ppmImage)
{
  int64_t ncol2, iix, iiy;
  int64_t ncol = ppmImage->ncol;
  int64_t xwid = ppmImage->xwid;
  int64_t ywid = ppmImage->ywid;

  unsigned char *imin = ppmImage->image;
  if (imin == NULL) // if no image there
    return;

  ncol2 = ncol + 1; // extra layer for alpha
  unsigned char *imout = (unsigned char *)malloc((ppmImage->xwid * ppmImage->ywid * ncol2) * sizeof(unsigned char));

  for (iiy = 0; iiy < ywid; iiy++)
  {
    for (iix = 0; iix < xwid; iix++)
    {
      imout[iix * ncol2 + iiy * xwid * ncol2] = imin[iix * ncol + iiy * xwid * ncol];         // red components
      imout[iix * ncol2 + iiy * xwid * ncol2 + 1] = imin[iix * ncol + iiy * xwid * ncol + 1]; // green components
      imout[iix * ncol2 + iiy * xwid * ncol2 + 2] = imin[iix * ncol + iiy * xwid * ncol + 2]; // blue components

      char xxx = 128;
      imout[iix * ncol2 + iiy * xwid * ncol2 + 3] = xxx; // alpha components
    }
  }
  free(ppmImage->image);

  ppmImage->ncol = ncol2; // update to 4;
  ppmImage->image = imout;
  return;
}

// reduces alpha outside main drawing of piece
void softenRGBA(ppm_t *ppmImage)
{
  int64_t iix, iiy;
  int64_t ncol = ppmImage->ncol;
  int64_t xwid = ppmImage->xwid;
  int64_t ywid = ppmImage->ywid;
  unsigned char *imin = ppmImage->image;
  if (imin == NULL || ncol != 4) // don't do if no image there or doesn't have 4 layers (r,g,b,alpha)
    return;

  char alphaFg = 255;
  for (iiy = 0; iiy < ywid; iiy++)
    for (iix = 0; iix < xwid; iix++)
      imin[iix * ncol + iiy * xwid * ncol + 3] = alphaFg; // set foreground alpha components


  // start in a corner and let it recurse through the lot
  int64_t threshold =  1*128; // ** have had seg faults from value here - keep an eye! **
  softenAlpha(0, 0, ppmImage, threshold); // main recursive one
  deblipAlpha(ppmImage, threshold); // removes a few missed blips (probably none left after other tweaks)

 
  return;
}

// a reursive 'softener' which reduce alpha away from the main piece 'drawing'
void softenAlpha(int64_t iix, int64_t iiy, ppm_t *ppmImage, int64_t threshold)
{
  int64_t ncol = ppmImage->ncol;
  int64_t xwid = ppmImage->xwid;
  int64_t ywid = ppmImage->ywid;

  if (iix < 0 || iiy < 0 || iix >= xwid || iiy >= ywid)
    return;

  unsigned char *imin = ppmImage->image;
  fflush(stdout);

  int64_t iptr = iix * ncol + iiy * xwid * ncol;
  if (imin[iptr + 3] ==0) // check if alpha is already zeroed
    return;

  int64_t pixelSum = (int64_t)imin[iptr] + (int64_t)imin[iptr + 1] + (int64_t)imin[iptr + 2]; // R + G + B values

  if (pixelSum < threshold) // have a dark line
    return;
  else
  { // set alpha to 0 and set adjacent (recursive) instances off
    imin[iptr + 3] = 0;
    softenAlpha(iix + 1, iiy, ppmImage, threshold);
    softenAlpha(iix - 1, iiy, ppmImage, threshold);
    softenAlpha(iix, iiy + 1, ppmImage, threshold);
    softenAlpha(iix, iiy - 1, ppmImage, threshold);
    softenAlpha(iix + 1, iiy + 1, ppmImage, threshold);
    softenAlpha(iix + 1, iiy - 1, ppmImage, threshold);
    softenAlpha(iix - 1, iiy + 1, ppmImage, threshold);
    softenAlpha(iix - 1, iiy - 1, ppmImage, threshold);
  }
  return;
}

// gets right into the edges
void deblipAlpha( ppm_t *ppmImage, int64_t threshold)
{
  int64_t ncol = ppmImage->ncol;
  int64_t xwid = ppmImage->xwid;
  int64_t ywid = ppmImage->ywid;


  unsigned char *imin = ppmImage->image;

  for(int64_t ix=0; ix<xwid;++ix)
  {
    for(int64_t iy=0; iy<ywid;++iy)
    {
      
      int64_t iptr = ix * ncol + iy * xwid * ncol;
      if(imin[iptr+3]!= 0)
      { // check if its a candidate
        int64_t pixelSum = (int64_t)imin[iptr] + (int64_t)imin[iptr + 1] + (int64_t)imin[iptr + 2]; // R + G + B values
        if (pixelSum > threshold) // is light
        {
          int64_t foundNearby = 0;
          // check surrounding square for an alpha = 0
          if(ix <(xwid-1))
            if(imin[(ix+1) * ncol + iy * xwid * ncol +3] ==0) foundNearby =1;
          if(ix >0)
            if(imin[(ix-1) * ncol + iy * xwid * ncol +3] ==0) foundNearby =1;
          if(iy <(ywid-1))
             if(imin[(ix+0) * ncol + (iy+1) * xwid * ncol +3] ==0) foundNearby =1;
          if(iy > 0)
            if(imin[(ix+0) * ncol + (iy-1) * xwid * ncol +3] ==0) foundNearby =1;
          if(ix <(xwid-1) && iy<(ywid-1))
            if(imin[(ix+1) * ncol + (iy+1) * xwid * ncol +3] ==0) foundNearby =1;
          if(ix >0  && iy<(ywid-1))
            if(imin[(ix-1) * ncol + (iy+1) * xwid * ncol +3] ==0) foundNearby =1;
          if(ix <(xwid-1) && iy>0)
            if(imin[(ix+1) * ncol + (iy-1) * xwid * ncol +3] ==0) foundNearby =1;
          if(ix >0 && iy>0)
            if(imin[(ix-1) * ncol + (iy-1) * xwid * ncol +3] ==0) foundNearby =1;

          if(foundNearby >0)
            imin[iptr+3] = 0;// drop the alpha if one found adjacent
        }

      }
    }
  }
  return;
}

// write a basic ppm_t
void writePPMimage(char *filename, ppm_t InImage)
{
  FILE *fout;
  fout = fopen(filename, "wb");
  printf("\n writing %s %ld %ld %ld", filename, InImage.xwid, InImage.ywid, InImage.ncol);
  if (fout == (FILE *)NULL)
  {
    printf("\n image file %s can't be opened", filename);
    return;
  }

  fprintf(fout, "P6\n%ld %ld\n255\n", InImage.xwid, InImage.ywid);
  fwrite(InImage.image, (InImage.xwid * InImage.ywid * 3), 1, fout);
  fclose(fout);
}

void freePPMimage(ppm_t InImage)
{
  if (InImage.image != NULL)
    free(InImage.image);
}
