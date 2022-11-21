// Part of foxhole solver
// {c} 2022 Paul H Alfille
// MIT License
// See https://github.com/alfille/foxholes

#ifndef COMPARE
#define COMPARE

int compare1(const void* pA, const void* pB) ;
int compare2(const void* pA, const void* pB) ;
int compare3(const void* pA, const void* pB) ;
int compare4(const void* pA, const void* pB) ;
int compare5(const void* pA, const void* pB) ;
int compare6(const void* pA, const void* pB) ;
int compareP(const void* pA, const void* pB) ;

int (* compare )(const void *, const void *) ;

#endif // COMPARE

//#define COMPsetup const Bits_t * ulongA = pA; const Bits_t * ulongB = pB 
#define COMPsetup const Bits_t * ulongA = pA; const Bits_t * ulongB = pB ; register Bits_t diff
//#define COMPpass(n) if ( ulongA[n]<ulongB[n] ) return 1; if ( ulongA[n]>ulongB[n] ) return -1
#define COMPpass(n) diff = ( ulongA[n]<ulongB[n] ) - ( ulongA[n]>ulongB[n] ) ; if (diff) return diff 
#define COMPreturn(n) return ( ulongA[n]<ulongB[n] ) - ( ulongA[n]>ulongB[n] )
#define COMPdefault(n) return memcmp( (const void *) (ulongA+(n)), (const void *) (ulongB+(n)), (search_elements - (n))*sizeof(Bits_t) )

int compare1(const void* pA, const void* pB) {
    const Bits_t * ulongA = pA;
    const Bits_t * ulongB = pB ;
    COMPreturn(0) ;
}
    
int compare2(const void* pA, const void* pB) {
    COMPsetup ;
    COMPpass(0) ;
    COMPreturn(1) ;
}
    
int compare3(const void* pA, const void* pB) {
    COMPsetup ;
    COMPpass(0) ;
    COMPpass(1) ;
    COMPreturn(2) ;
}
    
int compare4(const void* pA, const void* pB) {
    COMPsetup ;
    COMPpass(0) ;
    COMPpass(1) ;
    COMPpass(2) ;
    COMPreturn(3) ;
}
    
int compare5(const void* pA, const void* pB) {
    COMPsetup ;
    COMPpass(0) ;
    COMPpass(1) ;
    COMPpass(2) ;
    COMPpass(3) ;
    COMPreturn(4) ;
}
    
int compare6(const void* pA, const void* pB) {
    COMPsetup ;
    COMPpass(0) ;
    COMPpass(1) ;
    COMPpass(2) ;
    COMPpass(3) ;
    COMPpass(4) ;
    COMPreturn(5) ;
}
    
int compareP(const void* pA, const void* pB) {
    COMPsetup ;
    COMPpass(0) ;
    COMPpass(1) ;
    COMPpass(2) ;
    COMPpass(3) ;
    COMPpass(4) ;
    COMPpass(5) ;
    COMPdefault(6) ;
}
    
