#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <malloc.h>
#include <unistd.h>
#include <string.h>
#include <search.h>

/*
 * Foxhole solver for <= 64 holes
 * (Use foxhole_solver for <- 32 holes -- more efficient)
 * uses bitmaps
 *
 * c 2022 Paul H Alfille
 * MIT license
 *
 * Find best solution
 * 
 * */

typedef uint64_t Bits_t; // bitmap for games state, moves
typedef Bits_t RGM_t; // refer, game, move0, .. move[poison-1] == poison_plus+1 length
typedef Bits_t GM_t; // game, move0, .. move[poison-1] == poison_plus length
typedef Bits_t GMM_t; // game, move0, .. move[poison] == poison_plus+1 length

#include "foxholes.h"

#define STORESIZE 100000000
Bits_t Store[STORESIZE] ;
#define UNSORTSIZE 50

struct {
    Bits_t * Jumps ;
    Bits_t * Possible ;
    Bits_t * Sorted ;
    Bits_t * Unsorted ;
    Bits_t * Next ;
    size_t iPossible ;
    size_t iSorted ;
    size_t iUnsorted ;
    size_t free ;
} Loc = {
    .Jumps = Store,
    .free = STORESIZE ,
} ;

// Limits
#define MaxHoles 64
#define MaxDays 300
#define MaxPoison 40

// Bit macros
// 64 bit for games, moves, jumps
#define long1 ((uint64_t) 1)
#define setB( map, b ) map |= (long1 << (b))
#define clearB( map, b ) map &= ~(long1 << (b))
#define getB( map, b ) (((map) & (long1 << (b))) ? 1:0)

// Array of moves to be tested (or saved for backtrace -- circular buffer with macros)
#define gamestate_length 0x1000000

// Array of moves to be tested (or saved for backtrace -- circular buffer with macros)
// All in offset index (Bits_t size)

// Use same array -- add some buffer to accomodate larger element size
#define RGMoveLength ( ( gamestate_length -MaxPoison ) )
#define RGMoveINC(x) ( ((x)+poison_plus+1) % RGMoveLength )
#define RGMoveDEC(x) ( ((x)+RGMoveLength-poison_plus-1) % RGMoveLength )
#define RGMoveDIFF(x,y) ( ( size_t) ( ( ( RGMoveLength+(x)-(y) ) % RGMoveLength ) / (poison_plus + 1 ) ) )
#define RGM_size   ( (poison_plus+1) * sizeof(Bits_t) )

// pointers to start and end of circular buffer
size_t indexRGMove ;
size_t nextRGMove ;

// poison_plus + 1 elements per entry:
// 0 - refer
// 1 - game position
// 2 - most recent move
// 3 - next most recent move
// ...
// total of poison-1 moves (min 0)
RGM_t RGMoveCurrent[gamestate_length] ;
RGM_t RGMoveBack[gamestate_length] ;



// structure holding backtracing info to recreate winning strategy
// circular buffer of prior game states with references from active buffer and back buffer
// if not large enough, will need to recurse later to fix up missing
#define backLook_length (MaxDays+1)
struct {
    int addDay ; // next day to add to backtrace
    int incrementDay ; // incrementDay for addDay
    int startEntry ; // start index for entries list
    int nextEntry ; // next index for entries list
    struct {
        int start; // start in back trace
        int day; // corresponding day
    } entries[backLook_length] ; // cirular entries buffer
} backTraceState ;

#define backINC(x) ( ((x)+1) % backLook_length )
#define backDEC(x) ( ((x)-1+backLook_length) % backLook_length )

// function prototypes
int main( int argc, char **argv ) ;
void getOpts( int argc, char ** argv ) ;
void help( char * progname ) ;
char * geoName( Geometry_t g ) ;
char * connName( Connection_t c ) ;
void printStatus( char * progname ) ;
Validation_t validate( void ) ;

void makeStoredState() ;
void jumpHolesCreate( Bits_t * J ) ;

size_t binomial( int N, int M ) ;
int premadeMovesRecurse( Bits_t * Moves, int index, int start_hole, int left, Bits_t pattern ) ;

