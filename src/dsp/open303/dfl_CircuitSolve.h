// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 David Lowenfels

#ifndef dfl_CircuitSolve_h
#define dfl_CircuitSolve_h

// Small dense linear solve shared by the circuit-native filters' Newton iterations.

#include <cmath>
#include <utility>
#include "rosic_RealFunctions.h"

namespace dfl
{

  /** Solves J dx = -F by Gaussian elimination with partial pivoting (J and F are overwritten). */
  template <int N>
  INLINE void solveLinear(double (&J)[N][N], double (&F)[N], double (&dx)[N])
  {
    for (int c = 0; c < N; ++c)
    {
      int p = c;
      for (int r = c + 1; r < N; ++r) if (std::fabs(J[r][c]) > std::fabs(J[p][c])) p = r;
      if (p != c) { for (int k = 0; k < N; ++k) std::swap(J[c][k], J[p][k]); std::swap(F[c], F[p]); }
      for (int r = c + 1; r < N; ++r)
      {
        const double m = J[r][c] / J[c][c];
        for (int k = c; k < N; ++k) J[r][k] -= m * J[c][k];
        F[r] -= m * F[c];
      }
    }
    for (int r = N - 1; r >= 0; --r)
    {
      double s = -F[r];
      for (int k = r + 1; k < N; ++k) s -= J[r][k] * dx[k];
      dx[r] = s / J[r][r];
    }
  }

} // end namespace dfl

#endif // dfl_CircuitSolve_h
