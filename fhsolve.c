#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <malloc.h>
#include <unistd.h>
#include <string.h>
#include <search.h>

/*
 * Foxhole solver for <= 64 holes
 *
 * c 2022 Paul H Alfille
 * MIT license
 *
 * Finds any solution
 * 
 * */

typedef uint64_t Bits_t;
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
#define MaxDays 1000
#define MaxPoison MaxDays

struct {
    enum state { bi_initial, bi_unbounded, bi_bounded, } ;
    int known_bad ; // largest unsuccessful day limit
    int known_good ; // smallest successful day limit (-1 if no solution yet found)
    int current_max ; // current limit to be tested
    int increment ;
    int max ;
} Bisect = {
    .state = bi_initial,
    .known_bad = 0,
    .known_good = -1 ;
    .max = MaxDays,
}

Bool_t Bisector( int found, int * new_max )
{
    // Does bisect analysis for limit between 0 and MaxDays
    // found is solution day, or <1 for no solution
    // new_max is limit days to try, negative if no solution
    // Return True if more processing possible, else False
    
    switch (Bisect.state) {
        case bi_initial:
            Bisect.increment = holes / visits ;
            Bisect.state = bi_unbounded ;
            break ;
        case bi_unbounded:
            if ( found > 0 ) {
                Bisect.state = bi_bounded ;
                Bisect.known_good = found ;
                Bisect.increment = (Bisect.known_good - Bisect.known_bad) / 2 ;
            } else {
                Bisect.increment *= 2 ;
                int real_limit = Bisect.max - Bisect.known_bad ;
                if ( Bisect.increment > real_limit ) {
                    Bisect.increment = real_limit ;
                }
            }
            break ;
        case bi_bounded:
            if ( found > 0 ) {
                Bisect.known_good = found ;
            } else 
                Bisect.known_bad = found ;
            }
            Bisect.increment = (Bisect.known_good - Bisect.known_bad) / 2 ;
            break ;
    }
    if ( Bisect.increment < 1 ) {
        *new_max = Bisect.current_max ;
        return False ;
    } else {
        Bisect.current_max = Bisect.known_bad + Bisect.increment ;
        *new_max = Bisect.current_max ;
        return True ;
    }
}        

int maxdays ;

// Bit macros
// 64 bit for games, moves, jumps
#define long1 ((uint64_t) 1)
#define setB( map, b ) map |= (long1 << (b))
#define clearB( map, b ) map &= ~(long1 << (b))
#define getB( map, b ) (((map) & (long1 << (b))) ? 1:0)

// function prototypes
int main( int argc, char **argv ) ;
void getOpts( int argc, char ** argv ) ;
void help( char * progname ) ;
char * geoName( Geometry_t g ) ;
char * connName( Connection_t c ) ;
void printStatus( char * progname ) ;
Validation_t validate( void ) ;

void jumpHolesCreate( Bits_t * J ) ;

size_t binomial( int N, int M ) ;
int premadeMovesRecurse( Bits_t * Moves, int index, int start_hole, int left, Bits_t pattern ) ;

Searchstate_t firstDay( void ) ;
Searchstate_t nextDay( int day ) ;

Searchstate_t calcMove( int day ) ;
Searchstate_t calcMoveFinal( int day ) ;

void makeStoredState() ;
Bool_t findStoredStates( GM_t * g ) ;

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
#include "compare.c"
#include "storedState.c"

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

    switch ( firstDay() ) {
        case won:
            printf("\n");
            printf("Victory in %d days\n",victoryDay);
            printf("Winning Strategy:\n");
            for ( int d = 0 ; d < victoryDay+1 ; ++d ) {
                printf("Day%3d victoryMove ## victoryGame \n",d);
                showDoubleBits( victoryMove[d],victoryGame[d] ) ;
            }
            break ;
        case lost:
            printf("\n");
            printf("Not solved.\nUnwinnable game (with these settings)\n");
            break ;
        case overflow:
            printf("\n");
            printf("No solution within maximum %d days.\n",MaxDays);
            break ;        
        default:
            fprintf(stderr,"Unknown error\n");
            break ;            
    }        
    
    // print json
    if (json) {
        jsonOut() ;
        if ( jsonfile ) {
            fclose( jfile ) ;
        }
    }
}

