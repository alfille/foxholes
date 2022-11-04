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

typedef uint64_t Bits_t;

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
#define MaxPoison 4

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
#define gamestate_lengthP = ( sizeof(gameList)/sizeof(Bits_t) )
#define INCP(x) ( ((x)+poison_plus+1) % gamestate_lengthP )
#define DECP(x) ( ((x)+gamestate_lengthP-poison_plus-1) % gamestate_lengthP )
#define DIFFP(x,y) ( ( ( gamestate_lengthP+(x)-(y) ) % gamestate_lengthP ) / (poison_plus + 1 ) )

#define ReferP(x) = (x)
#define GameP(x) = ((x)+1)
#define MoveP(x) = ((x)+2)

// Arrays holding winning moves and game positions
int victoryDay;  // >=0 for success
Bits_t victoryGame[MaxDays+1] ;
Bits_t victoryMove[MaxDays+1] ;

// structure holding backtracing info to recreate winning strategy
// circular buffer of prior game states with references from active buffer and back buffer
// if not large enough, will need to recurse later to fix up missing
#define backLook_length (MaxDays+1)
struct {
    int addDay ; // next day to add to backtrace
    int increment ; // increment for addDay
    int start ; // start index for entries list
    int next ; // next index for entries list
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

void highPoison( void ) ;
Searchstate_t highPoisonCreate( void ) ;
Searchstate_t highPoisonDay( int Day, Bits_t target ) ;

void lowPoison( void ) ;
Searchstate_t lowPoisonCreate( void ) ;
Searchstate_t lowPoisonDay( int Day, Bits_t target ) ;

Searchstate_t calcMove( Bits_t* move, Bits_t thisGame, Bits_t *new_game, Bits_t target ) ;
Searchstate_t calcMoveP( Bits_t* move, Bits_t * target ) ;

int compare(const void* numA, const void* numB) ;
int compareP(const void * pA, const void * pB ) ;

void gamesSeenAdd( Bits_t g ) ;
void gamesSeenAddP( Bits_t * g ) ;
Bool_t gamesSeenFound( Bits_t g ) ;
Bool_t gamesSeenFoundP( Bits_t * g ) ;

void backTraceCreate( void ) ;
void backTraceAdd( int Day ) ;
void backTraceRestart( int lastDay, int thisDay ) ;
void backTraceExecute( int generation, Bits_t refer ) ;

void fixupTrace( void ) ;
void fixupMoves( void ) ;

void showBits( Bits_t bb ) ;
void showDoubleBits( Bits_t bb, Bits_t cc ) ;

void jsonOut( void ) ;


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
    
