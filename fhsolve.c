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

typedef uint32_t hBits_t ;
typedef struct {
        Bits_t game ;
        union {
            struct {
                hBits_t day;
                hBits_t move[64] ;
            } ;
            Bits_t full[33] ;
        } ;
} search_t ;
search_t search_current ; // used for current search candidate
int search_moves; // how many moves include

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
int maxdays ;

// Arrays holding winning moves and game positions
int victoryDay = -1;  // >=0 for success
Bits_t victoryGame[MaxDays+1];
Bits_t victoryMovePlus[MaxPoison+MaxDays+1];
Bits_t * victoryMove = victoryMovePlus + MaxPoison ; // allows prior space for poisons

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

Bool_t Bisector( int found, int * new_max ) ;
Searchstate_t BisectionSearch( void ) ;

int compare0(const void* vkey, const void* vlist) ;
int compare1(const void* vkey, const void* vlist) ;
int compare2(const void* vkey, const void* vlist) ;
int compare3(const void* vkey, const void* vlist) ;
int compare4(const void* vkey, const void* vlist) ;
int compareP(const void* vkey, const void* vlist) ;

int (* compare )(const void *, const void *) ;

void jumpHolesCreate( Bits_t * J ) ;

size_t binomial( int N, int M ) ;
int premadeMovesRecurse( Bits_t * Moves, int index, int start_hole, int left, Bits_t pattern ) ;

Searchstate_t firstDay( void ) ;
Searchstate_t nextDay( int day ) ;

Searchstate_t calcMove( int day ) ;
Searchstate_t calcMoveFinal( int day ) ;

void makeStoredState( void ) ;
Bool_t findStoredStates( void ) ;

void showBits( Bits_t bb ) ;
void showDoubleBits( Bits_t bb, Bits_t cc ) ;
void showWin( void ) ;
void showW( void ) ;
void setupVictory() ;


void jsonOut( void ) ;

#include "getOpts.c"
#include "showBits.c"
#include "help.c"
#include "validate.c"
#include "jumpHolesCreate.c"
#include "status.c"
#include "moves.c"

