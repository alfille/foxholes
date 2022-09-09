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

typedef uint32_t Bits;
typedef uint64_t Map;

Bits Game_none = 0; // no foxes
Bits Game_all ;

Bits * premadeMoves = NULL ;
int iPremadeMoves ;

typedef int Move ;

typedef enum { False, True } Bool ;

// Globals from command line
int xlength = 5;
int ylength = 1;
int holes;
int poison = 0;
int visits = 1;
Bool update = 0 ;
Bool offset = False ;
Bool circle = False ;
int maxday = 1000000 ;
int searchCount = 0 ;
Bool json = False ;
Bool jsonfile = False ;
FILE * jfile ;

// Limits
#define MAXHOLES 32
#define MAXPOISON 4
#define MaxDays 300

// Bit macros
// 32 bit for games, moves, jumps
#define setB( map, b ) map |= (1 << (b))
#define clearB( map, b ) map &= ~(1 << (b))
#define getB( map, b ) (((map) & (1 << (b))) ? 1:0)

// 64 bit for GamesMap array
#define Big1 ( (Map) 1 )
#define Exp64 6
#define GAMESIZE ( Big1 << (32-Exp64) )
#define Mask64 ( ( Big1<<Exp64) -1 )

#define setB64( b ) ( GamesMap[(b)>>Exp64] |= (Big1 << ( (b) & Mask64 )) )
#define getB64( b ) ( ( GamesMap[(b)>>Exp64] & (Big1 << ( (b) & Mask64 ))) ? 1 : 0 )

Map GamesMap[GAMESIZE]; // bitmap of all possible game states
    // indexed by foxes as bits to make a number

// Jumps -- macros for convenient definitions

// For moves -- go from x,y to index
// index from x,y
#define I(xx,yy) ( (xx) + (yy)*xlength )
// index from x,y but wrap x if past end (circle)
#define W(xx,yy) ( I( ((xx)+xlength)%xlength, (yy) ) )

Bits * jumpHoles = NULL ; // bitmap for moves from a hold indexed by that hole

// Array of moves to be tested (or saved for backtrace -- circular buffer with macros)

#define gamestate_length 0x100000
#define INC(x) ( ((x)+1) % gamestate_length )
#define DEC(x) ( ((x)+gamestate_length-1) % gamestate_length )
#define DIFF(x,y) ( ( gamestate_length+(x)-(y) ) % gamestate_length )

struct tryStruct {
    Bits game ;
    Bits refer ;
} ;
struct tryStruct gameList[gamestate_length]; // fixed size array holding intermediate game states
struct tryStruct backTraceList[gamestate_length]; // fixed size array holding intermediate game states
void * pGL = NULL ; // allocated poisoned move storage
// pointers to start and end of circular buffer
int gameListStart ;
int gameListNext ;

// Array holding winning moves and game positions
int victoryDay = -1; // >=0 for success
Bits victoryGame[MaxDays+1];
Bits victoryMove[MaxDays+1];

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
void printStatus() ;

void gamesMapCreate() ;
void jumpHolesCreate() ;

void premadeMovesCreate( void ) ;
int premadeMovesRecurse( int index, int start, int level, Bits pattern ) ;

void HighPoison( void ) ;
searchState HighPoisonCreate( void ) ;
searchState HighPoisonDay( int Day, Bits target ) ;

void LowPoison( void ) ;
searchState LowPoisonCreate( void ) ;
searchState LowPoisonDay( int Day, Bits target ) ;

searchState calcMove( Bits* move, Bits thisGame, Bits *new_game, Bits target ) ;

void backTraceCreate( void ) ;
void backTraceAdd( int Day ) ;
void backTraceRestart( int lastDay, int thisDay ) ;
void backTraceExecute( int generation, Bits refer ) ;

void fixupTrace( void ) ;
void fixupMoves( void ) ;

void showBits( Bits bb ) ;
void showDoubleBits( Bits bb, Bits cc ) ;

void Json_out( void ) ;


