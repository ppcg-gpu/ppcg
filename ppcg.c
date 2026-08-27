/*
 * Copyright 2011      INRIA Saclay
 * Copyright 2013      Ecole Normale Superieure
 * Copyright 2015      Sven Verdoolaege
 *
 * Use of this software is governed by the MIT license
 *
 * Written by Sven Verdoolaege, INRIA Saclay - Ile-de-France,
 * Parc Club Orsay Universite, ZAC des vignes, 4 rue Jacques Monod,
 * 91893 Orsay, France
 * and Ecole Normale Superieure, 45 rue d'Ulm, 75230 Paris, France
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <isl/ctx.h>
#include <isl/id.h>
#include <isl/val.h>
#include <isl/set.h>
#include <isl/union_set.h>
#include <isl/union_map.h>
#include <isl/aff.h>
#include <isl/flow.h>
#include <isl/options.h>
#include <isl/schedule.h>
#include <isl/ast.h>
#include <isl/id_to_ast_expr.h>
#include <isl/ast_build.h>
#include <isl/schedule.h>
#include <pet.h>
#include "ppcg.h"
#include "reduction.h"
#include "ppcg_options.h"
#include "cuda.h"
#include "opencl.h"
#include "cpu.h"


/* Return a pointer to the final path component of "filename" or
 * to "filename" itself if it does not contain any components.
 */
const char *ppcg_base_name(const char *filename)
{
	const char *base;

	base = strrchr(filename, '/');
	if (base)
		return ++base;
	else
		return filename;
}

/* Copy the base name of "input" to "name" and return its length.
 * "name" is not NULL terminated.
 *
 * In particular, remove all leading directory components and
 * the final extension, if any.
 */
int ppcg_extract_base_name(char *name, const char *input)
{
	const char *base;
	const char *ext;
	int len;

	base = ppcg_base_name(input);
	ext = strrchr(base, '.');
	len = ext ? ext - base : strlen(base);

	memcpy(name, base, len);

	return len;
}

/* Does "scop" refer to any arrays that are declared, but not
 * exposed to the code after the scop?
 */
int ppcg_scop_any_hidden_declarations(struct ppcg_scop *scop)
{
	int i;

	if (!scop)
		return 0;

	for (i = 0; i < scop->pet->n_array; ++i)
		if (scop->pet->arrays[i]->declared &&
		    !scop->pet->arrays[i]->exposed)
			return 1;

	return 0;
}

/* Collect all variable names that are in use in "scop".
 * In particular, collect all parameters in the context and
 * all the array names.
 * Store these names in an isl_id_to_ast_expr by mapping
 * them to a dummy value (0).
 */
static __isl_give isl_id_to_ast_expr *collect_names(struct pet_scop *scop)
{
	int i, n;
	isl_ctx *ctx;
	isl_ast_expr *zero;
	isl_id_to_ast_expr *names;

	ctx = isl_set_get_ctx(scop->context);

	n = isl_set_dim(scop->context, isl_dim_param);

	names = isl_id_to_ast_expr_alloc(ctx, n + scop->n_array);
	zero = isl_ast_expr_from_val(isl_val_zero(ctx));

	for (i = 0; i < n; ++i) {
		isl_id *id;

		id = isl_set_get_dim_id(scop->context, isl_dim_param, i);
		names = isl_id_to_ast_expr_set(names,
						id, isl_ast_expr_copy(zero));
	}

	for (i = 0; i < scop->n_array; ++i) {
		struct pet_array *array = scop->arrays[i];
		isl_id *id;

		id = isl_set_get_tuple_id(array->extent);
		names = isl_id_to_ast_expr_set(names,
						id, isl_ast_expr_copy(zero));
	}

	isl_ast_expr_free(zero);

	return names;
}

/* Return an isl_id called "prefix%d", with "%d" set to "i".
 * If an isl_id with such a name already appears among the variable names
 * of "scop", then adjust the name to "prefix%d_%d".
 */
static __isl_give isl_id *generate_name(struct ppcg_scop *scop,
	const char *prefix, int i)
{
	int j;
	char name[23];
	isl_ctx *ctx;
	isl_id *id;
	int has_name;

	ctx = isl_set_get_ctx(scop->context);
	snprintf(name, sizeof(name), "%s%d", prefix, i);
	id = isl_id_alloc(ctx, name, NULL);

	j = 0;
	while ((has_name = isl_id_to_ast_expr_has(scop->names, id)) == 1) {
		isl_id_free(id);
		snprintf(name, sizeof(name), "%s%d_%d", prefix, i, j++);
		id = isl_id_alloc(ctx, name, NULL);
	}

	return has_name < 0 ? isl_id_free(id) : id;
}

/* Return a list of "n" isl_ids of the form "prefix%d".
 * If an isl_id with such a name already appears among the variable names
 * of "scop", then adjust the name to "prefix%d_%d".
 */
__isl_give isl_id_list *ppcg_scop_generate_names(struct ppcg_scop *scop,
	int n, const char *prefix)
{
	int i;
	isl_ctx *ctx;
	isl_id_list *names;

	ctx = isl_set_get_ctx(scop->context);
	names = isl_id_list_alloc(ctx, n);
	for (i = 0; i < n; ++i) {
		isl_id *id;

		id = generate_name(scop, prefix, i);
		names = isl_id_list_add(names, id);
	}

	return names;
}

/* Is "stmt" not a kill statement?
 */
static int is_not_kill(struct pet_stmt *stmt)
{
	return !pet_stmt_is_kill(stmt);
}

/* Collect the iteration domains of the statements in "scop" that
 * satisfy "pred".
 */
static __isl_give isl_union_set *collect_domains(struct pet_scop *scop,
	int (*pred)(struct pet_stmt *stmt))
{
	int i;
	isl_set *domain_i;
	isl_union_set *domain;

	if (!scop)
		return NULL;

	domain = isl_union_set_empty(isl_set_get_space(scop->context));

	for (i = 0; i < scop->n_stmt; ++i) {
		struct pet_stmt *stmt = scop->stmts[i];

		if (!pred(stmt))
			continue;

		if (stmt->n_arg > 0)
			isl_die(isl_union_set_get_ctx(domain),
				isl_error_unsupported,
				"data dependent conditions not supported",
				return isl_union_set_free(domain));

		domain_i = isl_set_copy(scop->stmts[i]->domain);
		domain = isl_union_set_add_set(domain, domain_i);
	}

	return domain;
}

/* Collect the iteration domains of the statements in "scop",
 * skipping kill statements.
 */
static __isl_give isl_union_set *collect_non_kill_domains(struct pet_scop *scop)
{
	return collect_domains(scop, &is_not_kill);
}