int main( int argc, char **argv )
{
    // Parse Arguments
    getOpts(argc,argv) ;
    
    // Print final arguments
    printStatus(argv[0]);    

    jumpHolesCreate(Loc.Jumps); // array of fox jump locations
    Loc.Possible = Loc.Jumps + holes ;
    Loc.free -= holes ;

    if ( update ) {
        printf("Starting search\n");
    }
    Loc.iPossible = binomial( holes, visits ) + 1 ; // 0 index is no move
    if ( Loc.iPossible > Loc.free ) {
        // check memory although unlikely exhausted at this stage
        fprintf( stderr, "Memory exhaused from possible move list\n");
        exit(1);
    }
    premadeMovesRecurse( Loc.Possible, 0, 0, visits, Game_none ); // array of moves
    Loc.Sorted = Loc.Possible + Loc.iPossible ;
    Loc.free -= Loc.iPossible ;
    
    memset( search_current.move, 0, sizeof( search_current.move ) ) ; // clear all moves 


    if (rigorous) {
        // game + roundup( (day+(poison-1) )/2 )
        search_moves = poison_plus-1 ;
        search_elements = 1 + ( 1 + poison_plus ) / 2 ;
    } else {
        // game + day+/-move
        search_moves = 0;
        search_elements = 2 ;
    }
    search_size = search_elements * sizeof( Bits_t ) ;

    if ( poison <2 ) {
        // No moves included (no poison, or just a single day)
        compare = compare0 ;
    } else {
        switch ( search_elements ) {
            case 2:
                // poison = 2;
                compare = compare1 ;
                break ;
            case 3:
                // poison = 3 or 4
                compare = compare2 ;
                break ;
            case 4:
                // poison = 5 or 6
                compare = compare3 ;
                break ;
            case 5:
                // poison = 7 or 8
                compare = compare4 ;
                break ;
            default:
                // poison > 6
                compare = compareP ;
                break ;
        }
    }
    //printf("poison %d, poison_plus %d, search_moves %d, search_elements %d, search_size %d\n",poison,poison_plus,search_moves,search_elements,search_size);

    switch ( BisectionSearch() ) {
        case won:
            printf("Winning Strategy (%d days):\n",victoryDay);
            showWin();
            break ;
        case lost:
            printf("\n");
            printf("Not solved.\nUnwinnable game (with these settings)\n");
            break ;
        case overflow:
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

void showWin( void ) {
    for ( int d = 0 ; d <= victoryDay ; ++d ) {
        printf("Day%3d Move ## Game \n",d);
        showDoubleBits( Loc.Possible[victoryMove[d]],victoryGame[d] ) ;
    }
}

Searchstate_t BisectionSearch( void )
{
    int found  = 0 ;
    Bits_t vM[MaxDays+1] ;
    Bits_t vG[MaxDays+1] ;

    while ( Bisector( found, &maxdays ) ) {
        setupVictory() ;

        // reset stored state
        makeStoredState(); // bitmap of game layouts (to avoid revisiting)
    
        switch ( firstDay() ) {
            case won:
                printf("\tVictory in %d days\n",victoryDay);
                for ( int d=0 ; d<=victoryDay ; ++d ) {
                    vG[d] = victoryGame[d];
                    vM[d] = victoryMove[d];
                }
                found = victoryDay ;
                break ;
            case lost:
                printf("\tNot solved. Unwinnable game (with these settings)\n");
                return lost ;
                break ;
            case overflow:
                printf("\tNo solution within %d days.\n",maxdays);
                for ( int d=0 ; d<=victoryDay ; ++d ) {
                    victoryGame[d] = vG[d];
                    victoryMove[d] = vM[d];
                }
                found = 0 ;
                break ;
/*
            case forward:
                printf("BS forward\n");
                break ;
            case backward:
                printf("BS backward\n");
                break ;
            case retry:
                printf("BS retry\n");
                break ;
*/
            default:
                fprintf(stderr,"Unknown error\n");
                break ;            
        }        
    }

    return maxdays>0? won : overflow ;
}

struct {
    enum { bi_initial, bi_unbounded, bi_bounded, } state ;
    int known_bad ; // largest unsuccessful day limit
    int known_good ; // smallest successful day limit (-1 if no solution yet found)
    int current_max ; // current limit to be tested
    int increment ;
    int max ;
} Bisect = {
    .state = bi_initial,
} ;

Bool_t Bisector( int found, int * new_max )
{
    // Does bisect analysis for limit between 0 and MaxDays
    // found is solution day, or <1 for no solution
    // new_max is limit days to try, negative if no solution
    // Return True if more processing possible, else False

    // Round up increments
    switch (Bisect.state) {
        case bi_initial:
            Bisect.known_bad = 0,
            Bisect.known_good = -1 ;
            Bisect.max = MaxDays,
            Bisect.increment = (holes + visits - 1 ) / visits ;
            Bisect.state = bi_unbounded ;
            break ;
        case bi_unbounded:
            if ( found > 0 ) {
                Bisect.state = bi_bounded ;
                Bisect.known_good = found ;
                Bisect.increment = (Bisect.known_good - Bisect.known_bad +1) / 2 ;
            } else {
                Bisect.known_bad = Bisect.current_max ;
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
            } else {
                Bisect.known_bad = Bisect.current_max ;
            }
            Bisect.increment = (Bisect.known_good - Bisect.known_bad) / 2 ;
            break ;
    }
    if ( Bisect.increment < 1 ) {
        // no more -- either no solution (still unbounded), or solution is current limit
        *new_max = Bisect.known_good ;
        return False ;
    } else {
        Bisect.current_max = Bisect.known_bad + Bisect.increment ;
        *new_max = Bisect.current_max ;
        switch ( Bisect.state ) {
            case bi_bounded:
                printf( "Search for solution in %d days. (Known to be %d to %d).\n",Bisect.current_max,Bisect.known_bad,Bisect.known_good);
                break ;
            default:
                printf( "Search for solution in %d days. (Known to be greater than %d).\n",Bisect.current_max,Bisect.known_bad);
                break ;
            }
        return True ;
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
    Bits_t thisGame = victoryGame[day-1] ;
    thisGame &= ~Loc.Possible[victoryMove[day]] ;

    // calculate where they jump to
    Bits_t nextGame = Game_none ;
    for ( int j=0 ; j<holes ; ++j ) {
        if ( getB( thisGame, j ) ) { // fox currently
             nextGame |= Loc.Jumps[j] ; // jumps to here
        }
    }

    // do poisoning
    for ( int p=0 ; p<poison ; ++p ) {
        nextGame &= ~Loc.Possible[victoryMove[day-p]] ;
    }
    victoryGame[day] = nextGame ;
    //printf("%lX -> %lX\n",thisGame,nextGame);
    
    // Victory? (No foxes or other goal in fixup backtracking mode)
    if ( nextGame == Game_none ) {
            return won ;
    }

    // Already seen?
    search_current.game = nextGame ;
    search_current.day = day ;
    for ( int p=0; p<search_moves ; ++p ) {
        search_current.move[p] = victoryMove[day-p] ;
    }
    if ( findStoredStates() == True ) {
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
    Bits_t thisGame = victoryGame[day-1] ;
    thisGame &= ~Loc.Possible[victoryMove[day]] ;

    // calculate where they jump to
    Bits_t nextGame = Game_none ;
    for ( int j=0 ; j<holes ; ++j ) {
        if ( getB( thisGame, j ) ) { // fox currently
             nextGame |= Loc.Jumps[j] ; // jumps to here
        }
    }

    // do poisoning
    for ( int p=0 ; p<poison ; ++p ) {
        nextGame &= ~Loc.Possible[victoryMove[day-p]] ;
    }
    
    // Victory? (No foxes or other goal in fixup backtracking mode)
    if ( nextGame == Game_none ) {
        victoryGame[day] = nextGame ;
        return won ;
    }

    // Not victory -- back up since at maxdays
    return retry ;
}

Searchstate_t firstDay( void ) {
    // set up Games and Moves

    victoryGame[0] = Game_all ;
    victoryMove[0] = Game_none ; // temporary
    
    search_current.game = Game_all ;
    search_current.day = 0 ;
    findStoredStates() ; // salt the sort array

    switch ( nextDay( 1 ) ) {
        case won:
            // move working into victoryMove (reverse it)
            return won ;
        case backward:
            return lost;
        case overflow:
            return overflow;
        case forward:
            printf("FD forward\n");
            break ;
        case lost:
            printf("FD lost\n");
            break ;
        case retry:
            printf("FD retry\n");
            break ;
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
    if ( day == maxdays ) {
        for ( size_t ip=1 ; ip<Loc.iPossible ; ++ip ) { // each possible move -- ignore 0 index
            //printf("Final day=%d ip=%lu\n",day,ip);
            //victoryMove[ day ] = Loc.Possible[ip] ; // test move
            victoryMove[ day ] = ip ; // test move
                    
            switch( calcMoveFinal( day ) ) {
                case won:
                    victoryDay = day ;
                    return won;
                case retry:
                    break ;
/*
                case lost:
                    printf("ND0 lost\n");
                    break ;
                case forward:
                    printf("ND0 forward\n");
                    break ;
                case backward:
                    printf("ND0 backward\n");
                    break ;
                case overflow:
                    printf("ND0 overflow\n");
                    break ;
*/
                default:
                    fprintf(stderr,"Unknown error\n");
                    break ;            
            }
        }
        return overflow ;            
    }

    for ( size_t ip=1 ; ip<Loc.iPossible ; ++ip ) { // each possible move -- igmore 0 index
        //printf("Intermediate day=%d ip=%lu\n",day,ip);
        //victoryMove[ day ] = Loc.Possible[ip] ; // test move
        victoryMove[ day ] = ip ; // test move
                
        switch( calcMove( day ) ) {
            case won:
                victoryDay = day ;
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
/*
                    case lost:
                        printf("ND lost\n");
                        break ;
                    case forward:
                        printf("ND forward\n");
                        break ;
                    case retry:
                        printf("ND retry\n");
                        break ;
*/
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

void setupVictory() {
    // Set all bits and set up pre-computed arrays
    Game_all = 0 ;
    for ( int h=0 ; h<holes ; ++h ) {
        setB( Game_all, h ) ;
    }

    for ( size_t d = 0 ; d < sizeof(victoryGame)/sizeof(victoryGame[0]) ; ++d ) {
        victoryGame[d] = Game_all ;
    }
    
    for ( size_t d = 0 ; d < sizeof(victoryMovePlus)/sizeof(victoryMovePlus[0]) ; ++d ) {
        victoryMovePlus[d] = Game_none ;
    }
}

#define QQ(x) "\"" #x "\"" 

void jsonOut( void ) {
    // output settings and moves
    fprintf(jfile,"{");
        fprintf(jfile, QQ(length)":%d,\n", xlength);
        fprintf(jfile, QQ(width)":%d,\n",  ylength);
        fprintf(jfile, QQ(visits)":%d,\n", visits );
        fprintf(jfile, QQ(poison_days)":%d,\n", poison );
        fprintf(jfile, QQ(connection)":"QQ(%s)",\n", connName(connection) );
        fprintf(jfile, QQ(geometry)":"QQ(%s)",\n", geoName(geo) );
        if ( victoryDay >= 0 ) {
            fprintf(jfile, QQ(days)":%d,\n", victoryDay );
            fprintf(jfile, QQ(moves)":[" ) ;
                for ( int d=1; d<=victoryDay ; ++d ) {
                    fprintf(jfile,"[");
                    int v = 0 ;
                    for ( int b=0 ; b<holes ; ++b ) {
                        if ( getB(Loc.Possible[victoryMove[d]],b) ) {
                            ++v ;
                            fprintf(jfile,"%d%s",b,v==visits?"":",");
                        }
                    }
                    fprintf(jfile,"]%s",d==victoryDay?"":",");
                }
            fprintf(jfile,"],\n") ;
            fprintf(jfile, QQ(solved)":%s", "true" ); // last, no comma
        } else {
            fprintf(jfile, QQ(solved)":%s", "false" ); // last, no comma
        }   
    fprintf(jfile,"}\n");
}

#define COMPsetup const search_t * skey = vkey;\
                  const search_t * slist = vlist;\
                  register int diff = ( skey->game < slist->game ) - ( skey->game > slist->game ) ;\
                  if ( diff ) return diff

#define COMPfirst diff = ( skey->move[0] < slist->move[0] ) - ( skey->move[0] > slist->move[0] ) ;\
                  if ( diff ) return diff

#define COMPfull(n) diff = ( skey->full[n] < slist->full[n] ) - ( skey->full[n] > slist->full[n] ) ;\
                  if ( diff ) return diff


int compare0(const void* vkey, const void* vlist) {
    COMPsetup ;
    return 0 ;
}

int compare1(const void* vkey, const void* vlist) {
    COMPsetup ;
    COMPfirst ;
    return 0 ;
}

int compare2(const void* vkey, const void* vlist) {
    COMPsetup ;
    COMPfirst ;
    COMPfull(1) ;
    return 0 ;
}

int compare3(const void* vkey, const void* vlist) {
    COMPsetup ;
    COMPfirst ;
    COMPfull(1) ;
    COMPfull(2) ;
    return 0 ;
}

int compare4(const void* vkey, const void* vlist) {
    COMPsetup ;
    COMPfirst ;
    COMPfull(1) ;
    COMPfull(2) ;
    COMPfull(3) ;
    return 0 ;
}

int compareP(const void* vkey, const void* vlist) {
    COMPsetup ;
    COMPfirst ;
    COMPfull(1) ;
    COMPfull(2) ;
    COMPfull(3) ;
    for ( int n = 6 ; n <= search_elements ; ++n ) {
        COMPfull(n-2) ;
    }
    return 0 ;
}

void makeStoredState() {
    // Loc.Sorted already set in premadeMovesCreate
    // Basically there is a list of stored states ( game positions possibly including poisoned path )
    // The list is sorted except the tail end that is unsorted for a linear search
    // every so often, the unsorted is sorted back into sorted.
    Loc.iSorted = 0 ;
    Loc.Unsorted = Loc.Sorted + Loc.iSorted ;
    Loc.iUnsorted = 0 ;
    Loc.Next = Loc.Unsorted ;
}

Bool_t findStoredStates( void ) {
    // actually tries to find --> returns True
    // or adds to unsorted --> returns False
    // might re-sort if threshold met
    // aborts if no space

    // fast binary search in sorted
    search_t * found =  bsearch( &search_current, Loc.Sorted,    Loc.iSorted,   search_size, compare ) ;
    if ( found ) {
        //printf("Bsearch found %lX, %lX\n",found->game,found->full[0]);    
        // a matching element -- test date
        if ( found->day > search_current.day ) {
            // later occurence so doesn't count
            // but update occurence to this one -- no need to add or sort
            found->day = search_current.day ;
            return False ;
        }
        return True ;
    }

    // liner search in unsorted -- will add so need to keep track of size to see that
    found = lfind( &search_current, Loc.Unsorted, &Loc.iUnsorted, search_size, compare ) ;
    if ( found ) {
        //printf("Lsearch found %lX, %lX\n",found->game,found->full[0]);    
        // a matching element -- test date
        if ( found->day > search_current.day ) {
            // later occurence so doesn't count
            // but update occurence to this one -- no need to add
            found->day = search_current.day ;
            return False ;
        }
        return True ;
    }
    
    // add to unsorted, move counter and pointers
    memcpy( Loc.Next, &search_current, search_size );
    ++Loc.iUnsorted ;
    Loc.Next += search_elements ;
    
    // See if need to move unsorted to sorted and check for enough space for next increment
    if ( Loc.iUnsorted >= UNSORTSIZE ) { // move unsorted -> sorted
        Loc.free -= Loc.iUnsorted * search_elements ;
        if ( Loc.free <= (size_t) (UNSORTSIZE * search_elements) ) { // test if enough space
            fprintf(stderr, "Memory exhausted adding games seen\n");
            exit(1);
        }
        //printf("Sorting\n");
        Loc.iSorted += Loc.iUnsorted ;
        qsort( Loc.Sorted, Loc.iSorted, search_size, compare );
        Loc.Unsorted = Loc.Next ;
        Loc.iUnsorted = 0 ;
    }
    return False ;
}
