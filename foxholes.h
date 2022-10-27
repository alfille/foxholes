// Part of foxhole solver
// {c} 2022 Paul H Alfille
// MIT License
// See https://github.com/alfille/foxholes

#ifndef FOXHOLES_H
#define FOXHOLES_H

Bits_t Game_none = 0; // no foxes
Bits_t Game_all ;

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

// Jumps -- macros for convenient definitions

// For moves -- go from x,y to index
// index from x,y
#define I(xx,yy) ( (xx) + (yy)*xlength )
// index from x,y but wrap x if past end (circle)
#define W(xx,yy) ( I( ((xx)+xlength)%xlength, (yy) ) )
// Index into Triangle
#define T(xx,yy) ( (yy)*((yy)+1)/2+(xx) )

// For recursion
typedef enum {
    won, // all foxes caught
    lost, // no more moves
    forward, // go to next day
    backward, // no choices here
    overflow, // too many days
    retry, // try another move for this day
} Searchstate_t ;

#endif /* FOXHOLES_H */