void primarySolve( void ) ;
Searchstate_t primarySolveCreate( void ) ;
Searchstate_t primarySolveDay( int Day, Bits_t target ) ;

Searchstate_t calcMove( GMM_t* move, Bits_t target ) ;

Bool_t findStoredStates( GM_t * g ) ;

void backTraceCreate( void ) ;
void backTraceAdd( int Day ) ;
void backTraceExecute( int generation, Bits_t refer ) ;
void backTraceRestart( int lastDay, int thisDay ) ;

void fixupTrace( void ) ;
void fixupMoves( void ) ;
void fixupDay( int Day ) ;
void fixupGames( void ) ;


void showBits( Bits_t bb ) ;
void showDoubleBits( Bits_t bb, Bits_t cc ) ;
void showWin( void ) ;

void loadToVictory( int Day, GM_t * move ) ;
void loadToVictoryPlus( int Day, GMM_t * move ) ;
void loadFromVictory( int Day, GM_t * move ) ;
void loadFromVictoryPlus( int Day, GM_t * move ) ;

void jsonOut( void ) ;

#include "victory.c"
#include "getOpts.c"
#include "showBits.c"
#include "help.c"
#include "validate.c"
#include "jumpHolesCreate.c"
#include "jsonOut.c"
#include "status.c"
#include "moves.c"
#include "compare.c"
#include "storedState.c"

int main( int argc, char **argv )
{
    // Parse Arguments
    getOpts(argc,argv) ;
    
    // Print final arguments
    printStatus( argv[0]);    
    
    setupVictory() ;

    if ( update ) {
        printf("Setting up moves\n");
    }
    jumpHolesCreate(Loc.Jumps); // array of fox jump locations
    Loc.Possible = Loc.Jumps + holes ;
    Loc.free -= holes ;

    if ( update ) {
        printf("Starting search\n");
    }
    Loc.iPossible = binomial( holes, visits ) + 1 ; // 0-index is no move
    if ( Loc.iPossible > Loc.free ) {
        // check memory although unlikely exhausted at this stage
        fprintf( stderr, "Memory exhaused from possible move list\n");
        exit(1);
    }
    premadeMovesRecurse( Loc.Possible, 0, 0, visits, Game_none ); // array of moves
    Loc.Sorted = Loc.Possible + Loc.iPossible ;
    Loc.free -= Loc.iPossible ;
    
    if ( update ) {
        printf("Setting up game array\n");
    }
    makeStoredState(); // bitmap of game layouts (to avoid revisiting)

    if (rigorous) {
        search_elements = poison_plus ;
        search_size = search_elements * sizeof( Bits_t ) ;
    }

    switch ( search_elements ) {
        case 1:
            compare = compare1 ;
            break ;
        case 2:
            compare = compare2 ;
            break ;
        case 3:
            compare = compare3 ;
            break ;
        case 4:
            compare = compare4 ;
            break ;
        case 5:
            compare = compare4 ;
            break ;
        case 6:
            compare = compare4 ;
            break ;
        default:
            compare = compareP ;
            break ;
    }

    // Execute search
    primarySolve() ;
    
    // print json
    if (json) {
        jsonOut() ;
        if ( jsonfile ) {
            fclose( jfile ) ;
        }
    }
}

Searchstate_t calcMove( GMM_t * gmove, Bits_t target ) {
    // coming in, move has game,newmove,move0,..move[p-2]
    // returning move has newgame,newmove,move0,...,move[n-p]
    // target points to:
    //   NULL Game_none
    //   non--NULL, move array (poison_plus length)
    if ( update ) {
        ++searchCount ;
        if ( (searchCount & 0xFFFFFF) == 0 ) {
            printf(".\n");
        }
    }

    // clear moves
    Bits_t thisGame = gmove[0] ;
    thisGame &= ~gmove[1] ;
    //showBits( thisGame ) ;
    //for (int m=1 ; m<=poison_plus ; ++m ) printf("%lu ",gmove[m]) ;
    //printf("\n");

    // calculate where they jump to
    gmove[0] = Game_none ;
    for ( int j=0 ; j<holes ; ++j ) {
        if ( getB( thisGame, j ) ) { // fox currently
             gmove[0] |= Loc.Jumps[j] ; // jumps to here
        }
    }

    // do poisoning
    for ( int p=1 ; p<=poison ; ++p ) {
        gmove[0] &= ~gmove[p] ;
    }
    
    // Victory? (No foxes or other goal in fixup backtracking mode)
    if ( gmove[0] == target ) {
            return won ;
    }

    // Already seen?
    if ( findStoredStates( gmove ) == True ) {
        // game configuration already seen
        return retry ; // means try another move
    }
    
    // Valid new game, continue further
    return forward ;
}

