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

Bits_t Game_none = 0; // no foxes
Bits_t Game_all ;

Bits_t * premadeMoves = NULL ;
int iPremadeMoves ;

typedef int Move_t ;

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

typedef enum {
    Val_ok,
    Val_fix,
    Val_fail,
} Validation_t ;

// Limits
#define MaxHoles 32
#define MaxDays 300
#define MaxPoison 4

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

// Jumps -- macros for convenient definitions

// For moves -- go from x,y to index
// index from x,y
#define I(xx,yy) ( (xx) + (yy)*xlength )
// index from x,y but wrap x if past end (circle)
#define W(xx,yy) ( I( ((xx)+xlength)%xlength, (yy) ) )
// Index into Triangle
#define T(xx,yy) ( (yy)*((yy)+1)/2+(xx) )

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
void * pGL = NULL ; // allocated poisoned move storage
// pointers to start and end of circular buffer
int gameListStart ;
int gameListNext ;

// Arrays holding winning moves and game positions
struct {
    int day; // >=0 for success
    Bits_t game[MaxDays+1] ;
    Bits_t move[MaxDays+1] ;
} Victory ; 


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

// For recursion
typedef enum {
    won, // all foxes caught
    lost, // no more moves
    forward, // go to next day
    retry, // try another move for this day
} searchState ;

// function prototypes
int main( int argc, char **argv ) ;
void help( void ) ;
char * geoName( Geometry_t g ) ;
char * connName( Connection_t c ) ;
void printStatus() ;
Validation_t validate( void ) ;

void gamesMapCreate() ;
void jumpHolesCreate() ;

void premadeMovesCreate( void ) ;
int premadeMovesRecurse( int index, int start, int level, Bits_t pattern ) ;

void highPoison( void ) ;
searchState highPoisonCreate( void ) ;
searchState highPoisonDay( int Day, Bits_t target ) ;

void lowPoison( void ) ;
searchState lowPoisonCreate( void ) ;
searchState lowPoisonDay( int Day, Bits_t target ) ;

searchState calcMove( Bits_t* move, Bits_t thisGame, Bits_t *new_game, Bits_t target ) ;

void backTraceCreate( void ) ;
void backTraceAdd( int Day ) ;
void backTraceRestart( int lastDay, int thisDay ) ;
void backTraceExecute( int generation, Bits_t refer ) ;

void fixupTrace( void ) ;
void fixupMoves( void ) ;

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
            help() ;
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
            help();
            break ;
        }
    }

    // Check parameters and fix or exit
    
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
    
    // Set all bits and saet up pre-computed arrays
    Game_all = 0;
    for ( int h=0 ; h<holes ; ++h ) {
        setB( Game_all, h ) ;
    }
    Victory.day = -1;

    if ( update ) {
        printf("Setting up moves\n");
    }
    jumpHolesCreate(); // array of fox jump locations

    if ( update ) {
        printf("Setting up game array\n");
    }
    gamesMapCreate(); // bitmap of game layouts (to avoid revisiting)

    if ( update ) {
        printf("Starting search\n");
    }
    premadeMovesCreate(); // array of moves
    
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

void help( void ) {
    printf("Foxhole32 -- solve the foxhole puzzle\n") ;
    printf("\tsee https://github.com/alfille/doublefox.github.io\n") ;
    printf("\n") ;
    printf("By Paul H Alfille 2022 -- MIT license\n") ;
    printf("\n") ;
    printf("\t-l 5\tlength (3 to 32 default 5)\n") ;
    printf("\t-w 1\twidth (1 to 10 but max holes 32)") ;
    printf("\n") ;
    printf("\t-c\tfoxholes in a Circle\n") ;
    printf("\t-g\tfoxholes in a Grid\n") ;
    printf("\t-o\tfoxholes Offset\n") ;
    printf("\n") ;
    printf("\t-v 1\tholes Visited per day\n") ;
    printf("\t-p 0\tdays visited holes are Poisoned") ;
    printf("\n") ;
    printf("\t-u\tperiodic Updates\n") ;
    printf("\t-h\thelp\n") ;
    exit(0) ;
}

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

void gamesMapCreate() {
    memset( GamesMap, 0, sizeof( GamesMap ) ) ;
}

