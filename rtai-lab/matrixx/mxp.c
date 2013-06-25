/*
 * Copyright 2006 Pierangelo Masarati
 * All rights reserved.
 *
 * Use this software as you like, according to GNU's GPL,
 * provided you do not alter this Copyright statement.
 * This software is provided AS IS, without any warranty.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <regex.h>

#include "sa_types.h"

#include "mxp.h"

#ifndef MXP_LIB
static int	nnames;
static mxp_t	*nametypes;
#else /* MXP_LIB */
#include "mxp_data.hc"
#endif /* MXP_LIB */

// this function is hacked from system.h
static inline void set_double(double *to, double *from)
{
	unsigned long l = ((unsigned long *)from)[0];
	unsigned long h = ((unsigned long *)from)[1];
	__asm__ __volatile__ ("1: movl (%0), %%eax; movl 4(%0), %%edx; lock; cmpxchg8b (%0); jnz 1b" : : "D"(to), "b"(l), "c"(h) : "ax", "dx", "memory");
}

int
mxp_getnvars(void)
{
	return nnames;
}

int
mxp_getidxbyname(const char *name)
{
	int	i;

	for (i = 0; i < nnames; i++) {
		if (strcmp(name, nametypes[i].name) == 0) {
			return i;
		}
	}

	return -1;
}

int
mxp_getvarbyname(const char *name, mxp_t *nt)
{
	int	i = mxp_getidxbyname(name);

	if (i == -1) {
		return -1;
	}

	*nt = nametypes[i];

	return 0;
}

int
mxp_getvarbyidx(int idx, mxp_t *nt)
{
	if (idx < 0 || idx >= nnames) {
		return -1;
	}

	*nt = nametypes[idx];

	return 0;
}

int
mxp_setptrbyname(const char *name, void *ptr)
{
	int	i = mxp_getidxbyname(name);

	if (i == -1) {
		return -1;
	}

	nametypes[i].ptr = ptr;

	return 0;
}

int
mxp_gettypebyname(const char *name)
{
	int	i = mxp_getidxbyname(name);

	if (i == -1 ) {
		return -1;
	}

	return nametypes[i].type;
}

int
mxp_getndimbyname(const char *name)
{
	int	i = mxp_getidxbyname(name);

	if (i == -1 ) {
		return -1;
	}

	return nametypes[i].ndim;
}

int
mxp_getdimbyname(const char *name, int dim)
{
	int	i = mxp_getidxbyname(name);

	if (i == -1 ) {
		return -1;
	}

	if (dim >= nametypes[i].ndim) {
		return -1;
	}

	return nametypes[i].dim[dim];
}

static int
mxp_pvt_checkparamidx(int var, int *idxv)
{
	int	idx = 0,
		offset = 1,
		i;

	/* assumes var is valid and idxv is not NULL */
	for (i = nametypes[var].ndim; i-- > 0;) {
		if (idxv[i] < 0 || idxv[i] >= nametypes[var].dim[i]) {
			return -1;
		}

		idx += offset*idxv[i];
		offset *= nametypes[var].dim[i];
	}

	return idx;
}

int
mxp_setparam(int var, int *idxv, double val)
{
	int	idx;

	if (var < 0 || var >= nnames) {
		return -1;
	}

	idx = mxp_pvt_checkparamidx(var, idxv);
	if (idx == -1) {
		return -1;
	}

	/* NOTE: keep in sync with strtype */
	switch (nametypes[var].type) {
	case 0:
		((int *)nametypes[var].ptr)[idx] = (int)val;
		break;

	case 1:
		((long *)nametypes[var].ptr)[idx] = (long)val;
		break;

	case 2:
		((float *)nametypes[var].ptr)[idx] = (float)val;
		break;

	case 3:
		set_double(&((double *)nametypes[var].ptr)[idx], &val);
		break;

	case 4:
		((VAR_INTEGER *)nametypes[var].ptr)[idx] = (VAR_INTEGER)val;
		break;

	case 5:
		if (sizeof(VAR_FLOAT) == sizeof(double)) {
			set_double(&((double *)nametypes[var].ptr)[idx], &val);
		} else {
			((VAR_FLOAT *)nametypes[var].ptr)[idx] = (VAR_FLOAT)val;
		}
		break;

	default:
		return -1;
	}

	return 0;
}

