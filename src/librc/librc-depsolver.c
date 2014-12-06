/*
   librc-depsolver
   rc service dependency and ordering
   */

/*
 * Copyright (c) 2014 Dmitry Yu Okunev <dyokunev@ut.mephi.ru>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <search.h>		/* hsearch() */
#include <stdint.h>		/* uint32_t/uint64_t */

#include "queue.h"
#include "librc.h"
#include "librc-depend.h"
#include "librc-depsolver.h"
#include "../libeinfo/einfo.h"

#define LOOPSOLVER_LIMIT	128

/*! Type definition of service ID */
typedef uint32_t service_id_t;

/*! Enumeration of solve_loop()'s return cases */
typedef enum loopfound {
	LOOP_SOLVABLE	= 0x01,
	LOOP_UNSOLVABLE	= 0x02,
	LOOP_CANCELED	= 0x03,
} loopfound_t;

/* "use, need, after" dependencies matrix types */
typedef enum unapm_type {
	UNAPM_USE            = 0,
	UNAPM_AFTER          = 1,
	UNAPM_NEED           = 2,
	UNAPM_PROVIDEDBY     = 3,
	UNAPM_MIXED          = 4,
	UNAPM_MIXED_EXPANDED = 5,
	UNAPM_MAX
} unapm_type_t;

typedef struct idid_entry {
	uint64_t	idid;
	void		*data;
} idid_entry_t;


static inline int
unapm_expandsdeps(service_id_t **unap, service_id_t service_id)
{
	int dep_num, dep_count;
	int ismodified;

	ismodified = 0;
	dep_num    = 0;
	dep_count  = unap[service_id][0];
	while (dep_num < dep_count) {
		service_id_t dep_service_id;
		int dep_dep_num, dep_dep_count;

		dep_num++;
		dep_service_id = unap[service_id][dep_num];
		/*printf("service_id == %i; dep_num == %i (%i); dep_service_id == %i\n", service_id, dep_num, dep_count, dep_service_id);*/

		dep_dep_num   = 0;
		dep_dep_count = unap[dep_service_id][0];

		while (dep_dep_num < dep_dep_count) {
			int istoadd, dep_num_2;
			service_id_t dep_dep_service_id;
			dep_dep_num++;

			dep_dep_service_id = unap[dep_service_id][dep_dep_num];

			istoadd   = 1;
			dep_num_2 = 0;
			while (dep_num_2 < dep_count) {
				dep_num_2++;
				if (dep_dep_service_id == unap[service_id][dep_num_2]) {
					istoadd = 0;
					break;
				}
			}

			if (istoadd) {
				ismodified = 1;
				dep_count++;
				unap[service_id][dep_count] = dep_dep_service_id;
				unap[service_id][0]         = dep_count;
			}
		}
	}

	return ismodified;
}

/*! Fills dependency matrix for further loop detection
 * @param unap_matrix matrix to fill
 * @param useneedafter_count number of use/need/after/provide dependencies (zero to do not allocate memory)
 * @param service_id ID of the service for dependency scanning
 * @param type dependencies type
 * @param depinfo dependencies information */
static void
unapm_getdependencies(service_id_t **unap_matrix,
	int useneedafter_count, service_id_t service_id,
	const char *type, RC_DEPINFO *depinfo)
{
	RC_STRING *svc, *svc_np;
	RC_DEPTYPE *deptype;

	if (useneedafter_count)
		unap_matrix[service_id] = xcalloc((useneedafter_count+1), sizeof(**unap_matrix));

	deptype = rc_deptree_get_deptype(depinfo, type);
	if (deptype == NULL)
		return;

	TAILQ_FOREACH_SAFE(svc, deptype->services, entries, svc_np) {
		ENTRY item, *item_p;
		service_id_t dependon;

		item.key = svc->value;

		item_p = hsearch(item, FIND);
		if (item_p == NULL)	/* Deadend branch, no sense to continue checking it anyway */
			continue;

		dependon = (int)(long int)item_p->data;

		if (dependon == service_id)
			continue;	/* To prevent looping detection services on themselves (for example in case of depending on '*') */

		unap_matrix[service_id][ ++unap_matrix[service_id][0] ] = dependon;
	}

	return;
}