/* This function is used as a callback to pet_expr_foreach_call_expr
 * to detect if there is any call expression in the input expression.
 * Assign the value 1 to the integer that "user" points to and
 * abort the search since we have found what we were looking for.
 */
static int set_has_call(__isl_keep pet_expr *expr, void *user)
{
	int *has_call = user;

	*has_call = 1;

	return -1;
}

/* Does "expr" contain any call expressions?
 */
static int expr_has_call(__isl_keep pet_expr *expr)
{
	int has_call = 0;

	if (pet_expr_foreach_call_expr(expr, &set_has_call, &has_call) < 0 &&
	    !has_call)
		return -1;

	return has_call;
}

/* This function is a callback for pet_tree_foreach_expr.
 * If "expr" contains any call (sub)expressions, then set *has_call
 * and abort the search.
 */
static int check_call(__isl_keep pet_expr *expr, void *user)
{
	int *has_call = user;

	if (expr_has_call(expr))
		*has_call = 1;

	return *has_call ? -1 : 0;
}

/* Does "stmt" contain any call expressions?
 */
static int has_call(struct pet_stmt *stmt)
{
	int has_call = 0;

	if (pet_tree_foreach_expr(stmt->body, &check_call, &has_call) < 0 &&
	    !has_call)
		return -1;

	return has_call;
}

/* Collect the iteration domains of the statements in "scop"
 * that contain a call expression.
 */
static __isl_give isl_union_set *collect_call_domains(struct pet_scop *scop)
{
	return collect_domains(scop, &has_call);
}

/* Is "stmt" a return statement?
 */
static int is_return(struct pet_stmt *stmt)
{
	return pet_tree_get_type(stmt->body) == pet_tree_return;
}

/* Collect the iteration domains of the return statements in "scop".
 */
static __isl_give isl_union_set *collect_return_domains(struct pet_scop *scop)
{
	return collect_domains(scop, &is_return);
}

/* Given a union of "tagged" access relations of the form
 *
 *	[S_i[...] -> R_j[]] -> A_k[...]
 *
 * project out the "tags" (R_j[]).
 * That is, return a union of relations of the form
 *
 *	S_i[...] -> A_k[...]
 */
static __isl_give isl_union_map *project_out_tags(
	__isl_take isl_union_map *umap)
{
	return isl_union_map_domain_factor_domain(umap);
}

/* Construct a function from tagged iteration domains to the corresponding
 * untagged iteration domains with as range of the wrapped map in the domain
 * the reference tags that appear in any of the reads, writes or kills.
 * Store the result in ps->tagger.
 *
 * For example, if the statement with iteration space S[i,j]
 * contains two array references R_1[] and R_2[], then ps->tagger will contain
 *
 *	{ [S[i,j] -> R_1[]] -> S[i,j]; [S[i,j] -> R_2[]] -> S[i,j] }
 */
static void compute_tagger(struct ppcg_scop *ps)
{
	isl_union_map *tagged;
	isl_union_pw_multi_aff *tagger;

	tagged = isl_union_map_copy(ps->tagged_reads);
	tagged = isl_union_map_union(tagged,
				isl_union_map_copy(ps->tagged_may_writes));
	tagged = isl_union_map_union(tagged,
				isl_union_map_copy(ps->tagged_must_kills));
	tagged = isl_union_map_universe(tagged);
	tagged = isl_union_set_unwrap(isl_union_map_domain(tagged));

	tagger = isl_union_map_domain_map_union_pw_multi_aff(tagged);

	ps->tagger = tagger;
}

/* Compute the live out accesses, i.e., the writes that are
 * potentially not killed by any kills or any other writes, and
 * store them in ps->live_out.
 *
 * We compute the "dependence" of any "kill" (an explicit kill
 * or a must write) on any may write.
 * The elements accessed by the may writes with a "depending" kill
 * also accessing the element are definitely killed.
 * The remaining may writes can potentially be live out.
 *
 * The result of the dependence analysis is
 *
 *	{ IW -> [IK -> A] }
 *
 * with IW the instance of the write statement, IK the instance of kill
 * statement and A the element that was killed.
 * The range factor range is
 *
 *	{ IW -> A }
 *
 * containing all such pairs for which there is a kill statement instance,
 * i.e., all pairs that have been killed.
 */
static void compute_live_out(struct ppcg_scop *ps)
{
	isl_schedule *schedule;
	isl_union_map *kills;
	isl_union_map *exposed;
	isl_union_map *covering;
	isl_union_access_info *access;
	isl_union_flow *flow;

	/* THE PLAIN WRITES, NOT THE COMPOSED ONES.
	 *
	 * An annotation composes every member of a storage group onto one
	 * representative array, which is right for the dependence analysis
	 * and wrong here: subtracting kills from may-writes over the
	 * composed relations lets a later member's write cover an earlier
	 * member's write, and the earlier computation is classified dead
	 * even though the caller can still reach it through the array that
	 * wrote it.  Measured on the 402-node scop: nine of the eleven
	 * parameters losing writes were the representatives the pragmas
	 * named first, and the scheduler died in "unable to carry
	 * dependences" with zero bands.  Liveness is a property of what the
	 * caller sees through each array, so it is judged over the arrays
	 * the source names.
	 */
	schedule = isl_schedule_copy(ps->schedule);
	kills = isl_union_map_union(isl_union_map_copy(ps->plain_must_writes),
				    isl_union_map_copy(ps->must_kills));
	access = isl_union_access_info_from_sink(kills);
	access = isl_union_access_info_set_may_source(access,
				    isl_union_map_copy(ps->plain_may_writes));
	access = isl_union_access_info_set_schedule(access, schedule);
	flow = isl_union_access_info_compute_flow(access);
	covering = isl_union_flow_get_full_may_dependence(flow);
	isl_union_flow_free(flow);

	covering = isl_union_map_range_factor_range(covering);
	exposed = isl_union_map_copy(ps->plain_may_writes);
	exposed = isl_union_map_subtract(exposed, covering);
	ps->live_out = exposed;
}

/* Compute the tagged flow dependences and the live_in accesses and store
 * the results in ps->tagged_dep_flow and ps->live_in.
 *
 * Both must-writes and must-kills are allowed to kill dependences
 * from earlier writes to subsequent reads.
 * The must-kills are not included in the potential sources, though.
 * The flow dependences with a must-kill as source would
 * reflect possibly uninitialized reads.
 * No dependences need to be introduced to protect such reads
 * (other than those imposed by potential flows from may writes
 * that follow the kill).  Those flow dependences are therefore not needed.
 * The dead code elimination also assumes
 * the flow sources are non-kill instances.
 */