int
mxp_getparam(int var, int *idxv, double *valp)
{
	int	idx;

	if (var < 0 || var >= nnames) {
		return -1;
	}

	idx = mxp_pvt_checkparamidx(var, idxv);
	if (idx == -1) {
		return -1;
	}

	/* NOTE: keep in sync with strtype */
	switch (nametypes[var].type) {
	case 0:
		*valp = (double)((int *)nametypes[var].ptr)[idx];
		break;

	case 1:
		*valp = (double)((long *)nametypes[var].ptr)[idx];
		break;

	case 2:
		*valp = (double)((float *)nametypes[var].ptr)[idx];
		break;

	case 3:
		*valp = (double)((double *)nametypes[var].ptr)[idx];
		break;

	case 4:
		*valp = (double)((VAR_INTEGER *)nametypes[var].ptr)[idx];
		break;

	case 5:
		*valp = (double)((VAR_FLOAT *)nametypes[var].ptr)[idx];
		break;

	default:
		return -1;
	}

	return 0;
}

#ifndef MXP_LIB

typedef struct mxp_lv_t {
	size_t	len;
	char	*val;
} mxp_lv_t;

#define STRLENOF(s)	(sizeof((s)) - 1)
#define	LVC(s)		{ STRLENOF((s)), (s) }

/* list types here; the position in the array identifies them */
typedef struct mxp_type_t {
	mxp_lv_t	type;
	size_t		size;
} mxp_type_t;

/* NOTE: keep in sync with mxp_{g|s}etparam() */
static mxp_type_t strtype[] = {
	{ LVC("int"),		sizeof(int) },
	{ LVC("long"),		sizeof(long) },
	{ LVC("float"),		sizeof(float) },
	{ LVC("double"),	sizeof(double) },

	/* ... */

	{ LVC("VAR_INTEGER"),	sizeof(VAR_INTEGER) },
	{ LVC("VAR_FLOAT"),	sizeof(VAR_FLOAT) },

	/* ... */

	{ { 0, NULL },		0 }
};

static int		ntypes;

static int
mxp_pvt_lvcmp(const mxp_lv_t *lv1, const mxp_lv_t *lv2)
{
	int	d = lv1->len - lv2->len;

	if (d != 0) {
		return d;
	}

	return strncmp(lv1->val, lv2->val, lv1->len);
}

static int
mxp_pvt_lvn1cmp(const mxp_lv_t *lv1, const mxp_lv_t *lv2)
{
	int	d = lv1->len - lv2->len;

	if (d > 0) {
		return d;
	}

	return strncmp(lv1->val, lv2->val, lv1->len);
}

static int
mxp_pvt_destroy(void)
{
	int	i;

	for (i = 0; i < nnames; i++) {
		free(nametypes[i].name);
	}

	free(nametypes);

	return 0;
}

static int
mxp_pvt_str2type(const char *s)
{
	int		i;
	mxp_lv_t	lv;

	lv.len = strlen(s);
	lv.val = (char *)s;

	for (i = 0; strtype[i].type.val != NULL; i++) {
		if (mxp_pvt_lvcmp(&lv, &strtype[i].type) == 0) {
			return i;
		}
	}

	return -1;
}

#ifdef notused
static const char *
mxp_pvt_type2str(int type)
{
	if (type < 0 || type >= ntypes) {
		return NULL;
	}

	return strtype[type].type.val;
}
#endif /* notused */