static int
svc_id2depinfo_bt_compare(const void *a, const void *b)
{
	return ((const ENTRY *)a)->key - ((const ENTRY *)b)->key;
}

static int
idid_compare(const void *a, const void *b)
{
	return ((const idid_entry_t *)a)->idid - ((const idid_entry_t *)b)->idid;
}

static int
idid_compare_data_desc(const void *a, const void *b)
{
	intptr_t a_dat = (intptr_t)(((const idid_entry_t *)a)->data);
	intptr_t b_dat = (intptr_t)(((const idid_entry_t *)b)->data);

	return b_dat - a_dat;
}

static inline char
deptype2char(unapm_type_t type) {
	switch (type) {
		case UNAPM_USE:
			return 'u';
		case UNAPM_AFTER:
			return 'a';
		case UNAPM_NEED:
			return 'n';
		case UNAPM_PROVIDEDBY:
			return 'p';
		default:	/* shouldn't happend */
			return '?';
	}

	return '?';
}


/*! an "action" function for twalk() to build an unsorted array of dependencies presence from prepared btree */

static int           idid_btree_builddescarray_amount;		/* number of elements of the result array (should be set to zero before running this function) */
static idid_entry_t *idid_btree_builddescarray_counters;	/* the result array (should be allocated before running this function) */

static void
idid_btree_builddescarray(const void *nodep, const VISIT which, const int depth) {
	idid_entry_t *idid_counters = idid_btree_builddescarray_counters;	/* Just a shorthand */
	(void)depth;
	switch (which) {
		case preorder:
		case leaf: {
			const idid_entry_t *idid_entry_p = *(idid_entry_t * const*)nodep;

			memcpy(
				&idid_counters[idid_btree_builddescarray_amount],
				idid_entry_p,
				sizeof(idid_counters[idid_btree_builddescarray_amount])
			);
			idid_btree_builddescarray_amount++;
			break;
		}
		default:
			break;
	}
	return;
}


/*! removes a looping dependency (in both directions)
 * @param unap matrixes to scan ways to solve the loop (it should be modified to be able to continue scanning after the dependency removing)
 * @param dep_remove_from_service_id source service id of the dependency
 * @param dep_remove_to_service_id   destination service id of the dependency
 * @param di_from RC_DEPINFO of the direct-way dependency
 * @param di_to   RC_DEPINFO of the reverse-way dependency
 * @param type the direct-way dependency type (string, e.g. "iuse")
 * @param unapm_type binary direct-way dependency type for the UNA matrix */
static void
remove_loopdependency(service_id_t **unap[UNAPM_MAX], service_id_t dep_remove_from_service_id, service_id_t dep_remove_to_service_id, RC_DEPINFO *di_from, RC_DEPINFO *di_to, const char *const type, unapm_type_t unapm_type)
{
	RC_DEPTYPE *deptype_from = NULL, *deptype_to = NULL;
	int dep_num, dep_count;
	const char *type_reverse = NULL;
	int deptype_num;

	/* removing use/after from cache */
	if (di_from != NULL) {
		deptype_from = rc_deptree_get_deptype(di_from, type);
		if (deptype_from != NULL)
			rc_stringlist_delete(deptype_from->services, di_to->service);
	}

	/* removing from the UNAP matrix */
	if (deptype_from != NULL || di_from == NULL) {
		dep_num   = 0;
		dep_count = unap[unapm_type][dep_remove_from_service_id][0];
		while (dep_num++ < dep_count) {
			if (unap[unapm_type][dep_remove_from_service_id][dep_num] == dep_remove_to_service_id) {
				unap[unapm_type][dep_remove_from_service_id][dep_num] =
					unap[unapm_type][dep_remove_from_service_id][dep_count--];
				if (deptype_from != NULL && di_to != NULL)
					ewarn("Solving the loop by breaking %s %c> %s.",
						di_to->service, deptype2char(unapm_type), di_from->service);
			}
		}
		unap[unapm_type][dep_remove_from_service_id][0] = dep_count;
	}


	/* removing the reverse dependency */

	if (di_to == NULL)
		return;

	deptype_num = 0;
	while (deppairs[deptype_num].depend) {
		if (!strcmp(deppairs[deptype_num].depend, type)) {
			type_reverse = deppairs[deptype_num].addto;
			break;
		}
		deptype_num++;
	}

	deptype_to = rc_deptree_get_deptype(di_to, type_reverse);

	if (deptype_to != NULL)
		rc_stringlist_delete(deptype_to->services, di_from->service);

	return;
}

