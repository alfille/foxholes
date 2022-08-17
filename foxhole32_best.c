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
Bits Game_none = 0; // no foxes

Bits * Possible = NULL ;
int iPossible ;

typedef int Move ;

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

#define maxbits 32

// 2^^maxbits_pos = maxbits
#define maxbits_pos 5

#define maxbits_mask (maxbits - 1)

#define MAXHOLES maxbits

//single word

#define setB( map, b ) map |= (1 << (b))
#define clearB( map, b ) map &= ~(1 << (b))
#define getB( map, b ) (((map) & (1 << (b))) ? 1:0)

// Long array
void setBit( Bits * map, unsigned int b ) {
    setB( (map[ (b)>>maxbits_pos ]), (b&maxbits_mask) ) ;
}

void clearBit( Bits * map, unsigned int b ) {
    clearB( (map[ (b)>>maxbits_pos ]), (b&maxbits_mask) ) ;
}

int getBit( Bits * map, unsigned int b ) {
    return getB( (map[ (b)>>maxbits_pos ]), (b&maxbits_mask) ) ;
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

Bits * Gamespace = NULL; // bitmap of all possible game states
    // indexed by foxes as bits to make a number

void makeGamespace() {
    uint64_t gamepages = 1 ; // to figure number of possible games 
    // 2^^holes games
    // 1 bit / game
    // 32 bits in each "page"

	for ( int h=maxbits_pos; h<holes ; ++h ) {
		gamepages *= 2;
	}
	printf("gamepages = %ld\n",gamepages);

    Gamespace = ( Bits *) malloc( gamepages * sizeof(Bits) ) ;
    if ( Gamespace == NULL ) {
        fprintf( stderr, "Memory exhausted -- games\n" );
        exit(1);
    }
    for ( int i=0 ; i<gamepages ; ++i ) {
        Gamespace[i] = 0 ;
    }
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
            printf( getB( bb, I(x,y) ) ? "X|":" |" )     Day = 0 ;
;
        }
        printf("\n");
    }
}

#define gamestate_space 1000000
Bits Tries[gamestate_space]; // fixed size array holding intermediate game states

int oldNum;
int newIndex;
int maxIndex;
int Day ;

typedef enum {
    won, // all foxes caught
    lost, // no more moves
    forward, // go to next day
    retry, // try another move for this day
    backward, // go back a day
} searchState ;

searchState calcMove( Bits move, int iold, Bits * new_game ) {
    struct Intermediate {
        Bits game ;
        Bits moves[ poison>1 ? poison-1 : 0 ] ;
    } ;

    typedef struct Intermediate * pInter ;
    pInter T = (pInter) Tries ;

    Bits thisGame = T[iold].game ; // current fox locations

    if ( update ) {
        ++searchCount ;
        if ( (searchCount & 0xFFFFFF) == 0 ) {
            printf(".\n");
        }
    }

    // clear moves
    thisGame &= ~move ;

    // calculate where they jump to
    *new_game = Game_none ;
    for ( int i=0 ; i<holes ; ++i ) {
        if ( getB( thisGame, i ) ) { // fox currently
             *new_game|= Jumpspace[i] ; // jumps to here
        }
    }

    // do poisoning
    if ( poison > 1 ) { // any?
        *new_game &= ~move ; // today
        for ( int p = 1 ; p<poison ; ++p ) { // maybe past days?
            *new_game &= ~T[iold].moves[p-1] ;
        }
    }

/*
    printf("Day %d:\n",Day);
    printf("\tGame coming in:\n");
    showBits(game);
    printf("\tMoves:\n");
    showBits(move);
//    printf("\tGame minus moves:\n");
//    showBits(thisGame);
    printf("\tResult:\n");
    showBits(*new_game);
*/

    // Victory? (No foxes)
    if ( *new_game==Game_none ) {
        return won ;
    }

    // Already seen?
    if ( getBit( Gamespace, *new_game ) == 1 ) {
        // game configuration already seen
        return retry ; // means try another move
    }
    
    // Valid new game, continue further
    setBit( Gamespace, *new_game ) ; // flag this configuration
    return forward ;
}