static int
mxp_pvt_init(int ndim, regex_t *r)
{
	const char	pattern_start[] = "([_[:alpha:]][_[:alnum:]]*([[:space:]]+[_[:alpha:]][_[:alnum:]]*)*)[[:space:]]+([_[:alpha:]][_[:alnum:]]*)*",
			pattern_end[] = "[[:space:]]*;",
			pattern_dim_start[] = "((\\[([^]]+)])",
			pattern_dim_end[] = ")*";
	char		pattern[BUFSIZ],
			*ptr;
	int		i, rc = 0;

	if (sizeof(pattern_start) - 1
		+ ndim*(sizeof(pattern_dim_start) - 1 + sizeof(pattern_dim_end) - 1)
		+ sizeof(pattern_end) >= sizeof(pattern))
	{
		fprintf(stderr, "too many dimensions %d\n", ndim);
		return -1;
	}

	ptr = pattern;
	memcpy(ptr, pattern_start, sizeof(pattern_start));
	ptr += sizeof(pattern_start) - 1;
	for (i = 0; i < ndim; i++) {
		memcpy(ptr, pattern_dim_start, sizeof(pattern_dim_start));
		ptr += sizeof(pattern_dim_start) - 1;
	}
	for (i = 0; i < ndim; i++) {
		memcpy(ptr, pattern_dim_end, sizeof(pattern_dim_end));
		ptr += sizeof(pattern_dim_end) - 1;
	}
	memcpy(ptr, pattern_end, sizeof(pattern_end));

	rc = regcomp(r, pattern, REG_EXTENDED|REG_ICASE);
	if (rc != 0) {
		fprintf(stderr, "unable to compile regular expression\n");
	}

	for (ntypes = 0; strtype[ntypes].type.val != NULL; ntypes++)
		/* count'em */ ;

	return rc;
}

static int
mxp_pvt_addvar(mxp_t *nt)
{
	mxp_t	*tmpnametypes;
	int	i, j;

	for (i = 0; i < nnames; i++) {
		int	cmp = strcmp(nt->name, nametypes[i].name);

		if (cmp == 0) {
			fprintf(stderr, "var \"%s\" redefined?\n", nt->name);
			return -1;
		}

		if (cmp < 0) {
			break;
		}
	}

	tmpnametypes = realloc(nametypes, sizeof(mxp_t) * (nnames + 2));
	if (tmpnametypes == NULL) {
		return -1;
	}

	/* keep it sorted */
	nametypes = tmpnametypes;

	for (j = nnames; j > i; j--) {
		nametypes[j] = nametypes[j-1];
	}

	nametypes[i] = *nt;
	nametypes[i].name = strdup(nt->name);
	nnames++;
	nametypes[nnames].name = NULL;

	return 0;
}

static int
mxp_pvt_parse_line(regex_t *r, const char *line, int ndim, int nmatch, regmatch_t *match)
{
	char		namebuf[BUFSIZ];
	mxp_t		nt = { NULL };
	int		rc;
	int		i;
	const char	*ptr;

	rc = regexec(r, line, nmatch, match, 0);
	if (rc != 0) {
		fprintf(stderr, "line=\"%s\": no match!\n", line);
		return rc;
	}

#define TYPE	(1)
	memcpy(namebuf, &line[match[TYPE].rm_so], match[TYPE].rm_eo - match[TYPE].rm_so);
	namebuf[match[TYPE].rm_eo - match[TYPE].rm_so] = '\0';
	/* TODO: may need to normalize type (e.g. count number of '*',
	 * count modifiers, trim extra spaces and so */
	nt.type = mxp_pvt_str2type(namebuf);
	if (nt.type == -1) {
		fprintf(stderr, "unknown type \"%s\"\n", namebuf);
		return -1;
	}

#define	VAR	(3)
	memcpy(namebuf, &line[match[VAR].rm_so], match[VAR].rm_eo - match[VAR].rm_so);
	namebuf[match[VAR].rm_eo - match[VAR].rm_so] = '\0';

#define	DIM	(6)
	for (i = 0; i < NDIM; i++) {
		char	*next = NULL;
		int	j = DIM + 3*i;

		if (match[j].rm_so == -1) {
			break;
		}

		ptr = &line[match[j].rm_so];
		nt.dim[i] = strtol(ptr, &next, 0);
		if (next == ptr || next - ptr != match[j].rm_eo - match[j].rm_so) {
			fprintf(stderr, "unable to parse dimension #%d \"%.*s\"\n",
				i, match[j].rm_eo - match[j].rm_so, ptr);
			return -1;
		}

		if (i == NDIM - 1 && match[j].rm_so != match[j - 3].rm_eo + 2) {
			fprintf(stderr, "silly check on number of dimensions failed\n");
			return -1;
		}
	}

	nt.ndim = i;
	nt.name = namebuf;

	mxp_pvt_addvar(&nt);

	return 0;
}

