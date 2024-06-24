#ifndef rosic_FunctionTemplates_h
#define rosic_FunctionTemplates_h

// rosic includes:
//#include "GlobalDefinitions.h"
#include "rosic_RealFunctions.h"
#include <algorithm>
#include <cstring>

namespace rosic
{
  // todo write functions for element-wise multiply, divide, negate,
  // max, min, absMax, createCopy, filter, impulseResponse, impulseResponseLength,
  // fillWith(double value = 0.0), circularShift, resample,

  // maybe introduce a range (start....end) to which the process is to be applied

  /** Returns the absolute value of the input argument for types that define the comparison
  operators: '<', '>', the unary operator '-' and a constructor that takes an int and initializes
  to zero when 0 is passed.  */
  template <class T>
  T absT(T x);

  /** Adds the elements of 'buffer1' and 'buffer2' - type must define operator '+'. The 'result'
  buffer may be the same as 'buffer1' or 'buffer2'. */
  template <class T>
  void add(T *buffer1, T *buffer2, T *result, int length);

  /** Circularly shifts the content of the buffer by 'numPositions' to the right - for leftward
  shifts use negative values for numPositions. If the absolute value of 'numPositions' is greater
  than the length of the buffer, it will use numPositions modulo the length - so if the length is 6
  and numPositions is 8, it will whift by 2 positions. */
  template<class T>
  void circularShift(T *buffer, int length, int numPositions);

  /** Restricts the values in the buffer to the range between min and max for types that define the
  operators '<' and '>'. */
  template <class T>
  void clipBuffer(T *buffer, int length, T min, T max);

  /** Copies the data of one array into another one. */
  template <class T>
  void copyBuffer(T *source, T *destination, int length);

  /** Fills the passed array with all zeros - the type must have a constructor that takes an int
  and initializes to the zero element when 0 is passed. */
  template <class T>
  void fillWithZeros(T *buffer, int length);

  /** Finds and returns the maximum absolute value of the buffer. */
  template <class T>
  T maxAbs(T *buffer, int length);

  /** Returns the index of maximum value of the buffer (">"-operator must be defined). */
  template <class T>
  INLINE int maxIndex(T *buffer, int length);

  /** Returns the maximum value of the buffer (">"-operator must be defined). */
  template <class T>
  INLINE T maxValue(T *buffer, int length);

  /** Returns the index of minimum value of the buffer ("<"-operator must be defined). */
  template <class T>
  INLINE int minIndex(T *buffer, int length);

  /** Returns the minimum value of the buffer ("<"-operator must be defined). */
  template <class T>
  INLINE T minValue(T *buffer, int length);

  /** Computes the mean (i.e. the DC-component) from the passed buffer. The type must define operators:
  +=, / and a constructor which takes an int and initializes to zero when 0 is passed and a typecast
  from int. */
  template <class T>
  T mean(T *buffer, int length);

  /** Returns the median of the passed buffer. */
  template <class T>
  T median(T *buffer, int length);

  /** Multiplies the elements of 'buffer1' and 'buffer2' - type must define operator '*'. The
  'result' buffer may be the same as 'buffer1' or 'buffer2'. */
  template <class T>
  void multiply(T *buffer1, T *buffer2, T *result, int length);

  /** Normalizes the maximum value of the passed array. The type must define: >, *=, / and a
  constructor that takes an int and initializes to 0 when 0 is passed. Additionaly, it must be
  suitable for absT - that additionaly requires definition of unary '-' and '<'. */
  template <class T>
  void normalize(T *buffer, int length, T maximum);

  /** Rearranges/permutes and array of type T into bit-reversed order. The 'length' MUST be the
  'numBits' th power of two (this is not checked for). */
  template <class T>
  INLINE void orderBitReversedOutOfPlace(T *inBuffer, T *outBuffer, int length, int numBits);

  /** Returns the product of the elements in the buffer for types which define the
  multiplication operator (the *= version thereof) and a constructor which can take an int
  paramater as argument and initializes to the multiplicative neutral element of that class when 1
  is passed . */
  template <class T>
  INLINE T product(T *buffer, int length);

  /** Removes mean (i.e. the DC-component) from the passed buffer. The type must define operators:
  +=, -=, / and a constructor which takes an int and initializes to zero when 0 is passed and a
  typecast from int. */
  template <class T>
  void removeMean(T *buffer, int length);

  /** Reverses the order of the elements the passed array. */
  template <class T>
  void reverse(T *buffer, int length);

  /** The maximum of two objects on which the ">"-operator is defined. */
  template <class T>
  INLINE T rmax(T in1, T in2);

  /** The maximum of four objects on which the ">"-operator is defined. */
  template <class T>
  INLINE T rmax(T in1, T in2, T in3, T in4);

  /** The minimum of two objects on which the ">"-operator is defined. */
  template <class T>
  INLINE T rmin(T in1, T in2);

  /** The minimum of four objects on which the ">"-operator is defined. */
  template <class T>
  INLINE T rmin(T in1, T in2, T in3, T in4);

  /** Scales the buffer by a constant factor. */
  template <class T>
  void scale(T *buffer, int length, T scaleFactor);

  /** Subtracts the elements of 'buffer2' from 'buffer1' - type must define operator '-'. The
  'result' buffer may be the same as 'buffer1' or 'buffer2'. */
  template <class T>
  void subtract(T *buffer1, T *buffer2, T *result, int length);

  /** Returns the sum of the elements in the buffer for types which define the
  addition operator (the += version thereof) and a constructor which can take an int
  paramater as argument and initializes to the additive neutral element of that class when 0
  is passed . */
  template <class T>
  INLINE T sum(T *buffer, int length);

