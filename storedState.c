// Part of foxhole solver
// {c} 2022 Paul H Alfille
// MIT License
// See https://github.com/alfille/foxholes

void makeStoredState() {
    // Loc.Sorted already set in premadeMovesCreate
    // Basically there is a list of stored states ( game positions possibly including poisoned path )
    // The list is sorted except the tail end that has unsorted for a linear search
    // every so often, the unsorted is sorted back into sorted.
    Loc.iSorted = 0 ;
    Loc.Unsorted = Loc.Sorted + Loc.iSorted ;
    Loc.iUnsorted = 0 ;
    Loc.Next = Loc.Unsorted ;
    //Loc.free = STORESIZE - (Loc.Sorted - Loc.Jumps) ;
    //printf("Jump %p, Possible %p, Sort %p, Usort %p\n",Loc.Jumps,Loc.Possible,Loc.Sorted,Loc.Unsorted);
    //printf("Jump %lu, Possible %lu, Sort %lu, Usort %lu\n",(size_t) holes,Loc.iPossible,Loc.iSorted,Loc.iUnsorted);
}

Bool_t findStoredStates( GM_t * g ) {
    // actually tries to find --> returns True
    // or adds to unsorted --> returns False
    // might re-sort if threshold met
    // aborts if no space

    // fast binary search in sorted
    if ( bsearch( g, Loc.Sorted,    Loc.iSorted,   search_size, compare ) != NULL ) {
        return True ;
    }

    // liner search in unsorted -- will add so need to keep track of size to see that
    size_t ius = Loc.iUnsorted ;
    lsearch( g, Loc.Unsorted, &Loc.iUnsorted, search_size, compare ) ;
    if ( ius == Loc.iUnsorted ) {
        // found
        return True ;
    }
    
    // added to unsorted, move counter. iUnsorted (the count) already incremented
    Loc.Next += search_elements ;
    
    // See if need to move unsorted to sorted and check for enough space for next increment
    if ( Loc.iUnsorted >= UNSORTSIZE ) { // move unsorted -> sorted
        Loc.free -= Loc.iUnsorted * search_elements ;
        if ( Loc.free <= (size_t) (UNSORTSIZE * search_elements) ) { // test if enough space
            fprintf(stderr, "Memory exhausted adding games seen\n");
            exit(1);
        }
        //printf("Sorting\n");
        Loc.iSorted += Loc.iUnsorted ;
        qsort( Loc.Sorted, Loc.iSorted, search_size, compare );
        Loc.Unsorted = Loc.Next ;
        Loc.iUnsorted = 0 ;
    }
    return False ;
}
