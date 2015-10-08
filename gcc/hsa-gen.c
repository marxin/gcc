/* A pass for lowering gimple to HSAIL
   Copyright (C) 2013 Free Software Foundation, Inc.

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

/* TODO: Some of the following includes might be redundant because of ongoing
   header cleanups.  */

#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "tm.h"
#include "is-a.h"
#include "hash-set.h"
#include "defaults.h"
#include "hard-reg-set.h"
#include "symtab.h"
#include "vec.h"
#include "input.h"
#include "alias.h"
#include "double-int.h"
#include "inchash.h"
#include "tree.h"
#include "tree-pass.h"
#include "tree-ssa-alias.h"
#include "internal-fn.h"
#include "gimple-expr.h"
#include "dominance.h"
#include "cfg.h"
#include "function.h"
#include "predict.h"
#include "basic-block.h"
#include "fold-const.h"
#include "gimple.h"
#include "gimple-iterator.h"
#include "machmode.h"
#include "output.h"
#include "function.h"
#include "bitmap.h"
#include "dumpfile.h"
#include "gimple-pretty-print.h"
#include "diagnostic-core.h"
#include "alloc-pool.h"
#include "tree-ssa-operands.h"
#include "gimple-ssa.h"
#include "tree-phinodes.h"
#include "stringpool.h"
#include "tree-ssanames.h"
#include "rtl.h"
#include "expr.h"
#include "tree-dfa.h"
#include "ssa-iterators.h"
#include "ipa-ref.h"
#include "lto-streamer.h"
#include "cgraph.h"
#include "stor-layout.h"
#include "gimplify-me.h"
#include "print-tree.h"
#include "symbol-summary.h"
#include "hsa.h"
#include "cfghooks.h"
#include "tree-cfg.h"
#include "cfgloop.h"
#include "cfganal.h"

/* Print a warning message and set that we have seen an error.  */

#define HSA_SORRY_MSG "could not emit HSAIL for the function"

