/* Definitions for simple data type for real numbers.
   Copyright (C) 2002-2014 Free Software Foundation, Inc.

This file is part of GCC.

GCC is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 3, or (at your option) any later
version.

GCC is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License
along with GCC; see the file COPYING3.  If not see
<http://www.gnu.org/licenses/>.  */

#ifndef GCC_SREAL_H
#define GCC_SREAL_H

/* SREAL_PART_BITS has to be an even number.  */
#define SREAL_PART_BITS 32

#define UINT64_BITS	64

#define SREAL_MIN_SIG ((uint64_t) 1 << (SREAL_PART_BITS - 1))
#define SREAL_MAX_SIG (((uint64_t) 1 << SREAL_PART_BITS) - 1)
#define SREAL_MAX_EXP (INT_MAX / 4)

#define SREAL_BITS SREAL_PART_BITS

/* Structure for holding a simple real number.  */
class sreal
{
public:
  /* Construct an uninitialized sreal.  */
  sreal () : m_sig (-1), m_exp (-1), m_negative (0) {}

  /* Construct a sreal.  */
  sreal (int64_t sig, int exp = 0) : m_exp (exp)
  {
    m_negative = sig < 0;

    if (sig < 0)
      sig = -sig;

    m_sig = (uint64_t) sig;

    normalize ();
  }

  void dump (FILE *) const;
  int64_t to_int () const;
  sreal operator+ (const sreal &other) const;
  sreal operator- (const sreal &other) const;
  sreal operator* (const sreal &other) const;
  sreal operator/ (const sreal &other) const;

  bool operator< (const sreal &other) const
  {
    /* We negate result in case of negative numbers and
       it would return true for equal negative numbers.  */
    if (*this == other)
      return false;

    if (m_negative != other.m_negative)
      return m_negative > other.m_negative;

    bool r = m_exp < other.m_exp
      || (m_exp == other.m_exp && m_sig < other.m_sig);

    return m_negative ? !r : r;
  }

  bool operator== (const sreal &other) const
  {
    return m_exp == other.m_exp && m_sig == other.m_sig
		    && m_negative == other.m_negative;
  }

  sreal operator- () const
  {
    if (m_sig == 0)
      return *this;

    sreal tmp = *this;
    tmp.m_negative = !tmp.m_negative;

    return tmp;
  }

  sreal shift (int sig) const
  {
    sreal tmp = *this;
    tmp.m_sig += sig;

    return tmp;
  }

  /* Global minimum sreal can hold.  */
  inline static sreal min ()
  {
    static sreal min = sreal (-SREAL_MAX_SIG, SREAL_MAX_EXP);
    return min;
  }

  /* Global minimum sreal can hold.  */
  inline static sreal max ()
  {
    static sreal max = sreal (SREAL_MAX_SIG, SREAL_MAX_EXP);
    return max;
  }

private:
  void normalize ();
  void shift_right (int amount);

  static sreal signedless_plus (const sreal &a, const sreal &b, bool negative);
  static sreal signedless_minus (const sreal &a, const sreal &b, bool negative);

  uint64_t m_sig;			/* Significant.  */
  signed int m_exp;			/* Exponent.  */
  bool m_negative;			/* Negative sign.  */
};

extern void debug (sreal &ref);
extern void debug (sreal *ptr);

inline sreal &operator+= (sreal &a, const sreal &b)
{
  return a = a + b;
}

inline sreal &operator-= (sreal &a, const sreal &b)
{
  return a = a - b;
}

inline sreal &operator/= (sreal &a, const sreal &b)
{
  return a = a / b;
}

inline sreal &operator*= (sreal &a, const sreal &b)
{
  return a = a  * b;
}

inline bool operator!= (const sreal &a, const sreal &b)
{
  return !(a == b);
}

inline bool operator> (const sreal &a, const sreal &b)
{
  return !(a == b || a < b);
}

inline bool operator<= (const sreal &a, const sreal &b)
{
  return a < b || a == b;
}

inline bool operator>= (const sreal &a, const sreal &b)
{
  return a == b || a > b;
}

inline sreal operator<< (const sreal &a, int exp)
{
  return a.shift (exp);
}

inline sreal operator>> (const sreal &a, int exp)
{
  return a.shift (-exp);
}

static void verify_numbers (int a, int b)
{
  gcc_assert ((a < b) == (sreal (a) < sreal (b)));
  gcc_assert ((a <= b) == (sreal (a) <= sreal (b)));
  gcc_assert ((a == b) == (sreal (a) == sreal (b)));
  gcc_assert ((a != b) == (sreal (a) != sreal (b)));
  gcc_assert ((a > b) == (sreal (a) > sreal (b)));
  gcc_assert ((a >= b) == (sreal (a) >= sreal (b)));
  gcc_assert ((a + b) == (sreal (a) + sreal (b)).to_int ());
  gcc_assert ((a - b) == (sreal (a) - sreal (b)).to_int ());
  gcc_assert ((b + a) == (sreal (b) + sreal (a)).to_int ());
  gcc_assert ((b - a) == (sreal (b) - sreal (a)).to_int ());
}

static void check_sreal ()
{
  sreal minimum = INT_MIN;
  sreal maximum = INT_MAX;
  sreal seven = 7;
  sreal minus_two = -2;
  sreal minus_nine = -9;

  gcc_assert (minimum.to_int () == INT_MIN);
  gcc_assert (maximum.to_int () == INT_MAX);

  gcc_assert (!(minus_two < minus_two));
  gcc_assert (!(seven < seven));
  gcc_assert (seven > minus_two);
  gcc_assert (minus_two < seven);
  gcc_assert (minus_two != seven);
  gcc_assert (minus_two == minus_two);
  gcc_assert (seven == seven);

  gcc_assert (seven == ((seven >> 40) << 40));

  gcc_assert ((seven + minus_two) == 5);
  gcc_assert ((seven + minus_nine) == -2);

  for (int a = -100; a < 100; a++)
    for (int b = -100; b < 100; b++)
      {
        verify_numbers (a, b);
        verify_numbers (INT_MIN + 100, b);
        verify_numbers (INT_MAX - 100, b);
      }


  srand (123456);

  for (int i = 0; i < 1000 * 1000; i++)
    {
      verify_numbers (rand () % 10, rand () % 1000000);
      verify_numbers (rand () % 100, rand () % 10000);
    }
}


#endif
