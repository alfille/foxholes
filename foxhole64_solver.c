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
typedef Bits_t RGMove_t; // refer, game, move0, .. move[poison-1] == poison_plus+1 length
typedef Bits_t GMove_t; // game, move0, .. move[poison-1] == poison_plus length

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
#define gamestate_length 0x100000
#define INC(x) ( ((x)+1) % gamestate_length )
#define DEC(x) ( ((x)+gamestate_length-1) % gamestate_length )
#define DIFF(x,y) ( ( gamestate_length+(x)-(y) ) % gamestate_length )

struct tryStruct {
    Bits_t game ;
    Bits_t refer ;
} ;
struct tryStruct gameList[gamestate_length]; // fixed size array holding intermediate game states
struct tryStruct backTraceList[gamestate_length]; // fixed size array holding intermediate game states
void * pGL = NULL ; // allocated poisoned move storage
// pointers to start and end of circular buffer
int gameListStart ;
int gameListNext ;

// Poison version -- uses same space
// Array of moves to be tested (or saved for backtrace -- circular buffer with macros)
// All in offset index (Bits_t size)

// Use same array -- add some buffer to accomodate larger element size
#define RGMoveLength ( ( gamestate_length -MaxPoison )* sizeof(struct tryStruct)/sizeof(Bits_t) )
#define RGMoveINC(x) ( ((x)+poison_plus+1) % RGMoveLength )
#define RGMoveDEC(x) ( ((x)+RGMoveLength-poison_plus-1) % RGMoveLength )
#define RGMoveDIFF(x,y) ( ( ( RGMoveLength+(x)-(y) ) % RGMoveLength ) / (poison_plus + 1 ) )
#define RGM_size   ( (poison_plus+1) * sizeof(Bits_t) )

#define indexRGMove gameListStart
#define nextRGMove  gameListNext

// poison_plus + 1 elements per entry:
// 0 - refer
// 1 - game position
// 2 - most recent move
// 3 - next most recent move
// ...
// total of poison-1 moves (min 0)
RGMove_t * RGMoveCurrent = (RGMove_t *) gameList ;
RGMove_t * RGMoveBack = (RGMove_t *) backTraceList ;



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
        int start; // start in backTraceList
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

void gamesSeenCreate() ;
void jumpHolesCreate( Bits_t * J ) ;

size_t binomial( int N, int M ) ;
int premadeMovesRecurse( Bits_t * Moves, int index, int start_hole, int left, Bits_t pattern ) ;

void highPoisonP( void ) ;
Searchstate_t highPoisonCreateP( void ) ;
Searchstate_t highPoisonDayP( int Day, Bits_t target ) ;

void lowPoison( void ) ;
Searchstate_t lowPoisonCreate( void ) ;
Searchstate_t lowPoisonDay( int Day, Bits_t target ) ;

Searchstate_t calcMove( Bits_t* move, Bits_t thisGame, Bits_t *new_game, Bits_t target ) ;
Searchstate_t calcMoveP( GMove_t* move, Bits_t target ) ;

int compare(const void* numA, const void* numB) ;
int compareP(const void * pA, const void * pB ) ;

void gamesSeenAdd( Bits_t g ) ;
Bool_t gamesSeenFound( Bits_t g ) ;
Bool_t gamesSeenFoundP( GMove_t * g ) ;

void backTraceCreate( void ) ;
void backTraceAdd( int Day ) ;
void backTraceAddP( int Day ) ;
void backTraceRestart( int lastDay, int thisDay ) ;
void backTraceExecute( int generation, Bits_t refer ) ;
void backTraceExecuteP( int generation, Bits_t refer ) ;

void fixupTrace( void ) ;
void fixupMoves( void ) ;
void fixupMovesP( void ) ;
void fixupDayP( int Day ) ;
void fixupGamesP( void ) ;


void showBits( Bits_t bb ) ;
void showDoubleBits( Bits_t bb, Bits_t cc ) ;
void showWin( void ) ;