void backTraceCreate( void ) {
    backTraceState.startEntry = 0 ;
    backTraceState.nextEntry = 0 ;
    backTraceState.entries[0].start = 0 ;
    backTraceState.addDay = 1 ;
    backTraceState.incrementDay = poison_plus ;
}

void backTraceExecute( int generation, Bits_t refer ) {
    // generation is index in backTraceState entries list (in turn indexes circular buffer)
    while ( refer != Game_all ) {
        int day = backTraceState.entries[generation].day ;
        loadToVictory( day, RGMoveBack + refer + 1 ) ; // backtrace stores poison-1 moves
        refer = RGMoveBack[refer] ;
        //printf("Backtracing, day %d.\n",day);
        //showWin();
        if ( generation == backTraceState.startEntry ) {
            break ;
        }
        generation = backDEC(generation) ;
    }
}

void backTraceAdd( int Day ) {
    // poison > 1

    // back room for new data
    size_t rgm_insert = RGMoveDIFF(nextRGMove,indexRGMove) ; // length of refer/game/move data to add
    //printf("Backtrace add day %d\n",Day);
    if ( backTraceState.startEntry != backTraceState.nextEntry ) {
        while ( RGMoveDIFF(
                    backTraceState.entries[backTraceState.startEntry].start,
                    backTraceState.entries[backTraceState.nextEntry].start
                    ) < rgm_insert ) {
            //printf("Backtrace add day %d makeroom\n",Day);
            // loop through making space by removing old backtrace data
            backTraceState.startEntry = backINC(backTraceState.startEntry) ;
            ++backTraceState.incrementDay ;
        }
    }

    // move current state to "Back" array"
    backTraceState.entries[backTraceState.nextEntry].day = Day ;
    int iback = backTraceState.entries[backTraceState.nextEntry].start ;
    for ( size_t igame=indexRGMove ; igame != nextRGMove ; igame = RGMoveINC(igame), iback=RGMoveINC(iback) ) {
        memmove( RGMoveBack + iback, RGMoveCurrent + igame, RGM_size ) ;
        RGMoveCurrent[igame] = iback ; // update refer
    }
    backTraceState.nextEntry = backINC(backTraceState.nextEntry) ;
    backTraceState.entries[backTraceState.nextEntry].start = iback ;
    backTraceState.addDay += backTraceState.incrementDay ;
}

void backTraceRestart( int lastDay, int thisDay ) {
    // Poison > 1
    // Set up arrays of game states
    //  will evaluate all states for each day for all moves and add to next day if unique

    RGM_t move[ poison_plus + 1 ] ;
    Bits_t target = victoryGame[thisDay] ;

    nextRGMove = 0 ;
    indexRGMove = nextRGMove ;
    
    // set solver state
    backTraceCreate() ;
    backTraceState.addDay = lastDay+1 ;

    // set Initial position
    loadFromVictory( lastDay, move+1 ) ;
    move[0] = Game_all ; // refer
    //printf( "Days %d -> %d", lastDay, thisDay ) ;
    //for (int i = 0 ; i <= poison_plus ; ++i ) { printf("%d=>%lu  ",i,move[i]) ; }
    printf("\n");
    // set Initial position
    memmove( RGMoveCurrent + nextRGMove, move, RGM_size ) ;
    findStoredStates( move+1 ) ; // flag this configuration
    nextRGMove = RGMoveINC( nextRGMove ) ;
    
    // Now loop through days
    for ( int Day=lastDay+1 ; Day<=thisDay ; ++Day ) {
        switch ( primarySolveDay( Day, target ) ) {
            case won:
            case lost:
                return ;
            default:
                break ;
        }
        // Save intermediate for backtracing winning strategy
        if ( Day == backTraceState.addDay ) {
            backTraceAdd(Day) ;
        }
    }
    
}

