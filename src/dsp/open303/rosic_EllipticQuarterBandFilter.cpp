#include "rosic_EllipticQuarterBandFilter.h"
using namespace rosic;

//-------------------------------------------------------------------------------------------------
// construction/destruction:

EllipticQuarterBandFilter::EllipticQuarterBandFilter()
{
  reset();  
}

//-------------------------------------------------------------------------------------------------
// parameter settings:

void EllipticQuarterBandFilter::reset()
{
  for(int i=0; i<12; i++)
    w[i] = 0.0;
}

