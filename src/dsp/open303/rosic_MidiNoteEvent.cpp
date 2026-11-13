#include "rosic_MidiNoteEvent.h"
using namespace rosic;

//-------------------------------------------------------------------------------------------------
// construction/destruction:

MidiNoteEvent::MidiNoteEvent()
{
  key       = 64;
  vel       = 64;
  detune    = 0.0;
  priority  = 0;
}

MidiNoteEvent::MidiNoteEvent(int initKey, int initVel, int initDetune, int initPriority)
{
  if( initKey >=0 && initKey <= 127)
    key = initKey;
  else
    key = 64;

  if( initVel >=0 && initVel <= 127)
    vel = initVel;
  else
    vel = 64;

  if( initPriority >=0 )
    priority = initPriority;
  else
    priority = 0;

  detune = initDetune;
}

MidiNoteEvent::~MidiNoteEvent()
{

}

//-------------------------------------------------------------------------------------------------
// parameter settings:

void MidiNoteEvent::setKey(int newKey)
{
  if( newKey >=0 && newKey <= 127)
    key = newKey;
}

void MidiNoteEvent::setVelocity(int newVelocity)
{
  if( newVelocity >=0 && newVelocity <= 127)
    vel = newVelocity;
}

void MidiNoteEvent::setDetune(double newDetune)
{
  detune = newDetune;
}

void MidiNoteEvent::setPriority(int newPriority)
{
  if( newPriority >=0 )
    priority = newPriority;
}
