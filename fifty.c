#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <time.h>

#define SIZE 50
#define SIZE2 (SIZE*2)
#define REP 1000

/* ---------------
Mersene Twister from https://github.com/ESultanik/mtwister
   --------------- */
/* An implementation of the MT19937 Algorithm for the Mersenne Twister
 * by Evan Sultanik.  Based upon the pseudocode in: M. Matsumoto and
 * T. Nishimura, "Mersenne Twister: A 623-dimensionally
 * equidistributed uniform pseudorandom number generator," ACM
 * Transactions on Modeling and Computer Simulation Vol. 8, No. 1,
 * January pp.3-30 1998.
 *
 * http://www.sultanik.com/Mersenne_twister
 */
#define UPPER_MASK		0x80000000
#define LOWER_MASK		0x7fffffff
#define TEMPERING_MASK_B	0x9d2c5680
#define TEMPERING_MASK_C	0xefc60000

#define STATE_VECTOR_LENGTH 624
#define STATE_VECTOR_M      397 /* changes to STATE_VECTOR_LENGTH also require changes to this */

typedef struct tagMTRand {
  unsigned long mt[STATE_VECTOR_LENGTH];
  int index;
} MTRand;

MTRand seedRand(unsigned long seed);
unsigned long genRandLong(MTRand* rand);
double genRand(MTRand* rand);


inline static void m_seedRand(MTRand* rand, unsigned long seed) {
  /* set initial seeds to mt[STATE_VECTOR_LENGTH] using the generator
   * from Line 25 of Table 1 in: Donald Knuth, "The Art of Computer
   * Programming," Vol. 2 (2nd Ed.) pp.102.
   */
  rand->mt[0] = seed & 0xffffffff;
  for(rand->index=1; rand->index<STATE_VECTOR_LENGTH; rand->index++) {
    rand->mt[rand->index] = (6069 * rand->mt[rand->index-1]) & 0xffffffff;
  }
}

/**
* Creates a new random number generator from a given seed.
*/
MTRand seedRand(unsigned long seed) {
  MTRand rand;
  m_seedRand(&rand, seed);
  return rand;
}

/**
 * Generates a pseudo-randomly generated long.
 */
unsigned long genRandLong(MTRand* rand) {

  unsigned long y;
  static unsigned long mag[2] = {0x0, 0x9908b0df}; /* mag[x] = x * 0x9908b0df for x = 0,1 */
  if(rand->index >= STATE_VECTOR_LENGTH || rand->index < 0) {
    /* generate STATE_VECTOR_LENGTH words at a time */
    int kk;
    if(rand->index >= STATE_VECTOR_LENGTH+1 || rand->index < 0) {
      m_seedRand(rand, 4357);
    }
    for(kk=0; kk<STATE_VECTOR_LENGTH-STATE_VECTOR_M; kk++) {
      y = (rand->mt[kk] & UPPER_MASK) | (rand->mt[kk+1] & LOWER_MASK);
      rand->mt[kk] = rand->mt[kk+STATE_VECTOR_M] ^ (y >> 1) ^ mag[y & 0x1];
    }
    for(; kk<STATE_VECTOR_LENGTH-1; kk++) {
      y = (rand->mt[kk] & UPPER_MASK) | (rand->mt[kk+1] & LOWER_MASK);
      rand->mt[kk] = rand->mt[kk+(STATE_VECTOR_M-STATE_VECTOR_LENGTH)] ^ (y >> 1) ^ mag[y & 0x1];
    }
    y = (rand->mt[STATE_VECTOR_LENGTH-1] & UPPER_MASK) | (rand->mt[0] & LOWER_MASK);
    rand->mt[STATE_VECTOR_LENGTH-1] = rand->mt[STATE_VECTOR_M-1] ^ (y >> 1) ^ mag[y & 0x1];
    rand->index = 0;
  }
  y = rand->mt[rand->index++];
  y ^= (y >> 11);
  y ^= (y << 7) & TEMPERING_MASK_B;
  y ^= (y << 15) & TEMPERING_MASK_C;
  y ^= (y >> 18);
  return y;
}

/* ---------------
Mersene Twister from https://github.com/ESultanik/mtwister
   --------------- */

MTRand mtrand ;
#define Seed do { mtrand = seedRand(time(NULL)) ; } while (0)
#define Rand_Int genRandLong( &mtrand )

#define STATE_VECTOR_LENGTH 624
#define STATE_VECTOR_M      397 /* changes to STATE_VECTOR_LENGTH also require changes to this */
#define UPPER_MASK		0x80000000
#define LOWER_MASK		0x7fffffff
#define TEMPERING_MASK_B	0x9d2c5680
#define TEMPERING_MASK_C	0xefc60000


int boxes[SIZE2] ;

void load_boxes( void ) {
	for ( int b = 0 ; b<SIZE2 ; ++b ) {
		boxes[b] = b ;
	}
}

void permute( void ) {
	for ( int b=SIZE2-1 ; b>0 ; --b ) {
		long r = Rand_Int % (b+1) ;
		int c = boxes[r] ;
		boxes[r] = boxes[b] ;
		boxes[b] = c ;
	}
}

void show_permute() {
	for ( int b; b<SIZE2 ; ++b ) {
		printf("%3d",boxes[b]);
	}
	printf("\n");
}

int test( void ) {
	for ( int p = 0 ; p < SIZE2 ; ++p ) { // prisoners (use 0 index)
		int nf = 1 ; // not found ;
		for ( int b=0 ; b< SIZE ; ++b ) {
			if ( boxes[(p+b) % SIZE2] == p ) {
				nf = 0 ; 
				break ;
			}
		}
		if ( nf ) {
			printf("%3d",p);
			return 0 ; // not found
		}
	}
	return 1 ;
}

void main( void ) {
	int successes = 0 ; // count of successful runs
	load_boxes() ; // set up boxes array
	Seed ;
	for ( int t=0 ; t<REP ; ++t ) {
		permute();
		//show_permute();
		if ( test() ) {
			printf("Success\n");
			++successes ;
		}
	}
	printf( "%d Rate = %g\n", successes, (0.0+successes)/REP ) ;
}
	
			
			
	