void loadToVictory( int Day, GMove_t * move ) ;
void loadToVictoryPlus( int Day, GMove_t * move ) ;
void loadFromVictory( int Day, GMove_t * move ) ;



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
    Loc.iPossible = binomial( holes, visits );
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
    gamesSeenCreate(); // bitmap of game layouts (to avoid revisiting)

    // Execute search (different if more than 1 poisoned day)
    if ( poison < 2 && ! xperimental ) {
        // Does full backtrace to show full winning strategy
        lowPoison() ;
    } else {
        if (rigorous) {
            search_elements = poison_plus ;
            search_size = search_elements * sizeof( Bits_t ) ;
        }
        highPoisonP() ;
    }
    
    // print json
    if (json) {
        jsonOut() ;
        if ( jsonfile ) {
            fclose( jfile ) ;
        }
    }
}

void gamesSeenCreate() {
    // Loc.Sorted already set in premadeMovesCreate
    Loc.iSorted = 0 ;
    Loc.Unsorted = Loc.Sorted + Loc.iSorted ;
    Loc.iUnsorted = 0 ;
    Loc.Next = Loc.Unsorted ;
    //Loc.free = STORESIZE - (Loc.Sorted - Loc.Jumps) ;
    //printf("Jump %p, Possible %p, Sort %p, Usort %p\n",Loc.Jumps,Loc.Possible,Loc.Sorted,Loc.Unsorted);
    //printf("Jump %lu, Possible %lu, Sort %lu, Usort %lu\n",(size_t) holes,Loc.iPossible,Loc.iSorted,Loc.iUnsorted);
}

int compare(const void* numA, const void* numB) {
    const Bits_t num1 = *(const Bits_t*)numA;
    const Bits_t num2 = *(const Bits_t*)numB;

    if (num1 > num2) {
        return 1;
    }
    else {
        if (num1 == num2)
            return 0;
        else
            return -1;
    }
}

int compareP(const void * pA, const void * pB) {
    return memcmp( pA, pB, search_size ) ;
}

void gamesSeenAdd( Bits_t g ) {
    Loc.Unsorted[Loc.iUnsorted] = g ;
    ++Loc.iUnsorted ;
    if ( Loc.iUnsorted >= UNSORTSIZE ) {
        Loc.free -= Loc.iUnsorted ;
        if ( Loc.free <= UNSORTSIZE ) {
            fprintf(stderr, "Memory exhausted adding games seen\n");
            exit(1);
        }
        //printf("Sorting\n");
        Loc.iSorted += Loc.iUnsorted ;
        qsort( Loc.Sorted, Loc.iSorted, sizeof( Bits_t), compare );
        Loc.Unsorted = Loc.Sorted + Loc.iSorted ;
        Loc.iUnsorted = 0 ;
    }
}

Bool_t gamesSeenFound( Bits_t g ) {
    if ( bsearch( &g, Loc.Sorted,    Loc.iSorted,   sizeof( Bits_t ), compare ) != NULL ) {
        return True ;
    }
    if (   lfind( &g, Loc.Unsorted, &Loc.iUnsorted, sizeof( Bits_t ), compare ) != NULL ) {
        return True ;
    }
    gamesSeenAdd( g ) ;
    return False ;
}

Bool_t gamesSeenFoundP( GMove_t * g ) {
    if ( bsearch( g, Loc.Sorted,    Loc.iSorted,   search_size, compareP ) != NULL ) {
        return True ;
    }
    size_t ius = Loc.iUnsorted ;
    lsearch( g, Loc.Unsorted, &Loc.iUnsorted, search_size, compareP ) ;
    if ( ius == Loc.iUnsorted ) {
        // found
        return True ;
    }
    // added to unsorted. See if need to move unsorted to sorted and check for enough space for next increment
    Loc.Next += search_elements ;
    if ( Loc.iUnsorted >= UNSORTSIZE ) { // move unsorted -> sorted
        Loc.free -= Loc.iUnsorted * search_elements ;
        if ( Loc.free <= UNSORTSIZE ) { // test if enough space
            fprintf(stderr, "Memory exhausted adding games seen\n");
            exit(1);
        }
        //printf("Sorting\n");
        Loc.iSorted += Loc.iUnsorted ;
        qsort( Loc.Sorted, Loc.iSorted, search_size, compareP );
        Loc.Unsorted = Loc.Next ;
        Loc.iUnsorted = 0 ;
    }
    return False ;
}

