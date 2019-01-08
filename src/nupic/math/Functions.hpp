/* ---------------------------------------------------------------------
 * Numenta Platform for Intelligent Computing (NuPIC)
 * Copyright (C) 2013, Numenta, Inc.  Unless you have an agreement
 * with Numenta, Inc., for a separate license for this software code, the
 * following terms and conditions apply:
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU Affero Public License for more details.
 *
 * You should have received a copy of the GNU Affero Public License
 * along with this program.  If not, see http://www.gnu.org/licenses.
 *
 * http://numenta.org/licenses/
 * ---------------------------------------------------------------------
 */

/** @file
 * Declarations for math functions
 */

#ifndef NTA_MATH_FUNCTIONS_HPP
#define NTA_MATH_FUNCTIONS_HPP

#include <nupic/utils/Log.hpp> // For NTA_ASSERT
#include <cmath>

#if __cplusplus >= 201703L // C++17 or greater
  namespace funcs = std;
#else
  #include <boost/math/special_functions/beta.hpp>
  #include <boost/math/special_functions/erf.hpp>
  #include <boost/math/special_functions/gamma.hpp>
  namespace funcs = boost::math;
#endif
#include <cmath>

namespace nupic {

  // TODO: replace other functions by std/math

static const double pi = 3.14159265358979311600e+00;

//--------------------------------------------------------------------------------
template <typename T> inline T lgamma(T x) { return funcs::lgamma(x); }


//--------------------------------------------------------------------------------
template <typename T> inline T beta(T x, T y) {
  return funcs::beta(x, y);
}

//--------------------------------------------------------------------------------
template <typename T> inline T erf(T x) { return funcs::erf(x); }


//--------------------------------------------------------------------------------
//   digamma(T x) is not defined in std:: under C++17 so we include a resonable
//                implementation derived from http://web.science.mq.edu.au/~mjohnson/code/digamma.c
template <typename T> inline T digamma(T x) {
  T result = 0, xx, xx2, xx4;
  NTA_ASSERT(x > 0);
  for ( ; x < 7.0; ++x)
    result -= 1.0/x;
  x -= 1.0/2.0;
  xx = 1.0/x;
  xx2 = xx*xx;
  xx4 = xx2*xx2;
  result += log(x)+(1.0/24.0)*xx2-(7.0/960.0)*xx4+(31.0/8064.0)*xx4*xx2-(127.0/30720.0)*xx4*xx4;
  return result;
}

//--------------------------------------------------------------------------------
double fact(unsigned long n) {
  static double a[171];
  static bool init = true;

  if (init) {
    init = false;
    a[0] = 1.0;
    for (size_t i = 1; i != 171; ++i)
      a[i] = i * a[i - 1];
  }

  if (n < 171)
    return a[n];
  else
    return exp(lgamma(n + 1.0));
}

//--------------------------------------------------------------------------------
double lfact(unsigned long n) {
  static double a[2000];
  static bool init = true;

  if (init) {
    for (size_t i = 0; i != 2000; ++i)
      a[i] = lgamma(i + 1.0);
  }

  if (n < 2000)
    return a[n];
  else
    return lgamma(n + 1.0);
}

//--------------------------------------------------------------------------------
double binomial(unsigned long n, unsigned long k) {
  {
    NTA_ASSERT(k <= n) << "binomial: Wrong arguments: n= " << n << " k= " << k;
  }

  if (n < 171)
    return floor(0.5 + fact(n) / (fact(k) * fact(n - k)));
  else
    return floor(0.5 + exp(lfact(n) - lfact(k) - lfact(n - k)));
}

//--------------------------------------------------------------------------------
}; // namespace nupic

#endif // NTA_MATH_FUNCTIONS_HPP
