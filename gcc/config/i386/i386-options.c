/* Subroutines used for code generation on IA-32.
   Copyright (C) 1988-2019 Free Software Foundation, Inc.

This file is part of GCC.

GCC is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 3, or (at your option)
any later version.

GCC is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GCC; see the file COPYING3.  If not see
<http://www.gnu.org/licenses/>.  */

#define IN_TARGET_CODE 1

#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "backend.h"
#include "rtl.h"
#include "tree.h"
#include "memmodel.h"
#include "gimple.h"
#include "cfghooks.h"
#include "cfgloop.h"
#include "df.h"
#include "tm_p.h"
#include "stringpool.h"
#include "expmed.h"
#include "optabs.h"
#include "regs.h"
#include "emit-rtl.h"
#include "recog.h"
#include "cgraph.h"
#include "diagnostic.h"
#include "cfgbuild.h"
#include "alias.h"
#include "fold-const.h"
#include "attribs.h"
#include "calls.h"
#include "stor-layout.h"
#include "varasm.h"
#include "output.h"
#include "insn-attr.h"
#include "flags.h"
#include "except.h"
#include "explow.h"
#include "expr.h"
#include "cfgrtl.h"
#include "common/common-target.h"
#include "langhooks.h"
#include "reload.h"
#include "gimplify.h"
#include "dwarf2.h"
#include "tm-constrs.h"
#include "params.h"
#include "cselib.h"
#include "sched-int.h"
#include "opts.h"
#include "tree-pass.h"
#include "context.h"
#include "pass_manager.h"
#include "target-globals.h"
#include "gimple-iterator.h"
#include "tree-vectorizer.h"
#include "shrink-wrap.h"
#include "builtins.h"
#include "rtl-iter.h"
#include "tree-iterator.h"
#include "dbgcnt.h"
#include "case-cfn-macros.h"
#include "dojump.h"
#include "fold-const-call.h"
#include "tree-vrp.h"
#include "tree-ssanames.h"
#include "selftest.h"
#include "selftest-rtl.h"
#include "print-rtl.h"
#include "intl.h"
#include "ifcvt.h"
#include "symbol-summary.h"
#include "ipa-prop.h"
#include "ipa-fnsummary.h"
#include "wide-int-bitmask.h"
#include "tree-vector-builder.h"
#include "debug.h"
#include "dwarf2out.h"

/* Return a string that documents the current -m options.  The caller is
   responsible for freeing the string.  */

char *
ix86_target_string (HOST_WIDE_INT isa, HOST_WIDE_INT isa2,
		    int flags, int flags2,
		    const char *arch, const char *tune,
		    enum fpmath_unit fpmath, bool add_nl_p)
{
  struct ix86_target_opts
  {
    const char *option;		/* option string */
    HOST_WIDE_INT mask;		/* isa mask options */
  };

  /* This table is ordered so that options like -msse4.2 that imply other
     ISAs come first.  Target string will be displayed in the same order.  */
  static struct ix86_target_opts isa2_opts[] =
  {
    { "-mcx16",		OPTION_MASK_ISA_CX16 },
    { "-mvaes",		OPTION_MASK_ISA_VAES },
    { "-mrdpid",	OPTION_MASK_ISA_RDPID },
    { "-mpconfig",	OPTION_MASK_ISA_PCONFIG },
    { "-mwbnoinvd",     OPTION_MASK_ISA_WBNOINVD },
    { "-msgx",		OPTION_MASK_ISA_SGX },
    { "-mavx5124vnniw", OPTION_MASK_ISA_AVX5124VNNIW },
    { "-mavx5124fmaps", OPTION_MASK_ISA_AVX5124FMAPS },
    { "-mhle",		OPTION_MASK_ISA_HLE },
    { "-mmovbe",	OPTION_MASK_ISA_MOVBE },
    { "-mclzero",	OPTION_MASK_ISA_CLZERO },
    { "-mmwaitx",	OPTION_MASK_ISA_MWAITX },
    { "-mmovdir64b",	OPTION_MASK_ISA_MOVDIR64B },
    { "-mwaitpkg",	OPTION_MASK_ISA_WAITPKG },
    { "-mcldemote",	OPTION_MASK_ISA_CLDEMOTE },
    { "-mptwrite",	OPTION_MASK_ISA_PTWRITE }
  };
  static struct ix86_target_opts isa_opts[] =
  {
    { "-mavx512vpopcntdq", OPTION_MASK_ISA_AVX512VPOPCNTDQ },
    { "-mavx512bitalg", OPTION_MASK_ISA_AVX512BITALG },
    { "-mvpclmulqdq",	OPTION_MASK_ISA_VPCLMULQDQ },
    { "-mgfni",		OPTION_MASK_ISA_GFNI },
    { "-mavx512vnni",	OPTION_MASK_ISA_AVX512VNNI },
    { "-mavx512vbmi2",	OPTION_MASK_ISA_AVX512VBMI2 },
    { "-mavx512vbmi",	OPTION_MASK_ISA_AVX512VBMI },
    { "-mavx512ifma",	OPTION_MASK_ISA_AVX512IFMA },
    { "-mavx512vl",	OPTION_MASK_ISA_AVX512VL },
    { "-mavx512bw",	OPTION_MASK_ISA_AVX512BW },
    { "-mavx512dq",	OPTION_MASK_ISA_AVX512DQ },
    { "-mavx512er",	OPTION_MASK_ISA_AVX512ER },
    { "-mavx512pf",	OPTION_MASK_ISA_AVX512PF },
    { "-mavx512cd",	OPTION_MASK_ISA_AVX512CD },
    { "-mavx512f",	OPTION_MASK_ISA_AVX512F },
    { "-mavx2",		OPTION_MASK_ISA_AVX2 },
    { "-mfma",		OPTION_MASK_ISA_FMA },
    { "-mxop",		OPTION_MASK_ISA_XOP },
    { "-mfma4",		OPTION_MASK_ISA_FMA4 },
    { "-mf16c",		OPTION_MASK_ISA_F16C },
    { "-mavx",		OPTION_MASK_ISA_AVX },
/*  { "-msse4"		OPTION_MASK_ISA_SSE4 }, */
    { "-msse4.2",	OPTION_MASK_ISA_SSE4_2 },
    { "-msse4.1",	OPTION_MASK_ISA_SSE4_1 },
    { "-msse4a",	OPTION_MASK_ISA_SSE4A },
    { "-mssse3",	OPTION_MASK_ISA_SSSE3 },
    { "-msse3",		OPTION_MASK_ISA_SSE3 },
    { "-maes",		OPTION_MASK_ISA_AES },
    { "-msha",		OPTION_MASK_ISA_SHA },
    { "-mpclmul",	OPTION_MASK_ISA_PCLMUL },
    { "-msse2",		OPTION_MASK_ISA_SSE2 },
    { "-msse",		OPTION_MASK_ISA_SSE },
    { "-m3dnowa",	OPTION_MASK_ISA_3DNOW_A },
    { "-m3dnow",	OPTION_MASK_ISA_3DNOW },
    { "-mmmx",		OPTION_MASK_ISA_MMX },
    { "-mrtm",		OPTION_MASK_ISA_RTM },
    { "-mprfchw",	OPTION_MASK_ISA_PRFCHW },
    { "-mrdseed",	OPTION_MASK_ISA_RDSEED },
    { "-madx",		OPTION_MASK_ISA_ADX },
    { "-mprefetchwt1",	OPTION_MASK_ISA_PREFETCHWT1 },
    { "-mclflushopt",	OPTION_MASK_ISA_CLFLUSHOPT },
    { "-mxsaves",	OPTION_MASK_ISA_XSAVES },
    { "-mxsavec",	OPTION_MASK_ISA_XSAVEC },
    { "-mxsaveopt",	OPTION_MASK_ISA_XSAVEOPT },
    { "-mxsave",	OPTION_MASK_ISA_XSAVE },
    { "-mabm",		OPTION_MASK_ISA_ABM },
    { "-mbmi",		OPTION_MASK_ISA_BMI },
    { "-mbmi2",		OPTION_MASK_ISA_BMI2 },
    { "-mlzcnt",	OPTION_MASK_ISA_LZCNT },
    { "-mtbm",		OPTION_MASK_ISA_TBM },
    { "-mpopcnt",	OPTION_MASK_ISA_POPCNT },
    { "-msahf",		OPTION_MASK_ISA_SAHF },
    { "-mcrc32",	OPTION_MASK_ISA_CRC32 },
    { "-mfsgsbase",	OPTION_MASK_ISA_FSGSBASE },
    { "-mrdrnd",	OPTION_MASK_ISA_RDRND },
    { "-mpku",		OPTION_MASK_ISA_PKU },
    { "-mlwp",		OPTION_MASK_ISA_LWP },
    { "-mfxsr",		OPTION_MASK_ISA_FXSR },
    { "-mclwb",		OPTION_MASK_ISA_CLWB },
    { "-mshstk",	OPTION_MASK_ISA_SHSTK },
    { "-mmovdiri",	OPTION_MASK_ISA_MOVDIRI }
  };