Searchstate_t calcMove( Bits_t* move, Bits_t thisGame, GMove_t *new_game, Bits_t target ) {
    if ( update ) {
        ++searchCount ;
        if ( (searchCount & 0xFFFFFF) == 0 ) {
            printf(".\n");
        }
    }

    // clear moves
    thisGame &= ~move[0] ;

    // calculate where they jump to
    *new_game = Game_none ;
    for ( int j=0 ; j<holes ; ++j ) {
        if ( getB( thisGame, j ) ) { // fox currently
             *new_game|= Loc.Jumps[j] ; // jumps to here
        }
    }

    // do poisoning
    for ( int p=0 ; p<poison ; ++p ) {
        *new_game &= ~move[p] ;
    }

    // Victory? (No foxes)
    if ( *new_game==target ) {
        return won ;
    }

    // Already seen?
    if ( gamesSeenFound( *new_game ) == True ) {
        // game configuration already seen
        return retry ; // means try another move
    }
    
    // Valid new game, continue further
    return forward ;
}

Searchstate_t calcMoveP( GMove_t * gmove, Bits_t target ) {
    // comming in, move has game,newmove,move0,..move[p-2]
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
    if ( gamesSeenFoundP( gmove ) == True ) {
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
        //printf("backtrace generation=%d day=%d refer=%d\n",generation,backTraceState.entries[generation].day,refer) ;
        victoryGame[backTraceState.entries[generation].day] = backTraceList[refer].game ;
        //showBits( victoryGame[0] ) ;
        refer = backTraceList[refer].refer ;
        if ( generation == backTraceState.startEntry ) {
            break ;
        }
        generation = backDEC(generation) ;
    }
}