#define HSA_SORRY_ATV(location, message, ...) \
  do \
  { \
    hsa_fail_cfun (); \
    if (warning_at (EXPR_LOCATION (hsa_cfun->decl), OPT_Whsa, \
		    HSA_SORRY_MSG)) \
      inform (location, message, ##__VA_ARGS__); \
  } \
  while (false);

/* Same as previous, but highlight a location.  */

#define HSA_SORRY_AT(location, message) \
  do \
  { \
    hsa_fail_cfun (); \
    if (warning_at (EXPR_LOCATION (hsa_cfun->decl), OPT_Whsa, \
		    HSA_SORRY_MSG)) \
      inform (location, message); \
  } \
  while (false);

/* Following structures are defined in the final version
   of HSA specification.  */

/* HSA kernel dispatch is collection of informations needed for
   a kernel dispatch.  */

struct hsa_kernel_dispatch
{
  /* Pointer to a command queue associated with a kernel dispatch agent.  */
  void *queue;
  /* Pointer to reserved memory for OMP data struct copying.  */
  void *omp_data_memory;
  /* Pointer to a memory space used for kernel arguments passing.  */
  void *kernarg_address;
  /* Kernel object.  */
  uint64_t object;
  /* Synchronization signal used for dispatch synchronization.  */
  uint64_t signal;
  /* Private segment size.  */
  uint32_t private_segment_size;
  /* Group segment size.  */
  uint32_t group_segment_size;
  /* Number of children kernel dispatches.  */
  uint64_t kernel_dispatch_count;
  /* Number of threads.  */
  uint32_t omp_num_threads;
  /* Debug purpose argument.  */
  uint64_t debug;
  /* Kernel dispatch structures created for children kernel dispatches.  */
  struct hsa_kernel_dispatch **children_dispatches;
};

/* HSA queue packet is shadow structure, originally provided by AMD.  */

struct hsa_queue_packet
{
  uint16_t header;
  uint16_t setup;
  uint16_t workgroup_size_x;
  uint16_t workgroup_size_y;
  uint16_t workgroup_size_z;
  uint16_t reserved0;
  uint32_t grid_size_x;
  uint32_t grid_size_y;
  uint32_t grid_size_z;
  uint32_t private_segment_size;
  uint32_t group_segment_size;
  uint64_t kernel_object;
  void *kernarg_address;
  uint64_t reserved2;
  uint64_t completion_signal;
};

/* HSA queue is shadow structure, originally provided by AMD.  */

struct hsa_queue
{
  int type;
  uint32_t features;
  void *base_address;
  uint64_t doorbell_signal;
  uint32_t size;
  uint32_t reserved1;
  uint64_t id;
};

/* Alloc pools for allocating basic hsa structures such as operands,
   instructions and other basic entities.s */
static object_allocator<hsa_op_address> *hsa_allocp_operand_address;
static object_allocator<hsa_op_immed> *hsa_allocp_operand_immed;
static object_allocator<hsa_op_reg> *hsa_allocp_operand_reg;
static object_allocator<hsa_op_code_list> *hsa_allocp_operand_code_list;
static object_allocator<hsa_insn_basic> *hsa_allocp_inst_basic;
static object_allocator<hsa_insn_phi> *hsa_allocp_inst_phi;
static object_allocator<hsa_insn_mem> *hsa_allocp_inst_mem;
static object_allocator<hsa_insn_atomic> *hsa_allocp_inst_atomic;
static object_allocator<hsa_insn_signal> *hsa_allocp_inst_signal;
static object_allocator<hsa_insn_seg> *hsa_allocp_inst_seg;
static object_allocator<hsa_insn_cmp> *hsa_allocp_inst_cmp;
static object_allocator<hsa_insn_br> *hsa_allocp_inst_br;
static object_allocator<hsa_insn_sbr> *hsa_allocp_inst_sbr;
static object_allocator<hsa_insn_call> *hsa_allocp_inst_call;
static object_allocator<hsa_insn_arg_block> *hsa_allocp_inst_arg_block;
static object_allocator<hsa_insn_comment> *hsa_allocp_inst_comment;
static object_allocator<hsa_insn_queue> *hsa_allocp_inst_queue;
static object_allocator<hsa_bb> *hsa_allocp_bb;
static object_allocator<hsa_symbol> *hsa_allocp_symbols;

/* Vectors with selected instructions and operands that need
   a destruction.  */
static vec <hsa_op_code_list *> hsa_list_operand_code_list;
static vec <hsa_op_reg *> hsa_list_operand_reg;
static vec <hsa_op_immed*> hsa_list_operand_immed;

/* Constructor of class representing global HSA function/kernel information and
   state.  */

/* TODO: Move more initialization here. */

hsa_function_representation::hsa_function_representation ()
{
  name = NULL;
  input_args_count = 0;
  reg_count = 0;
  input_args = NULL;
  output_arg = NULL;

  int sym_init_len = (vec_safe_length (cfun->local_decls) / 2) + 1;;
  local_symbols = new hash_table <hsa_noop_symbol_hasher> (sym_init_len);
  spill_symbols = vNULL;
  readonly_variables = vNULL;
  hbb_count = 0;
  in_ssa = true;	/* We start in SSA.  */
  kern_p = false;
  declaration_p = false;
  called_functions = vNULL;
  shadow_reg = NULL;
  kernel_dispatch_count = 0;
  maximum_omp_data_size = 0;
  seen_error = false;
}

/* Destructor of class holding function/kernel-wide informaton and state.  */

hsa_function_representation::~hsa_function_representation ()
{
  delete local_symbols;
  free (input_args);
  free (output_arg);
  /* Kernel names are deallocated at the end of BRIG output when deallocating
     hsa_decl_kernel_mapping.  */
  if (!kern_p)
    free (name);
  spill_symbols.release ();
  readonly_variables.release ();
  called_functions.release ();
}

hsa_op_reg *
hsa_function_representation::get_shadow_reg ()
{
  gcc_assert (kern_p);

  if (shadow_reg)
    return shadow_reg;

  /* Append the shadow argument.  */
  hsa_symbol *shadow = &input_args[input_args_count++];
  shadow->type = BRIG_TYPE_U64;
  shadow->segment = BRIG_SEGMENT_KERNARG;
  shadow->linkage = BRIG_LINKAGE_FUNCTION;
  shadow->name = "hsa_runtime_shadow";

  hsa_op_reg *r = new hsa_op_reg (BRIG_TYPE_U64);
  hsa_op_address *addr = new hsa_op_address (shadow);

  hsa_insn_mem *mem = new hsa_insn_mem (BRIG_OPCODE_LD, BRIG_TYPE_U64, r, addr);
  hsa_bb_for_bb (ENTRY_BLOCK_PTR_FOR_FN (cfun))->append_insn (mem);
  shadow_reg = r;

  return r;
}

bool hsa_function_representation::has_shadow_reg_p ()
{
  return shadow_reg != NULL;
}

/* Allocate HSA structures that we need only while generating with this.  */

static void
hsa_init_data_for_cfun ()
{
  hsa_init_compilation_unit_data ();
  hsa_allocp_operand_address
    = new object_allocator<hsa_op_address> ("HSA address operands");
  hsa_allocp_operand_immed
    = new object_allocator<hsa_op_immed> ("HSA immediate operands");
  hsa_allocp_operand_reg
    = new object_allocator<hsa_op_reg> ("HSA register operands");
  hsa_allocp_operand_code_list
    = new object_allocator<hsa_op_code_list> ("HSA code list operands");
  hsa_allocp_inst_basic
    = new object_allocator<hsa_insn_basic> ("HSA basic instructions");
  hsa_allocp_inst_phi
    = new object_allocator<hsa_insn_phi> ("HSA phi operands");
  hsa_allocp_inst_mem
    = new object_allocator<hsa_insn_mem> ("HSA memory instructions");
  hsa_allocp_inst_atomic
    = new object_allocator<hsa_insn_atomic> ("HSA atomic instructions");
  hsa_allocp_inst_signal
    = new object_allocator<hsa_insn_signal> ("HSA signal instructions");
  hsa_allocp_inst_seg
    = new object_allocator<hsa_insn_seg> ("HSA segment conversion instructions");
  hsa_allocp_inst_cmp
    = new object_allocator<hsa_insn_cmp> ("HSA comparison instructions");
  hsa_allocp_inst_br
    = new object_allocator<hsa_insn_br> ("HSA branching instructions");
  hsa_allocp_inst_sbr
    = new object_allocator<hsa_insn_sbr> ("HSA switch branching instructions");
  hsa_allocp_inst_call
    = new object_allocator<hsa_insn_call> ("HSA call instructions");
  hsa_allocp_inst_arg_block
    = new object_allocator<hsa_insn_arg_block> ("HSA arg block instructions");
  hsa_allocp_inst_comment
    = new object_allocator<hsa_insn_comment> ("HSA comment instructions");
  hsa_allocp_inst_queue
    = new object_allocator<hsa_insn_queue> ("HSA queue instructions");
  hsa_allocp_bb = new object_allocator<hsa_bb> ("HSA basic blocks");
  hsa_allocp_symbols = new object_allocator<hsa_symbol> ("HSA symbols");
  hsa_cfun = new hsa_function_representation ();

  /* The entry/exit blocks don't contain incoming code,
     but the HSA generator might use them to put code into,
     so we need hsa_bb instances of them.  */
  hsa_init_new_bb (ENTRY_BLOCK_PTR_FOR_FN (cfun));
  hsa_init_new_bb (EXIT_BLOCK_PTR_FOR_FN (cfun));
}

/* Deinitialize HSA subsystem and free all allocated memory.  */

static void
hsa_deinit_data_for_cfun (void)
{
  basic_block bb;

  FOR_ALL_BB_FN (bb, cfun)
    if (bb->aux)
      {
	hsa_bb *hbb = hsa_bb_for_bb (bb);
	hsa_insn_phi *phi;
	for (phi = hbb->first_phi;
	     phi;
	     phi = phi->next ? as_a <hsa_insn_phi *> (phi->next): NULL)
	  phi->~hsa_insn_phi ();
	for (hsa_insn_basic *insn = hbb->first_insn; insn; insn = insn->next)
	  hsa_destroy_insn (insn);

	hbb->~hsa_bb ();
	bb->aux = NULL;
      }

  for (unsigned int i = 0; i < hsa_list_operand_code_list.length (); i++)
    hsa_list_operand_code_list[i]->~hsa_op_code_list ();

  for (unsigned int i = 0; i < hsa_list_operand_reg.length (); i++)
    hsa_list_operand_reg[i]->~hsa_op_reg ();

  for (unsigned int i = 0; i < hsa_list_operand_immed.length (); i++)
    hsa_list_operand_immed[i]->~hsa_op_immed ();

  hsa_list_operand_code_list.release ();
  hsa_list_operand_reg.release ();
  hsa_list_operand_immed.release ();

  delete hsa_allocp_operand_address;
  delete hsa_allocp_operand_immed;
  delete hsa_allocp_operand_reg;
  delete hsa_allocp_operand_code_list;
  delete hsa_allocp_inst_basic;
  delete hsa_allocp_inst_phi;
  delete hsa_allocp_inst_atomic;
  delete hsa_allocp_inst_mem;
  delete hsa_allocp_inst_signal;
  delete hsa_allocp_inst_seg;
  delete hsa_allocp_inst_cmp;
  delete hsa_allocp_inst_br;
  delete hsa_allocp_inst_sbr;
  delete hsa_allocp_inst_call;
  delete hsa_allocp_inst_arg_block;
  delete hsa_allocp_inst_comment;
  delete hsa_allocp_inst_queue;
  delete hsa_allocp_bb;
  delete hsa_allocp_symbols;
  delete hsa_cfun;
}

/* Return the type which holds addresses in the given SEGMENT.  */

static BrigType16_t
hsa_get_segment_addr_type (BrigSegment8_t segment)
{
  switch (segment)
    {
    case BRIG_SEGMENT_NONE:
      gcc_unreachable ();

    case BRIG_SEGMENT_FLAT:
    case BRIG_SEGMENT_GLOBAL:
    case BRIG_SEGMENT_READONLY:
    case BRIG_SEGMENT_KERNARG:
      return hsa_machine_large_p () ? BRIG_TYPE_U64 : BRIG_TYPE_U32;

    case BRIG_SEGMENT_GROUP:
    case BRIG_SEGMENT_PRIVATE:
    case BRIG_SEGMENT_SPILL:
    case BRIG_SEGMENT_ARG:
      return BRIG_TYPE_U32;
    }
  gcc_unreachable ();
}

/* Return integer brig type according to provided SIZE in bytes.  If SIGN
   is set to true, return signed integer type.  */

static BrigType16_t
get_integer_type_by_bytes (unsigned size, bool sign)
{
  if (sign)
    switch (size)
      {
      case 1:
	return BRIG_TYPE_S8;
      case 2:
	return BRIG_TYPE_S16;
      case 4:
	return BRIG_TYPE_S32;
      case 8:
	return BRIG_TYPE_S64;
      default:
	break;
      }
  else
    switch (size)
      {
      case 1:
	return BRIG_TYPE_U8;
      case 2:
	return BRIG_TYPE_U16;
      case 4:
	return BRIG_TYPE_U32;
      case 8:
	return BRIG_TYPE_U64;
      default:
	break;
      }

  return 0;
}

/* Return HSA type for tree TYPE, which has to fit into BrigType16_t.  Pointers
   are assumed to use flat addressing.  If min32int is true, always expand
   integer types to one that has at least 32 bits.  */

static BrigType16_t
hsa_type_for_scalar_tree_type (const_tree type, bool min32int)
{
  HOST_WIDE_INT bsize;
  const_tree base;
  BrigType16_t res = BRIG_TYPE_NONE;

  gcc_checking_assert (TYPE_P (type));
  gcc_checking_assert (!AGGREGATE_TYPE_P (type));
  if (POINTER_TYPE_P (type))
    return hsa_get_segment_addr_type (BRIG_SEGMENT_FLAT);

  if (TREE_CODE (type) == VECTOR_TYPE || TREE_CODE (type) == COMPLEX_TYPE)
    base = TREE_TYPE (type);
  else
    base = type;

  if (!tree_fits_uhwi_p (TYPE_SIZE (base)))
    {
      HSA_SORRY_ATV (EXPR_LOCATION (type),
		     "support for HSA does not implement huge or "
		     "variable-sized type %T", type);
      return res;
    }

  bsize = tree_to_uhwi (TYPE_SIZE (base));
  unsigned byte_size = bsize / BITS_PER_UNIT;
  if (INTEGRAL_TYPE_P (base))
    res = get_integer_type_by_bytes (byte_size, !TYPE_UNSIGNED (base));
  else if (SCALAR_FLOAT_TYPE_P (base))
    {
      switch (bsize)
	{
	case 16:
	  res = BRIG_TYPE_F16;
	  break;
	case 32:
	  res = BRIG_TYPE_F32;
	  break;
	case 64:
	  res = BRIG_TYPE_F64;
	  break;
	default:
	  break;
	}
    }

  if (res == BRIG_TYPE_NONE)
    {
      HSA_SORRY_ATV (EXPR_LOCATION (type),
		     "support for HSA does not implement type %T", type);
      return res;
    }

  if (TREE_CODE (type) == VECTOR_TYPE || TREE_CODE (type) == COMPLEX_TYPE)
    {
      HOST_WIDE_INT tsize = tree_to_uhwi (TYPE_SIZE (type));

      if (bsize == tsize)
	{
	  HSA_SORRY_ATV (EXPR_LOCATION (type),
			 "support for HSA does not implement a vector type "
			 "where a type and unit size are equal: %T", type);
	  return res;
	}

      switch (tsize)
	{
	case 32:
	  res |= BRIG_TYPE_PACK_32;
	  break;
	case 64:
	  res |= BRIG_TYPE_PACK_64;
	  break;
	case 128:
	  res |= BRIG_TYPE_PACK_128;
	  break;
	default:
	  HSA_SORRY_ATV (EXPR_LOCATION (type),
			 "support for HSA does not implement type %T", type);
	}
    }

  if (min32int)
    {
      /* Registers/immediate operands can only be 32bit or more except for
         f16.  */
      if (res == BRIG_TYPE_U8 || res == BRIG_TYPE_U16)
	res = BRIG_TYPE_U32;
      else if (res == BRIG_TYPE_S8 || res == BRIG_TYPE_S16)
	res = BRIG_TYPE_S32;
    }
  return res;
}

/* Returns the BRIG type we need to load/store entities of TYPE.  */

static BrigType16_t
mem_type_for_type (BrigType16_t type)
{
  /* HSA has non-intuitive constraints on load/store types.  If it's
     a bit-type it _must_ be B128, if it's not a bit-type it must be
     64bit max.  So for loading entities of 128 bits (e.g. vectors)
     we have to to B128, while for loading the rest we have to use the
     input type (??? or maybe also flattened to a equally sized non-vector
     unsigned type?).  */
  if ((type & BRIG_TYPE_PACK_MASK) == BRIG_TYPE_PACK_128)
    return BRIG_TYPE_B128;
  return type;
}

/* Return HSA type for tree TYPE.  If it cannot fit into BrigType16_t, some
   kind of array will be generated, setting DIM appropriately.  Otherwise, it
   will be set to zero.  */

static BrigType16_t
hsa_type_for_tree_type (const_tree type, unsigned HOST_WIDE_INT *dim_p = NULL,
			bool min32int = false)
{
  gcc_checking_assert (TYPE_P (type));
  if (!tree_fits_uhwi_p (TYPE_SIZE_UNIT (type)))
    {
      HSA_SORRY_ATV (EXPR_LOCATION (type), "support for HSA does not "
		     "implement huge or variable-sized type %T", type);
      return BRIG_TYPE_NONE;
    }

  if (RECORD_OR_UNION_TYPE_P (type))
    {
      if (dim_p)
	*dim_p = tree_to_uhwi (TYPE_SIZE_UNIT (type));
      return BRIG_TYPE_U8 | BRIG_TYPE_ARRAY;
    }

  if (TREE_CODE (type) == ARRAY_TYPE)
    {
      /* We try to be nice and use the real base-type when this is an array of
	 scalars and only resort to an array of bytes if the type is more
	 complex.  */

      unsigned HOST_WIDE_INT dim = 1;

      while (TREE_CODE (type) == ARRAY_TYPE)
	{
	  tree domain = TYPE_DOMAIN (type);
	  if (!TYPE_MIN_VALUE (domain)
	      || !TYPE_MAX_VALUE (domain)
	      || !tree_fits_shwi_p (TYPE_MIN_VALUE (domain))
	      || !tree_fits_shwi_p (TYPE_MAX_VALUE (domain)))
	    {
	      HSA_SORRY_ATV (EXPR_LOCATION (type),
			     "support for HSA does not implement array %T with "
			     "unknown bounds", type);
	      return BRIG_TYPE_NONE;
	    }
	  HOST_WIDE_INT min = tree_to_shwi (TYPE_MIN_VALUE (domain));
	  HOST_WIDE_INT max = tree_to_shwi (TYPE_MAX_VALUE (domain));
	  dim = dim * (unsigned HOST_WIDE_INT) (max - min + 1);
	  type = TREE_TYPE (type);
	}

      BrigType16_t res;
      if (RECORD_OR_UNION_TYPE_P (type))
	{
	  dim = dim * tree_to_uhwi (TYPE_SIZE_UNIT (type));
	  res = BRIG_TYPE_U8;
	}
      else
	res = hsa_type_for_scalar_tree_type (type, false);

      if (dim_p)
	*dim_p = dim;
      return res | BRIG_TYPE_ARRAY;
    }

  /* Scalar case: */
  if (dim_p)
    *dim_p = 0;

  return hsa_type_for_scalar_tree_type (type, min32int);
}

/* Returns true if converting from STYPE into DTYPE needs the _CVT
   opcode.  If false a normal _MOV is enough.  */

static bool
hsa_needs_cvt (BrigType16_t dtype, BrigType16_t stype)
{
  /* float <-> int conversions are real converts.  */
  if (hsa_type_float_p (dtype) != hsa_type_float_p (stype))
    return true;
  /* When both types have different size, then we need CVT as well.  */
  if (hsa_type_bit_size (dtype) != hsa_type_bit_size (stype))
    return true;
  return false;
}

/* Fill in those values into SYM according to DECL, which are determined
   independently from whether it is parameter, result, or a variable, local or
   global.  */

static void
fillup_sym_for_decl (tree decl, struct hsa_symbol *sym)
{
  sym->decl = decl;
  sym->type = hsa_type_for_tree_type (TREE_TYPE (decl), &sym->dim);
}

/* Lookup or create the associated hsa_symbol structure with a given VAR_DECL
   or lookup the hsa_structure corresponding to a PARM_DECL.  */

static hsa_symbol *
get_symbol_for_decl (tree decl)
{
  struct hsa_symbol **slot;
  struct hsa_symbol dummy, *sym;

  gcc_assert (TREE_CODE (decl) == PARM_DECL
	      || TREE_CODE (decl) == RESULT_DECL
	      || TREE_CODE (decl) == VAR_DECL);

  dummy.decl = decl;

  if (TREE_CODE (decl) == VAR_DECL && is_global_var (decl))
    {
      slot = hsa_global_variable_symbols->find_slot (&dummy, INSERT);
      gcc_checking_assert (slot);
      if (*slot)
	return *slot;
      sym = XCNEW (struct hsa_symbol);
      sym->segment = BRIG_SEGMENT_GLOBAL;
      sym->linkage = BRIG_LINKAGE_FUNCTION;

      /* Following type of global variables can be handled.  */
      if (TREE_READONLY (decl) && !TREE_ADDRESSABLE (decl)
	  && DECL_INITIAL (decl) && TREE_CODE (TREE_TYPE (decl)) == ARRAY_TYPE
	  && TREE_CODE (TREE_TYPE (TREE_TYPE (decl))) == INTEGER_TYPE)
	{
	  sym->segment = BRIG_SEGMENT_READONLY;
	  sym->linkage = BRIG_LINKAGE_MODULE;
	  sym->cst_value = new hsa_op_immed (DECL_INITIAL (decl), false);
	  hsa_cfun->readonly_variables.safe_push (sym);
	}
      else
	warning (0, "referring to global symbol %q+D by name from HSA code "
		 "won't work", decl);
    }
  else
    {
      slot = hsa_cfun->local_symbols->find_slot (&dummy, INSERT);
      gcc_checking_assert (slot);
      if (*slot)
	return *slot;
      gcc_assert (TREE_CODE (decl) == VAR_DECL);
      sym = hsa_allocp_symbols->allocate ();
      memset (sym, 0, sizeof (hsa_symbol));
      sym->segment = BRIG_SEGMENT_PRIVATE;
      sym->linkage = BRIG_LINKAGE_FUNCTION;
    }

  fillup_sym_for_decl (decl, sym);
  sym->name = hsa_get_declaration_name (decl);
  *slot = sym;
  return sym;
}

/* For a given function declaration, return a GPU function
   of the function.  */

static tree
hsa_get_gpu_function (tree decl)
{
  hsa_function_summary *s = hsa_summaries->get (cgraph_node::get_create (decl));
  gcc_assert (s->kind != HSA_NONE);
  gcc_assert (!s->gpu_implementation_p);

  return s->binded_function->decl;
}

/* For a given HSA function declaration, return a host
   function declaration.  */

tree
hsa_get_host_function (tree decl)
{
  hsa_function_summary *s = hsa_summaries->get (cgraph_node::get_create (decl));
  gcc_assert (s->kind != HSA_NONE);
  gcc_assert (s->gpu_implementation_p);

  return s->binded_function->decl;
}

/* Create a spill symbol of type TYPE.  */

hsa_symbol *
hsa_get_spill_symbol (BrigType16_t type)
{
  hsa_symbol *sym = hsa_allocp_symbols->allocate ();
  memset (sym, 0, sizeof (hsa_symbol));
  sym->segment = BRIG_SEGMENT_SPILL;
  sym->linkage = BRIG_LINKAGE_FUNCTION;
  sym->type = type;
  hsa_cfun->spill_symbols.safe_push(sym);
  return sym;
}

/* Create a symbol for a read-only string constant.  */
hsa_symbol *
hsa_get_string_cst_symbol (tree string_cst)
{
  gcc_checking_assert (TREE_CODE (string_cst) == STRING_CST);

  hsa_symbol **slot = hsa_cfun->string_constants_map.get (string_cst);
  if (slot)
    return *slot;

  hsa_symbol *sym = hsa_allocp_symbols->allocate ();
  memset (sym, 0, sizeof (hsa_symbol));

  sym->segment = BRIG_SEGMENT_GLOBAL;
  sym->linkage = BRIG_LINKAGE_MODULE;
  sym->cst_value = new hsa_op_immed (string_cst);
  sym->type = sym->cst_value->type;
  sym->dim = TREE_STRING_LENGTH (string_cst);
  sym->name_number = hsa_cfun->readonly_variables.length ();
  sym->global_scope_p = true;

  hsa_cfun->readonly_variables.safe_push (sym);
  hsa_cfun->string_constants_map.put (string_cst, sym);
  return sym;
}

/* Constructor of the ancetor if all operands.  K is BRIG kind that identified
   what the operator is.  */

hsa_op_base::hsa_op_base (BrigKind16_t k)
{
  next = NULL;
  brig_op_offset = 0;
  kind = k;
}

/* Constructor of ancestor of all operands which have a type.  K is BRIG kind
   that identified what the operator is.  T is the type of the operator.  */

hsa_op_with_type::hsa_op_with_type (BrigKind16_t k, BrigType16_t t)
  : hsa_op_base (k)
{
  type = t;
}

/* Constructor of class representing HSA immediate values.  TREE_VAL is the
   tree representation of the immediate value.  If min32int is true,
   always expand integer types to one that has at least 32 bits.  */

hsa_op_immed::hsa_op_immed (tree tree_val, bool min32int)
  : hsa_op_with_type (BRIG_KIND_OPERAND_CONSTANT_BYTES,
		      hsa_type_for_tree_type (TREE_TYPE (tree_val), NULL,
					      min32int))
{
  if (hsa_seen_error ())
    return;

  gcc_checking_assert ((is_gimple_min_invariant (tree_val)
		       && (!POINTER_TYPE_P (TREE_TYPE (tree_val))
			   || TREE_CODE (tree_val) == INTEGER_CST))
		       || TREE_CODE (tree_val) == CONSTRUCTOR);
  tree_value = tree_val;
  brig_repr_size = hsa_get_imm_brig_type_len (type);

  if (TREE_CODE (tree_value) == STRING_CST)
    brig_repr_size = TREE_STRING_LENGTH (tree_value);
  else if (TREE_CODE (tree_value) == CONSTRUCTOR)
    {
      brig_repr_size = tree_to_uhwi (TYPE_SIZE_UNIT (TREE_TYPE (tree_value)));

      /* Verify that all elements of a contructor are constants.  */
      for (unsigned i = 0; i < vec_safe_length (CONSTRUCTOR_ELTS (tree_value));
	   i++)
	{
	  tree v = CONSTRUCTOR_ELT (tree_value, i)->value;
	  if (!CONSTANT_CLASS_P (v))
	    {
	      HSA_SORRY_AT (EXPR_LOCATION (tree_val),
			    "HSA ctor should have only constants");
	      return;
	    }
	}
    }

  emit_to_buffer (tree_value);
  hsa_list_operand_immed.safe_push (this);
}

/* Constructor of class representing HSA immediate values.  INTEGER_VALUE is the
   integer representation of the immediate value.  TYPE is BRIG type.  */

hsa_op_immed::hsa_op_immed (HOST_WIDE_INT integer_value, BrigKind16_t type)
  : hsa_op_with_type (BRIG_KIND_OPERAND_CONSTANT_BYTES, type), tree_value (NULL)
{
  gcc_assert (hsa_type_integer_p (type));
  int_value = integer_value;
  brig_repr_size = hsa_type_bit_size (type) / BITS_PER_UNIT;

  hsa_bytes bytes;

  switch (brig_repr_size)
    {
    case 1:
      bytes.b8 = (uint8_t) int_value;
      break;
    case 2:
      bytes.b16 = (uint16_t) int_value;
      break;
    case 4:
      bytes.b32 = (uint32_t) int_value;
      break;
    case 8:
      bytes.b64 = (uint64_t) int_value;
      break;
    default:
      gcc_unreachable ();
    }

  brig_repr = XNEWVEC (char, brig_repr_size);
  memcpy (brig_repr, &bytes, brig_repr_size);
  hsa_list_operand_immed.safe_push (this);
}

/* New operator to allocate immediate operands from pool alloc.  */

void *
hsa_op_immed::operator new (size_t)
{
  return hsa_allocp_operand_immed->vallocate ();
}

/* Destructor.  */

hsa_op_immed::~hsa_op_immed ()
{
  free (brig_repr);
}

/* Change type of the immediate value to T.  */

void
hsa_op_immed::set_type (BrigType16_t t)
{
  type = t;
}

/* Constructor of class representing HSA registers and pseudo-registers.  T is
   the BRIG type fo the new register.  */

hsa_op_reg::hsa_op_reg (BrigType16_t t)
  : hsa_op_with_type (BRIG_KIND_OPERAND_REGISTER, t)
{
  gimple_ssa = NULL_TREE;
  def_insn = NULL;
  spill_sym = NULL;
  order = hsa_cfun->reg_count++;
  lr_begin = lr_end = 0;
  reg_class = 0;
  hard_num = 0;

  hsa_list_operand_reg.safe_push (this);
}

/* New operator to allocate a register from pool alloc.  */

void *
hsa_op_reg::operator new (size_t)
{
  return hsa_allocp_operand_reg->vallocate ();
}

/* Verify register operand.  */

void
hsa_op_reg::verify_ssa ()
{
  /* Verify that each HSA register has a definition assigned.
     Exceptions are VAR_DECL and PARM_DECL that are a default
     definition.  */
  gcc_checking_assert (def_insn
		       || (gimple_ssa != NULL
			   && (!SSA_NAME_VAR (gimple_ssa)
		               || (TREE_CODE (SSA_NAME_VAR (gimple_ssa))
				   != PARM_DECL))
			   && SSA_NAME_IS_DEFAULT_DEF (gimple_ssa)));

  /* Verify that every use of the register is really present
     in an instruction.  */
  for (unsigned i = 0; i < uses.length (); i++)
    {
      hsa_insn_basic *use = uses[i];

      bool is_visited = false;
      for (unsigned j = 0; j < use->operand_count (); j++)
	{
	  hsa_op_base *u = use->get_op (j);
	  hsa_op_address *addr; addr = dyn_cast <hsa_op_address *> (u);
	  if (addr && addr->reg)
	    u = addr->reg;

	  if (u == this)
	    {
	      bool r = !addr && hsa_opcode_op_output_p (use->opcode, j);

	      if (r)
		{
		  error ("HSA SSA name defined by intruction that is supposed "
			 "to be using it");
		  debug_hsa_operand (this);
		  debug_hsa_insn (use);
		  internal_error ("HSA SSA verification failed");
		}

	      is_visited = true;
	    }
	}

      if (!is_visited)
	{
	  error ("HSA SSA name not among operands of instruction that is "
		 "supposed to use it");
	  debug_hsa_operand (this);
	  debug_hsa_insn (use);
	  internal_error ("HSA SSA verification failed");
	}
    }
}

hsa_op_address::hsa_op_address (hsa_symbol *sym, hsa_op_reg *r,
				HOST_WIDE_INT offset)
  : hsa_op_base (BRIG_KIND_OPERAND_ADDRESS), symbol (sym), reg (r),
  imm_offset (offset)
{
}

hsa_op_address::hsa_op_address (hsa_symbol *sym, HOST_WIDE_INT offset)
  : hsa_op_base (BRIG_KIND_OPERAND_ADDRESS), symbol (sym), reg (NULL),
  imm_offset (offset)
{
}

hsa_op_address::hsa_op_address (hsa_op_reg *r, HOST_WIDE_INT offset)
  : hsa_op_base (BRIG_KIND_OPERAND_ADDRESS), symbol (NULL), reg (r),
  imm_offset (offset)
{
}

/* New operator to allocate address operands from pool alloc.  */

void *
hsa_op_address::operator new (size_t)
{
  return hsa_allocp_operand_address->vallocate ();
}


/* Constructor of an operand referring to HSAIL code.  */

hsa_op_code_ref::hsa_op_code_ref () : hsa_op_base (BRIG_KIND_OPERAND_CODE_REF)
{
  directive_offset = 0;
}

/* Constructor of an operand representing a code list.  Set it up so that it
   can contain ELEMENTS number of elements.  */

hsa_op_code_list::hsa_op_code_list (unsigned elements)
  : hsa_op_base (BRIG_KIND_OPERAND_CODE_LIST)
{
  offsets.create (1);
  offsets.safe_grow_cleared (elements);

  hsa_list_operand_code_list.safe_push (this);
}

/* New operator to allocate code list operands from pool alloc.  */

void *
hsa_op_code_list::operator new (size_t)
{
  return hsa_allocp_operand_code_list->vallocate ();
}

/* Lookup or create a HSA pseudo register for a given gimple SSA name.  */

static hsa_op_reg *
hsa_reg_for_gimple_ssa (tree ssa, vec <hsa_op_reg_p> *ssa_map)
{
  hsa_op_reg *hreg;

  gcc_checking_assert (TREE_CODE (ssa) == SSA_NAME);
  if ((*ssa_map)[SSA_NAME_VERSION (ssa)])
    return (*ssa_map)[SSA_NAME_VERSION (ssa)];

  hreg = new  hsa_op_reg (hsa_type_for_scalar_tree_type (TREE_TYPE (ssa),
							 true));
  hreg->gimple_ssa = ssa;
  (*ssa_map)[SSA_NAME_VERSION (ssa)] = hreg;

  return hreg;
}

void
hsa_op_reg::set_definition (hsa_insn_basic *insn)
{
  if (hsa_cfun->in_ssa)
    {
      gcc_checking_assert (!def_insn);
      def_insn = insn;
    }
  else
    def_insn = NULL;
}

/* Constructor of the class which is the bases of all instructions and directly
   represents the most basic ones.  NOPS is the number of operands that the
   operand vector will contain (and which will be cleared).  OP is the opcode
   of the instruction.  This constructor does not set type.  */

hsa_insn_basic::hsa_insn_basic (unsigned nops, int opc)
{
  opcode = opc;

  prev = next = NULL;
  bb = NULL;
  number = 0;
  type = BRIG_TYPE_NONE;
  brig_offset = 0;

  if (nops > 0)
    operands.safe_grow_cleared (nops);
}

/* Make OP the operand number INDEX of operands of this instuction.  If OP is a
   register or an address containing a register, then either set the definition
   of the register to this instruction if it an output operand or add this
   instruction to the uses if it is an input one.  */

void
hsa_insn_basic::set_op (int index, hsa_op_base *op)
{
  /* Each address operand is always use.  */
  hsa_op_address *addr = dyn_cast <hsa_op_address *> (op);
  if (addr && addr->reg)
    addr->reg->uses.safe_push (this);
  else
    {
      hsa_op_reg *reg = dyn_cast <hsa_op_reg *> (op);
      if (reg)
	{
	  if (hsa_opcode_op_output_p (opcode, index))
	    reg->set_definition (this);
	  else
	    reg->uses.safe_push (this);
	}
    }

  operands[index] = op;
}

/* Get INDEX-th operand of the instruction.  */

hsa_op_base *
hsa_insn_basic::get_op (int index)
{
  return operands[index];
}

/* Get address of INDEX-th operand of the instruction.  */

hsa_op_base **
hsa_insn_basic::get_op_addr (int index)
{
  return &operands[index];
}

/* Get number of operands of the instruction.  */
unsigned int
hsa_insn_basic::operand_count ()
{
  return operands.length ();
}

/* Constructor of the class which is the bases of all instructions and directly
   represents the most basic ones.  NOPS is the number of operands that the
   operand vector will contain (and which will be cleared).  OPC is the opcode
   of the instruction, T is the type of the instruction.  */

hsa_insn_basic::hsa_insn_basic (unsigned nops, int opc, BrigType16_t t,
				hsa_op_base *arg0, hsa_op_base *arg1,
				hsa_op_base *arg2, hsa_op_base *arg3)
{
  opcode = opc;
  type = t;

  prev = next = NULL;
  bb = NULL;
  number = 0;
  brig_offset = 0;

  if (nops > 0)
    operands.safe_grow_cleared (nops);

  if (arg0 != NULL)
    {
      gcc_checking_assert (nops >= 1);
      set_op (0, arg0);
    }

  if (arg1 != NULL)
    {
      gcc_checking_assert (nops >= 2);
      set_op (1, arg1);
    }

  if (arg2 != NULL)
    {
      gcc_checking_assert (nops >= 3);
      set_op (2, arg2);
    }

  if (arg3 != NULL)
    {
      gcc_checking_assert (nops >= 4);
      set_op (3, arg3);
    }
}

/* New operator to allocate basic instruction from pool alloc.  */

void *
hsa_insn_basic::operator new (size_t)
{
  return hsa_allocp_inst_basic->vallocate ();
}

/* Verify the instruction.  */

void
hsa_insn_basic::verify ()
{
  hsa_op_address *addr;
  hsa_op_reg *reg;

  /* Iterate all register operands and verify that the instruction
     is set in uses of the register.  */
  for (unsigned i = 0; i < operand_count (); i++)
    {
      hsa_op_base *use = get_op (i);

      if ((addr = dyn_cast <hsa_op_address *> (use)) && addr->reg)
	{
	  gcc_assert (addr->reg->def_insn != this);
	  use = addr->reg;
	}

      if ((reg = dyn_cast <hsa_op_reg *> (use))
	  && !hsa_opcode_op_output_p (opcode, i))
	{
	  unsigned j;
	  for (j = 0; j < reg->uses.length (); j++)
	    {
	      if (reg->uses[j] == this)
		break;
	    }

	  if (j == reg->uses.length ())
	    {
	      error ("HSA instruction uses a register but is not among "
		     "recorded register uses");
	      debug_hsa_operand (reg);
	      debug_hsa_insn (this);
	      internal_error ("HSA instruction verification failed");
	    }
	}
    }
}

/* Constructor of an instruction representing a PHI node.  NOPS is the number
   of operands (equal to the number of predecessors).  */

hsa_insn_phi::hsa_insn_phi (unsigned nops, hsa_op_reg *dst)
  : hsa_insn_basic (nops, HSA_OPCODE_PHI)
{
  dest = dst;
  dst->set_definition (this);
}

/* New operator to allocate PHI instruction from pool alloc.  */

void *
hsa_insn_phi::operator new (size_t)
{
  return hsa_allocp_inst_phi->vallocate ();
}

/* Constructor of clas representing instruction for conditional jump, CTRL is
   the control register deterining whether the jump will be carried out, the
   new instruction is automatically added to its uses list.  */

hsa_insn_br::hsa_insn_br (hsa_op_reg *ctrl)
: hsa_insn_basic (1, BRIG_OPCODE_CBR, BRIG_TYPE_B1, ctrl)
{
  width = BRIG_WIDTH_1;
}

/* New operator to allocate branch instruction from pool alloc.  */

void *
hsa_insn_br::operator new (size_t)
{
  return hsa_allocp_inst_br->vallocate ();
}

/* Constructor of class representing instruction for switch jump, CTRL is
   the index register.  */

hsa_insn_sbr::hsa_insn_sbr (hsa_op_reg *index, unsigned jump_count)
: hsa_insn_basic (1, BRIG_OPCODE_SBR, BRIG_TYPE_B1, index)
{
  width = BRIG_WIDTH_1;
  jump_table = vNULL;
  default_bb = NULL;
  label_code_list = new hsa_op_code_list (jump_count);
}

/* New operator to allocate switch branch instruction from pool alloc.  */

void *
hsa_insn_sbr::operator new (size_t)
{
  return hsa_allocp_inst_sbr->vallocate ();
}

/* Replace all occurrences of OLD_BB with NEW_BB in the statements
   jump table.  */

void
hsa_insn_sbr::replace_all_labels (basic_block old_bb, basic_block new_bb)
{
  for (unsigned i = 0; i < jump_table.length (); i++)
    if (jump_table[i] == old_bb)
      jump_table[i] = new_bb;
}

/* Constructor of comparison instructin.  CMP is the comparison operation and T
   is the result type.  */

hsa_insn_cmp::hsa_insn_cmp (BrigCompareOperation8_t cmp, BrigType16_t t,
			    hsa_op_base *arg0, hsa_op_base *arg1,
			    hsa_op_base *arg2)
  : hsa_insn_basic (3 , BRIG_OPCODE_CMP, t, arg0, arg1, arg2)
{
  compare = cmp;
}

/* New operator to allocate compare instruction from pool alloc.  */

void *
hsa_insn_cmp::operator new (size_t)
{
  return hsa_allocp_inst_cmp->vallocate ();
}

/* Constructor of classes representing memory accesses.  OPC is the opcode (must
   be BRIG_OPCODE_ST or BRIG_OPCODE_LD) and T is the type.  The instruction
   operands are provided as ARG0 and ARG1.  */

hsa_insn_mem::hsa_insn_mem (int opc, BrigType16_t t, hsa_op_base *arg0,
			    hsa_op_base *arg1)
  : hsa_insn_basic (2, opc, t, arg0, arg1)
{
  gcc_checking_assert (opc == BRIG_OPCODE_LD || opc == BRIG_OPCODE_ST
		       || opc == BRIG_OPCODE_EXPAND);

  equiv_class = 0;
}

/* Constructor for descendants allowing different opcodes and number of
   operands, it passes its arguments directly to hsa_insn_basic
   constructor.  The instruction operands are provided as ARG[0-3].  */


hsa_insn_mem::hsa_insn_mem (unsigned nops, int opc, BrigType16_t t,
			    hsa_op_base *arg0, hsa_op_base *arg1,
			    hsa_op_base *arg2, hsa_op_base *arg3)
  : hsa_insn_basic (nops, opc, t, arg0, arg1, arg2, arg3)
{
  equiv_class = 0;
}

/* New operator to allocate memory instruction from pool alloc.  */

void *
hsa_insn_mem::operator new (size_t)
{
  return hsa_allocp_inst_mem->vallocate ();
}

/* Constructor of class representing atomic instructions and signals. OPC is
   the prinicpal opcode, aop is the specific atomic operation opcode.  T is the
   type of the instruction.  The instruction operands
   are provided as ARG[0-3].  */

hsa_insn_atomic::hsa_insn_atomic (int nops, int opc,
				  enum BrigAtomicOperation aop,
				  BrigType16_t t, hsa_op_base *arg0,
				  hsa_op_base *arg1, hsa_op_base *arg2,
				  hsa_op_base *arg3)
  : hsa_insn_mem (nops, opc, t, arg0, arg1, arg2, arg3)
{
  gcc_checking_assert (opc == BRIG_OPCODE_ATOMICNORET ||
		       opc == BRIG_OPCODE_ATOMIC ||
		       opc == BRIG_OPCODE_SIGNAL ||
		       opc == BRIG_OPCODE_SIGNALNORET);
  atomicop = aop;
  /* TODO: Review the following defaults (together with the few overriddes we
     have in the code).  */
  memoryorder = BRIG_MEMORY_ORDER_SC_ACQUIRE_RELEASE;
  memoryscope = BRIG_MEMORY_SCOPE_SYSTEM;
}

/* New operator to allocate signal instruction from pool alloc.  */

void *
hsa_insn_atomic::operator new (size_t)
{
  return hsa_allocp_inst_atomic->vallocate ();
}

/* Constructor of class representing signal instructions.  OPC is the prinicpa;
   opcode, sop is the specific signal operation opcode.  T is the type of the
   instruction.  The instruction operands are provided as ARG[0-3].  */

hsa_insn_signal::hsa_insn_signal (int nops, int opc,
				  enum BrigAtomicOperation sop,
				  BrigType16_t t, hsa_op_base *arg0,
				  hsa_op_base *arg1, hsa_op_base *arg2,
				  hsa_op_base *arg3)
  : hsa_insn_atomic (nops, opc, sop, t, arg0, arg1, arg2, arg3)
{
}

/* New operator to allocate signal instruction from pool alloc.  */

void *
hsa_insn_signal::operator new (size_t)
{
  return hsa_allocp_inst_signal->vallocate ();
}

/* Constructor of class representing segment conversion instructions.  OPC is
   the opcode which must be either BRIG_OPCODE_STOF or BRIG_OPCODE_FTOS.  DESTT
   and SRCT are destination and source types respectively, SEG is the segment
   we are converting to or from.  The instruction operands are
   provided as ARG0 and ARG1.  */

hsa_insn_seg::hsa_insn_seg (int opc, BrigType16_t dest, BrigType16_t srct,
			    BrigSegment8_t seg, hsa_op_base *arg0,
			    hsa_op_base *arg1)
  : hsa_insn_basic (2, opc, dest, arg0, arg1)
{
  gcc_checking_assert (opc == BRIG_OPCODE_STOF || opc == BRIG_OPCODE_FTOS);
  src_type = srct;
  segment = seg;
}

/* New operator to allocate address conversion instruction from pool alloc.  */

void *
hsa_insn_seg::operator new (size_t)
{
  return hsa_allocp_inst_seg->vallocate ();
}

/* Constructor of class representing a call instruction.  CALLEE is the tree
   representation of the function being called.  */

hsa_insn_call::hsa_insn_call (tree callee) : hsa_insn_basic (0, BRIG_OPCODE_CALL)
{
  called_function = callee;
  args_code_list = NULL;
  result_symbol = NULL;
  result_code_list = NULL;
}

/* New operator to allocate call instruction from pool alloc.  */

void *
hsa_insn_call::operator new (size_t)
{
  return hsa_allocp_inst_call->vallocate ();
}

/* Constructor of class representing the argument block required to invoke
   a call in HSAIL.  */
hsa_insn_arg_block::hsa_insn_arg_block (BrigKind brig_kind,
					hsa_insn_call * call)
  : hsa_insn_basic (0, HSA_OPCODE_ARG_BLOCK), kind (brig_kind),
  call_insn (call)
{
}

/* New operator to allocate argument block instruction from pool alloc.  */

void *
hsa_insn_arg_block::operator new (size_t)
{
  return hsa_allocp_inst_arg_block->vallocate ();
}

hsa_insn_comment::hsa_insn_comment (const char *s)
  : hsa_insn_basic (0, BRIG_KIND_DIRECTIVE_COMMENT)
{
  unsigned l = strlen (s);

  /* Append '// ' to the string.  */
  char *buf = XNEWVEC (char, l + 4);
  sprintf (buf, "// %s", s);
  comment = buf;
}

/* New operator to allocate comment instruction from pool alloc.  */

void *
hsa_insn_comment::operator new (size_t)
{
  return hsa_allocp_inst_comment->vallocate ();
}

void
hsa_insn_comment::release_string ()
{
  gcc_checking_assert (comment);
  free (comment);
  comment = NULL;
}

/* Constructor of class representing the queue instruction in HSAIL.  */
hsa_insn_queue::hsa_insn_queue (int nops, BrigOpcode opcode)
  : hsa_insn_basic (nops, opcode, BRIG_TYPE_U64)
{
}

/* Append an instruction INSN into the basic block.  */

void
hsa_bb::append_insn (hsa_insn_basic *insn)
{
  gcc_assert (insn->opcode != 0 || insn->operand_count () == 0);
  gcc_assert (!insn->bb);

  insn->bb = bb;
  insn->prev = last_insn;
  insn->next = NULL;
  if (last_insn)
    last_insn->next = insn;
  last_insn = insn;
  if (!first_insn)
    first_insn = insn;
}

/* Insert HSA instruction NEW_INSN immediately before an existing instruction
   OLD_INSN.  */

static void
hsa_insert_insn_before (hsa_insn_basic *new_insn, hsa_insn_basic *old_insn)
{
  hsa_bb *hbb = hsa_bb_for_bb (old_insn->bb);

  if (hbb->first_insn == old_insn)
    hbb->first_insn = new_insn;
  new_insn->prev = old_insn->prev;
  new_insn->next = old_insn;
  if (old_insn->prev)
    old_insn->prev->next = new_insn;
  old_insn->prev = new_insn;
}

/* Append HSA instruction NEW_INSN immediately after an existing instruction
   OLD_INSN.  */

static void
hsa_append_insn_after (hsa_insn_basic *new_insn, hsa_insn_basic *old_insn)
{
  hsa_bb *hbb = hsa_bb_for_bb (old_insn->bb);

  if (hbb->last_insn == old_insn)
    hbb->last_insn = new_insn;
  new_insn->prev = old_insn;
  new_insn->next = old_insn->next;
  if (old_insn->next)
    old_insn->next->prev = new_insn;
  old_insn->next = new_insn;
}

/* Lookup or create a HSA pseudo register for a given gimple SSA name and if
   necessary, convert it to REQTYPE.  SSA_MAP is a vector mapping gimple
   SSA names to HSA registers.  Append an new conversion statements to HBB.  */

static hsa_op_reg *
hsa_reg_for_gimple_ssa_reqtype (tree ssa, vec <hsa_op_reg_p> *ssa_map,
				hsa_bb *hbb, BrigType16_t reqtype)
{
  hsa_op_reg *reg = hsa_reg_for_gimple_ssa (ssa, ssa_map);
  if (hsa_needs_cvt (reqtype, reg->type))
    {
      hsa_op_reg *converted = new hsa_op_reg (reqtype);
      hsa_insn_basic *insn = new hsa_insn_basic (2, BRIG_OPCODE_CVT, reqtype,
						 converted, reg);
      hbb->append_insn (insn);
      return converted;
    }

  return reg;
}

/* Return a register containing the calculated value of EXP which must be an
   expression consisting of PLUS_EXPRs, MULT_EXPRs, NOP_EXPRs, SSA_NAMEs and
   integer constants as returned by get_inner_reference.  SSA_MAP is used to
   lookup HSA equivalent of SSA_NAMEs, newly generated HSA instructions will be
   appended to HBB.  Perform all calculations in ADDRTYPE.  */

static hsa_op_with_type *
gen_address_calculation (tree exp, hsa_bb *hbb, vec <hsa_op_reg_p> *ssa_map,
			 BrigType16_t addrtype)
{
  int opcode;

  if (TREE_CODE (exp) == NOP_EXPR)
    exp =TREE_OPERAND (exp, 0);

  switch (TREE_CODE (exp))
    {
    case SSA_NAME:
      {
	hsa_op_reg *res = hsa_reg_for_gimple_ssa_reqtype (exp, ssa_map, hbb,
							  addrtype);
	return res;
      }

    case INTEGER_CST:
      {
       hsa_op_immed *imm = new hsa_op_immed (exp);
       if (addrtype != imm->type)
	 imm->type = addrtype;
       return imm;
      }

    case PLUS_EXPR:
      opcode = BRIG_OPCODE_ADD;
      break;

    case MULT_EXPR:
      opcode = BRIG_OPCODE_MUL;
      break;

    default:
      gcc_unreachable ();
    }

  hsa_op_reg *res = new hsa_op_reg (addrtype);
  hsa_insn_basic *insn = new hsa_insn_basic (3, opcode, addrtype);
  insn->set_op (0, res);

  hsa_op_with_type *op1 = gen_address_calculation (TREE_OPERAND (exp, 0), hbb,
						   ssa_map, addrtype);
  hsa_op_with_type *op2 = gen_address_calculation (TREE_OPERAND (exp, 1), hbb,
						   ssa_map, addrtype);
  insn->set_op (1, op1);
  insn->set_op (2, op2);

  hbb->append_insn (insn);
  return res;
}

/* If R1 is NULL, just return R2, otherwise append an instruction adding them
   to HBB and return the register holding the result.  */

static hsa_op_reg *
add_addr_regs_if_needed (hsa_op_reg *r1, hsa_op_reg *r2, hsa_bb *hbb)
{
  gcc_checking_assert (r2);
  if (!r1)
    return r2;

  hsa_op_reg *res = new hsa_op_reg (r1->type);
  gcc_assert (!hsa_needs_cvt (r1->type, r2->type));
  hsa_insn_basic *insn = new hsa_insn_basic (3, BRIG_OPCODE_ADD, res->type);
  insn->set_op (0, res);
  insn->set_op (1, r1);
  insn->set_op (2, r2);
  hbb->append_insn (insn);
  return res;
}

/* Helper of gen_hsa_addr.  Update *SYMBOL, *ADDRTYPE, *REG and *OFFSET to
   reflect BASE which is the first operand of a MEM_REF or a TARGET_MEM_REF.
   Use SSA_MAP to find registers corresponding to gimple SSA_NAMEs.  */

static void
process_mem_base (tree base, hsa_symbol **symbol, BrigType16_t *addrtype,
		  hsa_op_reg **reg, offset_int *offset, hsa_bb *hbb,
		  vec <hsa_op_reg_p> *ssa_map)
{
  if (TREE_CODE (base) == SSA_NAME)
    {
      gcc_assert (!*reg);
      *reg = hsa_reg_for_gimple_ssa_reqtype (base, ssa_map, hbb, *addrtype);
    }
  else if (TREE_CODE (base) == ADDR_EXPR)
    {
      tree decl = TREE_OPERAND (base, 0);

      if (!DECL_P (decl) || TREE_CODE (decl) == FUNCTION_DECL)
	{
	  HSA_SORRY_AT (EXPR_LOCATION (base),
			"support for HSA does not implement a memory reference "
			"to a non-declaration type");
	  return;
	}

      gcc_assert (!*symbol);

      *symbol = get_symbol_for_decl (decl);
      *addrtype = hsa_get_segment_addr_type ((*symbol)->segment);
    }
  else if (TREE_CODE (base) == INTEGER_CST)
    *offset += wi::to_offset (base);
  else
    gcc_unreachable ();
}

/* Generate HSA address operand for a given tree memory reference REF.  If
   instructions need to be created to calculate the address, they will be added
   to the end of HBB, SSA_MAP is an array mapping gimple SSA names to HSA
   pseudo-registers.  If a caller provider OUTPUT_BITSIZE and OUTPUT_BITPOS,
   the function assumes that the caller will handle possible
   bitfield references.  Otherwise if we reference a bitfield, sorry message
   is displayed.  */

static hsa_op_address *
gen_hsa_addr (tree ref, hsa_bb *hbb, vec <hsa_op_reg_p> *ssa_map,
	      HOST_WIDE_INT *output_bitsize = NULL,
	      HOST_WIDE_INT *output_bitpos = NULL)
{
  hsa_symbol *symbol = NULL;
  hsa_op_reg *reg = NULL;
  offset_int offset = 0;
  tree origref = ref;
  tree varoffset = NULL_TREE;
  BrigType16_t addrtype = hsa_get_segment_addr_type (BRIG_SEGMENT_FLAT);
  HOST_WIDE_INT bitsize = 0, bitpos = 0;

  if (TREE_CODE (ref) == STRING_CST)
    {
      symbol = hsa_get_string_cst_symbol (ref);
      goto out;
    }
  else if (TREE_CODE (ref) == BIT_FIELD_REF
	   && ((tree_to_uhwi (TREE_OPERAND (ref, 1)) % BITS_PER_UNIT) != 0
	       || (tree_to_uhwi (TREE_OPERAND (ref, 2)) % BITS_PER_UNIT) != 0))
    {
      HSA_SORRY_ATV (EXPR_LOCATION (origref),
		     "support for HSA does not implement "
		     "bit field references such as %E", ref);
      goto out;
    }

  if (handled_component_p (ref))
    {
      enum machine_mode mode;
      int unsignedp, volatilep;

      ref = get_inner_reference (ref, &bitsize, &bitpos, &varoffset, &mode,
				 &unsignedp, &volatilep, false);

      offset = bitpos;
      offset = wi::rshift (offset, LOG2_BITS_PER_UNIT, SIGNED);
    }

  switch (TREE_CODE (ref))
    {
    case ADDR_EXPR:
      gcc_unreachable ();

    case PARM_DECL:
    case VAR_DECL:
    case RESULT_DECL:
      gcc_assert (!symbol);
      symbol = get_symbol_for_decl (ref);
      addrtype = hsa_get_segment_addr_type (symbol->segment);
      break;

    case MEM_REF:
      process_mem_base (TREE_OPERAND (ref, 0), &symbol, &addrtype, &reg,
			&offset, hbb, ssa_map);

      if (!integer_zerop (TREE_OPERAND (ref, 1)))
	offset += wi::to_offset (TREE_OPERAND (ref, 1));
      break;

    case TARGET_MEM_REF:
      process_mem_base (TMR_BASE (ref), &symbol, &addrtype, &reg, &offset, hbb,
			ssa_map);
      if (TMR_INDEX (ref))
	{
	  hsa_op_reg *disp1, *idx;
	  idx = hsa_reg_for_gimple_ssa_reqtype (TMR_INDEX (ref), ssa_map, hbb,
						addrtype);
	  if (TMR_STEP (ref) && !integer_onep (TMR_STEP (ref)))
	    {
	      disp1 = new hsa_op_reg (addrtype);
	      hsa_insn_basic *insn = new hsa_insn_basic (3, BRIG_OPCODE_MUL,
							 addrtype);

	      /* As step must respect addrtype, we overwrite the type
		 of an immediate value.  */
	      hsa_op_immed *step = new hsa_op_immed (TMR_STEP (ref));
	      step->type = addrtype;

	      insn->set_op (0, disp1);
	      insn->set_op (1, idx);
	      insn->set_op (2, step);
	      hbb->append_insn (insn);
	    }
	  else
	    disp1 = idx;
	  reg = add_addr_regs_if_needed (reg, disp1, hbb);
	}
      if (TMR_INDEX2 (ref))
	{
	  hsa_op_reg *disp2;
	  disp2 = hsa_reg_for_gimple_ssa_reqtype (TMR_INDEX2 (ref), ssa_map,
						  hbb, addrtype);
	  reg = add_addr_regs_if_needed (reg, disp2, hbb);
	}
      offset += wi::to_offset (TMR_OFFSET (ref));
      break;
    case FUNCTION_DECL:
      HSA_SORRY_AT (EXPR_LOCATION (origref),
		    "support for HSA does not implement function pointers");
      goto out;
    case SSA_NAME:
    default:
      HSA_SORRY_ATV (EXPR_LOCATION (origref), "support for HSA does "
		     "not implement memory access to %E", origref);
      goto out;
    }

  if (varoffset)
    {
      if (TREE_CODE (varoffset) == INTEGER_CST)
	offset += wi::to_offset (varoffset);
      else
	{
	  hsa_op_base *off_op = gen_address_calculation (varoffset, hbb, ssa_map,
							 addrtype);
	  reg = add_addr_regs_if_needed (reg, as_a <hsa_op_reg *> (off_op), hbb);
	}
    }

  gcc_checking_assert ((symbol
			&& addrtype
			== hsa_get_segment_addr_type (symbol->segment))
		       || (!symbol
			   && addrtype
			   == hsa_get_segment_addr_type (BRIG_SEGMENT_FLAT)));
out:
  HOST_WIDE_INT hwi_offset = offset.to_shwi ();

  /* Calculate remaining bitsize offset (if presented).  */
  bitpos %= BITS_PER_UNIT;
  /* If bitsize is a power of two that is greater or equal to BITS_PER_UNIT, it
     is not a reason to think this is a bit-field access.  */
  if (bitpos == 0
      && (bitsize >= BITS_PER_UNIT)
      && !(bitsize & (bitsize - 1)))
    bitsize = 0;

  if ((bitpos || bitsize) && (output_bitpos == NULL || output_bitsize == NULL))
    HSA_SORRY_ATV (EXPR_LOCATION (origref), "support for HSA does not "
		   "implement unhandled bit field reference such as %E", ref);

  if (output_bitsize != NULL && output_bitpos != NULL)
    {
      *output_bitsize = bitsize;
      *output_bitpos = bitpos;
    }

  return new hsa_op_address (symbol, reg, hwi_offset);
}

/* Generate HSA address for a function call argument of given TYPE.
   INDEX is used to generate corresponding name of the arguments.
   Special value -1 represents fact that result value is created.  */

static hsa_op_address *
gen_hsa_addr_for_arg (tree tree_type, int index)
{
  hsa_symbol *sym = hsa_allocp_symbols->allocate ();
  memset (sym, 0, sizeof (hsa_symbol));
  sym->segment = BRIG_SEGMENT_ARG;
  sym->linkage = BRIG_LINKAGE_ARG;

  sym->type = hsa_type_for_tree_type (tree_type, &sym->dim);

  if (index == -1) /* Function result.  */
    sym->name = "res";
  else /* Function call arguments.  */
    {
      sym->name = NULL;
      sym->name_number = index;
    }

  return new hsa_op_address (sym);
}

/* Generate HSA instructions that calculate address of VAL including all
   necessary conversions to flat addressing and place the result into DEST.
   Instructions are appended to HBB.  SSA_MAP maps gimple SSA names to HSA
   pseudo registers.  */

static void
gen_hsa_addr_insns (tree val, hsa_op_reg *dest, hsa_bb *hbb,
		    vec <hsa_op_reg_p> *ssa_map)
{
  /* Handle cases like tmp = NULL, where we just emit a move instruction
     to a register.  */
  if (TREE_CODE (val) == INTEGER_CST)
    {
      hsa_op_immed *c = new hsa_op_immed (val);
      hsa_insn_basic *insn = new hsa_insn_basic (2, BRIG_OPCODE_MOV,
						 dest->type, dest, c);
      hbb->append_insn (insn);
      return;
    }

  hsa_op_address *addr;

  gcc_assert (dest->type == hsa_get_segment_addr_type (BRIG_SEGMENT_FLAT));
  if (TREE_CODE (val) == ADDR_EXPR)
    val = TREE_OPERAND (val, 0);
  addr = gen_hsa_addr (val, hbb, ssa_map);
  hsa_insn_basic *insn = new hsa_insn_basic (2, BRIG_OPCODE_LDA);
  insn->set_op (1, addr);
  if (addr->symbol && addr->symbol->segment != BRIG_SEGMENT_GLOBAL)
    {
      /* LDA produces segment-relative address, we need to convert
	 it to the flat one.  */
      hsa_op_reg *tmp;
      tmp = new hsa_op_reg (hsa_get_segment_addr_type (addr->symbol->segment));
      hsa_insn_seg *seg;
      seg = new hsa_insn_seg (BRIG_OPCODE_STOF,
			      hsa_get_segment_addr_type (BRIG_SEGMENT_FLAT),
			      tmp->type, addr->symbol->segment, dest, tmp);

      insn->set_op (0, tmp);
      insn->type = tmp->type;
      hbb->append_insn (insn);
      hbb->append_insn (seg);
    }
  else
    {
      insn->set_op (0, dest);
      insn->type = hsa_get_segment_addr_type (BRIG_SEGMENT_FLAT);
      hbb->append_insn (insn);
    }
}

/* Return an HSA register or HSA immediate value operand corresponding to
   gimple operand OP.  SSA_MAP maps gimple SSA names to
   HSA pseudo registers.  */

static hsa_op_with_type *
hsa_reg_or_immed_for_gimple_op (tree op, hsa_bb *hbb,
				vec <hsa_op_reg_p> *ssa_map)
{
  hsa_op_reg *tmp;

  if (TREE_CODE (op) == SSA_NAME)
    tmp = hsa_reg_for_gimple_ssa (op, ssa_map);
  else if (!POINTER_TYPE_P (TREE_TYPE (op)))
    return new hsa_op_immed (op);
  else
    {
      tmp = new hsa_op_reg (hsa_get_segment_addr_type (BRIG_SEGMENT_FLAT));
      gen_hsa_addr_insns (op, tmp, hbb, ssa_map);
    }
  return tmp;
}

/* Create a simple movement instruction with register destination DEST and
   register or immediate source SRC and append it to the end of HBB.  */

void
hsa_build_append_simple_mov (hsa_op_reg *dest, hsa_op_base *src, hsa_bb *hbb)
{
  hsa_insn_basic *insn = new hsa_insn_basic (2, BRIG_OPCODE_MOV, dest->type,
					     dest, src);
  if (hsa_op_reg *sreg = dyn_cast <hsa_op_reg *> (src))
    gcc_assert (hsa_type_bit_size (dest->type)
		== hsa_type_bit_size (sreg->type));
  else
    gcc_assert (hsa_type_bit_size (dest->type)
		== hsa_type_bit_size (as_a <hsa_op_immed *> (src)->type));

  hbb->append_insn (insn);
}

/* Generate HSAIL instructions loading a bit field into register DEST.
   VALUE_REG is a register of a SSA name that is used in the bit field
   reference.  To identify a bit field BITPOS is offset to the loaded memory
   and BITSIZE is number of bits of the bit field.
   Add instructions to HBB.  */

static void
gen_hsa_insns_for_bitfield (hsa_op_reg *dest, hsa_op_reg *value_reg,
			    HOST_WIDE_INT bitsize, HOST_WIDE_INT bitpos,
			    hsa_bb *hbb)
{
  unsigned type_bitsize = hsa_type_bit_size (dest->type);
  unsigned left_shift = type_bitsize - (bitsize + bitpos);
  unsigned right_shift = left_shift + bitpos;

  if (left_shift)
    {
      hsa_op_reg *value_reg_2 = new hsa_op_reg (dest->type);
      hsa_op_immed *c = new hsa_op_immed (left_shift, BRIG_TYPE_U32);

      hsa_insn_basic *lshift = new hsa_insn_basic
	(3, BRIG_OPCODE_SHL, value_reg_2->type, value_reg_2, value_reg, c);

      hbb->append_insn (lshift);

      value_reg = value_reg_2;
    }

  if (right_shift)
    {
      hsa_op_reg *value_reg_2 = new hsa_op_reg (dest->type);
      hsa_op_immed *c = new hsa_op_immed (right_shift, BRIG_TYPE_U32);

      hsa_insn_basic *rshift = new hsa_insn_basic
	(3, BRIG_OPCODE_SHR, value_reg_2->type, value_reg_2, value_reg, c);

      hbb->append_insn (rshift);

      value_reg = value_reg_2;
    }

    hsa_insn_basic *assignment = new hsa_insn_basic
      (2, BRIG_OPCODE_MOV, dest->type, dest, value_reg);
    hbb->append_insn (assignment);
}


/* Generate HSAIL instructions loading a bit field into register DEST.  ADDR is
   prepared memory address which is used to load the bit field.  To identify
   a bit field BITPOS is offset to the loaded memory and BITSIZE is number
   of bits of the bit field.  Add instructions to HBB.  */

static void
gen_hsa_insns_for_bitfield_load (hsa_op_reg *dest, hsa_op_address *addr,
				HOST_WIDE_INT bitsize, HOST_WIDE_INT bitpos,
				hsa_bb *hbb)
{
  hsa_op_reg *value_reg = new hsa_op_reg (dest->type);
  hsa_insn_mem *mem = new hsa_insn_mem (BRIG_OPCODE_LD, dest->type, value_reg,
					addr);
  hbb->append_insn (mem);
  gen_hsa_insns_for_bitfield (dest, value_reg, bitsize, bitpos, hbb);
}

/* Generate HSAIL instructions loading something into register DEST.  RHS is
   tree representation of the loaded data, which are loaded as type TYPE.  Add
   instructions to HBB, use SSA_MAP for HSA SSA lookup.  */

static void
gen_hsa_insns_for_load (hsa_op_reg *dest, tree rhs, tree type, hsa_bb *hbb,
			vec <hsa_op_reg_p> *ssa_map)
{
  /* The destination SSA name will give us the type.  */
  if (TREE_CODE (rhs) == VIEW_CONVERT_EXPR)
    rhs = TREE_OPERAND (rhs, 0);

  if (TREE_CODE (rhs) == SSA_NAME)
    {
      hsa_op_reg *src = hsa_reg_for_gimple_ssa (rhs, ssa_map);
      hsa_build_append_simple_mov (dest, src, hbb);
    }
  else if (is_gimple_min_invariant (rhs)
	   || TREE_CODE (rhs) == ADDR_EXPR)
    {
      if (POINTER_TYPE_P (TREE_TYPE (rhs)))
	{
	  if (dest->type != hsa_get_segment_addr_type (BRIG_SEGMENT_FLAT))
	    {
	      HSA_SORRY_ATV (EXPR_LOCATION (rhs),
			     "support for HSA does not implement conversion "
			     "of %E to the requested non-pointer type.", rhs);
	      return;
	    }

	  gen_hsa_addr_insns (rhs, dest, hbb, ssa_map);
	}
      else if (TREE_CODE (rhs) == COMPLEX_CST)
	{
	  tree pack_type = TREE_TYPE (rhs);
	  hsa_op_immed *real_part = new hsa_op_immed (TREE_REALPART (rhs));
	  hsa_op_immed *imag_part = new hsa_op_immed (TREE_IMAGPART (rhs));

	  hsa_op_reg *real_part_reg = new hsa_op_reg
	    (hsa_type_for_scalar_tree_type (TREE_TYPE (type), false));
	  hsa_op_reg *imag_part_reg = new hsa_op_reg
	    (hsa_type_for_scalar_tree_type (TREE_TYPE (type), false));

	  hsa_build_append_simple_mov (real_part_reg, real_part, hbb);
	  hsa_build_append_simple_mov (imag_part_reg, imag_part, hbb);

	  hsa_insn_basic *insn = new hsa_insn_basic
	    (3, BRIG_OPCODE_COMBINE,
	     hsa_type_for_scalar_tree_type (pack_type, false), dest,
	     real_part_reg, imag_part_reg);
	  hbb->append_insn (insn);
	}
      else
	{
	  hsa_op_immed *imm = new hsa_op_immed (rhs);
	  hsa_build_append_simple_mov (dest, imm, hbb);
	}
    }
  else if (TREE_CODE (rhs) == REALPART_EXPR || TREE_CODE (rhs) == IMAGPART_EXPR)
    {
      tree pack_type = TREE_TYPE (TREE_OPERAND (rhs, 0));

      hsa_op_reg *packed_reg = new hsa_op_reg
	(hsa_type_for_scalar_tree_type (pack_type, false));

      gen_hsa_insns_for_load (packed_reg, TREE_OPERAND (rhs, 0), type, hbb,
			      ssa_map);

      hsa_op_reg *real_reg = new hsa_op_reg
	(hsa_type_for_scalar_tree_type (type, false));

      hsa_op_reg *imag_reg = new hsa_op_reg
	(hsa_type_for_scalar_tree_type (type, false));

      BrigKind16_t brig_type = packed_reg->type;
      hsa_insn_basic *expand = new hsa_insn_basic
	(3, BRIG_OPCODE_EXPAND, brig_type, real_reg, imag_reg, packed_reg);

      hbb->append_insn (expand);

      hsa_op_reg *source = TREE_CODE (rhs) == REALPART_EXPR ?
	real_reg : imag_reg;

      hsa_insn_basic *insn = new hsa_insn_basic (2, BRIG_OPCODE_MOV,
						 dest->type, dest, source);

      hbb->append_insn (insn);
    }
  else if (TREE_CODE (rhs) == BIT_FIELD_REF
	   && TREE_CODE (TREE_OPERAND (rhs, 0)) == SSA_NAME)
    {
      tree ssa_name = TREE_OPERAND (rhs, 0);
      HOST_WIDE_INT bitsize = tree_to_uhwi (TREE_OPERAND (rhs, 1));
      HOST_WIDE_INT bitpos = tree_to_uhwi (TREE_OPERAND (rhs, 2));

      hsa_op_reg *imm_value = hsa_reg_for_gimple_ssa (ssa_name, ssa_map);
      gen_hsa_insns_for_bitfield (dest, imm_value, bitsize, bitpos, hbb);
    }
  else if (DECL_P (rhs) || TREE_CODE (rhs) == MEM_REF
	   || TREE_CODE (rhs) == TARGET_MEM_REF
	   || handled_component_p (rhs))
    {
      HOST_WIDE_INT bitsize, bitpos;

      /* Load from memory.  */
      hsa_op_address *addr;
      addr = gen_hsa_addr (rhs, hbb, ssa_map, &bitsize, &bitpos);

      /* Handle load of a bit field.  */
      if (bitsize > 64)
	{
	  HSA_SORRY_AT (EXPR_LOCATION (rhs),
			"support for HSA does not implement load from a bit "
			"field bigger than 64 bits");
	  return;
	}

      if (bitsize || bitpos)
	gen_hsa_insns_for_bitfield_load (dest, addr, bitsize, bitpos, hbb);
      else
	{
	  BrigType16_t mtype;
	  /* Not dest->type, that's possibly extended.  */
	  mtype = mem_type_for_type (hsa_type_for_scalar_tree_type (type,
								    false));
	  hsa_insn_mem *mem = new hsa_insn_mem (BRIG_OPCODE_LD, mtype, dest,
						addr);
	  hbb->append_insn (mem);
	}
    }
  else
    HSA_SORRY_ATV
      (EXPR_LOCATION (rhs),
       "support for HSA does not implement loading of expression %E", rhs);
}

/* Return number of bits necessary for representation of a bit field,
   starting at BITPOS with size of BITSIZE.  */

static unsigned
get_bitfield_size (unsigned bitpos, unsigned bitsize)
{
  unsigned s = bitpos + bitsize;
  unsigned sizes[] = {8, 16, 32, 64};

  for (unsigned i = 0; i < 4; i++)
    if (s <= sizes[i])
      return sizes[i];

  gcc_unreachable ();
  return 0;
}

/* Generate HSAIL instructions storing into memory.  LHS is the destination of
   the store, SRC is the source operand.  Add instructions to HBB, use SSA_MAP
   for HSA SSA lookup.  */

static void
gen_hsa_insns_for_store (tree lhs, hsa_op_base *src, hsa_bb *hbb,
			 vec <hsa_op_reg_p> *ssa_map)
{
  HOST_WIDE_INT bitsize = 0, bitpos = 0;
  BrigType16_t mtype;
  mtype = mem_type_for_type (hsa_type_for_scalar_tree_type (TREE_TYPE (lhs),
							    false));
  hsa_op_address *addr;
  addr = gen_hsa_addr (lhs, hbb, ssa_map, &bitsize, &bitpos);

  /* Handle store to a bit field.  */
  if (bitsize > 64)
    {
      HSA_SORRY_AT (EXPR_LOCATION (lhs),
		    "support for HSA does not implement store to a bit field "
		    "bigger than 64 bits");
      return;
    }

  unsigned type_bitsize = get_bitfield_size (bitpos, bitsize);

  /* HSAIL does not support MOV insn with 16-bits integers.  */
  if (type_bitsize < 32)
    type_bitsize = 32;

  if (bitpos || (bitsize && type_bitsize != bitsize))
    {
      unsigned HOST_WIDE_INT mask = 0;
      BrigType16_t mem_type = get_integer_type_by_bytes
	(type_bitsize / BITS_PER_UNIT, !TYPE_UNSIGNED (TREE_TYPE (lhs)));

      for (unsigned i = 0; i < type_bitsize; i++)
	if (i < bitpos || i >= bitpos + bitsize)
	  mask |= ((unsigned HOST_WIDE_INT)1 << i);

      hsa_op_reg *value_reg = new hsa_op_reg (mem_type);

      /* Load value from memory.  */
      hsa_insn_mem *mem = new hsa_insn_mem (BRIG_OPCODE_LD, mem_type,
					    value_reg, addr);
      hbb->append_insn (mem);

      /* AND the loaded value with prepared mask.  */
      hsa_op_reg *cleared_reg = new hsa_op_reg (mem_type);

      hsa_op_immed *c = new hsa_op_immed
	(mask, get_integer_type_by_bytes (type_bitsize / BITS_PER_UNIT, false));

      hsa_insn_basic *clearing = new hsa_insn_basic
	(3, BRIG_OPCODE_AND, mem_type, cleared_reg, value_reg, c);
      hbb->append_insn (clearing);

      /* Shift to left a value that is going to be stored.  */
      hsa_op_reg *new_value_reg = new hsa_op_reg (mem_type);

      hsa_insn_basic *basic = new hsa_insn_basic (2, BRIG_OPCODE_MOV, mem_type,
						  new_value_reg, src);
      hbb->append_insn (basic);

      if (bitpos)
	{
	  hsa_op_reg *shifted_value_reg = new hsa_op_reg (mem_type);
	  c = new hsa_op_immed (bitpos, BRIG_TYPE_U32);

	  hsa_insn_basic *basic = new hsa_insn_basic
	    (3, BRIG_OPCODE_SHL, mem_type, shifted_value_reg, new_value_reg, c);
	  hbb->append_insn (basic);

	  new_value_reg = shifted_value_reg;
	}

      /* OR the prepared value with prepared chunk loaded from memory.  */
      hsa_op_reg *prepared_reg= new hsa_op_reg (mem_type);
      basic = new hsa_insn_basic (3, BRIG_OPCODE_OR, mem_type, prepared_reg,
				  new_value_reg, cleared_reg);
      hbb->append_insn (basic);

      src = prepared_reg;
      mtype = mem_type;
    }

  hsa_insn_mem *mem = new hsa_insn_mem (BRIG_OPCODE_ST, mtype, src, addr);

  /* XXX The HSAIL disasm has another constraint: if the source
     is an immediate then it must match the destination type.  If
     it's a register the low bits will be used for sub-word stores.
     We're always allocating new operands so we can modify the above
     in place.  */
  if (hsa_op_immed *imm = dyn_cast <hsa_op_immed *> (src))
    {
      if ((imm->type & BRIG_TYPE_PACK_MASK) == BRIG_TYPE_PACK_NONE)
	imm->type = mem->type;
      else
	{
	  /* ...and all vector immediates apparently need to be vectors of
	     unsigned bytes. */
	  unsigned bs = hsa_type_bit_size (imm->type);
	  gcc_assert (bs == hsa_type_bit_size (mem->type));
	  switch (bs)
	    {
	    case 32:
	      imm->type = BRIG_TYPE_U8X4;
	      break;
	    case 64:
	      imm->type = BRIG_TYPE_U8X8;
	      break;
	    case 128:
	      imm->type = BRIG_TYPE_U8X16;
	      break;
	    default:
	      gcc_unreachable ();
	    }
	}
    }

  hbb->append_insn (mem);
}

/* Generate memory copy instructions that are going to be used
   for copying a HSA symbol SRC_SYMBOL (or SRC_REG) to TARGET memory,
   represented by pointer in a register.  */

static void
gen_hsa_memory_copy (hsa_bb *hbb, hsa_op_address *target, hsa_op_address *src,
		     unsigned size)
{
  hsa_op_address *addr;
  hsa_insn_mem *mem;

  unsigned offset = 0;

  while (size)
    {
      unsigned s;
      if (size >= 8)
	s = 8;
      else if (size >= 4)
	s = 4;
      else if (size >= 2)
	s = 2;
      else
	s = 1;

      BrigType16_t t = get_integer_type_by_bytes (s, false);

      hsa_op_reg *tmp = new hsa_op_reg (t);
      addr = new hsa_op_address (src->symbol, src->reg,
				 src->imm_offset + offset);
      mem = new hsa_insn_mem (BRIG_OPCODE_LD, t, tmp, addr);
      hbb->append_insn (mem);

      addr = new hsa_op_address (target->symbol, target->reg,
				 target->imm_offset + offset);
      mem = new hsa_insn_mem (BRIG_OPCODE_ST, t, tmp, addr);
      hbb->append_insn (mem);
      offset += s;
      size -= s;
    }
}

/* Create a memset mask that is created by copying a CONSTANT byte value
   to an integer of BYTE_SIZE bytes.  */

static unsigned HOST_WIDE_INT
build_memset_value (unsigned HOST_WIDE_INT constant, unsigned byte_size)
{
  HOST_WIDE_INT v = constant;

  for (unsigned i = 1; i < byte_size; i++)
    v |= constant << (8 * i);

  return v;
}

/* Generate memory set instructions that are going to be used
   for setting a CONSTANT byte value to TARGET memory of SIZE bytes.  */

static void
gen_hsa_memory_set (hsa_bb *hbb, hsa_op_address *target,
		    unsigned HOST_WIDE_INT constant,
		    unsigned size)
{
  hsa_op_address *addr;
  hsa_insn_mem *mem;

  unsigned offset = 0;

  while (size)
    {
      unsigned s;
      if (size >= 8)
	s = 8;
      else if (size >= 4)
	s = 4;
      else if (size >= 2)
	s = 2;
      else
	s = 1;

      addr = new hsa_op_address (target->symbol, target->reg,
				 target->imm_offset + offset);

      BrigType16_t t = get_integer_type_by_bytes (s, false);
      HOST_WIDE_INT c = build_memset_value (constant, s);

      mem = new hsa_insn_mem (BRIG_OPCODE_ST, t, new hsa_op_immed (c, t),
			      addr);
      hbb->append_insn (mem);
      offset += s;
      size -= s;
    }
}

/* Generate HSAIL instructions for a single assignment
   of an empty constructor to an ADDR_LHS.  Constructor is passed as a
   tree RHS and all instructions are appended to HBB.  */

void
gen_hsa_ctor_assignment (hsa_op_address *addr_lhs, tree rhs, hsa_bb *hbb)
{
  if (vec_safe_length (CONSTRUCTOR_ELTS (rhs)))
    {
      HSA_SORRY_AT (EXPR_LOCATION (rhs),
		    "support for HSA does not implement load from constructor");
      return;
    }

  unsigned size = tree_to_uhwi (TYPE_SIZE_UNIT (TREE_TYPE (rhs)));
  gen_hsa_memory_set (hbb, addr_lhs, 0, size);
}

/* Generate HSA instructions for a single assignment of RHS to LHS.
   HBB is the basic block they will be appended to.  SSA_MAP maps gimple
   SSA names to HSA pseudo registers.  */

static void
gen_hsa_insns_for_single_assignment (tree lhs, tree rhs, hsa_bb *hbb,
				     vec <hsa_op_reg_p> *ssa_map)
{
  if (TREE_CODE (lhs) == SSA_NAME)
    {
      hsa_op_reg *dest = hsa_reg_for_gimple_ssa (lhs, ssa_map);
      if (hsa_seen_error ())
	return;

      gen_hsa_insns_for_load (dest, rhs, TREE_TYPE (lhs), hbb, ssa_map);
    }
  else if (TREE_CODE (rhs) == SSA_NAME
	   || (is_gimple_min_invariant (rhs) && TREE_CODE (rhs) != STRING_CST))
    {
      /* Store to memory.  */
      hsa_op_base *src = hsa_reg_or_immed_for_gimple_op (rhs, hbb, ssa_map);
      if (hsa_seen_error ())
	return;

      gen_hsa_insns_for_store (lhs, src, hbb, ssa_map);
    }
  else
    {
      hsa_op_address *addr_lhs = gen_hsa_addr (lhs, hbb, ssa_map);

      if (TREE_CODE (rhs) == CONSTRUCTOR)
	gen_hsa_ctor_assignment (addr_lhs, rhs, hbb);
      else
	{
	  hsa_op_address *addr_rhs = gen_hsa_addr (rhs, hbb, ssa_map);

	  unsigned size = tree_to_uhwi (TYPE_SIZE_UNIT (TREE_TYPE (rhs)));
	  gen_hsa_memory_copy (hbb, addr_lhs, addr_rhs, size);
	}
    }
}

/* Prepend before INSN a load from spill symbol of SPILL_REG.  Return the
   register into which we loaded.  If this required another register to convert
   from a B1 type, return it in *PTMP2, otherwise store NULL into it.  We
   assume we are out of SSA so the returned register does not have its
   definition set.  */

hsa_op_reg *
hsa_spill_in (hsa_insn_basic *insn, hsa_op_reg *spill_reg, hsa_op_reg **ptmp2)
{
  hsa_symbol *spill_sym = spill_reg->spill_sym;
  hsa_op_reg *reg = new hsa_op_reg (spill_sym->type);
  hsa_op_address *addr = new hsa_op_address (spill_sym);

  hsa_insn_mem *mem = new hsa_insn_mem (BRIG_OPCODE_LD, spill_sym->type,
					reg, addr);
  hsa_insert_insn_before (mem, insn);

  *ptmp2 = NULL;
  if (spill_reg->type == BRIG_TYPE_B1)
    {
      hsa_insn_basic *cvtinsn;
      *ptmp2 = reg;
      reg = new hsa_op_reg (spill_reg->type);

      cvtinsn = new hsa_insn_basic (2, BRIG_OPCODE_CVT, reg->type, reg, *ptmp2);

      hsa_insert_insn_before (cvtinsn, insn);
    }
  return reg;
}

/* Append after INSN a store to spill symbol of SPILL_REG.  Return the register
   from which we stored.  If this required another register to convert to a B1
   type, return it in *PTMP2, otherwise store NULL into it.  We assume we are
   out of SSA so the returned register does not have its use updated.  */

hsa_op_reg *
hsa_spill_out (hsa_insn_basic *insn, hsa_op_reg *spill_reg, hsa_op_reg **ptmp2)
{
  hsa_symbol *spill_sym = spill_reg->spill_sym;
  hsa_op_reg *reg = new hsa_op_reg (spill_sym->type);
  hsa_op_address *addr = new hsa_op_address (spill_sym);
  hsa_op_reg *returnreg;

  *ptmp2 = NULL;
  returnreg = reg;
  if (spill_reg->type == BRIG_TYPE_B1)
    {
      hsa_insn_basic *cvtinsn;
      *ptmp2 = new hsa_op_reg (spill_sym->type);
      reg->type = spill_reg->type;

      cvtinsn = new hsa_insn_basic (2, BRIG_OPCODE_CVT, spill_sym->type, *ptmp2,
				    returnreg);

      hsa_append_insn_after (cvtinsn, insn);
      insn = cvtinsn;
      reg = *ptmp2;
    }

  hsa_insn_mem *mem = new hsa_insn_mem (BRIG_OPCODE_ST, spill_sym->type, reg,
					addr);
  hsa_append_insn_after (mem, insn);
  return returnreg;
}

/* Generate a comparison instruction that will compare LHS and RHS with
   comparison specified by CODE and put result into register DEST.  DEST has to
   have its type set already but must not have its definition set yet.
   Generated instructions will be added to HBB, SSA_MAP maps gimple SSA names
   to HSA pseudo registers.  */

static void
gen_hsa_cmp_insn_from_gimple (enum tree_code code, tree lhs, tree rhs,
			      hsa_op_reg *dest, hsa_bb *hbb,
			      vec <hsa_op_reg_p> *ssa_map)
{
  BrigCompareOperation8_t compare;

  switch (code)
    {
    case LT_EXPR:
      compare = BRIG_COMPARE_LT;
      break;
    case LE_EXPR:
      compare = BRIG_COMPARE_LE;
      break;
    case GT_EXPR:
      compare = BRIG_COMPARE_GT;
      break;
    case GE_EXPR:
      compare = BRIG_COMPARE_GE;
      break;
    case EQ_EXPR:
      compare = BRIG_COMPARE_EQ;
      break;
    case NE_EXPR:
      compare = BRIG_COMPARE_NE;
      break;
    case UNORDERED_EXPR:
      compare = BRIG_COMPARE_NAN;
      break;
    case ORDERED_EXPR:
      compare = BRIG_COMPARE_NUM;
      break;
    case UNLT_EXPR:
      compare = BRIG_COMPARE_LTU;
      break;
    case UNLE_EXPR:
      compare = BRIG_COMPARE_LEU;
      break;
    case UNGT_EXPR:
      compare = BRIG_COMPARE_GTU;
      break;
    case UNGE_EXPR:
      compare = BRIG_COMPARE_GEU;
      break;
    case UNEQ_EXPR:
      compare = BRIG_COMPARE_EQU;
      break;
    case LTGT_EXPR:
      compare = BRIG_COMPARE_NEU;
      break;

    default:
      HSA_SORRY_ATV (EXPR_LOCATION (lhs),
		     "support for HSA does not implement comparison tree "
		     "code %s\n", get_tree_code_name (code));
      return;
    }

  hsa_insn_cmp *cmp = new hsa_insn_cmp (compare, dest->type);
  cmp->set_op (0, dest);
  cmp->set_op (1, hsa_reg_or_immed_for_gimple_op (lhs, hbb, ssa_map));
  cmp->set_op (2, hsa_reg_or_immed_for_gimple_op (rhs, hbb, ssa_map));
  hbb->append_insn (cmp);
}

/* Generate an unary instruction with OPCODE and append it to a basic block
   HBB.  The instruction uses DEST as a destination and OP1
   as a single operand.  */

static void
gen_hsa_unary_operation (int opcode, hsa_op_reg *dest,
			 hsa_op_with_type *op1, hsa_bb *hbb)
{
  gcc_checking_assert (dest);
  hsa_insn_basic *insn = new hsa_insn_basic (2, opcode, dest->type, dest, op1);

  if (opcode == BRIG_OPCODE_ABS || opcode == BRIG_OPCODE_NEG)
    {
      /* ABS and NEG only exist in _s form :-/  */
      if (insn->type == BRIG_TYPE_U32)
	insn->type = BRIG_TYPE_S32;
      else if (insn->type == BRIG_TYPE_U64)
	insn->type = BRIG_TYPE_S64;
    }
  else if (opcode == BRIG_OPCODE_MOV && hsa_needs_cvt (dest->type, op1->type))
    insn->opcode = BRIG_OPCODE_CVT;

  hbb->append_insn (insn);
}

/* Generate a binary instruction with OPCODE and append it to a basic block
   HBB.  The instruction uses DEST as a destination and operands OP1
   and OP2.  */

static void
gen_hsa_binary_operation (int opcode, hsa_op_reg *dest,
			  hsa_op_base *op1, hsa_op_base *op2, hsa_bb *hbb)
{
  gcc_checking_assert (dest);

  if ((opcode == BRIG_OPCODE_SHL || opcode == BRIG_OPCODE_SHR)
      && is_a <hsa_op_immed *> (op2))
    {
      hsa_op_immed *i = dyn_cast <hsa_op_immed *> (op2);
      i->set_type (BRIG_TYPE_U32);
    }

  hsa_insn_basic *insn = new hsa_insn_basic (3, opcode, dest->type, dest,
					     op1, op2);
  hbb->append_insn (insn);
}

/* Generate HSA instructions for a single assignment.  HBB is the basic block
   they will be appended to.  SSA_MAP maps gimple SSA names to HSA pseudo
   registers.  */

static void
gen_hsa_insns_for_operation_assignment (gimple *assign, hsa_bb *hbb,
					vec <hsa_op_reg_p> *ssa_map)
{
  tree_code code = gimple_assign_rhs_code (assign);
  gimple_rhs_class rhs_class = get_gimple_rhs_class (gimple_expr_code (assign));

  tree lhs = gimple_assign_lhs (assign);
  tree rhs1 = gimple_assign_rhs1 (assign);
  tree rhs2 = gimple_assign_rhs2 (assign);
  tree rhs3 = gimple_assign_rhs3 (assign);

  int opcode;

  switch (code)
    {
    CASE_CONVERT:
    case FLOAT_EXPR:
      /* The opcode is changed to BRIG_OPCODE_CVT if BRIG types
	 needs a conversion.  */
      opcode = BRIG_OPCODE_MOV;
      break;

    case PLUS_EXPR:
    case POINTER_PLUS_EXPR:
      opcode = BRIG_OPCODE_ADD;
      break;
    case MINUS_EXPR:
      opcode = BRIG_OPCODE_SUB;
      break;
    case MULT_EXPR:
      opcode = BRIG_OPCODE_MUL;
      break;
    case MULT_HIGHPART_EXPR:
      opcode = BRIG_OPCODE_MULHI;
      break;
    case RDIV_EXPR:
    case TRUNC_DIV_EXPR:
    case EXACT_DIV_EXPR:
      opcode = BRIG_OPCODE_DIV;
      break;
    case CEIL_DIV_EXPR:
    case FLOOR_DIV_EXPR:
    case ROUND_DIV_EXPR:
      HSA_SORRY_AT (gimple_location (assign),
		    "support for HSA does not implement CEIL_DIV_EXPR, "
		    "FLOOR_DIV_EXPR or ROUND_DIV_EXPR");
      return;
    case TRUNC_MOD_EXPR:
      opcode = BRIG_OPCODE_REM;
      break;
    case CEIL_MOD_EXPR:
    case FLOOR_MOD_EXPR:
    case ROUND_MOD_EXPR:
      HSA_SORRY_AT (gimple_location (assign),
		    "support for HSA does not implement CEIL_MOD_EXPR, "
		    "FLOOR_MOD_EXPR or ROUND_MOD_EXPR");
      return;
    case NEGATE_EXPR:
      opcode = BRIG_OPCODE_NEG;
      break;
    case MIN_EXPR:
      opcode = BRIG_OPCODE_MIN;
      break;
    case MAX_EXPR:
      opcode = BRIG_OPCODE_MAX;
      break;
    case ABS_EXPR:
      opcode = BRIG_OPCODE_ABS;
      break;
    case LSHIFT_EXPR:
      opcode = BRIG_OPCODE_SHL;
      break;
    case RSHIFT_EXPR:
      opcode = BRIG_OPCODE_SHR;
      break;
    case LROTATE_EXPR:
    case RROTATE_EXPR:
      {
	hsa_insn_basic *insn = NULL;
	int code1 = code == LROTATE_EXPR ? BRIG_OPCODE_SHL : BRIG_OPCODE_SHR;
	int code2 = code != LROTATE_EXPR ? BRIG_OPCODE_SHL : BRIG_OPCODE_SHR;
	BrigType16_t btype = hsa_type_for_scalar_tree_type (TREE_TYPE (lhs),
							    true);

	hsa_op_with_type *src = hsa_reg_or_immed_for_gimple_op (rhs1, hbb,
								ssa_map);
	hsa_op_reg *op1 = new hsa_op_reg (btype);
	hsa_op_reg *op2 = new hsa_op_reg (btype);
	hsa_op_with_type *shift1 = hsa_reg_or_immed_for_gimple_op
	  (rhs2, hbb, ssa_map);

	tree type = TREE_TYPE (rhs2);
	unsigned HOST_WIDE_INT bitsize = TREE_INT_CST_LOW (TYPE_SIZE (type));

	hsa_op_with_type *shift2 = NULL;
	if (TREE_CODE (rhs2) == INTEGER_CST)
	  shift2 = new hsa_op_immed (bitsize - tree_to_uhwi (rhs2),
				     BRIG_TYPE_U32);
	else if (TREE_CODE (rhs2) == SSA_NAME)
	  {
	    hsa_op_reg *s = hsa_reg_for_gimple_ssa (rhs2, ssa_map);
	    hsa_op_reg *d = new hsa_op_reg (s->type);
	    hsa_op_immed *size_imm = new hsa_op_immed (bitsize, BRIG_TYPE_U32);

	    insn = new hsa_insn_basic (3, BRIG_OPCODE_SUB, d->type,
				       d, s, size_imm);
	    hbb->append_insn (insn);

	    shift2 = d;
	  }
	else
	  gcc_unreachable ();

	hsa_op_reg *dest = hsa_reg_for_gimple_ssa (lhs, ssa_map);
	gen_hsa_binary_operation (code1, op1, src, shift1, hbb);
	gen_hsa_binary_operation (code2, op2, src, shift2, hbb);
	gen_hsa_binary_operation (BRIG_OPCODE_OR, dest, op1, op2, hbb);

	return;
      }
    case BIT_IOR_EXPR:
      opcode = BRIG_OPCODE_OR;
      break;
    case BIT_XOR_EXPR:
      opcode = BRIG_OPCODE_XOR;
      break;
    case BIT_AND_EXPR:
      opcode = BRIG_OPCODE_AND;
      break;
    case BIT_NOT_EXPR:
      opcode = BRIG_OPCODE_NOT;
      break;
    case FIX_TRUNC_EXPR:
      {
	hsa_op_reg *dest = hsa_reg_for_gimple_ssa (lhs, ssa_map);
	hsa_op_with_type *v = hsa_reg_or_immed_for_gimple_op (rhs1, hbb,
							      ssa_map);

	if (hsa_needs_cvt (dest->type, v->type))
	  {
	    hsa_op_reg *tmp = new hsa_op_reg (v->type);

	    hsa_insn_basic *insn = new hsa_insn_basic (2, BRIG_OPCODE_TRUNC,
						       tmp->type, tmp, v);
	    hbb->append_insn (insn);

	    hsa_insn_basic *cvtinsn = new hsa_insn_basic
	      (2, BRIG_OPCODE_CVT, dest->type, dest, tmp);
	    hbb->append_insn (cvtinsn);
	  }
	else
	  {
	    hsa_insn_basic *insn = new hsa_insn_basic (2, BRIG_OPCODE_TRUNC,
						       dest->type, dest, v);
	    hbb->append_insn (insn);
	  }

	return;
      }
      opcode = BRIG_OPCODE_TRUNC;
      break;

    case LT_EXPR:
    case LE_EXPR:
    case GT_EXPR:
    case GE_EXPR:
    case EQ_EXPR:
    case NE_EXPR:
    case UNORDERED_EXPR:
    case ORDERED_EXPR:
    case UNLT_EXPR:
    case UNLE_EXPR:
    case UNGT_EXPR:
    case UNGE_EXPR:
    case UNEQ_EXPR:
    case LTGT_EXPR:
      {
	hsa_op_reg *dest = hsa_reg_for_gimple_ssa (gimple_assign_lhs (assign),
						   ssa_map);

	gen_hsa_cmp_insn_from_gimple (code, rhs1, rhs2, dest, hbb, ssa_map);
	return;
      }
    case COND_EXPR:
      {
	hsa_op_reg *dest = hsa_reg_for_gimple_ssa (gimple_assign_lhs (assign),
						   ssa_map);
	hsa_op_with_type *ctrl = NULL;
	tree cond = rhs1;

	if (CONSTANT_CLASS_P (cond) || TREE_CODE (cond) == SSA_NAME)
	  ctrl = hsa_reg_or_immed_for_gimple_op (cond, hbb, ssa_map);
	else
	  {
	    hsa_op_reg *r = new hsa_op_reg (BRIG_TYPE_B1);

	    gen_hsa_cmp_insn_from_gimple (TREE_CODE (cond),
				  TREE_OPERAND (cond, 0),
				  TREE_OPERAND (cond, 1),
				  r, hbb, ssa_map);

	    ctrl = r;
	  }

	hsa_op_with_type *rhs2_reg = hsa_reg_or_immed_for_gimple_op
	  (rhs2, hbb, ssa_map);
	hsa_op_with_type *rhs3_reg = hsa_reg_or_immed_for_gimple_op
	  (rhs3, hbb, ssa_map);

	BrigType16_t btype = hsa_bittype_for_type (dest->type);
	hsa_op_reg *tmp = new hsa_op_reg (btype);

	rhs2_reg->type = btype;
	rhs3_reg->type = btype;

	hsa_insn_basic *insn = new hsa_insn_basic
	  (4, BRIG_OPCODE_CMOV, tmp->type, tmp, ctrl, rhs2_reg, rhs3_reg);

	hbb->append_insn (insn);

	/* As operands of a CMOV insn must be Bx types, we have to emit
	   a conversion insn.  */
	hsa_insn_basic *mov = new hsa_insn_basic (2, BRIG_OPCODE_MOV,
						  dest->type, dest, tmp);
	hbb->append_insn (mov);

	return;
      }
    case COMPLEX_EXPR:
      {
	hsa_op_reg *dest = hsa_reg_for_gimple_ssa (gimple_assign_lhs (assign),
						   ssa_map);
	hsa_op_base *rhs1_reg = hsa_reg_or_immed_for_gimple_op
	  (rhs1, hbb, ssa_map);
	hsa_op_base *rhs2_reg = hsa_reg_or_immed_for_gimple_op
	  (rhs2, hbb, ssa_map);

	hsa_insn_basic *insn = new hsa_insn_basic (3, BRIG_OPCODE_COMBINE,
						   dest->type, dest, rhs1_reg,
						   rhs2_reg);

	hbb->append_insn (insn);

	return;
      }
    default:
      /* Implement others as we come across them.  */
      HSA_SORRY_ATV (gimple_location (assign),
		     "support for HSA does not implement operation %s",
		     get_tree_code_name (code));
      return;
    }


  hsa_op_reg *dest = hsa_reg_for_gimple_ssa (gimple_assign_lhs (assign),
					     ssa_map);

  /* FIXME: Allocate an instruction with modifiers if appropriate.  */
  hsa_op_with_type *op1 = hsa_reg_or_immed_for_gimple_op (rhs1, hbb, ssa_map);
  hsa_op_with_type *op2 = rhs2 != NULL_TREE ?
    hsa_reg_or_immed_for_gimple_op (rhs2, hbb, ssa_map) : NULL;

  switch (rhs_class)
    {
    case GIMPLE_TERNARY_RHS:
      gcc_unreachable ();
      return;

      /* Fall through */
    case GIMPLE_BINARY_RHS:
      gen_hsa_binary_operation (opcode, dest, op1, op2, hbb);
      break;
      /* Fall through */
    case GIMPLE_UNARY_RHS:
      gen_hsa_unary_operation (opcode, dest, op1, hbb);
      break;
    default:
      gcc_unreachable ();
    }
}

/* Generate HSA instructions for a given gimple condition statement COND.
   Instructions will be appended to HBB, which also needs to be the
   corresponding structure to the basic_block of COND.  SSA_MAP maps gimple SSA
   names to HSA pseudo registers.  */

static void
gen_hsa_insns_for_cond_stmt (gimple *cond, hsa_bb *hbb,
			     vec <hsa_op_reg_p> *ssa_map)
{
  hsa_op_reg *ctrl = new hsa_op_reg (BRIG_TYPE_B1);
  hsa_insn_br *cbr;

  gen_hsa_cmp_insn_from_gimple (gimple_cond_code (cond),
				gimple_cond_lhs (cond),
				gimple_cond_rhs (cond),
				ctrl, hbb, ssa_map);

  cbr = new hsa_insn_br (ctrl);
  hbb->append_insn (cbr);
}

/* Maximum number of elements in a jump table for an HSA SBR instruction.  */

#define HSA_MAXIMUM_SBR_LABELS	16

/* Return lowest value of a switch S that is handled in a non-default
   label.  */

static tree
get_switch_low (gswitch *s)
{
  unsigned labels = gimple_switch_num_labels (s);
  gcc_checking_assert (labels >= 1);

  return CASE_LOW (gimple_switch_label (s, 1));
}

/* Return highest value of a switch S that is handled in a non-default
   label.  */

static tree
get_switch_high (gswitch *s)
{
  unsigned labels = gimple_switch_num_labels (s);

  /* Compare last label to maximum number of labels.  */
  tree label = gimple_switch_label (s, labels - 1);
  tree low = CASE_LOW (label);
  tree high = CASE_HIGH (label);

  return high != NULL_TREE ? high : low;
}

static tree
get_switch_size (gswitch *s)
{
  return int_const_binop (MINUS_EXPR, get_switch_high (s), get_switch_low (s));
}

/* Generate HSA instructions for a given gimple switch.
   Instructions will be appended to HBB and SSA_MAP maps gimple SSA
   names to HSA pseudo registers.  */

static void
gen_hsa_insns_for_switch_stmt (gswitch *s, hsa_bb *hbb,
			       vec <hsa_op_reg_p> *ssa_map)
{
  function *func = DECL_STRUCT_FUNCTION (current_function_decl);
  tree index_tree = gimple_switch_index (s);
  tree lowest = get_switch_low (s);

  hsa_op_reg *index = hsa_reg_for_gimple_ssa (index_tree, ssa_map);
  hsa_op_reg *sub_index = new hsa_op_reg (index->type);
  hbb->append_insn (new hsa_insn_basic (3, BRIG_OPCODE_SUB, sub_index->type,
					sub_index, index,
					new hsa_op_immed (lowest)));

  if (hsa_needs_cvt (BRIG_TYPE_U64, sub_index->type))
    {
      hsa_op_reg *sub_index_cvt = new hsa_op_reg (BRIG_TYPE_U64);
      hbb->append_insn (new hsa_insn_basic (2, BRIG_OPCODE_CVT,
					    sub_index_cvt->type,
					    sub_index_cvt, sub_index));

      sub_index = sub_index_cvt;
    }

  unsigned labels = gimple_switch_num_labels (s);
  unsigned HOST_WIDE_INT size = tree_to_uhwi (get_switch_size (s));

  hsa_insn_sbr *sbr = new hsa_insn_sbr (sub_index, size + 1);
  tree default_label = gimple_switch_default_label (s);
  basic_block default_label_bb = label_to_block_fn
    (func, CASE_LABEL (default_label));

  sbr->default_bb = default_label_bb;

  /* Prepare array with default label destination.  */
  for (unsigned HOST_WIDE_INT i = 0; i <= size; i++)
    sbr->jump_table.safe_push (default_label_bb);

  /* Iterate all labels and fill up the jump table.  */
  for (unsigned i = 1; i < labels; i++)
    {
      tree label = gimple_switch_label (s, i);
      basic_block bb = label_to_block_fn (func, CASE_LABEL (label));

      unsigned HOST_WIDE_INT sub_low = tree_to_uhwi
	(int_const_binop (MINUS_EXPR, CASE_LOW (label), lowest));

      unsigned HOST_WIDE_INT sub_high = sub_low;
      tree high = CASE_HIGH (label);
      if (high != NULL)
	sub_high = tree_to_uhwi (int_const_binop (MINUS_EXPR, high, lowest));

      for (unsigned HOST_WIDE_INT j = sub_low; j <= sub_high; j++)
	sbr->jump_table[j] = bb;
    }

  hbb->append_insn (sbr);
}

/* Verify that the function DECL can be handled by HSA.  */

static void
verify_function_arguments (tree decl)
{
  if (DECL_STATIC_CHAIN (decl))
    {
      HSA_SORRY_ATV (EXPR_LOCATION (decl),
		     "HSA does not support nested functions: %D", decl);
      return;
    }
  else if (!TYPE_ARG_TYPES (TREE_TYPE (decl)))
    {
      HSA_SORRY_ATV (EXPR_LOCATION (decl),
		     "HSA does not support functions with variadic arguments "
		     "(or unknown return type): %D", decl);
      return;
    }
}

/* Generate HSA instructions for a direct call instruction.
   Instructions will be appended to HBB, which also needs to be the
   corresponding structure to the basic_block of STMT. SSA_MAP maps gimple SSA
   names to HSA pseudo registers.  */

static void
gen_hsa_insns_for_direct_call (gimple *stmt, hsa_bb *hbb,
			       vec <hsa_op_reg_p> *ssa_map)
{
  tree decl = gimple_call_fndecl (stmt);
  verify_function_arguments (decl);
  if (hsa_seen_error ())
    return;

  hsa_insn_call *call_insn = new hsa_insn_call (decl);
  hsa_cfun->called_functions.safe_push (call_insn->called_function);

  /* Argument block start.  */
  hsa_insn_arg_block *arg_start = new hsa_insn_arg_block
    (BRIG_KIND_DIRECTIVE_ARG_BLOCK_START, call_insn);
  hbb->append_insn (arg_start);

  /* Preparation of arguments that will be passed to function.  */
  const unsigned args = gimple_call_num_args (stmt);
  for (unsigned i = 0; i < args; ++i)
    {
      tree parm = gimple_call_arg (stmt, (int)i);

      if (AGGREGATE_TYPE_P (TREE_TYPE (parm)))
	{
	  HSA_SORRY_AT (gimple_location (stmt),
			"support for HSA does not "
			"implement an aggregate argument in a function call");
	  return;
	}

      BrigType16_t mtype = mem_type_for_type (hsa_type_for_scalar_tree_type
					      (TREE_TYPE (parm), false));
      hsa_op_address *addr = gen_hsa_addr_for_arg (TREE_TYPE (parm), i);
      hsa_op_base *src = hsa_reg_or_immed_for_gimple_op (parm, hbb, ssa_map);
      hsa_insn_mem *mem = new hsa_insn_mem (BRIG_OPCODE_ST, mtype, src, addr);

      call_insn->input_args.safe_push (addr->symbol);
      hbb->append_insn (mem);

      call_insn->args_symbols.safe_push (addr->symbol);
    }

  call_insn->args_code_list = new hsa_op_code_list (args);
  hbb->append_insn (call_insn);

  tree result_type = TREE_TYPE (TREE_TYPE (decl));

  tree result = gimple_call_lhs (stmt);
  hsa_insn_mem *result_insn = NULL;
  if (!VOID_TYPE_P (result_type))
    {
      if (AGGREGATE_TYPE_P (result_type))
	{
	  HSA_SORRY_ATV (gimple_location (stmt),
			 "support for HSA does not implement returning a value "
			 "which is of an aggregate type %T", result_type);
	  return;
	}

      hsa_op_address *addr = gen_hsa_addr_for_arg (result_type, -1);

      /* Even if result of a function call is unused, we have to emit
	 declaration for the result.  */
      if (result)
	{
	  BrigType16_t mtype = mem_type_for_type
	    (hsa_type_for_scalar_tree_type (TREE_TYPE (result), false));
	  hsa_op_reg *dst = hsa_reg_for_gimple_ssa (result, ssa_map);

	  result_insn = new hsa_insn_mem (BRIG_OPCODE_LD, mtype, dst, addr);

	  hbb->append_insn (result_insn);
	}

      call_insn->output_arg = addr->symbol;
      call_insn->result_symbol = addr->symbol;
      call_insn->result_code_list = new hsa_op_code_list (1);
    }
  else
    {
      if (result)
	{
	  HSA_SORRY_AT (gimple_location (stmt),
			"support for HSA does not implement an assignment of "
			"return value from a void function");
	  return;
	}

      call_insn->result_code_list = new hsa_op_code_list (0);
    }

  /* Argument block start.  */
  hsa_insn_arg_block *arg_end = new hsa_insn_arg_block
    (BRIG_KIND_DIRECTIVE_ARG_BLOCK_END, call_insn);
  hbb->append_insn (arg_end);
}

/* Generate HSA instructions for a return value instruction.
   Instructions will be appended to HBB, which also needs to be the
   corresponding structure to the basic_block of STMT. SSA_MAP maps gimple SSA
   names to HSA pseudo registers.  */

static void
gen_hsa_insns_for_return (greturn *stmt, hsa_bb *hbb,
			vec <hsa_op_reg_p> *ssa_map)
{
  tree retval = gimple_return_retval (stmt);
  if (retval)
    {
      if (AGGREGATE_TYPE_P (TREE_TYPE (retval)))
	{
	  HSA_SORRY_AT (gimple_location (stmt),
			"HSA does not support return "
			"statement with an aggregate value type");
	  return;
	}

      /* Store of return value.  */
      BrigType16_t mtype = mem_type_for_type
	(hsa_type_for_scalar_tree_type (TREE_TYPE (retval), false));
      hsa_op_address *addr = new hsa_op_address (hsa_cfun->output_arg);
      hsa_op_base *src = hsa_reg_or_immed_for_gimple_op (retval, hbb, ssa_map);
      hsa_insn_mem *mem = new hsa_insn_mem (BRIG_OPCODE_ST, mtype, src, addr);
      hbb->append_insn (mem);
    }

  /* HSAIL return instruction emission.  */
  hsa_insn_basic *ret = new hsa_insn_basic (0, BRIG_OPCODE_RET);
  hbb->append_insn (ret);
}

/* Emit instructions that assign number of threads to lhs of gimple STMT.
 Intructions are appended to basic block HBB and SSA_MAP maps gimple
 SSA names to HSA pseudo registers.  */

static void
gen_get_num_threads (gimple *stmt, hsa_bb *hbb, vec <hsa_op_reg_p> *ssa_map)
{
  if (gimple_call_lhs (stmt) == NULL_TREE)
    return;

  hbb->append_insn (new hsa_insn_comment ("omp_get_num_threads"));
  hsa_op_address *addr = new hsa_op_address (hsa_num_threads);

  hsa_op_reg *dest = hsa_reg_for_gimple_ssa (gimple_call_lhs (stmt),
					     ssa_map);
  hsa_insn_basic *basic = new hsa_insn_mem
    (BRIG_OPCODE_LD, dest->type, dest, addr);

  hbb->append_insn (basic);
}


/* Emit instructions that set hsa_num_threads according to provided VALUE.
 Intructions are appended to basic block HBB and SSA_MAP maps gimple
 SSA names to HSA pseudo registers.  */

static void
gen_set_num_threads (tree value, hsa_bb *hbb, vec <hsa_op_reg_p> *ssa_map)
{
  hbb->append_insn (new hsa_insn_comment ("omp_set_num_threads"));
  hsa_op_with_type *src = hsa_reg_or_immed_for_gimple_op (value, hbb,
							  ssa_map);

  BrigType16_t dtype = hsa_num_threads->type;
  if (hsa_needs_cvt (dtype, src->type))
    {
      hsa_op_reg *tmp = new hsa_op_reg (dtype);
      hbb->append_insn (new hsa_insn_basic (2, BRIG_OPCODE_CVT, tmp->type,
					    tmp, src));
      src = tmp;
    }
  else
    src->type = dtype;

  hsa_op_address *addr = new hsa_op_address (hsa_num_threads);

  hsa_op_immed *limit = new hsa_op_immed (64, BRIG_TYPE_U32);
  hsa_op_reg *r = new hsa_op_reg (BRIG_TYPE_B1);
  hbb->append_insn
    (new hsa_insn_cmp (BRIG_COMPARE_LT, r->type, r, src, limit));

  BrigType16_t btype = hsa_bittype_for_type (hsa_num_threads->type);
  hsa_op_reg *src_min_reg = new hsa_op_reg (btype);

  hbb->append_insn
    (new hsa_insn_basic (4, BRIG_OPCODE_CMOV, src_min_reg->type,
			 src_min_reg, r, src, limit));

  hsa_insn_basic *basic = new hsa_insn_mem
    (BRIG_OPCODE_ST, hsa_num_threads->type, src_min_reg, addr);

  hbb->append_insn (basic);
}

/* Emit instructions that assign number of teams to lhs of gimple STMT.
   Intructions are appended to basic block HBB and SSA_MAP maps gimple
   SSA names to HSA pseudo registers.  */

static void
gen_get_num_teams (gimple *stmt, hsa_bb *hbb, vec <hsa_op_reg_p> *ssa_map)
{
  if (gimple_call_lhs (stmt) == NULL_TREE)
    return;

  hbb->append_insn
    (new hsa_insn_comment ("__builtin_omp_get_num_teams"));

  tree lhs = gimple_call_lhs (stmt);
  hsa_op_reg *dest = hsa_reg_for_gimple_ssa (lhs, ssa_map);
  hsa_op_immed *one = new hsa_op_immed (1, dest->type);

  hsa_insn_basic *basic = new hsa_insn_basic
    (2, BRIG_OPCODE_MOV, dest->type, dest, one);

  hbb->append_insn (basic);
}

/* Emit instructions that assign a team number to lhs of gimple STMT.
   Intructions are appended to basic block HBB and SSA_MAP maps gimple
   SSA names to HSA pseudo registers.  */

static void
gen_get_team_num (gimple *stmt, hsa_bb *hbb, vec <hsa_op_reg_p> *ssa_map)
{
  if (gimple_call_lhs (stmt) == NULL_TREE)
    return;

  hbb->append_insn
    (new hsa_insn_comment ("__builtin_omp_get_team_num"));

  tree lhs = gimple_call_lhs (stmt);
  hsa_op_reg *dest = hsa_reg_for_gimple_ssa (lhs, ssa_map);
  hsa_op_immed *zero = new hsa_op_immed (0, dest->type);

  hsa_insn_basic *basic = new hsa_insn_basic
    (2, BRIG_OPCODE_MOV, dest->type, dest, zero);

  hbb->append_insn (basic);
}

/* Set VALUE to a shadow kernel debug argument and append a new instruction
   to HBB basic block.  */

static void
set_debug_value (hsa_bb *hbb, hsa_op_with_type *value)
{
  hsa_op_reg *shadow_reg_ptr = hsa_cfun->get_shadow_reg ();

  hsa_op_address *addr = new hsa_op_address
    (shadow_reg_ptr, offsetof (hsa_kernel_dispatch, debug));
  hsa_insn_mem *mem = new hsa_insn_mem (BRIG_OPCODE_ST, BRIG_TYPE_U64, value,
					addr);
  hbb->append_insn (mem);
}

/* If STMT is a call of a known library function, generate code to perform
   it and return true.  */

static bool
gen_hsa_insns_for_known_library_call (gimple *stmt, hsa_bb *hbb,
				      vec <hsa_op_reg_p> *ssa_map)
{
  const char *name = hsa_get_declaration_name (gimple_call_fndecl (stmt));

  if (strcmp (name, "omp_is_initial_device") == 0)
    {
      tree lhs = gimple_call_lhs (stmt);
      if (!lhs)
	return true;

      hsa_op_reg *dest = hsa_reg_for_gimple_ssa (lhs, ssa_map);
      hsa_op_immed *imm = new hsa_op_immed (build_zero_cst (TREE_TYPE (lhs)));

      hsa_build_append_simple_mov (dest, imm, hbb);
      return true;
    }
  else if (strcmp (name, "omp_set_num_threads") == 0)
    {
      gen_set_num_threads (gimple_call_arg (stmt, 0), hbb, ssa_map);
      return true;
    }
  else if (strcmp (name, "omp_get_num_threads") == 0)
    {
      gen_get_num_threads (stmt, hbb, ssa_map);
      return true;
    }
  else if (strcmp (name, "omp_get_num_teams") == 0)
    {
      gen_get_num_teams (stmt, hbb, ssa_map);
      return true;
    }
  else if (strcmp (name, "omp_get_team_num") == 0)
    {
      gen_get_team_num (stmt, hbb, ssa_map);
      return true;
    }
  else if (strcmp (name, "hsa_set_debug_value") == 0)
    {
      /* FIXME: show warning if user uses a different function description.  */

      if (hsa_cfun->has_shadow_reg_p ())
	{
	  tree rhs1 = gimple_call_arg (stmt, 0);
	  hsa_op_with_type *src = hsa_reg_or_immed_for_gimple_op (rhs1, hbb,
								  ssa_map);

	  BrigType16_t dtype = BRIG_TYPE_U64;
	  if (hsa_needs_cvt (dtype, src->type))
	    {
	      hsa_op_reg *tmp = new hsa_op_reg (dtype);
	      hbb->append_insn (new hsa_insn_basic (2, BRIG_OPCODE_CVT,
						    tmp->type, tmp, src));
	      src = tmp;
	    }
	  else
	    src->type = dtype;

	  set_debug_value (hbb, src);
	  return true;
	}
    }

  return false;
}

/* Generate HSA instructions for the given kernel call statement CALL.
   Instructions will be appended to HBB.  */

static void
gen_hsa_insns_for_kernel_call (hsa_bb *hbb, gcall *call)
{
  /* TODO: all emitted instructions assume that
     we run on a LARGE_MODEL agent.  */

  hsa_insn_mem *mem;
  hsa_op_address *addr;
  hsa_op_immed *c;

  hsa_op_reg *shadow_reg_ptr = hsa_cfun->get_shadow_reg ();

  /* Get my kernel dispatch argument.  */
  hbb->append_insn (new hsa_insn_comment ("get kernel dispatch structure"));
  addr = new hsa_op_address
    (shadow_reg_ptr, offsetof (hsa_kernel_dispatch, children_dispatches));

  hsa_op_reg *shadow_reg_base_ptr = new hsa_op_reg (BRIG_TYPE_U64);
  mem = new hsa_insn_mem (BRIG_OPCODE_LD, BRIG_TYPE_U64, shadow_reg_base_ptr,
			  addr);
  hbb->append_insn (mem);

  unsigned index = hsa_cfun->kernel_dispatch_count;
  unsigned byte_offset = index * sizeof (hsa_kernel_dispatch *);

  addr = new hsa_op_address (shadow_reg_base_ptr, byte_offset);

  hsa_op_reg *shadow_reg = new hsa_op_reg (BRIG_TYPE_U64);
  mem = new hsa_insn_mem (BRIG_OPCODE_LD, BRIG_TYPE_U64, shadow_reg, addr);
  hbb->append_insn (mem);

  /* Load an address of the command queue to a register.  */
  hbb->append_insn (new hsa_insn_comment
		    ("load base address of command queue"));

  hsa_op_reg *queue_reg = new hsa_op_reg (BRIG_TYPE_U64);
  addr = new hsa_op_address (shadow_reg, offsetof (hsa_kernel_dispatch, queue));

  mem = new hsa_insn_mem (BRIG_OPCODE_LD, BRIG_TYPE_U64, queue_reg, addr);

  hbb->append_insn (mem);

  /* Load an address of prepared memory for a kernel arguments.  */
  hbb->append_insn (new hsa_insn_comment ("get a kernarg address"));
  hsa_op_reg *kernarg_reg = new hsa_op_reg (BRIG_TYPE_U64);

  addr = new hsa_op_address (shadow_reg,
			     offsetof (hsa_kernel_dispatch, kernarg_address));

  mem = new hsa_insn_mem (BRIG_OPCODE_LD, BRIG_TYPE_U64, kernarg_reg, addr);
  hbb->append_insn (mem);

  /* Load an kernel object we want to call.  */
  hbb->append_insn (new hsa_insn_comment ("get a kernel object"));
  hsa_op_reg *object_reg = new hsa_op_reg (BRIG_TYPE_U64);

  addr = new hsa_op_address (shadow_reg,
			     offsetof (hsa_kernel_dispatch, object));

  mem = new hsa_insn_mem (BRIG_OPCODE_LD, BRIG_TYPE_U64, object_reg, addr);
  hbb->append_insn (mem);

  /* Get signal prepared for the kernel dispatch.  */
  hbb->append_insn (new hsa_insn_comment ("get a signal by kernel call index"));

  hsa_op_reg *signal_reg = new hsa_op_reg (BRIG_TYPE_U64);
  addr = new hsa_op_address (shadow_reg,
			     offsetof (hsa_kernel_dispatch, signal));
  mem = new hsa_insn_mem (BRIG_OPCODE_LD, BRIG_TYPE_U64, signal_reg, addr);
  hbb->append_insn (mem);

  /* Store to synchronization signal.  */
  hbb->append_insn (new hsa_insn_comment ("store 1 to signal handle"));

  c = new hsa_op_immed (1, BRIG_TYPE_U64);

  hsa_insn_signal *signal= new hsa_insn_signal (2, BRIG_OPCODE_SIGNALNORET,
						BRIG_ATOMIC_ST, BRIG_TYPE_B64,
						signal_reg, c);
  signal->memoryorder = BRIG_MEMORY_ORDER_RELAXED;
  signal->memoryscope = BRIG_MEMORY_SCOPE_SYSTEM;
  hbb->append_insn (signal);

  /* Get private segment size.  */
  hsa_op_reg *private_seg_reg = new hsa_op_reg (BRIG_TYPE_U32);

  hbb->append_insn (new hsa_insn_comment
		    ("get a kernel private segment size by kernel call index"));

  addr = new hsa_op_address
    (shadow_reg, offsetof (hsa_kernel_dispatch, private_segment_size));
  mem = new hsa_insn_mem (BRIG_OPCODE_LD, BRIG_TYPE_U32, private_seg_reg, addr);
  hbb->append_insn (mem);

  /* Get group segment size.  */
  hsa_op_reg *group_seg_reg = new hsa_op_reg (BRIG_TYPE_U32);

  hbb->append_insn (new hsa_insn_comment
		    ("get a kernel group segment size by kernel call index"));

  addr = new hsa_op_address
    (shadow_reg, offsetof (hsa_kernel_dispatch, group_segment_size));
  mem = new hsa_insn_mem (BRIG_OPCODE_LD, BRIG_TYPE_U32, group_seg_reg, addr);
  hbb->append_insn (mem);

  /* Get a write index to the command queue.  */
  hsa_op_reg *queue_index_reg = new hsa_op_reg (BRIG_TYPE_U64);

  c = new hsa_op_immed (1, BRIG_TYPE_U64);
  hsa_insn_queue *queue = new hsa_insn_queue (3,
					      BRIG_OPCODE_ADDQUEUEWRITEINDEX);

  addr = new hsa_op_address (queue_reg);
  queue->set_op (0, queue_index_reg);
  queue->set_op (1, addr);
  queue->set_op (2, c);

  hbb->append_insn (queue);

  /* Get packet base address.  */
  size_t addr_offset = offsetof (hsa_queue, base_address);

  hsa_op_reg *queue_addr_reg = new hsa_op_reg (BRIG_TYPE_U64);

  c = new hsa_op_immed (addr_offset, BRIG_TYPE_U64);
  hsa_insn_basic *insn = new hsa_insn_basic
    (3, BRIG_OPCODE_ADD, BRIG_TYPE_U64, queue_addr_reg, queue_reg, c);

  hbb->append_insn (insn);

  hbb->append_insn (new hsa_insn_comment
		    ("get base address of prepared packet"));

  hsa_op_reg *queue_addr_value_reg = new hsa_op_reg (BRIG_TYPE_U64);
  addr = new hsa_op_address (queue_addr_reg);
  mem = new hsa_insn_mem (BRIG_OPCODE_LD, BRIG_TYPE_U64, queue_addr_value_reg,
			  addr);
  hbb->append_insn (mem);

  c = new hsa_op_immed (sizeof (hsa_queue_packet), BRIG_TYPE_U64);
  hsa_op_reg *queue_packet_offset_reg = new hsa_op_reg (BRIG_TYPE_U64);
  insn = new hsa_insn_basic
    (3, BRIG_OPCODE_MUL, BRIG_TYPE_U64, queue_packet_offset_reg,
     queue_index_reg, c);

  hbb->append_insn (insn);

  hsa_op_reg *queue_packet_reg = new hsa_op_reg (BRIG_TYPE_U64);
  insn = new hsa_insn_basic
    (3, BRIG_OPCODE_ADD, BRIG_TYPE_U64, queue_packet_reg, queue_addr_value_reg,
     queue_packet_offset_reg);

  hbb->append_insn (insn);


  /* Write to packet->setup.  */
  hbb->append_insn (new hsa_insn_comment ("set packet->setup |= 1"));

  addr = new hsa_op_address (queue_packet_reg,
			     offsetof (hsa_queue_packet, setup));
  hsa_op_reg *packet_setup_reg = new hsa_op_reg (BRIG_TYPE_U16);
  mem = new hsa_insn_mem (BRIG_OPCODE_LD, BRIG_TYPE_U16, packet_setup_reg,
			  addr);
  hbb->append_insn (mem);

  hsa_op_reg *packet_setup_u32 = new hsa_op_reg (BRIG_TYPE_U32);

  hsa_insn_basic *cvtinsn = new hsa_insn_basic
    (2, BRIG_OPCODE_CVT, BRIG_TYPE_U32, packet_setup_u32, packet_setup_reg);
  hbb->append_insn (cvtinsn);

  hsa_op_reg *packet_setup_u32_2 = new hsa_op_reg (BRIG_TYPE_U32);
  c = new hsa_op_immed (1, BRIG_TYPE_U32);
  insn = new hsa_insn_basic (3, BRIG_OPCODE_OR, BRIG_TYPE_U32,
			     packet_setup_u32_2, packet_setup_u32, c);

  hbb->append_insn (insn);

  hsa_op_reg *packet_setup_reg_2 = new hsa_op_reg (BRIG_TYPE_U16);

  cvtinsn = new hsa_insn_basic (2, BRIG_OPCODE_CVT, BRIG_TYPE_U16,
				packet_setup_reg_2, packet_setup_u32_2);
  hbb->append_insn (cvtinsn);

  addr = new hsa_op_address (queue_packet_reg,
			     offsetof (hsa_queue_packet, setup));
  mem = new hsa_insn_mem (BRIG_OPCODE_ST, BRIG_TYPE_U16, packet_setup_reg_2,
			  addr);
  hbb->append_insn (mem);

  /* Write to packet->grid_size_x.  */
  hbb->append_insn (new hsa_insn_comment
		    ("set packet->grid_size_x = hsa_num_threads"));

  addr = new hsa_op_address (queue_packet_reg,
			     offsetof (hsa_queue_packet, grid_size_x));

  hsa_op_reg *hsa_num_threads_reg = new hsa_op_reg (hsa_num_threads->type);
  hbb->append_insn (new hsa_insn_mem (BRIG_OPCODE_LD, hsa_num_threads->type,
				      hsa_num_threads_reg,
				      new hsa_op_address (hsa_num_threads)));

  hsa_op_reg *threads_u16_reg = new hsa_op_reg (BRIG_TYPE_U16);
  hbb->append_insn (new hsa_insn_basic (2, BRIG_OPCODE_CVT, BRIG_TYPE_U16,
					threads_u16_reg, hsa_num_threads_reg));

  mem = new hsa_insn_mem (BRIG_OPCODE_ST, BRIG_TYPE_U16, threads_u16_reg,
			  addr);
  hbb->append_insn (mem);

  /* Write to shadow_reg->omp_num_threads = hsa_num_threads.  */
  hbb->append_insn (new hsa_insn_comment
		    ("set shadow_reg->omp_num_threads = hsa_num_threads"));

  addr = new hsa_op_address (shadow_reg, offsetof (hsa_kernel_dispatch,
						   omp_num_threads));
  hbb->append_insn
    (new hsa_insn_mem (BRIG_OPCODE_ST, hsa_num_threads_reg->type,
		       hsa_num_threads_reg, addr));

  /* Write to packet->workgroup_size_x.  */
  hbb->append_insn (new hsa_insn_comment
		    ("set packet->workgroup_size_x = hsa_num_threads"));

  addr = new hsa_op_address (queue_packet_reg,
			     offsetof (hsa_queue_packet, workgroup_size_x));
  mem = new hsa_insn_mem (BRIG_OPCODE_ST, BRIG_TYPE_U16, threads_u16_reg,
			  addr);
  hbb->append_insn (mem);

  /* Write to packet->grid_size_y.  */
  hbb->append_insn (new hsa_insn_comment ("set packet->grid_size_y = 1"));

  addr = new hsa_op_address (queue_packet_reg,
			     offsetof (hsa_queue_packet, grid_size_y));
  c = new hsa_op_immed (1, BRIG_TYPE_U16);
  mem = new hsa_insn_mem (BRIG_OPCODE_ST, BRIG_TYPE_U16, c, addr);
  hbb->append_insn (mem);

  /* Write to packet->workgroup_size_y.  */
  hbb->append_insn (new hsa_insn_comment ("set packet->workgroup_size_y = 1"));

  addr = new hsa_op_address (queue_packet_reg,
			     offsetof (hsa_queue_packet, workgroup_size_y));
  c = new hsa_op_immed (1, BRIG_TYPE_U16);
  mem = new hsa_insn_mem (BRIG_OPCODE_ST, BRIG_TYPE_U16, c, addr);
  hbb->append_insn (mem);

  /* Write to packet->grid_size_z.  */
  hbb->append_insn (new hsa_insn_comment ("set packet->grid_size_z = 1"));

  addr = new hsa_op_address (queue_packet_reg,
			     offsetof (hsa_queue_packet, grid_size_z));
  c = new hsa_op_immed (1, BRIG_TYPE_U16);
  mem = new hsa_insn_mem (BRIG_OPCODE_ST, BRIG_TYPE_U16, c, addr);
  hbb->append_insn (mem);

  /* Write to packet->workgroup_size_z.  */
  hbb->append_insn (new hsa_insn_comment ("set packet->workgroup_size_z = 1"));

  addr = new hsa_op_address (queue_packet_reg,
			     offsetof (hsa_queue_packet, workgroup_size_z));
  c = new hsa_op_immed (1, BRIG_TYPE_U16);
  mem = new hsa_insn_mem (BRIG_OPCODE_ST, BRIG_TYPE_U16, c, addr);
  hbb->append_insn (mem);

  /* Write to packet->private_segment_size.  */
  hbb->append_insn (new hsa_insn_comment ("set packet->private_segment_size"));

  hsa_op_reg *private_seg_reg_u16 = new hsa_op_reg (BRIG_TYPE_U16);

  cvtinsn = new hsa_insn_basic (2, BRIG_OPCODE_CVT, BRIG_TYPE_U16,
				private_seg_reg_u16, private_seg_reg);
  hbb->append_insn (cvtinsn);

  addr = new hsa_op_address (queue_packet_reg,
			     offsetof (hsa_queue_packet, private_segment_size));
  mem = new hsa_insn_mem (BRIG_OPCODE_ST, BRIG_TYPE_U16, private_seg_reg_u16,
			  addr);
  hbb->append_insn (mem);

  /* Write to packet->group_segment_size.  */
  hbb->append_insn (new hsa_insn_comment ("set packet->group_segment_size"));

  hsa_op_reg *group_seg_reg_u16 = new hsa_op_reg (BRIG_TYPE_U16);

  cvtinsn = new hsa_insn_basic (2, BRIG_OPCODE_CVT, BRIG_TYPE_U16,
				group_seg_reg_u16, group_seg_reg);
  hbb->append_insn (cvtinsn);

  addr = new hsa_op_address (queue_packet_reg,
			     offsetof (hsa_queue_packet, group_segment_size));
  mem = new hsa_insn_mem (BRIG_OPCODE_ST, BRIG_TYPE_U16, group_seg_reg_u16,
			  addr);
  hbb->append_insn (mem);

  /* Write to packet->kernel_object.  */
  hbb->append_insn (new hsa_insn_comment ("set packet->kernel_object"));

  addr = new hsa_op_address (queue_packet_reg,
			     offsetof (hsa_queue_packet, kernel_object));
  mem = new hsa_insn_mem (BRIG_OPCODE_ST, BRIG_TYPE_U64, object_reg, addr);
  hbb->append_insn (mem);

  /* Copy locally allocated memory for arguments to a prepared one.  */
  hbb->append_insn (new hsa_insn_comment ("get address of omp data memory"));

  hsa_op_reg *omp_data_memory_reg = new hsa_op_reg (BRIG_TYPE_U64);

  addr = new hsa_op_address (shadow_reg,
			     offsetof (hsa_kernel_dispatch, omp_data_memory));

  mem = new hsa_insn_mem (BRIG_OPCODE_LD, BRIG_TYPE_U64, omp_data_memory_reg,
			  addr);
  hbb->append_insn (mem);

  hsa_op_address *dst_addr = new hsa_op_address (omp_data_memory_reg);

  tree argument = gimple_call_arg (call, 1);

  if (TREE_CODE (argument) == ADDR_EXPR)
    {
      /* Emit instructions that copy OMP arguments.  */

      tree d = TREE_TYPE (TREE_OPERAND (argument, 0));
      unsigned omp_data_size = tree_to_uhwi (TYPE_SIZE_UNIT (d));
      gcc_checking_assert (omp_data_size > 0);

      if (omp_data_size > hsa_cfun->maximum_omp_data_size)
	hsa_cfun->maximum_omp_data_size = omp_data_size;

      hsa_symbol *var_decl = get_symbol_for_decl (TREE_OPERAND (argument, 0));

      hbb->append_insn (new hsa_insn_comment ("memory copy instructions"));

      hsa_op_address *src_addr = new hsa_op_address (var_decl);
      gen_hsa_memory_copy (hbb, dst_addr, src_addr, var_decl->dim);
    }
  else if (integer_zerop (argument))
    {
      /* If NULL argument is passed, do nothing.  */
    }
  else
    gcc_unreachable ();

  hbb->append_insn (new hsa_insn_comment
		    ("write memory pointer to packet->kernarg_address"));

  addr = new hsa_op_address (queue_packet_reg,
			     offsetof (hsa_queue_packet, kernarg_address));
  mem = new hsa_insn_mem (BRIG_OPCODE_ST, BRIG_TYPE_U64, kernarg_reg, addr);
  hbb->append_insn (mem);

  /* Write to packet->kernarg_address.  */
  hbb->append_insn (new hsa_insn_comment
		    ("write argument0 to *packet->kernarg_address"));

  addr = new hsa_op_address (kernarg_reg);

  mem = new hsa_insn_mem (BRIG_OPCODE_ST, BRIG_TYPE_U64, omp_data_memory_reg,
			  addr);
  hbb->append_insn (mem);

  /* Pass shadow argument to another dispatched kernel module.  */
  hbb->append_insn (new hsa_insn_comment
		    ("write argument1 to *packet->kernarg_address"));

  addr = new hsa_op_address (kernarg_reg, 8);
  mem = new hsa_insn_mem (BRIG_OPCODE_ST, BRIG_TYPE_U64, shadow_reg, addr);
  hbb->append_insn (mem);

  /* Write to packet->competion_signal.  */
  hbb->append_insn (new hsa_insn_comment ("set packet->completion_signal"));

  addr = new hsa_op_address (queue_packet_reg,
			     offsetof (hsa_queue_packet, completion_signal));
  mem = new hsa_insn_mem (BRIG_OPCODE_ST, BRIG_TYPE_U64, signal_reg, addr);
  hbb->append_insn (mem);

  /* Atomically write to packer->header.  */
  hbb->append_insn
    (new hsa_insn_comment ("store atomically to packet->header"));

  addr = new hsa_op_address (queue_packet_reg,
			     offsetof (hsa_queue_packet, header));

  /* Store 5122 << 16 + 1 to packet->header.  */
  c = new hsa_op_immed (70658, BRIG_TYPE_U32);

  hsa_insn_atomic *atomic = new hsa_insn_atomic (2, BRIG_OPCODE_ATOMICNORET,
						 BRIG_ATOMIC_ST, BRIG_TYPE_B32,
						 addr, c);
  atomic->memoryorder = BRIG_MEMORY_ORDER_SC_RELEASE;
  atomic->memoryscope = BRIG_MEMORY_SCOPE_SYSTEM;

  hbb->append_insn (atomic);

  /* Ring doorbell signal.  */
  hbb->append_insn (new hsa_insn_comment ("store index to doorbell signal"));

  hsa_op_reg *doorbell_signal_reg = new hsa_op_reg (BRIG_TYPE_U64);
  addr = new hsa_op_address (queue_reg, offsetof (hsa_queue, doorbell_signal));
  mem = new hsa_insn_mem (BRIG_OPCODE_LD, BRIG_TYPE_U64, doorbell_signal_reg,
			  addr);
  hbb->append_insn (mem);

  signal = new hsa_insn_signal (2, BRIG_OPCODE_SIGNALNORET, BRIG_ATOMIC_ST,
				BRIG_TYPE_B64, doorbell_signal_reg,
				queue_index_reg);
  signal->memoryorder = BRIG_MEMORY_ORDER_SC_RELEASE;
  signal->memoryscope = BRIG_MEMORY_SCOPE_SYSTEM;
  hbb->append_insn (signal);

  /* Emit blocking signal waiting instruction.  */
  hbb->append_insn (new hsa_insn_comment ("wait for the signal"));

  hsa_op_reg *signal_result_reg = new hsa_op_reg (BRIG_TYPE_U64);
  c = new hsa_op_immed (1, BRIG_TYPE_S64);
  hsa_op_immed *c2 = new hsa_op_immed (UINT64_MAX, BRIG_TYPE_U64);

  signal = new hsa_insn_signal (4, BRIG_OPCODE_SIGNAL,
				BRIG_ATOMIC_WAITTIMEOUT_LT, BRIG_TYPE_S64);
  signal->memoryorder = BRIG_MEMORY_ORDER_SC_ACQUIRE;
  signal->memoryscope = BRIG_MEMORY_SCOPE_SYSTEM;
  signal->set_op (0, signal_result_reg);
  signal->set_op (1, signal_reg);
  signal->set_op (2, c);
  signal->set_op (3, c2);
  hbb->append_insn (signal);

  hsa_cfun->kernel_dispatch_count++;
}

/* Helper functions to create a single unary HSA operations out of calls to
   builtins.  OPCODE is the HSA operation to be generated.  STMT is a gimple
   call to a builtin.  HBB is the HSA BB to which the instruction should be
   added and SSA_MAP is used to map gimple SSA names to HSA
   pseudoregisters.  */

static void
gen_hsa_unaryop_for_builtin (int opcode, gimple *stmt, hsa_bb *hbb,
			     vec <hsa_op_reg_p> *ssa_map)
{
  tree lhs = gimple_call_lhs (stmt);
  /* FIXME: Since calls without a LHS are not removed, double check that
     they cannot have side effects.  */
  if (!lhs)
    return;
  hsa_op_reg *dest = hsa_reg_for_gimple_ssa (lhs, ssa_map);
  hsa_op_with_type *op = hsa_reg_or_immed_for_gimple_op
    (gimple_call_arg (stmt, 0), hbb, ssa_map);
  gen_hsa_unary_operation (opcode, dest, op, hbb);
}

/* Generate HSA address corresponding to a value VAL (as opposed to a memory
   reference tree), for example an SSA_NAME or an ADDR_EXPR.  HBB is the HSA BB
   to which the instruction should be added and SSA_MAP is used to map gimple
   SSA names to HSA pseudoregisters.  */

static hsa_op_address *
get_address_from_value (tree val, hsa_bb *hbb, vec <hsa_op_reg_p> *ssa_map)
{
  switch (TREE_CODE (val))
    {
    case SSA_NAME:
      {
	BrigType16_t addrtype = hsa_get_segment_addr_type (BRIG_SEGMENT_FLAT);
	hsa_op_reg *reg = hsa_reg_for_gimple_ssa_reqtype (val, ssa_map,
							hbb, addrtype);
	return new hsa_op_address (NULL, reg, 0);
      }
    case ADDR_EXPR:
      return gen_hsa_addr (TREE_OPERAND (val, 0), hbb, ssa_map);

    case INTEGER_CST:
      if (tree_fits_shwi_p (val))
	return new hsa_op_address (NULL, NULL, tree_to_shwi (val));
      /* Otherwise fall-through */

    default:
      HSA_SORRY_ATV (EXPR_LOCATION (val),
		     "support for HSA does not implement memory access to %E",
		     val);
      return new hsa_op_address (NULL, NULL, 0);
    }
}

/* Helper function to create an HSA atomic binary operation instruction out of
   calls to atomic builtins.  RET_ORIG is true if the built-in is the variant
   that return s the value before applying operation, and false if it should
   return the value after applying the operation (if it returns value at all).
   ACODE is the atomic operation code, STMT is a gimple call to a builtin.  HBB
   is the HSA BB to which the instruction should be added and SSA_MAP is used
   to map gimple SSA names to HSA pseudoregisters.*/

static void
gen_hsa_ternary_atomic_for_builtin (bool ret_orig,
 				    enum BrigAtomicOperation acode, gimple *stmt,
				    hsa_bb *hbb, vec <hsa_op_reg_p> *ssa_map)
{
  tree lhs = gimple_call_lhs (stmt);

  tree type = TREE_TYPE (gimple_call_arg (stmt, 1));
  BrigType16_t hsa_type  = hsa_type_for_scalar_tree_type (type, false);
  BrigType16_t mtype = mem_type_for_type (hsa_type);

  /* Certain atomic insns must have Bx memory types.  */
  switch (acode)
    {
    case BRIG_ATOMIC_LD:
    case BRIG_ATOMIC_ST:
      mtype = hsa_bittype_for_type (mtype);
      break;
    default:
      break;
    }

  hsa_op_reg *dest;
  int nops, opcode;
  if (lhs)
    {
      if (ret_orig)
	dest = hsa_reg_for_gimple_ssa (lhs, ssa_map);
      else
	dest = new hsa_op_reg (hsa_type);
      opcode = BRIG_OPCODE_ATOMIC;
      nops = 3;
    }
  else
    {
      dest = NULL;
      opcode = BRIG_OPCODE_ATOMICNORET;
      nops = 2;
    }

  hsa_insn_atomic *atominsn = new hsa_insn_atomic (nops, opcode, acode, mtype);

  /* Overwrite default memory order for ATOMIC_ST insn which can have just
     RLX or SCREL memory order.  */
  if (acode == BRIG_ATOMIC_ST)
    atominsn->memoryorder = BRIG_MEMORY_ORDER_SC_RELEASE;

  hsa_op_address *addr;
  addr = get_address_from_value (gimple_call_arg (stmt, 0), hbb, ssa_map);
  /* TODO: Warn if addr has private segment, because the finalizer will not
     accept that (and it does not make much sense).  */
  hsa_op_base *op = hsa_reg_or_immed_for_gimple_op (gimple_call_arg (stmt, 1),
						    hbb, ssa_map);

  if (lhs)
    {
      atominsn->set_op (0, dest);
      atominsn->set_op (1, addr);
      atominsn->set_op (2, op);
    }
  else
    {
      atominsn->set_op (0, addr);
      atominsn->set_op (1, op);
    }
  /* FIXME: Perhaps select a more relaxed memory model based on the last
     argument of the buildin call.  */

  hbb->append_insn (atominsn);

  /* HSA does not natively support the variants that return the modified value,
     so re-do the operation again non-atomically if that is what was
     requested.  */
  if (lhs && !ret_orig)
    {
      int arith;
      switch (acode)
	{
	case BRIG_ATOMIC_ADD:
	  arith = BRIG_OPCODE_ADD;
	  break;
	case BRIG_ATOMIC_AND:
	  arith = BRIG_OPCODE_AND;
	  break;
	case BRIG_ATOMIC_OR:
	  arith = BRIG_OPCODE_OR;
	  break;
	case BRIG_ATOMIC_SUB:
	  arith = BRIG_OPCODE_SUB;
	  break;
	case BRIG_ATOMIC_XOR:
	  arith = BRIG_OPCODE_XOR;
	  break;
	default:
	  gcc_unreachable ();
	}
      hsa_op_reg *real_dest = hsa_reg_for_gimple_ssa (lhs, ssa_map);
      gen_hsa_binary_operation (arith, real_dest, dest, op, hbb);
    }
}

#define HSA_MEMORY_BUILTINS_LIMIT     128

/* Generate HSA instructions for the given call statement STMT.  Instructions
   will be appended to HBB.  SSA_MAP maps gimple SSA names to HSA pseudo
   registers.  */

static void
gen_hsa_insns_for_call (gimple *stmt, hsa_bb *hbb,
			vec <hsa_op_reg_p> *ssa_map)
{
  tree lhs = gimple_call_lhs (stmt);
  hsa_op_reg *dest;
  hsa_insn_basic *insn;
  int opcode;

  if (!gimple_call_builtin_p (stmt, BUILT_IN_NORMAL))
    {
      tree function_decl = gimple_call_fndecl (stmt);
      if (function_decl == NULL_TREE)
	{
	  HSA_SORRY_AT (gimple_location (stmt),
			"support for HSA does not implement indirect calls");
	  return;
	}

      if (hsa_callable_function_p (function_decl))
        gen_hsa_insns_for_direct_call (stmt, hbb, ssa_map);
      else if (!gen_hsa_insns_for_known_library_call (stmt, hbb, ssa_map))
	HSA_SORRY_AT (gimple_location (stmt),
		      "HSA does support only call of functions within omp "
		      "declare target or with 'hsafunc' attribute");
      return;
    }

  tree fndecl = gimple_call_fndecl (stmt);
  switch (DECL_FUNCTION_CODE (fndecl))
    {
    case BUILT_IN_OMP_GET_THREAD_NUM:
      opcode = BRIG_OPCODE_WORKITEMABSID;
      goto specialop;

    case BUILT_IN_OMP_GET_NUM_THREADS:
      {
	gen_get_num_threads (stmt, hbb, ssa_map);
	break;
      }

specialop:
      {
	hsa_op_reg *tmp;
	dest = hsa_reg_for_gimple_ssa (lhs, ssa_map);
	/* We're using just one-dimensional kernels, so hard-coded
	   dimension X.  */
	hsa_op_immed *imm = new hsa_op_immed
	  (build_zero_cst (uint32_type_node));
	if (dest->type != BRIG_TYPE_U32)
	  tmp = new hsa_op_reg (BRIG_TYPE_U32);
	else
	  tmp = dest;
	insn = new hsa_insn_basic (2, opcode, tmp->type, tmp, imm);
	hbb->append_insn (insn);
	if (dest != tmp)
	  {
	    int opc2 = dest->type == BRIG_TYPE_S32 ? BRIG_OPCODE_MOV
	      : BRIG_OPCODE_CVT;
	    insn = new hsa_insn_basic (2, opc2, dest->type, dest, tmp);
	    hbb->append_insn (insn);
	  }
	break;
      }

    case BUILT_IN_FABS:
    case BUILT_IN_FABSF:
      gen_hsa_unaryop_for_builtin (BRIG_OPCODE_ABS, stmt, hbb, ssa_map);
      break;

    case BUILT_IN_CEIL:
    case BUILT_IN_CEILF:
      gen_hsa_unaryop_for_builtin (BRIG_OPCODE_CEIL, stmt, hbb, ssa_map);
      break;

    case BUILT_IN_FLOOR:
    case BUILT_IN_FLOORF:
      gen_hsa_unaryop_for_builtin (BRIG_OPCODE_FLOOR, stmt, hbb, ssa_map);
      break;

    case BUILT_IN_RINT:
    case BUILT_IN_RINTF:
      gen_hsa_unaryop_for_builtin (BRIG_OPCODE_RINT, stmt, hbb, ssa_map);
      break;

    case BUILT_IN_SQRT:
    case BUILT_IN_SQRTF:
      /* TODO: Perhaps produce BRIG_OPCODE_NSQRT with -ffast-math?  */
      gen_hsa_unaryop_for_builtin (BRIG_OPCODE_SQRT, stmt, hbb, ssa_map);
      break;

    case BUILT_IN_TRUNC:
    case BUILT_IN_TRUNCF:
      gen_hsa_unaryop_for_builtin (BRIG_OPCODE_TRUNC, stmt, hbb, ssa_map);
      break;

    case BUILT_IN_COS:
    case BUILT_IN_COSF:
      /* FIXME: Using the native instruction may not be precise enough.
	 Perhaps only allow if using -ffast-math?  */
      gen_hsa_unaryop_for_builtin (BRIG_OPCODE_NCOS, stmt, hbb, ssa_map);
      break;

    case BUILT_IN_EXP2:
    case BUILT_IN_EXP2F:
      /* FIXME: Using the native instruction may not be precise enough.
	 Perhaps only allow if using -ffast-math?  */
      gen_hsa_unaryop_for_builtin (BRIG_OPCODE_NEXP2, stmt, hbb, ssa_map);
      break;

    case BUILT_IN_LOG2:
    case BUILT_IN_LOG2F:
      /* FIXME: Using the native instruction may not be precise enough.
	 Perhaps only allow if using -ffast-math?  */
      gen_hsa_unaryop_for_builtin (BRIG_OPCODE_NLOG2, stmt, hbb, ssa_map);
      break;

    case BUILT_IN_SIN:
    case BUILT_IN_SINF:
      /* FIXME: Using the native instruction may not be precise enough.
	 Perhaps only allow if using -ffast-math?  */
      gen_hsa_unaryop_for_builtin (BRIG_OPCODE_NSIN, stmt, hbb, ssa_map);
      break;

    case BUILT_IN_ATOMIC_LOAD_1:
    case BUILT_IN_ATOMIC_LOAD_2:
    case BUILT_IN_ATOMIC_LOAD_4:
    case BUILT_IN_ATOMIC_LOAD_8:
    case BUILT_IN_ATOMIC_LOAD_16:
      {
	BrigType16_t mtype;
	hsa_op_address *addr;
	addr = get_address_from_value (gimple_call_arg (stmt, 0), hbb, ssa_map);

	if (lhs)
	  {
	    mtype = mem_type_for_type
	      (hsa_type_for_scalar_tree_type (TREE_TYPE (lhs), false));
	    mtype = hsa_bittype_for_type (mtype);
	    dest = hsa_reg_for_gimple_ssa (lhs, ssa_map);
	  }
	else
	  {
	    mtype = BRIG_TYPE_B64;
	    dest = new hsa_op_reg (mtype);
	  }

	hsa_insn_atomic *atominsn
	  = new hsa_insn_atomic (2, BRIG_OPCODE_ATOMIC, BRIG_ATOMIC_LD, mtype,
				 dest, addr);

	atominsn->memoryorder = BRIG_MEMORY_ORDER_SC_ACQUIRE;

	hbb->append_insn (atominsn);
	break;
      }

    case BUILT_IN_ATOMIC_EXCHANGE_1:
    case BUILT_IN_ATOMIC_EXCHANGE_2:
    case BUILT_IN_ATOMIC_EXCHANGE_4:
    case BUILT_IN_ATOMIC_EXCHANGE_8:
    case BUILT_IN_ATOMIC_EXCHANGE_16:
      gen_hsa_ternary_atomic_for_builtin (true, BRIG_ATOMIC_EXCH, stmt, hbb,
					  ssa_map);
      break;

    case BUILT_IN_ATOMIC_FETCH_ADD_1:
    case BUILT_IN_ATOMIC_FETCH_ADD_2:
    case BUILT_IN_ATOMIC_FETCH_ADD_4:
    case BUILT_IN_ATOMIC_FETCH_ADD_8:
    case BUILT_IN_ATOMIC_FETCH_ADD_16:
      gen_hsa_ternary_atomic_for_builtin (true, BRIG_ATOMIC_ADD, stmt, hbb,
					  ssa_map);
      break;

    case BUILT_IN_ATOMIC_FETCH_SUB_1:
    case BUILT_IN_ATOMIC_FETCH_SUB_2:
    case BUILT_IN_ATOMIC_FETCH_SUB_4:
    case BUILT_IN_ATOMIC_FETCH_SUB_8:
    case BUILT_IN_ATOMIC_FETCH_SUB_16:
      gen_hsa_ternary_atomic_for_builtin (true, BRIG_ATOMIC_SUB, stmt, hbb,
					  ssa_map);
      break;

    case BUILT_IN_ATOMIC_FETCH_AND_1:
    case BUILT_IN_ATOMIC_FETCH_AND_2:
    case BUILT_IN_ATOMIC_FETCH_AND_4:
    case BUILT_IN_ATOMIC_FETCH_AND_8:
    case BUILT_IN_ATOMIC_FETCH_AND_16:
      gen_hsa_ternary_atomic_for_builtin (true, BRIG_ATOMIC_AND, stmt, hbb,
					  ssa_map);
      break;

    case BUILT_IN_ATOMIC_FETCH_XOR_1:
    case BUILT_IN_ATOMIC_FETCH_XOR_2:
    case BUILT_IN_ATOMIC_FETCH_XOR_4:
    case BUILT_IN_ATOMIC_FETCH_XOR_8:
    case BUILT_IN_ATOMIC_FETCH_XOR_16:
      gen_hsa_ternary_atomic_for_builtin (true, BRIG_ATOMIC_XOR, stmt, hbb,
					  ssa_map);
      break;

    case BUILT_IN_ATOMIC_FETCH_OR_1:
    case BUILT_IN_ATOMIC_FETCH_OR_2:
    case BUILT_IN_ATOMIC_FETCH_OR_4:
    case BUILT_IN_ATOMIC_FETCH_OR_8:
    case BUILT_IN_ATOMIC_FETCH_OR_16:
      gen_hsa_ternary_atomic_for_builtin (true, BRIG_ATOMIC_OR, stmt, hbb,
					  ssa_map);
      break;

    case BUILT_IN_ATOMIC_STORE_1:
    case BUILT_IN_ATOMIC_STORE_2:
    case BUILT_IN_ATOMIC_STORE_4:
    case BUILT_IN_ATOMIC_STORE_8:
    case BUILT_IN_ATOMIC_STORE_16:
      /* Since there canot be any LHS, the first parameter is meaningless.  */
      gen_hsa_ternary_atomic_for_builtin (true, BRIG_ATOMIC_ST, stmt, hbb,
					  ssa_map);
      break;

    case BUILT_IN_ATOMIC_ADD_FETCH_1:
    case BUILT_IN_ATOMIC_ADD_FETCH_2:
    case BUILT_IN_ATOMIC_ADD_FETCH_4:
    case BUILT_IN_ATOMIC_ADD_FETCH_8:
    case BUILT_IN_ATOMIC_ADD_FETCH_16:
      gen_hsa_ternary_atomic_for_builtin (false, BRIG_ATOMIC_ADD, stmt, hbb,
					  ssa_map);
      break;

    case BUILT_IN_ATOMIC_SUB_FETCH_1:
    case BUILT_IN_ATOMIC_SUB_FETCH_2:
    case BUILT_IN_ATOMIC_SUB_FETCH_4:
    case BUILT_IN_ATOMIC_SUB_FETCH_8:
    case BUILT_IN_ATOMIC_SUB_FETCH_16:
      gen_hsa_ternary_atomic_for_builtin (false, BRIG_ATOMIC_SUB, stmt, hbb,
					  ssa_map);
      break;

    case BUILT_IN_ATOMIC_AND_FETCH_1:
    case BUILT_IN_ATOMIC_AND_FETCH_2:
    case BUILT_IN_ATOMIC_AND_FETCH_4:
    case BUILT_IN_ATOMIC_AND_FETCH_8:
    case BUILT_IN_ATOMIC_AND_FETCH_16:
      gen_hsa_ternary_atomic_for_builtin (false, BRIG_ATOMIC_AND, stmt, hbb,
					  ssa_map);
      break;

    case BUILT_IN_ATOMIC_XOR_FETCH_1:
    case BUILT_IN_ATOMIC_XOR_FETCH_2:
    case BUILT_IN_ATOMIC_XOR_FETCH_4:
    case BUILT_IN_ATOMIC_XOR_FETCH_8:
    case BUILT_IN_ATOMIC_XOR_FETCH_16:
      gen_hsa_ternary_atomic_for_builtin (false, BRIG_ATOMIC_XOR, stmt, hbb,
					  ssa_map);
      break;

    case BUILT_IN_ATOMIC_OR_FETCH_1:
    case BUILT_IN_ATOMIC_OR_FETCH_2:
    case BUILT_IN_ATOMIC_OR_FETCH_4:
    case BUILT_IN_ATOMIC_OR_FETCH_8:
    case BUILT_IN_ATOMIC_OR_FETCH_16:
      gen_hsa_ternary_atomic_for_builtin (false, BRIG_ATOMIC_OR, stmt, hbb,
					  ssa_map);
      break;

    case BUILT_IN_SYNC_VAL_COMPARE_AND_SWAP_1:
    case BUILT_IN_SYNC_VAL_COMPARE_AND_SWAP_2:
    case BUILT_IN_SYNC_VAL_COMPARE_AND_SWAP_4:
    case BUILT_IN_SYNC_VAL_COMPARE_AND_SWAP_8:
    case BUILT_IN_SYNC_VAL_COMPARE_AND_SWAP_16:
      {
	/* XXX Ignore mem model for now.  */
	tree type = TREE_TYPE (gimple_call_arg (stmt, 1));

	BrigType16_t atype  = hsa_bittype_for_type
	  (hsa_type_for_scalar_tree_type (type, false));

	hsa_insn_atomic *atominsn = new hsa_insn_atomic
	  (4, BRIG_OPCODE_ATOMIC, BRIG_ATOMIC_CAS, atype);
	hsa_op_address *addr;
	addr = get_address_from_value (gimple_call_arg (stmt, 0), hbb, ssa_map);

	if (lhs != NULL)
	  dest = hsa_reg_for_gimple_ssa (lhs, ssa_map);
	else
	  dest = new hsa_op_reg (atype);

	/* Should check what the memory scope is */
	atominsn->memoryscope = BRIG_MEMORY_SCOPE_WORKGROUP;
	atominsn->set_op (0, dest);
	atominsn->set_op (1, addr);
	atominsn->set_op
	  (2, hsa_reg_or_immed_for_gimple_op (gimple_call_arg (stmt, 1),
					      hbb, ssa_map));
	atominsn->set_op
	  (3, hsa_reg_or_immed_for_gimple_op (gimple_call_arg (stmt, 2),
					      hbb, ssa_map));
	atominsn->memoryorder = BRIG_MEMORY_ORDER_SC_ACQUIRE_RELEASE;

	hbb->append_insn (atominsn);
	break;
      }
    case BUILT_IN_GOMP_PARALLEL:
      {
	gcc_checking_assert (gimple_call_num_args (stmt) == 4);
	tree called = gimple_call_arg (stmt, 0);
	gcc_checking_assert (TREE_CODE (called) == ADDR_EXPR);
	called = TREE_OPERAND (called, 0);
	gcc_checking_assert (TREE_CODE (called) == FUNCTION_DECL);

	const char *name = hsa_get_declaration_name
	  (hsa_get_gpu_function (called));
	hsa_add_kernel_dependency (hsa_cfun->decl,
				   hsa_brig_function_name (name));
	gen_hsa_insns_for_kernel_call (hbb, as_a <gcall *> (stmt));

	break;
      }
    case BUILT_IN_GOMP_TEAMS:
      {
	gen_set_num_threads (gimple_call_arg (stmt, 1), hbb, ssa_map);
	break;
      }
    case BUILT_IN_OMP_GET_NUM_TEAMS:
      {
	gen_get_num_teams (stmt, hbb, ssa_map);
	break;
      }
    case BUILT_IN_OMP_GET_TEAM_NUM:
      {
	gen_get_team_num (stmt, hbb, ssa_map);
	break;
      }
    case BUILT_IN_MEMCPY:
      {
	tree byte_size = gimple_call_arg (stmt, 2);

	if (TREE_CODE (byte_size) != INTEGER_CST)
	  {
	    gen_hsa_insns_for_direct_call (stmt, hbb, ssa_map);
	    return;
	  }

	unsigned n = tree_to_uhwi (byte_size);

	if (n > HSA_MEMORY_BUILTINS_LIMIT)
	  {
	    gen_hsa_insns_for_direct_call (stmt, hbb, ssa_map);
	    return;
	  }

	tree dst = gimple_call_arg (stmt, 0);
	tree src = gimple_call_arg (stmt, 1);

	hsa_op_address *dst_addr = get_address_from_value (dst, hbb, ssa_map);
	hsa_op_address *src_addr = get_address_from_value (src, hbb, ssa_map);

	gen_hsa_memory_copy (hbb, dst_addr, src_addr, n);

	tree lhs = gimple_call_lhs (stmt);
	if (lhs)
	  gen_hsa_insns_for_single_assignment (lhs, dst, hbb, ssa_map);

	break;
      }
    case BUILT_IN_MEMSET:
      {
	tree dst = gimple_call_arg (stmt, 0);
	tree c = gimple_call_arg (stmt, 1);

	if (TREE_CODE (c) != INTEGER_CST)
	  {
	    gen_hsa_insns_for_direct_call (stmt, hbb, ssa_map);
	    return;
	  }

	tree byte_size = gimple_call_arg (stmt, 2);

	if (TREE_CODE (byte_size) != INTEGER_CST)
	  {
	    gen_hsa_insns_for_direct_call (stmt, hbb, ssa_map);
	    return;
	  }

	unsigned n = tree_to_uhwi (byte_size);

	if (n > HSA_MEMORY_BUILTINS_LIMIT)
	  {
	    gen_hsa_insns_for_direct_call (stmt, hbb, ssa_map);
	    return;
	  }

	hsa_op_address *dst_addr;
	dst_addr = get_address_from_value (dst, hbb,
					   ssa_map);
	unsigned HOST_WIDE_INT constant = tree_to_uhwi
	  (fold_convert (unsigned_char_type_node, c));

	gen_hsa_memory_set (hbb, dst_addr, constant, n);

	tree lhs = gimple_call_lhs (stmt);
	if (lhs)
	  gen_hsa_insns_for_single_assignment (lhs, dst, hbb, ssa_map);

	break;
      }
    default:
      {
	gen_hsa_insns_for_direct_call (stmt, hbb, ssa_map);
	return;
      }
    }
}

/* Generate HSA instructions for a given gimple statement.  Instructions will be
   appended to HBB.  SSA_MAP maps gimple SSA names to HSA pseudo registers.  */

static void
gen_hsa_insns_for_gimple_stmt (gimple *stmt, hsa_bb *hbb,
			       vec <hsa_op_reg_p> *ssa_map)
{
  switch (gimple_code (stmt))
    {
    case GIMPLE_ASSIGN:
      if (gimple_clobber_p (stmt))
	break;

      if (gimple_assign_single_p (stmt))
	{
	  tree lhs = gimple_assign_lhs (stmt);
	  tree rhs = gimple_assign_rhs1 (stmt);
	  gen_hsa_insns_for_single_assignment (lhs, rhs, hbb, ssa_map);
	}
      else
	gen_hsa_insns_for_operation_assignment (stmt, hbb, ssa_map);
      break;
    case GIMPLE_RETURN:
      gen_hsa_insns_for_return (as_a <greturn *> (stmt), hbb, ssa_map);
      break;
    case GIMPLE_COND:
      gen_hsa_insns_for_cond_stmt (stmt, hbb, ssa_map);
      break;
    case GIMPLE_CALL:
      gen_hsa_insns_for_call (stmt, hbb, ssa_map);
      break;
    case GIMPLE_DEBUG:
      /* ??? HSA supports some debug facilities.  */
      break;
    case GIMPLE_LABEL:
    {
      tree label = gimple_label_label (as_a <glabel *> (stmt));
      if (FORCED_LABEL (label))
	HSA_SORRY_AT (gimple_location (stmt),
		      "support for HSA does not implement gimple label with "
		      "address taken");

      break;
    }
    case GIMPLE_NOP:
    {
      hbb->append_insn (new hsa_insn_basic (0, BRIG_OPCODE_NOP));
      break;
    }
    case GIMPLE_SWITCH:
    {
      gen_hsa_insns_for_switch_stmt (as_a <gswitch *> (stmt), hbb, ssa_map);
      break;
    }
    default:
      HSA_SORRY_ATV (gimple_location (stmt),
		     "support for HSA does not implement gimple statement %s",
		     gimple_code_name[(int) gimple_code (stmt)]);
    }
}

/* Generate a HSA PHI from a gimple PHI.  SSA_MAP maps gimple SSA names to HSA
   pseudo registers.  */

static void
gen_hsa_phi_from_gimple_phi (gimple *phi_stmt, hsa_bb *hbb,
			     vec <hsa_op_reg_p> *ssa_map)
{
  hsa_insn_phi *hphi;
  unsigned count = gimple_phi_num_args (phi_stmt);

  hsa_op_reg *dest = hsa_reg_for_gimple_ssa (gimple_phi_result (phi_stmt),
					     ssa_map);
  hphi = new hsa_insn_phi (count, dest);
  hphi->bb = hbb->bb;

  tree lhs = gimple_phi_result (phi_stmt);

  for (unsigned i = 0; i < count; i++)
    {
      tree op = gimple_phi_arg_def (phi_stmt, i);

      if (TREE_CODE (op) == SSA_NAME)
	{
	  hsa_op_reg *hreg = hsa_reg_for_gimple_ssa (op, ssa_map);
	  hphi->set_op (i, hreg);
	}
      else
	{
	  gcc_assert (is_gimple_min_invariant (op));
	  tree t = TREE_TYPE (op);
	  if (!POINTER_TYPE_P (t)
	      || (TREE_CODE (op) == STRING_CST
		  && TREE_CODE (TREE_TYPE (t)) == INTEGER_TYPE))
	    hphi->set_op (i, new hsa_op_immed (op));
	  else if (POINTER_TYPE_P (TREE_TYPE (lhs))
		   && TREE_CODE (op) == INTEGER_CST)
	    {
	      /* Handle assignment of NULL value to a pointer type.  */
	      hphi->set_op (i, new hsa_op_immed (op));
	    }
	  else if (TREE_CODE (op) == ADDR_EXPR)
	    {
	      edge e = gimple_phi_arg_edge (as_a <gphi *> (phi_stmt), i);
	      hsa_bb *hbb_src = hsa_init_new_bb (split_edge (e));
	      hsa_op_address *addr = gen_hsa_addr (TREE_OPERAND (op, 0),
						hbb_src, ssa_map);

	      hsa_op_reg *dest = new hsa_op_reg (BRIG_TYPE_U64);
	      hsa_insn_basic *insn = new  hsa_insn_basic
		(2, BRIG_OPCODE_LDA, BRIG_TYPE_U64, dest, addr);
	      hbb_src->append_insn (insn);

	      hphi->set_op (i, dest);
	    }
	  else
	    {
	      HSA_SORRY_AT (gimple_location (phi_stmt),
			    "support for HSA does not handle PHI nodes with "
			    "constant address operands");
	      return;
	    }
	}
    }

  hphi->prev = hbb->last_phi;
  hphi->next = NULL;
  if (hbb->last_phi)
    hbb->last_phi->next = hphi;
  hbb->last_phi = hphi;
  if (!hbb->first_phi)
    hbb->first_phi = hphi;
}

/* Constructor of class containing HSA-specific information about a basic
   block.  CFG_BB is the CFG BB this HSA BB is associated with.  IDX is the new
   index of this BB (so that the constructor does not attempt to use
   hsa_cfun during its construction).  */

hsa_bb::hsa_bb (basic_block cfg_bb, int idx)
{
  gcc_assert (!cfg_bb->aux);
  cfg_bb->aux = this;
  bb = cfg_bb;
  first_insn = last_insn = NULL;
  first_phi = last_phi = NULL;
  index = idx;
  livein = BITMAP_ALLOC (NULL);
  liveout = BITMAP_ALLOC (NULL);
}

/* Constructor of class containing HSA-specific information about a basic
   block.  CFG_BB is the CFG BB this HSA BB is associated with.  */

hsa_bb::hsa_bb (basic_block cfg_bb)
{
  gcc_assert (!cfg_bb->aux);
  cfg_bb->aux = this;
  bb = cfg_bb;
  first_insn = last_insn = NULL;
  first_phi = last_phi = NULL;
  index = hsa_cfun->hbb_count++;
  livein = BITMAP_ALLOC (NULL);
  liveout = BITMAP_ALLOC (NULL);
}

/* Destructor of class representing HSA BB.  */

hsa_bb::~hsa_bb ()
{
  BITMAP_FREE (livein);
  BITMAP_FREE (liveout);
}

/* Create and initialize and return a new hsa_bb structure for a given CFG
   basic block BB.  */

hsa_bb *
hsa_init_new_bb (basic_block bb)
{
  return new (hsa_allocp_bb) hsa_bb (bb);
}

/* Initialize OMP in an HSA basic block PROLOGUE.  */

static void
init_omp_in_prologue (void)
{
  if (!hsa_cfun->kern_p)
    return;

  hsa_bb *prologue = hsa_bb_for_bb (ENTRY_BLOCK_PTR_FOR_FN (cfun));

  /* Load a default value from shadow argument.  */
  hsa_op_reg *shadow_reg_ptr = hsa_cfun->get_shadow_reg ();
  hsa_op_address *addr = new hsa_op_address
    (shadow_reg_ptr, offsetof (hsa_kernel_dispatch, omp_num_threads));

  hsa_op_reg *threads = new hsa_op_reg (BRIG_TYPE_U32);
  hsa_insn_basic *basic = new hsa_insn_mem
    (BRIG_OPCODE_LD, threads->type, threads, addr);
  prologue->append_insn (basic);

  /* Save it to private variable hsa_num_threads.  */
  basic = new hsa_insn_mem (BRIG_OPCODE_ST, hsa_num_threads->type, threads,
			    new hsa_op_address (hsa_num_threads));
  prologue->append_insn (basic);

  /* Create a magic number that is going to be printed by libgomp.  */
  unsigned index = hsa_get_number_decl_kernel_mappings ();

  /* Emit store to debug argument.  */
  set_debug_value (prologue, new hsa_op_immed (1000 + index, BRIG_TYPE_U64));
}

/* Go over gimple representation and generate our internal HSA one.  SSA_MAP
   maps gimple SSA names to HSA pseudo registers.  */

static void
gen_body_from_gimple (vec <hsa_op_reg_p> *ssa_map)
{
  basic_block bb;

  /* Verify CFG for complex edges we are unable to handle.  */
  edge_iterator ei;
  edge e;

  FOR_EACH_BB_FN (bb, cfun)
    {
      FOR_EACH_EDGE (e, ei, bb->succs)
	{
	  /* Verify all unsupported flags for edges that point
	     to the same basic block.  */
	  if (e->flags & EDGE_EH)
	    {
	      HSA_SORRY_AT
		(UNKNOWN_LOCATION,
		 "support for HSA does not implement exception handling");
	      return;
	    }
	}
    }

  FOR_EACH_BB_FN (bb, cfun)
    {
      gimple_stmt_iterator gsi;
      hsa_bb *hbb = hsa_init_new_bb (bb);

      for (gsi = gsi_start_bb (bb); !gsi_end_p (gsi); gsi_next (&gsi))
	{
	  gen_hsa_insns_for_gimple_stmt (gsi_stmt (gsi), hbb, ssa_map);
	  if (hsa_seen_error ())
	    return;
	}
    }

  FOR_EACH_BB_FN (bb, cfun)
    {
      gimple_stmt_iterator gsi;
      hsa_bb *hbb = hsa_bb_for_bb (bb);

      for (gsi = gsi_start_phis (bb); !gsi_end_p (gsi); gsi_next (&gsi))
	if (!virtual_operand_p (gimple_phi_result (gsi_stmt (gsi))))
	  gen_hsa_phi_from_gimple_phi (gsi_stmt (gsi), hbb, ssa_map);
    }

  if (dump_file)
    {
      fprintf (dump_file, "------- Generated SSA form -------\n");
      dump_hsa_cfun (dump_file);
    }
}

/* For a function DECL, get number of arguments.  */

static unsigned
get_function_arg_count (tree decl)
{
  unsigned count = 0;

  for (tree parm = TYPE_ARG_TYPES (TREE_TYPE (decl)); parm;
       parm = TREE_CHAIN (parm))
    count++;

  /* Return type is the last element of tree list.  */
  return count - 1;
}

static void
gen_function_decl_parameters (hsa_function_representation *f,
			      tree decl)
{
  tree parm;
  unsigned i;

  f->input_args_count = get_function_arg_count (decl);
  f->input_args = XCNEWVEC (hsa_symbol, f->input_args_count);
  for (parm = TYPE_ARG_TYPES (TREE_TYPE (decl)), i = 0;
       parm;
       parm = TREE_CHAIN (parm), i++)
    {
      /* Result type if last in the tree list.  */
      if (i == f->input_args_count)
	break;

      tree v = TREE_VALUE (parm);
      f->input_args[i].type = hsa_type_for_tree_type (v, &f->input_args[i].dim);
      f->input_args[i].segment = BRIG_SEGMENT_ARG;
      f->input_args[i].linkage = BRIG_LINKAGE_NONE;
      f->input_args[i].name_number = i;
    }

  tree result_type = TREE_TYPE (TREE_TYPE (decl));
  if (!VOID_TYPE_P (result_type))
    {
      f->output_arg = XCNEW (hsa_symbol);
      f->output_arg->type = hsa_type_for_tree_type (result_type,
						   &f->output_arg->dim);
      f->output_arg->segment = BRIG_SEGMENT_ARG;
      f->output_arg->linkage = BRIG_LINKAGE_NONE;
      f->output_arg->name = "res";
    }
}

/* Generate the vector of parameters of the HSA representation of the current
   function.  This also includes the output parameter representing the
   result.  */

static void
gen_function_def_parameters (hsa_function_representation *f,
			     vec <hsa_op_reg_p> *ssa_map)
{
  tree parm;
  int i, count = 0;

  for (parm = DECL_ARGUMENTS (cfun->decl); parm; parm = DECL_CHAIN (parm))
    count++;

  f->input_args_count = count;

  /* Allocate one more argument which can be potentially used for a kernel
     dispatching.  */
  f->input_args = XCNEWVEC (hsa_symbol, f->input_args_count + 1);

  hsa_bb *prologue = hsa_bb_for_bb (ENTRY_BLOCK_PTR_FOR_FN (cfun));

  for (parm = DECL_ARGUMENTS (cfun->decl), i = 0;
       parm;
       parm = DECL_CHAIN (parm), i++)
    {
      struct hsa_symbol **slot;

      fillup_sym_for_decl (parm, &f->input_args[i]);
      f->input_args[i].segment = f->kern_p ? BRIG_SEGMENT_KERNARG :
				       BRIG_SEGMENT_ARG;

      f->input_args[i].linkage = BRIG_LINKAGE_FUNCTION;
      f->input_args[i].name = hsa_get_declaration_name (parm);

      slot = f->local_symbols->find_slot (&f->input_args[i],
						INSERT);
      gcc_assert (!*slot);
      *slot = &f->input_args[i];

      if (is_gimple_reg (parm))
	{
	  tree ddef = ssa_default_def (cfun, parm);
	  if (ddef && !has_zero_uses (ddef))
	    {
	      BrigType16_t mtype = mem_type_for_type
		(hsa_type_for_scalar_tree_type (TREE_TYPE (ddef), false));
	      hsa_op_reg *dest = hsa_reg_for_gimple_ssa (ddef, ssa_map);
	      hsa_op_address *addr;

	      addr = gen_hsa_addr (parm, prologue, ssa_map);
	      hsa_insn_mem *mem = new hsa_insn_mem (BRIG_OPCODE_LD, mtype,
						    dest, addr);
	      gcc_assert (!addr->reg);
	      prologue->append_insn (mem);
	    }
	}
    }

  if (!VOID_TYPE_P (TREE_TYPE (TREE_TYPE (cfun->decl))))
    {
      struct hsa_symbol **slot;

      f->output_arg = XCNEW (hsa_symbol);
      fillup_sym_for_decl (DECL_RESULT (cfun->decl), f->output_arg);
      f->output_arg->segment = BRIG_SEGMENT_ARG;
      f->output_arg->linkage = BRIG_LINKAGE_FUNCTION;
      f->output_arg->name = "res";
      slot = f->local_symbols->find_slot (f->output_arg, INSERT);
      gcc_assert (!*slot);
      *slot = f->output_arg;
    }
}

/* Generate function representation that corresponds to
   a function declaration.  */

hsa_function_representation *
hsa_generate_function_declaration (tree decl)
{
  hsa_function_representation *fun = XCNEW (hsa_function_representation);

  fun->declaration_p = true;
  fun->decl = decl;
  fun->name = xstrdup (hsa_get_declaration_name (decl));
  hsa_sanitize_name (fun->name);

  gen_function_decl_parameters (fun, decl);

  return fun;
}

/* Return true if switch statement S can be transformed
   to a SBR instruction in HSAIL.  */

static bool
transformable_switch_to_sbr_p (gswitch *s)
{
  /* Identify if a switch statement can be transformed to
     SBR instruction, like:

     sbr_u32 $s1 [@label1, @label2, @label3];
  */

  tree size = get_switch_size (s);
  if (!tree_fits_uhwi_p (size))
    return false;

  if (tree_to_uhwi (size) > HSA_MAXIMUM_SBR_LABELS)
    return false;

  return true;
}

/* Structure hold connection between PHI nodes and immediate
   values hold by there nodes.  */

struct phi_definition
{
  phi_definition (unsigned phi_i, unsigned label_i, tree imm):
    phi_index (phi_i), label_index (label_i), phi_value (imm)
  {}

  unsigned phi_index;
  unsigned label_index;
  tree phi_value;
};

/* Function transforms GIMPLE SWITCH statements to a series of IF statements.
   Let's assume following example:

L0:
   switch (index)
     case C1:
L1:    hard_work_1 ();
       break;
     case C2..C3:
L2:    hard_work_2 ();
       break;
     default:
LD:    hard_work_3 ();
       break;

  The tranformation encompases following steps:
    1) all immediate values used by edges coming from the switch basic block
       are saved
    2) all these edges are removed
    3) the switch statement (in L0) is replaced by:
	 if (index == C1)
	   goto L1;
	 else
	   goto L1';

    4) newly created basic block Lx' is used for generation of
       a next condition
    5) else branch of the last condition goes to LD
    6) fix all immediate values in PHI nodes that were propagated though
       edges that were removed in step 2

  Note: if a case is made by a range C1..C2, then process
	following transformation:

  switch_cond_op1 = C1 <= index;
  switch_cond_op2 = index <= C2;
  switch_cond_and = switch_cond_op1 & switch_cond_op2;
  if (switch_cond_and != 0)
    goto Lx;
  else
    goto Ly;

*/

static void
convert_switch_statements ()
{
  function *func = DECL_STRUCT_FUNCTION (current_function_decl);
  basic_block bb;
  push_cfun (func);

  bool need_update = false;

  FOR_EACH_BB_FN (bb, func)
  {
    gimple_stmt_iterator gsi = gsi_last_bb (bb);
    if (gsi_end_p (gsi))
      continue;

    gimple *stmt = gsi_stmt (gsi);

    if (gimple_code (stmt) == GIMPLE_SWITCH)
      {
	gswitch *s = as_a <gswitch *> (stmt);

	/* If the switch can utilize SBR insn, skip the statement.  */
	if (transformable_switch_to_sbr_p (s))
	  continue;

	need_update = true;

	unsigned labels = gimple_switch_num_labels (s);
	tree index = gimple_switch_index (s);
	tree default_label = gimple_switch_default_label (s);
	basic_block default_label_bb = label_to_block_fn
	  (func, CASE_LABEL (default_label));
	basic_block cur_bb = bb;

	auto_vec <edge> new_edges;
	auto_vec <phi_definition *> phi_todo_list;

	/* Investigate all labels that and PHI nodes in these edges which
	   should be fixed after we add new collection of edges.  */
	for (unsigned i = 0; i < labels; i++)
	  {
	    tree label = gimple_switch_label (s, i);
	    basic_block label_bb = label_to_block_fn (func, CASE_LABEL (label));
	    edge e = find_edge (bb, label_bb);
	    gphi_iterator phi_gsi;

	    /* Save PHI definitions that will be destroyed because of an edge
	       is going to be removed.  */
	    unsigned phi_index = 0;
	    for (phi_gsi = gsi_start_phis (e->dest);
		 !gsi_end_p (phi_gsi); gsi_next (&phi_gsi))
	      {
		gphi *phi = phi_gsi.phi ();
		for (unsigned j = 0; j < gimple_phi_num_args (phi); j++)
		  {
		    if (gimple_phi_arg_edge (phi, j) == e)
		      {
			tree imm = gimple_phi_arg_def (phi, j);
			phi_todo_list.safe_push
			  (new phi_definition (phi_index, i, imm));
			break;
		      }
		  }
		phi_index++;
	      }
	  }

	/* Remove all edges for the current basic block.  */
	for (int i = EDGE_COUNT (bb->succs) - 1; i >= 0; i--)
 	  {
	    edge e = EDGE_SUCC (bb, i);
	    remove_edge (e);
	  }

	/* Iterate all non-default labels.  */
	for (unsigned i = 1; i < labels; i++)
	  {
	    tree label = gimple_switch_label (s, i);
	    tree low = CASE_LOW (label);
	    tree high = CASE_HIGH (label);

	    gimple_stmt_iterator cond_gsi = gsi_last_bb (cur_bb);
	    gimple *c = NULL;
	    if (high)
	      {
		tree tmp1 = make_temp_ssa_name (boolean_type_node, NULL,
						"switch_cond_op1");
		gimple *assign1 = gimple_build_assign (tmp1, LE_EXPR, low,
						      index);

		tree tmp2 = make_temp_ssa_name (boolean_type_node, NULL,
						"switch_cond_op2");
		gimple *assign2 = gimple_build_assign (tmp2, LE_EXPR, index,
						      high);

		tree tmp3 = make_temp_ssa_name (boolean_type_node, NULL,
						"switch_cond_and");
		gimple *assign3 = gimple_build_assign (tmp3, BIT_AND_EXPR, tmp1,
						      tmp2);

		gsi_insert_before (&cond_gsi, assign1, GSI_SAME_STMT);
		gsi_insert_before (&cond_gsi, assign2, GSI_SAME_STMT);
		gsi_insert_before (&cond_gsi, assign3, GSI_SAME_STMT);

		c = gimple_build_cond (NE_EXPR, tmp3, constant_boolean_node
				       (false, boolean_type_node), NULL, NULL);
	      }
	    else
	      c = gimple_build_cond (EQ_EXPR, index, low, NULL, NULL);

	    gimple_set_location (c, gimple_location (stmt));

	    gsi_insert_before (&cond_gsi, c, GSI_SAME_STMT);

	    basic_block label_bb = label_to_block_fn
	      (func, CASE_LABEL (label));
	    edge new_edge = make_edge (cur_bb, label_bb, EDGE_TRUE_VALUE);
	    new_edges.safe_push (new_edge);

	    if (i < labels - 1)
	      {
		/* Prepare another basic block that will contain
		   next condition.  */
		basic_block next_bb = create_empty_bb (cur_bb);
		if (current_loops)
		  {
		    add_bb_to_loop (next_bb, cur_bb->loop_father);
		    loops_state_set (LOOPS_NEED_FIXUP);
		  }

		make_edge (cur_bb, next_bb, EDGE_FALSE_VALUE);
		cur_bb = next_bb;
	      }
	    else /* Link last IF statement and default label
		    of the switch.  */
	      {
		edge e = make_edge (cur_bb, default_label_bb, EDGE_FALSE_VALUE);
		new_edges.safe_insert (0, e);
	      }
	  }

	  /* Restore original PHI immediate value.  */
	  for (unsigned i = 0; i < phi_todo_list.length (); i++)
	    {
	      phi_definition *phi_def = phi_todo_list[i];
	      edge new_edge = new_edges[phi_def->label_index];

	      gphi_iterator it = gsi_start_phis (new_edge->dest);
	      for (unsigned i = 0; i < phi_def->phi_index; i++)
		gsi_next (&it);

	      gphi *phi = it.phi ();
	      add_phi_arg (phi, phi_def->phi_value, new_edge, UNKNOWN_LOCATION);
	    }

	/* Remove the original GIMPLE switch statement.  */
	gsi_remove (&gsi, true);
      }
  }

  if (dump_file)
    dump_function_to_file (current_function_decl, dump_file, TDF_DETAILS);

  if (need_update)
    {
      free_dominance_info (CDI_DOMINATORS);
      calculate_dominance_info (CDI_DOMINATORS);
    }
}

/* Emit HSA module variables that are global for the entire module.  */

static void
emit_hsa_module_variables (void)
{
  hsa_num_threads = new hsa_symbol ();
  memset (hsa_num_threads, 0, sizeof (hsa_symbol));

  hsa_num_threads->name = "hsa_num_threads";
  hsa_num_threads->type = BRIG_TYPE_U32;
  hsa_num_threads->segment = BRIG_SEGMENT_PRIVATE;
  hsa_num_threads->linkage = BRIG_LINKAGE_MODULE;
  hsa_num_threads->global_scope_p = true;

  hsa_brig_emit_omp_symbols ();
}

/* Generate HSAIL representation of the current function and write into a
   special section of the output file.  If KERNEL is set, the function will be
   considered an HSA kernel callable from the host, otherwise it will be
   compiled as an HSA function callable from other HSA code.  */

static void
generate_hsa (bool kernel)
{
  vec <hsa_op_reg_p> ssa_map = vNULL;

  if (hsa_num_threads == NULL)
    emit_hsa_module_variables ();

  hsa_init_data_for_cfun ();
  verify_function_arguments (cfun->decl);
  if (hsa_seen_error ())
    goto fail;

  hsa_cfun->decl = cfun->decl;
  hsa_cfun->kern_p = kernel;

  ssa_map.safe_grow_cleared (SSANAMES (cfun)->length ());
  hsa_cfun->name
    = xstrdup (hsa_get_declaration_name (current_function_decl));
  hsa_sanitize_name (hsa_cfun->name);

  gen_function_def_parameters (hsa_cfun, &ssa_map);
  if (hsa_seen_error ())
    goto fail;

  init_omp_in_prologue ();

  gen_body_from_gimple (&ssa_map);
  if (hsa_seen_error ())
    goto fail;

  if (hsa_cfun->kern_p)
    {
      hsa_add_kern_decl_mapping (current_function_decl, hsa_cfun->name,
				 hsa_cfun->maximum_omp_data_size);
    }

#ifdef ENABLE_CHECKING
  for (unsigned i = 0; i < ssa_map.length (); i++)
    if (ssa_map[i])
      ssa_map[i]->verify_ssa ();

  basic_block bb;
  FOR_EACH_BB_FN (bb, cfun)
    {
      hsa_bb *hbb = hsa_bb_for_bb (bb);

      for (hsa_insn_basic *insn = hbb->first_insn; insn; insn = insn->next)
	insn->verify ();
    }

#endif

  ssa_map.release ();

  hsa_regalloc ();

  hsa_brig_emit_function ();

 fail:
  hsa_deinit_data_for_cfun ();
}

namespace {

const pass_data pass_data_gen_hsail =
{
  GIMPLE_PASS,
  "hsagen",	 			/* name */
  OPTGROUP_NONE,                        /* optinfo_flags */
  TV_NONE,				/* tv_id */
  PROP_cfg | PROP_ssa,                  /* properties_required */
  0,					/* properties_provided */
  0,					/* properties_destroyed */
  0,					/* todo_flags_start */
  0					/* todo_flags_finish */
};

class pass_gen_hsail : public gimple_opt_pass
{
public:
  pass_gen_hsail (gcc::context *ctxt)
    : gimple_opt_pass(pass_data_gen_hsail, ctxt)
  {}

  /* opt_pass methods: */
  bool gate (function *);
  unsigned int execute (function *);

}; // class pass_gen_hsail

/* Determine whether or not to run generation of HSAIL.  */

bool
pass_gen_hsail::gate (function *f)
{
  return hsa_gen_requested_p ()
    && hsa_gpu_implementation_p (f->decl);
}

unsigned int
pass_gen_hsail::execute (function *)
{
  hsa_function_summary *s = hsa_summaries->get
    (cgraph_node::get_create (current_function_decl));

  convert_switch_statements ();
  generate_hsa (s->kind == HSA_KERNEL);
  TREE_ASM_WRITTEN (current_function_decl) = 1;
  return TODO_stop_pass_execution;
}

} // anon namespace

/* Create the instance of hsa gen pass.  */

gimple_opt_pass *
make_pass_gen_hsail (gcc::context *ctxt)
{
  return new pass_gen_hsail (ctxt);
}