/*! solves dependecies loops
 * @param unap_matrix matrixes to scan ways to solve the loop
 * @param service_id looped service id
 * @param svc_id2depinfo_bt ptr to binary tree root to get depinfo by svc id
 * @param end_dep_num looping dependency id in use/need/after/provide matrix line */
static loopfound_t
solve_loop(service_id_t **unap_matrix[UNAPM_MAX], service_id_t service_id, void *svc_id2depinfo_bt, int end_dep_num, RC_DT_FLAGS flags) {
	char **chain_strs = NULL;
	service_id_t **chains;
	unapm_type_t **deptypes;
	unapm_type_t minimal_cost;
	int chains_size = unap_matrix[0][0][0], chain_count;

	/* svc_id2depinfo_bt may be NULL while any unit tests to simplify them */
	char printwarn  =  (svc_id2depinfo_bt != NULL) && (flags & RCDTFLAGS_LOOPSOLVER_WARNINGS);
	char printerr   =   svc_id2depinfo_bt != NULL;

	if (! (flags & RCDTFLAGS_LOOPSOLVER))
		return LOOP_CANCELED;

	chains = xmalloc(chains_size * sizeof(*chains));

	/* building all dependency chains of the service */

	{

		int dep_num, unap_line_size;
		service_id_t *unap_line;

		unap_line_size = unap_matrix[UNAPM_MIXED_EXPANDED][service_id][0]+1;
		unap_line =  xmalloc(unap_line_size * sizeof(*unap_line));

		memcpy(unap_line, unap_matrix[UNAPM_MIXED_EXPANDED][service_id], unap_line_size * sizeof(*unap_line));

		unap_line[0] =  service_id;
		chain_count =  0;
		dep_num     = -1;
		while (++dep_num < end_dep_num) {
			int dep_dep_num, added_count;
			service_id_t dep_service_id;

			dep_service_id = unap_line[dep_num];
			dep_dep_num    = dep_num;
			added_count    = 0;
			while (++dep_dep_num <= end_dep_num) {
				int chain_num, chain_count_new, dep_dep_check_num, dep_dep_check_count, istobeadded;
				service_id_t dep_dep_service_id, dep_dep_service_id_added;

				dep_dep_service_id = unap_line[dep_dep_num];

				dep_dep_check_num   = 0;
				dep_dep_check_count = unap_matrix[UNAPM_MIXED][dep_service_id][0];
				istobeadded         = 0;
				while (dep_dep_check_num++ < dep_dep_check_count) {
					if (unap_matrix[UNAPM_MIXED][dep_service_id][dep_dep_check_num] == dep_dep_service_id) {
						istobeadded = 1;
						break;
					}
				}

				if (istobeadded) {
					int chain_len;

					chain_num       = 0;
					chain_count_new = chain_count;

#define					CHAINS_CHECK_SIZE {\
						if (chain_count_new >= chains_size) {\
							chains_size += unap_matrix[0][0][0];\
							chains       = xrealloc(chains, chains_size*sizeof(*chains));\
						}\
						/*printf("A: %i\n", chain_count_new);*/\
						chains[chain_count_new]    = xmalloc(end_dep_num*sizeof(**chains));\
						chains[chain_count_new][0] = 0;\
					}

					if (dep_num == 0) {
						CHAINS_CHECK_SIZE;
						chains[chain_count_new][1] = dep_dep_service_id;
						chains[chain_count_new][0] = 1;
						chain_count_new++;
					} else
					while (chain_num < chain_count) {
						if (!added_count) {
							chain_len = chains[chain_num][0];
							if (chains[chain_num][chain_len] == dep_service_id) {
								chains[chain_num][++chain_len] = dep_dep_service_id;
								dep_dep_service_id_added       = dep_dep_service_id;
								chains[chain_num][0]           = chain_len;
							}
						} else {
							/* required chains were been enlarged by previous iteration, so "-1" */
							chain_len = chains[chain_num][0]-1;
							if (chains[chain_num][chain_len] == dep_service_id && chains[chain_num][chain_len+1] == dep_dep_service_id_added) {
								CHAINS_CHECK_SIZE;
								memcpy(chains[chain_count_new], chains[chain_num], (chain_len+1)*sizeof(**chains));
								chains[chain_count_new][++chain_len] = dep_dep_service_id;
								chains[chain_count_new][0]           = chain_len;
								chain_count_new++;
							}
						}
						chain_num++;
					}
					added_count++;
					chain_count = chain_count_new;
				}
			}
		}

		free(unap_line);
	}

	/* removing non-looping chains */

	{
		int i;

		i = 0;
		while (i < chain_count) {
			int j, chain_len, islooping;

			chain_len = chains[i][0];

			islooping = 0;
			j         = 0;
			while (j++ < chain_len)
				if (chains[i][j] == service_id) {
					islooping = 1;
					break;
				}

			if (!islooping) {
				free(chains[i]);
				chains[i] = chains[--chain_count];
				continue;
			}

			i++;
		}
	}

	/* getting dependencies types */

	{
		int i;
		deptypes = xmalloc(chain_count * sizeof(*deptypes));

		i = 0;
		while (i < chain_count) {
			int j, chain_len;

			chain_len = chains[i][0];

			deptypes[i]    = xmalloc((chain_len+1) * sizeof(**deptypes));

			j     = 0;
			while (j++ < chain_len) {
				service_id_t dep_service_id_from, dep_service_id_to;
				unapm_type_t type;

				dep_service_id_from = j==1 ? service_id : chains[i][j-1];
				dep_service_id_to   =                     chains[i][j];

				type = UNAPM_PROVIDEDBY+1;
				while (type-- > 0) {
					int dep_dep_num, dep_dep_count;
					dep_dep_num   = 0;
					dep_dep_count = unap_matrix[type][dep_service_id_from][0];

					while (dep_dep_num++ < dep_dep_count) {
						if (unap_matrix[type][dep_service_id_from][dep_dep_num] == dep_service_id_to) {
							deptypes[i][j] 	= type;
							type 		= 0;	/* to break parent while (), too */
							break;
						}
					}
				}
			}
			i++;
		}
	}

	/* preparing services chain for further printing */

	if (printwarn || printerr) {
		int chain_num;
		chain_strs = xmalloc(chain_count * sizeof(*chain_strs));

		chain_num = 0;
		while (chain_num < chain_count) {
			char *chain_str, *chain_str_end;
			int chain_len;

			chain_str     = chain_strs[chain_num] = xmalloc(BUFSIZ);
			chain_str_end = &chain_str[BUFSIZ-2];

			/* Preparing a string of services forming the loop */
			{
				char *ptr_dst;
				int i;

				ptr_dst = chain_str;

				chain_len = chains[chain_num][0];
				chains[chain_num][0] = service_id;

				i = chain_len+1;
				while (i--) {
					ENTRY item, **item_pp;
					RC_DEPINFO *depinfo;
					const char *service_name, *ptr_src;

					item.key = (void *)(long)chains[chain_num][i];
					item_pp  = tfind(&item, &svc_id2depinfo_bt, svc_id2depinfo_bt_compare);
					depinfo  = (RC_DEPINFO *)((ENTRY *)*item_pp)->data;

					service_name = depinfo->service;

					ptr_src = service_name;
					while (*ptr_src && (ptr_dst < chain_str_end))
						*(ptr_dst++) = *(ptr_src++);

					if (ptr_dst >= chain_str_end) {
						ptr_dst--;
						break;
					}

					if (i) {
						if (&ptr_dst[4] >= chain_str_end)
							break;

						*(ptr_dst++) = ' ';
						*(ptr_dst++) = deptype2char(deptypes[chain_num][i]);
						memcpy(ptr_dst, "> ", 2);
						ptr_dst += 2;
					}
				}

				chains[chain_num][0] = chain_len;

				*ptr_dst = 0;
			}

/*
			{
				int j;
				chain_len = chains[chain_num][0];
				j = 0;
				printf("%i: %i (%i):", service_id, chain_num, chain_len);
				while (j++ < chain_len)
					printf(" %i<%i>", chains[chain_num][j], deptypes[chain_num][j]);
				printf("\n");
			}
*/
			chain_num++;
		}

	}

	/* checking the cost of loop solving (use/after/need) */

	{
		int i;

		minimal_cost = 0;
		i = 0;
		while (i < chain_count) {
			int j, chain_len;
			unapm_type_t chain_cost;

			chain_cost = UNAPM_MAX;

			chain_len = chains[i][0];
			j         = 0;
			while (j++ < chain_len)
				chain_cost = MIN(chain_cost, deptypes[i][j]);

			minimal_cost = MAX(minimal_cost, chain_cost);

/*
			RC_DEPINFO *depinfo;
			ENTRY item, **item_pp;
			item.key = (void *)(long)service_id;
			item_pp  = tfind(&item, &svc_id2depinfo_bt, svc_id2depinfo_bt_compare);
			depinfo  = (RC_DEPINFO *)((ENTRY *)*item_pp)->data;
*/
			if (minimal_cost > UNAPM_AFTER) {
				if (printerr)
					eerror("Found an unresolvable dependency: %s.", chain_strs[i]);
			} else {
				if (printwarn)
					ewarn("Found a solvable dependency loop: %s.", chain_strs[i]);
			}

			i++;
		}
		/*printf("minimal cost: %i\n", minimal_cost);*/

		if (minimal_cost > UNAPM_AFTER)
			return LOOP_UNSOLVABLE;
	}

	/* calculating optimal way to solve the loop and solving it */

	{
		void *btree = NULL;
		uint64_t *idid_to_break;
		int idid_count, idid_to_break_count;
		idid_to_break = xmalloc(chain_count * sizeof(*idid_to_break));

		/* counting a presence of each dependency through all chains */
		{
			int i;

			idid_count = 0;
			i = 0;
			while (i < chain_count) {
				int j, chain_len;

				chain_len = chains[i][0];
				j         = 0;
				while (j++ < chain_len) {
					uint64_t idid;
					service_id_t service_id_from, service_id_to;
					idid_entry_t idid_entry, *idid_entry_p;
					void *tfind_res;

					if (deptypes[i][j] > minimal_cost)	/* we don't break this dependency, skipping */
						continue;

					service_id_from = j>1 ? chains[i][j-1] : service_id;
					service_id_to   =       chains[i][j];

					idid  =  ((uint64_t)service_id_from << bitsizeof(service_id_to)) | service_id_to;

					idid_entry.idid = idid;
					tfind_res       = tfind(&idid_entry, &btree, idid_compare);
					/*printf("A: (%i -> %i) %li: %p: %p\n", service_id_from, service_id_to, idid, tfind_res, tfind_res==NULL?NULL:*(idid_entry_t **)tfind_res);*/
					if (tfind_res == NULL) {
						idid_entry_p       = xmalloc(sizeof(*idid_entry_p));
						idid_entry_p->idid = idid;
						idid_entry_p->data = (void *)1;
						tsearch(idid_entry_p, &btree, idid_compare);
						idid_count++;
					} else {
						idid_entry_p       = *(idid_entry_t **)tfind_res;
						idid_entry_p->data = (void *)((long)idid_entry_p->data + 1);
					}
				}

				i++;
			}
		}

		/* building array of dependencies sorted by descending presence counter */

		{
			int idid_count2;

			idid_btree_builddescarray_counters = xmalloc(idid_count * sizeof(*idid_btree_builddescarray_counters));
			idid_btree_builddescarray_amount   = 0;

			twalk(btree, idid_btree_builddescarray);

			qsort(
				idid_btree_builddescarray_counters,
				idid_btree_builddescarray_amount,
				sizeof(*idid_btree_builddescarray_counters),
				idid_compare_data_desc
			);

			idid_to_break_count = MIN(idid_btree_builddescarray_amount, chain_count);
			idid_count2 = 0;
			while (idid_count2 < idid_to_break_count) {
				idid_to_break[idid_count2] = idid_btree_builddescarray_counters[idid_count2].idid;
				/*printf("B: %li %li\n", idid_counters[idid_count2].idid, (long)idid_counters[idid_count2].data);*/
				idid_count2++;
			}

			free(idid_btree_builddescarray_counters);
		}

		/* solving loops */

		{
			idid_count = 0;
			while (idid_count < idid_to_break_count) {
				service_id_t service_id_from, service_id_to;

				service_id_from = ((uint64_t)idid_to_break[idid_count] & ((uint64_t)((service_id_t)~0) << bitsizeof(service_id_t))) >> bitsizeof(service_id_t);
				service_id_to   =            idid_to_break[idid_count] &            ((service_id_t)~0);

				idid_count++;

				{
					ENTRY item, **item_pp;
					RC_DEPINFO *depinfo_from = NULL, *depinfo_to = NULL;

					if (printwarn) {
						item.key     = (void *)(long)service_id_from;
						item_pp      = tfind(&item, &svc_id2depinfo_bt, svc_id2depinfo_bt_compare);
						depinfo_from = (RC_DEPINFO *)((ENTRY *)*item_pp)->data;

						item.key     = (void *)(long)service_id_to;
						item_pp      = tfind(&item, &svc_id2depinfo_bt, svc_id2depinfo_bt_compare);
						depinfo_to   = (RC_DEPINFO *)((ENTRY *)*item_pp)->data;
					}

					/* Remove weak dependency */

					remove_loopdependency(unap_matrix, service_id_from, service_id_to, depinfo_from, depinfo_to, "iuse",   UNAPM_USE);
					remove_loopdependency(unap_matrix, service_id_from, service_id_to, depinfo_from, depinfo_to, "iafter", UNAPM_AFTER);

				}
			}
		}

		/* cleanup */

		tdestroy(btree, free);
		free(idid_to_break);
	}

	/* cleanup */
	{
		int i;

		i = 0;
		while (i < chain_count) {
			if (printwarn || printerr)
				free(chain_strs[i]);
			free(deptypes[i]);
			free(chains[i]);
			i++;
		}
		if (printwarn || printerr)
			free(chain_strs);
		free(deptypes);
		free(chains);
	}

	return LOOP_SOLVABLE;
}

