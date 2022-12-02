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

#define working_size MaxPoison + MaxDays
GMM_t working[ working_size ] ;

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

Searchstate_t calcMove( GMM_t * gmove ) ;

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

    for ( int i=0 ; i<working_size ; ++i ) {
        working[i] = 0 ;
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

Searchstate_t calcMove( GMM_t * gmove ) {
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
    if ( gmove[0] == Game_none ) {
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

Searchstate_t firstDay( void ) {
    // set up Games and Moves
    for ( int i=0 ; i<working_size ; ++i ) {
        working[i] = Game_none ;
    }

    GMM_t * pworking = & working[ working_size - poison_plus -1 ] ;
    victoryGame[0] = Game_all ;
    pworking[0] = victoryGame[0] ;
    findStoredStates( pworking ) ; // salt the sort array

    switch ( nextDay( 0 ) ) {
        case won:
            // move working into victoryMove (reverse it)
            for ( int d = 0 ; d <= victoryDay ; ++d ) {
                victoryMove[d] = working[ working_size - poison_plus - d ] ;
            }
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

    if ( day >= MaxDays ) {
        return overflow;
    }

    GMM_t * pworking = & working[ working_size - poison_plus -1 -day ] ;
    
    for ( size_t ip=0 ; ip<Loc.iPossible ; ++ip ) { // each possible move
        pworking[0] = victoryGame[day] ;
        pworking[1] = Loc.Possible[ip] ; // actual move (and poisoning)
                
        switch( calcMove( pworking ) ) {
            case won:
                victoryDay = day+1 ;
                victoryGame[victoryDay] = Game_none ;
                return won;
            case forward:
                victoryGame[day+1] = pworking[0] ;
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
