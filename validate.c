// Part of foxhole solver
// {c} 2022 Paul H Alfille
// MIT License
// See https://github.com/alfille/foxholes

Validation_t validate( void ) {
    Validation_t v = Val_ok ; // default

    // length
    if ( xlength < 3 ) {
        fprintf(stderr,"Bad length will set to 5\n");
        xlength = 5;
        v = Val_fix ;
    }
    if ( xlength > MaxHoles ) {
        fprintf(stderr,"Bad length will set to %d\n",MaxHoles);
        xlength = MaxHoles;
        v = Val_fix ;
    }

    // width
    if ( ylength < 1 ) {
        fprintf(stderr,"Bad width will set to 1\n");
        ylength = 1;
        v = Val_fix ;
    }
    if ( ylength > MaxHoles/3 ) {
        fprintf(stderr,"Bad width will set to %d\n",MaxHoles/3);
        ylength = MaxHoles/3 ;
        v = Val_fix ;
    }

    // poison
    if (poison<0) {
        fprintf(stderr,"Bad poisoning will set to 0 days\n");
        poison = 0;
        v = Val_fix ;
    }
    if (poison>MaxPoison) {
        fprintf(stderr,"Bad poisoning will set to %d days\n",MaxPoison);
        poison = MaxPoison;
        v = Val_fix ;
    }
    
    poison_plus = poison==0 ? 1 : poison ;
    poison_size = poison_plus * sizeof ( Bits_t ) ;

    // visits (lower)
    if (visits<1) {
        fprintf(stderr,"Holes visited/day will be set to 1\n");
        visits=1 ;
        v = Val_fix ;
    }

    // Calculate holes
    // total holes
    switch( geo ) {
        case Circle:
        case Grid:
            holes = xlength * ylength ;
            break ;
        case Triangle:
            ylength = xlength ;
            holes = xlength * ( xlength+1) / 2 ;
            break ;
    }
    if ( holes > MaxHoles ) {
        fprintf(stderr,"%d Holes calculated for %s -- greater than max %d\n",holes,geoName(geo),MaxHoles);
        return Val_fail ;
    }

    // visits
    if ( visits > holes ) {
        fprintf(stderr, "Changing visits to be no larger than the number of holes (%d)\n",holes);
        visits = holes ;
        v = Val_fix ;
    }

    return v;
}
