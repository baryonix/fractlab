#ifndef _MANDEL_FPDEFS_H
#define _MANDEL_FPDEFS_H

// GMP doesn't currently provide mpf_t to long double conversion.
typedef double mandel_fp_t;
#define mpf_get_mandel_fp mpf_get_d

#endif /* _MANDEL_FPDEFS_H */