searchState dayPass( void ) {
    struct Intermediate {
        Bits game ;
        Bits moves[ poison>1 ? poison-1 : 0 ] ;
    } ;

    typedef struct Intermediate * pInter ;
    pInter T = (pInter) Tries ;

    // T array has <old> then <new>
    // but first need to move new into old
    memmove( &T[0] , &T[oldNum] , sizeof( struct Intermediate ) * ( newIndex - oldNum ) ) ;
    oldNum = newIndex - oldNum ;
    newIndex = oldNum ; 

    ++Day;
    if ( Day > maxday ) {
        printf( "Exceeded %d days.\n",maxday ) ;
        return lost ;
    }
    
    for ( int iold=0 ; iold<oldNum ; ++iold ) { // each possible position
        for ( int ip=0 ; ip<iPossible ; ++ip ) { // each possible move
            Bits newT ;
            switch( calcMove( Possible[ip], iold, &newT ) ) {
                case won:
                    return won;
                case forward:
                    // Add this game state to the furture evaluation list
                    T[newIndex].game = newT ;
                    if ( poison > 1 ) { // update poison list
                        for ( int p=poison-2 ; p>0 ; --p ) {
                            T[newIndex].moves[p] = T[iold].moves[p-1] ;
                        }
                        T[newIndex].moves[0] = Possible[ip] ;
                    }
                    newIndex++ ;
                    if ( newIndex > maxIndex ) {
                        printf("Too large a solution space\n");
                        return lost ;
                    }
                    break ;
                default:
                    break ;
            }
        }
    }
    return newIndex>0 ? forward : lost ;
}
    
searchState startDays( void ) {
    // Set up arrays of game states
    //  will evaluate all states for each day for all moves and add to next day if unique

    struct Intermediate {
        Bits game ;
        Bits moves[ poison>1 ? poison-1 : 0 ] ;
    } ;

    newIndex = 0 ;
    oldNum = 0 ;
    maxIndex = gamestate_space / sizeof( struct Intermediate ) ;
    typedef struct Intermediate * pInter ;
    pInter T = (pInter) Tries ;

	// initial game position
    Day = 0 ;
    T[newIndex].game = Game_none ;
    for ( int h=0 ; h<holes ; ++h ) {
		setB( T[newIndex].game, h ); // all holes have foxes
    }
	// initially no poison history of course
    for ( int p=0 ; p<poison-1 ; ++p ) {
        T[newIndex].moves[p] = 0 ; // no prior moves
    }
    setBit( Gamespace, T[newIndex].game ) ; // flag this configuration
    ++newIndex ;

	// Now loop through days
    do {
        printf("Day %d, States: %d, Moves %d\n",Day+1,newIndex,iPossible);
        switch ( dayPass() ) {
            case won:
                printf("Victory in %d days!\n",Day ) ; // 0 index
                return won ;
            case lost:
                printf("No solution despite %d days\n",Day );
                return lost ;
            default:
                break ;
        }
    } while(1) ;
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
            offset = 1 ;
            break ;
        case 'c':
        case 'C':
            circle = 1 ;
            break;
        case 'g':
        case 'G':
            circle = 0;
            break ;
        case 'u':
        case 'U':
            update = 1 ;
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

    printStatus();    

    if ( update ) {
        printf("Setting up moves\n");
    }
    makeJumpspace(); // array of fox jump locations

    if ( update ) {
        printf("Setting up game array\n");
    }
    makeGamespace(); // bitmap of game layouts (to avoid revisiting)

    if ( update ) {
        printf("Starting search\n");
    }
    makePossible(); // array of moves

    startDays() ; // start searching through days until a solution is found (will be fastest by definition)
}
