// Part of foxhole solver
// {c} 2022 Paul H Alfille
// MIT License
// See https://github.com/alfille/foxholes

size_t binomial( int N, int M ) {
    uint64_t top = 1;
    uint64_t bot = 1;
    int i = M ;

    if ( i > N/2 ) { // use shorter length
        i = N-M ;
    }

    for ( ; i>0 ; --i ) {
        top *= N+1-i;
        bot *= i;
    }
    return top/bot;
}

int premadeMovesRecurse( Bits_t * Moves, int index, int start_hole, int left, Bits_t pattern ) {
    // index into premadeMoves (list of moves)
    // start_hole hole to start_hole current left with
    // left visit number (visits down to 1)
    // pattern bitmap pattern to this point

    // 0 index is no move
    Moves[index]=0;
    ++index ;

    // generate moves:
    for ( int h=start_hole ; h<holes-left+1 ; ++h ) { // scan through positions for this visit
        Bits_t P = pattern ;
        setB( P, h );
        if ( left==1 ) { // last visit, put in array and increment index
            Moves[index] = P;
            ++index;
        } else { // recurse into futher visits
            index = premadeMovesRecurse( Moves, index, h+1, left-1, P );
        }
    }
    return index;
}
