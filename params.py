#!/usr/bin/env python3

from operator import *

params_mapping = """
#define MAX_INLINE_INSNS_SINGLE \
  PARAM_VALUE (PARAM_MAX_INLINE_INSNS_SINGLE)
#define MAX_INLINE_INSNS_AUTO \
  PARAM_VALUE (PARAM_MAX_INLINE_INSNS_AUTO)
#define MAX_VARIABLE_EXPANSIONS \
  PARAM_VALUE (PARAM_MAX_VARIABLE_EXPANSIONS)
#define MIN_VECT_LOOP_BOUND \
  PARAM_VALUE (PARAM_MIN_VECT_LOOP_BOUND)
#define MAX_DELAY_SLOT_INSN_SEARCH \
  PARAM_VALUE (PARAM_MAX_DELAY_SLOT_INSN_SEARCH)
#define MAX_DELAY_SLOT_LIVE_SEARCH \
  PARAM_VALUE (PARAM_MAX_DELAY_SLOT_LIVE_SEARCH)
#define MAX_PENDING_LIST_LENGTH \
  PARAM_VALUE (PARAM_MAX_PENDING_LIST_LENGTH)
#define MAX_GCSE_MEMORY \
  ((size_t) PARAM_VALUE (PARAM_MAX_GCSE_MEMORY))
#define MAX_GCSE_INSERTION_RATIO \
  ((size_t) PARAM_VALUE (PARAM_MAX_GCSE_INSERTION_RATIO))
#define GCSE_AFTER_RELOAD_PARTIAL_FRACTION \
  PARAM_VALUE (PARAM_GCSE_AFTER_RELOAD_PARTIAL_FRACTION)
#define GCSE_AFTER_RELOAD_CRITICAL_FRACTION \
  PARAM_VALUE (PARAM_GCSE_AFTER_RELOAD_CRITICAL_FRACTION)
#define GCSE_COST_DISTANCE_RATIO \
  PARAM_VALUE (PARAM_GCSE_COST_DISTANCE_RATIO)
#define GCSE_UNRESTRICTED_COST \
  PARAM_VALUE (PARAM_GCSE_UNRESTRICTED_COST)
#define MAX_HOIST_DEPTH \
  PARAM_VALUE (PARAM_MAX_HOIST_DEPTH)
#define MAX_UNROLLED_INSNS \
  PARAM_VALUE (PARAM_MAX_UNROLLED_INSNS)
#define SMS_MAX_II_FACTOR \
  PARAM_VALUE (PARAM_SMS_MAX_II_FACTOR)
#define SMS_DFA_HISTORY \
  PARAM_VALUE (PARAM_SMS_DFA_HISTORY)
#define SMS_LOOP_AVERAGE_COUNT_THRESHOLD \
  PARAM_VALUE (PARAM_SMS_LOOP_AVERAGE_COUNT_THRESHOLD)
#define INTEGER_SHARE_LIMIT \
  PARAM_VALUE (PARAM_INTEGER_SHARE_LIMIT)
#define MAX_LAST_VALUE_RTL \
  PARAM_VALUE (PARAM_MAX_LAST_VALUE_RTL)
#define MAX_FIELDS_FOR_FIELD_SENSITIVE \
  ((size_t) PARAM_VALUE (PARAM_MAX_FIELDS_FOR_FIELD_SENSITIVE))
#define MAX_SCHED_READY_INSNS \
  PARAM_VALUE (PARAM_MAX_SCHED_READY_INSNS)
#define PREFETCH_LATENCY \
  PARAM_VALUE (PARAM_PREFETCH_LATENCY)
#define SIMULTANEOUS_PREFETCHES \
  PARAM_VALUE (PARAM_SIMULTANEOUS_PREFETCHES)
#define L1_CACHE_SIZE \
  PARAM_VALUE (PARAM_L1_CACHE_SIZE)
#define L1_CACHE_LINE_SIZE \
  PARAM_VALUE (PARAM_L1_CACHE_LINE_SIZE)
#define L2_CACHE_SIZE \
  PARAM_VALUE (PARAM_L2_CACHE_SIZE)
#define PREFETCH_DYNAMIC_STRIDES \
  PARAM_VALUE (PARAM_PREFETCH_DYNAMIC_STRIDES)
#define PREFETCH_MINIMUM_STRIDE \
  PARAM_VALUE (PARAM_PREFETCH_MINIMUM_STRIDE)
#define USE_CANONICAL_TYPES \
  PARAM_VALUE (PARAM_USE_CANONICAL_TYPES)
#define IRA_MAX_LOOPS_NUM \
  PARAM_VALUE (PARAM_IRA_MAX_LOOPS_NUM)
#define IRA_MAX_CONFLICT_TABLE_SIZE \
  PARAM_VALUE (PARAM_IRA_MAX_CONFLICT_TABLE_SIZE)
#define IRA_LOOP_RESERVED_REGS \
  PARAM_VALUE (PARAM_IRA_LOOP_RESERVED_REGS)
#define LRA_MAX_CONSIDERED_RELOAD_PSEUDOS \
  PARAM_VALUE (PARAM_LRA_MAX_CONSIDERED_RELOAD_PSEUDOS)
#define LRA_INHERITANCE_EBB_PROBABILITY_CUTOFF \
  PARAM_VALUE (PARAM_LRA_INHERITANCE_EBB_PROBABILITY_CUTOFF)
#define SWITCH_CONVERSION_BRANCH_RATIO \
  PARAM_VALUE (PARAM_SWITCH_CONVERSION_BRANCH_RATIO)
#define LOOP_INVARIANT_MAX_BBS_IN_LOOP \
  PARAM_VALUE (PARAM_LOOP_INVARIANT_MAX_BBS_IN_LOOP)
#define SLP_MAX_INSNS_IN_BB \
  PARAM_VALUE (PARAM_SLP_MAX_INSNS_IN_BB)
#define MIN_INSN_TO_PREFETCH_RATIO \
  PARAM_VALUE (PARAM_MIN_INSN_TO_PREFETCH_RATIO)
#define PREFETCH_MIN_INSN_TO_MEM_RATIO \
  PARAM_VALUE (PARAM_PREFETCH_MIN_INSN_TO_MEM_RATIO)
#define MIN_NONDEBUG_INSN_UID \
  PARAM_VALUE (PARAM_MIN_NONDEBUG_INSN_UID)
#define MAX_STORES_TO_SINK \
  PARAM_VALUE (PARAM_MAX_STORES_TO_SINK)
#define ASAN_STACK \
  PARAM_VALUE (PARAM_ASAN_STACK)
#define ASAN_PROTECT_ALLOCAS \
  PARAM_VALUE (PARAM_ASAN_PROTECT_ALLOCAS)
#define ASAN_GLOBALS \
  PARAM_VALUE (PARAM_ASAN_GLOBALS)
#define ASAN_INSTRUMENT_READS \
  PARAM_VALUE (PARAM_ASAN_INSTRUMENT_READS)
#define ASAN_INSTRUMENT_WRITES \
  PARAM_VALUE (PARAM_ASAN_INSTRUMENT_WRITES)
#define ASAN_MEMINTRIN \
  PARAM_VALUE (PARAM_ASAN_MEMINTRIN)
#define ASAN_USE_AFTER_RETURN \
  PARAM_VALUE (PARAM_ASAN_USE_AFTER_RETURN)
#define ASAN_INSTRUMENTATION_WITH_CALL_THRESHOLD \
  PARAM_VALUE (PARAM_ASAN_INSTRUMENTATION_WITH_CALL_THRESHOLD)
#define ASAN_PARAM_USE_AFTER_SCOPE_DIRECT_EMISSION_THRESHOLD \
  PARAM_VALUE (PARAM_USE_AFTER_SCOPE_DIRECT_EMISSION_THRESHOLD)
"""

