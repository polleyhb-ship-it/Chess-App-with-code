#include "chessMk9.h"

uint64_t pieceHash[13][8][8];
uint64_t whiteToPlayHash;
uint64_t enPassantHash[8][8];
uint64_t WhiteCantCastle[2];// one for KS and one for QS (potentially)
uint64_t BlackCantCastle[2];// one for KS and one for QS (potentially)

#define hashIndexingBits 18
uint64_t maxHash = ((uint64_t)1 << hashIndexingBits) - 1;

// for search/hit statistics
int64_t exactHashSearches = 0;
int64_t exactHashHits = 0;
int64_t orderingHashSearches = 0;
int64_t orderingHashHits = 0;

// controlling whether hash is used (uses GBs beyond 9 moves)
Int32 doHashing = 1; // set to 0 to turn off hashing completely
Int32 pauseHashing = 1;// initially paused, turned on at start of each move

extern Int32 moveNumber;
extern castlingRights_t castlingRights;

// structure to hold a move in minimum space
typedef struct struct_miniMove
{
  int8_t from[2];
  int8_t to[2];
  Int32 status;
} miniMove_t;

typedef struct struct_hashpoint
{
    uint64_t hashval; // actual Zobrist hash value
    Int32 depthOfSearch; // depth searched beyond assessment point
    Int32 alpha;         // assessed value
    miniMove_t miniMove;  // best move at point
    Int32 vintageOfMove; // base move when calculated, to help with 'retirement'
}hashPoint_t;

typedef struct struct_hashLedge
{
    Int32 maxNum;
    Int32 num;
    hashPoint_t *hashpoint;
} _hashLedge_t;
typedef _hashLedge_t *hashLedge_t;

hashLedge_t ledge = NULL;


// Global state for the splitmix 64-bit generator (seed it with any 64-bit number)
uint64_t rng_state = 123456789ULL; 
uint64_t u64rand()
{

    // SplitMix64 generator apparently great for zobrist hashing ..
    // .. because each bit is a true coin flip
    uint64_t z = (rng_state += 0x9e3779b97f4a7c15ULL);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);


    // old one - apparently very dubious using rand() (linear dependence etc)
    //uint64_t op = 0;
    //uint64_t rdm = (uint64_t)rand();
    //op = rdm << 62;
    //op = op ^ ((uint64_t)rand() << 31);
    //op = op ^ ((uint64_t)rand());
    // //needs to be able to cope with less than 16 bits for windows
    //op=rdm<<56 ^ ((uint64_t)rand() << 48) ^ ((uint64_t)rand() << 40) ^ ((uint64_t)rand() << 32)
    //^ ((uint64_t)rand() << 24) ^ ((uint64_t)rand() << 16) ^ ((uint64_t)rand() <<8) ^ ((uint64_t)rand());
    //return (op);
}

void setupHashTables()
{
    printf("\n maxhash %lu, number of hash ledges %lu", maxHash, maxHash + 1);
    // seed for repeatability
    uint32_t seed = 1234567890;
    srand(seed);

   // uint64_t bitCheck=0; // to check on all bits used
    // note kk=6 (for piece ==0) isn't used
    for (uint64_t kk = 0; kk < 13; ++kk)
    {
        for (uint64_t ii = 0; ii < 8; ++ii)
        {
            for (uint64_t jj = 0; jj < 8; ++jj)
            {
                uint64_t rndNum = u64rand();
                if(kk != 6)
                  pieceHash[kk][ii][jj] = rndNum;
                else
                  pieceHash[kk][ii][jj]=0;
                //bitCheck = bitCheck | rndNum; // should take around 4 numbers to reach 0XFFFF .. FFFF
               // printf("\n random bits or-ed = %lx",bitCheck);
            }
        }
    }

    // and for colours (only white used, black deduced)
        whiteToPlayHash = u64rand();
    
        // and setup hash table for enpassant captures
        for (uint64_t ii = 0; ii < 8; ++ii)
        {
            for (uint64_t jj = 0; jj < 8; ++jj)
            {
                enPassantHash[ii][jj] = u64rand();
            }
        }

    // castling hashes
    WhiteCantCastle[0]=u64rand(); //KS
    WhiteCantCastle[1]=u64rand(); //QS
    BlackCantCastle[0]=u64rand(); //KS
    BlackCantCastle[1]=u64rand(); //QS
    
    // allocate for hash ledges
    setupHashLedges();
}