/*! Mixing all dependencies to UNAPM_MIXED and expanding them (with dependencies of dependencies) in UNAPM_MIXED_EXPANDED
 * @param unap_matrix matrixes to scan ways to solve the loop
 * @param useneedafter_count total count of use/need/before/provide relations */
static void
unapm_prepare_mixed(service_id_t **unap_matrix[UNAPM_MAX], unsigned int useneedafter_count) {
	service_id_t service_id;
	int onemorecycle;

	/* getting pre-matrix of all dependencies types together */

	service_id = 1;
	while (service_id < (useneedafter_count+1)) {
		service_id_t services_dst;
		unapm_type_t unapm_type;

		memset(unap_matrix[UNAPM_MIXED][service_id], 0, (useneedafter_count+1) * sizeof(**unap_matrix));
		services_dst = 0;
		unapm_type = 0;
		while (unapm_type < UNAPM_MIXED) {
			service_id_t services_src;

			services_src = unap_matrix[unapm_type][service_id][0];
			while (services_src)  {
				unap_matrix[UNAPM_MIXED][service_id][ ++services_dst ] =
					unap_matrix[unapm_type][service_id][ services_src-- ];
			}
			unapm_type++;
		}
		unap_matrix[UNAPM_MIXED][service_id][0] = services_dst;
		service_id++;
	}

	/* preparing full dependencies matrix */

	/* copying UNAPM_MIXED -> UNAPM_MIXED_EXPANDED */
	service_id = 1;
	while (service_id < (useneedafter_count+1)) {
		memcpy(unap_matrix[UNAPM_MIXED_EXPANDED][service_id], unap_matrix[UNAPM_MIXED][service_id], (useneedafter_count+1) * sizeof(**unap_matrix));
		service_id++;
	}

	do {
		onemorecycle = 0;

		/* direct way: service_id = 1 -> end */
		service_id = 1;
		while (service_id < (useneedafter_count+1))
			onemorecycle += unapm_expandsdeps(unap_matrix[UNAPM_MIXED_EXPANDED], service_id++);

		/* reverse way: service_id = end -> 1 */
		while (--service_id)
			onemorecycle += unapm_expandsdeps(unap_matrix[UNAPM_MIXED_EXPANDED], service_id);

	} while (onemorecycle);

	return;
}