static void compute_tagged_flow_dep_only(struct ppcg_scop *ps)
{
	isl_union_pw_multi_aff *tagger;
	isl_schedule *schedule;
	isl_union_map *live_in;
	isl_union_access_info *access;
	isl_union_flow *flow;
	isl_union_map *must_source;
	isl_union_map *kills;
	isl_union_map *tagged_flow;

	tagger = isl_union_pw_multi_aff_copy(ps->tagger);
	schedule = isl_schedule_copy(ps->schedule);
	schedule = isl_schedule_pullback_union_pw_multi_aff(schedule, tagger);
	kills = isl_union_map_copy(ps->tagged_must_kills);
	must_source = isl_union_map_copy(ps->tagged_must_writes);
	kills = isl_union_map_union(kills, must_source);
	access = isl_union_access_info_from_sink(
				isl_union_map_copy(ps->tagged_reads));
	access = isl_union_access_info_set_kill(access, kills);
	access = isl_union_access_info_set_may_source(access,
				isl_union_map_copy(ps->tagged_may_writes));
	access = isl_union_access_info_set_schedule(access, schedule);
	flow = isl_union_access_info_compute_flow(access);
	tagged_flow = isl_union_flow_get_may_dependence(flow);
	ps->tagged_dep_flow = tagged_flow;
	live_in = isl_union_flow_get_may_no_source(flow);
	ps->live_in = project_out_tags(live_in);
	isl_union_flow_free(flow);
}

/* Compute ps->dep_flow from ps->tagged_dep_flow
 * by projecting out the reference tags.
 */
static void derive_flow_dep_from_tagged_flow_dep(struct ppcg_scop *ps)
{
	ps->dep_flow = isl_union_map_copy(ps->tagged_dep_flow);
	ps->dep_flow = isl_union_map_factor_domain(ps->dep_flow);
}

/* Compute the flow dependences and the live_in accesses and store
 * the results in ps->dep_flow and ps->live_in.
 * A copy of the flow dependences, tagged with the reference tags
 * is stored in ps->tagged_dep_flow.
 *
 * We first compute ps->tagged_dep_flow, i.e., the tagged flow dependences
 * and then project out the tags.
 */
static void compute_tagged_flow_dep(struct ppcg_scop *ps)
{
	compute_tagged_flow_dep_only(ps);
	derive_flow_dep_from_tagged_flow_dep(ps);
}

/* Compute the order dependences that prevent the potential live ranges
 * from overlapping.
 *
 * In particular, construct a union of relations
 *
 *	[R[...] -> R_1[]] -> [W[...] -> R_2[]]
 *
 * where [R[...] -> R_1[]] is the range of one or more live ranges
 * (i.e., a read) and [W[...] -> R_2[]] is the domain of one or more
 * live ranges (i.e., a write).  Moreover, the read and the write
 * access the same memory element and the read occurs before the write
 * in the original schedule.
 * The scheduler allows some of these dependences to be violated, provided
 * the adjacent live ranges are all local (i.e., their domain and range
 * are mapped to the same point by the current schedule band).
 *
 * Note that if a live range is not local, then we need to make
 * sure it does not overlap with _any_ other live range, and not
 * just with the "previous" and/or the "next" live range.
 * We therefore add order dependences between reads and
 * _any_ later potential write.
 *
 * We also need to be careful about writes without a corresponding read.
 * They are already prevented from moving past non-local preceding
 * intervals, but we also need to prevent them from moving past non-local
 * following intervals.  We therefore also add order dependences from
 * potential writes that do not appear in any intervals
 * to all later potential writes.
 * Note that dead code elimination should have removed most of these
 * dead writes, but the dead code elimination may not remove all dead writes,
 * so we need to consider them to be safe.
 *
 * The order dependences are computed by computing the "dataflow"
 * from the above unmatched writes and the reads to the may writes.
 * The unmatched writes and the reads are treated as may sources
 * such that they would not kill order dependences from earlier
 * such writes and reads.
 */
static void compute_order_dependences(struct ppcg_scop *ps)
{
	isl_union_map *reads;
	isl_union_map *shared_access;
	isl_union_set *matched;
	isl_union_map *unmatched;
	isl_union_pw_multi_aff *tagger;
	isl_schedule *schedule;
	isl_union_access_info *access;
	isl_union_flow *flow;

	tagger = isl_union_pw_multi_aff_copy(ps->tagger);
	schedule = isl_schedule_copy(ps->schedule);
	schedule = isl_schedule_pullback_union_pw_multi_aff(schedule, tagger);
	reads = isl_union_map_copy(ps->tagged_reads);
	matched = isl_union_map_domain(isl_union_map_copy(ps->tagged_dep_flow));
	unmatched = isl_union_map_copy(ps->tagged_may_writes);
	unmatched = isl_union_map_subtract_domain(unmatched, matched);
	reads = isl_union_map_union(reads, unmatched);
	access = isl_union_access_info_from_sink(
				isl_union_map_copy(ps->tagged_may_writes));
	access = isl_union_access_info_set_may_source(access, reads);
	access = isl_union_access_info_set_schedule(access, schedule);
	flow = isl_union_access_info_compute_flow(access);
	shared_access = isl_union_flow_get_may_dependence(flow);
	isl_union_flow_free(flow);

	ps->tagged_dep_order = isl_union_map_copy(shared_access);
	ps->dep_order = isl_union_map_factor_domain(shared_access);
}

/* Compute those validity dependences of the program represented by "scop"
 * that should be unconditionally enforced even when live-range reordering
 * is used.
 *
 * In particular, compute the external false dependences
 * as well as order dependences between sources with the same sink.
 * The anti-dependences are already taken care of by the order dependences.
 * The external false dependences are only used to ensure that live-in and
 * live-out data is not overwritten by any writes inside the scop.
 * The independences are removed from the external false dependences,
 * but not from the order dependences between sources with the same sink.
 *
 * In particular, the reads from live-in data need to precede any
 * later write to the same memory element.
 * As to live-out data, the last writes need to remain the last writes.
 * That is, any earlier write in the original schedule needs to precede
 * the last write to the same memory element in the computed schedule.
 * The possible last writes have been computed by compute_live_out.
 * They may include kills, but if the last access is a kill,
 * then the corresponding dependences will effectively be ignored
 * since we do not schedule any kill statements.
 *
 * Note that the set of live-in and live-out accesses may be
 * an overapproximation.  There may therefore be potential writes
 * before a live-in access and after a live-out access.
 *
 * In the presence of may-writes, there may be multiple live-ranges
 * with the same sink, accessing the same memory element.
 * The sources of these live-ranges need to be executed
 * in the same relative order as in the original program
 * since we do not know which of the may-writes will actually
 * perform a write.  Consider all sources that share a sink and
 * that may write to the same memory element and compute
 * the order dependences among them.
 */