mapping = {}
lines = params_mapping.strip().split('\n')

for l in lines:
  tokens = [l for l in l.split(' ') if l]
  mapping[tokens[1]] = tokens[-1].strip('(').strip(')')

usage = """
gcc/auto-profile.c:    for (int i = 0; i < PARAM_VALUE (PARAM_EARLY_INLINER_MAX_ITERATIONS); i++)
gcc/bb-reorder.c:    max_size *= PARAM_VALUE (PARAM_MAX_GROW_COPY_BB_INSNS);
gcc/bb-reorder.c:    = uncond_jump_length * PARAM_VALUE (PARAM_MAX_GOTO_DUPLICATION_INSNS);
gcc/builtins.c:	       PARAM_VALUE (BUILTIN_STRING_CMP_INLINE_LENGTH))
gcc/c/gimple-parser.c:      gcov_type t = PARAM_VALUE (PARAM_GIMPLE_FE_COMPUTED_HOT_BB_THRESHOLD);
gcc/cfgcleanup.c:  if ((nmatch < PARAM_VALUE (PARAM_MIN_CROSSJUMP_INSNS))
gcc/cfgcleanup.c:  max = PARAM_VALUE (PARAM_MAX_CROSSJUMP_EDGES);
gcc/cfgexpand.c:	   < PARAM_VALUE (PARAM_MIN_SIZE_FOR_STACK_SHARING)));
gcc/cfgexpand.c:	  unsigned HOST_WIDE_INT max = PARAM_VALUE (PARAM_SSP_BUFFER_SIZE);
gcc/cfgexpand.c:		 (int) PARAM_VALUE (PARAM_SSP_BUFFER_SIZE));
gcc/cfgexpand.c:      >= PARAM_VALUE (PARAM_MAX_DEBUG_MARKER_COUNT))
gcc/cfgloopanal.c:      expected = PARAM_VALUE (PARAM_AVG_LOOP_NITER);
gcc/cfgloopanal.c:	  expected = PARAM_VALUE (PARAM_AVG_LOOP_NITER);
gcc/cfgloopanal.c:      expected = PARAM_VALUE (PARAM_AVG_LOOP_NITER);
gcc/cgraph.c:  if (sreal_frequency () * PARAM_VALUE (HOT_BB_FREQUENCY_FRACTION) <= 1)
gcc/combine.c:  int max_combine = PARAM_VALUE (PARAM_MAX_COMBINE_INSNS);
gcc/config/aarch64/aarch64.c:    = 1 << PARAM_VALUE (PARAM_STACK_CLASH_PROTECTION_GUARD_SIZE);
gcc/config/aarch64/aarch64.c:    = 1 << PARAM_VALUE (PARAM_STACK_CLASH_PROTECTION_GUARD_SIZE);
gcc/config/aarch64/aarch64.c:    = 1 << PARAM_VALUE (PARAM_STACK_CLASH_PROTECTION_GUARD_SIZE);
gcc/config/aarch64/aarch64.c:  int guard_size = PARAM_VALUE (PARAM_STACK_CLASH_PROTECTION_GUARD_SIZE);
gcc/config/aarch64/aarch64.c:    = PARAM_VALUE (PARAM_STACK_CLASH_PROTECTION_PROBE_INTERVAL);
gcc/config/i386/i386.c:	    << PARAM_VALUE (PARAM_STACK_CLASH_PROTECTION_PROBE_INTERVAL));
gcc/config/i386/i386.c:  if (size < (1 << PARAM_VALUE (PARAM_STACK_CLASH_PROTECTION_GUARD_SIZE)))
gcc/config/i386/i386.c:    return PARAM_VALUE (param);
gcc/config/ia64/ia64.c:    return PARAM_VALUE (PARAM_SCHED_MEM_TRUE_DEP_COST);
gcc/config/rs6000/rs6000-logue.c:	  << PARAM_VALUE (PARAM_STACK_CLASH_PROTECTION_PROBE_INTERVAL));
gcc/config/rs6000/rs6000-logue.c:	  << PARAM_VALUE (PARAM_STACK_CLASH_PROTECTION_GUARD_SIZE));
gcc/config/s390/s390.c:    = 1 << PARAM_VALUE (PARAM_STACK_CLASH_PROTECTION_PROBE_INTERVAL);
gcc/config/s390/s390.c:    = 1 << PARAM_VALUE (PARAM_STACK_CLASH_PROTECTION_GUARD_SIZE);
gcc/config/s390/s390.c:    = 1 << PARAM_VALUE (PARAM_STACK_CLASH_PROTECTION_PROBE_INTERVAL);
gcc/config/s390/s390.c:  if (!DISP_IN_RANGE ((1 << PARAM_VALUE (PARAM_STACK_CLASH_PROTECTION_PROBE_INTERVAL))))
gcc/coverage.c:  if  PARAM_VALUE (PARAM_PROFILE_FUNC_INTERNAL_ID))
gcc/coverage.c:      bool use_name_only =  PARAM_VALUE (PARAM_PROFILE_FUNC_INTERNAL_ID) == 0);
gcc/coverage.c:  if  PARAM_VALUE (PARAM_PROFILE_FUNC_INTERNAL_ID))
gcc/coverage.c:      if  PARAM_VALUE (PARAM_PROFILE_FUNC_INTERNAL_ID))
gcc/cp/name-lookup.c:  m_limit = PARAM_VALUE (CXX_MAX_NAMESPACES_FOR_DIAGNOSTIC_HELP);
gcc/cse.c:      while (bb && path_size < PARAM_VALUE (PARAM_MAX_CSE_PATH_LENGTH))
gcc/cse.c:	      && num_insns++ > PARAM_VALUE (PARAM_MAX_CSE_INSNS))
gcc/cse.c:			   PARAM_VALUE (PARAM_MAX_CSE_PATH_LENGTH));
gcc/cselib.c:	  if (num_mems < PARAM_VALUE (PARAM_MAX_CSELIB_MEMORY_LOCATIONS)
gcc/dse.c:  int max_active_local_stores = PARAM_VALUE (PARAM_MAX_DSE_ACTIVE_LOCAL_STORES);
gcc/explow.c:    = 1 << PARAM_VALUE (PARAM_STACK_CLASH_PROTECTION_PROBE_INTERVAL);
gcc/final.c:		 (1, PARAM_VALUE (PARAM_ALIGN_THRESHOLD));
gcc/final.c:		     PARAM_VALUE (PARAM_ALIGN_LOOP_ITERATIONS), 1)))
gcc/fold-const.c:  if  PARAM_VALUE (PARAM_LOGICAL_OP_NON_SHORT_CIRCUIT) != -1)
gcc/fold-const.c:      = PARAM_VALUE (PARAM_LOGICAL_OP_NON_SHORT_CIRCUIT);
gcc/fold-const.c:  if  PARAM_VALUE (PARAM_LOGICAL_OP_NON_SHORT_CIRCUIT) != -1)
gcc/fold-const.c:      = PARAM_VALUE (PARAM_LOGICAL_OP_NON_SHORT_CIRCUIT);
gcc/fold-const.c:	      && depth < PARAM_VALUE (PARAM_MAX_SSA_NAME_QUERY_DEPTH)
gcc/fold-const.c:	      && depth < PARAM_VALUE (PARAM_MAX_SSA_NAME_QUERY_DEPTH)
gcc/ggc-page.c:    MAX (G.allocated_last_gc, PARAM_VALUE (GGC_MIN_HEAPSIZE) * 1024);
gcc/ggc-page.c:  float min_expand = allocated_last_gc * PARAM_VALUE (GGC_MIN_EXPAND) / 100;
gcc/gimple-loop-interchange.cc:#define MAX_NUM_STMT     PARAM_VALUE (PARAM_LOOP_INTERCHANGE_MAX_NUM_STMTS))
gcc/gimple-loop-interchange.cc:#define MAX_DATAREFS     PARAM_VALUE (PARAM_LOOP_MAX_DATAREFS_FOR_DATADEPS))
gcc/gimple-loop-interchange.cc:#define OUTER_STRIDE_RATIO   PARAM_VALUE (PARAM_LOOP_INTERCHANGE_STRIDE_RATIO))
gcc/gimple-loop-jam.c:      if (i PARAM_VALUE (PARAM_UNROLL_JAM_MIN_PERCENT))
gcc/gimple-loop-jam.c:	  < (unsigned) PARAM_VALUE (PARAM_UNROLL_JAM_MIN_PERCENT))
gcc/gimple-loop-jam.c:      if (unroll_factor > (unsigned) PARAM_VALUE (PARAM_UNROLL_JAM_MAX_UNROLL))
gcc/gimple-loop-jam.c:	unroll_factor = PARAM_VALUE (PARAM_UNROLL_JAM_MAX_UNROLL);
gcc/gimple-loop-versioning.cc:	  ? PARAM_VALUE (PARAM_LOOP_VERSIONING_MAX_OUTER_INSNS)
gcc/gimple-loop-versioning.cc:	  : PARAM_VALUE (PARAM_LOOP_VERSIONING_MAX_INNER_INSNS));
gcc/gimple-ssa-split-paths.c:      >= PARAM_VALUE (PARAM_MAX_JUMP_THREAD_DUPLICATION_STMTS))
gcc/gimple-ssa-store-merging.c:    = !STRICT_ALIGNMENT && PARAM_VALUE (PARAM_STORE_MERGING_ALLOW_UNALIGNED);
gcc/gimple-ssa-store-merging.c:	      > (unsigned) PARAM_VALUE (PARAM_STORE_MERGING_MAX_SIZE)))
gcc/gimple-ssa-store-merging.c:    = !STRICT_ALIGNMENT && PARAM_VALUE (PARAM_STORE_MERGING_ALLOW_UNALIGNED);
gcc/gimple-ssa-store-merging.c:	  == (unsigned int) PARAM_VALUE (PARAM_MAX_STORES_TO_MERGE))
gcc/gimple-ssa-strength-reduction.c:  int max_iters = PARAM_VALUE (PARAM_MAX_SLSR_CANDIDATE_SCAN);
gcc/graphite-isl-ast-to-gimple.c:		|| PARAM_VALUE (PARAM_GRAPHITE_ALLOW_CODEGEN_ERRORS));
gcc/graphite-isl-ast-to-gimple.c:  int max_operations = PARAM_VALUE (PARAM_MAX_ISL_OPERATIONS);
gcc/graphite-optimize-isl.c:  long tile_size = PARAM_VALUE (PARAM_LOOP_BLOCK_TILE_SIZE);
gcc/graphite-optimize-isl.c:  int max_operations = PARAM_VALUE (PARAM_MAX_ISL_OPERATIONS);
gcc/graphite-scop-detection.c:      unsigned max_arrays = PARAM_VALUE (PARAM_GRAPHITE_MAX_ARRAYS_PER_SCOP);
gcc/graphite-scop-detection.c:      graphite_dim_t max_dim = PARAM_VALUE (PARAM_GRAPHITE_MAX_NB_SCOP_PARAMS);
gcc/haifa-sched.c:  modulo_backtracks_left = PARAM_VALUE (PARAM_MAX_MODULO_BACKTRACK_ATTEMPTS);
gcc/haifa-sched.c:  if  PARAM_VALUE (PARAM_SCHED_AUTOPREF_QUEUE_DEPTH) >= 0)
gcc/haifa-sched.c:  if (!insn_queue || PARAM_VALUE (PARAM_SCHED_AUTOPREF_QUEUE_DEPTH) <= 0)
gcc/haifa-sched.c:      if  PARAM_VALUE (PARAM_SCHED_AUTOPREF_QUEUE_DEPTH) == 1)
gcc/haifa-sched.c:      int n_stalls = PARAM_VALUE (PARAM_SCHED_AUTOPREF_QUEUE_DEPTH) - 1;
gcc/haifa-sched.c:		      PARAM_VALUE (PARAM_SCHED_PRESSURE_ALGORITHM));
gcc/haifa-sched.c:             PARAM_VALUE (PARAM_SCHED_SPEC_PROB_CUTOFF) * MAX_DEP_WEAK) / 100;
gcc/haifa-sched.c:             PARAM_VALUE (PARAM_SCHED_SPEC_PROB_CUTOFF)
gcc/hsa-gen.c:  if  PARAM_VALUE (PARAM_HSA_GEN_DEBUG_STORES) > 0)
gcc/ifcvt.c:  unsigned param = PARAM_VALUE (PARAM_MAX_RTL_IF_CONVERSION_INSNS);
gcc/ifcvt.c:  int limit = PARAM_VALUE (PARAM_MAX_RTL_IF_CONVERSION_INSNS);
gcc/ipa-cp.c:  if (values_count == PARAM_VALUE (PARAM_IPA_CP_VALUE_LIST_SIZE))
gcc/ipa-cp.c:      if (dest_plats->aggs_count == PARAM_VALUE (PARAM_IPA_MAX_AGG_ITEMS))
gcc/ipa-cp.c:    result += PARAM_VALUE (PARAM_IPA_CP_LOOP_HINT_BONUS);
gcc/ipa-cp.c:		  * (100 - PARAM_VALUE (PARAM_IPA_CP_RECURSION_PENALTY))) / 100;
gcc/ipa-cp.c:		  * (100 - PARAM_VALUE (PARAM_IPA_CP_SINGLE_CALL_PENALTY)))
gcc/ipa-cp.c:		 evaluation, PARAM_VALUE (PARAM_IPA_CP_EVAL_THRESHOLD));
gcc/ipa-cp.c:      return evaluation >= PARAM_VALUE (PARAM_IPA_CP_EVAL_THRESHOLD);
gcc/ipa-cp.c:		 evaluation, PARAM_VALUE (PARAM_IPA_CP_EVAL_THRESHOLD));
gcc/ipa-cp.c:      return evaluation >= PARAM_VALUE (PARAM_IPA_CP_EVAL_THRESHOLD);
gcc/ipa-cp.c:  if (max_new_size < PARAM_VALUE (PARAM_LARGE_UNIT_INSNS))
gcc/ipa-cp.c:    max_new_size = PARAM_VALUE (PARAM_LARGE_UNIT_INSNS);
gcc/ipa-cp.c:  max_new_size += max_new_size * PARAM_VALUE (PARAM_IPCP_UNIT_GROWTH) / 100 + 1;
gcc/ipa-fnsummary.c:  int op_limit = PARAM_VALUE (PARAM_IPA_MAX_PARAM_EXPR_OPS);
gcc/ipa-fnsummary.c:  int bound_limit = PARAM_VALUE (PARAM_IPA_MAX_SWITCH_PREDICATE_BOUNDS);
gcc/ipa-fnsummary.c:  sreal time = PARAM_VALUE (PARAM_UNINLINED_FUNCTION_TIME);
gcc/ipa-fnsummary.c:  int size = PARAM_VALUE (PARAM_UNINLINED_FUNCTION_INSNS);
gcc/ipa-fnsummary.c:	  fbi.aa_walk_budget = PARAM_VALUE (PARAM_IPA_MAX_AA_STEPS);
gcc/ipa-fnsummary.c:  info->account_size_time  PARAM_VALUE (PARAM_UNINLINED_FUNCTION_INSNS)
gcc/ipa-fnsummary.c:			   PARAM_VALUE (PARAM_UNINLINED_FUNCTION_TIME),
gcc/ipa-fnsummary.c:			       * PARAM_VALUE (PARAM_UNINLINED_FUNCTION_THUNK_INSNS)
gcc/ipa-fnsummary.c:			       PARAM_VALUE (PARAM_UNINLINED_FUNCTION_THUNK_TIME)
gcc/ipa-inline-analysis.c:		     * (100 - PARAM_VALUE (PARAM_COMDAT_SHARING_PROBABILITY))
gcc/ipa-inline.c:  limit += limit * PARAM_VALUE (PARAM_LARGE_FUNCTION_GROWTH) / 100;
gcc/ipa-inline.c:      && newsize > PARAM_VALUE (PARAM_LARGE_FUNCTION_INSNS)
gcc/ipa-inline.c:		       * PARAM_VALUE (PARAM_STACK_FRAME_GROWTH) / 100);
gcc/ipa-inline.c:      && inlined_stack > PARAM_VALUE (PARAM_LARGE_STACK_FRAME))
gcc/ipa-inline.c:	return PARAM_VALUE (PARAM_MAX_INLINE_INSNS_SINGLE)
gcc/ipa-inline.c:	       * PARAM_VALUE (PARAM_INLINE_HEURISTICS_HINT_PERCENT) / 100;
gcc/ipa-inline.c:      return PARAM_VALUE (PARAM_MAX_INLINE_INSNS_SINGLE);
gcc/ipa-inline.c:	return PARAM_VALUE (PARAM_MAX_INLINE_INSNS_SINGLE_O2)
gcc/ipa-inline.c:	       * PARAM_VALUE (PARAM_INLINE_HEURISTICS_HINT_PERCENT_O2) / 100;
gcc/ipa-inline.c:      return PARAM_VALUE (PARAM_MAX_INLINE_INSNS_SINGLE_O2);
gcc/ipa-inline.c:	return PARAM_VALUE (PARAM_MAX_INLINE_INSNS_AUTO)
gcc/ipa-inline.c:	       * PARAM_VALUE (PARAM_INLINE_HEURISTICS_HINT_PERCENT) / 100;
gcc/ipa-inline.c:      return PARAM_VALUE (PARAM_MAX_INLINE_INSNS_AUTO);
gcc/ipa-inline.c:	return PARAM_VALUE (PARAM_MAX_INLINE_INSNS_AUTO_O2)
gcc/ipa-inline.c:	       * PARAM_VALUE (PARAM_INLINE_HEURISTICS_HINT_PERCENT_O2) / 100;
gcc/ipa-inline.c:      return PARAM_VALUE (PARAM_MAX_INLINE_INSNS_AUTO_O2);
gcc/ipa-inline.c:	  if (growth > PARAM_VALUE (PARAM_MAX_INLINE_INSNS_SIZE)
gcc/ipa-inline.c:				 ? PARAM_VALUE (PARAM_EARLY_INLINING_INSNS)
gcc/ipa-inline.c:				 : PARAM_VALUE (PARAM_EARLY_INLINING_INSNS_O2);
gcc/ipa-inline.c:      if (growth <= PARAM_VALUE (PARAM_MAX_INLINE_INSNS_SIZE))
gcc/ipa-inline.c:	      ? PARAM_VALUE (PARAM_INLINE_MIN_SPEEDUP)
gcc/ipa-inline.c:	      : PARAM_VALUE (PARAM_INLINE_MIN_SPEEDUP_O2);
gcc/ipa-inline.c:      if (growth <= PARAM_VALUE (PARAM_MAX_INLINE_INSNS_SIZE))
gcc/ipa-inline.c:	       && growth >= PARAM_VALUE (PARAM_MAX_INLINE_INSNS_SMALL))
gcc/ipa-inline.c:  int max_depth = PARAM_VALUE (PARAM_MAX_INLINE_RECURSIVE_DEPTH_AUTO);
gcc/ipa-inline.c:    max_depth = PARAM_VALUE (PARAM_MAX_INLINE_RECURSIVE_DEPTH);
gcc/ipa-inline.c:	     * PARAM_VALUE (PARAM_MIN_INLINE_RECURSIVE_PROBABILITY))
gcc/ipa-inline.c:		     < PARAM_VALUE (PARAM_PARTIAL_INLINING_ENTRY_PROBABILITY)
gcc/ipa-inline.c:  int limit = PARAM_VALUE (PARAM_MAX_INLINE_INSNS_RECURSIVE_AUTO);
gcc/ipa-inline.c:    limit = PARAM_VALUE (PARAM_MAX_INLINE_INSNS_RECURSIVE);
gcc/ipa-inline.c:  if (max_insns < PARAM_VALUE (PARAM_LARGE_UNIT_INSNS))
gcc/ipa-inline.c:    max_insns = PARAM_VALUE (PARAM_LARGE_UNIT_INSNS);
gcc/ipa-inline.c:	  * (100 + PARAM_VALUE (PARAM_INLINE_UNIT_GROWTH)) / 100);
gcc/ipa-inline.c:      while (iterations < PARAM_VALUE (PARAM_EARLY_INLINER_MAX_ITERATIONS)
gcc/ipa-inline.c:	  if (iterations < PARAM_VALUE (PARAM_EARLY_INLINER_MAX_ITERATIONS) - 1)
gcc/ipa-polymorphic-call.c:  unsigned max = PARAM_VALUE (PARAM_MAX_SPECULATIVE_DEVIRT_MAYDEFS);
gcc/ipa-profile.c:      cutoff = (overall_time * PARAM_VALUE (HOT_BB_COUNT_WS_PERMILLE) + 500) / 1000;
gcc/ipa-prop.c:  int ipa_max_agg_items = PARAM_VALUE (PARAM_IPA_MAX_AGG_ITEMS);
gcc/ipa-prop.c:  fbi.aa_walk_budget = PARAM_VALUE (PARAM_IPA_MAX_AA_STEPS);
gcc/ipa-prop.c:  fbi.aa_walk_budget = PARAM_VALUE (PARAM_IPA_MAX_AA_STEPS);
gcc/ipa-split.c:	    PARAM_VALUE (PARAM_PARTIAL_INLINING_ENTRY_PROBABILITY), 100))))
gcc/ipa-split.c:	 <= (unsigned int) PARAM_VALUE (PARAM_EARLY_INLINING_INSNS) / 2)
gcc/ipa-sra.c:      == (unsigned) PARAM_VALUE (PARAM_IPA_SRA_MAX_REPLACEMENTS))
gcc/ipa-sra.c:	param_size_limit =  PARAM_VALUE (PARAM_IPA_SRA_PTR_GROWTH_FACTOR)
gcc/ipa-sra.c:	  aa_walking_limit = PARAM_VALUE (PARAM_IPA_MAX_AA_STEPS);
gcc/ipa-sra.c:	 > (unsigned) PARAM_VALUE (PARAM_IPA_SRA_MAX_REPLACEMENTS))
gcc/loop-doloop.c:    = COSTS_N_INSNS  PARAM_VALUE (PARAM_MAX_ITERATIONS_COMPUTATION_COST));
gcc/loop-unroll.c:  nunroll = PARAM_VALUE (PARAM_MAX_UNROLLED_INSNS) / loop->ninsns;
gcc/loop-unroll.c:    = PARAM_VALUE (PARAM_MAX_AVERAGE_UNROLLED_INSNS) / loop->av_ninsns;
gcc/loop-unroll.c:  if (nunroll > (unsigned) PARAM_VALUE (PARAM_MAX_UNROLL_TIMES))
gcc/loop-unroll.c:    nunroll = PARAM_VALUE (PARAM_MAX_UNROLL_TIMES);
gcc/loop-unroll.c:  nunroll = PARAM_VALUE (PARAM_MAX_UNROLLED_INSNS) / loop->ninsns;
gcc/loop-unroll.c:  nunroll_by_av = PARAM_VALUE (PARAM_MAX_AVERAGE_UNROLLED_INSNS) / loop->av_ninsns;
gcc/loop-unroll.c:  if (nunroll > (unsigned) PARAM_VALUE (PARAM_MAX_UNROLL_TIMES))
gcc/loop-unroll.c:    nunroll = PARAM_VALUE (PARAM_MAX_UNROLL_TIMES);
gcc/loop-unroll.c:  nunroll = PARAM_VALUE (PARAM_MAX_UNROLLED_INSNS) / loop->ninsns;
gcc/loop-unroll.c:    = PARAM_VALUE (PARAM_MAX_AVERAGE_UNROLLED_INSNS) / loop->av_ninsns;
gcc/loop-unroll.c:  if (nunroll > (unsigned) PARAM_VALUE (PARAM_MAX_UNROLL_TIMES))
gcc/loop-unroll.c:    nunroll = PARAM_VALUE (PARAM_MAX_UNROLL_TIMES);
gcc/loop-unroll.c:  if  PARAM_VALUE (PARAM_MAX_VARIABLE_EXPANSIONS) > ve->expansion_count)
gcc/lto/lto-partition.c:  if  PARAM_VALUE (MIN_PARTITION_SIZE) > max_partition_size)
gcc/lto/lto-partition.c:  if (partition_size < PARAM_VALUE (MIN_PARTITION_SIZE))
gcc/lto/lto-partition.c:    partition_size = PARAM_VALUE (MIN_PARTITION_SIZE);
gcc/lto/lto-partition.c:	  if (partition_size < PARAM_VALUE (MIN_PARTITION_SIZE))
gcc/lto/lto-partition.c:	    partition_size = PARAM_VALUE (MIN_PARTITION_SIZE);
gcc/lto/lto.c:    lto_parallelism = PARAM_VALUE (PARAM_MAX_LTO_STREAMING_PARALLELISM);
gcc/lto/lto.c:      if (lto_parallelism >= PARAM_VALUE (PARAM_MAX_LTO_STREAMING_PARALLELISM))
gcc/lto/lto.c:	lto_parallelism = PARAM_VALUE (PARAM_MAX_LTO_STREAMING_PARALLELISM);
gcc/lto/lto.c:    lto_balanced_map  PARAM_VALUE (PARAM_LTO_PARTITIONS),
gcc/lto/lto.c:		      PARAM_VALUE (MAX_PARTITION_SIZE));
gcc/modulo-sched.c:	  if (stage_count < PARAM_VALUE (PARAM_SMS_MIN_SC)
gcc/predict.c:      const int hot_frac = PARAM_VALUE (HOT_BB_COUNT_FRACTION);
gcc/predict.c:      if (count.apply_scale  PARAM_VALUE (HOT_BB_FREQUENCY_FRACTION), 1)
gcc/predict.c:      const int unlikely_frac = PARAM_VALUE (UNLIKELY_BB_COUNT_FRACTION);
gcc/predict.c:       <= PARAM_VALUE (PARAM_PREDICTABLE_BRANCH_OUTCOME) * REG_BR_PROB_BASE / 100)
gcc/predict.c:          <= PARAM_VALUE (PARAM_PREDICTABLE_BRANCH_OUTCOME) * REG_BR_PROB_BASE / 100))
gcc/predict.c:	  int max = PARAM_VALUE (PARAM_MAX_PREDICTED_ITERATIONS);
gcc/predict.c:		      = HITRATE  PARAM_VALUE (BUILTIN_EXPECT_PROBABILITY));
gcc/predict.c:		    = HITRATE  PARAM_VALUE (BUILTIN_EXPECT_PROBABILITY));
gcc/predict.c:	      int percent = PARAM_VALUE (BUILTIN_EXPECT_PROBABILITY);
gcc/predict.c:  const int unlikely_frac = PARAM_VALUE (UNLIKELY_BB_COUNT_FRACTION);
gcc/reload.c:	  || num > PARAM_VALUE (PARAM_MAX_RELOAD_SEARCH_INSNS))
gcc/sched-ebb.c:    probability_cutoff = PARAM_VALUE (TRACER_MIN_BRANCH_PROBABILITY_FEEDBACK);
gcc/sched-ebb.c:    probability_cutoff = PARAM_VALUE (TRACER_MIN_BRANCH_PROBABILITY);
gcc/sched-rgn.c:      probability_cutoff = PARAM_VALUE (TRACER_MIN_BRANCH_PROBABILITY_FEEDBACK);
gcc/sched-rgn.c:      probability_cutoff = PARAM_VALUE (TRACER_MIN_BRANCH_PROBABILITY);
gcc/sched-rgn.c:  return ((*num_bbs > PARAM_VALUE (PARAM_MAX_SCHED_REGION_BLOCKS))
gcc/sched-rgn.c:	  || (*num_insns > PARAM_VALUE (PARAM_MAX_SCHED_REGION_INSNS)));
gcc/sched-rgn.c:      extend_regions_p = PARAM_VALUE (PARAM_MAX_SCHED_EXTEND_REGIONS_ITERS) > 0;
gcc/sched-rgn.c:  max_iter = PARAM_VALUE (PARAM_MAX_SCHED_EXTEND_REGIONS_ITERS);
gcc/sched-rgn.c:                   > PARAM_VALUE (PARAM_MAX_SCHED_INSN_CONFLICT_DELAY))
gcc/sched-rgn.c:	             PARAM_VALUE (PARAM_SCHED_STATE_EDGE_PROB_CUTOFF)))
gcc/sched-rgn.c:  min_spec_prob = ( PARAM_VALUE (PARAM_MIN_SPEC_PROB) * REG_BR_PROB_BASE)
gcc/sel-sched-ir.c:      > (unsigned) PARAM_VALUE (PARAM_MAX_PIPELINE_REGION_BLOCKS))
gcc/sel-sched-ir.c:  if ((int) loop->ninsns > PARAM_VALUE (PARAM_MAX_PIPELINE_REGION_INSNS))
gcc/sel-sched-ir.h:#define MAX_WS  PARAM_VALUE (PARAM_SELSCHED_MAX_LOOKAHEAD))
gcc/sel-sched.c:	  >= PARAM_VALUE (PARAM_SELSCHED_MAX_SCHED_TIMES))
gcc/sel-sched.c:  max_insns_to_rename = PARAM_VALUE (PARAM_SELSCHED_INSNS_TO_RENAME);
gcc/shrink-wrap.c:  max_grow_size *= PARAM_VALUE (PARAM_MAX_GROW_COPY_BB_INSNS);
gcc/targhooks.c:    return PARAM_VALUE (param);
gcc/toplev.c:	       PARAM_VALUE (GGC_MIN_EXPAND), PARAM_VALUE (GGC_MIN_HEAPSIZE));
gcc/toplev.c:      = PARAM_VALUE (PARAM_HASH_TABLE_VERIFICATION_LIMIT);
gcc/tracer.c:    probability_cutoff = PARAM_VALUE (TRACER_MIN_BRANCH_PROBABILITY_FEEDBACK);
gcc/tracer.c:    probability_cutoff = PARAM_VALUE (TRACER_MIN_BRANCH_PROBABILITY);
gcc/tracer.c:    (REG_BR_PROB_BASE / 100 * PARAM_VALUE (TRACER_MIN_BRANCH_RATIO));
gcc/tracer.c:    cover_insns = PARAM_VALUE (TRACER_DYNAMIC_COVERAGE_FEEDBACK);
gcc/tracer.c:    cover_insns = PARAM_VALUE (TRACER_DYNAMIC_COVERAGE);
gcc/tracer.c:  max_dup_insns = (ninsns * PARAM_VALUE (TRACER_MAX_CODE_GROWTH) + 50) / 100;
gcc/trans-mem.c:	      < PARAM_VALUE (PARAM_TM_MAX_AGGREGATE_SIZE))
gcc/tree-chrec.c:		&& size < PARAM_VALUE (PARAM_SCEV_MAX_EXPR_SIZE))
gcc/tree-chrec.c:	    else if (size < PARAM_VALUE (PARAM_SCEV_MAX_EXPR_SIZE))
gcc/tree-data-ref.c:  unsigned limit = PARAM_VALUE (PARAM_SSA_NAME_DEF_CHAIN_LIMIT);
gcc/tree-data-ref.c:      > PARAM_VALUE (PARAM_LOOP_MAX_DATAREFS_FOR_DATADEPS))
gcc/tree-if-conv.c:  ((unsigned) PARAM_VALUE (PARAM_MAX_TREE_IF_CONVERSION_PHI_ARGS))
gcc/tree-inline.c:	      > PARAM_VALUE (PARAM_MAX_DEBUG_MARKER_COUNT))
gcc/tree-loop-distribution.c:	((unsigned) PARAM_VALUE (PARAM_LOOP_MAX_DATAREFS_FOR_DATADEPS))
gcc/tree-parloops.c:#define MIN_PER_THREAD PARAM_VALUE (PARAM_PARLOOPS_MIN_PER_THREAD)
gcc/tree-parloops.c:      int chunk_size = PARAM_VALUE (PARAM_PARLOOPS_CHUNK_SIZE);
gcc/tree-parloops.c:	= (enum PARAM_PARLOOPS_SCHEDULE_KIND) PARAM_VALUE (PARAM_PARLOOPS_SCHEDULE);
gcc/tree-predcom.c:  unsigned max = PARAM_VALUE (PARAM_MAX_UNROLL_TIMES);
gcc/tree-scalar-evolution.c:      if (limit > PARAM_VALUE (PARAM_SCEV_MAX_EXPR_COMPLEXITY))
gcc/tree-scalar-evolution.c:  if (size_expr++ > PARAM_VALUE (PARAM_SCEV_MAX_EXPR_SIZE))
gcc/tree-sra.c:      ? PARAM_VALUE (param)
gcc/tree-ssa-ccp.c:  threshold = (unsigned HOST_WIDE_INT) PARAM_VALUE (PARAM_LARGE_STACK_FRAME);
gcc/tree-ssa-dse.c:	  <= PARAM_VALUE (PARAM_DSE_MAX_OBJECT_SIZE)))
gcc/tree-ssa-dse.c:      if (++cnt > PARAM_VALUE (PARAM_DSE_MAX_ALIAS_QUERIES_PER_STORE))
gcc/tree-ssa-dse.c:	  if (++cnt > PARAM_VALUE (PARAM_DSE_MAX_ALIAS_QUERIES_PER_STORE))
gcc/tree-ssa-dse.c:    m_live_bytes  PARAM_VALUE (PARAM_DSE_MAX_OBJECT_SIZE)),
gcc/tree-ssa-ifcombine.c:	  if  PARAM_VALUE (PARAM_LOGICAL_OP_NON_SHORT_CIRCUIT) != -1)
gcc/tree-ssa-ifcombine.c:	      = PARAM_VALUE (PARAM_LOGICAL_OP_NON_SHORT_CIRCUIT);
gcc/tree-ssa-loop-ch.c:      int initial_limit = PARAM_VALUE (PARAM_MAX_LOOP_HEADER_INSNS);
gcc/tree-ssa-loop-im.c:#define LIM_EXPENSIVE ((unsigned) PARAM_VALUE (PARAM_LIM_EXPENSIVE))
gcc/tree-ssa-loop-ivcanon.c:      && n_unroll > (unsigned) PARAM_VALUE (PARAM_MAX_COMPLETELY_PEEL_TIMES))
gcc/tree-ssa-loop-ivcanon.c:		 PARAM_VALUE (PARAM_MAX_COMPLETELY_PEELED_INSNS));
gcc/tree-ssa-loop-ivcanon.c:		   > PARAM_VALUE (PARAM_MAX_PEEL_BRANCHES))
gcc/tree-ssa-loop-ivcanon.c:		   > (unsigned) PARAM_VALUE (PARAM_MAX_COMPLETELY_PEELED_INSNS))
gcc/tree-ssa-loop-ivcanon.c:      || PARAM_VALUE (PARAM_MAX_PEEL_TIMES) <= 0
gcc/tree-ssa-loop-ivcanon.c:  if (npeel > PARAM_VALUE (PARAM_MAX_PEEL_TIMES) - 1)
gcc/tree-ssa-loop-ivcanon.c:			   PARAM_VALUE (PARAM_MAX_PEELED_INSNS));
gcc/tree-ssa-loop-ivcanon.c:      > PARAM_VALUE (PARAM_MAX_PEELED_INSNS))
gcc/tree-ssa-loop-ivcanon.c:	 && ++iteration <= PARAM_VALUE (PARAM_MAX_UNROLL_ITERATIONS));
gcc/tree-ssa-loop-ivopts.c:      if (niter == -1 || niter > PARAM_VALUE (PARAM_AVG_LOOP_NITER))
gcc/tree-ssa-loop-ivopts.c:	return PARAM_VALUE (PARAM_AVG_LOOP_NITER);
gcc/tree-ssa-loop-ivopts.c:  ((unsigned) PARAM_VALUE (PARAM_IV_CONSIDER_ALL_CANDIDATES_BOUND))
gcc/tree-ssa-loop-ivopts.c:  ((unsigned) PARAM_VALUE (PARAM_IV_MAX_CONSIDERED_USES))
gcc/tree-ssa-loop-ivopts.c:  ((unsigned) PARAM_VALUE (PARAM_IV_ALWAYS_PRUNE_CAND_SET_BOUND))
gcc/tree-ssa-loop-manip.c:      > (unsigned) PARAM_VALUE (PARAM_MAX_UNROLLED_INSNS))
gcc/tree-ssa-loop-niter.c:  ((unsigned) PARAM_VALUE (PARAM_MAX_ITERATIONS_TO_TRACK))
gcc/tree-ssa-loop-prefetch.c:  upper_bound = PARAM_VALUE (PARAM_MAX_UNROLLED_INSNS) / ninsns;
gcc/tree-ssa-loop-unswitch.c:	  > (unsigned) PARAM_VALUE (PARAM_MAX_UNSWITCH_INSNS))
gcc/tree-ssa-loop-unswitch.c:	      && num > PARAM_VALUE (PARAM_MAX_UNSWITCH_LEVEL)
gcc/tree-ssa-loop-unswitch.c:      else if (num > PARAM_VALUE (PARAM_MAX_UNSWITCH_LEVEL))
gcc/tree-ssa-math-opts.c:				? PARAM_VALUE (PARAM_MAX_POW_SQRT_DEPTH)
gcc/tree-ssa-math-opts.c:	   <= PARAM_VALUE (PARAM_AVOID_FMA_MAX_BITS)));
gcc/tree-ssa-math-opts.c:  fma_deferring_state fma_state  PARAM_VALUE (PARAM_AVOID_FMA_MAX_BITS) > 0);
gcc/tree-ssa-phiopt.c:  int param_align = PARAM_VALUE (PARAM_L1_CACHE_LINE_SIZE);
gcc/tree-ssa-phiopt.c:	  && PARAM_VALUE (PARAM_L1_CACHE_LINE_SIZE)
gcc/tree-ssa-pre.c:  unsigned int cnt = PARAM_VALUE (PARAM_SCCVN_MAX_ALIAS_QUERIES_PER_ACCESS);
gcc/tree-ssa-pre.c:  unsigned long max_pa = PARAM_VALUE (PARAM_MAX_PARTIAL_ANTIC_LENGTH);
gcc/tree-ssa-reassoc.c:  int param_width = PARAM_VALUE (PARAM_TREE_REASSOC_WIDTH);
gcc/tree-ssa-sccvn.c:      unsigned limit = PARAM_VALUE (PARAM_SCCVN_MAX_ALIAS_QUERIES_PER_ACCESS);
gcc/tree-ssa-sccvn.c:      unsigned limit = PARAM_VALUE (PARAM_SCCVN_MAX_ALIAS_QUERIES_PER_ACCESS);
gcc/tree-ssa-sccvn.c:      unsigned max_depth = PARAM_VALUE (PARAM_RPO_VN_MAX_LOOP_DEPTH);
gcc/tree-ssa-scopedtables.c:      unsigned limit = PARAM_VALUE (PARAM_SCCVN_MAX_ALIAS_QUERIES_PER_ACCESS);
gcc/tree-ssa-sink.c:  threshold = PARAM_VALUE (PARAM_SINK_FREQUENCY_THRESHOLD);
gcc/tree-ssa-strlen.c:  if (max_stridx >= PARAM_VALUE (PARAM_MAX_TRACKED_STRLENS))
gcc/tree-ssa-strlen.c:  if (max_stridx >= PARAM_VALUE (PARAM_MAX_TRACKED_STRLENS))
gcc/tree-ssa-strlen.c:  unsigned limit = PARAM_VALUE (PARAM_SSA_NAME_DEF_CHAIN_LIMIT);
gcc/tree-ssa-strlen.c:    ssa_def_max  PARAM_VALUE (PARAM_SSA_NAME_DEF_CHAIN_LIMIT)) { }
gcc/tree-ssa-tail-merge.c:  int max_comparisons = PARAM_VALUE (PARAM_MAX_TAIL_MERGE_COMPARISONS);
gcc/tree-ssa-tail-merge.c:  int max_iterations = PARAM_VALUE (PARAM_MAX_TAIL_MERGE_ITERATIONS);
gcc/tree-ssa-threadbackward.c:      > (unsigned) PARAM_VALUE (PARAM_MAX_FSM_THREAD_LENGTH))
gcc/tree-ssa-threadbackward.c:      if (n_insns >= PARAM_VALUE (PARAM_MAX_FSM_THREAD_PATH_INSNS))
gcc/tree-ssa-threadbackward.c:      && (n_insns * (unsigned) PARAM_VALUE (PARAM_FSM_SCALE_PATH_STMTS)
gcc/tree-ssa-threadbackward.c:	     (unsigned) PARAM_VALUE (PARAM_FSM_SCALE_PATH_BLOCKS))))
gcc/tree-ssa-threadbackward.c:      && (n_insns * PARAM_VALUE (PARAM_FSM_SCALE_PATH_STMTS)
gcc/tree-ssa-threadbackward.c:	  >= PARAM_VALUE (PARAM_MAX_JUMP_THREAD_DUPLICATION_STMTS)))
gcc/tree-ssa-threadbackward.c:	  >= (unsigned) PARAM_VALUE (PARAM_FSM_MAXIMUM_PHI_ARGUMENTS)))
gcc/tree-ssa-threadbackward.c:  m_max_threaded_paths = PARAM_VALUE (PARAM_MAX_FSM_THREAD_PATHS);
gcc/tree-ssa-threadedge.c:  max_stmt_count = PARAM_VALUE (PARAM_MAX_JUMP_THREAD_DUPLICATION_STMTS);
gcc/tree-ssa-threadedge.c:	      == PARAM_VALUE (PARAM_MAX_JUMP_THREAD_DUPLICATION_STMTS))
gcc/tree-ssa-uninit.c:  if (*num_calls > PARAM_VALUE (PARAM_UNINIT_CONTROL_DEP_ATTEMPTS))
gcc/tree-switch-conversion.c:       ? PARAM_VALUE (PARAM_JUMP_TABLE_MAX_GROWTH_RATIO_FOR_SIZE)
gcc/tree-switch-conversion.c:       : PARAM_VALUE (PARAM_JUMP_TABLE_MAX_GROWTH_RATIO_FOR_SPEED));
gcc/tree-switch-conversion.h:  unsigned int threshold = PARAM_VALUE (PARAM_CASE_VALUES_THRESHOLD);
gcc/tree-vect-data-refs.c:  if ((unsigned) PARAM_VALUE (PARAM_VECT_MAX_VERSION_FOR_ALIAS_CHECKS) == 0)
gcc/tree-vect-data-refs.c:            = PARAM_VALUE (PARAM_VECT_MAX_PEELING_FOR_ALIGNMENT);
gcc/tree-vect-data-refs.c:                     >= (unsigned) PARAM_VALUE (PARAM_VECT_MAX_VERSION_FOR_ALIGNMENT_CHECKS))
gcc/tree-vect-data-refs.c:  unsigned limit = PARAM_VALUE (PARAM_VECT_MAX_VERSION_FOR_ALIAS_CHECKS);
gcc/tree-vect-loop.c:  int min_scalar_loop_bound =  PARAM_VALUE (PARAM_MIN_VECT_LOOP_BOUND)
gcc/tree-vect-loop.c:	    > (unsigned) PARAM_VALUE (PARAM_LOOP_MAX_DATAREFS_FOR_DATADEPS))
gcc/tree-vect-loop.c:      && PARAM_VALUE (PARAM_VECT_EPILOGUES_NOMASK);
gcc/tree-vect-slp.c:      if (insns > PARAM_VALUE (PARAM_SLP_MAX_INSNS_IN_BB))
gcc/tree-vrp.c:  int insertion_limit = PARAM_VALUE (PARAM_MAX_VRP_SWITCH_ASSERTIONS);
gcc/tree-vrp.c:  const unsigned limit = PARAM_VALUE (PARAM_SSA_NAME_DEF_CHAIN_LIMIT);
gcc/var-tracking.c:#define EXPR_USE_DEPTH  PARAM_VALUE (PARAM_MAX_VARTRACK_EXPR_DEPTH))
gcc/var-tracking.c:    else if (count == PARAM_VALUE (PARAM_MAX_VARTRACK_REVERSE_OP_SIZE))
gcc/var-tracking.c:  int htabmax = PARAM_VALUE (PARAM_MAX_VARTRACK_SIZE);
"""

