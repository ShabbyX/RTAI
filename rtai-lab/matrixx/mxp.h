/*
 * Copyright 2006 Pierangelo Masarati
 * All rights reserved.
 *
 * Use this software as you like, according to GNU's GPL,
 * provided you do not alter this Copyright statement.
 * This software is provided AS IS, without any warranty.
 */

#ifndef MXP_H
#define MXP_H

#define	NDIM		(7)

typedef struct mxp_t {
	char	*name;
	void	*ptr;
	int	type;
	int	ndim;
	int	dim[NDIM];
} mxp_t;

/* return number of vars */
extern int mxp_getnvars(void);

/* return idx of var "name" (-1 if missing) */
extern int mxp_getidxbyname(const char *name);

/* return var based on idx (-1 if missing) */
extern int mxp_getvarbyidx(int idx, mxp_t *nt);

/* copy var in nt ("name" is the original, not a copy!), 
 * or return -1 if missing */
extern int mxp_getvarbyname(const char *name, mxp_t *nt);

/* set pointer to var "name" (return -1 if missing) */
extern int mxp_setptrbyname(const char *name, void *ptr);

/* get type of var "name" (-1 if missing) */
extern int mxp_gettypebyname(const char *name);

/* get number of dimensions of var "name" (-1 if missing) */
extern int mxp_getndimbyname(const char *name);

/* get size of dimension dim of var "name" (-1 if missing) */
extern int mxp_getdimbyname(const char *name, int dim);

/* set var[idx] = val */
extern int mxp_setparam(int var, int *idxv, double val);

/* return var[idx] */
extern int mxp_getparam(int var, int *idxv, double *valp);

#endif /* ! MXP_H */