static void compute_forced_dependences(struct ppcg_scop *ps)
{
	isl_union_map *shared_access;
	isl_union_map *exposed;
	isl_union_map *live_in;
	isl_union_map *sink_access;
	isl_union_map *shared_sink;
	isl_union_access_info *access;
	isl_union_flow *flow;
	isl_schedule *schedule;

	exposed = isl_union_map_copy(ps->live_out);
	schedule = isl_schedule_copy(ps->schedule);
	access = isl_union_access_info_from_sink(exposed);
	access = isl_union_access_info_set_may_source(access,
				isl_union_map_copy(ps->may_writes));
	access = isl_union_access_info_set_schedule(access, schedule);
	flow = isl_union_access_info_compute_flow(access);
	shared_access = isl_union_flow_get_may_dependence(flow);
	isl_union_flow_free(flow);
	ps->dep_forced = shared_access;

	schedule = isl_schedule_copy(ps->schedule);
	access = isl_union_access_info_from_sink(
				isl_union_map_copy(ps->may_writes));
	access = isl_union_access_info_set_may_source(access,
				isl_union_map_copy(ps->live_in));
	access = isl_union_access_info_set_schedule(access, schedule);
	flow = isl_union_access_info_compute_flow(access);
	live_in = isl_union_flow_get_may_dependence(flow);
	isl_union_flow_free(flow);

	ps->dep_forced = isl_union_map_union(ps->dep_forced, live_in);
	ps->dep_forced = isl_union_map_subtract(ps->dep_forced,
				isl_union_map_copy(ps->independence));

	schedule = isl_schedule_copy(ps->schedule);
	sink_access = isl_union_map_copy(ps->tagged_dep_flow);
	sink_access = isl_union_map_range_product(sink_access,
				isl_union_map_copy(ps->tagged_may_writes));
	sink_access = isl_union_map_domain_factor_domain(sink_access);
	access = isl_union_access_info_from_sink(
				isl_union_map_copy(sink_access));
	access = isl_union_access_info_set_may_source(access, sink_access);
	access = isl_union_access_info_set_schedule(access, schedule);
	flow = isl_union_access_info_compute_flow(access);
	shared_sink = isl_union_flow_get_may_dependence(flow);
	isl_union_flow_free(flow);
	ps->dep_forced = isl_union_map_union(ps->dep_forced, shared_sink);
}

/* Remove independence from the tagged flow dependences.
 * Since the user has guaranteed that source and sink of an independence
 * can be executed in any order, there cannot be a flow dependence
 * between them, so they can be removed from the set of flow dependences.
 * However, if the source of such a flow dependence is a must write,
 * then it may have killed other potential sources, which would have
 * to be recovered if we were to remove those flow dependences.
 * We therefore keep the flow dependences that originate in a must write,
 * even if it corresponds to a known independence.
 */
static void remove_independences_from_tagged_flow(struct ppcg_scop *ps)
{
	isl_union_map *tf;
	isl_union_set *indep;
	isl_union_set *mw;

	tf = isl_union_map_copy(ps->tagged_dep_flow);
	tf = isl_union_map_zip(tf);
	indep = isl_union_map_wrap(isl_union_map_copy(ps->independence));
	tf = isl_union_map_intersect_domain(tf, indep);
	tf = isl_union_map_zip(tf);
	mw = isl_union_map_domain(isl_union_map_copy(ps->tagged_must_writes));
	tf = isl_union_map_subtract_domain(tf, mw);
	ps->tagged_dep_flow = isl_union_map_subtract(ps->tagged_dep_flow, tf);
}

/* Compute the dependences of the program represented by "scop"
 * in case live range reordering is allowed.
 *
 * We compute the actual live ranges and the corresponding order
 * false dependences.
 *
 * The independences are removed from the flow dependences
 * (provided the source is not a must-write) as well as
 * from the external false dependences (by compute_forced_dependences).
 */
static void compute_live_range_reordering_dependences(struct ppcg_scop *ps)
{
	compute_tagged_flow_dep_only(ps);
	remove_independences_from_tagged_flow(ps);
	derive_flow_dep_from_tagged_flow_dep(ps);
	compute_order_dependences(ps);
	compute_forced_dependences(ps);
}

/* Compute the potential flow dependences and the potential live in
 * accesses.
 *
 * Both must-writes and must-kills are allowed to kill dependences
 * from earlier writes to subsequent reads, as in compute_tagged_flow_dep_only.
 */
static void compute_flow_dep(struct ppcg_scop *ps)
{
	isl_union_access_info *access;
	isl_union_flow *flow;
	isl_union_map *kills, *must_writes;

	access = isl_union_access_info_from_sink(isl_union_map_copy(ps->reads));
	kills = isl_union_map_copy(ps->must_kills);
	must_writes = isl_union_map_copy(ps->must_writes);
	kills = isl_union_map_union(kills, must_writes);
	access = isl_union_access_info_set_kill(access, kills);
	access = isl_union_access_info_set_may_source(access,
				isl_union_map_copy(ps->may_writes));
	access = isl_union_access_info_set_schedule(access,
				isl_schedule_copy(ps->schedule));
	flow = isl_union_access_info_compute_flow(access);

	ps->dep_flow = isl_union_flow_get_may_dependence(flow);
	ps->live_in = isl_union_flow_get_may_no_source(flow);
	isl_union_flow_free(flow);
}

/* Compute the dependences of the program represented by "scop".
 * Store the computed potential flow dependences
 * in scop->dep_flow and the reads with potentially no corresponding writes in
 * scop->live_in.
 * Store the potential live out accesses in scop->live_out.
 * Store the potential false (anti and output) dependences in scop->dep_false.
 *
 * If live range reordering is allowed, then we compute a separate
 * set of order dependences and a set of external false dependences
 * in compute_live_range_reordering_dependences.
 */