void fixupTrace( void ) {
    // any unsolved?
    int unsolved ;
    do {
        unsolved = False ;
        int lastDay = 0 ;
        int gap = False ;

        for ( int d=1 ; d<victoryDay ; ++d ) { // go through days
            if ( victoryGame[d] == Game_all ) {
                // unknown solution
                gap = True ;
                if ( ! unsolved ) {
                    // do only once if needed
                    makeStoredState() ; // clear bits
                    unsolved = True ;
                }
            } else {
                // known solution
                if ( gap ) {
                    // solution after unknown gap
                    //printf("Go back to day %d for intermediate position\n",d);
                    //showWin();
                    backTraceRestart( lastDay, d );
                } ;

                gap = False ;
                lastDay = d ;
            }
        }
//    } while ( unsolved ) ;
    } while ( False ) ;
}

void fixupDay( int Day ) {
    // find the move for this day
    GMM_t move[poison_plus+1] ;
    loadFromVictory( Day-1, move+1 ) ; // full poison days
    for ( size_t ip=1 ; ip<Loc.iPossible ; ++ip ) { // each possible move -- ignore 0 index
        move[0] = victoryGame[ Day-1 ] ; // prior game position
        move[1] = Loc.Possible[ip] ; // test move

        switch ( calcMove( move, victoryGame[Day] ) ) {
            case won:
                loadToVictoryPlus( Day, move ) ;
                return ;
            default:
                break ;
        }
    }
    printf("Uh oh -- cannot find move for day = %d\n",Day) ;
}
            

void fixupMoves( void ) {
    for ( int Day=1 ; Day < victoryDay ; ++Day ) {
        // solve unknown moves
        if ( victoryMove[Day] == Game_none ) {
            if ( update ) {
                //printf("Fixup Moves Day %d\n",Day);
                //showWin();
            }
            fixupDay( Day ) ;
        }
    }
}

void fixupGames( void ) {
    // Use moves to fill in games
    GMM_t gmove[ poison_plus+1 ] ; // initial state
    for ( int Day = 1 ; Day <= victoryDay ; ++Day ) {
        loadFromVictoryPlus( Day, gmove ) ;
        gmove[0] = victoryGame[Day-1] ;
        calcMove( gmove, Game_none ) ; // calculate game
        victoryGame[Day] = gmove[0] ;
    }
}

void showWin( void ) {
    for ( int d = 0 ; d <= victoryDay ; ++d ) {
        printf("Day%3d Move ## Game \n",d);
        showDoubleBits( victoryMove[d],victoryGame[d] ) ;
    }
}    

void primarySolve( void ) {
    switch (primarySolveCreate()) { // start searching through days until a solution is found (will be fastest by definition)
    case won:
        fixupTrace() ; // fill in game path
        fixupMoves() ; // fill in missing moves
        fixupGames() ; // fill in missing games
        printf("\n");
        printf("Winning Strategy:\n");
        showWin() ;
        break ;
    default:
        break ;    
    }
}

Searchstate_t primarySolveCreate( void ) {
    // Solve Poison >1
    // Set up arrays of game states
    //  will evaluate all states for each day for all moves and add to next day if unique

    nextRGMove = 0 ; // start of the circular move list
    indexRGMove = nextRGMove ;

    // set back-solver state
    backTraceCreate() ;

    // initially no poison history of course
    RGM_t rgm2gmm[poison_plus+1] ;
    rgm2gmm[0] = Game_all ; // refer -- marker for start of the chain
    loadFromVictory( 0, rgm2gmm+1 ) ; // aready set up for defaults

    // set Initial position -- sinmple movelist (only one element, of course, the initial position)
    memmove( RGMoveCurrent + nextRGMove, rgm2gmm, RGM_size ) ;
    nextRGMove = RGMoveINC( nextRGMove ) ;
    findStoredStates( rgm2gmm+1 ) ; // flag this configuration
    
    // Now loop through days
    for ( int Day=1 ; Day < MaxDays ; ++Day ) {
        printf("Day %d, States: %lu, Moves %lu, Total %lu\n",Day,RGMoveDIFF(nextRGMove,indexRGMove),Loc.iPossible,Loc.iSorted+Loc.iUnsorted);
        switch ( primarySolveDay( Day, Game_none ) ) {
            case won:
                printf("Victory in %d days!\n",Day ) ; // 0 index
                return won ;
            case lost:
                printf("No solution despite %d days\n",Day );
                return lost ;
            default:
                break ;
        }
        // Save intermediate for backtracing winning strategy
        if ( Day == backTraceState.addDay ) {
            backTraceAdd(Day) ;
        }
    }
    printf( "Exceeded %d days.\n",MaxDays ) ;
    return lost ;
}