  /* Flag options.  */
  static struct ix86_target_opts flag_opts[] =
  {
    { "-m128bit-long-double",		MASK_128BIT_LONG_DOUBLE },
    { "-mlong-double-128",		MASK_LONG_DOUBLE_128 },
    { "-mlong-double-64",		MASK_LONG_DOUBLE_64 },
    { "-m80387",			MASK_80387 },
    { "-maccumulate-outgoing-args",	MASK_ACCUMULATE_OUTGOING_ARGS },
    { "-malign-double",			MASK_ALIGN_DOUBLE },
    { "-mcld",				MASK_CLD },
    { "-mfp-ret-in-387",		MASK_FLOAT_RETURNS },
    { "-mieee-fp",			MASK_IEEE_FP },
    { "-minline-all-stringops",		MASK_INLINE_ALL_STRINGOPS },
    { "-minline-stringops-dynamically",	MASK_INLINE_STRINGOPS_DYNAMICALLY },
    { "-mms-bitfields",			MASK_MS_BITFIELD_LAYOUT },
    { "-mno-align-stringops",		MASK_NO_ALIGN_STRINGOPS },
    { "-mno-fancy-math-387",		MASK_NO_FANCY_MATH_387 },
    { "-mno-push-args",			MASK_NO_PUSH_ARGS },
    { "-mno-red-zone",			MASK_NO_RED_ZONE },
    { "-momit-leaf-frame-pointer",	MASK_OMIT_LEAF_FRAME_POINTER },
    { "-mrecip",			MASK_RECIP },
    { "-mrtd",				MASK_RTD },
    { "-msseregparm",			MASK_SSEREGPARM },
    { "-mstack-arg-probe",		MASK_STACK_PROBE },
    { "-mtls-direct-seg-refs",		MASK_TLS_DIRECT_SEG_REFS },
    { "-mvect8-ret-in-mem",		MASK_VECT8_RETURNS },
    { "-m8bit-idiv",			MASK_USE_8BIT_IDIV },
    { "-mvzeroupper",			MASK_VZEROUPPER },
    { "-mstv",				MASK_STV },
    { "-mavx256-split-unaligned-load",	MASK_AVX256_SPLIT_UNALIGNED_LOAD },
    { "-mavx256-split-unaligned-store",	MASK_AVX256_SPLIT_UNALIGNED_STORE },
    { "-mcall-ms2sysv-xlogues",		MASK_CALL_MS2SYSV_XLOGUES }
  };

  /* Additional flag options.  */
  static struct ix86_target_opts flag2_opts[] =
  {
    { "-mgeneral-regs-only",		OPTION_MASK_GENERAL_REGS_ONLY }
  };

  const char *opts[ARRAY_SIZE (isa_opts) + ARRAY_SIZE (isa2_opts)
		   + ARRAY_SIZE (flag_opts) + ARRAY_SIZE (flag2_opts) + 6][2];

  char isa_other[40];
  char isa2_other[40];
  char flags_other[40];
  char flags2_other[40];
  unsigned num = 0;
  unsigned i, j;
  char *ret;
  char *ptr;
  size_t len;
  size_t line_len;
  size_t sep_len;
  const char *abi;

  memset (opts, '\0', sizeof (opts));

  /* Add -march= option.  */
  if (arch)
    {
      opts[num][0] = "-march=";
      opts[num++][1] = arch;
    }

  /* Add -mtune= option.  */
  if (tune)
    {
      opts[num][0] = "-mtune=";
      opts[num++][1] = tune;
    }

  /* Add -m32/-m64/-mx32.  */
  if ((isa & OPTION_MASK_ISA_64BIT) != 0)
    {
      if ((isa & OPTION_MASK_ABI_64) != 0)
	abi = "-m64";
      else
	abi = "-mx32";
      isa &= ~ (OPTION_MASK_ISA_64BIT
		| OPTION_MASK_ABI_64
		| OPTION_MASK_ABI_X32);
    }
  else
    abi = "-m32";
  opts[num++][0] = abi;

  /* Pick out the options in isa2 options.  */
  for (i = 0; i < ARRAY_SIZE (isa2_opts); i++)
    {
      if ((isa2 & isa2_opts[i].mask) != 0)
	{
	  opts[num++][0] = isa2_opts[i].option;
	  isa2 &= ~ isa2_opts[i].mask;
	}
    }

  if (isa2 && add_nl_p)
    {
      opts[num++][0] = isa2_other;
      sprintf (isa2_other, "(other isa2: %#" HOST_WIDE_INT_PRINT "x)", isa2);
    }

  /* Pick out the options in isa options.  */
  for (i = 0; i < ARRAY_SIZE (isa_opts); i++)
    {
      if ((isa & isa_opts[i].mask) != 0)
	{
	  opts[num++][0] = isa_opts[i].option;
	  isa &= ~ isa_opts[i].mask;
	}
    }

  if (isa && add_nl_p)
    {
      opts[num++][0] = isa_other;
      sprintf (isa_other, "(other isa: %#" HOST_WIDE_INT_PRINT "x)", isa);
    }

  /* Add flag options.  */
  for (i = 0; i < ARRAY_SIZE (flag_opts); i++)
    {
      if ((flags & flag_opts[i].mask) != 0)
	{
	  opts[num++][0] = flag_opts[i].option;
	  flags &= ~ flag_opts[i].mask;
	}
    }

  if (flags && add_nl_p)
    {
      opts[num++][0] = flags_other;
      sprintf (flags_other, "(other flags: %#x)", flags);
    }

    /* Add additional flag options.  */
  for (i = 0; i < ARRAY_SIZE (flag2_opts); i++)
    {
      if ((flags2 & flag2_opts[i].mask) != 0)
	{
	  opts[num++][0] = flag2_opts[i].option;
	  flags2 &= ~ flag2_opts[i].mask;
	}
    }

  if (flags2 && add_nl_p)
    {
      opts[num++][0] = flags2_other;
      sprintf (flags2_other, "(other flags2: %#x)", flags2);
    }

  /* Add -fpmath= option.  */
  if (fpmath)
    {
      opts[num][0] = "-mfpmath=";
      switch ((int) fpmath)
	{
	case FPMATH_387:
	  opts[num++][1] = "387";
	  break;

	case FPMATH_SSE:
	  opts[num++][1] = "sse";
	  break;

	case FPMATH_387 | FPMATH_SSE:
	  opts[num++][1] = "sse+387";
	  break;

	default:
	  gcc_unreachable ();
	}
    }

  /* Any options?  */
  if (num == 0)
    return NULL;

  gcc_assert (num < ARRAY_SIZE (opts));

  /* Size the string.  */
  len = 0;
  sep_len = (add_nl_p) ? 3 : 1;
  for (i = 0; i < num; i++)
    {
      len += sep_len;
      for (j = 0; j < 2; j++)
	if (opts[i][j])
	  len += strlen (opts[i][j]);
    }

  /* Build the string.  */
  ret = ptr = (char *) xmalloc (len);
  line_len = 0;