static void compute_dependences(struct ppcg_scop *scop)
{
	isl_union_map *may_source;
	isl_union_access_info *access;
	isl_union_flow *flow;

	if (!scop)
		return;

	compute_live_out(scop);

	if (scop->options->live_range_reordering)
		compute_live_range_reordering_dependences(scop);
	else if (scop->options->target != PPCG_TARGET_C)
		compute_tagged_flow_dep(scop);
	else
		compute_flow_dep(scop);

	may_source = isl_union_map_union(isl_union_map_copy(scop->may_writes),
					isl_union_map_copy(scop->reads));
	access = isl_union_access_info_from_sink(
				isl_union_map_copy(scop->may_writes));
	access = isl_union_access_info_set_kill(access,
				isl_union_map_copy(scop->must_writes));
	access = isl_union_access_info_set_may_source(access, may_source);
	access = isl_union_access_info_set_schedule(access,
				isl_schedule_copy(scop->schedule));
	flow = isl_union_access_info_compute_flow(access);

	scop->dep_false = isl_union_flow_get_may_dependence(flow);
	scop->dep_false = isl_union_map_coalesce(scop->dep_false);
	isl_union_flow_free(flow);

	/* An accumulation orders its own iterations and nothing else, and
	 * the result does not depend on that order.  Dropping those
	 * dependences is what lets the loop be run in parallel; the C
	 * backend then names the accumulators in a clause that combines
	 * the partial results.  No other backend does that yet, so no
	 * other target may have them dropped.
	 */
	if (scop->options->target == PPCG_TARGET_C) {
		scop->reductions = ppcg_find_reductions(scop);
		scop->reduction_deps =
			ppcg_reduction_dependences(scop, scop->reductions);
	}

	/* THE RELATIONS AN ORDER DEPENDENCE IS BUILT FROM, on request.
	 *
	 * dep_false takes may_writes as its sink and may_writes union reads
	 * as its source, so a missing anti-dependence is a missing element
	 * of one of exactly four relations.  Printing them beside the result
	 * is the difference between naming the slot and guessing it.
	 */
	if (getenv("PPCG_DEBUG_DEPS")) {
		fprintf(stderr, "reads       : ");
		isl_union_map_dump(scop->reads);
		fprintf(stderr, "may_writes  : ");
		isl_union_map_dump(scop->may_writes);
		fprintf(stderr, "tagged_reads: ");
		isl_union_map_dump(scop->tagged_reads);
		fprintf(stderr, "tagged_mayw : ");
		isl_union_map_dump(scop->tagged_may_writes);
		fprintf(stderr, "dep_false   : ");
		isl_union_map_dump(scop->dep_false);
	}
}

/* Report an empty context, meaning that the original code
 * cannot not be executed.
 *
 * Make a distinction between whether the original context
 * was already empty or whether the current context
 * (with additional constraints specified by the user) is empty.
 */
static void report_empty_context(struct ppcg_scop *ps)
{
	isl_bool empty;

	if (!ps->options->debug->verbose)
		return;
	empty = isl_set_is_empty(ps->pet->context);
	if (empty < 0)
		return;
	if (empty) {
		fprintf(stdout, "Original code cannot be executed "
			"under any conditions\n");
		return;
	}
	empty = isl_set_is_empty(ps->context);
	if (empty < 0)
		return;
	if (!empty)
		return;
	fprintf(stdout, "Original code cannot be executed "
		"under specified conditions\n");
}

/* Report the eliminated dead code, and refuse to continue unless the
 * caller has said that losing statements is acceptable.
 *
 * NOTHING LEAVES A SCOP WITHOUT A WORD.  This used to print under --verbose
 * and to stdout, which is a comment rather than a diagnostic: a run that
 * dropped work looked exactly like a run that did not.  Measured twice in
 * one session on the same tree.  A union member reached through a pointer
 * lost the store into it -- "Eliminated dead instances: { S_2[] }" for a
 * write to the caller's memory -- and a kernel whose windows were typed
 * local pointers lost both of its loops, 8192 instances, while ppcg exited
 * 0 and removed the pragma, so every check that asks "was it scheduled"
 * answered yes.  Both were mismodelled storage, and in both the elimination
 * was the only trace of it.
 *
 * Dead code that is genuinely dead does exist, so this is an option rather
 * than a law -- but the default is to stop, because the cost of stopping is
 * a flag and the cost of continuing is a wrong answer that looks right.
 *
 * Returns 0 if the run may continue and -1 if it may not.
 */
/* Collect the name of the array "set" belongs to.
 */
static isl_stat collect_array_name(__isl_take isl_set *set, void *user)
{
	isl_printer **p = user;
	const char *name = isl_set_get_tuple_name(set);

	*p = isl_printer_print_str(*p, name ? name : "?");
	*p = isl_printer_print_str(*p, " ");
	isl_set_free(set);

	return isl_stat_ok;
}

static int report_dead_code(struct ppcg_scop *ps,
	__isl_keep isl_union_set *live)
{
	isl_ctx *ctx;
	isl_printer *p;
	isl_union_set *dead;
	isl_union_map *lost;
	isl_union_set *arrays;

	if (isl_union_set_is_equal(ps->domain, live))
		return 0;

	ctx = isl_union_set_get_ctx(live);
	dead = isl_union_set_subtract(isl_union_set_copy(ps->domain),
					isl_union_set_copy(live));

	p = isl_printer_to_file(ctx, stderr);
	p = isl_printer_print_str(p, ps->options->stop_on_dead_code ?
			"ppcg: STATEMENTS WOULD BE LOST: " :
			"ppcg: eliminated dead instances: ");
	p = isl_printer_print_union_set(p, dead);
	p = isl_printer_end_line(p);

	/* WHICH ARRAYS LOSE A WRITE, not merely which statements go.
	 *
	 * A list of statement names cannot be classified without mapping it
	 * back through the generated C by hand.  The arrays can be: a write
	 * to a buffer the scop declared itself and never reads again is dead
	 * code, and a write to a buffer that came in as a parameter is a
	 * store into the caller's memory that this tool has decided nobody
	 * wants.  Those are different findings and only the second is a
	 * defect, so the report names them rather than leaving the reader to
	 * count statement numbers.
	 *
	 * OVER THE ARRAYS THE SOURCE NAMES, like the liveness this reports
	 * on.  Read from the composed relations, a dead statement writing
	 * through a member is charged to the storage's REPRESENTATIVE, which
	 * the source never wrote and which loses nothing: the member is in
	 * the report too, judged correctly, and the representative's entry is
	 * the same write wearing another name.  At 402 nodes that phantom put
	 * three representatives in the losses -- 90, 18 and 4 composed writes
	 * against 2, 2 and 1 of their own -- and made the report unreadable
	 * on exactly the question it was added to answer.
	 */
	lost = isl_union_map_intersect_domain(
			isl_union_map_copy(ps->plain_may_writes),
			isl_union_set_copy(dead));
	arrays = isl_union_map_range(lost);
	p = isl_printer_print_str(p, "ppcg: arrays losing a write: ");
	isl_union_set_foreach_set(arrays, &collect_array_name, &p);
	p = isl_printer_end_line(p);
	isl_union_set_free(arrays);

	if (ps->options->stop_on_dead_code) {
		p = isl_printer_print_str(p, "ppcg: these instances write to "
			"storage nothing later reads, and --stop-on-dead-code "
			"says this scop should lose nothing, so it stops.");
		p = isl_printer_end_line(p);
	}
	isl_printer_free(p);

	isl_union_set_free(dead);

	return ps->options->stop_on_dead_code ? -1 : 0;
}