void setupHashLedges()
{
    // set up hash ledges
    ledge = (hashLedge_t)calloc(maxHash + 1, sizeof(_hashLedge_t));
    Int32 startnum = 10;
    for (int64_t ii = 0; ii <= (int64_t)maxHash; ++ii)
    {
        ledge[ii].hashpoint = (hashPoint_t *)calloc(startnum, sizeof(hashPoint_t));
        ledge[ii].num = 0;
        ledge[ii].maxNum = startnum;
    }
}

void addHashPoint(uint64_t hashval,  Int32 alpha, Int32 depthOfSearch, move_t *inMove)
{
    // don't save if paused
    if(pauseHashing == 1)
      return; 

    int64_t incnum=10;
    uint64_t index = hashval>>(64 - hashIndexingBits);
    // add at ledge 'index'
    // first try to replace a matching hashval ..
    // ..  then try to replace a zero (from retirement pruning), then add

    Int32 zeroLocation = -1;
    // looking for older one
    for(Int32 ii=0; ii < ledge[index].num;++ii)
    {
        if( (ledge[index].hashpoint[ii].hashval == hashval) )
        {
           if (ledge[index].hashpoint[ii].depthOfSearch <= (depthOfSearch)) 
        {
            ledge[index].hashpoint[ii].alpha = alpha;
            ledge[index].hashpoint[ii].depthOfSearch = depthOfSearch;
            ledge[index].hashpoint[ii].vintageOfMove = moveNumber; // base move when calculated for 'retirement' purposes
            // and store successful move
            ledge[index].hashpoint[ii].miniMove.from[0]=(int8_t)inMove->from[0];
            ledge[index].hashpoint[ii].miniMove.from[1]=(int8_t)inMove->from[1];
            ledge[index].hashpoint[ii].miniMove.to[0]=(int8_t)inMove->to[0];
            ledge[index].hashpoint[ii].miniMove.to[1]=(int8_t)inMove->to[1];
            ledge[index].hashpoint[ii].miniMove.status =inMove->status;
            //printf(" replacing hash--");
            
        }
        return; // dont add if there at more depth
    }
        // and check for a zero location in case no match found
        if( (ledge[index].hashpoint[ii].hashval == 0) && zeroLocation < 0)
          zeroLocation = ii; // record the first one (as early ones are found quicker)
    }

    // if got here no matching hash found, but maybe a zero
      if(zeroLocation >= 0)
      {
        ledge[index].hashpoint[zeroLocation].hashval = hashval;
        ledge[index].hashpoint[zeroLocation].alpha = alpha;
        ledge[index].hashpoint[zeroLocation].depthOfSearch = depthOfSearch;
        ledge[index].hashpoint[zeroLocation].vintageOfMove = moveNumber; // base move when calculated for 'retirement' purposes
        // and store successful move
        ledge[index].hashpoint[zeroLocation].miniMove.from[0]=(int8_t)inMove->from[0];
        ledge[index].hashpoint[zeroLocation].miniMove.from[1]=(int8_t)inMove->from[1];
        ledge[index].hashpoint[zeroLocation].miniMove.to[0]=(int8_t)inMove->to[0];
        ledge[index].hashpoint[zeroLocation].miniMove.to[1]=(int8_t)inMove->to[1];
        ledge[index].hashpoint[zeroLocation].miniMove.status =inMove->status;
       // printf(" replacing zero--");
        return; // job done
      }

// if got here, then just add to the end


    //  first check space remains
    if(ledge[index].num >= ledge[index].maxNum) // will need to realloc
    {
        ledge[index].hashpoint = (hashPoint_t*)realloc(ledge[index].hashpoint,
            (ledge[index].maxNum + incnum)*sizeof(hashPoint_t));
            // and zero new ones
        for(Int32 kk=ledge[index].maxNum;kk<(ledge[index].maxNum + incnum);++kk)
           ledge[index].hashpoint[kk].hashval = 0;
          
        ledge[index].maxNum += incnum;
    } // end if not big enough

    // put in num and increment num
    ledge[index].hashpoint[ledge[index].num].hashval = hashval;
    ledge[index].hashpoint[ledge[index].num].alpha = alpha;
    ledge[index].hashpoint[ledge[index].num].depthOfSearch = depthOfSearch;
    ledge[index].hashpoint[ledge[index].num].vintageOfMove = moveNumber; // base move when calculated for 'retirement' purposes
    // and store successful move
    ledge[index].hashpoint[ledge[index].num].miniMove.from[0]=(int8_t)inMove->from[0];
    ledge[index].hashpoint[ledge[index].num].miniMove.from[1]=(int8_t)inMove->from[1];
    ledge[index].hashpoint[ledge[index].num].miniMove.to[0]=(int8_t)inMove->to[0];
    ledge[index].hashpoint[ledge[index].num].miniMove.to[1]=(int8_t)inMove->to[1];
    ledge[index].hashpoint[ledge[index].num].miniMove.status=inMove->status;
    ledge[index].num += 1;

   // printf("\n %lu added at %lu",hashval,index);
}