void
rc_depsolver_tryfix (
	RC_DEPTREE *deptree,
	unsigned int useneedafter_count,
	RC_DT_FLAGS flags)
{
	RC_DEPINFO *depinfo = NULL;
	int loopfound;
	unapm_type_t unapm_type;
	service_id_t **unap_matrix[UNAPM_MAX];
	service_id_t service_id;
	void *svc_id2depinfo_bt = NULL;
	int loopsolver_counter = 0;

	hcreate(useneedafter_count*2);

	unapm_type = 0;
	while (unapm_type < UNAPM_MAX)
		unap_matrix[unapm_type++] = xmalloc(sizeof(*unap_matrix) * (useneedafter_count+1));

	/* preparing a hash-table: service_name -> service_id */
	service_id = 1;
	TAILQ_FOREACH(depinfo, deptree, entries) {
		ENTRY item, *item_p;

		item.key     = depinfo->service;
		item.data    = (void *)(long int)service_id;
		hsearch(item, ENTER);

		item_p       = xmalloc(sizeof(*item_p));
		item_p->key  = (void *)(long int)service_id;
		item_p->data = depinfo;
		tsearch(item_p, &svc_id2depinfo_bt, svc_id2depinfo_bt_compare);

		service_id++;
	}

	unap_matrix[0][0]    = xmalloc(sizeof(***unap_matrix)*1);
	unap_matrix[0][0][0] = service_id;

	/* getting dependencies pre-matrixes */
	service_id = 1;
	TAILQ_FOREACH(depinfo, deptree, entries) {
		unapm_getdependencies(unap_matrix[UNAPM_USE],         useneedafter_count, service_id, "iuse",       depinfo);
		unapm_getdependencies(unap_matrix[UNAPM_NEED],        useneedafter_count, service_id, "ineed",      depinfo);
		unapm_getdependencies(unap_matrix[UNAPM_PROVIDEDBY],  useneedafter_count, service_id, "providedby", depinfo);
		unapm_getdependencies(unap_matrix[UNAPM_AFTER],       useneedafter_count, service_id, "iafter",     depinfo);
		service_id++;
	}

	hdestroy();

	/* getting pre-matrix of all dependencies types together (allocating memory) */

	service_id = 1;
	while (service_id < (useneedafter_count+1)) {
		unap_matrix[UNAPM_MIXED         ][service_id] = xmalloc((useneedafter_count+1) * sizeof(**unap_matrix));
		unap_matrix[UNAPM_MIXED_EXPANDED][service_id] = xmalloc((useneedafter_count+1) * sizeof(**unap_matrix));
		service_id++;
	}

	do {
		loopfound = 0;

		/* updating UNAPM_MIXED and UNAPM_MIXED_EXPANDED in UNAP matrix */

		unapm_prepare_mixed(unap_matrix, useneedafter_count);

		/* detecting and solving loop (non-recursive method) */
		/* the loop is a situation where service is depended on itself */
		service_id=1;
		while ((service_id < (useneedafter_count+1)) && !loopfound) {
			int dep_num, dep_count;

			dep_num = 0;
			dep_count = unap_matrix[UNAPM_MIXED_EXPANDED][service_id][0];
			while (dep_num < dep_count) {
				dep_num++;
				if (unap_matrix[UNAPM_MIXED_EXPANDED][service_id][dep_num] == service_id) {
					loopfound = solve_loop(unap_matrix, service_id, svc_id2depinfo_bt, dep_num, flags);
					loopsolver_counter++;
					break;
				}
			}

			service_id++;
		}
	} while (loopfound == LOOP_SOLVABLE && loopsolver_counter < LOOPSOLVER_LIMIT);

	if (loopsolver_counter >= LOOPSOLVER_LIMIT)
		eerror("Dependency loop solver reached iterations limit.");

	/* clean up */

	unapm_type = 0;
	while (unapm_type < UNAPM_MAX) {
		service_id=1;
		while (service_id < (useneedafter_count+1))
			free(unap_matrix[unapm_type][service_id++]);
		free(unap_matrix[unapm_type++]);
	}

	tdestroy(svc_id2depinfo_bt, free);

	return;
}