global_params_text = """
PARAM_INLINE_MIN_SPEEDUP
PARAM_INLINE_MIN_SPEEDUP_O2
PARAM_MAX_INLINE_INSNS_SINGLE
PARAM_MAX_INLINE_INSNS_SINGLE_O2
PARAM_MAX_INLINE_INSNS_AUTO
PARAM_MAX_INLINE_INSNS_AUTO_O2
PARAM_MAX_INLINE_INSNS_SMALL
PARAM_INLINE_HEURISTICS_HINT_PERCENT_O2
PARAM_UNINLINED_FUNCTION_INSNS
PARAM_UNINLINED_FUNCTION_TIME
PARAM_UNINLINED_FUNCTION_THUNK_INSNS
PARAM_UNINLINED_FUNCTION_THUNK_TIME
PARAM_MAX_INLINE_INSNS_RECURSIVE
PARAM_MAX_INLINE_INSNS_RECURSIVE_AUTO
PARAM_MAX_INLINE_RECURSIVE_DEPTH
PARAM_MAX_INLINE_RECURSIVE_DEPTH_AUTO
PARAM_MIN_INLINE_RECURSIVE_PROBABILITY
PARAM_EARLY_INLINER_MAX_ITERATIONS
PARAM_COMDAT_SHARING_PROBABILITY
PARAM_PARTIAL_INLINING_ENTRY_PROBABILITY
PARAM_LARGE_FUNCTION_INSNS
PARAM_LARGE_FUNCTION_GROWTH
PARAM_LARGE_UNIT_INSNS
PARAM_INLINE_UNIT_GROWTH
PARAM_IPCP_UNIT_GROWTH
PARAM_EARLY_INLINING_INSNS
PARAM_EARLY_INLINING_INSNS_O2
PARAM_LARGE_STACK_FRAME
PARAM_STACK_FRAME_GROWTH
HOT_BB_COUNT_FRACTION
HOT_BB_COUNT_WS_PERMILLE
HOT_BB_FREQUENCY_FRACTION
UNLIKELY_BB_COUNT_FRACTION
GGC_MIN_EXPAND
GGC_MIN_HEAPSIZE
PARAM_INTEGER_SHARE_LIMIT
PARAM_USE_CANONICAL_TYPES
PARAM_PROFILE_FUNC_INTERNAL_ID
PARAM_MAX_VARTRACK_SIZE
PARAM_MAX_VARTRACK_EXPR_DEPTH
PARAM_MAX_VARTRACK_REVERSE_OP_SIZE
PARAM_MAX_DEBUG_MARKER_COUNT
PARAM_MIN_NONDEBUG_INSN_UID
PARAM_IPA_SRA_PTR_GROWTH_FACTOR
PARAM_IPA_SRA_MAX_REPLACEMENTS
PARAM_IPA_CP_VALUE_LIST_SIZE
PARAM_IPA_CP_EVAL_THRESHOLD
PARAM_IPA_CP_RECURSION_PENALTY
PARAM_IPA_CP_SINGLE_CALL_PENALTY
PARAM_IPA_MAX_AGG_ITEMS
PARAM_IPA_CP_LOOP_HINT_BONUS
PARAM_IPA_MAX_AA_STEPS
PARAM_IPA_MAX_SWITCH_PREDICATE_BOUNDS
PARAM_IPA_MAX_PARAM_EXPR_OPS
PARAM_LTO_PARTITIONS
MIN_PARTITION_SIZE
MAX_PARTITION_SIZE
PARAM_MAX_LTO_STREAMING_PARALLELISM
CXX_MAX_NAMESPACES_FOR_DIAGNOSTIC_HELP
PARAM_ASAN_STACK
PARAM_ASAN_PROTECT_ALLOCAS
PARAM_ASAN_GLOBALS
PARAM_ASAN_INSTRUMENT_WRITES
PARAM_ASAN_INSTRUMENT_READS
PARAM_ASAN_MEMINTRIN
PARAM_ASAN_USE_AFTER_RETURN
PARAM_HSA_GEN_DEBUG_STORES
PARAM_MAX_SPECULATIVE_DEVIRT_MAYDEFS
PARAM_GIMPLE_FE_COMPUTED_HOT_BB_THRESHOLD
PARAM_HASH_TABLE_VERIFICATION_LIMIT
"""