  for (i = 0; i < num; i++)
    {
      size_t len2[2];

      for (j = 0; j < 2; j++)
	len2[j] = (opts[i][j]) ? strlen (opts[i][j]) : 0;

      if (i != 0)
	{
	  *ptr++ = ' ';
	  line_len++;

	  if (add_nl_p && line_len + len2[0] + len2[1] > 70)
	    {
	      *ptr++ = '\\';
	      *ptr++ = '\n';
	      line_len = 0;
	    }
	}

      for (j = 0; j < 2; j++)
	if (opts[i][j])
	  {
	    memcpy (ptr, opts[i][j], len2[j]);
	    ptr += len2[j];
	    line_len += len2[j];
	  }
    }

  *ptr = '\0';
  gcc_assert (ret + len >= ptr);

  return ret;
}

/* Function that is callable from the debugger to print the current
   options.  */
void ATTRIBUTE_UNUSED
ix86_debug_options (void)
{
  char *opts = ix86_target_string (ix86_isa_flags, ix86_isa_flags2,
				   target_flags, ix86_target_flags,
				   ix86_arch_string,ix86_tune_string,
				   ix86_fpmath, true);

  if (opts)
    {
      fprintf (stderr, "%s\n\n", opts);
      free (opts);
    }
  else
    fputs ("<no options>\n\n", stderr);

  return;
}

/* Save the current options */

void
ix86_function_specific_save (struct cl_target_option *ptr,
			     struct gcc_options *opts)
{
  ptr->arch = ix86_arch;
  ptr->schedule = ix86_schedule;
  ptr->prefetch_sse = x86_prefetch_sse;
  ptr->tune = ix86_tune;
  ptr->branch_cost = ix86_branch_cost;
  ptr->tune_defaulted = ix86_tune_defaulted;
  ptr->arch_specified = ix86_arch_specified;
  ptr->x_ix86_isa_flags_explicit = opts->x_ix86_isa_flags_explicit;
  ptr->x_ix86_isa_flags2_explicit = opts->x_ix86_isa_flags2_explicit;
  ptr->x_recip_mask_explicit = opts->x_recip_mask_explicit;
  ptr->x_ix86_arch_string = opts->x_ix86_arch_string;
  ptr->x_ix86_tune_string = opts->x_ix86_tune_string;
  ptr->x_ix86_cmodel = opts->x_ix86_cmodel;
  ptr->x_ix86_abi = opts->x_ix86_abi;
  ptr->x_ix86_asm_dialect = opts->x_ix86_asm_dialect;
  ptr->x_ix86_branch_cost = opts->x_ix86_branch_cost;
  ptr->x_ix86_dump_tunes = opts->x_ix86_dump_tunes;
  ptr->x_ix86_force_align_arg_pointer = opts->x_ix86_force_align_arg_pointer;
  ptr->x_ix86_force_drap = opts->x_ix86_force_drap;
  ptr->x_ix86_incoming_stack_boundary_arg = opts->x_ix86_incoming_stack_boundary_arg;
  ptr->x_ix86_pmode = opts->x_ix86_pmode;
  ptr->x_ix86_preferred_stack_boundary_arg = opts->x_ix86_preferred_stack_boundary_arg;
  ptr->x_ix86_recip_name = opts->x_ix86_recip_name;
  ptr->x_ix86_regparm = opts->x_ix86_regparm;
  ptr->x_ix86_section_threshold = opts->x_ix86_section_threshold;
  ptr->x_ix86_sse2avx = opts->x_ix86_sse2avx;
  ptr->x_ix86_stack_protector_guard = opts->x_ix86_stack_protector_guard;
  ptr->x_ix86_stringop_alg = opts->x_ix86_stringop_alg;
  ptr->x_ix86_tls_dialect = opts->x_ix86_tls_dialect;
  ptr->x_ix86_tune_ctrl_string = opts->x_ix86_tune_ctrl_string;
  ptr->x_ix86_tune_memcpy_strategy = opts->x_ix86_tune_memcpy_strategy;
  ptr->x_ix86_tune_memset_strategy = opts->x_ix86_tune_memset_strategy;
  ptr->x_ix86_tune_no_default = opts->x_ix86_tune_no_default;
  ptr->x_ix86_veclibabi_type = opts->x_ix86_veclibabi_type;

  /* The fields are char but the variables are not; make sure the
     values fit in the fields.  */
  gcc_assert (ptr->arch == ix86_arch);
  gcc_assert (ptr->schedule == ix86_schedule);
  gcc_assert (ptr->tune == ix86_tune);
  gcc_assert (ptr->branch_cost == ix86_branch_cost);
}

/* Restore the current options */

void
ix86_function_specific_restore (struct gcc_options *opts,
				struct cl_target_option *ptr)
{
  enum processor_type old_tune = ix86_tune;
  enum processor_type old_arch = ix86_arch;
  unsigned HOST_WIDE_INT ix86_arch_mask;
  int i;

  /* We don't change -fPIC.  */
  opts->x_flag_pic = flag_pic;

  ix86_arch = (enum processor_type) ptr->arch;
  ix86_schedule = (enum attr_cpu) ptr->schedule;
  ix86_tune = (enum processor_type) ptr->tune;
  x86_prefetch_sse = ptr->prefetch_sse;
  opts->x_ix86_branch_cost = ptr->branch_cost;
  ix86_tune_defaulted = ptr->tune_defaulted;
  ix86_arch_specified = ptr->arch_specified;
  opts->x_ix86_isa_flags_explicit = ptr->x_ix86_isa_flags_explicit;
  opts->x_ix86_isa_flags2_explicit = ptr->x_ix86_isa_flags2_explicit;
  opts->x_recip_mask_explicit = ptr->x_recip_mask_explicit;
  opts->x_ix86_arch_string = ptr->x_ix86_arch_string;
  opts->x_ix86_tune_string = ptr->x_ix86_tune_string;
  opts->x_ix86_cmodel = ptr->x_ix86_cmodel;
  opts->x_ix86_abi = ptr->x_ix86_abi;
  opts->x_ix86_asm_dialect = ptr->x_ix86_asm_dialect;
  opts->x_ix86_branch_cost = ptr->x_ix86_branch_cost;
  opts->x_ix86_dump_tunes = ptr->x_ix86_dump_tunes;
  opts->x_ix86_force_align_arg_pointer = ptr->x_ix86_force_align_arg_pointer;
  opts->x_ix86_force_drap = ptr->x_ix86_force_drap;
  opts->x_ix86_incoming_stack_boundary_arg = ptr->x_ix86_incoming_stack_boundary_arg;
  opts->x_ix86_pmode = ptr->x_ix86_pmode;
  opts->x_ix86_preferred_stack_boundary_arg = ptr->x_ix86_preferred_stack_boundary_arg;
  opts->x_ix86_recip_name = ptr->x_ix86_recip_name;
  opts->x_ix86_regparm = ptr->x_ix86_regparm;
  opts->x_ix86_section_threshold = ptr->x_ix86_section_threshold;
  opts->x_ix86_sse2avx = ptr->x_ix86_sse2avx;
  opts->x_ix86_stack_protector_guard = ptr->x_ix86_stack_protector_guard;
  opts->x_ix86_stringop_alg = ptr->x_ix86_stringop_alg;
  opts->x_ix86_tls_dialect = ptr->x_ix86_tls_dialect;
  opts->x_ix86_tune_ctrl_string = ptr->x_ix86_tune_ctrl_string;
  opts->x_ix86_tune_memcpy_strategy = ptr->x_ix86_tune_memcpy_strategy;
  opts->x_ix86_tune_memset_strategy = ptr->x_ix86_tune_memset_strategy;
  opts->x_ix86_tune_no_default = ptr->x_ix86_tune_no_default;
  opts->x_ix86_veclibabi_type = ptr->x_ix86_veclibabi_type;
  ix86_tune_cost = processor_cost_table[ix86_tune];
  /* TODO: ix86_cost should be chosen at instruction or function granuality
     so for cold code we use size_cost even in !optimize_size compilation.  */
  if (opts->x_optimize_size)
    ix86_cost = &ix86_size_cost;
  else
    ix86_cost = ix86_tune_cost;

  /* Recreate the arch feature tests if the arch changed */
  if (old_arch != ix86_arch)
    {
      ix86_arch_mask = HOST_WIDE_INT_1U << ix86_arch;
      for (i = 0; i < X86_ARCH_LAST; ++i)
	ix86_arch_features[i]
	  = !!(initial_ix86_arch_features[i] & ix86_arch_mask);
    }

