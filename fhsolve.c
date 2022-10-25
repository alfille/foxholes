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

Bits_t Game[MaxDays+1];
Bits_t MovePlus[MaxPoison+MaxDays+1];
Bits_t * Move = MovePlus + MaxPoison ; // allows prior space for poisons

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
void help( char * progname ) ;
char * geoName( Geometry_t g ) ;
char * connName( Connection_t c ) ;
void printStatus() ;
Validation_t validate( void ) ;

void gamesSeenCreate() ;
void jumpHolesCreate() ;

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

int main( int argc, char **argv )
{
    // Parse Arguments
    int c ;
    opterr = 0 ; // suppress option error display (to allow optional arguments)
    while ( (c = getopt( argc, argv, "468cCtTgGuUhHl:L:w:W:p:P:v:V:j:J:" )) != -1 ) {
        switch ( c ) {
        case 'h':
            help(argv[0]) ;
            break ;
        case 'l':
        case 'L':
            // xlength
            xlength = atoi(optarg);
            break ;
        case 'w':
        case 'W':
            // ylength (width)
            ylength = atoi(optarg);
            break ;
        case 'p':
        case 'P':
        // poison days
            poison = atoi(optarg);
            break ;
        case 'v':
        case 'V':
            visits = atoi(optarg);
            break ;
        case '4':
            connection = Rectangular ;
            break ;
        case '6':
            connection = Hexagonal ;
            break ;
        case '8':
            connection = Octagonal ;
            break ;
        case 'c':
        case 'C':
            geo = Circle ;
            break;
        case 'g':
        case 'G':
            geo = Grid ;
            break ;
        case 't':
        case 'T':
            geo = Triangle ;
            break ;
        case 'u':
        case 'U':
            update = True ;
            break ;
        case 'j':
        case 'J':
            json = True ;
            jfile = fopen(optarg,"w") ;
            if ( jfile == NULL ) {
                printf("Cannot open JSON file %s\n",optarg);
                exit(1);
            }
            jsonfile = True ;
            break ;
        case '?': // missing argument
            switch (optopt) {
                case 'j':
                case 'J':
                    json = True ;
                    jfile = stdout ;
                    jsonfile = False ;
                    break ;
                default:
                    break ;
            }
            break ;
        default:
            help(argv[0]);
            break ;
        }
    }

    switch ( validate() ) {
        case Val_ok:
            break ;
        case Val_fix:
            fprintf(stderr,"Continue with corrected values\n");
            break ;
        case Val_fail:
            fprintf(stderr,"Invalid parameters\n");
            exit(1) ;
    }
    
    // Print final arguments
    printStatus();    
    
    // Set all bits and set up pre-computed arrays
    Game_all = 0 ;
    for ( int h=0 ; h<holes ; ++h ) {
        setB( Game_all, h ) ;
    }

    if ( update ) {
        printf("Setting up moves\n");
    }
    jumpHolesCreate(); // array of fox jump locations

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
                printf("Day%3d Move ## Game \n",d);
                showDoubleBits( Move[d],Game[d] ) ;
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

Validation_t validate( void ) {
    Validation_t v = Val_ok ; // default

    // length
    if ( xlength < 3 ) {
        fprintf(stderr,"Bad length will set to 5\n");
        xlength = 5;
        v = Val_fix ;
    }
    if ( xlength > MaxHoles ) {
        fprintf(stderr,"Bad length will set to %d\n",MaxHoles);
        xlength = MaxHoles;
        v = Val_fix ;
    }

    // width
    if ( ylength < 1 ) {
        fprintf(stderr,"Bad width will set to 1\n");
        ylength = 1;
        v = Val_fix ;
    }
    if ( ylength > MaxHoles/3 ) {
        fprintf(stderr,"Bad width will set to %d\n",MaxHoles/3);
        ylength = MaxHoles/3 ;
        v = Val_fix ;
    }

    // poison
    if (poison<0) {
        fprintf(stderr,"Bad poisoning will set to 0 days\n");
        poison = 0;
        v = Val_fix ;
    }
    if (poison>MaxPoison) {
        fprintf(stderr,"Bad poisoning will set to %d days\n",MaxPoison);
        poison = MaxPoison;
        v = Val_fix ;
    }

    // visits (lower)
    if (visits<1) {
        fprintf(stderr,"Holes visited/day will be set to 1\n");
        visits=1 ;
        v = Val_fix ;
    }

    // Calculate holes
    // total holes
    switch( geo ) {
        case Circle:
        case Grid:
            holes = xlength * ylength ;
            break ;
        case Triangle:
            ylength = xlength ;
            holes = xlength * ( xlength+1) / 2 ;
            break ;
    }
    if ( holes > MaxHoles ) {
        fprintf(stderr,"%d Holes calculated for %s -- greater than max %d\n",holes,geoName(geo),MaxHoles);
        return Val_fail ;
    }

    // visits
    if ( visits > holes ) {
        fprintf(stderr, "Changing visits to be no larger than the number of holes (%d)\n",holes);
        visits = holes ;
        v = Val_fix ;
    }

    return v;
}

#include "help.c"

char * geoName( Geometry_t g ) {
    switch (g) {
        case Circle:
            return "circle";
        case Triangle:
            return "triangle";
        case Grid:
            return "grid";
        default:
            return "unknown_geometry";
        }
}

char * connName( Connection_t c ) {
    switch (c) {
        case Rectangular:
            return "rectangular";
        case Hexagonal:
            return "hexagonal";
        case Octagonal:
            return "octagonal";
        default:
            return "unknown_connection";
        }
}

void printStatus() {
    printf("Foxhole32 solver -- {c} 2022 Paul H Alfille\n");
    printf("\n");
    printf("Length %d X Width %d\n",xlength, ylength);
    printf("\t total %d holes \n",holes);
    printf("Geometry_t: %s\n",geoName(geo));
    printf("Connection: %s\n",connName(connection));
    printf("\n");
    printf("%d holes visited per day\n",visits);
    printf("\tHoles poisoned for %d days\n",poison);
    printf("\n");
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

void jumpHolesCreate() {
    // make a bitmap of move-to location for each current location

    /* Explanation of moves:
     *
     * Rectangular:
     *         Above
     *   Left    O    Right
     *         Below
     *
     * Hexagonal:
     *     AboveL  AboveR
     *   Left    O    Right
     *     BelowL  BelowR
     *
     * Octagonal:
     *   AboveL  Above  AboveR
     *   Left    O      Right
     *   BelowL  Below  BelowR
     *
     * */

    Bits_t * J = Loc.Jumps ; // bitmap to be constructed

    // loop though for all current locations
    switch ( geo ) {
        case Circle:
            for ( int y=0 ; y<ylength ; ++y ) { // vertical
                for ( int x=0 ; x<xlength ; ++x ) { // horizontal
                    *J = 0 ; // clear it first
                    // circle geometry -- right/left wraps around
                    switch ( connection ) {
                        case Hexagonal:
                            setB( *J, W(x-1,y) ); // Left
                            setB( *J, W(x+1,y) ) ; // Right
                            if ( y > 0 ) { // not top
                                setB( *J, W(x+(y&1)-1,y-1) ); // AboveL
                                setB( *J, W(x+(y&1)  ,y-1) ) ; // AboveR
                            }
                            if ( y < ylength-1 ) { // not bottom
                                setB( *J, W(x+(y&1)-1,y+1) ); // BelowL
                                setB( *J, W(x+(y&1)  ,y+1) ) ; // BelowR
                            }
                            break ;
                        case Rectangular:   
                            setB( *J, W(x-1,y) ); // Left
                            setB( *J, W(x+1,y) ) ; // Right
                            if ( y > 0 ) { // not top
                                setB( *J, W(x,y-1) ); // Above
                            }
                            if ( y < ylength-1 ) { // not bottom
                                setB( *J, W(x,y+1) ); // Below
                            }
                            break ;
                        case Octagonal:   
                            setB( *J, W(x-1,y) ); // Left
                            setB( *J, W(x+1,y) ) ; // Right
                            if ( y > 0 ) { // not top
                                setB( *J, W(x-1,y-1) ); // AboveL
                                setB( *J, W(x,y-1) ); // Above
                                setB( *J, W(x+1,y-1) ); // AboveR
                            }
                            if ( y < ylength-1 ) { // not bottom
                                setB( *J, W(x-1,y+1) ); // BelowL
                                setB( *J, W(x,y+1) ); // Below
                                setB( *J, W(x+1,y+1) ); // BelowR
                            }
                            break ;
                    }
                    ++J;
                }
            }
            break ;
        case Grid:
            // grid geometry -- right/left limited by boundaries
            for ( int y=0 ; y<ylength ; ++y ) { // vertical
                for ( int x=0 ; x<xlength ; ++x ) { // horizontal
                    *J = 0 ; // clear it first
                    switch ( connection ) {
                        case Hexagonal:
                            // left and right the same, up and down have 2 targets each depending on even/odd
                            if ( x > 0 ) { // not left edge
                                setB( *J, I(x-1,y) ); // Left
                            }
                            if ( x < xlength-1 ) { // not right edge
                                setB( *J, I(x+1,y) ) ; // Right
                            }
                            if ( y > 0 ) { // not top
                                if ( y&1 ) { // odd row
                                    setB( *J, I(x,y-1) ); // AboveL 
                                    if ( x < xlength-1 ) {
                                        setB( *J, I(x+1,y-1) ); // AboveR
                                    }
                                } else { // even row
                                    if ( x >0 ) {
                                        setB( *J, I(x-1,y-1) ); // AboveL
                                    }
                                    setB( *J, I(x,y-1) ); // AboveR
                                }
                            }
                            if ( y < ylength-1 ) { // not bottom
                                if ( y&1 ) { // odd row
                                    setB( *J, I(x,y+1) ); // AboveL
                                    if ( x < xlength-1 ) {
                                        setB( *J, I(x+1,y+1) ); // AboveR
                                    }
                                } else { // even row
                                    if ( x >0 ) {
                                        setB( *J, I(x-1,y+1) ); // AboveL
                                    }
                                    setB( *J, I(x,y+1) ); // AboveR
                                }
                            }
                            break ;
                        case Rectangular:
                            // normal grid (left, right, up, down) not past edges
                            if ( x > 0 ) {
                                setB( *J, I(x-1,y) );
                            }
                            if ( x < xlength-1 ) {
                                setB( *J, I(x+1,y) ) ;
                            }
                            if ( y > 0 ) {
                                setB( *J, I(x,y-1) );
                            }
                            if ( y < ylength-1 ) {
                                setB( *J, I(x,y+1) );
                            }
                            break ;
                        case Octagonal:
                            // not past edges
                            if ( x > 0 ) {
                                if ( y > 0 ) {
                                    setB( *J, I(x-1,y-1) );
                                }
                                setB( *J, I(x-1,y) );
                                if ( y < ylength-1 ) {
                                    setB( *J, I(x-1,y+1) );
                                }
                            }
                            if ( y > 0 ) {
                                setB( *J, I(x,y-1) );
                            }
                            if ( y < ylength-1 ) {
                                setB( *J, I(x,y+1) );
                            }
                            if ( x < xlength-1 ) {
                                if ( y > 0 ) {
                                    setB( *J, I(x+1,y-1) );
                                }
                                setB( *J, I(x+1,y) );
                                if ( y < ylength-1 ) {
                                    setB( *J, I(x+1,y+1) );
                                }
                            }
                            break ;
                        }
                    J++;
                }
            }
            break ;
        case Triangle:
            // triangle geometry -- right/left limited by boundaries
            for ( int y=0 ; y<xlength ; ++y ) { // vertical
                for ( int x=0 ; x<=y ; ++x ) { // horizontal
                    *J = 0 ; // clear it first
                    switch ( connection ) {
                        case Hexagonal:
                            // offset, left and right the same, up and down have 2 targets each depending on even/odd
                            if ( x > 0 ) {
                                setB( *J, T(x-1,y) ); // Left
                            }
                            if ( x < y ) {
                                setB( *J, T(x+1,y) ) ; // Right
                            }
                            if ( y > 0 ) { // above
                                if ( x>0 ) { 
                                    setB( *J, T(x-1,y-1) ); // AboveL
                                } 
                                if ( x < y ) {
                                        setB( *J, T(x,y-1) ); // AboveR
                                }
                            }
                            if ( y < xlength-1 ) { // below
                                setB( *J, T(x,y+1) ); // BelowL
                                setB( *J, T(x+1,y+1) ); // BelowR
                            }
                            break ;
                        case Rectangular:
                            // normal triangle (left, right, up, down) not past edges
                            if ( x > 0 ) {
                                setB( *J, T(x-1,y) );
                            }
                            if ( x < y ) {
                                setB( *J, T(x+1,y) ) ;
                                setB( *J, T(x,y-1) );
                            }
                            if ( y < ylength-1 ) {
                                setB( *J, T(x,y+1) );
                            }
                            break ;
                        case Octagonal:
                            // normal triangle (left, right, up, down) not past edges
                            if ( x > 0 ) {
                                setB( *J, T(x-1,y) );
                                if ( y > 0 ) {
                                    setB( *J, T(x-1,y-1) );
                                }
                                if ( y < ylength-1 ) {
                                    setB( *J, T(x-1,y+1) );
                                }
                                    
                            }
                            if ( x < y ) {
                                setB( *J, T(x+1,y) ) ;
                                setB( *J, T(x,y-1) );
                                if ( x < y-1 ) {
                                    setB( *J, T(x+1,y-1) );
                                }
                            }
                            if ( y < ylength-1 ) {
                                setB( *J, T(x,y+1) );
                                setB( *J, T(x+1,y+1) );
                            }
                            break ;
                    }
                    ++J ;
                }
            }
            break ;
    }
    Loc.Possible = J ;
}
        
void showBits( Bits_t bb ) {
    switch (geo) {
        case Triangle:
            for ( int y=0 ; y<ylength ; ++y ) {
                for ( int x=0 ; x<xlength ; ++x ) {
                    printf( (x<=y) && getB( bb, T(x,y) ) ? "X|":" |" );
                }
                printf("\n");
            }
        break ;
        default:
            for ( int y=0 ; y<ylength ; ++y ) {
                for ( int x=0 ; x<xlength ; ++x ) {
                    printf( getB( bb, I(x,y) ) ? "X|":" |" );
                }
                printf("\n");
            }
            break ;
    }
}

void showDoubleBits( Bits_t bb, Bits_t cc ) {
    switch( geo ) {
        case Triangle:
            for ( int y=0 ; y<ylength ; ++y ) {
                for ( int x=0 ; x<xlength ; ++x ) {
                    printf( (x<=y) && getB( bb, T(x,y) ) ? "X|":" |" );
                }
                printf("  ##  ");
                for ( int x=0 ; x<xlength ; ++x ) {
                    printf( (x<=y) && getB( cc, I(x,y) ) ? "X|":" |" );
                }
                printf("\n");
            }
            break ;
        default:
            for ( int y=0 ; y<ylength ; ++y ) {
                for ( int x=0 ; x<xlength ; ++x ) {
                    printf( getB( bb, I(x,y) ) ? "X|":" |" );
                }
                printf("  ##  ");
                for ( int x=0 ; x<xlength ; ++x ) {
                    printf( getB( cc, I(x,y) ) ? "X|":" |" );
                }
                printf("\n");
            }
            break ;
    }
}

Searchstate_t calcMove( int day ) {
    // move to day+1
    if ( update ) {
        ++searchCount ;
        if ( (searchCount & 0xFFFFFF) == 0 ) {
            printf(".\n");
        }
    }

    Bits_t thisGame = Game[day] ;

    // clear moves
    thisGame &= ~Move[day] ;

    // calculate where they jump to
    Bits_t new_game = Game_none ;
    for ( int j=0 ; j<holes ; ++j ) {
        if ( getB( thisGame, j ) ) { // fox currently
             new_game|= Loc.Jumps[j] ; // jumps to here
        }
    }

    // do poisoning
    for ( int p=0 ; p<poison ; ++p ) {
        new_game &= ~Move[day-p] ;
    }

    // Victory? (No foxes)
    if ( new_game==Game_none ) {
        Game[day+1] = new_game ;
        victoryDay = day+1 ;
        return won ;
    }

    // Already seen?
    if ( gamesSeenFound( new_game ) == True ) {
        // game configuration already seen
        return retry ; // means try another move
    }
    
    // Valid new game, continue further
    Game[day+1] = new_game ;
    return forward ;
}

Searchstate_t firstDay( void ) {
    // set up Games and Moves
    for( int i=0 ; i<=MaxDays ; ++i ) { // forward
        Game[i] = Game_all ;
        Move[i] = Game_none ;
    }
    for( int i=0 ; i<=MaxPoison ; ++i ) { // backward
        MovePlus[i] = Game_none ;
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
    // Game is set for this day
    // Moves are tested for this day
    int ovrflw = 0 ;

    if ( day >= MaxDays ) {
        return overflow;
    }
    
    
    for ( size_t ip=0 ; ip<Loc.iPossible ; ++ip ) { // each possible move
        Move[day] = Loc.Possible[ip] ; // actual move (and poisoning)
                
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

#define QQ(x) "\"" #x "\"" 

void jsonOut( void ) {
    // output settings and moves
    fprintf(jfile,"{");
        fprintf(jfile, QQ(length)":%d,\n", xlength);
        fprintf(jfile, QQ(width)":%d,\n",  ylength);
        fprintf(jfile, QQ(visits)":%d,\n", visits );
        fprintf(jfile, QQ(connection)":"QQ(%s)",\n", connName(connection) );
        fprintf(jfile, QQ(geometry)":"QQ(%s)",\n", geoName(geo) );
        if ( victoryDay >= 0 ) {
            fprintf(jfile, QQ(days)":%d,\n", victoryDay );
            if ( poison < 2 ) { // low poison, backtrace possible
                fprintf(jfile, QQ(moves)":[" ) ;
                    for ( int d=1; d<=victoryDay ; ++d ) {
                        fprintf(jfile,"[");
                        int v = 0 ;
                        for ( int b=0 ; b<holes ; ++b ) {
                            if ( getB(Move[d],b) ) {
                                ++v ;
                                fprintf(jfile,"%d%s",b,v==visits?"":",");
                            }
                        }
                        fprintf(jfile,"]%s",d==victoryDay?"":",");
                    }
                fprintf(jfile,"],\n") ;
            }
            fprintf(jfile, QQ(solved)":%s", "true" ); // last, no comma
        } else {
            fprintf(jfile, QQ(solved)":%s", "false" ); // last, no comma
        }   
    fprintf(jfile,"}\n");
}
