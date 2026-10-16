#ifndef rosic_Complex_h
#define rosic_Complex_h

// rosic-indcludes:
#include "rosic_RealFunctions.h"

namespace rosic
{

  /**

  This is a class for complex numbers. It defines the basic arithmetic operations between complex 
  numbers as well as the special cases when one of the operands is real (double).

  ATTENTION: do not define any further member variables, nor switch the ordering of re and im 
  because the FourierTransformer classes rely on the fact that a complex number consists of two 
  doubles re, im and nothing else (the algorithms actually run on buffers of doubles).

  */

  class Complex  
  {

  public:

    //---------------------------------------------------------------------------------------------
    // public member variables:

    /** Real part */
    double re;  

    /** Imaginary part */
    double im;  

    //---------------------------------------------------------------------------------------------
    // construction/destruction:

    /** Constructor. Initializes real and imaginary part to zero. */
    Complex(); 

    /** Constructor. Initializes real part to the argument "reInit" and imaginary part to zero. */
    Complex(double reInit);

    /** Constructor. Initializes real and imaginary parts with the parameters. */
    Complex(double reInit, double imInit);

    /** Destructor. */
    ~Complex(); 

    //---------------------------------------------------------------------------------------------
    // overloaded operators:

    /** Compares two complex numbers of equality. */
    bool operator==(const Complex& z) const  
    {
      if( re == z.re && im == z.im )
        return true;
      else
        return false;
    }

    /** Compares two complex numbers of inequality. */
    bool operator!=(const Complex& z) const  
    {
      if( re != z.re || im != z.im )
        return true;
      else
        return false;
    }

    /** Defines the negative of a complex number. */
    Complex operator-()
    { return Complex(-re, -im); }

    /** Adds another complex number to this complex and returns the result. */
    Complex& operator+=(const Complex &z)
    {
      this->re += z.re;
      this->im += z.im;
      return *this;
    }

    /** Adds a real number to this complex and returns the result. */
    Complex& operator+=(const double &r)
    {
      this->re += r;
      return *this;
    }

    /** Subtracts another complex number from this complex and returns the result. */
    Complex& operator-=(const Complex &z)
    {
      this->re -= z.re;
      this->im -= z.im;
      return *this;
    }

    /** Subtracts a real number from this complex and returns the result. */
    Complex& operator-=(const double &r)
    {
      this->re -= r;
      return *this;
    }

    /** Multiplies this complex number by another complex number and returns the result. */
    Complex& operator*=(const Complex &z)
    {
      double reNew = re*z.re - im*z.im;
      double imNew = re*z.im + im*z.re;
      this->re     = reNew;
      this->im     = imNew;
      return *this;
    }

    /** Multiplies this complex number by a real number and returns the result. */
    Complex& operator*=(const double &r)
    {
      this->re *= r;
      this->im *= r;
      return *this;
    }

    /** Divides this complex number by another complex number and returns the result. */
    Complex& operator/=(const Complex &z)
    {
      double scale = 1.0 / (z.re*z.re + z.im*z.im);
      double reNew = scale*( re*z.re  + im*z.im  );
      double imNew = scale*( im*z.re  - re*z.im  );
      this->re     = reNew;
      this->im     = imNew;
      return *this;
    }

    /** Divides this complex number by a real number and returns the result. */
    Complex& operator/=(const double &r)
    {
      double scale = 1.0 / r;
      this->re *= scale;
      this->im *= scale;
      return *this;
    }

    //---------------------------------------------------------------------------------------------
    // set-functions:

    /** Adjusts the radius of this complex number leaving the angle unchanged. */
    void setRadius(double newRadius);

    /** Adjusts the angle of this complex number leaving the magnitude unchanged. */    
    void setAngle(double newAngle);

    /** Sets the radius and angle of this complex number. */
    void setRadiusAndAngle(double newRadius, double newAngle);

    //---------------------------------------------------------------------------------------------
    // get-functions:

    /** Returns the radius of this complex number. */
    double getRadius();

    /** Returns the angle of this complex number. */
    double getAngle();

    /** Returns the complex conjugate of this complex number. */
    Complex getConjugate();

    /** Returns the reciprocal of this complex number. */
    Complex getReciprocal();

    /** Returns true, if this complex number is purely real. */
    bool isReal();

    /** Returns true, if this complex number is purely imaginary. */
    bool isImaginary();

    /** Returns true if real or imaginary part (or both) are plus or minus infinity, false 
    otherwise. */
    bool isInfinite();

  }; // end of class Complex

  // some binary operators are defined outside the class such that the left hand operand does 
  // not necesarrily need to be of class Complex

  /** Adds two complex numbers. */
  INLINE Complex operator+(const Complex &z, const Complex &w)
  { return Complex(z.re+w.re, z.im+w.im); }

  /** Adds a complex and a real number. */
  INLINE Complex operator+(const Complex &z, const double &r)
  { return Complex(z.re+r, z.im); }

  /** Adds a real and a complex number. */
  INLINE Complex operator+(const double &r, const Complex &z)
  { return Complex(z.re+r, z.im); }

  /** Subtracts two complex numbers. */
  INLINE Complex operator-(const Complex &z, const Complex &w)
  { return Complex(z.re-w.re, z.im-w.im); }

  /** Subtracts a real number from a complex number. */
  INLINE Complex operator-(const Complex &z, const double &r)
  { return Complex(z.re-r, z.im); }

  /** Subtracts a complex number from a real number. */
  INLINE Complex operator-(const double &r, const Complex &z)
  { return Complex(r-z.re, -z.im); }

  /** Multiplies two complex numbers. */
  INLINE Complex operator*(const Complex &z, const Complex &w)
  { return Complex(z.re*w.re-z.im*w.im, z.re*w.im+z.im*w.re); }

  /** Multiplies a complex number and a real number. */
  INLINE Complex operator*(const Complex &z, const double &r)
  { return Complex(z.re*r, z.im*r); }

  /** Multiplies a real number and a complex number. */
  INLINE Complex operator*(const double &r, const Complex &z)
  { return Complex(z.re*r, z.im*r); }

  /** Divides two complex numbers. */
  INLINE Complex operator/(const Complex &z, const Complex &w)
  { 
    double scale = 1.0 / (w.re*w.re + w.im*w.im);
    return Complex( scale*( z.re*w.re + z.im*w.im),     // real part
                    scale*( z.im*w.re - z.re*w.im)  );  // imaginary part
  }

  /** Divides a complex number by a real number. */
  INLINE Complex operator/(const Complex &z, const double &r)  
  {
    double scale = 1.0 / r;
    return Complex(scale*z.re, scale*z.im);
  }

  /** Divides a real number by a complex number. */
  INLINE Complex operator/(const double &r, const Complex &z)  
  {
    double scale = r / (z.re*z.re + z.im*z.im);
    return Complex(scale*z.re, -scale*z.im);
  }

}  // end namespace rosic

#endif // rosic_Complex_h
