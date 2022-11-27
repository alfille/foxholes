// Part of foxhole solver
// {c} 2022 Paul H Alfille
// MIT License
// See https://github.com/alfille/foxholes

#define QQ(x) "\"" #x "\"" 

void jsonOut( void ) {
    // output settings and moves
    fprintf(jfile,"{");
        fprintf(jfile, QQ(length)":%d,\n", xlength);
        fprintf(jfile, QQ(width)":%d,\n",  ylength);
        fprintf(jfile, QQ(visits)":%d,\n", visits );
        fprintf(jfile, QQ(poison_days)":%d,\n", poison );
        fprintf(jfile, QQ(connection)":"QQ(%s)",\n", connName(connection) );
        fprintf(jfile, QQ(geometry)":"QQ(%s)",\n", geoName(geo) );
        if ( victoryDay >= 0 ) {
            fprintf(jfile, QQ(days)":%d,\n", victoryDay );
            fprintf(jfile, QQ(moves)":[" ) ;
                for ( int d=1; d<=victoryDay ; ++d ) {
                    fprintf(jfile,"[");
                    int v = 0 ;
                    for ( int b=0 ; b<holes ; ++b ) {
                        if ( getB(victoryMove[d],b) ) {
                            ++v ;
                            fprintf(jfile,"%d%s",b,v==visits?"":",");
                        }
                    }
                    fprintf(jfile,"]%s",d==victoryDay?"":",");
                }
            fprintf(jfile,"],\n") ;
            fprintf(jfile, QQ(solved)":%s", "true" ); // last, no comma
        } else {
            fprintf(jfile, QQ(solved)":%s", "false" ); // last, no comma
        }   
    fprintf(jfile,"}\n");
}
