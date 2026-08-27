/*
 * Copyright 2010-2011 INRIA Saclay
 *
 * Use of this software is governed by the MIT license
 *
 * Written by Sven Verdoolaege, INRIA Saclay - Ile-de-France,
 * Parc Club Orsay Universite, ZAC des vignes, 4 rue Jacques Monod,
 * 91893 Orsay, France
 */

#include "ppcg_options.h"

static struct isl_arg_choice target[] = {
	{"c",		PPCG_TARGET_C},
	{"cuda",	PPCG_TARGET_CUDA},
	{"opencl",      PPCG_TARGET_OPENCL},
	{0}
};

/* Set defaults that depend on the target.
 * In particular, set --schedule-outer-coincidence iff target is a GPU.
 */
void ppcg_options_set_target_defaults(struct ppcg_options *options)
{
	char *argv[2] = { NULL };

	argv[0] = "ppcg_options_set_target_defaults";
	if (options->target == PPCG_TARGET_C)
		argv[1] = "--no-schedule-outer-coincidence";
	else
		argv[1] = "--schedule-outer-coincidence";

	isl_options_parse(options->isl, 2, argv, ISL_ARG_ALL);
}

/* Callback that is called whenever the "target" option is set (to "val").
 * The callback is called after target has been updated.
 *
 * Call ppcg_options_set_target_defaults to reset the target-dependent options.
 */
static int set_target(void *opt, unsigned val)
{
	struct ppcg_options *options = opt;

	ppcg_options_set_target_defaults(options);

	return 0;
}

ISL_ARGS_START(struct ppcg_debug_options, ppcg_debug_options_args)
ISL_ARG_BOOL(struct ppcg_debug_options, dump_schedule_constraints, 0,
	"dump-schedule-constraints", 0, "dump schedule constraints")
ISL_ARG_BOOL(struct ppcg_debug_options, dump_schedule, 0,
	"dump-schedule", 0, "dump isl computed schedule")
ISL_ARG_BOOL(struct ppcg_debug_options, dump_final_schedule, 0,
	"dump-final-schedule", 0, "dump PPCG computed schedule")
ISL_ARG_BOOL(struct ppcg_debug_options, dump_sizes, 0,
	"dump-sizes", 0,
	"dump effectively used per kernel tile, grid and block sizes")
ISL_ARG_BOOL(struct ppcg_debug_options, dump_reductions, 0,
	"dump-reductions", 0,
	"print the accumulations found in each scop and generate nothing")
ISL_ARG_BOOL(struct ppcg_debug_options, verbose, 'v', "verbose", 0, NULL)
ISL_ARGS_END

ISL_ARGS_START(struct ppcg_options, ppcg_opencl_options_args)
ISL_ARG_STR(struct ppcg_options, opencl_compiler_options, 0, "compiler-options",
	"options", NULL, "options to pass to the OpenCL compiler")
ISL_ARG_BOOL(struct ppcg_options, opencl_use_gpu, 0, "use-gpu", 1,
	"use GPU device (if available)")
ISL_ARG_STR_LIST(struct ppcg_options, opencl_n_include_file,
	opencl_include_files, 0, "include-file", "filename",
	"file to #include in generated OpenCL code")
ISL_ARG_BOOL(struct ppcg_options, opencl_print_kernel_types, 0,
	"print-kernel-types", 1,
	"print definitions of types in the kernel file")
ISL_ARG_BOOL(struct ppcg_options, opencl_embed_kernel_code, 0,
	"embed-kernel-code", 0, "embed kernel code into host code")
ISL_ARGS_END

ISL_ARGS_START(struct ppcg_options, ppcg_options_args)
ISL_ARG_CHILD(struct ppcg_options, isl, "isl", &isl_options_args, "isl options")
ISL_ARG_CHILD(struct ppcg_options, debug, NULL, &ppcg_debug_options_args,
	"debugging options")
ISL_ARG_BOOL(struct ppcg_options, group_chains, 0, "group-chains", 1,
	"group chains of interdependent statements that are executed "
	"consecutively in the original schedule before scheduling")
ISL_ARG_BOOL(struct ppcg_options, reschedule, 0, "reschedule", 1,
	"replace original schedule by isl computed schedule")
ISL_ARG_BOOL(struct ppcg_options, scale_tile_loops, 0,
	"scale-tile-loops", 1, NULL)
ISL_ARG_BOOL(struct ppcg_options, wrap, 0, "wrap", 1, NULL)
ISL_ARG_BOOL(struct ppcg_options, use_shared_memory, 0, "shared-memory", 1,
	"use shared memory in kernel code")
ISL_ARG_BOOL(struct ppcg_options, use_private_memory, 0, "private-memory", 1,
	"use private memory in kernel code")