  /* Recreate the tune optimization tests */
  if (old_tune != ix86_tune)
    set_ix86_tune_features (ix86_tune, false);
}

/* Adjust target options after streaming them in.  This is mainly about
   reconciling them with global options.  */

void
ix86_function_specific_post_stream_in (struct cl_target_option *ptr)
{
  /* flag_pic is a global option, but ix86_cmodel is target saved option
     partly computed from flag_pic.  If flag_pic is on, adjust x_ix86_cmodel
     for PIC, or error out.  */
  if (flag_pic)
    switch (ptr->x_ix86_cmodel)
      {
      case CM_SMALL:
	ptr->x_ix86_cmodel = CM_SMALL_PIC;
	break;

      case CM_MEDIUM:
	ptr->x_ix86_cmodel = CM_MEDIUM_PIC;
	break;

      case CM_LARGE:
	ptr->x_ix86_cmodel = CM_LARGE_PIC;
	break;

      case CM_KERNEL:
	error ("code model %s does not support PIC mode", "kernel");
	break;

      default:
	break;
      }
  else
    switch (ptr->x_ix86_cmodel)
      {
      case CM_SMALL_PIC:
	ptr->x_ix86_cmodel = CM_SMALL;
	break;

      case CM_MEDIUM_PIC:
	ptr->x_ix86_cmodel = CM_MEDIUM;
	break;

      case CM_LARGE_PIC:
	ptr->x_ix86_cmodel = CM_LARGE;
	break;

      default:
	break;
      }
}

/* Print the current options */

void
ix86_function_specific_print (FILE *file, int indent,
			      struct cl_target_option *ptr)
{
  char *target_string
    = ix86_target_string (ptr->x_ix86_isa_flags, ptr->x_ix86_isa_flags2,
			  ptr->x_target_flags, ptr->x_ix86_target_flags,
			  NULL, NULL, ptr->x_ix86_fpmath, false);

  gcc_assert (ptr->arch < PROCESSOR_max);
  fprintf (file, "%*sarch = %d (%s)\n",
	   indent, "",
	   ptr->arch, processor_names[ptr->arch]);

  gcc_assert (ptr->tune < PROCESSOR_max);
  fprintf (file, "%*stune = %d (%s)\n",
	   indent, "",
	   ptr->tune, processor_names[ptr->tune]);

  fprintf (file, "%*sbranch_cost = %d\n", indent, "", ptr->branch_cost);

  if (target_string)
    {
      fprintf (file, "%*s%s\n", indent, "", target_string);
      free (target_string);
    }
}


/* Inner function to process the attribute((target(...))), take an argument and
   set the current options from the argument. If we have a list, recursively go
   over the list.  */

bool
ix86_valid_target_attribute_inner_p (tree args, char *p_strings[],
				     struct gcc_options *opts,
				     struct gcc_options *opts_set,
				     struct gcc_options *enum_opts_set)
{
  char *next_optstr;
  bool ret = true;

#define IX86_ATTR_ISA(S,O)   { S, sizeof (S)-1, ix86_opt_isa, O, 0 }
#define IX86_ATTR_STR(S,O)   { S, sizeof (S)-1, ix86_opt_str, O, 0 }
#define IX86_ATTR_ENUM(S,O)  { S, sizeof (S)-1, ix86_opt_enum, O, 0 }
#define IX86_ATTR_YES(S,O,M) { S, sizeof (S)-1, ix86_opt_yes, O, M }
#define IX86_ATTR_NO(S,O,M)  { S, sizeof (S)-1, ix86_opt_no,  O, M }

  enum ix86_opt_type
  {
    ix86_opt_unknown,
    ix86_opt_yes,
    ix86_opt_no,
    ix86_opt_str,
    ix86_opt_enum,
    ix86_opt_isa
  };

  static const struct
  {
    const char *string;
    size_t len;
    enum ix86_opt_type type;
    int opt;
    int mask;
  } attrs[] = {
    /* isa options */
    IX86_ATTR_ISA ("pconfig",	OPT_mpconfig),
    IX86_ATTR_ISA ("wbnoinvd",	OPT_mwbnoinvd),
    IX86_ATTR_ISA ("sgx",	OPT_msgx),
    IX86_ATTR_ISA ("avx5124fmaps", OPT_mavx5124fmaps),
    IX86_ATTR_ISA ("avx5124vnniw", OPT_mavx5124vnniw),
    IX86_ATTR_ISA ("avx512vpopcntdq", OPT_mavx512vpopcntdq),
    IX86_ATTR_ISA ("avx512vbmi2", OPT_mavx512vbmi2),
    IX86_ATTR_ISA ("avx512vnni", OPT_mavx512vnni),
    IX86_ATTR_ISA ("avx512bitalg", OPT_mavx512bitalg),

    IX86_ATTR_ISA ("avx512vbmi", OPT_mavx512vbmi),
    IX86_ATTR_ISA ("avx512ifma", OPT_mavx512ifma),
    IX86_ATTR_ISA ("avx512vl",	OPT_mavx512vl),
    IX86_ATTR_ISA ("avx512bw",	OPT_mavx512bw),
    IX86_ATTR_ISA ("avx512dq",	OPT_mavx512dq),
    IX86_ATTR_ISA ("avx512er",	OPT_mavx512er),
    IX86_ATTR_ISA ("avx512pf",	OPT_mavx512pf),
    IX86_ATTR_ISA ("avx512cd",	OPT_mavx512cd),
    IX86_ATTR_ISA ("avx512f",	OPT_mavx512f),
    IX86_ATTR_ISA ("avx2",	OPT_mavx2),
    IX86_ATTR_ISA ("fma",	OPT_mfma),
    IX86_ATTR_ISA ("xop",	OPT_mxop),
    IX86_ATTR_ISA ("fma4",	OPT_mfma4),
    IX86_ATTR_ISA ("f16c",	OPT_mf16c),
    IX86_ATTR_ISA ("avx",	OPT_mavx),
    IX86_ATTR_ISA ("sse4",	OPT_msse4),
    IX86_ATTR_ISA ("sse4.2",	OPT_msse4_2),
    IX86_ATTR_ISA ("sse4.1",	OPT_msse4_1),
    IX86_ATTR_ISA ("sse4a",	OPT_msse4a),
    IX86_ATTR_ISA ("ssse3",	OPT_mssse3),
    IX86_ATTR_ISA ("sse3",	OPT_msse3),
    IX86_ATTR_ISA ("aes",	OPT_maes),
    IX86_ATTR_ISA ("sha",	OPT_msha),
    IX86_ATTR_ISA ("pclmul",	OPT_mpclmul),
    IX86_ATTR_ISA ("sse2",	OPT_msse2),
    IX86_ATTR_ISA ("sse",	OPT_msse),
    IX86_ATTR_ISA ("3dnowa",	OPT_m3dnowa),
    IX86_ATTR_ISA ("3dnow",	OPT_m3dnow),
    IX86_ATTR_ISA ("mmx",	OPT_mmmx),
    IX86_ATTR_ISA ("rtm",	OPT_mrtm),
    IX86_ATTR_ISA ("prfchw",	OPT_mprfchw),
    IX86_ATTR_ISA ("rdseed",	OPT_mrdseed),
    IX86_ATTR_ISA ("adx",	OPT_madx),
    IX86_ATTR_ISA ("prefetchwt1", OPT_mprefetchwt1),
    IX86_ATTR_ISA ("clflushopt", OPT_mclflushopt),
    IX86_ATTR_ISA ("xsaves",	OPT_mxsaves),
    IX86_ATTR_ISA ("xsavec",	OPT_mxsavec),
    IX86_ATTR_ISA ("xsaveopt",	OPT_mxsaveopt),
    IX86_ATTR_ISA ("xsave",	OPT_mxsave),
    IX86_ATTR_ISA ("abm",	OPT_mabm),
    IX86_ATTR_ISA ("bmi",	OPT_mbmi),
    IX86_ATTR_ISA ("bmi2",	OPT_mbmi2),
    IX86_ATTR_ISA ("lzcnt",	OPT_mlzcnt),
    IX86_ATTR_ISA ("tbm",	OPT_mtbm),
    IX86_ATTR_ISA ("popcnt",	OPT_mpopcnt),
    IX86_ATTR_ISA ("cx16",	OPT_mcx16),
    IX86_ATTR_ISA ("sahf",	OPT_msahf),
    IX86_ATTR_ISA ("movbe",	OPT_mmovbe),
    IX86_ATTR_ISA ("crc32",	OPT_mcrc32),
    IX86_ATTR_ISA ("fsgsbase",	OPT_mfsgsbase),
    IX86_ATTR_ISA ("rdrnd",	OPT_mrdrnd),
    IX86_ATTR_ISA ("mwaitx",	OPT_mmwaitx),
    IX86_ATTR_ISA ("clzero",	OPT_mclzero),
    IX86_ATTR_ISA ("pku",	OPT_mpku),
    IX86_ATTR_ISA ("lwp",	OPT_mlwp),
    IX86_ATTR_ISA ("hle",	OPT_mhle),
    IX86_ATTR_ISA ("fxsr",	OPT_mfxsr),
    IX86_ATTR_ISA ("clwb",	OPT_mclwb),
    IX86_ATTR_ISA ("rdpid",	OPT_mrdpid),
    IX86_ATTR_ISA ("gfni",	OPT_mgfni),
    IX86_ATTR_ISA ("shstk",	OPT_mshstk),
    IX86_ATTR_ISA ("vaes",	OPT_mvaes),
    IX86_ATTR_ISA ("vpclmulqdq", OPT_mvpclmulqdq),
    IX86_ATTR_ISA ("movdiri", OPT_mmovdiri),
    IX86_ATTR_ISA ("movdir64b", OPT_mmovdir64b),
    IX86_ATTR_ISA ("waitpkg", OPT_mwaitpkg),
    IX86_ATTR_ISA ("cldemote", OPT_mcldemote),
    IX86_ATTR_ISA ("ptwrite",   OPT_mptwrite),

    /* enum options */
    IX86_ATTR_ENUM ("fpmath=",	OPT_mfpmath_),

    /* string options */
    IX86_ATTR_STR ("arch=",	IX86_FUNCTION_SPECIFIC_ARCH),
    IX86_ATTR_STR ("tune=",	IX86_FUNCTION_SPECIFIC_TUNE),

    /* flag options */
    IX86_ATTR_YES ("cld",
		   OPT_mcld,
		   MASK_CLD),

    IX86_ATTR_NO ("fancy-math-387",
		  OPT_mfancy_math_387,
		  MASK_NO_FANCY_MATH_387),

    IX86_ATTR_YES ("ieee-fp",
		   OPT_mieee_fp,
		   MASK_IEEE_FP),

    IX86_ATTR_YES ("inline-all-stringops",
		   OPT_minline_all_stringops,
		   MASK_INLINE_ALL_STRINGOPS),

    IX86_ATTR_YES ("inline-stringops-dynamically",
		   OPT_minline_stringops_dynamically,
		   MASK_INLINE_STRINGOPS_DYNAMICALLY),

    IX86_ATTR_NO ("align-stringops",
		  OPT_mno_align_stringops,
		  MASK_NO_ALIGN_STRINGOPS),

    IX86_ATTR_YES ("recip",
		   OPT_mrecip,
		   MASK_RECIP),

  };

