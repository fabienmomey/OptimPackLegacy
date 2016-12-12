/*
 * op_vmlmb --
 *
 * Variable Metric Limited Memory with Bound constraints for OptimPack
 * library.
 *
 *-----------------------------------------------------------------------------
 *
 * Copyright (c) 2003-2009, 2016 Éric Thiébaut.
 *
 * This file is part of OptimPack <https://github.com/emmt/OptimPackLegacy>.
 *
 * OptimPack is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * OptimPack is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * OptimPack (file "LICENSE" in the top source directory); if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307 USA
 *
 *-----------------------------------------------------------------------------
 */

#include <math.h>
#include <string.h>
#include <stdio.h>
#include "optimpack.h"

#define op_dcopy(n, x, y) memcpy((y), (x), (n)*sizeof(double))

#define STPMAX 1e20

const double zero = 0.0;
const double one = 1.0;

static double max(double a, double b) {
  return (a >= b ? a : b);
}

/*---------------------------------------------------------------------------*/
/* LIMITED MEMORY VARIABLE METRIC METHOD (BFGS)
   WITH/WITHOUT BOUND CONSTRAINTS */

/* Indices 0-1 in ISAVE are reserved for op_cshrc. */
#define INDEX_OF_TASK       2
#define INDEX_OF_STAGE      3
#define INDEX_OF_M          4
#define INDEX_OF_N          5
#define INDEX_OF_ITER       6
#define INDEX_OF_MARK       7
#define INDEX_OF_MP         8
#define INDEX_OF_FLAGS      9
#define INDEX_OF_NEVALS    10
#define INDEX_OF_NRESTARTS 11
#if (OP_VMLMB_ISAVE_NUMBER != INDEX_OF_NRESTARTS + 1)
# error bad ISAVE settings
#endif

/* Indices 0-11 in ISAVE are reserved for op_cshrc. */
#define INDEX_OF_SFTOL    12
#define INDEX_OF_SGTOL    13
#define INDEX_OF_SXTOL    14
#define INDEX_OF_FRTOL    15
#define INDEX_OF_FATOL    16
#define INDEX_OF_FMIN     17
#define INDEX_OF_F0       18
#define INDEX_OF_GD       19
#define INDEX_OF_GD0      20
#define INDEX_OF_STP      21
#define INDEX_OF_STPMIN   22
#define INDEX_OF_STPMAX   23
#define INDEX_OF_DELTA    24
#define INDEX_OF_EPSILON  25
#define INDEX_OF_GPNORM   26
#define INDEX_OF_WORK     27 /* must be the last one */
#if (OP_VMLMB_DSAVE_NUMBER(0,0) != INDEX_OF_WORK)
# error bad DSAVE settings
#endif

#define FLAG(n)                          (1 << (n))
#define FLAG_IS_SET(value, flag)         (((value) & (flag)) != 0)
#define FLAG_FMIN                        FLAG(0)

#define SET_TASK(val, str) \
  (op_mcopy("op_vmlmb_first: " str, csave), task=(val))