/* The statements a scop is made of, and which of them reach a root.
 *
 * "id" holds one isl_id per statement, and isl hands out one id per
 * name per context, so two ids are the same statement exactly when
 * they are the same pointer.  "reached" says whether the statement has
 * been found to feed something live, and "queue" is the breadth first
 * frontier as indices into "id".
 *
 * "edge_from" and "edge_to" are the dependence graph with one vertex
 * per statement rather than one per statement instance.
 */
struct ppcg_stmt_graph {
	isl_ctx *ctx;
	int n;
	int n_alloc;
	isl_id **id;
	int *reached;
	int *queue;
	int n_queue;

	int n_edge;
	int n_edge_alloc;
	int *edge_from;
	int *edge_to;
};

/* Return the index of "id" in "g", adding it if it is not there yet,
 * or -1 on error.  "id" is consumed.
 */
static int stmt_index(struct ppcg_stmt_graph *g, __isl_take isl_id *id)
{
	int i;

	if (!id)
		return -1;
	for (i = 0; i < g->n; ++i)
		if (g->id[i] == id) {
			isl_id_free(id);
			return i;
		}
	if (g->n == g->n_alloc) {
		int n_alloc = g->n_alloc ? 2 * g->n_alloc : 64;
		isl_id **id_new = realloc(g->id, n_alloc * sizeof(*id_new));
		int *reached = realloc(g->reached, n_alloc * sizeof(*reached));
		int *queue = realloc(g->queue, n_alloc * sizeof(*queue));

		if (id_new)
			g->id = id_new;
		if (reached)
			g->reached = reached;
		if (queue)
			g->queue = queue;
		if (!id_new || !reached || !queue) {
			isl_id_free(id);
			return -1;
		}
		g->n_alloc = n_alloc;
	}
	g->id[g->n] = id;
	g->reached[g->n] = 0;
	return g->n++;
}

/* Record that "from" feeds "to", both statement indices.
 */
static isl_stat add_edge(struct ppcg_stmt_graph *g, int from, int to)
{
	if (from < 0 || to < 0)
		return isl_stat_error;
	if (g->n_edge == g->n_edge_alloc) {
		int n_alloc = g->n_edge_alloc ? 2 * g->n_edge_alloc : 256;
		int *f = realloc(g->edge_from, n_alloc * sizeof(*f));
		int *t = realloc(g->edge_to, n_alloc * sizeof(*t));

		if (f)
			g->edge_from = f;
		if (t)
			g->edge_to = t;
		if (!f || !t)
			return isl_stat_error;
		g->n_edge_alloc = n_alloc;
	}
	g->edge_from[g->n_edge] = from;
	g->edge_to[g->n_edge] = to;
	g->n_edge++;

	return isl_stat_ok;
}

/* Add the statement level edge described by one map of dep_flow,
 * which runs from the instance that writes to the instance that reads.
 */
static isl_stat collect_edge(__isl_take isl_map *map, void *user)
{
	struct ppcg_stmt_graph *g = user;
	int from, to;

	from = stmt_index(g, isl_map_get_tuple_id(map, isl_dim_in));
	to = stmt_index(g, isl_map_get_tuple_id(map, isl_dim_out));
	isl_map_free(map);

	return add_edge(g, from, to);
}

/* Mark the statement of "set" as a root and put it on the frontier.
 */
static isl_stat mark_root(__isl_take isl_set *set, void *user)
{
	struct ppcg_stmt_graph *g = user;
	int i;

	i = stmt_index(g, isl_set_get_tuple_id(set));
	isl_set_free(set);
	if (i < 0)
		return isl_stat_error;
	if (!g->reached[i]) {
		g->reached[i] = 1;
		g->queue[g->n_queue++] = i;
	}

	return isl_stat_ok;
}

/* The graph, and the live instances collected out of a domain.
 */
struct ppcg_keep_data {
	struct ppcg_stmt_graph *g;
	isl_union_set *live;
};

/* Keep "set" if its statement was reached, drop it otherwise.
 *
 * A statement of the domain that never appears in dep_flow and is no
 * root is added here for the first time, unreached, and dropped --
 * which is right: nothing it computes is read and it is not itself an
 * output.
 */
static isl_stat keep_reached(__isl_take isl_set *set, void *user)
{
	struct ppcg_keep_data *d = user;
	int i;

	i = stmt_index(d->g, isl_set_get_tuple_id(set));
	if (i < 0) {
		isl_set_free(set);
		return isl_stat_error;
	}
	if (d->g->reached[i])
		d->live = isl_union_set_add_set(d->live, set);
	else
		isl_set_free(set);

	return isl_stat_ok;
}

static void stmt_graph_clear(struct ppcg_stmt_graph *g)
{
	int i;

	for (i = 0; i < g->n; ++i)
		isl_id_free(g->id[i]);
	free(g->id);
	free(g->reached);
	free(g->queue);
	free(g->edge_from);
	free(g->edge_to);
}

/* Eliminate dead code from ps->domain.
 *
 * In particular, intersect both ps->domain and the domain of
 * ps->schedule with the (parts of) iteration
 * domains that are needed to produce the output or for statement
 * iterations that call functions.
 * Also intersect the range of the dataflow dependences with
 * this domain such that the removed instances will no longer
 * be considered as targets of dataflow.
 *
 * We start with the iteration domains that call functions, those that
 * return from the scop, and the set of iterations that last write to
 * an array (except those that are later killed).  These are the roots:
 * what the scop computes for whoever comes after it.
 *
 * Then we keep every statement that can reach a root by walking
 * dep_flow backwards.  The walk is over statements and not over
 * statement instances: one vertex per tuple name, one edge per map of
 * dep_flow, and a breadth first search from the statements the roots
 * belong to.  A statement that is reached keeps all of its instances.
 *
 * That is an overapproximation, and it is the right one.  A statement
 * some of whose instances feed a root keeps the instances that do not,
 * which costs a missed elimination and nothing else; nothing needed is
 * ever dropped, which is the only thing that would be unsound.  What
 * dead code elimination is for here is whole statements that compute
 * what nobody reads, and that is a property of the statement.
 *
 * The precise answer -- the backward image of the roots under the
 * transitive closure of dep_flow -- is what this used to compute, and
 * it does not survive a whole program.  On one engine of 668 nodes the
 * closure took a union map of 2765 relations to 1184629 and was still
 * growing, at forty gigabytes, having spent its time in the cubic
 * Floyd-Warshall that isl runs over the components of a union map.
 * The walk below touches each edge once.
 *
 * The roots are marked reached at the start rather than added at the
 * end: a root need not lie in its own backward image, since the last
 * write to a live-out array has no consumer inside the scop, which is
 * exactly what makes it a root.
 *
 * No affine hull is taken anywhere, which is what an older formulation
 * of this procedure used to bound the number of iterations.  Widening
 * "live" to its affine hull and then cutting the result back with the
 * constraints that survive is not a monotone step, and it could fail
 * to terminate.
 */
