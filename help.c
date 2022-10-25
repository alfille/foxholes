// Part of foxhole solver
// {c} 2022 Paul H Alfille
// MIT License
// See https://github.com/alfille/foxholes


void help( char * progname ) {
    printf("%s -- solve the foxhole puzzle\n",progname) ;
    printf("\tsee https://github.com/alfille/foxholes\n") ;
    printf("\n") ;
    printf("By Paul H Alfille 2022 -- MIT license\n") ;
    printf("\n") ;
    printf("\t-l 5\tlength (3 to 32 default 5)\n") ;
    printf("\t-w 1\twidth (1 to 10 but max holes 32)") ;
    printf("\n") ;
    printf("\t-c\tfoxholes in a Circle\n") ;
    printf("\t-g\tfoxholes in a Grid\n") ;
    printf("\t-t\tfoxholes in a Triangle\n") ;
    printf("\n") ;
    printf("\t-4\tfoxholes connected Rectangularly\n") ;
    printf("\t-6\tfoxholes connected Hexagonally\n") ;
    printf("\t-8\tfoxholes connected Octagonally\n") ;
    printf("\n") ;
    printf("\t-v 1\tholes Visited per day\n") ;
    printf("\t-p 0\tdays visited holes are Poisoned") ;
    printf("\n") ;
    printf("\t-j \t JSONrepresentation of game and solution\n") ;
    printf("\t-j filename \twrite JSON to file") ;
    printf("\n") ;
    printf("\t-u\tperiodic Updates\n") ;
    printf("\t-h\thelp\n") ;
    exit(0) ;
}

