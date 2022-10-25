// Part of foxhole solver
// {c} 2022 Paul H Alfille
// MIT License
// See https://github.com/alfille/foxholes

void jumpHolesCreate( Bits_t * J ) {
    // <J> is starting location
    // uses <total> locations 

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