global_params = set([x.strip() for x in global_params_text.split('\n') if x])

used = set()
for line in usage.strip().split('\n'):
  tokens = line.split(' ')
  index = tokens.index('PARAM_VALUE')
  name = tokens[index + 1]
  assert name[0] == '('
  assert ')' in name
  name = name[1:]
  name = name[:name.index(')')]
  used.add(name)

class Param:
  def __init__(self, tokens):
    self.enum = tokens[0]
    self.name = tokens[1]
    self.default = int(tokens[2])
    self.min = int(tokens[3])
    self.max = int(tokens[4])
    self.description = tokens[5]

  def canonical_enum(self):
    e = self.enum.lower()
    if not e.startswith('param_'):
      e = 'param_' + e
    return e

params = [Param(x.split(';')) for x in open('params.txt').readlines()]
params = sorted(params, key = attrgetter('name'))

with open('replacement.txt', 'w+') as f:
  for k, v in mapping.items():
    if not v.startswith('PARAM_'):
      v = 'PARAM_' + v
    r = ' s/%s/%s/g' % (k, v.lower())
    f.write('find . -type f -name "*.h" -exec sed -i "%s" {} +\n' % r)
    f.write('find . -type f -name "*.c" -exec sed -i "%s" {} +\n' % r)
    f.write('find . -type f -name "*.cc" -exec sed -i "%s" {} +\n' % r)

  for p in params:
    r = ' s/PARAM_VALUE (%s)/%s/g' % (p.enum, p.canonical_enum())
    f.write('find . -type f -name "*.h" -exec sed -i "%s" {} +\n' % r)
    f.write('find . -type f -name "*.c" -exec sed -i "%s" {} +\n' % r)
    f.write('find . -type f -name "*.cc" -exec sed -i "%s" {} +\n' % r)

exit(0)

#for p in params:
#  if not p.enum in used and not p.enum in mapping.values():
#    print('Not used: %s' % p.enum)

params_dict = { p.enum : p for p in params }

for k, v in mapping.items():
  assert v in params_dict

for p in params:
  print('-param=%s=' % p.name)
  print('Common Joined UInteger Var(%s)' % p.canonical_enum(), end = '')
  if not p.enum in global_params:
    print(' Optimization', end = '')
  if p.default != 0:
    print(' Init(%d)' % p.default, end = '')
  if p.min != 0 or p.max != 0:
    print(' IntegerRange(%d, %d)' % (p.min, p.max), end = '')
  print(' Param')
  print(p.description)