Searchstate_t primarySolveDay( int Day, Bits_t target ) {
    RGM_t rgm2gmm[poison_plus+1] ; // for poisoning

    size_t iold = indexRGMove ; // need to define before loop
    indexRGMove = nextRGMove ;
        
    for (  ; iold!=indexRGMove ; iold=RGMoveINC(iold) ) { // each possible position
        memmove( rgm2gmm, RGMoveCurrent+iold, RGM_size ) ; // leave space at start
        Bits_t thisGame = rgm2gmm[1] ;
        
        if ( victoryMove[Day] == Game_none ) { // no move specified
            // Search though all moves
            for ( size_t ip=1 ; ip<Loc.iPossible ; ++ip ) { // each possible move -- ignore 0 index
                rgm2gmm[0] = thisGame ; // overwrites refer, essentially adds to start of movelist 
                rgm2gmm[1]= Loc.Possible[ip] ; // actual move (and poisoning)
                switch( calcMove( rgm2gmm, target ) ) {
                    case won:
                        victoryGame[Day-1] = thisGame ;
                        loadToVictoryPlus( Day, rgm2gmm ) ; // have the full poison days
                        if ( target == Game_none ) {
                            // real end of game
                            victoryDay = Day ;
                        }
                        printf("Victory Day %d\n",Day);
                        // Go through backtraces and fill in solutions found
                        backTraceExecute( backDEC(backTraceState.nextEntry), RGMoveCurrent[iold] ) ;
                        return won;
                    case forward:
                        // Add this game state to the furture evaluation list
                        RGMoveCurrent[nextRGMove] = RGMoveCurrent[iold] ; // refer
                        memmove( RGMoveCurrent + nextRGMove +1 , rgm2gmm, poison_size ) ; // rest of gmove
                        nextRGMove = RGMoveINC( nextRGMove ) ;
                        if ( nextRGMove == iold ) { // head touches tail
                            printf("Too large a solution space\n");
                            return lost ;
                        }
                        break ;
                    default:
                        // next possible move
                        break ;
                }
            }
        } else {
            // known move
            rgm2gmm[0] = thisGame ; // again shifting entries to make soon for newer move
            rgm2gmm[1]= victoryMove[Day] ; // actual move (and poisoning)
            switch( calcMove( rgm2gmm, target ) ) {
                case won:
                    victoryGame[Day-1] = thisGame ;
                    loadToVictoryPlus( Day, rgm2gmm ) ; // full victory days
                    if ( target == Game_none ) {
                        // real end of game
                        victoryDay = Day ;
                    }
                    // Go through backtraces and fill in solutions found
                    backTraceExecute( backDEC(backTraceState.nextEntry), RGMoveCurrent[iold] ) ;
                    return won;
                case forward:
                    // Add this game state to the future evaluation list
                    RGMoveCurrent[nextRGMove] = RGMoveCurrent[iold] ; // refer
                    memmove( RGMoveCurrent + nextRGMove + 1, rgm2gmm, poison_size ) ;
                    nextRGMove = RGMoveINC( nextRGMove ) ;
                    if ( nextRGMove == iold ) { // head touches tail
                        printf("Too large a solution space\n");
                        return lost ;
                    }
                    break ;
                default:
                    // next possible move
                    break ;
            }
        }
    }
    return nextRGMove != indexRGMove ? forward : lost ;
}
