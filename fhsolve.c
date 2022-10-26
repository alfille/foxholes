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

typedef enum {
    Val_ok,
    Val_fix,
    Val_fail,
} Validation_t ;

Bits_t Game_none = 0; // no foxes
Bits_t Game_all ;

typedef enum { False, True } Bool_t ;

typedef enum { Circle, Grid, Triangle } Geometry_t ;
typedef enum { Rectangular, Hexagonal, Octagonal } Connection_t ;

// Globals from command line
int xlength = 5;
int ylength = 1;
int holes;
int poison = 0;
int visits = 1;
Bool_t update = 0 ;
Connection_t connection = Rectangular ;
Geometry_t geo = Circle ;
int searchCount = 0 ;
Bool_t json = False ;
Bool_t jsonfile = False ;
FILE * jfile ;

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

// Jumps -- macros for convenient definitions

// For moves -- go from x,y to index
// index from x,y
#define I(xx,yy) ( (xx) + (yy)*xlength )
// index from x,y but wrap x if past end (circle)
#define W(xx,yy) ( I( ((xx)+xlength)%xlength, (yy) ) )
// Index into Triangle
#define T(xx,yy) ( (yy)*((yy)+1)/2+(xx) )

// Arrays holding winning moves and game positions
int victoryDay = -1 ;

Bits_t victoryGame[MaxDays+1];
Bits_t victoryMovePlus[MaxPoison+MaxDays+1];
Bits_t * victoryMove = victoryMovePlus + MaxPoison ; // allows prior space for poisons

// For recursion

typedef enum {
    won, // all foxes caught
    lost, // no more moves
    forward, // go to next day
    backward, // no choices here
    overflow, // too many days
    retry, // try another move for this day
} Searchstate_t ;

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

void premadeMovesCreate( void ) ;
void premadeMovesRecurse( int start_hole, int left, Bits_t pattern ) ;

Searchstate_t firstDay( void ) ;
Searchstate_t nextDay( int day ) ;

Searchstate_t calcMove( int day ) ;

int compare(const void* numA, const void* numB) ;
void gamesSeenAdd( Bits_t g ) ;
Bool_t gamesSeenFound( Bits_t g ) ;

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

int main( int argc, char **argv )
{
    // Parse Arguments
    getOpts(argc,argv) ;
    
    // Print final arguments
    printStatus(argv[0]);    
    
    // Set all bits and set up pre-computed arrays
    Game_all = 0 ;
    for ( int h=0 ; h<holes ; ++h ) {
        setB( Game_all, h ) ;
    }

    if ( update ) {
        printf("Setting up moves\n");
    }
    jumpHolesCreate(Loc.Jumps); // array of fox jump locations
    Loc.Possible = Loc.Jumps + holes ;

    if ( update ) {
        printf("Starting search\n");
    }
    premadeMovesCreate(); // array of moves
    
    if ( update ) {
        printf("Setting up game array\n");
    }
    gamesSeenCreate(); // bitmap of game layouts (to avoid revisiting)

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

void gamesSeenCreate() {
    // Loc.Sorted already set in premadeMovesCreate
    Loc.iSorted = 0 ;
    Loc.Unsorted = Loc.Sorted + Loc.iSorted ;
    Loc.iUnsorted = 0 ;
    Loc.free = STORESIZE - (Loc.Sorted - Loc.Jumps) ;
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
    if ( lfind( &g, Loc.Unsorted, &Loc.iUnsorted, sizeof( Bits_t ), compare ) != NULL ) {
        return True ;
    }
    gamesSeenAdd( g ) ;
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
    if ( gamesSeenFound( new_game ) == True ) {
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
    gamesSeenAdd( Game_all ) ;

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


void premadeMovesRecurse( int start_hole, int left, Bits_t pattern ) {
    // index into premadeMoves (list of moves)
    // start hole to start current level with
    // level visit number (visits down to 1)
    // pattern bitmap pattern to this point
    for ( int h=start_hole ; h<holes-left+1 ; ++h ) { // scan through positions for this visit
        Bits_t P = pattern ;
        setB( P, h );
        if ( left==1 ) { // last visit, put in array and increment index
            Loc.Possible[Loc.iPossible] = P;
            ++Loc.iPossible;
            --Loc.free ;
            if ( Loc.free == 0 ) {
                // check memory although unlikely exhausted at this stage
                fprintf( stderr, "Memory exhaused from possible move list\n");
                exit(1);
            }
        } else { // recurse into futher visits
            premadeMovesRecurse( h+1, left-1, P );
        }
    }
}

void premadeMovesCreate( void ) {
    // Create bitmaps of all possible moves (permutations of visits bits in holes slots)
    Loc.free = STORESIZE - holes ;

    // recursive fill of premadeMoves
    premadeMovesRecurse( 0, visits, Game_none );
    Loc.Sorted = Loc.Possible + Loc.iPossible ;
}
