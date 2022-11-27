// Part of foxhole solver
// {c} 2022 Paul H Alfille
// MIT License
// See https://github.com/alfille/foxholes

#ifndef VICTORY
#define VICTORY

// Arrays holding winning moves and game positions
int victoryDay = -1;  // >=0 for success
Bits_t victoryGame[MaxDays+1];
Bits_t victoryMovePlus[MaxPoison+MaxDays+1];
Bits_t * victoryMove = victoryMovePlus + MaxPoison ; // allows prior space for poisons

#endif // VICTORY

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

void loadFromVictory( int Day, GM_t * move ) {
    move[0] = victoryGame[Day] ;
    for ( int p = 1 ; p < poison_plus ; ++p ) {
        move[p] = victoryMove[ Day + 1 - p ] ;
    }
    //printf("Load from victory day %d\n",Day);
    //showDoubleBits( move[1],move[0]);
}

void loadToVictory( int Day, GM_t * move ) {
    victoryGame[ Day ] = move[0] ;
    for ( int p = 1 ; p < poison_plus ; ++p ) {
        victoryMove[ Day + 1 - p ] = move[p] ;
    }
    //printf("Load to victory day %d\n",Day);
    //showDoubleBits( move[1],move[0]);
}

void loadToVictoryPlus( int Day, GMM_t * move ) {
    victoryGame[ Day ] = move[0] ;
    for ( int p = 1 ; p <= poison_plus ; ++p ) {
        victoryMove[ Day + 1 - p ] = move[p] ;
    }
    //printf("Load to victory day %d\n",Day);
    //showDoubleBits( move[1],move[0]);
}

