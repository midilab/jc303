#include "rosic_AcidSequencer.h"
using namespace rosic;

//-------------------------------------------------------------------------------------------------
// construction/destruction:

AcidSequencer::AcidSequencer()
{
  sampleRate    = 44100.0;
  bpm           = 140.0;
  activePattern = 0;
  running       = false;
  countDown     = 0;
  step          = 0;
  sequencerMode = OFF;
  driftError    = 0.0;
  modeChanged   = false;

  for(int k=0; k<=12; k++)
    keyPermissible[k] = true;
}

//-------------------------------------------------------------------------------------------------
// parameter settings:

void AcidSequencer::setSampleRate(double newSampleRate)
{
  if( newSampleRate > 0.0 )
    sampleRate = newSampleRate;
}

void AcidSequencer::setMode(int newMode)
{
  if( newMode >= 0 && newMode < NUM_SEQUENCER_MODES )
  {
    sequencerMode = newMode;
    modeChanged   = true;
  }
}

void AcidSequencer::setKeyPermissible(int key, bool shouldBePermissible)
{
  if( key >= 0 && key <= 12 )
    keyPermissible[key] = shouldBePermissible;
}

void AcidSequencer::toggleKeyPermissibility(int key)
{
  if( key >= 0 && key <= 12 )
    keyPermissible[key] = !keyPermissible[key];
}

//-------------------------------------------------------------------------------------------------
// inquiry:

AcidPattern* AcidSequencer::getPattern(int index)
{
  if( index < 0 || index >= numPatterns )
    return NULL;
  else
    return &patterns[index];
}

bool AcidSequencer::modeWasChanged()
{
  bool result = modeChanged;
  modeChanged = false;
  return result;
  // mmm...wouldn't we need mutexes here? the mode changes from the GUI and modeWasChanged
  // is called from the audio-thread - otherwise note-hangs could happen?
}

bool AcidSequencer::isKeyPermissible(int key)
{
  if( key >= 0 && key <= 12 )
    return keyPermissible[key];
  else
    return false;
}

//-------------------------------------------------------------------------------------------------
// event handling:

void AcidSequencer::start()
{
  // set up members such that we will trap in the else-branch in the next call to getNote():
  running    = true;
  countDown  = -1;
  step       = 0;
  driftError = 0.0;
}

void AcidSequencer::stop()
{
  running = false;
}

//-------------------------------------------------------------------------------------------------
// others:
