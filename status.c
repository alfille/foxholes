// Part of foxhole solver
// {c} 2022 Paul H Alfille
// MIT License
// See https://github.com/alfille/foxholes

char * geoName( Geometry_t g ) {
	switch (g) {
		case Circle:
			return "circle";
		case Triangle:
			return "triangle";
		case Grid:
			return "grid";
		default:
			return "unknown_geometry";
		}
}

char * connName( Connection_t c ) {
    switch (c) {
        case Rectangular:
            return "rectangular";
        case Hexagonal:
            return "hexagonal";
        case Octagonal:
            return "octagonal";
        default:
            return "unknown_connection";
        }
}

void printStatus( char * progname ) {
    printf("%s solver -- {c} 2022 Paul H Alfille\n",progname);
    printf("\n");
    printf("Length %d X Width %d\n",xlength, ylength);
    printf("\t total %d holes \n",holes);
    printf("Geometry_t: %s\n",geoName(geo));
    printf("Connection: %s\n",connName(connection));
    printf("\n");
    printf("%d holes visited per day\n",visits);
    printf("\tHoles poisoned for %d days\n",poison);
    printf("\n");
}