  /** Swaps two objects of class T. */
  template <class T>
  INLINE void swap(T &in1, T &in2);

  //===============================================================================================
  // implementation:

  template <class T>
  T absT(T x)
  {
    if( x > T(0) )
      return x;
    else if( x < T(0) )
      return -x;
    else
      return T(0);
  }

  template <class T>
  void add(T *buffer1, T *buffer2, T *result, int length)
  {
    for(int i=0; i<length; i++)
      result[i] = buffer1[i] + buffer2[i];
  }

  template <class T>
  void circularShift(T *buffer, int length, int numPositions)
  {
    int na = abs(numPositions);
    while( na > length )
      na -=length;
    T *tmp = new T[na];
    if( numPositions < 0 )
    {
      memcpy(  tmp,                buffer,              na*sizeof(T));
      memmove( buffer,            &buffer[na], (length-na)*sizeof(T));
      memcpy( &buffer[length-na],  tmp,                 na*sizeof(T));
    }
    else if( numPositions > 0 )
    {
      memcpy(  tmp,        &buffer[length-na],          na*sizeof(T));
      memmove(&buffer[na],  buffer,            (length-na)*sizeof(T));
      memcpy(  buffer,      tmp,                        na*sizeof(T));
    }
    delete[] tmp;
  }

  template <class T>
  void clipBuffer(T *buffer, int length, T min, T max)
  {
    for(int i=0; i<length; i++)
    {
      if( buffer[i] < min )
        buffer[i] = min;
      else if( buffer[i] > max )
        buffer[i] = max;
    }
  }

  template <class T>
  void copyBuffer(T *source, T *destination, int length)
  {
    for(int i=0; i<length; i++)
      destination[i] = source[i];
  }

  template <class T>
  void fillWithZeros(T *buffer, int length)
  {
    for(int i=0; i<length; i++)
      buffer[i] = T(0);
  }

  template <class T>
  T maxAbs(T *buffer, int length)
  {
    T max = T(0);
    for(int i=0; i<length; i++)
    {
      if( absT(buffer[i]) > max)
        max = absT(buffer[i]);
    }
    return max;
  }

  template <class T>
  int maxIndex(T *buffer, int length)
  {
    T   value = buffer[0];
    int index = 0;
    for(int i=0; i<length; i++)
    {
      if( buffer[i] > value )
      {
        value = buffer[i];
        index = i;
      }
    }
    return index;
  }

  template <class T>
  T maxValue(T *buffer, int length)
  {
    return buffer[maxIndex(buffer, length)];
  }

  template <class T>
  int minIndex(T *buffer, int length)
  {
    T   value = buffer[0];
    int index = 0;
    for(int i=0; i<length; i++)
    {
      if( buffer[i] < value )
      {
        value = buffer[i];
        index = i;
      }
    }
    return index;
  }

  template <class T>
  T minValue(T *buffer, int length)
  {
    return buffer[minIndex(buffer, length)];
  }

  template <class T>
  T mean(T *buffer, int length)
  {
    return sum(buffer, length) / (T) length;
  }

  template <class T>
  void multiply(T *buffer1, T *buffer2, T *result, int length)
  {
    for(int i=0; i<length; i++)
      result[i] = buffer1[i] * buffer2[i];
  }

  template <class T>
  void normalize(T *buffer, int length, T maximum)
  {
    T max   = maxAbs(buffer, length);;
    T scale = maximum / max;
    for(int i=0; i<length; i++)
      buffer[i] *= scale;
  }

  template <class T>
  INLINE T product(T *buffer, int length)
  {
    T accu = T(1); // constructor call with 1 should initilize to multiplicative neutral element
    for(int n=0; n<length; n++)
      accu *= buffer[n];
    return accu;
  }

  template <class T>
  void removeMean(T *buffer, int length)
  {
    T m = mean(buffer, length);
    for(int i=0; i<length; i++)
      buffer[i] -= m;
  }

  template <class T>
  INLINE T rmax(T in1, T in2)
  {
    if( in1 > in2 )
      return in1;
    else
      return in2;
  }

  template <class T>
  INLINE T rmax(T in1, T in2, T in3, T in4)
  {
    return rmax(rmax(in1, in2), rmax(in3, in4));
  }

  template <class T>
  INLINE T rmin(T in1, T in2)
  {
    if( in1 < in2 )
      return in1;
    else
      return in2;
  }

  template <class T>
  INLINE T rmin(T in1, T in2, T in3, T in4)
  {
    return rmin(rmin(in1, in2), rmin(in3, in4));
  }

  template <class T>
  void reverse(T *buffer, int length)
  {
    T tmp;
    int lengthMinus1 = length-1;
    for(int i=0; i<=(length-2)/2; i++)
    {
      tmp                    = buffer[lengthMinus1-i];
      buffer[lengthMinus1-i] = buffer[i];
      buffer[i]              = tmp;
    }
  }

  template <class T>
  void scale(T *buffer, int length, T scaleFactor)
  {
    for(int n=0; n<length; n++)
      buffer[n] *= scaleFactor;
  }

  template <class T>
  void subtract(T *buffer1, T *buffer2, T *result, int length)
  {
    for(int i=0; i<length; i++)
      result[i] = buffer1[i] - buffer2[i];
  }

  template <class T>
  INLINE T sum(T *buffer, int length)
  {
    T accu = T(0); // constructor call with 0 should initilizes to additive neutral element
    for(int n=0; n<length; n++)
      accu += buffer[n];
    return accu;
  }

  template <class T>
  INLINE void swap(T &in1, T &in2)
  {
    T tmp = in1;
    in1   = in2;
    in2   = tmp;
  }

} // end namespace rosic

#endif // #ifndef rosic_FunctionTemplates_h
