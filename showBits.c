// Part of foxhole solver
// {c} 2022 Paul H Alfille
// MIT License
// See https://github.com/alfille/foxholes

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
                    printf( (x<=y) && getB( cc, T(x,y) ) ? "X|":" |" );
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