Int32 getEqualHashPosition(uint64_t hashval,Int32 depthOfSearch, Int32 *alphaVal, move_t *inMove)

{
    // return if paused
    if(pauseHashing == 1)
      return(0);
    ++exactHashSearches; // count overall search tries
    uint64_t index = hashval>>(64 - hashIndexingBits);
    for(Int32 ii=0; ii < ledge[index].num;++ii)
    {
        if( (ledge[index].hashpoint[ii].hashval == hashval) && (ledge[index].hashpoint[ii].depthOfSearch >= (depthOfSearch)) )
        {
            *alphaVal = ledge[index].hashpoint[ii].alpha;
            inMove->from[0] = (Int32)ledge[index].hashpoint[ii].miniMove.from[0];
            inMove->from[1] = (Int32)ledge[index].hashpoint[ii].miniMove.from[1];
            inMove->to[0] = (Int32)ledge[index].hashpoint[ii].miniMove.to[0];
            inMove->to[1] = (Int32)ledge[index].hashpoint[ii].miniMove.to[1];
            inMove->status = ledge[index].hashpoint[ii].miniMove.status;
            ++exactHashHits;
            return(1); // success - found same position
        }
    }
    return(0); // no match found
}

Int32 getPreviousBestHashPosition(uint64_t hashval,Int32 depthOfSearch, Int32 *alphaVal, move_t *inMove)

{
    // return if paused
    if(pauseHashing == 1)
      return(0);

    ++orderingHashSearches; // count overall search tries
    uint64_t index = hashval>>(64 - hashIndexingBits);
    for(Int32 ii=0; ii < ledge[index].num;++ii)
    {
        if( (ledge[index].hashpoint[ii].hashval == hashval) &&
         (ledge[index].hashpoint[ii].depthOfSearch >= (depthOfSearch -8)) ) // -8 allows much less 'solid' move ...
        {
            *alphaVal = ledge[index].hashpoint[ii].alpha;
            inMove->from[0] = (Int32)ledge[index].hashpoint[ii].miniMove.from[0];
            inMove->from[1] = (Int32)ledge[index].hashpoint[ii].miniMove.from[1];
            inMove->to[0] = (Int32)ledge[index].hashpoint[ii].miniMove.to[0];
            inMove->to[1] = (Int32)ledge[index].hashpoint[ii].miniMove.to[1];
            inMove->status = ledge[index].hashpoint[ii].miniMove.status;
            ++orderingHashHits;
            return(1); // success - found same position
        }
    }
    return(0); // no match found
}

void clearHashLedges()
{
        if (ledge != NULL)
    {
        for (int64_t ii = 0; ii <= (int64_t)maxHash; ++ii)
            ledge[ii].num = 0;
    }
}

// nulls the haspoints generated more than maxkeep before now
void removeOldHashpoints(Int32 maxKeep)
{
    for (int64_t ii = 0; ii <= (int64_t)maxHash; ++ii)
    {
        for(Int32 ipt=0; ipt < ledge[ii].num;++ipt)
        {
            if(ledge[ii].hashpoint[ipt].vintageOfMove < (moveNumber - maxKeep))
            ledge[ii].hashpoint[ipt].hashval = (uint64_t)0;
        }
    }
}