int op_vmlmb_first(op_integer_t n, op_integer_t m,
		   double fatol, double frtol,
		   double sftol, double sgtol, double sxtol,
		   double delta, double epsilon,
		   char csave[], op_integer_t isave[], double dsave[])
{
  int task = OP_TASK_FG;
  if (n <= 0) return SET_TASK(OP_TASK_ERROR, "N <= 0");
  if (m <= 0) return SET_TASK(OP_TASK_ERROR, "M <= 0");
  if (fatol < 0.0) return SET_TASK(OP_TASK_ERROR, "FATOL < 0");
  if (frtol < 0.0) return SET_TASK(OP_TASK_ERROR, "FRTOL < 0");
  if (sxtol <= 0.0) return SET_TASK(OP_TASK_ERROR, "SXTOL <= 0");
  if (sxtol >= 1.0) return SET_TASK(OP_TASK_ERROR, "SXTOL >= 1");
  if (sftol <= 0.0) return SET_TASK(OP_TASK_ERROR, "SFTOL <= 0");
  if (sftol >= 1.0) return SET_TASK(OP_TASK_ERROR, "SFTOL >= 1");
  if (sgtol <= 0.0) return SET_TASK(OP_TASK_ERROR, "SGTOL <= 0");
  if (sgtol >= 1.0) return SET_TASK(OP_TASK_ERROR, "SGTOL >= 1");
  if (sftol >= sgtol) return SET_TASK(OP_TASK_ERROR, "SFTOL >= SGTOL");
  if (delta < 0.0) return SET_TASK(OP_TASK_ERROR, "DELTA < 0");
  if (epsilon < 0.0) return SET_TASK(OP_TASK_ERROR, "EPSILON < 0");

  isave[INDEX_OF_TASK]      = OP_TASK_FG;
  isave[INDEX_OF_STAGE]     = 0L;
  isave[INDEX_OF_M]         = m;
  isave[INDEX_OF_N]         = n;
  isave[INDEX_OF_ITER]      = 0L;
  isave[INDEX_OF_MARK]      = 0L;
  isave[INDEX_OF_MP]        = 0L;
  isave[INDEX_OF_FLAGS]     = 0L;
  isave[INDEX_OF_NEVALS]    = 0L;
  isave[INDEX_OF_NRESTARTS] = 0L;

  dsave[INDEX_OF_SFTOL]     = sftol;
  dsave[INDEX_OF_SGTOL]     = sgtol;
  dsave[INDEX_OF_SXTOL]     = sxtol;
  dsave[INDEX_OF_FRTOL]     = frtol;
  dsave[INDEX_OF_FATOL]     = fatol;
  dsave[INDEX_OF_FMIN]      = 0.0;
  dsave[INDEX_OF_F0]        = 0.0;
  dsave[INDEX_OF_GD]        = 0.0;
  dsave[INDEX_OF_GD0]       = 0.0;
  dsave[INDEX_OF_STP]       = 0.0;
  dsave[INDEX_OF_STPMIN]    = 0.0;
  dsave[INDEX_OF_STPMAX]    = 0.0;
  dsave[INDEX_OF_DELTA]     = delta;
  dsave[INDEX_OF_EPSILON]   = epsilon;
  dsave[INDEX_OF_GPNORM]    = 0.0;

  return isave[INDEX_OF_TASK];
}
#undef SET_TASK

/*---------------------------------------------------------------------------*/

/* Check H and ACTIVE arrays, possibly fix ACTIVE, returns non-zero
   in case of error. */
static int check_active(op_integer_t n, op_logical_t active[], const double h[],
			int *task, char csave[]);

#define S(K)     &s[(K)*n]
#define Y(K)     &y[(K)*n]
#define SET_TASK(val, str) \
  (op_mcopy("op_vmlmb_next: " str, csave), task=(val))

