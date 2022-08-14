#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <malloc.h>
#include <unistd.h>

/*
 * Foxhole solver for <= 32 holes
 * uses bitmaps
 *
 * c 2022 Paul H Alfille
 * MIT license
 * */

typedef uint32_t Bits;

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
int maxday = 1000000;
int searchCount = 0 ;

#define maxbits 32

// 2^^maxbits_pos = maxbits
#define maxbits_pos 5

#define maxbits_mask (maxbits - 1)

#define MAXHOLES maxbits

//single word

#define setB( map, b ) map |= (1 << b)
#define clearB( map, b ) map &= ~(1 << b)
#define getB( map, b ) ((map & (1 << b)) ? 1:0)

// Long array
void setBit( Bits * map, unsigned int b ) {
    map[ b>>maxbits_pos ] |= (1 << (b&maxbits_mask)) ;
}

void clearBit( Bits * map, unsigned int b ) {
    map[ b>>maxbits_pos ] &= ~(1 << (b&maxbits_mask)) ;
}

int getBit( Bits * map, unsigned int b ) {
    return (map[ b>>maxbits_pos ] & (1 << (b&maxbits_mask))) ? 1:0 ;
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
    Bits power2 = 0 ; // to figure number of possible games 
    int i ;

    setB( power2, holes ) ; // 2^^holes
    Gamespace = ( Bits *) malloc( power2 * sizeof(Bits) ) ;
    if ( Gamespace == NULL ) {
        fprintf( stderr, "Memory exhausted -- games\n" );
        exit(1);
    }
    for ( i=0 ; i<power2 ; ++i ) {
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
    int x,y; // indexes
    Move i ;
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
    for ( y=0 ; y<ylength ; ++y ) { // vertical
        for ( x=0 ; x<xlength ; ++x ) { // horizontal
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
    int x,y;
    for ( y=0 ; y<ylength ; ++y ) {
        for ( x=0 ; x<xlength ; ++x ) {
            printf( getB( bb, I(x,y) ) ? "X|":" |" ) ;
        }
        printf("\n");
    }
}
    
typedef enum {
    won, // all foxes caught
    lost, // no more moves
    forward, // go to next day
    retry, // try another move for this day
    backward, // go back a day
} searchState ;

struct movestate {
    int allocated ;
    Bits * games ;
    Move * list ;
} moveState ;

void enlargeMovestate(void) {
    moveState.list = (Move *) realloc( moveState.list, 1000*visits*sizeof(Move) ) ;
    if ( moveState.list == NULL ) {
        fprintf( stderr, "Memory exhausted -- moves\n" );
        exit(1);
    }
    moveState.games = (Bits *) realloc( moveState.games, 1000*sizeof(Bits) ) ;
    if ( moveState.games == NULL ) {
        fprintf( stderr, "Memory exhausted -- gamelist\n" );
        exit(1);
    }
    moveState.allocated += 1000 ;
}

void makeMovestate(void) {
    int h;

    moveState.list = NULL ;
    moveState.games = NULL ;
    moveState.allocated = 0 ;
    enlargeMovestate() ;

    // initial game position (all foxes)
    for ( h=0 ; h<holes ; ++h ) {
        setB( moveState.games[0], h ) ;
    }
    setBit( Gamespace, moveState.games[0] ) ;
}

searchState calcMove( int day ) {
    Bits thisGame = moveState.games[day] ; // current fox locations
    Bits nextGame = 0 ; // assume no foxes

    int i;
    int liststart = day*visits;
    int listend = liststart + visits;

    int poison_start = listend - (poison)*visits;
    if (poison_start<0) {
        poison_start = 0 ;
    }

    if ( update ) {
        ++searchCount ;
        if ( (searchCount & 0xFFFFFF) == 0 ) {
            printf(".\n");
        }
    }

    // clear moves
    for ( i=liststart ; i<listend ; ++i ) {
        clearB( thisGame, moveState.list[i] ) ;
    }
    // calculate where they jump to
    for ( i=0 ; i<holes ; ++i ) {
        if ( getB( thisGame, i ) ) { // fox currently
            nextGame |= Jumpspace[i] ; // jumps to here
        }
    }
    // clear poisoned holes
    for ( i=poison_start ; i<listend ; ++i ) {
        clearB( nextGame, moveState.list[i] ) ;
    }

    // Victory? (No foxes)
    if ( nextGame==0 ) {
        int j,d;
        printf("Victory on day %d. Moves:\n",day);
        for ( d=0 ; d<=day ; ++d ) {
            printf("\n");
            showBits( moveState.games[d]);
            for( j = 0 ; j < visits ; ++j ) {
                printf("%d ",moveState.list[d*visits+j]);
            }                
            printf("\n");
        }
        return won ;
    }

    // Already seen?
    if ( getBit( Gamespace, nextGame ) == 1 ) {
        // game configuration already seen
        return retry ; // means try another move
    }
    
    // Valid new game, continue further
    moveState.games[day+1] = nextGame;
    setBit( Gamespace, nextGame ) ; // flag this configuration
    return forward ;
}

searchState searchDay( int day ) {
    int v ;
    Move * move = & moveState.list[day*visits] ;
    
    if ( day > maxday ) {
        return backward;
    }

    // next day
    if ( moveState.allocated - day < 5 ) {
        enlargeMovestate() ;
    }
    

    // set up initial move sequence
    for ( v=0 ; v<visits ; ++v ) {
        move[v]=v ;
    }
    do {
        switch( calcMove(day) ) {
            case won:
                return won;
                break ;
            case retry:
                break ;
            case forward:
                switch ( searchDay(day+1) ) {
                    case won:
                        return won;
                        break ;
                    case backward:
                        break ;
                }
                break ;
        }
        // increment move starting from last position
        for ( v=visits-1 ; v>-1 ; --v ) {
            ++move[v] ;
            if ( move[v] < holes + visits - v -1 ) {
                // good value, place remaining moves immediately after
                int vv ;
                for ( vv=v+1 ; vv < holes ; ++vv ) {
                    move[vv] = move[vv-1]+1 ;
                }
                break ;                
            }
            if ( v==0 ) {
                return backward ;
            }
        }
    } while(1) ;
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
    makeJumpspace();
    if ( update ) {
        printf("Setting up game array\n");
    }
    makeGamespace();
    if ( update ) {
        printf("Starting search\n");
    }
    makeMovestate();

    switch ( searchDay(0) ) {
        case backward:
            printf("No good moves\n");
            break ;
        case won:
            printf("Victory!\n");
            break ;
    }
}