void backTraceExecuteP( int generation, Bits_t refer ) {
    // generation is index in backTraceState entries list (in turn indexes circular buffer)
    while ( refer != Game_all ) {
        int day = backTraceState.entries[generation].day ;
        loadToVictory( day, RGMoveBack + refer + 1 ) ;
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
    // poison <= 1

    // back room for new data
    int insertLength = DIFF(gameListNext,gameListStart) ;
    if ( backTraceState.startEntry != backTraceState.nextEntry ) {
        while ( DIFF(backTraceState.entries[backTraceState.startEntry].start,backTraceState.entries[backTraceState.nextEntry].start) < insertLength ) {
            backTraceState.startEntry = backINC(backTraceState.startEntry) ;
            ++backTraceState.incrementDay ;
        }
    }

    // move current state to "Back" array"
    backTraceState.entries[backTraceState.nextEntry].day = Day ;
    int index = backTraceState.entries[backTraceState.nextEntry].start ;
    for ( int inew=gameListStart ; inew != gameListNext ; inew = INC(inew), index=INC(index) ) {
        backTraceList[index].game  = gameList[inew].game ;
        backTraceList[index].refer = gameList[inew].refer ;
        gameList[inew].refer = index ;
    }
    backTraceState.nextEntry = backINC(backTraceState.nextEntry) ;
    backTraceState.entries[backTraceState.nextEntry].start = index ;
    backTraceState.addDay += backTraceState.incrementDay ;
}

void backTraceAddP( int Day ) {
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
    for ( int igame=indexRGMove ; igame != nextRGMove ; igame = RGMoveINC(igame), iback=RGMoveINC(iback) ) {
        memmove( RGMoveBack + iback, RGMoveCurrent + igame, RGM_size ) ;
        RGMoveCurrent[igame] = iback ; // update refer
    }
    backTraceState.nextEntry = backINC(backTraceState.nextEntry) ;
    backTraceState.entries[backTraceState.nextEntry].start = iback ;
    backTraceState.addDay += backTraceState.incrementDay ;
}

void backTraceRestart( int lastDay, int thisDay ) {
    // Poison <= 1
    // Set up arrays of game states
    //  will evaluate all states for each day for all moves and add to next day if unique

    Bits_t current = victoryGame[lastDay] ;
    Bits_t target = victoryGame[thisDay] ;

    gameListNext = 0 ;
    gameListStart = gameListNext ;
    
    // set solver state
    backTraceCreate() ;
    backTraceState.addDay = lastDay+1 ;

    // set Initial position
    gameList[gameListNext].game = current ;
    gameList[gameListNext].refer = Game_all ;
    gamesSeenFound( gameList[gameListNext].game ) ; // flag this configuration
    ++gameListNext ;
    
    // Now loop through days
    for ( int Day=lastDay+1 ; Day<=thisDay ; ++Day ) {
        switch ( lowPoisonDay( Day, target ) ) {
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

void backTraceRestartP( int lastDay, int thisDay ) {
    // Poison > 1
    // Set up arrays of game states
    //  will evaluate all states for each day for all moves and add to next day if unique

    RGMove_t move[ poison_plus + 1 ] ;
    Bits_t target = victoryGame[thisDay] ;

    nextRGMove = 0 ;
    indexRGMove = nextRGMove ;
    
    // set solver state
    backTraceCreate() ;
    backTraceState.addDay = lastDay+1 ;

    // set Initial position
    loadFromVictory( lastDay, move+1 ) ;
    move[0] = Game_all ; // refer
    printf( "Days %d -> %d", lastDay, thisDay ) ;
    for (int i = 0 ; i <= poison_plus ; ++i ) { printf("%d=>%lu  ",i,move[i]) ; }
    printf("\n");
    // set Initial position
    memmove( RGMoveCurrent + nextRGMove, move, RGM_size ) ;
    gamesSeenFoundP( move+1 ) ; // flag this configuration
    nextRGMove = RGMoveINC( nextRGMove ) ;
    
    // Now loop through days
    for ( int Day=lastDay+1 ; Day<=thisDay ; ++Day ) {
        switch ( highPoisonDayP( Day, target ) ) {
            case won:
            case lost:
                return ;
            default:
                break ;
        }
        // Save intermediate for backtracing winning strategy
        if ( Day == backTraceState.addDay ) {
            backTraceAddP(Day) ;
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
                    gamesSeenCreate() ; // clear bits
                    unsolved = True ;
                }
            } else {
                // known solution
                if ( gap ) {
                    // solution after unknown gap
                    //printf("Go back to day %d for intermediate position\n",d);
                    //showWin();
                    if ( poison > 1 ) {
                        backTraceRestartP( lastDay, d );
                    } else {
                        backTraceRestart( lastDay, d );
                    }
                } ;

                gap = False ;
                lastDay = d ;
            }
        }
//    } while ( unsolved ) ;
    } while ( False ) ;
}

void fixupMoves( void ) {
    GMove_t move[1] ; // at most one poison day
    
    for ( int Day=1 ; Day < victoryDay ; ++Day ) {
        // solve unknown moves
        if ( victoryMove[Day] == Game_none ) {
            if ( update ) {
                printf("Fixup Moves Day %d\n",Day);
            }
            for ( size_t ip=0 ; ip<Loc.iPossible ; ++ip ) { // each possible move
                Bits_t newGame = Game_none ;
                Bits_t thisGame = victoryGame[Day-1] ;
                move[0] = Loc.Possible[ip] ; // actual move (and poisoning)

                // clear moves
                thisGame &= ~move[0] ;

                // calculate where they jump to
                for ( int j=0 ; j<holes ; ++j ) {
                    if ( getB( thisGame, j ) ) { // fox currently
                         newGame |= Loc.Jumps[j] ; // jumps to here
                    }
                }

                // do poisoning
                if ( poison ) {
                    newGame &= ~move[0] ;
                }

                // Match target?
                if ( newGame == victoryGame[Day] ) {
                    victoryMove[Day] = move[0] ;
                    break ;
                }
            }
        }
    }
}

void fixupDayP( int Day ) {
    GMove_t move[poison_plus+1] ;
    loadFromVictory( Day-1, move+1 ) ;
    for ( size_t ip=0 ; ip<Loc.iPossible ; ++ip ) { // each possible move
        move[0] = victoryGame[ Day-1 ] ;
        move[1] = Loc.Possible[ip] ; // actual move (and poisoning)

        switch ( calcMoveP( move, victoryGame[Day] ) ) {
            case won:
                loadToVictoryPlus( Day, move ) ;
                return ;
            default:
                break ;
        }
    }
    printf("Uh oh -- cannot find move for day = %d\n",Day) ;
}
            

void fixupMovesP( void ) {
    for ( int Day=1 ; Day < victoryDay ; ++Day ) {
        // solve unknown moves
        if ( victoryMove[Day] == Game_none ) {
            if ( update ) {
                //printf("Fixup Moves Day %d\n",Day);
                //showWin();
            }
            fixupDayP( Day ) ;
        }
    }
}

void fixupGamesP( void ) {
    // Use moves to fill in games
    GMove_t gmove[ poison_plus+1 ] ; // initial state
    loadFromVictory( 0, gmove ) ;
    for ( int Day = 1 ; Day < victoryDay ; ++Day ) {
        memmove( gmove+1, gmove, poison_plus ) ; // move state to prior
        gmove[1] = victoryMove[Day] ; // add newest move
        calcMoveP( gmove, Game_none ) ; // calculate game
        loadToVictory( Day, gmove ) ;
    }
}

void showWin( void ) {
    for ( int d = 0 ; d <= victoryDay ; ++d ) {
        printf("Day%3d Move ## Game \n",d);
        showDoubleBits( victoryMove[d],victoryGame[d] ) ;
    }
}    

void lowPoison( void ) {
    // only for poison <= 1    
    switch (lowPoisonCreate()) { // start searching through days until a solution is found (will be fastest by definition)
    case won:
        fixupTrace() ; // fill in game path
        fixupMoves() ; // fill in moves
        printf("\n");
        printf("Winning Strategy:\n");
        showWin() ;
        break ;
    default:
        break ;    
    }
}

Searchstate_t lowPoisonCreate( void ) {
    // Solve for Poison <= 1
    // Set up arrays of game states
    //  will evaluate all states for each day for all moves and add to next day if unique

    gameListNext = 0 ;
    gameListStart = gameListNext ;
    
    // Set winning path to unknown
    for ( int d=0 ; d<MaxDays ; ++d ) {
        victoryGame[d] = Game_all ;
        victoryMove[d] = Game_none ;
    }
    
    // set back-solver state
    backTraceCreate() ;
        
    // set Initial position
    gameList[gameListNext].game = Game_all ;
    gameList[gameListNext].refer = Game_all ;
    gamesSeenFound( gameList[gameListNext].game ) ; // flag this configuration
    ++gameListNext ;
    
    // Now loop through days
    for ( int Day=1 ; Day < MaxDays ; ++Day ) {
        printf("Day %d, States: %d, Moves %lu, Total %lu\n",Day+1,DIFF(gameListNext,gameListStart),Loc.iPossible,Loc.iSorted+Loc.iUnsorted);
        switch ( lowPoisonDay(Day,Game_none) ) {
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

Searchstate_t lowPoisonDay( int Day, Bits_t target ) {
    // poison <= 1
    
    GMove_t move[1] ; // for poisoning

    int iold = gameListStart ; // need to define before loop
    gameListStart = gameListNext ;
    //printf("Monitor day %d\n",Day);
    //showBits( victoryGame[0] ) ;

    for (  ; iold!=gameListStart ; iold=INC(iold) ) { // each possible position
        for ( size_t ip=0 ; ip<Loc.iPossible ; ++ip ) { // each possible move
            Bits_t newT ;
            move[0] = Loc.Possible[ip] ; // actual move (and poisoning)
                    
            switch( calcMove( move, gameList[iold].game, &newT, target ) ) {
                case won:
                    victoryGame[Day] = target ;
                    victoryMove[Day] = move[0] ;
                    victoryGame[Day-1] = gameList[iold].game ;
                    if ( target == Game_none ) {
                        // real end of game
                        victoryDay = Day ;
                    }
                    backTraceExecute( backDEC(backTraceState.nextEntry), gameList[iold].refer ) ;
                    return won;
                case forward:
                    // Add this game state to the furture evaluation list
                    gameList[gameListNext].game = newT ;
                    gameList[gameListNext].refer = gameList[iold].refer ;
                    gameListNext = INC(gameListNext) ;
                    if ( gameListNext == iold ) { // head touches tail
                        printf("Too large a solution space\n");
                        return lost ;
                    }
                    break ;
                default:
                    break ;
            }
        }
    }
    return gameListNext != gameListStart ? forward : lost ;
}


void highPoisonP( void ) {
    switch (highPoisonCreateP()) { // start searching through days until a solution is found (will be fastest by definition)
    case won:
        fixupTrace() ; // fill in game path
        fixupMovesP() ; // fill in missing moves
        //fixupGamesP() ; // fill in missing games
        printf("\n");
        printf("Winning Strategy:\n");
        showWin() ;
        break ;
    default:
        break ;    
    }
}

Searchstate_t highPoisonCreateP( void ) {
    // Solve Poison >1
    // Set up arrays of game states
    //  will evaluate all states for each day for all moves and add to next day if unique

    nextRGMove = 0 ;
    indexRGMove = nextRGMove ;

    // set back-solver state
    backTraceCreate() ;

    // initially no poison history of course
    RGMove_t rgmove[poison_plus+1] ;
    rgmove[0] = Game_all ; // refer
    loadFromVictory( 0, rgmove+1 ) ;

    // set Initial position
    memmove( RGMoveCurrent + nextRGMove, rgmove, RGM_size ) ;
    nextRGMove = RGMoveINC( nextRGMove ) ;
    gamesSeenFoundP( rgmove+1 ) ; // flag this configuration
    
    // Now loop through days
    for ( int Day=1 ; Day < MaxDays ; ++Day ) {
        printf("Day %d, States: %lu, Moves %lu, Total %lu\n",Day,RGMoveDIFF(nextRGMove,indexRGMove),Loc.iPossible,Loc.iSorted+Loc.iUnsorted);
        switch ( highPoisonDayP( Day, Game_none ) ) {
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
            backTraceAddP(Day) ;
        }
    }
    printf( "Exceeded %d days.\n",MaxDays ) ;
    return lost ;
}

Searchstate_t highPoisonDayP( int Day, Bits_t target ) {
    RGMove_t rgmove[poison_plus+1] ; // for poisoning

    int iold = indexRGMove ; // need to define before loop
    indexRGMove = nextRGMove ;
        
    for (  ; iold!=indexRGMove ; iold=RGMoveINC(iold) ) { // each possible position
        memmove( rgmove, RGMoveCurrent+iold, RGM_size ) ; // leave space at start
        Bits_t thisGame = rgmove[1] ;
        
        if ( victoryMove[Day] == Game_none ) { // no move specified
            // Search though all moves
            for ( size_t ip=0 ; ip<Loc.iPossible ; ++ip ) { // each possible move
                rgmove[0] = thisGame ; // overwrites refer, essentially adds to start of movelist 
                rgmove[1]= Loc.Possible[ip] ; // actual move (and poisoning)
                switch( calcMoveP( rgmove, target ) ) {
                    case won:
                        victoryGame[Day-1] = thisGame ;
                        loadToVictoryPlus( Day, rgmove ) ;
                        if ( target == Game_none ) {
                            // real end of game
                            victoryDay = Day ;
                        }
                        printf("Victory Day %d\n",Day);
                        //showWin() ;
                        // Go through backtraces and fill in solutions found
                        backTraceExecuteP( backDEC(backTraceState.nextEntry), RGMoveCurrent[iold] ) ;
                        return won;
                    case forward:
                        // Add this game state to the furture evaluation list
                        RGMoveCurrent[nextRGMove] = RGMoveCurrent[iold] ; // refer
                        memmove( RGMoveCurrent + nextRGMove +1 , rgmove, poison_size ) ; // rest of gmove
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
            rgmove[0] = thisGame ; // again shifting entries to make soon for newer move
            rgmove[1]= victoryMove[Day] ; // actual move (and poisoning)
            switch( calcMoveP( rgmove, target ) ) {
                case won:
                    victoryGame[Day-1] = thisGame ;
                    loadToVictoryPlus( Day, rgmove ) ;
                    if ( target == Game_none ) {
                        // real end of game
                        victoryDay = Day ;
                    }
                    // Go through backtraces and fill in solutions found
                    backTraceExecuteP( backDEC(backTraceState.nextEntry), RGMoveCurrent[iold] ) ;
                    return won;
                case forward:
                    // Add this game state to the future evaluation list
                    RGMoveCurrent[nextRGMove] = RGMoveCurrent[iold] ; // refer
                    memmove( RGMoveCurrent + nextRGMove + 1, rgmove, poison_size ) ;
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