int op_vmlmb_next(double x[], double *f, double g[],
		  op_logical_t active[], const double h[],
		  char csave[], op_integer_t isave[], double dsave[])
{
  /* Get local variables.  (Note: depending on the value of STAGE, it is
     possible to restore only _some_ of theses variables, but this would
     result in a less readable code with negligible speedup gain.) */
  int          task      = isave[INDEX_OF_TASK];
  int          stage     = isave[INDEX_OF_STAGE];
  op_integer_t m         = isave[INDEX_OF_M];
  op_integer_t n         = isave[INDEX_OF_N];
  op_integer_t iter      = isave[INDEX_OF_ITER];
  op_integer_t mark      = isave[INDEX_OF_MARK];
  op_integer_t mp        = isave[INDEX_OF_MP];
  op_integer_t flags     = isave[INDEX_OF_FLAGS];
  op_integer_t nevals    = isave[INDEX_OF_NEVALS];
  op_integer_t nrestarts = isave[INDEX_OF_NRESTARTS];
  int          have_fmin = ((flags & FLAG_FMIN) != 0);

  double sftol    = dsave[INDEX_OF_SFTOL];
  double sgtol    = dsave[INDEX_OF_SGTOL];
  double sxtol    = dsave[INDEX_OF_SXTOL];
  double fmin     = dsave[INDEX_OF_FMIN];
  double frtol    = dsave[INDEX_OF_FRTOL];
  double fatol    = dsave[INDEX_OF_FATOL];
  double f0       = dsave[INDEX_OF_F0];
  double gd       = dsave[INDEX_OF_GD];
  double gd0      = dsave[INDEX_OF_GD0];
  double stp      = dsave[INDEX_OF_STP];
  double stpmin   = dsave[INDEX_OF_STPMIN];
  double stpmax   = dsave[INDEX_OF_STPMAX];
  double delta    = dsave[INDEX_OF_DELTA];
  double epsilon  = dsave[INDEX_OF_EPSILON];
  double gpnorm   = dsave[INDEX_OF_GPNORM];

  double *alpha = dsave + INDEX_OF_WORK;
  double *rho = alpha + m;
  double *d   = rho + m; /* anti-search direction */
  double *s   = d + n;
  double *y   = s + n*m;
  double *Sm, *Ym, *x0;
  int info, descent;
  op_integer_t i, j, k;

  if (task == OP_TASK_FG) {
    ++nevals;
  }

  if (stage == 0) {
    /* First search direction is the (normalized) steepest descent. */
    if (have_fmin && *f <= fmin) {
      SET_TASK(OP_TASK_ERROR, "initial F <= FMIN");
      goto done;
    }
    iter = 0;  /* number of successful iterations */
    nevals = 1;
    nrestarts = 0;
    mark = 0;  /* index of current direction */
  restart:
    mp = 0;    /* number of saved directions */
    if (check_active(n, active, h, &task, csave) != 0) {
      goto done;
    }
    op_dcopy_active(n, g, d, active); /* steepest ascent */
    gpnorm = op_dnrm2(n, d); /* norm of the projected gradient */
    if (gpnorm == zero) {
      SET_TASK(OP_TASK_CONV, "local minimum found");
      goto done;
    }
    if (h == NULL) {
      /* No preconditioning, use scaled steepest ascent. */
      double scale;
      if (delta > zero) {
        scale = (op_dnrm2(n, x)/gpnorm)*delta;
      } else {
        scale = zero;
      }
      if (scale <= zero) {
        /* The following scale is quite arbitrary but to avoid this would
           require to know the typical size of the variables. */
        scale = one/gpnorm;
      }
      if (scale != one) {
        op_dscal(n, scale, d);
      }
      gd = -(scale*gpnorm)*gpnorm;
    } else {
      /* Use diagonal preconditioner to compute initial search direction. */
      for (i = 0; i < n; ++i) {
	d[i] *= h[i];
      }
      gd = -op_ddot(n, g, d); /* FIXME: active? */
      if (gd >= zero) {
	SET_TASK(OP_TASK_ERROR, "preconditioner is not positive definite");
	goto done;
      }
    }
    stage = 1; /* set STAGE to initialize the line search */

  } else if (stage == 3) {

    /* Previous step was successful.  Compute new search direction H(k).g(k)
     * based on the two-loop recursion L-BFGS formula.  H(k) is the limited
     * memory BFGS approximation of the inverse Hessian, g(k) is the
     * gradient at k-th step.  H(k) is approximated by using the M last
     * pairs (s, y) where:
     *
     *   s(j) = x(j+1) - x(j)
     *   y(j) = g(j+1) - g(j)
     *
     * The two-loop recursion algorithm writes:
     *
     *   1- start with current gradient:
     *        d := g(k)
     *
     *   2- for j = k-1, ..., k-m
     *        rho(j) = s(j)'.y(j)           (save rho(j))
     *        alpha(j) = (s(j)'.d)/rho(j)   (save alpha(j))
     *        d := d - alpha(j) y(j)
     *
     *   3- apply approximation of inverse k-th Hessian:
     *        d := H0(k).d
     *      for instance:
     *        d := (rho(k-1)/(y(k-1)'.y(k-1))) d
     *
     *   4- for j = k-m, ..., k-1
     *        d := d + (alpha(j) - (y(j)'.d)/rho(j)) s(j)
     *
     * Note: in fact, this program saves -S and -Y, but by looking at
     * above relations it is clear that this change of sign:
     *  - has no influence on RHO, and dot products between S and Y;
     *  - changes the sign of ALPHA but not that of ALPHA(j) times
     *    S(j) or Y(j);
     * the two-loop recursion therefore involves the same equations
     * with:     s(j) = x(j+1) - x(j)  and  y(j) = g(j+1) - g(j)
     * or with:  s(j) = x(j) - x(j+1)  and  y(j) = g(j) - g(j+1).
     */
    double gamma = zero;
    op_integer_t mm = mark + m;

    if (check_active(n, active, h, &task, csave) != 0) goto done;
    op_dcopy_active(n, g, d, active);
    for (k = 0; k < mp; ++k) {
      j = (mm - k)%m;
      if (active != NULL) {
        rho[j] = op_ddot_active(n, S(j), Y(j), active);
      }
      if (rho[j] > zero) {
        alpha[j] = op_ddot(n, S(j), d)/rho[j];
        op_daxpy_active(n, -alpha[j], Y(j), d, active);
        if (gamma <= zero) {
          gamma = rho[j]/op_ddot_active(n, Y(j), Y(j), active);
        }
      }
    }
    if (h != NULL) {
      /* Apply diagonal preconditioner. */
      for (i = 0; i < n; ++i) {
	d[i] *= h[i];
      }
    } else if (gamma > zero) {
      /* Apply initial H (i.e. just scale D) and perform the second stage of
	 the 2-loop recursion. */
      op_dscal(n, gamma, d);
    } else {
      /* All correction pairs are invalid: restart the BFGS recursion. */
      ++nrestarts;
      goto restart;
    }
    for (k = mp - 1; k >= 0; --k) {
      j = (mm - k)%m;
      if (rho[j] > zero) {
        op_daxpy_active(n, alpha[j] - op_ddot(n, Y(j), d)/rho[j],
                        S(j), d, active);
      }
    }

    /* Compute dot product of gradient and search direction. */
    gd = -op_ddot(n, g, d); /* FIXME: should be dot product with active */
    if (epsilon > zero) {
      descent = (gd <= -epsilon*op_dnrm2(n, g)*op_dnrm2(n, d)); // FIXME: idem as above
    } else {
      descent = (gd < zero);
    }
    if (! descent) {
      /* L-BFGS recursion yields a search direction which is not a descent.
	 We therefore restart the algorithm with steepest descent. */
      ++nrestarts;
      goto restart;
    }

    /* Advance the mark. */
    mark = (mark + 1)%m;

    /* Set STAGE to initialize the line search. */
    stage = 1;

  } else if (stage == 2) {
    /* Line search in progress: compute derivative with respect to step
       size. */
    gd = -op_ddot(n, g, d); /* FIXME: active? */
  } else {
    SET_TASK(OP_TASK_ERROR, "corrupted workspace");
    goto done;
  }

  if (stage == 1) {
    /* Set variables so as to initialize the line search subroutine. */
    f0 = *f;
    gd0 = gd;
    stpmin = zero;
#if 1
    stpmax = STPMAX;
#else
    if (have_fmin) {
      stpmax = (fmin - f0)/(sgtol*gd0);
    } else {
      double temp = fabs(f0);
      if (temp < 1.0) {
	temp = 1.0;
      }
      stpmax = temp/(sgtol*gd0);
    }
#endif
    stp = OP_MIN(1.0, stpmax);
    op_dcopy(n, x, S(mark)); /* save parameters X0 */
    op_dcopy(n, g, Y(mark)); /* save gradient G0 */
    stage = 2;
    task = OP_TASK_START; /* set TASK so as to start line search */
  } else {
    task = OP_TASK_FG; /* set TASK so as to continue line search */
  }

  if (stage == 2) {
    /* Determine the line search parameter. */
    if (have_fmin && *f < fmin) {
      SET_TASK(OP_TASK_WARN, "F < FMIN");
    } else {
      /* Call line search iterator. */
      info = op_csrch(*f, gd, &stp, sftol, sgtol, sxtol, stpmin, stpmax, &task,
		      csave, isave, dsave);
      if (info == 1) {
        /* Compute the new iterate. */
	for (x0 = S(mark), i = 0; i < n; ++i) {
	  x[i] = x0[i] - stp*d[i];
	}
      } else if (info == 2 || info == 5) {
	/* Line search has converged. */
	++iter;
        if (mp < m) ++mp;
	stage = 3;

        /* Compute the step and gradient change.  Note: we always compute the
	   effective step (parameter difference) to account for bound
	   constraints and, at least, numerical rounding or truncation
	   errors. */
	for (Ym = Y(mark), i = 0; i < n; ++i) Ym[i] -= g[i];
	for (Sm = S(mark), i = 0; i < n; ++i) Sm[i] -= x[i];
	if (active == NULL) rho[mark] = op_ddot(n, Y(mark), S(mark));

	/* Test for global convergence otherwise set TASK to signal a new
	   iterate.  Set STAGE to compute a new search direction. */
        if (op_noneof(n, S(mark))) {
	  SET_TASK(OP_TASK_WARN, "no parameter change");
        } else if (op_noneof(n, Y(mark))) {
	  SET_TASK(OP_TASK_WARN, "no gradient change");
        } else {
          double change = max(fabs(*f - f0), fabs(stp*gd0));
          if (change <= frtol*fabs(f0)) {
            SET_TASK(OP_TASK_CONV, "FRTOL test satisfied");
          } else if (change <= fatol) {
            SET_TASK(OP_TASK_CONV, "FATOL test satisfied");
          } else {
            SET_TASK(OP_TASK_NEWX,
		     "new improved solution available for inspection");
          }
        }
      } else if (info >= 3) {
        /* INFO >= 3 means that line search could not converge.
	   Restore solution at start of line search. */
        op_dcopy(n, S(mark), x);
        op_dcopy(n, Y(mark), g);
        *f = f0;
      }
    }
  }

  /* Save local variables (but constant ones) and return TASK. */
 done:
  isave[INDEX_OF_TASK]      = task;
  isave[INDEX_OF_STAGE]     = stage;
  isave[INDEX_OF_ITER]      = iter;
  isave[INDEX_OF_MARK]      = mark;
  isave[INDEX_OF_MP]        = mp;
#if 0
  isave[INDEX_OF_FLAGS]     = flags; /* constant */
#endif
  isave[INDEX_OF_NEVALS]    = nevals;
  isave[INDEX_OF_NRESTARTS] = nrestarts;

#if 0
  dsave[INDEX_OF_SFTOL]     = sftol; /* constant */
  dsave[INDEX_OF_SGTOL]     = sgtol; /* constant */
  dsave[INDEX_OF_SXTOL]     = sxtol; /* constant */
  dsave[INDEX_OF_FRTOL]     = frtol; /* constant */
  dsave[INDEX_OF_FATOL]     = fatol; /* constant */
  dsave[INDEX_OF_FMIN]      = fmin;  /* constant */
#endif
  dsave[INDEX_OF_F0]        = f0;
  dsave[INDEX_OF_GD]        = gd;
  dsave[INDEX_OF_GD0]       = gd0;
  dsave[INDEX_OF_STP]       = stp;
  dsave[INDEX_OF_STPMIN]    = stpmin;
  dsave[INDEX_OF_STPMAX]    = stpmax;
#if 0
  dsave[INDEX_OF_DELTA]     = delta;   /* constant */
  dsave[INDEX_OF_EPSILON]   = epsilon; /* constant */
  dsave[INDEX_OF_GPNORM]    = gpnorm;  /* FIXME: save gpnorm */
#endif
  return task;
}