    // Set all bits and set up pre-computed arrays
    Game_all = 0 ;
    for ( int h=0 ; h<holes ; ++h ) {
        setB( Game_all, h ) ;
    }
    victoryDay = -1;

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
    if ( poison < 2 ) {
        // Does full backtrace to show full winning strategy
        lowPoison() ;
    } else {
        // Just shows shortest time
        highPoison() ;
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
	return memcmp( pA, pB, poison_size ) ;
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

void gamesSeenAddP( Bits_t * g ) {
    memmove( Loc.Next, g, poison_size ) ;
    ++Loc.iUnsorted ;
    Loc.Next += poison_plus ;
    if ( Loc.iUnsorted >= UNSORTSIZE ) {
        Loc.free -= Loc.iUnsorted * poison_plus ;
        if ( Loc.free <= UNSORTSIZE ) {
            fprintf(stderr, "Memory exhausted adding games seen\n");
            exit(1);
        }
        //printf("Sorting\n");
        Loc.iSorted += Loc.iUnsorted ;
        qsort( Loc.Sorted, Loc.iSorted, sizeof( Bits_t), compare );
        Loc.Unsorted = Loc.Next ;
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

Bool_t gamesSeenFoundP( Bits_t * g ) {
    if ( bsearch( g, Loc.Sorted,    Loc.iSorted,   poison_size, compareP ) != NULL ) {
        return True ;
    }
    if (   lfind( g, Loc.Unsorted, &Loc.iUnsorted, poison_size, compareP ) != NULL ) {
        return True ;
    }
    gamesSeenAddP( g ) ;
    return False ;
}

Searchstate_t calcMove( Bits_t* move, Bits_t thisGame, Bits_t *new_game, Bits_t target ) {
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

Searchstate_t calcMoveP( Bits_t* move, Bits_t * target ) {
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
    Bits_t thisGame = move[0] ;
    thisGame &= ~move[1] ;

    // calculate where they jump to
    move[0] = Game_none ;
    for ( int j=0 ; j<holes ; ++j ) {
        if ( getB( thisGame, j ) ) { // fox currently
             move[0] |= Loc.Jumps[j] ; // jumps to here
        }
    }

    // do poisoning
    for ( int p=1 ; p<=poison ; ++p ) {
        move[0] &= ~move[p] ;
    }
    
    // Victory? (No foxes)
    if ( target == NULL ) {
		if ( move[0]==Game_none ) {
			return won ;
		}
	} else {
		if ( memcmp( move, target, poison_size ) == 0 ) {
			return won ;
		}
	}
    // Already seen?
    if ( gamesSeenFoundP( move ) == True ) {
        // game configuration already seen
        return retry ; // means try another move
    }
    
    // Valid new game, continue further
    return forward ;
}

void backTraceCreate( void ) {
    backTraceState.start = 0 ;
    backTraceState.next = 0 ;
    backTraceState.entries[0].start = 0 ;
    backTraceState.addDay = 1 ;
    backTraceState.increment = 1 ;
}

void backTraceExecute( int generation, Bits_t refer ) {
    // generation is index in backTraceState entries list (in turn indexes circular buffer)
    while ( refer != Game_all ) {
        //printf("backtrace generation=%d day=%d refer=%d\n",generation,backTraceState.entries[generation].day,refer) ;
        victoryGame[backTraceState.entries[generation].day] = backTraceList[refer].game ;
        refer = backTraceList[refer].refer ;
        if ( generation == backTraceState.start ) {
            break ;
        }
        generation = backDEC(generation) ;
    }
}

void backTraceAdd( int Day ) {
    // poison <= 1

    // back room for new data
    int insertLength = DIFF(gameListNext,gameListStart) ;
    if ( backTraceState.start != backTraceState.next ) {
        while ( DIFF(backTraceState.entries[backTraceState.start].start,backTraceState.entries[backTraceState.next].start) < insertLength ) {
            backTraceState.start = backINC(backTraceState.start) ;
            ++backTraceState.increment ;
        }
    }

    // move current state to "Back" array"
    backTraceState.entries[backTraceState.next].day = Day ;
    int index = backTraceState.entries[backTraceState.next].start ;
    for ( int inew=gameListStart ; inew != gameListNext ; inew = INC(inew), index=INC(index) ) {
        backTraceList[index].game  = gameList[inew].game ;
        backTraceList[index].refer = gameList[inew].refer ;
        gameList[inew].refer = index ;
    }
    backTraceState.next = backINC(backTraceState.next) ;
    backTraceState.entries[backTraceState.next].start = index ;
    backTraceState.addDay += backTraceState.increment ;
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
                    printf("Go back to day %d for intermediate position\n",d);
                    backTraceRestart( lastDay, d );
                } ;

                gap = False ;
                lastDay = d ;
            }
        }
    } while ( unsolved ) ;
}

void fixupMoves( void ) {
    Bits_t move[1] ; // at most one poison day
    
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

void lowPoison( void ) {
    // only for poison <= 1    
    switch (lowPoisonCreate()) { // start searching through days until a solution is found (will be fastest by definition)
    case won:
        fixupTrace() ; // fill in game path
        fixupMoves() ; // fill in moves
        printf("\n");
        printf("Winning Strategy:\n");
        for ( int d = 0 ; d < victoryDay+1 ; ++d ) {
            printf("Day%3d Move ## Game \n",d);
            showDoubleBits( victoryMove[d],victoryGame[d] ) ;
        }
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
    
    // set solver state
    backTraceCreate() ;
        
    // set Initial position
    gameList[gameListNext].game = Game_all ;
    gameList[gameListNext].refer = Game_all ;
    gamesSeenFound( gameList[gameListNext].game ) ; // flag this configuration
    ++gameListNext ;
    
    // Now loop through days
    for ( int Day=1 ; Day < MaxDays ; ++Day ) {
        printf("Day %d, States: %d, Moves %lu\n",Day+1,DIFF(gameListNext,gameListStart),Loc.iPossible);
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
    
    Bits_t move[1] ; // for poisoning

    int iold = gameListStart ; // need to define before loop
    gameListStart = gameListNext ;

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
                    backTraceExecute( backDEC(backTraceState.next), gameList[iold].refer ) ;
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


void highPoison( void ) {
    highPoisonCreate() ;
}

Searchstate_t highPoisonCreate( void ) {
    // Solve Poison >1
    // Set up arrays of game states
    //  will evaluate all states for each day for all moves and add to next day if unique

    gameListNext = 0 ;
    gameListStart = gameListNext ;
    
    typedef struct {
        Bits_t move[poison-1] ; 
    } Pstruct ;
    pGL = malloc( gamestate_length * sizeof(Pstruct) ) ;
    Pstruct * gameListPoison = pGL ;

    // initially no poison history of course
    for ( int p=1 ; p<poison ; ++p ) {
        gameListPoison[gameListStart].move[p-1] = 0 ; // no prior moves
    }

    // set Initial position
    gameList[gameListNext].game = Game_all ;
    gamesSeenFound( gameList[gameListNext].game ) ; // flag this configuration
    ++gameListNext ;
    

    // Now loop through days
    for ( int Day=1 ; Day < MaxDays ; ++Day ) {
        printf("Day %d, States: %d, Moves %lu\n",Day+1,DIFF(gameListNext,gameListStart),Loc.iPossible);
        switch ( highPoisonDay(Day,Game_none) ) {
            case won:
                printf("Victory in %d days!\n",Day ) ; // 0 index
                return won ;
            case lost:
                printf("No solution despite %d days\n",Day );
                return lost ;
            default:
                break ;
        }
    }
    printf( "Exceeded %d days.\n",MaxDays ) ;
    return lost ;
}

Searchstate_t highPoisonDay( int Day, Bits_t target ) {
    typedef struct {
        Bits_t move[poison-1] ; 
    } Pstruct ;
    Pstruct * gameListPoison = pGL ;
    
    Bits_t move[MaxPoison] ; // for poisoning

    int iold = gameListStart ; // need to define before loop
    gameListStart = gameListNext ;
        
    for (  ; iold!=gameListStart ; iold=INC(iold) ) { // each possible position
        for ( size_t ip=0 ; ip<Loc.iPossible ; ++ip ) { // each possible move
            Bits_t newT ;
            move[0] = Loc.Possible[ip] ; // actual move (and poisoning)
            for ( int p=1 ; p<poison ; ++p ) {
                move[p] = gameListPoison[iold].move[p-1] ;
            }
                    
            switch( calcMove( move, gameList[iold].game, &newT, target ) ) {
                case won:
                    victoryGame[Day] = target ;
                    victoryMove[Day] = move[0] ;
                    victoryGame[Day-1] = gameList[iold].game ;
                    if ( target == Game_none ) {
                        // real end of game
                        victoryDay = Day ;
                    }
                    return won;
                case forward:
                    // Add this game state to the furture evaluation list
                    gameList[gameListNext].game = newT ;
                    for ( int p=poison-1 ; p>0 ; --p ) {
                        gameListPoison[gameListNext].move[p] = move[p-1] ;
                    }
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
