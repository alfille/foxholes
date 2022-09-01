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

Bits * Possible = NULL ;
int iPossible ;

typedef int Move ;

#define True 1
#define False 0

// Globals
int xlength = 5;
int ylength = 1;
int holes;
int poison = 0;
int visits = 1;
int update = 0 ;
int offset = 0 ;
int circle = 0 ;
int maxday = 1000000 ;
int searchCount = 0 ;

#define MAXHOLES 32
#define MAXPOISON 4

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


void makeGamesMap() {
    memset( GamesMap, 0, sizeof( GamesMap ) ) ;
}

// For moves -- go from x,y to index
// index from x,y
#define I(xx,yy) ( (xx) + (yy)*xlength )
// index from x,y but wrap x if past end (circle)
#define W(xx,yy) ( I( ((xx)+xlength)%xlength, (yy) ) )

Bits * Jumpspace = NULL ; // bitmap for moves from a hold indexed by that hole

void makeJumpspace() {
    // make a bitmap of move-to location for each current location

    // make space for the array 
    Jumpspace = (Bits *) malloc( holes * sizeof(Bits) ); // small

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
            Bits * J = &Jumpspace[x + xlength*y] ; // bitmap to be constructed
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

#define gamestate_length 0x100000
#define INC(x) ( ((x)+1) % gamestate_length )
#define DEC(x) ( ((x)+gamestate_length-1) % gamestate_length )
#define DIFF(x,y) ( ( gamestate_length+(x)-(y) ) % gamestate_length )
#define MaxDays 300
struct tryStruct {
    Bits game ;
    Bits refer ;
} ;
struct tryStruct Tries[gamestate_length]; // fixed size array holding intermediate game states
struct tryStruct Halves[gamestate_length]; // fixed size array holding intermediate game states
void * TP = NULL ;
void * HP = NULL ;
Bits WinState[MaxDays+1];
Bits WinMove[MaxDays+1];

#define backLook_length (MaxDays+1)
struct {
    int addDay ; // next day to add to backtrace
    int increment ; // increment for addDay
    int start ; // start index for entries list
    int next ; // next index for entries list
    struct {
        int start; // start in Halves
        int day; // corresponding day
    } entries[backLook_length] ; // cirular entries buffer
} backSolve ;

#define backINC(x) ( ((x)+1) % backLook_length )
#define backDEC(x) ( ((x)-1+backLook_length) % backLook_length )
void makeBack( void ) {
    backSolve.start = 0 ;
    backSolve.next = 0 ;
    backSolve.entries[0].start = 0 ;
    backSolve.addDay = 1 ;
    backSolve.increment = 1 ;
}

int victoryDay = 0 ;
int newStart ;
int newNext ;

typedef enum {
    won, // all foxes caught
    lost, // no more moves
    forward, // go to next day
    retry, // try another move for this day
} searchState ;

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
             *new_game|= Jumpspace[j] ; // jumps to here
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

void backTrace( int generation, int refer ) {
    while ( refer != Game_all ) {
        printf("backtrace generation=%d day=%d refer=%d\n",generation,backSolve.entries[generation].day,refer) ;
        WinState[backSolve.entries[generation].day] = Halves[refer].game ;
        refer = Halves[refer].refer ;
        if ( generation == backSolve.start ) {
            break ;
        }
        generation = backDEC(generation) ;
    }
}

searchState dayPass( int Day, Bits target ) {
    typedef struct {
        Bits move[poison-1] ; 
    } Pstruct ;
    Pstruct * TryPoison = TP ;
    Pstruct * HalhPoison = HP ;
    
    Bits move[MAXPOISON] ; // for poisoning

    int iold = newStart ; // need to define before loop
    newStart = newNext ;
        
    for (  ; iold!=newStart ; iold=INC(iold) ) { // each possible position
        for ( int ip=0 ; ip<iPossible ; ++ip ) { // each possible move
            Bits newT ;
            move[0] = Possible[ip] ; // actual move (and poisoning)
            for ( int p=1 ; p<poison ; ++p ) {
                move[p] = TryPoison[iold].move[p-1] ;
            }
                    
            switch( calcMove( move, Tries[iold].game, &newT, target ) ) {
                case won:
                    WinState[Day] = target ;
                    WinMove[Day] = move[0] ;
                    WinState[Day-1] = Tries[iold].game ;
                    if ( target == Game_none ) {
                        // real end of game
                        victoryDay = Day ;
                    }
                    backTrace( backDEC(backSolve.next), Tries[iold].refer ) ;
                    return won;
                case forward:
                    // Add this game state to the furture evaluation list
                    Tries[newNext].game = newT ;
                    Tries[newNext].refer = Tries[iold].refer ;
                    if ( poison > 1 ) { // update poison list
                        for ( int p=poison-1 ; p>0 ; --p ) {
                            TryPoison[newNext].move[p] = move[p-1] ;
                        }
                    }
                    newNext = INC(newNext) ;
                    if ( newNext == iold ) { // head touches tail
                        printf("Too large a solution space\n");
                        return lost ;
                    }
                    break ;
                default:
                    break ;
            }
        }
    }
    return newNext != newStart ? forward : lost ;
}

void addBack( int Day ) {
    typedef struct {
        Bits move[poison-1] ; 
    } Pstruct ;
    Pstruct * TryPoison = TP ;
    Pstruct * HalfPoison = HP ;
    
    printf("Back add Day=%d Start=%d Next = %d\n",Day,backSolve.start,backSolve.next);

    // back room for new data
    int insertLength = DIFF(newNext,newStart) ;
    if ( backSolve.start != backSolve.next ) {
        while ( DIFF(backSolve.entries[backSolve.start].start,backSolve.entries[backSolve.next].start) < insertLength ) {
            backSolve.start = backINC(backSolve.start) ;
            ++backSolve.increment ;
        }
    }

    // move current state to "Back" array"
    backSolve.entries[backSolve.next].day = Day ;
    int index = backSolve.entries[backSolve.next].start ;
    for ( int inew=newStart ; inew != newNext ; inew = INC(inew), index=INC(index) ) {
        Halves[index].game  = Tries[inew].game ;
        Halves[index].refer = Tries[inew].refer ;
        Tries[inew].refer = index ;
        for ( int p=1 ; p<poison ; ++p ) {
            HalfPoison[index].move[p] = TryPoison[inew].move[p] ;
        }
    }
    backSolve.next = backINC(backSolve.next) ;
    backSolve.entries[backSolve.next].start = index ;
    backSolve.addDay += backSolve.increment ;
}
    
    
searchState startDays( void ) {
    // Set up arrays of game states
    //  will evaluate all states for each day for all moves and add to next day if unique

    newNext = 0 ;
    newStart = newNext ;
    
    typedef struct {
        Bits move[poison-1] ; 
    } Pstruct ;
    Pstruct * TryPoison = TP ;
    Pstruct * HalfPoison = HP ;
    if ( poison > 1 ) {
        TP = malloc( gamestate_length * sizeof(Pstruct) ) ;
        HP = malloc( gamestate_length * sizeof(Pstruct) ) ;
        TryPoison = TP ;
        HalfPoison = HP ;
        // initially no poison history of course
        for ( int p=1 ; p<poison ; ++p ) {
            TryPoison[newStart].move[p-1] = 0 ; // no prior moves
        }
    }

    // create an all-fox state
    Game_all = 0 ; // clear bits
    for ( int h=0 ; h<holes ; ++h ) { // set all holes bits
        setB( Game_all, h ); // all holes have foxes
    }
    
    // Set winning path to unknown
    for ( int d=0 ; d<MaxDays ; ++d ) {
        WinState[d] = Game_all ;
        WinMove[d] = Game_none ;
    }
    
    // set solver state
    makeBack() ;
        
    // set Initial position
    Tries[newNext].game = Game_all ;
    Tries[newNext].refer = Game_all ;
    setB64( Tries[newNext].game ) ; // flag this configuration
    ++newNext ;
    

    // Now loop through days
    for ( int Day=1 ; Day < maxday ; ++Day ) {
        printf("Day %d, States: %d, Moves %d\n",Day+1,DIFF(newNext,newStart),iPossible);
        switch ( dayPass(Day,Game_none) ) {
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
        if ( Day == backSolve.addDay ) {
            addBack(Day) ;
        }
    }
    printf( "Exceeded %d days.\n",maxday ) ;
    return lost ;
}
searchState restartDays( int day, Bits current, Bits target ) {
    // Set up arrays of game states
    //  will evaluate all states for each day for all moves and add to next day if unique

    newNext = 0 ;
    newStart = newNext ;
    
    typedef struct {
        Bits move[poison-1] ; 
    } Pstruct ;
    Pstruct * TryPoison = TP ;
    Pstruct * HalfPoison = HP ;

    if ( poison > 1 ) {
        // initially no poison history of course
        for ( int p=1 ; p<poison ; ++p ) {
            TryPoison[newStart].move[p-1] = 0 ; // no prior moves
        }
    }

    
    // set solver state
    makeBack() ;
    backSolve.addDay = day+1 ;

    // set Initial position
    Tries[newNext].game = current ;
    Tries[newNext].refer = Game_all ;
    setB64( Tries[newNext].game ) ; // flag this configuration
    ++newNext ;
    
    // Now loop through days
    for ( int Day=day ; Day<=victoryDay ; ++Day ) {
        printf("Day %d, States: %d, Moves %d\n",Day+1,DIFF(newNext,newStart),iPossible);
        switch ( dayPass( Day, target ) ) {
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
        if ( Day == backSolve.addDay ) {
            addBack(Day) ;
        }
    }
}

void fixupTrace( void ) {
    while (True) {
        // any unsolved?
        int all_solved = True ;
        for ( int d=1 ; d<=victoryDay ; ++d ) {
            if ( WinState[d] == Game_all ) {
                printf("testing %d\n",d);
                showBits(WinState[d]);
                all_solved = False ;
                break ;
            }
        }
        if ( all_solved ) {
            return ;
        }
        // start search
        makeGamesMap() ; // clear bits
        int lastDay = 0 ;
        int gap = False ;
        for ( int d=1 ; d<victoryDay ; ++d ) { // go through days
            if ( WinState[d] == Game_all ) {
                // unknown solution
                gap = True ;
            } else {
                // known solution
                if ( gap ) {
                    // solution after unknown gap
                    restartDays( lastDay, WinState[lastDay], WinState[d] );
                }
                gap = False ;
                lastDay = d ;
            }
        }
    }
}

void fixupMoves( void ) {
    Bits move[poison] ; 
    for ( int p=0 ; p<poison ; ++p ) {
        move[p] = Game_none ;
    }
    
    for ( int Day=1 ; Day < victoryDay ; ++Day ) {
        printf("FixupMoves Day=%d\n",Day);
        // fill poison
        for ( int p=1 ; p<poison ; ++p ) {
            move[p] = move[p-1] ;
        }
        move[0] = WinMove[Day] ;

        // solve unknown moves
        if ( WinMove[Day] == Game_none ) {
            printf("FixupMoves fix Day=%d\n",Day);
            printf("\tfrom\n");
            showBits(WinState[Day-1]);
            printf("\tto\n");
            showBits(WinState[Day]);
            for ( int ip=0 ; ip<iPossible ; ++ip ) { // each possible move
                Bits newGame = Game_none ;
                Bits thisGame = WinState[Day-1] ;
                move[0] = Possible[ip] ; // actual move (and poisoning)

                // clear moves
                thisGame &= ~move[0] ;

                // calculate where they jump to
                for ( int j=0 ; j<holes ; ++j ) {
                    if ( getB( thisGame, j ) ) { // fox currently
                         newGame |= Jumpspace[j] ; // jumps to here
                    }
                }

                // do poisoning
                for ( int p=0 ; p<poison ; ++p ) {
                    newGame &= ~move[p] ;
                }

                // Match target?
                if ( newGame == WinState[Day] ) {
                    printf("FixupMoves found Day=%d\n",Day);
                    WinMove[Day] = move[0] ;
                    break ;
                }
            }
        }
    }
}
        
int setPscan( int index, int start, int level, Bits pattern ) {
    // index into Possible (list of moves)
    // start hole to start current level with
    // level visit number (visits down to 1)
    // pattern bitmap pattern to this point
    for ( int h=start ; h<holes-level+1 ; ++h ) { // scan through positions for this visit
        Bits P = pattern ;
        setB( P, h );
        if ( level==1 ) { // last visit, put in array and increment index
            Possible[index] = P;
            ++index;
        } else { // recurse into futher visits
            index = setPscan( index, h+1, level-1, P );
        }
    }
    return index;
}

Bits * makePossible( void ) {
    // Create bitmaps of all possible moves (permutations of visits bits in holes slots)
    uint64_t top,bot;
    int v = visits ;

    // calculate iPossible (Binomial coefficient holes,visits)
    if (v>holes/2) {
        v = holes-visits ;
    }
    top = 1;
    bot = 1;
    for ( int i=v ; i>0 ; --i ) {
        top *= holes+1-i ;
        bot *= i ;
    }
    iPossible = top / bot ;
    Possible = (Bits *) malloc( iPossible*sizeof(Bits) ) ;
    if ( Possible==NULL) {
        fprintf(stderr,"Memory exhausted move array\n");
        exit(1);
    }

    // recursive fill of Possible
    setPscan( 0, 0, visits, Game_none );
}
    

int main( int argc, char **argv )
{
    // Arguments
    int c;
    while ( (c = getopt( argc, argv, "ocguhl:L:w:W:p:P:v:V:m:M:" )) != -1 ) {
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
        default:
            help();
            break ;
        }
    }


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

    printStatus();    
    
    for ( int h=0 ; h<holes ; ++h ) {
        setB( Game_all, h ) ;
    }

    if ( update ) {
        printf("Setting up moves\n");
    }
    makeJumpspace(); // array of fox jump locations

    if ( update ) {
        printf("Setting up game array\n");
    }
    makeGamesMap(); // bitmap of game layouts (to avoid revisiting)

    if ( update ) {
        printf("Starting search\n");
    }
    makePossible(); // array of moves

    switch (startDays()) { // start searching through days until a solution is found (will be fastest by definition)
        case won:
            fixupTrace() ;
            fixupMoves() ;
            break ;
        default:
            break ;
    
    }

    for ( int d = 0 ; d < victoryDay+1 ; ++d ) {
        printf("Day %d\n",d);
        showBits( WinState[d] ) ;
    }
    for ( int d = 0 ; d < victoryDay+1 ; ++d ) {
        printf("Move %d\n",d);
        showBits( WinMove[d] ) ;
    }
}