void jumpHolesCreate() {
    // make a bitmap of move-to location for each current location

    // make space for the array 
    jumpHoles = (Bits_t *) malloc( holes * sizeof(Bits_t) ); // small

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
        
    Bits_t * J = jumpHoles ; // bitmap to be constructed

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


searchState calcMove( Bits_t* move, Bits_t thisGame, Bits_t *new_game, Bits_t target ) {
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
        Victory.game[backTraceState.entries[generation].day] = backTraceList[refer].game ;
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

    Bits_t current = Victory.game[lastDay] ;
    Bits_t target = Victory.game[thisDay] ;

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

        for ( int d=1 ; d<Victory.day ; ++d ) { // go through days
            if ( Victory.game[d] == Game_all ) {
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
    
    for ( int Day=1 ; Day < Victory.day ; ++Day ) {
        // solve unknown moves
        if ( Victory.move[Day] == Game_none ) {
            if ( update ) {
                printf("Fixup Moves Day %d\n",Day);
            }
            for ( int ip=0 ; ip<iPremadeMoves ; ++ip ) { // each possible move
                Bits_t newGame = Game_none ;
                Bits_t thisGame = Victory.game[Day-1] ;
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
                if ( newGame == Victory.game[Day] ) {
                    Victory.move[Day] = move[0] ;
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
        for ( int d = 0 ; d < Victory.day+1 ; ++d ) {
            printf("Day%3d Move_t ## Game \n",d);
            showDoubleBits( Victory.move[d],Victory.game[d] ) ;
        }
        break ;
    default:
        break ;    
    }
}

searchState lowPoisonCreate( void ) {
    // Solve for Poison <= 1
    // Set up arrays of game states
    //  will evaluate all states for each day for all moves and add to next day if unique

    gameListNext = 0 ;
    gameListStart = gameListNext ;
    
    // Set winning path to unknown
    for ( int d=0 ; d<MaxDays ; ++d ) {
        Victory.game[d] = Game_all ;
        Victory.move[d] = Game_none ;
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

searchState lowPoisonDay( int Day, Bits_t target ) {
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
                    Victory.game[Day] = target ;
                    Victory.move[Day] = move[0] ;
                    Victory.game[Day-1] = gameList[iold].game ;
                    if ( target == Game_none ) {
                        // real end of game
                        Victory.day = Day ;
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

searchState highPoisonCreate( void ) {
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
    setB64( gameList[gameListNext].game ) ; // flag this configuration
    ++gameListNext ;
    

    // Now loop through days
    for ( int Day=1 ; Day < MaxDays ; ++Day ) {
        printf("Day %d, States: %d, Moves %d\n",Day+1,DIFF(gameListNext,gameListStart),iPremadeMoves);
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

searchState highPoisonDay( int Day, Bits_t target ) {
    typedef struct {
        Bits_t move[poison-1] ; 
    } Pstruct ;
    Pstruct * gameListPoison = pGL ;
    
    Bits_t move[MaxPoison] ; // for poisoning

    int iold = gameListStart ; // need to define before loop
    gameListStart = gameListNext ;
        
    for (  ; iold!=gameListStart ; iold=INC(iold) ) { // each possible position
        for ( int ip=0 ; ip<iPremadeMoves ; ++ip ) { // each possible move
            Bits_t newT ;
            move[0] = premadeMoves[ip] ; // actual move (and poisoning)
            for ( int p=1 ; p<poison ; ++p ) {
                move[p] = gameListPoison[iold].move[p-1] ;
            }
                    
            switch( calcMove( move, gameList[iold].game, &newT, target ) ) {
                case won:
                    Victory.game[Day] = target ;
                    Victory.move[Day] = move[0] ;
                    Victory.game[Day-1] = gameList[iold].game ;
                    if ( target == Game_none ) {
                        // real end of game
                        Victory.day = Day ;
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
        
int premadeMovesRecurse( int index, int start, int level, Bits_t pattern ) {
    // index into premadeMoves (list of moves)
    // start hole to start current level with
    // level visit number (visits down to 1)
    // pattern bitmap pattern to this point
    for ( int h=start ; h<holes-level+1 ; ++h ) { // scan through positions for this visit
        Bits_t P = pattern ;
        setB( P, h );
        if ( level==1 ) { // last visit, put in array and increment index
            premadeMoves[index] = P;
            ++index;
        } else { // recurse into futher visits
            index = premadeMovesRecurse( index, h+1, level-1, P );
        }
    }
    return index;
}

void premadeMovesCreate( void ) {
    // Create bitmaps of all possible moves (permutations of visits bits in holes slots)
    uint64_t top,bot;
    int v = visits ;

    // calculate iPremadeMoves (Binomial coefficient holes,visits)
    if (v>holes/2) {
        v = holes-visits ;
    }
    top = 1;
    bot = 1;
    for ( int i=v ; i>0 ; --i ) {
        top *= holes+1-i ;  
        bot *= i ;
    }
    iPremadeMoves = top / bot ;
    premadeMoves = (Bits_t *) malloc( iPremadeMoves*sizeof(Bits_t) ) ;
    if ( premadeMoves==NULL) {
        fprintf(stderr,"Memory exhausted move array\n");
        exit(1);
    }

    // recursive fill of premadeMoves
    premadeMovesRecurse( 0, 0, visits, Game_none );
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
        if ( Victory.day >= 0 ) {
            fprintf(jfile, QQ(days)":%d,\n", Victory.day );
            if ( poison < 2 ) { // low poison, backtrace possible
                fprintf(jfile, QQ(moves)":[" ) ;
                    for ( int d=1; d<=Victory.day ; ++d ) {
                        fprintf(jfile,"[");
                        int v = 0 ;
                        for ( int b=0 ; b<holes ; ++b ) {
                            if ( getB(Victory.move[d],b) ) {
                                ++v ;
                                fprintf(jfile,"%d%s",b,v==visits?"":",");
                            }
                        }
                        fprintf(jfile,"]%s",d==Victory.day?"":",");
                    }
                fprintf(jfile,"],\n") ;
            }
            fprintf(jfile, QQ(solved)":%s", "true" ); // last, no comma
        } else {
            fprintf(jfile, QQ(solved)":%s", "false" ); // last, no comma
        }   
    fprintf(jfile,"}\n");
}