  /* If this is a list, recurse to get the options.  */
  if (TREE_CODE (args) == TREE_LIST)
    {
      bool ret = true;

      for (; args; args = TREE_CHAIN (args))
	if (TREE_VALUE (args)
	    && !ix86_valid_target_attribute_inner_p (TREE_VALUE (args),
						     p_strings, opts, opts_set,
						     enum_opts_set))
	  ret = false;

      return ret;
    }

  else if (TREE_CODE (args) != STRING_CST)
    {
      error ("attribute %<target%> argument not a string");
      return false;
    }

  /* Handle multiple arguments separated by commas.  */
  next_optstr = ASTRDUP (TREE_STRING_POINTER (args));

  while (next_optstr && *next_optstr != '\0')
    {
      char *p = next_optstr;
      char *orig_p = p;
      char *comma = strchr (next_optstr, ',');
      const char *opt_string;
      size_t len, opt_len;
      int opt;
      bool opt_set_p;
      char ch;
      unsigned i;
      enum ix86_opt_type type = ix86_opt_unknown;
      int mask = 0;

      if (comma)
	{
	  *comma = '\0';
	  len = comma - next_optstr;
	  next_optstr = comma + 1;
	}
      else
	{
	  len = strlen (p);
	  next_optstr = NULL;
	}

      /* Recognize no-xxx.  */
      if (len > 3 && p[0] == 'n' && p[1] == 'o' && p[2] == '-')
	{
	  opt_set_p = false;
	  p += 3;
	  len -= 3;
	}
      else
	opt_set_p = true;

      /* Find the option.  */
      ch = *p;
      opt = N_OPTS;
      for (i = 0; i < ARRAY_SIZE (attrs); i++)
	{
	  type = attrs[i].type;
	  opt_len = attrs[i].len;
	  if (ch == attrs[i].string[0]
	      && ((type != ix86_opt_str && type != ix86_opt_enum)
		  ? len == opt_len
		  : len > opt_len)
	      && memcmp (p, attrs[i].string, opt_len) == 0)
	    {
	      opt = attrs[i].opt;
	      mask = attrs[i].mask;
	      opt_string = attrs[i].string;
	      break;
	    }
	}

      /* Process the option.  */
      if (opt == N_OPTS)
	{
	  error ("attribute(target(\"%s\")) is unknown", orig_p);
	  ret = false;
	}

      else if (type == ix86_opt_isa)
	{
	  struct cl_decoded_option decoded;

	  generate_option (opt, NULL, opt_set_p, CL_TARGET, &decoded);
	  ix86_handle_option (opts, opts_set,
			      &decoded, input_location);
	}

      else if (type == ix86_opt_yes || type == ix86_opt_no)
	{
	  if (type == ix86_opt_no)
	    opt_set_p = !opt_set_p;

	  if (opt_set_p)
	    opts->x_target_flags |= mask;
	  else
	    opts->x_target_flags &= ~mask;
	}

      else if (type == ix86_opt_str)
	{
	  if (p_strings[opt])
	    {
	      error ("option(\"%s\") was already specified", opt_string);
	      ret = false;
	    }
	  else
	    p_strings[opt] = xstrdup (p + opt_len);
	}

      else if (type == ix86_opt_enum)
	{
	  bool arg_ok;
	  int value;

	  arg_ok = opt_enum_arg_to_value (opt, p + opt_len, &value, CL_TARGET);
	  if (arg_ok)
	    set_option (opts, enum_opts_set, opt, value,
			p + opt_len, DK_UNSPECIFIED, input_location,
			global_dc);
	  else
	    {
	      error ("attribute(target(\"%s\")) is unknown", orig_p);
	      ret = false;
	    }
	}

      else
	gcc_unreachable ();
    }

  return ret;
}

/* Release allocated strings.  */
static void
release_options_strings (char **option_strings)
{
  /* Free up memory allocated to hold the strings */
  for (unsigned i = 0; i < IX86_FUNCTION_SPECIFIC_MAX; i++)
    free (option_strings[i]);
}

/* Return a TARGET_OPTION_NODE tree of the target options listed or NULL.  */

tree
ix86_valid_target_attribute_tree (tree args,
				  struct gcc_options *opts,
				  struct gcc_options *opts_set)
{
  const char *orig_arch_string = opts->x_ix86_arch_string;
  const char *orig_tune_string = opts->x_ix86_tune_string;
  enum fpmath_unit orig_fpmath_set = opts_set->x_ix86_fpmath;
  int orig_tune_defaulted = ix86_tune_defaulted;
  int orig_arch_specified = ix86_arch_specified;
  char *option_strings[IX86_FUNCTION_SPECIFIC_MAX] = { NULL, NULL };
  tree t = NULL_TREE;
  struct cl_target_option *def
    = TREE_TARGET_OPTION (target_option_default_node);
  struct gcc_options enum_opts_set;

  memset (&enum_opts_set, 0, sizeof (enum_opts_set));

