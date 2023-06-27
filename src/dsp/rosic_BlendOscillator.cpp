#include "rosic_BlendOscillator.h"
using namespace rosic;

//-------------------------------------------------------------------------------------------------
// construction/destruction:

BlendOscillator::BlendOscillator()
{
  // init member variables:
  tableLengthDbl       = (double) MipMappedWaveTable::tableLength;  // typecasted version
  sampleRate           = 44100.0;
  freq                 = 440.0;
  increment            = (tableLengthDbl*freq)/sampleRate;
  phaseIndex           = 0.0;
  startIndex           = 0.0;
  waveTable1           = NULL;
  waveTable2           = NULL;

  // somewhat redundant:
  setSampleRate(44100.0);          // sampleRate = 44100 Hz by default
  setFrequency (440.0);            // frequency = 440 Hz by default
  setStartPhase(0.0);              // sartPhase = 0 by default

  setWaveForm1(MipMappedWaveTable::SAW);
  setWaveForm2(MipMappedWaveTable::SQUARE);

  resetPhase();
}

BlendOscillator::~BlendOscillator()
{

}

//-------------------------------------------------------------------------------------------------
// parameter settings:

void BlendOscillator::setSampleRate(double newSampleRate)
{
  if( newSampleRate > 0.0 )
    sampleRate = newSampleRate;
  sampleRateRec = 1.0 / sampleRate;
  increment = tableLengthDbl*freq*sampleRateRec;
}

void BlendOscillator::setWaveForm1(int newWaveForm1)
{
  if( waveTable1 != NULL )
    waveTable1->setWaveform(newWaveForm1);
}

void BlendOscillator::setWaveForm2(int newWaveForm2)
{
  if( waveTable2 != NULL )
    waveTable2->setWaveform(newWaveForm2);
}

void BlendOscillator::setWaveTable1(MipMappedWaveTable* newWaveTable1)
{
  waveTable1 = newWaveTable1;
}

void BlendOscillator::setWaveTable2(MipMappedWaveTable* newWaveTable2)
{
  waveTable2 = newWaveTable2;
}

void BlendOscillator::setStartPhase(double StartPhase)
{
  if( (StartPhase>=0) && (StartPhase<=360) )
    startIndex = (StartPhase/360.0)*tableLengthDbl;
}

//-------------------------------------------------------------------------------------------------
// event processing:

void BlendOscillator::resetPhase()
{
  phaseIndex = startIndex;
}

void BlendOscillator::setPhase(double PhaseIndex)
{
  phaseIndex = startIndex+PhaseIndex;
}
