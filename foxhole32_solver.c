#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <malloc.h>
#include <unistd.h>
#include <string.h>

/*
 * Foxhole solver for <= 32 holes
 * uses bitmaps
 *
 * c 2022 Paul H Alfille
 * MIT license
 *
 * Find best solution
 * 
 * */

typedef uint32_t Bits_t;
typedef uint64_t Map_t;
typedef Bits_t RGM_t; // refer, game, move0, .. move[poison-1] == poison_plus+1 length
typedef Bits_t GM_t; // game, move0, .. move[poison-1] == poison_plus length
typedef Bits_t GMM_t; // game, move0, .. move[poison] == poison_plus+1 length

#include "foxholes.h"

Bits_t * premadeMoves = NULL ;
int iPremadeMoves ;

typedef int Move_t ;

// Limits
#define MaxHoles 32
#define MaxDays 300
#define MaxPoison 1 // No long poison! -- use foxhole64 or fhsolver instead

// Bit macros
// 32 bit for games, moves, jumps
#define long1 ( (uint32_t) 1 )
#define setB( map, b ) map |= (long1 << (b))
#define clearB( map, b ) map &= ~(long1 << (b))
#define getB( map, b ) (((map) & (long1 << (b))) ? 1:0)

// 64 bit for GamesMap array
#define Big1 ( (Map_t) 1 )
#define Exp64 6
#define GAMESIZE ( Big1 << (32-Exp64) )
#define Mask64 ( ( Big1<<Exp64) -1 )

#define setB64( b ) ( GamesMap[(b)>>Exp64] |= (Big1 << ( (b) & Mask64 )) )
#define getB64( b ) ( ( GamesMap[(b)>>Exp64] & (Big1 << ( (b) & Mask64 ))) ? 1 : 0 )

Map_t GamesMap[GAMESIZE]; // bitmap of all possible game states
    // indexed by foxes as bits to make a number

Bits_t * jumpHoles = NULL ; // bitmap for moves from a hole indexed by that hole

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

// pointers to start and end of circular buffer
int gameListStart ;
int gameListNext ;

// Arrays holding winning moves and game positions
int victoryDay;  // >=0 for success

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

void gamesMapCreate() ;
void jumpHolesCreate( Bits_t * J ) ;

size_t binomial( int N, int M ) ;
int premadeMovesRecurse( Bits_t * Moves, int index, int start, int level, Bits_t pattern ) ;

void lowPoison( void ) ;
Searchstate_t lowPoisonCreate( void ) ;
Searchstate_t lowPoisonDay( int Day, Bits_t target ) ;

Searchstate_t calcMove( Bits_t* move, Bits_t thisGame, Bits_t *new_game, Bits_t target ) ;

void backTraceCreate( void ) ;
void backTraceAdd( int Day ) ;
void backTraceRestart( int lastDay, int thisDay ) ;
void backTraceExecute( int generation, Bits_t refer ) ;

void fixupTrace( void ) ;
void fixupMoves( void ) ;

void showBits( Bits_t bb ) ;
void showDoubleBits( Bits_t bb, Bits_t cc ) ;

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
    printStatus(argv[0]);    
    
    setupVictory() ;

    if ( update ) {
        printf("Setting up moves\n");
    }
    // make space for the array 
    jumpHoles = (Bits_t *) malloc( holes * sizeof(Bits_t) ); // small
    jumpHolesCreate( jumpHoles ); // array of fox jump locations

    if ( update ) {
        printf("Setting up game array\n");
    }
    gamesMapCreate(); // bitmap of game layouts (to avoid revisiting)

    if ( update ) {
        printf("Starting search\n");
    }

    // calculate iPremadeMoves (Binomial coefficient holes,visits)
    iPremadeMoves = binomial( holes, visits ) + 1 ; // 0 index is no move
    premadeMoves = (Bits_t *) malloc( iPremadeMoves*sizeof(Bits_t) ) ;
    if ( premadeMoves==NULL) {
        fprintf(stderr,"Memory exhausted move array\n");
        exit(1);
    }
    // recursive fill of premadeMoves
    premadeMovesRecurse( premadeMoves, 0, 0, visits, Game_none );
    
    // Execute search (never more than 1 poisoned day)
	// Does full backtrace to show full winning strategy
	lowPoison() ;

    // print json
    if (json) {
        jsonOut() ;
        if ( jsonfile ) {
            fclose( jfile ) ;
        }
    }
}

void gamesMapCreate() {
    memset( GamesMap, 0, sizeof( GamesMap ) ) ;
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
             *new_game|= jumpHoles[j] ; // jumps to here
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
    if ( getB64( *new_game ) == 1 ) {
        // game configuration already seen
        return retry ; // means try another move
    }
    
    // Valid new game, continue further
    setB64( *new_game ) ; // flag this configuration
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
    setB64( gameList[gameListNext].game ) ; // flag this configuration
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
                    gamesMapCreate() ; // clear bits
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
            for ( int ip=0 ; ip<iPremadeMoves ; ++ip ) { // each possible move
                Bits_t newGame = Game_none ;
                Bits_t thisGame = victoryGame[Day-1] ;
                move[0] = premadeMoves[ip] ; // actual move (and poisoning)

                // clear moves
                thisGame &= ~move[0] ;

                // calculate where they jump to
                for ( int j=0 ; j<holes ; ++j ) {
                    if ( getB( thisGame, j ) ) { // fox currently
                         newGame |= jumpHoles[j] ; // jumps to here
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
    setB64( gameList[gameListNext].game ) ; // flag this configuration
    ++gameListNext ;
    
    // Now loop through days
    for ( int Day=1 ; Day < MaxDays ; ++Day ) {
        printf("Day %d, States: %d, Moves %d\n",Day+1,DIFF(gameListNext,gameListStart),iPremadeMoves);
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
        for ( int ip=0 ; ip<iPremadeMoves ; ++ip ) { // each possible move
            Bits_t newT ;
            move[0] = premadeMoves[ip] ; // actual move (and poisoning)
                    
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