void freeHashLedges()
{
    // for o/p stats
    int64_t minn = ((uint64_t)1)<<60; // big number
    int64_t maxn = 0, sumn=0; 

    int64_t numNone=0, num1=0,num2=0,num3=0,num4=0;// for counts
    if (ledge != NULL)
    {
        for (uint64_t ii = 0; ii <= maxHash; ++ii)
        {
            sumn += ledge[ii].num;
            if(ledge[ii].num > maxn)
               maxn = (int64_t)ledge[ii].num;
             if(ledge[ii].num < minn)
               minn = (int64_t)ledge[ii].num;

            if(ledge[ii].num==0) numNone += 1;
            if(ledge[ii].num==1) num1 += 1;
            if(ledge[ii].num==2) num2 += 1;
            if(ledge[ii].num==3) num3 += 1;
            if(ledge[ii].num==4) num4 += 1;


            free(ledge[ii].hashpoint);
            ledge[ii].hashpoint = NULL;
            ledge[ii].num = 0;
            ledge[ii].maxNum = 0;
        }
        free(ledge);
        ledge = NULL;

        printf("\n hash ledge stats:");
        printf("\nTotal stored = %lu, peak no. per ledge %lu lowest no. on a ledge %lu",
            sumn,maxn,minn);
        printf("\n %ld ledges, average no. per ledge = %f", maxHash+1, (double)sumn/(double)(maxHash+1));
        printf("\n ledges with 0: %ld,1: %ld, 2: %ld, 3: %ld and 4: %ld",numNone,num1,num2,num3,num4);
        printf("\n hash search statistics:");
        printf("\n direct searches = %ld; total search hits = %ld, hit rate = %.3f %%",
            exactHashSearches,exactHashHits,100.0*(double)exactHashHits/((double)exactHashSearches+1e-10));
        printf("\n ordering-searches = %ld; total ordering hits = %ld, hit rate = %.3f %%",
                orderingHashSearches,orderingHashHits,100.0*(double)orderingHashHits/((double)orderingHashSearches +1e-10));
    }
}

uint64_t hashFullPosition(position_t board)
{
    uint64_t hash = 0;
    uint64_t newhash;
    for (Int32 jj = 7; jj >= 0; --jj) // starting at top
    {
        for (Int32 ii = 0; ii < 8; ++ii)
        {
            Int32 piece = board->board[ii][jj];
            if (piece != 0)
            {
                newhash = pieceHash[piece + 6][ii][jj];
                hash = hash ^ newhash;
            }
        } // end for ii
    } // end for jj

    // also needed is e.p.file and castling rights
    if(castlingRights.whiteKingHasMoved == 1)
      hash = hash^WhiteCantCastle[0];  // can expand to KS / Qs with [0] and [1]

    if(castlingRights.blackKingHasMoved == 1)
      hash = hash^BlackCantCastle[0]; 
    
    for(Int32 ii=0;ii<8;++ii)
    {
        for(Int32 jj=0;jj<8;++jj)
        {
            if(castlingRights.EPsquares.board[ii][jj] == 1)
            {
              hash = hash^enPassantHash[ii][jj];
            }
        }
    }
    

    return (hash);
}

uint64_t toggleWhiteToPlayHash(uint64_t hashval)
{
    uint64_t out;
    out = hashval^whiteToPlayHash;
    return(out);
}

void printHashSearchStats()
{
    printf("\n hash search statistics:");
    printf("\n total searches = %ld; total search hits = %ld, hit rate = %.3f %%",
        exactHashSearches,exactHashHits,100.0*(double)exactHashHits/(double)exactHashSearches);
    printf("\n ordering-searches = %ld; total ordering hits = %ld, hit rate = %.3f %%",
            orderingHashSearches,orderingHashHits,100.0*(double)orderingHashHits/(double)orderingHashSearches);
zeroHashStats();
        }

void zeroHashStats()
{
// for search/hit statistics
 exactHashSearches = 0;
 exactHashHits = 0;
 orderingHashSearches = 0;
 orderingHashHits = 0;
 printf(" - zeroed hash stats");
}