static int eliminate_dead_code(struct ppcg_scop *ps)
{
	isl_union_set *live;
	isl_union_pw_multi_aff *tagger;
	struct ppcg_stmt_graph g = { 0 };
	struct ppcg_keep_data keep;

	live = isl_union_map_domain(isl_union_map_copy(ps->live_out));
	if (!isl_union_set_is_empty(ps->call)) {
		live = isl_union_set_union(live, isl_union_set_copy(ps->call));
		live = isl_union_set_coalesce(live);
	}
	/* What a scop returns leaves it.  Nothing inside the scop writes
	 * it anywhere that outlives the scop -- the variable holding it is
	 * declared in the scop and killed at the end of it -- so without
	 * this the return, and everything that computes what it returns,
	 * is dead.  A whole function body is a scop under autodetection,
	 * which is the only way a linked AST is read, so this is the
	 * ordinary case there rather than a corner of it.
	 */
	if (!isl_union_set_is_empty(ps->ret)) {
		live = isl_union_set_union(live, isl_union_set_copy(ps->ret));
		live = isl_union_set_coalesce(live);
	}

	g.ctx = isl_union_set_get_ctx(live);
	if (isl_union_map_foreach_map(ps->dep_flow, &collect_edge, &g) < 0)
		goto error;
	if (isl_union_set_foreach_set(live, &mark_root, &g) < 0)
		goto error;

	/* Walk the edges backwards: an edge runs from what writes to what
	 * reads, so a statement is live when something it feeds is live.
	 * Each round takes one statement off the frontier and looks at
	 * every edge, which is enough here -- the frontier holds each
	 * statement at most once, so the whole walk is one pass over the
	 * edges per statement rather than the cube of them.
	 */
	while (g.n_queue > 0) {
		int to = g.queue[--g.n_queue];
		int e;

		for (e = 0; e < g.n_edge; ++e) {
			int from;

			if (g.edge_to[e] != to)
				continue;
			from = g.edge_from[e];
			if (g.reached[from])
				continue;
			g.reached[from] = 1;
			g.queue[g.n_queue++] = from;
		}
	}

	isl_union_set_free(live);
	keep.g = &g;
	keep.live = isl_union_set_empty(isl_union_set_get_space(ps->domain));
	if (isl_union_set_foreach_set(ps->domain, &keep_reached, &keep) < 0) {
		isl_union_set_free(keep.live);
		goto error;
	}
	live = isl_union_set_coalesce(keep.live);
	stmt_graph_clear(&g);

	if (report_dead_code(ps, live) < 0) {
		isl_union_set_free(live);
		return -1;
	}

	ps->domain = isl_union_set_intersect(ps->domain,
						isl_union_set_copy(live));
	ps->schedule = isl_schedule_intersect_domain(ps->schedule,
						isl_union_set_copy(live));
	ps->dep_flow = isl_union_map_intersect_range(ps->dep_flow,
						isl_union_set_copy(live));
	tagger = isl_union_pw_multi_aff_copy(ps->tagger);
	live = isl_union_set_preimage_union_pw_multi_aff(live, tagger);
	ps->tagged_dep_flow = isl_union_map_intersect_range(ps->tagged_dep_flow,
						live);

	return 0;
error:
	stmt_graph_clear(&g);
	isl_union_set_free(live);
	return -1;
}

/* Intersect "set" with the set described by "str", taking the NULL
 * string to represent the universal set.
 */
static __isl_give isl_set *set_intersect_str(__isl_take isl_set *set,
	const char *str)
{
	isl_ctx *ctx;
	isl_set *set2;

	if (!str)
		return set;

	ctx = isl_set_get_ctx(set);
	set2 = isl_set_read_from_str(ctx, str);
	set = isl_set_intersect(set, set2);

	return set;
}

static void *ppcg_scop_free(struct ppcg_scop *ps)
{
	if (!ps)
		return NULL;

	ppcg_reductions_free(ps->reductions);
	isl_union_map_free(ps->reduction_deps);

	isl_set_free(ps->context);
	isl_union_set_free(ps->domain);
	isl_union_set_free(ps->call);
	isl_union_set_free(ps->ret);
	isl_union_map_free(ps->tagged_reads);
	isl_union_map_free(ps->reads);
	isl_union_map_free(ps->live_in);
	isl_union_map_free(ps->tagged_may_writes);
	isl_union_map_free(ps->tagged_must_writes);
	isl_union_map_free(ps->may_writes);
	isl_union_map_free(ps->must_writes);
	isl_union_map_free(ps->plain_may_writes);
	isl_union_map_free(ps->plain_must_writes);
	isl_union_map_free(ps->live_out);
	isl_union_map_free(ps->tagged_must_kills);
	isl_union_map_free(ps->must_kills);
	isl_union_map_free(ps->tagged_dep_flow);
	isl_union_map_free(ps->dep_flow);
	isl_union_map_free(ps->dep_false);
	isl_union_map_free(ps->dep_forced);
	isl_union_map_free(ps->tagged_dep_order);
	isl_union_map_free(ps->dep_order);
	isl_schedule_free(ps->schedule);
	isl_union_pw_multi_aff_free(ps->tagger);
	isl_union_map_free(ps->independence);
	isl_id_to_ast_expr_free(ps->names);

	free(ps);

	return NULL;
}

/* Extract a ppcg_scop from a pet_scop.
 *
 * The constructed ppcg_scop refers to elements from the pet_scop
 * so the pet_scop should not be freed before the ppcg_scop.
 */
static struct ppcg_scop *ppcg_scop_from_pet_scop(struct pet_scop *scop,
	struct ppcg_options *options)
{
	int i;
	isl_ctx *ctx;
	struct ppcg_scop *ps;

	if (!scop)
		return NULL;

	ctx = isl_set_get_ctx(scop->context);

	ps = isl_calloc_type(ctx, struct ppcg_scop);
	if (!ps)
		return NULL;