int
main(int argc, char *argv[])
{
	char		buf[LINE_MAX];
	regex_t		r;
#define	NMATCH		(4 + 3*NDIM)
	regmatch_t	match[NMATCH];
	int		i;
	FILE		*fout = NULL;
	int		got_marker = 0,
			cur_marker = -1,
			num_markers;

	/* NOTE: keep end_marker in sync with begin_marker */
	static mxp_lv_t	begin_marker[] = {
		LVC("/* Model variable definitions. */"),
		LVC("/* Xmath variable definitions. */"),
		{ 0, NULL }
	},
			end_marker[] = {
		LVC("/* Model variable declarations. */"),
		LVC("/* Xmath variable declarations. */"),
		{ 0, NULL }
	};

	/* check begin_marker/end_marker consistency */
	for (num_markers = 0; begin_marker[num_markers].val != NULL; num_markers++) {
		if (end_marker[num_markers].val == NULL) {
			fprintf(stderr, "end_marker shorter than begin_marker\n");
			exit(EXIT_FAILURE);
		}
	}

	if (end_marker[num_markers].val != NULL) {
		fprintf(stderr, "end_marker longer than begin_marker\n");
		exit(EXIT_FAILURE);
	}

	/* initialize regex */
	if (mxp_pvt_init(NDIM, &r) != 0) {
		exit(EXIT_FAILURE);
	}

	/* parse lines and register mxp_t structures */
	while (fgets(buf, sizeof(buf), stdin) != NULL) {
		size_t	len = strlen(buf);

		if (buf[len - 1] == '\n') {
			buf[--len] = '\0';
		}

		if (cur_marker == -1) {
			mxp_lv_t	lv;

			lv.len = len;
			lv.val = buf;

			for (i = 0; i < num_markers; i++) {
				if (mxp_pvt_lvn1cmp(&begin_marker[i], &lv) == 0) {
					cur_marker = i;
					got_marker++;
					break;
				}
			}
			goto next_line;

		} else {
			mxp_lv_t	lv;

			lv.len = len;
			lv.val = buf;

			for (i = 0; i < num_markers; i++) {
				if (mxp_pvt_lvn1cmp(&end_marker[i], &lv) == 0) {
					cur_marker = -1;
					if (got_marker == num_markers) {
						goto done;
					}
					goto next_line;
				}
			}
		}

		if (mxp_pvt_parse_line(&r, buf, NDIM, NMATCH, match) ) {
			exit(EXIT_FAILURE);
		}

next_line:;
	}

done:;

	/* regex is no longer needed */
	regfree(&r);

	/* print all registered structures */
	for (i = 0; i < nnames; i++) {
		mxp_t	nt;
		char	*name = nametypes[i].name;

		/* NOTE: here we're playing fool, pretending we know
		 * the names of the vars */
		if (mxp_getvarbyname(name, &nt)) {
			fprintf(stderr, "unable to get var \"%s\"\n", name);
			exit(EXIT_FAILURE);
		}
	}

	fout = fopen("mxp_data.hc", "w");
	if (fout == NULL) {
		fprintf(stderr, "unable to open file \"mxp_data.hc\"\n");
		exit(EXIT_FAILURE);
	}

	fprintf(fout, "/*\n");
	fprintf(fout, " * Copyright 2006 Pierangelo Masarati\n");
	fprintf(fout, " * All rights reserved.\n");
	fprintf(fout, " *\n");
	fprintf(fout, " * Use this software as you like, according to GNU's GPL,\n");
	fprintf(fout, " * provided you do not alter this Copyright statement.\n");
	fprintf(fout, " * This software is provided AS IS, without any warranty.\n");
	fprintf(fout, " */\n");
	fprintf(fout, "/*\n");
	fprintf(fout, " * This is a generated file, modifications will be lost.\n");
	fprintf(fout, " */\n");
	fprintf(fout, "\n");

	fprintf(fout, "#ifndef MXP_DATA_HC\n");
	fprintf(fout, "#define MXP_DATA_HC\n");
	fprintf(fout, "\n");

	fprintf(fout, "/* private structure */\n");
	fprintf(fout, "static int nnames = %d;\n", nnames);
	fprintf(fout, "static mxp_t nametypes[] = {\n");
	for (i = 0; i < nnames; i++) {
		mxp_t	*nt = &nametypes[i];

		fprintf(fout, "\t{ \"%s\", NULL, %d, %d, { %d",
			nt->name, nt->type, nt->ndim, nt->dim[0]);
		if (nt->ndim > 0) {
			int	j;

			for (j = 1; j < nt->ndim; j++) {
				fprintf(fout, ", %d", nt->dim[j]);
			}
		}
		fprintf(fout, " } },\n");
	}
	fprintf(fout, "\t{ NULL }\n");
	fprintf(fout, "};\n");
	fprintf(fout, "\n");

	fprintf(fout, "#endif /* MXP_DATA_HC */\n");

	fclose(fout);

	fout = fopen("mxp_data.h", "w");
	if (fout == NULL) {
		fprintf(stderr, "unable to open file \"mxp_data.hc\"\n");
		exit(EXIT_FAILURE);
	}

	fprintf(fout, "/*\n");
	fprintf(fout, " * Copyright 2006 Pierangelo Masarati\n");
	fprintf(fout, " * All rights reserved.\n");
	fprintf(fout, " *\n");
	fprintf(fout, " * Use this software as you like, according to GNU's GPL,\n");
	fprintf(fout, " * provided you do not alter this Copyright statement.\n");
	fprintf(fout, " * This software is provided AS IS, without any warranty.\n");
	fprintf(fout, " */\n");
	fprintf(fout, "/*\n");
	fprintf(fout, " * This is a generated file, modifications will be lost.\n");
	fprintf(fout, " */\n");
	fprintf(fout, "\n");

	fprintf(fout, "#ifndef MXP_DATA_H\n");
	fprintf(fout, "#define MXP_DATA_H\n");
	fprintf(fout, "\n");

	fprintf(fout, "/* call this macro right after declarations */\n");
	fprintf(fout, "#define MXP_REGISTER \\\n");
	fprintf(fout, "\tdo { \\\n");
	for (i = 0; i < nnames; i++) {
		mxp_t      *nt = &nametypes[i];

		fprintf(fout, "\t\tmxp_setptrbyname(\"%s\", (void *)%s%s); \\\n",
			nt->name, nt->ndim ? "" : "&", nt->name);
	}
	fprintf(fout, "\t} while (0)\n");
	fprintf(fout, "\n");

	fprintf(fout, "#endif /* MXP_DATA_H */\n");

	fclose(fout);

	/* private data no longer needed */
	mxp_pvt_destroy();

	return 0;
}
#endif /* ! MXP_LIB */