  /* Process each of the options on the chain.  */
  if (! ix86_valid_target_attribute_inner_p (args, option_strings, opts,
					     opts_set, &enum_opts_set))
    return error_mark_node;

  /* If the changed options are different from the default, rerun
     ix86_option_override_internal, and then save the options away.
     The string options are attribute options, and will be undone
     when we copy the save structure.  */
  if (opts->x_ix86_isa_flags != def->x_ix86_isa_flags
      || opts->x_ix86_isa_flags2 != def->x_ix86_isa_flags2
      || opts->x_target_flags != def->x_target_flags
      || option_strings[IX86_FUNCTION_SPECIFIC_ARCH]
      || option_strings[IX86_FUNCTION_SPECIFIC_TUNE]
      || enum_opts_set.x_ix86_fpmath)
    {
      /* If we are using the default tune= or arch=, undo the string assigned,
	 and use the default.  */
      if (option_strings[IX86_FUNCTION_SPECIFIC_ARCH])
	{
	  opts->x_ix86_arch_string
	    = ggc_strdup (option_strings[IX86_FUNCTION_SPECIFIC_ARCH]);

	  /* If arch= is set,  clear all bits in x_ix86_isa_flags,
	     except for ISA_64BIT, ABI_64, ABI_X32, and CODE16.  */
	  opts->x_ix86_isa_flags &= (OPTION_MASK_ISA_64BIT
				     | OPTION_MASK_ABI_64
				     | OPTION_MASK_ABI_X32
				     | OPTION_MASK_CODE16);
	  opts->x_ix86_isa_flags2 = 0;
	}
      else if (!orig_arch_specified)
	opts->x_ix86_arch_string = NULL;

      if (option_strings[IX86_FUNCTION_SPECIFIC_TUNE])
	opts->x_ix86_tune_string
	  = ggc_strdup (option_strings[IX86_FUNCTION_SPECIFIC_TUNE]);
      else if (orig_tune_defaulted)
	opts->x_ix86_tune_string = NULL;

      /* If fpmath= is not set, and we now have sse2 on 32-bit, use it.  */
      if (enum_opts_set.x_ix86_fpmath)
	opts_set->x_ix86_fpmath = (enum fpmath_unit) 1;

      /* Do any overrides, such as arch=xxx, or tune=xxx support.  */
      bool r = ix86_option_override_internal (false, opts, opts_set);
      if (!r)
	{
	  release_options_strings (option_strings);
	  return error_mark_node;
	}

      /* Add any builtin functions with the new isa if any.  */
      ix86_add_new_builtins (opts->x_ix86_isa_flags, opts->x_ix86_isa_flags2);

      /* Save the current options unless we are validating options for
	 #pragma.  */
      t = build_target_option_node (opts);

      opts->x_ix86_arch_string = orig_arch_string;
      opts->x_ix86_tune_string = orig_tune_string;
      opts_set->x_ix86_fpmath = orig_fpmath_set;

      release_options_strings (option_strings);
    }

  return t;
}

/* Hook to validate attribute((target("string"))).  */

bool
ix86_valid_target_attribute_p (tree fndecl,
			       tree ARG_UNUSED (name),
			       tree args,
			       int ARG_UNUSED (flags))
{
  struct gcc_options func_options;
  tree new_target, new_optimize;
  bool ret = true;

  /* attribute((target("default"))) does nothing, beyond
     affecting multi-versioning.  */
  if (TREE_VALUE (args)
      && TREE_CODE (TREE_VALUE (args)) == STRING_CST
      && TREE_CHAIN (args) == NULL_TREE
      && strcmp (TREE_STRING_POINTER (TREE_VALUE (args)), "default") == 0)
    return true;

  tree old_optimize = build_optimization_node (&global_options);

  /* Get the optimization options of the current function.  */  
  tree func_optimize = DECL_FUNCTION_SPECIFIC_OPTIMIZATION (fndecl);
 
  if (!func_optimize)
    func_optimize = old_optimize;

  /* Init func_options.  */
  memset (&func_options, 0, sizeof (func_options));
  init_options_struct (&func_options, NULL);
  lang_hooks.init_options_struct (&func_options);
 
  cl_optimization_restore (&func_options,
			   TREE_OPTIMIZATION (func_optimize));

  /* Initialize func_options to the default before its target options can
     be set.  */
  cl_target_option_restore (&func_options,
			    TREE_TARGET_OPTION (target_option_default_node));

  new_target = ix86_valid_target_attribute_tree (args, &func_options,
						 &global_options_set);

  new_optimize = build_optimization_node (&func_options);

  if (new_target == error_mark_node)
    ret = false;

  else if (fndecl && new_target)
    {
      DECL_FUNCTION_SPECIFIC_TARGET (fndecl) = new_target;

      if (old_optimize != new_optimize)
	DECL_FUNCTION_SPECIFIC_OPTIMIZATION (fndecl) = new_optimize;
    }

  finalize_options_struct (&func_options);

  return ret;
}

/* Handle "cdecl", "stdcall", "fastcall", "regparm", "thiscall",
   and "sseregparm" calling convention attributes;
   arguments as in struct attribute_spec.handler.  */

static tree
ix86_handle_cconv_attribute (tree *node, tree name, tree args, int,
			     bool *no_add_attrs)
{
  if (TREE_CODE (*node) != FUNCTION_TYPE
      && TREE_CODE (*node) != METHOD_TYPE
      && TREE_CODE (*node) != FIELD_DECL
      && TREE_CODE (*node) != TYPE_DECL)
    {
      warning (OPT_Wattributes, "%qE attribute only applies to functions",
	       name);
      *no_add_attrs = true;
      return NULL_TREE;
    }

  /* Can combine regparm with all attributes but fastcall, and thiscall.  */
  if (is_attribute_p ("regparm", name))
    {
      tree cst;

      if (lookup_attribute ("fastcall", TYPE_ATTRIBUTES (*node)))
        {
	  error ("fastcall and regparm attributes are not compatible");
	}

      if (lookup_attribute ("thiscall", TYPE_ATTRIBUTES (*node)))
	{
	  error ("regparam and thiscall attributes are not compatible");
	}

      cst = TREE_VALUE (args);
      if (TREE_CODE (cst) != INTEGER_CST)
	{
	  warning (OPT_Wattributes,
		   "%qE attribute requires an integer constant argument",
		   name);
	  *no_add_attrs = true;
	}
      else if (compare_tree_int (cst, REGPARM_MAX) > 0)
	{
	  warning (OPT_Wattributes, "argument to %qE attribute larger than %d",
		   name, REGPARM_MAX);
	  *no_add_attrs = true;
	}

      return NULL_TREE;
    }

  if (TARGET_64BIT)
    {
      /* Do not warn when emulating the MS ABI.  */
      if ((TREE_CODE (*node) != FUNCTION_TYPE
	   && TREE_CODE (*node) != METHOD_TYPE)
	  || ix86_function_type_abi (*node) != MS_ABI)
	warning (OPT_Wattributes, "%qE attribute ignored",
	         name);
      *no_add_attrs = true;
      return NULL_TREE;
    }

  /* Can combine fastcall with stdcall (redundant) and sseregparm.  */
  if (is_attribute_p ("fastcall", name))
    {
      if (lookup_attribute ("cdecl", TYPE_ATTRIBUTES (*node)))
        {
	  error ("fastcall and cdecl attributes are not compatible");
	}
      if (lookup_attribute ("stdcall", TYPE_ATTRIBUTES (*node)))
        {
	  error ("fastcall and stdcall attributes are not compatible");
	}
      if (lookup_attribute ("regparm", TYPE_ATTRIBUTES (*node)))
        {
	  error ("fastcall and regparm attributes are not compatible");
	}
      if (lookup_attribute ("thiscall", TYPE_ATTRIBUTES (*node)))
	{
	  error ("fastcall and thiscall attributes are not compatible");
	}
    }

  /* Can combine stdcall with fastcall (redundant), regparm and
     sseregparm.  */
  else if (is_attribute_p ("stdcall", name))
    {
      if (lookup_attribute ("cdecl", TYPE_ATTRIBUTES (*node)))
        {
	  error ("stdcall and cdecl attributes are not compatible");
	}
      if (lookup_attribute ("fastcall", TYPE_ATTRIBUTES (*node)))
        {
	  error ("stdcall and fastcall attributes are not compatible");
	}
      if (lookup_attribute ("thiscall", TYPE_ATTRIBUTES (*node)))
	{
	  error ("stdcall and thiscall attributes are not compatible");
	}
    }

  /* Can combine cdecl with regparm and sseregparm.  */
  else if (is_attribute_p ("cdecl", name))
    {
      if (lookup_attribute ("stdcall", TYPE_ATTRIBUTES (*node)))
        {
	  error ("stdcall and cdecl attributes are not compatible");
	}
      if (lookup_attribute ("fastcall", TYPE_ATTRIBUTES (*node)))
        {
	  error ("fastcall and cdecl attributes are not compatible");
	}
      if (lookup_attribute ("thiscall", TYPE_ATTRIBUTES (*node)))
	{
	  error ("cdecl and thiscall attributes are not compatible");
	}
    }
  else if (is_attribute_p ("thiscall", name))
    {
      if (TREE_CODE (*node) != METHOD_TYPE && pedantic)
	warning (OPT_Wattributes, "%qE attribute is used for non-class method",
	         name);
      if (lookup_attribute ("stdcall", TYPE_ATTRIBUTES (*node)))
	{
	  error ("stdcall and thiscall attributes are not compatible");
	}
      if (lookup_attribute ("fastcall", TYPE_ATTRIBUTES (*node)))
	{
	  error ("fastcall and thiscall attributes are not compatible");
	}
      if (lookup_attribute ("cdecl", TYPE_ATTRIBUTES (*node)))
	{
	  error ("cdecl and thiscall attributes are not compatible");
	}
    }

  /* Can combine sseregparm with all attributes.  */

  return NULL_TREE;
}

