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

void makeStoredState() ;
void jumpHolesCreate( Bits_t * J ) ;

size_t binomial( int N, int M ) ;
int premadeMovesRecurse( Bits_t * Moves, int index, int start_hole, int left, Bits_t pattern ) ;

Searchstate_t firstDay( void ) ;
Searchstate_t nextDay( int day ) ;

Searchstate_t calcMove( int day ) ;

int compare(const void* numA, const void* numB) ;
void addStoredState( Bits_t g ) ;
Bool_t findStoredStates( Bits_t g ) ;

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

void makeStoredState() {
    // Loc.Sorted already set in premadeMovesCreate
    Loc.iSorted = 0 ;
    Loc.Unsorted = Loc.Sorted + Loc.iSorted ;
    Loc.iUnsorted = 0 ;
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

void addStoredState( Bits_t g ) {
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

Bool_t findStoredStates( Bits_t g ) {
    if ( bsearch( &g, Loc.Sorted,    Loc.iSorted,   sizeof( Bits_t ), compare ) != NULL ) {
        return True ;
    }
    if ( lfind( &g, Loc.Unsorted, &Loc.iUnsorted, sizeof( Bits_t ), compare ) != NULL ) {
        return True ;
    }
    addStoredState( g ) ;
    return False ;
}

Searchstate_t calcMove( int day ) {
    // move to day+1
    if ( update ) {
        ++searchCount ;
        if ( (searchCount & 0xFFFFFF) == 0 ) {
            printf(".\n");
        }
    }

    Bits_t thisGame = victoryGame[day] ;

    // clear moves
    thisGame &= ~victoryMove[day] ;

    // calculate where they jump to
    Bits_t new_game = Game_none ;
    for ( int j=0 ; j<holes ; ++j ) {
        if ( getB( thisGame, j ) ) { // fox currently
             new_game|= Loc.Jumps[j] ; // jumps to here
        }
    }

    // do poisoning
    for ( int p=0 ; p<poison ; ++p ) {
        new_game &= ~victoryMove[day-p] ;
    }

    // Victory? (No foxes)
    if ( new_game==Game_none ) {
        victoryGame[day+1] = new_game ;
        victoryDay = day+1 ;
        return won ;
    }

    // Already seen?
    if ( findStoredStates( new_game ) == True ) {
        // game configuration already seen
        return retry ; // means try another move
    }
    
    // Valid new game, continue further
    victoryGame[day+1] = new_game ;
    return forward ;
}

Searchstate_t firstDay( void ) {
    // set up Games and Moves
    for( int i=0 ; i<=MaxDays ; ++i ) { // forward
        victoryGame[i] = Game_all ;
        victoryMove[i] = Game_none ;
    }
    for( int i=0 ; i<=MaxPoison ; ++i ) { // backward
        victoryMovePlus[i] = Game_none ;
    }
    addStoredState( Game_all ) ;

    switch ( nextDay( 0 ) ) {
        case won:
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
    
    
    for ( size_t ip=0 ; ip<Loc.iPossible ; ++ip ) { // each possible move
        victoryMove[day] = Loc.Possible[ip] ; // actual move (and poisoning)
                
        switch( calcMove( day ) ) {
            case won:
                return won;
            case forward:
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