Searchstate_t calcMove( int day ) {
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
    //printf("Calc move day=%d %lX, %lX", day, victoryMove[day+1], victoryMove[day]);

    // clear moves
    Bits_t thisGame = victoryMove[day+1] ;
    thisGame &= ~victoryMove[day] ;

    // calculate where they jump to
    Bits_t nextGame = Game_none ;
    for ( int j=0 ; j<holes ; ++j ) {
        if ( getB( thisGame, j ) ) { // fox currently
             nextGame |= Loc.Jumps[j] ; // jumps to here
        }
    }

    // do poisoning
    for ( int p=0 ; p<poison ; ++p ) {
        nextGame &= ~victoryMove[day-p] ;
    }
    victoryMove[day+1] = nextGame ;
    //printf(" -> %lX\n",nextGame);
    
    // Victory? (No foxes or other goal in fixup backtracking mode)
    if ( nextGame == Game_none ) {
            return won ;
    }

    // Already seen?
    if ( findStoredStates( victoryMove + day + 2 - search_elements ) == True ) {
        // game configuration already seen
        return retry ; // means try another move
    }
    
    // Valid new game, continue further
    return forward ;
}

Searchstate_t calcMoveFinal( int day ) {
    // Called on last day -- no need to add to list or prune, just check for victory
    
    // coming in, move has game,newmove,move0,..move[p-2]
    // returning move has newgame,newmove,move0,...,move[n-p]
    // target points to:
    //   NULL Game_none
    //   non--NULL, move array (poison_plus length)

    // clear moves
    Bits_t thisGame = victoryMove[day+1] ;
    thisGame &= ~victoryMove[day] ;

    // calculate where they jump to
    Bits_t nextGame = Game_none ;
    for ( int j=0 ; j<holes ; ++j ) {
        if ( getB( thisGame, j ) ) { // fox currently
             nextGame |= Loc.Jumps[j] ; // jumps to here
        }
    }

    // do poisoning
    for ( int p=0 ; p<poison ; ++p ) {
        nextGame &= ~victoryMove[day-p] ;
    }
    
    // Victory? (No foxes or other goal in fixup backtracking mode)
    if ( nextGame == Game_none ) {
        victoryMove[day+1] = nextGame ;
        return won ;
    }

    // Not victory -- back up since at maxdays
    return retry ;
}

Searchstate_t firstDay( void ) {
    // set up Games and Moves

    victoryGame[0] = Game_all ;
    victoryMove[0] = Game_all ; // temporary
    findStoredStates( victoryMove +1 - search_elements ) ; // salt the sort array

    maxdays = MaxDays ; // copy of maxdays

    switch ( nextDay( 0 ) ) {
        case won:
            // move working into victoryMove (reverse it)
            return won ;
        case backward:
            return lost;
        case overflow:
            return overflow;
        default:
            fprintf(stderr,"Unknown error\n");
            break ;            
    }
    return lost ;
}

Searchstate_t nextDay( int day ) {
    // victoryGame is set for this day
    // Moves are tested for this day
    int ovrflw = 0 ;

    if ( day == maxdays-1 ) {
        victoryMove[ day+1 ]  = victoryGame[day] ; // temporary storage
        victoryMove[ day ] = Loc.Possible[ip] ; // test move
                
        switch( calcMoveFinal( day ) ) {
            case won:
                victoryDay = day+1 ;
                victoryGame[victoryDay] = Game_none ;
                return won;
            case retry:
                break ;
            default:
                fprintf(stderr,"Unknown error\n");
                break ;
        }
        return overflow ;            
    }

    for ( size_t ip=0 ; ip<Loc.iPossible ; ++ip ) { // each possible move
        victoryMove[ day+1 ]  = victoryGame[day] ; // temporary storage
        victoryMove[ day ] = Loc.Possible[ip] ; // test move
                
        switch( calcMove( day ) ) {
            case won:
                victoryDay = day+1 ;
                victoryGame[victoryDay] = Game_none ;
                return won;
            case forward:
                victoryGame[day+1] = victoryMove[day+1] ;
                switch ( nextDay( day+1 ) ) {
                    case won:
                        return won ;
                    case backward:
                        break ;
                    case overflow:
                        ovrflw = 1 ;
                        break ;
                    default:
                        fprintf(stderr,"Strange status reported day %d\n",day+1);
                        break ;
                }
                break ;
            case retry:
                break ;
            default:
                fprintf(stderr,"Unknown error\n");
                break ;            
        }
    }
    return ovrflw ? overflow : backward ;
}