#ifndef CHECK_STACK_LIMIT
#define CHECK_STACK_LIMIT (-1)
#endif

/* The transactional memory builtins are implicitly regparm or fastcall
   depending on the ABI.  Override the generic do-nothing attribute that
   these builtins were declared with, and replace it with one of the two
   attributes that we expect elsewhere.  */

static tree
ix86_handle_tm_regparm_attribute (tree *node, tree, tree,
				  int flags, bool *no_add_attrs)
{
  tree alt;

  /* In no case do we want to add the placeholder attribute.  */
  *no_add_attrs = true;

  /* The 64-bit ABI is unchanged for transactional memory.  */
  if (TARGET_64BIT)
    return NULL_TREE;

  /* ??? Is there a better way to validate 32-bit windows?  We have
     cfun->machine->call_abi, but that seems to be set only for 64-bit.  */
  if (CHECK_STACK_LIMIT > 0)
    alt = tree_cons (get_identifier ("fastcall"), NULL, NULL);
  else
    {
      alt = tree_cons (NULL, build_int_cst (NULL, 2), NULL);
      alt = tree_cons (get_identifier ("regparm"), alt, NULL);
    }
  decl_attributes (node, alt, flags);

  return NULL_TREE;
}

/* Handle a "force_align_arg_pointer" attribute.  */

static tree
ix86_handle_force_align_arg_pointer_attribute (tree *node, tree name,
					       tree, int, bool *no_add_attrs)
{
  if (TREE_CODE (*node) != FUNCTION_TYPE
      && TREE_CODE (*node) != METHOD_TYPE
      && TREE_CODE (*node) != FIELD_DECL
      && TREE_CODE (*node) != TYPE_DECL)
    {
      warning (OPT_Wattributes, "%qE attribute only applies to functions",
	       name);
      *no_add_attrs = true;
    }

  return NULL_TREE;
}

/* Handle a "ms_struct" or "gcc_struct" attribute; arguments as in
   struct attribute_spec.handler.  */

static tree
ix86_handle_struct_attribute (tree *node, tree name, tree, int,
			      bool *no_add_attrs)
{
  tree *type = NULL;
  if (DECL_P (*node))
    {
      if (TREE_CODE (*node) == TYPE_DECL)
	type = &TREE_TYPE (*node);
    }
  else
    type = node;

  if (!(type && RECORD_OR_UNION_TYPE_P (*type)))
    {
      warning (OPT_Wattributes, "%qE attribute ignored",
	       name);
      *no_add_attrs = true;
    }

  else if ((is_attribute_p ("ms_struct", name)
	    && lookup_attribute ("gcc_struct", TYPE_ATTRIBUTES (*type)))
	   || ((is_attribute_p ("gcc_struct", name)
		&& lookup_attribute ("ms_struct", TYPE_ATTRIBUTES (*type)))))
    {
      warning (OPT_Wattributes, "%qE incompatible attribute ignored",
               name);
      *no_add_attrs = true;
    }

  return NULL_TREE;
}

/* Handle a "callee_pop_aggregate_return" attribute; arguments as
   in struct attribute_spec handler.  */

static tree
ix86_handle_callee_pop_aggregate_return (tree *node, tree name, tree args, int,
					 bool *no_add_attrs)
{
  if (TREE_CODE (*node) != FUNCTION_TYPE
      && TREE_CODE (*node) != METHOD_TYPE
      && TREE_CODE (*node) != FIELD_DECL
      && TREE_CODE (*node) != TYPE_DECL)
    {
      warning (OPT_Wattributes, "%qE attribute only applies to functions",
	       name);
      *no_add_attrs = true;
      return NULL_TREE;
    }
  if (TARGET_64BIT)
    {
      warning (OPT_Wattributes, "%qE attribute only available for 32-bit",
	       name);
      *no_add_attrs = true;
      return NULL_TREE;
    }
  if (is_attribute_p ("callee_pop_aggregate_return", name))
    {
      tree cst;

      cst = TREE_VALUE (args);
      if (TREE_CODE (cst) != INTEGER_CST)
	{
	  warning (OPT_Wattributes,
		   "%qE attribute requires an integer constant argument",
		   name);
	  *no_add_attrs = true;
	}
      else if (compare_tree_int (cst, 0) != 0
	       && compare_tree_int (cst, 1) != 0)
	{
	  warning (OPT_Wattributes,
		   "argument to %qE attribute is neither zero, nor one",
		   name);
	  *no_add_attrs = true;
	}

      return NULL_TREE;
    }

  return NULL_TREE;
}

/* Handle a "ms_abi" or "sysv" attribute; arguments as in
   struct attribute_spec.handler.  */

static tree
ix86_handle_abi_attribute (tree *node, tree name, tree, int,
			   bool *no_add_attrs)
{
  if (TREE_CODE (*node) != FUNCTION_TYPE
      && TREE_CODE (*node) != METHOD_TYPE
      && TREE_CODE (*node) != FIELD_DECL
      && TREE_CODE (*node) != TYPE_DECL)
    {
      warning (OPT_Wattributes, "%qE attribute only applies to functions",
	       name);
      *no_add_attrs = true;
      return NULL_TREE;
    }

  /* Can combine regparm with all attributes but fastcall.  */
  if (is_attribute_p ("ms_abi", name))
    {
      if (lookup_attribute ("sysv_abi", TYPE_ATTRIBUTES (*node)))
        {
	  error ("ms_abi and sysv_abi attributes are not compatible");
	}

      return NULL_TREE;
    }
  else if (is_attribute_p ("sysv_abi", name))
    {
      if (lookup_attribute ("ms_abi", TYPE_ATTRIBUTES (*node)))
        {
	  error ("ms_abi and sysv_abi attributes are not compatible");
	}

      return NULL_TREE;
    }

  return NULL_TREE;
}

