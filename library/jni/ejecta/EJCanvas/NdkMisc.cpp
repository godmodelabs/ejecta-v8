#include "NdkMisc.h"


// Calculates log2 of number.
double log2( double n )
{
    // log(n)/log(2) is log2.
    return log( n ) / 0.30102999566398119521373889472449;
}

// Calculates log2 of number.
float log2f (float n)
{
    // log(n)/log(2) is log2.
    return logf (n) / 0.30102999566398119521373889472449f;
}
