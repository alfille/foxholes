// Part of foxhole solver
// {c} 2022 Paul H Alfille
// MIT License
// See https://github.com/alfille/foxholes

void getOpts( int argc, char ** argv ) {
    // Parse Arguments
    int c ;
    opterr = 0 ; // suppress option error display (to allow optional arguments)
    while ( (c = getopt( argc, argv, "468cCtTgGuUhHl:L:w:W:p:P:v:V:j:J:" )) != -1 ) {
        switch ( c ) {
        case 'h':
            help(argv[0]) ;
            break ;
        case 'l':
        case 'L':
            // xlength
            xlength = atoi(optarg);
            break ;
        case 'w':
        case 'W':
            // ylength (width)
            ylength = atoi(optarg);
            break ;
        case 'p':
        case 'P':
        // poison days
            poison = atoi(optarg);
            break ;
        case 'v':
        case 'V':
            visits = atoi(optarg);
            break ;
        case '4':
            connection = Rectangular ;
            break ;
        case '6':
            connection = Hexagonal ;
            break ;
        case '8':
            connection = Octagonal ;
            break ;
        case 'c':
        case 'C':
            geo = Circle ;
            break;
        case 'g':
        case 'G':
            geo = Grid ;
            break ;
        case 't':
        case 'T':
            geo = Triangle ;
            break ;
        case 'u':
        case 'U':
            update = True ;
            break ;
        case 'j':
        case 'J':
            json = True ;
            jfile = fopen(optarg,"w") ;
            if ( jfile == NULL ) {
                printf("Cannot open JSON file %s\n",optarg);
                exit(1);
            }
            jsonfile = True ;
            break ;
        case '?': // missing argument
            switch (optopt) {
                case 'j':
                case 'J':
                    json = True ;
                    jfile = stdout ;
                    jsonfile = False ;
                    break ;
                default:
                    break ;
            }
            break ;
        default:
            help(argv[0]);
            break ;
        }
    }

    switch ( validate() ) {
        case Val_ok:
            break ;
        case Val_fix:
            fprintf(stderr,"Continue with corrected values\n");
            break ;
        case Val_fail:
            fprintf(stderr,"Invalid parameters\n");
            exit(1) ;
    }
}