static tree
ix86_handle_fndecl_attribute (tree *node, tree name, tree args, int,
			      bool *no_add_attrs)
{
  if (TREE_CODE (*node) != FUNCTION_DECL)
    {
      warning (OPT_Wattributes, "%qE attribute only applies to functions",
               name);
      *no_add_attrs = true;
    }

  if (is_attribute_p ("indirect_branch", name))
    {
      tree cst = TREE_VALUE (args);
      if (TREE_CODE (cst) != STRING_CST)
	{
	  warning (OPT_Wattributes,
		   "%qE attribute requires a string constant argument",
		   name);
	  *no_add_attrs = true;
	}
      else if (strcmp (TREE_STRING_POINTER (cst), "keep") != 0
	       && strcmp (TREE_STRING_POINTER (cst), "thunk") != 0
	       && strcmp (TREE_STRING_POINTER (cst), "thunk-inline") != 0
	       && strcmp (TREE_STRING_POINTER (cst), "thunk-extern") != 0)
	{
	  warning (OPT_Wattributes,
		   "argument to %qE attribute is not "
		   "(keep|thunk|thunk-inline|thunk-extern)", name);
	  *no_add_attrs = true;
	}
    }

  if (is_attribute_p ("function_return", name))
    {
      tree cst = TREE_VALUE (args);
      if (TREE_CODE (cst) != STRING_CST)
	{
	  warning (OPT_Wattributes,
		   "%qE attribute requires a string constant argument",
		   name);
	  *no_add_attrs = true;
	}
      else if (strcmp (TREE_STRING_POINTER (cst), "keep") != 0
	       && strcmp (TREE_STRING_POINTER (cst), "thunk") != 0
	       && strcmp (TREE_STRING_POINTER (cst), "thunk-inline") != 0
	       && strcmp (TREE_STRING_POINTER (cst), "thunk-extern") != 0)
	{
	  warning (OPT_Wattributes,
		   "argument to %qE attribute is not "
		   "(keep|thunk|thunk-inline|thunk-extern)", name);
	  *no_add_attrs = true;
	}
    }

  return NULL_TREE;
}

static tree
ix86_handle_no_caller_saved_registers_attribute (tree *, tree, tree,
						 int, bool *)
{
  return NULL_TREE;
}

static tree
ix86_handle_interrupt_attribute (tree *node, tree, tree, int, bool *)
{
  /* DECL_RESULT and DECL_ARGUMENTS do not exist there yet,
     but the function type contains args and return type data.  */
  tree func_type = *node;
  tree return_type = TREE_TYPE (func_type);

  int nargs = 0;
  tree current_arg_type = TYPE_ARG_TYPES (func_type);
  while (current_arg_type
	 && ! VOID_TYPE_P (TREE_VALUE (current_arg_type)))
    {
      if (nargs == 0)
	{
	  if (! POINTER_TYPE_P (TREE_VALUE (current_arg_type)))
	    error ("interrupt service routine should have a pointer "
		   "as the first argument");
	}
      else if (nargs == 1)
	{
	  if (TREE_CODE (TREE_VALUE (current_arg_type)) != INTEGER_TYPE
	      || TYPE_MODE (TREE_VALUE (current_arg_type)) != word_mode)
	    error ("interrupt service routine should have %qs "
		   "as the second argument",
		   TARGET_64BIT
		   ? (TARGET_X32 ? "unsigned long long int"
				 : "unsigned long int")
		   : "unsigned int");
	}
      nargs++;
      current_arg_type = TREE_CHAIN (current_arg_type);
    }
  if (!nargs || nargs > 2)
    error ("interrupt service routine can only have a pointer argument "
	   "and an optional integer argument");
  if (! VOID_TYPE_P (return_type))
    error ("interrupt service routine can%'t have non-void return value");

  return NULL_TREE;
}

/* Handle fentry_name / fentry_section attribute.  */

static tree
ix86_handle_fentry_name (tree *node, tree name, tree args,
			 int, bool *no_add_attrs)
{
  if (TREE_CODE (*node) == FUNCTION_DECL
      && TREE_CODE (TREE_VALUE (args)) == STRING_CST)
    /* Do nothing else, just set the attribute.  We'll get at
       it later with lookup_attribute.  */
    ;
  else
    {
      warning (OPT_Wattributes, "%qE attribute ignored", name);
      *no_add_attrs = true;
    }

  return NULL_TREE;
}

/* Table of valid machine attributes.  */
const struct attribute_spec ix86_attribute_table[] =
{
  /* { name, min_len, max_len, decl_req, type_req, fn_type_req,
       affects_type_identity, handler, exclude } */
  /* Stdcall attribute says callee is responsible for popping arguments
     if they are not variable.  */
  { "stdcall",   0, 0, false, true,  true,  true, ix86_handle_cconv_attribute,
    NULL },
  /* Fastcall attribute says callee is responsible for popping arguments
     if they are not variable.  */
  { "fastcall",  0, 0, false, true,  true,  true, ix86_handle_cconv_attribute,
    NULL },
  /* Thiscall attribute says callee is responsible for popping arguments
     if they are not variable.  */
  { "thiscall",  0, 0, false, true,  true,  true, ix86_handle_cconv_attribute,
    NULL },
  /* Cdecl attribute says the callee is a normal C declaration */
  { "cdecl",     0, 0, false, true,  true,  true, ix86_handle_cconv_attribute,
    NULL },
  /* Regparm attribute specifies how many integer arguments are to be
     passed in registers.  */
  { "regparm",   1, 1, false, true,  true,  true, ix86_handle_cconv_attribute,
    NULL },
  /* Sseregparm attribute says we are using x86_64 calling conventions
     for FP arguments.  */
  { "sseregparm", 0, 0, false, true, true,  true, ix86_handle_cconv_attribute,
    NULL },
  /* The transactional memory builtins are implicitly regparm or fastcall
     depending on the ABI.  Override the generic do-nothing attribute that
     these builtins were declared with.  */
  { "*tm regparm", 0, 0, false, true, true, true,
    ix86_handle_tm_regparm_attribute, NULL },
  /* force_align_arg_pointer says this function realigns the stack at entry.  */
  { "force_align_arg_pointer", 0, 0,
    false, true,  true, false, ix86_handle_force_align_arg_pointer_attribute,
    NULL },
#if TARGET_DLLIMPORT_DECL_ATTRIBUTES
  { "dllimport", 0, 0, false, false, false, false, handle_dll_attribute,
    NULL },
  { "dllexport", 0, 0, false, false, false, false, handle_dll_attribute,
    NULL },
  { "shared",    0, 0, true,  false, false, false,
    ix86_handle_shared_attribute, NULL },
#endif
  { "ms_struct", 0, 0, false, false,  false, false,
    ix86_handle_struct_attribute, NULL },
  { "gcc_struct", 0, 0, false, false,  false, false,
    ix86_handle_struct_attribute, NULL },
#ifdef SUBTARGET_ATTRIBUTE_TABLE
  SUBTARGET_ATTRIBUTE_TABLE,
#endif
  /* ms_abi and sysv_abi calling convention function attributes.  */
  { "ms_abi", 0, 0, false, true, true, true, ix86_handle_abi_attribute, NULL },
  { "sysv_abi", 0, 0, false, true, true, true, ix86_handle_abi_attribute,
    NULL },
  { "ms_abi va_list", 0, 0, false, false, false, false, NULL, NULL },
  { "sysv_abi va_list", 0, 0, false, false, false, false, NULL, NULL },
  { "ms_hook_prologue", 0, 0, true, false, false, false,
    ix86_handle_fndecl_attribute, NULL },
  { "callee_pop_aggregate_return", 1, 1, false, true, true, true,
    ix86_handle_callee_pop_aggregate_return, NULL },
  { "interrupt", 0, 0, false, true, true, false,
    ix86_handle_interrupt_attribute, NULL },
  { "no_caller_saved_registers", 0, 0, false, true, true, false,
    ix86_handle_no_caller_saved_registers_attribute, NULL },
  { "naked", 0, 0, true, false, false, false,
    ix86_handle_fndecl_attribute, NULL },
  { "indirect_branch", 1, 1, true, false, false, false,
    ix86_handle_fndecl_attribute, NULL },
  { "function_return", 1, 1, true, false, false, false,
    ix86_handle_fndecl_attribute, NULL },
  { "indirect_return", 0, 0, false, true, true, false,
    NULL, NULL },
  { "fentry_name", 1, 1, true, false, false, false,
    ix86_handle_fentry_name, NULL },
  { "fentry_section", 1, 1, true, false, false, false,
    ix86_handle_fentry_name, NULL },
  { "cf_check", 0, 0, true, false, false, false,
    ix86_handle_fndecl_attribute, NULL },

  /* End element.  */
  { NULL, 0, 0, false, false, false, false, NULL, NULL }
};