ISL_ARG_STR(struct ppcg_options, ctx, 0, "ctx", "context", NULL,
    "Constraints on parameters")
ISL_ARG_BOOL(struct ppcg_options, non_negative_parameters, 0,
	"assume-non-negative-parameters", 0,
	"assume all parameters are non-negative)")
ISL_ARG_BOOL(struct ppcg_options, tile, 0, "tile", 0,
	"perform tiling (C target)")
ISL_ARG_INT(struct ppcg_options, tile_size, 'S', "tile-size", "size", 32, NULL)
ISL_ARG_BOOL(struct ppcg_options, isolate_full_tiles, 0, "isolate-full-tiles",
	0, "isolate full tiles from partial tiles (hybrid tiling)")
ISL_ARG_STR(struct ppcg_options, sizes, 0, "sizes", "sizes", NULL,
	"Per kernel tile, grid and block sizes")
ISL_ARG_INT(struct ppcg_options, max_shared_memory, 0,
	"max-shared-memory", "size", 8192, "maximal amount of shared memory")
ISL_ARG_USER_OPT_CHOICE(struct ppcg_options, target, 0, "target", target,
	&set_target, PPCG_TARGET_CUDA, PPCG_TARGET_CUDA,
	"the target to generate code for")
ISL_ARG_BOOL(struct ppcg_options, linearize_device_arrays, 0,
	"linearize-device-arrays", 1,
	"linearize all device arrays, even those of fixed size")
ISL_ARG_BOOL(struct ppcg_options, allow_gnu_extensions, 0,
	"allow-gnu-extensions", 1,
	"allow the use of GNU extensions in generated code")
ISL_ARG_BOOL(struct ppcg_options, live_range_reordering, 0,
	"live-range-reordering", 1,
	"allow successive live ranges on the same memory element "
	"to be reordered")
/* WORK MAY NOT LEAVE A SCOP WITHOUT A WORD.
 *
 * Dead-code elimination used to print under --verbose, to stdout, and carry
 * on.  Twice on one tree in one session that was the only trace of storage
 * this tool had misread: a store into the caller's memory through a union
 * member vanished, and a kernel whose windows were typed local pointers lost
 * both of its loops -- 8192 instances -- while the exit status stayed 0.
 * The report is therefore unconditional and on stderr.
 *
 * STOPPING, though, is not the default, and that was measured rather than
 * chosen.  ppcg's own examples/chemv.c legitimately loses statements under
 * incY <= 0 -- the arrays are pet's own varNN_Re and varNN_Im temporaries --
 * so a fatal default breaks correct input, and broke ppcg's own build in
 * three targets before this flag existed.  Removing dead code is neither an
 * inability nor an error; it is a transformation that must be SEEN.  A
 * caller who knows its scop should lose nothing asks to stop.
 */
ISL_ARG_BOOL(struct ppcg_options, stop_on_dead_code, 0,
	"stop-on-dead-code", 0,
	"stop when statement instances are eliminated as dead; the "
	"elimination is reported either way")
/* A SCOP THAT COMES BACK UNSCHEDULED IS A REFUSAL, NOT A RESULT.
 *
 * Copying the input to the output with its pragma intact and exiting 0 says
 * "success" to anything that reads the exit status and says nothing at all
 * to anything that reads the file.  Measured: a 402-node scop went from 316
 * parallel bands to 0 that way, in ninety seconds instead of thirty-three
 * minutes, and the only trace was an isl assertion buried in unrelated
 * output.  The harness in llama-dspark had already learned to test for the
 * pragma instead of the exit code, which is what a workaround for this looks
 * like.
 */
ISL_ARG_BOOL(struct ppcg_options, allow_unscheduled, 0,
	"allow-unscheduled", 0,
	"copy the input to the output when it cannot be scheduled instead "
	"of stopping; the reason is reported either way")
ISL_ARG_BOOL(struct ppcg_options, hybrid, 0, "hybrid", 0,
	"apply hybrid tiling whenever a suitable input pattern is found "
	"(GPU targets)")
ISL_ARG_BOOL(struct ppcg_options, unroll_copy_shared, 0, "unroll-copy-shared",
	0, "unroll code for copying to/from shared memory")
ISL_ARG_BOOL(struct ppcg_options, unroll_gpu_tile, 0, "unroll-gpu-tile", 0,
	"unroll code inside tile on GPU targets")
ISL_ARG_GROUP("opencl", &ppcg_opencl_options_args, "OpenCL options")
ISL_ARG_STR(struct ppcg_options, save_schedule_file, 0, "save-schedule",
	"file", NULL, "save isl computed schedule to <file>")
ISL_ARG_STR(struct ppcg_options, load_schedule_file, 0, "load-schedule",
	"file", NULL, "load schedule from <file>, "
	"using it instead of an isl computed schedule")
ISL_ARGS_END