int main( int argc, char **argv )
{
    // Parse Arguments
    int c ;
    opterr = 0 ; // suppress option error display (to allow optional arguments)
    while ( (c = getopt( argc, argv, "ocguhl:L:w:W:p:P:v:V:m:M:j:J:" )) != -1 ) {
        switch ( c ) {
        case 'h':
            help() ;
            break ;
        case 'l':
        case 'L':
            // xlength
            xlength = atoi(optarg);
            if ( xlength < 3 ) {
                fprintf(stderr,"Bad length will set to 5\n");
                xlength = 5;
            }
            break ;
        case 'w':
        case 'W':
            // ylength (width)
            ylength = atoi(optarg);
            if ( xlength < 1 ) {
                fprintf(stderr,"Bad width will set to 1\n");
                ylength = 1;
            }
            break ;
        case 'p':
        case 'P':
        // poison days
            poison = atoi(optarg);
            if (poison<0) {
                poison = 0;
            }
            break ;
        case 'v':
        case 'V':
            visits = atoi(optarg);
            if (visits<1) {
                visits=1 ;
            }
            break ;
        case 'o':
        case 'O':
            offset = True ;
            break ;
        case 'c':
        case 'C':
            circle = True ;
            break;
        case 'g':
        case 'G':
            circle = False ;
            break ;
        case 'u':
        case 'U':
            update = True ;
            break ;
        case 'm':
        case 'M':
            maxday = atoi(optarg);
            if ( maxday < 1 ) {
                fprintf( stderr, "Maximum number of days allowed is too small\n" );
                exit(1);
            }
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
    holes = xlength * ylength ;
    if ( holes > MAXHOLES ) {
        fprintf(stderr,"Length %d X Width %d = Total %d\n\tGreater than max %d\n",xlength,ylength,holes,MAXHOLES);
        exit(1);
    }

    // visits
    if ( visits > holes ) {
        fprintf(stderr, "Changing visits to be no larger than the number of holes (%d)\n",holes);
        visits = holes ;
    }

    // poison
    if ( poison > MAXPOISON ) {
        fprintf(stderr, "Changing poison days to be no larger than the maximum programmed for (%d)\n",MAXPOISON);
        poison = MAXPOISON ;
    }

    // Print final arguments
    printStatus();    
    
    // Set all bits and saet up pre-computed arrays
    for ( int h=0 ; h<holes ; ++h ) {
        setB( Game_all, h ) ;
    }

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
    
    // Execu7t search (different if more than 1 poisoned day)
    if ( poison < 2 ) {
        // Does full backtrace to show full winning strategy
        LowPoison() ;
    } else {
        // Just shows shortest time
        HighPoison() ;
    }
    
    // print json
    if (json) {
        Json_out() ;
        if ( jsonfile ) {
            fclose( jfile ) ;
        }
    }
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
    printf("\t-m 10\tmaximum number of days allowed") ;
    printf("\n") ;
    printf("\t-u\tperiodic Updates\n") ;
    printf("\t-h\thelp\n") ;
    exit(0) ;
}

void printStatus() {
    printf("Foxhole32 solver -- {c} 2022 Paul H Alfille\n");
    printf("\n");
    printf("Length %d X Width %d\n",xlength, ylength);
    printf("\t total %d holes \n",holes);
    printf(circle?"Circle geometry\n":"Grid geometry\n");
    printf(offset?"\toffset holes\n":"\tno offset\n");
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
    jumpHoles = (Bits *) malloc( holes * sizeof(Bits) ); // small

    /* Explanation of moves:
     *
     * Non-offset:
     *         Above
     *   Left    O    Right
     *         Below
     *
     * Offset:
     *     AboveL  AboveR
     *   Left    O    Right
     *     BelowL  BelowR
     *
     * */
        
    // loop though for all current locations
    for ( int y=0 ; y<ylength ; ++y ) { // vertical
        for ( int x=0 ; x<xlength ; ++x ) { // horizontal
            Bits * J = &jumpHoles[x + xlength*y] ; // bitmap to be constructed
            *J = 0 ; // clear it first
            if ( circle ) { // circle geometry -- right/left wraps around
                if ( offset ) { // offset circle
                    setB( *J, W(x-1,y) ); // Left
                    setB( *J, W(x+1,y) ) ; // Right
                    if ( y > 0 ) {
                        setB( *J, W(x+(y&1)-1,y-1) ); // AboveL
                        setB( *J, W(x+(y&1)  ,y-1) ) ; // AboveR
                    }
                    if ( y < ylength-1 ) { // below
                        setB( *J, W(x+(y&1)-1,y+1) ); // BelowL
                        setB( *J, W(x+(y&1)  ,y+1) ) ; // BelowR
                    }
                } else { // normal circle (wrap x, not y)
                    setB( *J, W(x-1,y) ); // Left
                    setB( *J, W(x+1,y) ) ; // Right
                    if ( y > 0 ) {
                        setB( *J, I(x,y-1) ); // Above
                    }
                    if ( y < ylength-1 ) {
                        setB( *J, I(x,y+1) ); // Below
                    }
                }
            } else { // grid geometry -- right/left limited by boundaries
                if ( offset ) { // offset, left and right the same, up and down have 2 targets each depending on even/odd
                    if ( x > 0 ) {
                        setB( *J, I(x-1,y) ); // Left
                    }
                    if ( x < xlength-1 ) {
                        setB( *J, I(x+1,y) ) ; // Right
                    }
                    if ( y > 0 ) { // above
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
                    if ( y < ylength-1 ) { // below
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
                } else { // normal grid (left, right, up, down) not past edges
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
                }
            }
        }
    }
}
        
void showBits( Bits bb ) {
    for ( int y=0 ; y<ylength ; ++y ) {
        for ( int x=0 ; x<xlength ; ++x ) {
            printf( getB( bb, I(x,y) ) ? "X|":" |" );
        }
        printf("\n");
    }
}

void showDoubleBits( Bits bb, Bits cc ) {
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
}

searchState calcMove( Bits* move, Bits thisGame, Bits *new_game, Bits target ) {
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

void backTraceExecute( int generation, Bits refer ) {
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

    Bits current = victoryGame[lastDay] ;
    Bits target = victoryGame[thisDay] ;

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
        switch ( LowPoisonDay( Day, target ) ) {
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
    Bits move[1] ; // at most one poison day
    
    for ( int Day=1 ; Day < victoryDay ; ++Day ) {
        // solve unknown moves
        if ( victoryMove[Day] == Game_none ) {
            if ( update ) {
                printf("Fixup Moves Day %d\n",Day);
            }
            for ( int ip=0 ; ip<iPremadeMoves ; ++ip ) { // each possible move
                Bits newGame = Game_none ;
                Bits thisGame = victoryGame[Day-1] ;
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

void LowPoison( void ) {
    // only for poison <= 1    
    switch (LowPoisonCreate()) { // start searching through days until a solution is found (will be fastest by definition)
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

searchState LowPoisonCreate( void ) {
    // Solve for Poison <= 1
    // Set up arrays of game states
    //  will evaluate all states for each day for all moves and add to next day if unique

    gameListNext = 0 ;
    gameListStart = gameListNext ;
    
    // create an all-fox state
    Game_all = 0 ; // clear bits
    for ( int h=0 ; h<holes ; ++h ) { // set all holes bits
        setB( Game_all, h ); // all holes have foxes
    }
    
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
    for ( int Day=1 ; Day < maxday ; ++Day ) {
        printf("Day %d, States: %d, Moves %d\n",Day+1,DIFF(gameListNext,gameListStart),iPremadeMoves);
        switch ( LowPoisonDay(Day,Game_none) ) {
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
    printf( "Exceeded %d days.\n",maxday ) ;
    return lost ;
}

searchState LowPoisonDay( int Day, Bits target ) {
    // poison <= 1
    
    Bits move[1] ; // for poisoning

    int iold = gameListStart ; // need to define before loop
    gameListStart = gameListNext ;
        
    for (  ; iold!=gameListStart ; iold=INC(iold) ) { // each possible position
        for ( int ip=0 ; ip<iPremadeMoves ; ++ip ) { // each possible move
            Bits newT ;
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


void HighPoison( void ) {
    HighPoisonCreate() ;
}

searchState HighPoisonCreate( void ) {
    // Solve Poison >1
    // Set up arrays of game states
    //  will evaluate all states for each day for all moves and add to next day if unique

    gameListNext = 0 ;
    gameListStart = gameListNext ;
    
    typedef struct {
        Bits move[poison-1] ; 
    } Pstruct ;
    pGL = malloc( gamestate_length * sizeof(Pstruct) ) ;
    Pstruct * gameListPoison = pGL ;

    // initially no poison history of course
    for ( int p=1 ; p<poison ; ++p ) {
        gameListPoison[gameListStart].move[p-1] = 0 ; // no prior moves
    }

    // create an all-fox state
    Game_all = 0 ; // clear bits
    for ( int h=0 ; h<holes ; ++h ) { // set all holes bits
        setB( Game_all, h ); // all holes have foxes
    }
    
    // set Initial position
    gameList[gameListNext].game = Game_all ;
    setB64( gameList[gameListNext].game ) ; // flag this configuration
    ++gameListNext ;
    

    // Now loop through days
    for ( int Day=1 ; Day < maxday ; ++Day ) {
        printf("Day %d, States: %d, Moves %d\n",Day+1,DIFF(gameListNext,gameListStart),iPremadeMoves);
        switch ( HighPoisonDay(Day,Game_none) ) {
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
    printf( "Exceeded %d days.\n",maxday ) ;
    return lost ;
}

searchState HighPoisonDay( int Day, Bits target ) {
    typedef struct {
        Bits move[poison-1] ; 
    } Pstruct ;
    Pstruct * gameListPoison = pGL ;
    
    Bits move[MAXPOISON] ; // for poisoning

    int iold = gameListStart ; // need to define before loop
    gameListStart = gameListNext ;
        
    for (  ; iold!=gameListStart ; iold=INC(iold) ) { // each possible position
        for ( int ip=0 ; ip<iPremadeMoves ; ++ip ) { // each possible move
            Bits newT ;
            move[0] = premadeMoves[ip] ; // actual move (and poisoning)
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
        
int premadeMovesRecurse( int index, int start, int level, Bits pattern ) {
    // index into premadeMoves (list of moves)
    // start hole to start current level with
    // level visit number (visits down to 1)
    // pattern bitmap pattern to this point
    for ( int h=start ; h<holes-level+1 ; ++h ) { // scan through positions for this visit
        Bits P = pattern ;
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
    premadeMoves = (Bits *) malloc( iPremadeMoves*sizeof(Bits) ) ;
    if ( premadeMoves==NULL) {
        fprintf(stderr,"Memory exhausted move array\n");
        exit(1);
    }

    // recursive fill of premadeMoves
    premadeMovesRecurse( 0, 0, visits, Game_none );
}

#define QQ(x) "\"" #x "\"" 

void Json_out( void ) {
    // output settings and moves
    fprintf(jfile,"{");
        fprintf(jfile, QQ(length)":%d,\n", xlength);
        fprintf(jfile, QQ(width)":%d,\n",  ylength);
        fprintf(jfile, QQ(visits)":%d,\n", visits );
        fprintf(jfile, QQ(offset)":%s,\n", offset?"true":"false" );
        fprintf(jfile, QQ(geometry)":%s,\n", circle? QQ(circle) : QQ(grid) );
        if ( victoryDay >= 0 ) {
            fprintf(jfile, QQ(days)":%d,\n", victoryDay );
            if ( poison < 2 ) { // low poison, backtrace possible
                fprintf(jfile, QQ(moves)":[" ) ;
                    for ( int d=1; d<=victoryDay ; ++d ) {
                        fprintf(jfile,"[");
                        int v = 0 ;
                        for ( int b=0 ; b<holes ; ++b ) {
                            if ( getB(victoryMove[d],b) ) {
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