#undef SET_TASK
#undef Y
#undef S

/*---------------------------------------------------------------------------*/

static int
check_active(op_integer_t n, op_logical_t active[],
             const double h[], int *task, char csave[])
{
  if (h != NULL) {
    const double zero = 0.0;
    op_integer_t i;
    if (active != NULL) {
      /* fix ACTIVE array */
      for (i = 0; i < n; ++i) {
	if (active[i] && h[i] <= zero) {
	  active[i] = 0;
	}
      }
    } else {
      /* check that H is positive definite */
      for (i = 0; i < n; ++i) {
	if (h[i] <= zero) {
	  op_mcopy("op_vmlmb_next: H is not positive definite", csave);
	  *task = OP_TASK_ERROR;
	  return -1;
	}
      }
    }
  }
  return 0;
}

/*---------------------------------------------------------------------------*/

#define EMIT_CODE(name, NAME)                                   \
  int op_vmlmb_set_##name(const char csave[],                   \
                          op_integer_t isave[],                 \
                          double dsave[],                       \
                          double new_value,                     \
                          double *old_value)                    \
  {                                                             \
    int test = ((isave[INDEX_OF_FLAGS] & FLAG_##NAME) != 0);    \
    if (test && old_value != (double *)NULL) {                  \
      *old_value = dsave[INDEX_OF_##NAME];                      \
    }                                                           \
    dsave[INDEX_OF_##NAME] = new_value;                         \
    isave[INDEX_OF_FLAGS] |= FLAG_##NAME;                       \
    return test;                                                \
  }                                                             \
  int op_vmlmb_get_##name(const char csave[],                   \
                          const op_integer_t isave[],           \
                          const double dsave[],                 \
                          double *ptr)                          \
  {                                                             \
    int test = ((isave[INDEX_OF_FLAGS] & FLAG_##NAME) != 0);    \
    if (test && ptr != (double *)NULL) {                        \
      *ptr = dsave[INDEX_OF_##NAME];                            \
    }                                                           \
    return test;                                                \
  }
EMIT_CODE(fmin, FMIN)
#undef EMIT_CODE

#define EMIT_CODE(type, foo, FOO, save)				\
type OP_CONCAT(op_vmlmb_get_,foo)(const char csave[],		\
				  const op_integer_t isave[],	\
				  const double dsave[])		\
{ return dsave[OP_CONCAT(INDEX_OF_,FOO)]; }
EMIT_CODE(double, sftol,      SFTOL,    dsave)
EMIT_CODE(double, sgtol,      SGTOL,    dsave)
EMIT_CODE(double, sxtol,      SXTOL,    dsave)
EMIT_CODE(double, frtol,      FRTOL,    dsave)
EMIT_CODE(double, fatol,      FATOL,    dsave)
EMIT_CODE(double, step,       STP,      dsave)
EMIT_CODE(double, delta,      DELTA,    dsave)
EMIT_CODE(double, epsilon,    EPSILON,  dsave)
EMIT_CODE(double, gpnorm,     GPNORM,   dsave)
EMIT_CODE(op_integer_t, iter,      ITER,      isave)
EMIT_CODE(op_integer_t, nevals,    NEVALS,    isave)
EMIT_CODE(op_integer_t, nrestarts, NRESTARTS, isave)
#undef EMIT_CODE

/*---------------------------------------------------------------------------*/