	ps->names = collect_names(scop);
	ps->options = options;
	ps->start = pet_loc_get_start(scop->loc);
	ps->end = pet_loc_get_end(scop->loc);
	ps->context = isl_set_copy(scop->context);
	ps->context = set_intersect_str(ps->context, options->ctx);
	if (options->non_negative_parameters) {
		isl_space *space = isl_set_get_space(ps->context);
		isl_set *nn = isl_set_nat_universe(space);
		ps->context = isl_set_intersect(ps->context, nn);
	}
	ps->domain = collect_non_kill_domains(scop);
	ps->call = collect_call_domains(scop);
	ps->ret = collect_return_domains(scop);
	ps->tagged_reads = pet_scop_get_tagged_may_reads(scop);
	ps->reads = pet_scop_get_may_reads(scop);
	ps->tagged_may_writes = pet_scop_get_tagged_may_writes(scop);
	ps->may_writes = pet_scop_get_may_writes(scop);
	ps->tagged_must_writes = pet_scop_get_tagged_must_writes(scop);
	ps->must_writes = pet_scop_get_must_writes(scop);
	ps->plain_may_writes = pet_scop_get_plain_may_writes(scop);
	ps->plain_must_writes = pet_scop_get_plain_must_writes(scop);
	ps->tagged_must_kills = pet_scop_get_tagged_must_kills(scop);
	ps->must_kills = pet_scop_get_must_kills(scop);
	ps->schedule = isl_schedule_copy(scop->schedule);
	ps->pet = scop;
	ps->independence = isl_union_map_empty(isl_set_get_space(ps->context));
	for (i = 0; i < scop->n_independence; ++i)
		ps->independence = isl_union_map_union(ps->independence,
			isl_union_map_copy(scop->independences[i]->filter));

	report_empty_context(ps);

	compute_tagger(ps);
	compute_dependences(ps);
	if (eliminate_dead_code(ps) < 0)
		return ppcg_scop_free(ps);

	if (!ps->context || !ps->domain || !ps->call || !ps->ret || !ps->reads ||
	    !ps->may_writes || !ps->must_writes || !ps->plain_may_writes ||
	    !ps->plain_must_writes || !ps->tagged_must_kills ||
	    !ps->must_kills || !ps->schedule || !ps->independence || !ps->names)
		return ppcg_scop_free(ps);

	return ps;
}

/* Internal data structure for ppcg_transform.
 */
struct ppcg_transform_data {
	struct ppcg_options *options;
	__isl_give isl_printer *(*transform)(__isl_take isl_printer *p,
		struct ppcg_scop *scop, void *user);
	void *user;
};

/* Should we print the original code?
 * That is, does "scop" involve any data dependent conditions or
 * nested expressions that cannot be handled by pet_stmt_build_ast_exprs?
 */
/* Would the transformed program have to be abandoned and the input printed
 * back instead, and if so, say why.
 *
 * A SCOP THAT COMES BACK UNSCHEDULED IS A REFUSAL, NOT A RESULT.  Both of
 * these used to whisper under --verbose, to stdout, and then the input was
 * copied to the output with its pragma intact and ppcg exited 0.  A caller
 * who looks at the exit status sees success; a caller who looks at the file
 * sees C.  Measured on a 402-node scop: 316 parallel bands became 0 in
 * ninety seconds instead of thirty-three minutes, and the only trace was an
 * isl assertion in the middle of thirty-seven kilobytes of unrelated output.
 * tests/ppcg in llama-dspark had already learned to test for the pragma
 * rather than the exit code, which is the shape of a workaround for exactly
 * this.
 *
 * Returns 1 if the original is to be printed and 0 otherwise; sets *fatal
 * when the caller should stop rather than print anything.
 */
static int print_original(struct pet_scop *scop, struct ppcg_options *options,
	int *fatal)
{
	const char *why = NULL;

	if (!pet_scop_can_build_ast_exprs(scop))
		why = "some index expressions cannot be printed";
	else if (pet_scop_has_data_dependent_conditions(scop))
		why = "the input involves data dependent conditions";

	if (!why)
		return 0;

	fprintf(stderr, "ppcg: %s: %s\n",
		options->allow_unscheduled ?
			"printing the original code because" :
			"REFUSING TO SCHEDULE because", why);
	if (!options->allow_unscheduled) {
		fprintf(stderr, "ppcg: the input would be copied to the "
			"output unchanged, which is a refusal wearing the "
			"shape of a result.  Pass --allow-unscheduled to get "
			"the copy anyway.\n");
		*fatal = 1;
	}

	return 1;
}

/* Turn "scop" into a ppcg_scop and hand it to "fn", printing to "p".
 *
 * If "scop" contains any data dependent conditions or if we may
 * not be able to print the transformed program, then just print
 * the original code.
 *
 * This is the part of transforming a scop that does not depend on
 * where the scop came from.  A scop read out of one C source file and
 * a scop read out of a linked AST reach the same code through it.
 */
__isl_give isl_printer *ppcg_transform_scop(__isl_take isl_printer *p,
	struct pet_scop *scop, struct ppcg_options *options,
	__isl_give isl_printer *(*fn)(__isl_take isl_printer *p,
		struct ppcg_scop *scop, void *user), void *user)
{
	struct ppcg_scop *ps;
	int fatal = 0;

	if (print_original(scop, options, &fatal)) {
		if (!fatal)
			p = pet_scop_print_original(scop, p);
		else
			p = isl_printer_free(p);
		pet_scop_free(scop);
		return p;
	}

	scop = pet_scop_align_params(scop);
	ps = ppcg_scop_from_pet_scop(scop, options);
	if (!ps) {
		/* Nothing came of the scop, so there is nothing to hand
		 * the callback: every one of them reads what it is given.
		 * Freeing the printer says so in the way the callers of
		 * this function already answer a scop they cannot use.
		 */
		pet_scop_free(scop);
		return isl_printer_free(p);
	}

	if (options->debug->dump_reductions) {
		struct ppcg_reductions *reductions;

		reductions = ppcg_find_reductions(ps);
		ppcg_reductions_print(stdout, ps, reductions);
		ppcg_reductions_free(reductions);
		ppcg_scop_free(ps);
		p = pet_scop_print_original(scop, p);
		pet_scop_free(scop);
		return p;
	}

	p = fn(p, ps, user);

	ppcg_scop_free(ps);
	pet_scop_free(scop);

	return p;
}

/* Callback for pet_transform_C_source that transforms
 * the given pet_scop to a ppcg_scop before calling the
 * ppcg_transform callback.
 */
static __isl_give isl_printer *transform(__isl_take isl_printer *p,
	struct pet_scop *scop, void *user)
{
	struct ppcg_transform_data *data = user;

	return ppcg_transform_scop(p, scop, data->options,
					data->transform, data->user);
}

/* Transform the C source file "input" by rewriting each scop
 * through a call to "transform".
 * The transformed C code is written to "out".
 *
 * This is a wrapper around pet_transform_C_source that transforms
 * the pet_scop to a ppcg_scop before calling "fn".
 */
int ppcg_transform(isl_ctx *ctx, const char *input, FILE *out,
	struct ppcg_options *options,
	__isl_give isl_printer *(*fn)(__isl_take isl_printer *p,
		struct ppcg_scop *scop, void *user), void *user)
{
	struct ppcg_transform_data data = { options, fn, user };
	return pet_transform_C_source(ctx, input, out, &transform, &data);
}

