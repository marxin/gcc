/* Producing binary form of HSA BRIG from our internal representation.
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

#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "tm.h"
#include "defaults.h"
#include "hard-reg-set.h"
#include "hsa.h"
#include "tree.h"
#include "stor-layout.h"
#include "tree-cfg.h"
#include "machmode.h"
#include "output.h"
#include "dominance.h"
#include "cfg.h"
#include "function.h"
#include "basic-block.h"
#include "vec.h"
#include "gimple-pretty-print.h"
#include "diagnostic-core.h"
#include "hash-map.h"

#define BRIG_SECTION_DATA_NAME    "hsa_data"
#define BRIG_SECTION_CODE_NAME    "hsa_code"
#define BRIG_SECTION_OPERAND_NAME "hsa_operand"

#define BRIG_CHUNK_MAX_SIZE (64 * 1024)

/* FIXME: The code below uses endian.h routines to convert numbers to
   little-endian.  I suspect this only works on glibc platforms, so we might
   need an alternative solutin later.  */

/* Chunks of BRIG binary data.  */

struct hsa_brig_data_chunk
{
  /* Size of the data already stored into a chunk.  */
  unsigned size;

  /* Pointer to the data.  */
  char *data;
};

/* Structure represeting a BRIC section, holding and writing its data.  */

class hsa_brig_section
{
public:
  /* Section name that will be output to the BRIG.  */
  const char *section_name;
  /* Size in bytes of all data stored in the section.  */
  unsigned total_size, header_byte_count;

  /* Buffers of binary data, each containing BRIG_CHUNK_MAX_SIZE bytes.  */
  vec <struct hsa_brig_data_chunk> chunks;

  /* More convenient access to the last chunk from the vector above. */
  struct hsa_brig_data_chunk *cur_chunk;

  void allocate_new_chunk ();
  void init (const char *name);
  void release ();
  void output ();
  unsigned add (const void *data, unsigned len);
  void round_size_up (int factor);
  void *get_ptr_by_offset (unsigned int offset);
};

static struct hsa_brig_section brig_data, brig_code, brig_operand;
static uint32_t brig_insn_count;
static bool brig_initialized = false;

/* Mapping between emitted HSA functions and their offset in code segment.  */
static hash_map <tree, BrigCodeOffset32_t> function_offsets;

struct function_linkage_pair
{
  function_linkage_pair (tree decl, unsigned int off):
    function_decl (decl), offset (off) {}

  /* Declaration of called function.  */
  tree function_decl;

  /* Offset in operand section.  */
  unsigned int offset;
};

/* Vector of function calls where we need to resolve function offsets.  */
static auto_vec <function_linkage_pair> function_call_linkage;

/* Add a new chunk, allocate data for it and initialize it.  */

void
hsa_brig_section::allocate_new_chunk ()
{
  struct hsa_brig_data_chunk new_chunk;

  new_chunk.data = XCNEWVEC (char, BRIG_CHUNK_MAX_SIZE);
  new_chunk.size = 0;
  cur_chunk = chunks.safe_push (new_chunk);
}

/* Initialize the brig section.  */

void
hsa_brig_section::init (const char *name)
{
  struct BrigSectionHeader sample;

  section_name = name;
  total_size = sizeof(sample.byteCount) + sizeof(sample.headerByteCount)
        + sizeof(sample.nameLength);
  /* Add strlen + null termination to the section size*/
  total_size = total_size + strlen(section_name) + 1;
  chunks.create (1);
  allocate_new_chunk ();
  round_size_up (4);
  header_byte_count = total_size;
}

/* Free all data in the section.  */

void
hsa_brig_section::release ()
{
  for (unsigned i = 0; i < chunks.length (); i++)
    free (chunks[i].data);
  chunks.release ();
  cur_chunk = NULL;
}

/* Write the section to the output file to a section with the name given at
   initialization.  Switches the output section and does not restore it.  */

void
hsa_brig_section::output ()
{
  struct BrigSectionHeader section_header;

  switch_to_section (get_section (section_name, SECTION_NOTYPE, NULL));

  section_header.byteCount = htole32 (total_size);
  section_header.nameLength = htole32 (strlen(section_name));
  section_header.headerByteCount = htole32 (header_byte_count);
  assemble_string ((const char*) &section_header, 12);
  assemble_string (section_name, (section_header.nameLength + 1));
  for (unsigned i = 0; i < chunks.length (); i++)
    assemble_string (chunks[i].data, chunks[i].size);
}

/* Add to the stream LEN bytes of opaque binary DATA.  Return the offset at
   which it was stored.  */

unsigned
hsa_brig_section::add (const void *data, unsigned len)
{
  unsigned offset = total_size;

  gcc_assert (len <= BRIG_CHUNK_MAX_SIZE);
  if (cur_chunk->size > (BRIG_CHUNK_MAX_SIZE - len))
    allocate_new_chunk ();

  memcpy (cur_chunk->data + cur_chunk->size, data, len);
  cur_chunk->size += len;
  total_size += len;

  return offset;
}

/* Add padding to section so that its size is divisble by FACTOR.  */

void
hsa_brig_section::round_size_up (int factor)
{
  unsigned padding, res = total_size % factor;

  if (res == 0)
    return;

  padding = factor - res;
  total_size += padding;
  if (cur_chunk->size > (BRIG_CHUNK_MAX_SIZE - padding))
    {
      padding -= BRIG_CHUNK_MAX_SIZE - cur_chunk->size;
      cur_chunk->size = BRIG_CHUNK_MAX_SIZE;
      allocate_new_chunk ();
    }

  cur_chunk->size += padding;
}

/* Return pointer to data by global OFFSET in the section.  */

void*
hsa_brig_section::get_ptr_by_offset (unsigned int offset)
{
  gcc_assert (offset < total_size);

  offset -= header_byte_count;
  unsigned int i;

  for (i = 0; offset >= chunks[i].size; i++)
    offset -= chunks[i].size;

  return chunks[i].data + offset;
}


/* BRIG string data hashing.  */

struct brig_string_slot
{
  const char *s;
  char prefix;
  int len;
  uint32_t offset;
};

/* Hashtable helpers.  */

struct brig_string_slot_hasher
{
  typedef brig_string_slot value_type;
  typedef brig_string_slot compare_type;
  static inline hashval_t hash (const value_type *);
  static inline bool equal (const value_type *, const compare_type *);
  static inline void remove (value_type *);
};

/* Returns a hash code for DS.  Adapted from libiberty's htab_hash_string
   to support strings that may not end in '\0'.  */

inline hashval_t
brig_string_slot_hasher::hash (const value_type *ds)
{
  hashval_t r = ds->len;
  int i;

  for (i = 0; i < ds->len; i++)
     r = r * 67 + (unsigned)ds->s[i] - 113;
  r = r * 67 + (unsigned)ds->prefix - 113;
  return r;
}

/* Returns nonzero if DS1 and DS2 are equal.  */

inline bool
brig_string_slot_hasher::equal (const value_type *ds1, const compare_type *ds2)
{
  if (ds1->len == ds2->len)
    return ds1->prefix == ds2->prefix && memcmp (ds1->s, ds2->s, ds1->len) == 0;

  return 0;
}

inline void
brig_string_slot_hasher::remove (value_type *ds)
{
  free (const_cast<char*> (ds->s));
}

static hash_table<brig_string_slot_hasher> *brig_string_htab;

static void
sanitize_hsa_name (char *p)
{
  for (; *p; p++)
    if (*p == '.')
      *p = '_';
}

/* Emit a null terminated string STR to the data section and return its
   offset in it.  If PREFIX is non-zero, output it just before STR too.  */

static unsigned
brig_emit_string (const char *str, char prefix = 0)
{
  unsigned slen = strlen (str);
  unsigned offset, len = slen + (prefix ? 1 : 0);
  uint32_t hdr_len = htole32 (len);
  brig_string_slot s_slot;
  brig_string_slot **slot;
  char *str2;

  /* XXX Sanitize the names without all the strdup.  */
  str2 = xstrdup (str);
  sanitize_hsa_name (str2);
  s_slot.s = str2;
  s_slot.len = slen;
  s_slot.prefix = prefix;
  s_slot.offset = 0;

  slot = brig_string_htab->find_slot (&s_slot, INSERT);
  if (*slot == NULL)
    {
      brig_string_slot *new_slot = XCNEW (brig_string_slot);

      /* In theory we should fill in BrigData but that would mean copying
         the string to a buffer for no reason, so we just emaulate it. */
      offset = brig_data.add (&hdr_len, sizeof (hdr_len));
      if (prefix)
        brig_data.add (&prefix, 1);

      brig_data.add (str2, slen);
      brig_data.round_size_up (4);

      /* XXX could use the string we just copied into brig_string->cur_chunk */
      new_slot->s = str2;
      new_slot->len = slen;
      new_slot->prefix = prefix;
      new_slot->offset = offset;
      *slot = new_slot;
    }
  else
    {
      offset = (*slot)->offset;
      free (str2);
    }

  return offset;
}

/* Linked list of queued operands.  */

static struct operand_queue
{
  /* First from the chain of queued operands.  */
  hsa_op_base *first_op, *last_op;

  /* The offset at which the next operand will be enqueued.  */
  unsigned projected_size;

} op_queue;

/* Unless already initialized, initialzie infrastructure to produce BRIG.  */

static void
brig_init (void)
{
  struct BrigDirectiveVersion verdir;
  brig_insn_count = 0;

  if (brig_initialized)
    return;

  brig_string_htab = new hash_table<brig_string_slot_hasher> (37);
  brig_data.init (BRIG_SECTION_DATA_NAME);
  brig_code.init (BRIG_SECTION_CODE_NAME);
  brig_operand.init (BRIG_SECTION_OPERAND_NAME);

  verdir.base.byteCount = htole16 (sizeof (verdir));
  verdir.base.kind = htole16 (BRIG_KIND_DIRECTIVE_VERSION);
  verdir.hsailMajor = htole32 (BRIG_VERSION_HSAIL_MAJOR) ;
  verdir.hsailMinor =  htole32 (BRIG_VERSION_HSAIL_MINOR);
  verdir.brigMajor = htole32 (BRIG_VERSION_BRIG_MAJOR);
  verdir.brigMinor = htole32 (BRIG_VERSION_BRIG_MINOR);
  verdir.profile = hsa_full_profile_p () ? BRIG_PROFILE_FULL: BRIG_PROFILE_BASE;
  if (hsa_machine_large_p ())
    verdir.machineModel = BRIG_MACHINE_LARGE;
  else
    verdir.machineModel = BRIG_MACHINE_SMALL;
  verdir.reserved = 0;
  brig_code.add (&verdir, sizeof (verdir));
  brig_initialized = true;
}

/* Free all BRIG data.  */

static void
brig_release_data (void)
{
  delete brig_string_htab;
  brig_data.release ();
  brig_code.release ();
  brig_operand.release ();

  brig_initialized = 0;
}

/* Find the alignment base on the type.  */

static BrigAlignment8_t
get_alignment (BrigType16_t type)
{
  BrigType16_t bit_type ;
  bit_type = bittype_for_type (type) ;

  if (bit_type == BRIG_TYPE_B1)
    return BRIG_ALIGNMENT_1;
  if (bit_type == BRIG_TYPE_B8)
    return BRIG_ALIGNMENT_1;
  if (bit_type == BRIG_TYPE_B16)
    return BRIG_ALIGNMENT_2;
  if (bit_type == BRIG_TYPE_B32)
    return BRIG_ALIGNMENT_4;
  if (bit_type == BRIG_TYPE_B64)
    return BRIG_ALIGNMENT_8;
  if (bit_type == BRIG_TYPE_B128)
    return BRIG_ALIGNMENT_16;
  gcc_unreachable ();
}

/* Emit directive describing a symbol if it has not been emitted already.
   Return the offset of the directive.  */

static unsigned
emit_directive_variable (struct hsa_symbol *symbol)
{
  struct BrigDirectiveVariable dirvar;
  unsigned name_offset;
  static unsigned res_name_offset;
  char prefix;

  if (symbol->directive_offset)
    return symbol->directive_offset;

  dirvar.base.byteCount = htole16 (sizeof (dirvar));
  dirvar.base.kind = htole16 (BRIG_KIND_DIRECTIVE_VARIABLE);
  dirvar.allocation = BRIG_ALLOCATION_AUTOMATIC;

  if (symbol->decl && is_global_var (symbol->decl))
    {
      prefix = '&';
      dirvar.allocation = BRIG_ALLOCATION_PROGRAM ;
      if (TREE_CODE (symbol->decl) == VAR_DECL)
	warning (0, "referring to global symbol %q+D by name from HSA code won't work", symbol->decl);
    }
  else
    prefix = '%';

  if (symbol->decl && TREE_CODE (symbol->decl) == RESULT_DECL)
    {
      if (res_name_offset == 0)
	res_name_offset = brig_emit_string (symbol->name, '%');
      name_offset = res_name_offset;
    }
  else if (symbol->name)
    name_offset = brig_emit_string (symbol->name, prefix);
  else
    {
      char buf[64];
      sprintf (buf, "__%s_%i", hsa_seg_name (symbol->segment),
	       symbol->name_number);
      name_offset = brig_emit_string (buf, prefix);
    }

  dirvar.name = htole32 (name_offset);
  dirvar.init = 0;
  dirvar.type = htole16 (symbol->type);
  dirvar.segment = symbol->segment;
  dirvar.align = get_alignment (dirvar.type);
  gcc_assert (symbol->linkage);
  dirvar.linkage = symbol->linkage;
  dirvar.dim.lo = htole32 (symbol->dimLo);
  dirvar.dim.hi = htole32 (symbol->dimHi);
  dirvar.modifier = BRIG_SYMBOL_DEFINITION;
  dirvar.reserved = 0;

  symbol->directive_offset = brig_code.add (&dirvar, sizeof (dirvar));
  return symbol->directive_offset;
}

/* Emit directives describing the function, for example its input and output
   arguments, local variables etc.  */

static BrigDirectiveExecutable *
emit_function_directives (void)
{
  struct BrigDirectiveExecutable fndir;
  unsigned name_offset, inarg_off, scoped_off, next_toplev_off;
  int count = 0;
  BrigDirectiveExecutable *ptr_to_fndir;
  hsa_symbol *sym;

  name_offset = brig_emit_string (hsa_cfun.name, '&');
  inarg_off = brig_code.total_size + sizeof(fndir)
    + (hsa_cfun.output_arg ? sizeof (struct BrigDirectiveVariable) : 0);
  scoped_off = inarg_off
    + hsa_cfun.input_args_count * sizeof (struct BrigDirectiveVariable);
  for (hash_table <hsa_noop_symbol_hasher>::iterator iter
	 = hsa_cfun.local_symbols->begin ();
       iter != hsa_cfun.local_symbols->end ();
       ++iter)
    if (TREE_CODE ((*iter)->decl) == VAR_DECL)
      count++;
  count += hsa_cfun.spill_symbols.length ();

  next_toplev_off = scoped_off + count * sizeof (struct BrigDirectiveVariable);

  fndir.base.byteCount = htole16 (sizeof (fndir));
  fndir.base.kind = htole16 (hsa_cfun.kern_p ? BRIG_KIND_DIRECTIVE_KERNEL : BRIG_KIND_DIRECTIVE_FUNCTION);
  fndir.name = htole32 (name_offset);
  fndir.inArgCount = htole16 (hsa_cfun.input_args_count);
  fndir.outArgCount = htole16 (hsa_cfun.output_arg ? 1 : 0);
  fndir.firstInArg = htole32 (inarg_off);
  fndir.firstCodeBlockEntry = htole32 (scoped_off);
  fndir.nextModuleEntry = htole32 (next_toplev_off);
  fndir.linkage = BRIG_LINKAGE_PROGRAM;
  fndir.codeBlockEntryCount = htole32 (0);
  fndir.modifier = BRIG_EXECUTABLE_DEFINITION;
  memset (&fndir.reserved, 0, sizeof (fndir.reserved));

  function_offsets.put (cfun->decl, brig_code.total_size);

  brig_code.add (&fndir, sizeof (fndir));
  /* XXX terrible hack: we need to set instCount after we emit all
     insns, but we need to emit directive in order, and we emit directives
     during insn emitting.  So we need to emit the FUNCTION directive
     early, then the insns, and then we need to set instCount, so remember
     a pointer to it, in some horrible way.  cur_chunk.data+size points
     directly to after fndir here.  */
  ptr_to_fndir
      = (BrigDirectiveExecutable *)(brig_code.cur_chunk->data
                                    + brig_code.cur_chunk->size
                                    - sizeof (fndir));

  if (hsa_cfun.output_arg)
    emit_directive_variable(hsa_cfun.output_arg);
  for (int i = 0; i < hsa_cfun.input_args_count; i++)
    emit_directive_variable(&hsa_cfun.input_args[i]);
  for (hash_table <hsa_noop_symbol_hasher>::iterator iter
	 = hsa_cfun.local_symbols->begin ();
       iter != hsa_cfun.local_symbols->end ();
       ++iter)
    {
      if (TREE_CODE ((*iter)->decl) == VAR_DECL)
	brig_insn_count++;
      emit_directive_variable(*iter);
    }
  for (int i = 0; hsa_cfun.spill_symbols.iterate (i, &sym); i++)
    {
      emit_directive_variable (sym);
      brig_insn_count++;
    }

  return ptr_to_fndir;
}

/* Emit a label directive for the given HBB.  We assume it is about to start on
   the current offset in the code section.  */

static void
emit_bb_label_directive (hsa_bb *hbb)
{
  struct BrigDirectiveLabel lbldir;
  char buf[32];

  lbldir.base.byteCount = htole16 (sizeof (lbldir));
  lbldir.base.kind = htole16 (BRIG_KIND_DIRECTIVE_LABEL);
  sprintf (buf, "BB_%u_%i", DECL_UID (current_function_decl), hbb->index);
  lbldir.name = htole32 (brig_emit_string (buf, '@'));

  hbb->label_ref.directive_offset = brig_code.add (&lbldir, sizeof (lbldir));
  brig_insn_count++;
}

BrigType16_t
bittype_for_type (BrigType16_t t)
{
  switch (t)
    {
    case BRIG_TYPE_B1:
      return BRIG_TYPE_B1;

    case BRIG_TYPE_U8:
    case BRIG_TYPE_S8:
    case BRIG_TYPE_B8:
      return BRIG_TYPE_B8;

    case BRIG_TYPE_U16:
    case BRIG_TYPE_S16:
    case BRIG_TYPE_B16:
    case BRIG_TYPE_F16:
      return BRIG_TYPE_B16;

    case BRIG_TYPE_U32:
    case BRIG_TYPE_S32:
    case BRIG_TYPE_B32:
    case BRIG_TYPE_F32:
    case BRIG_TYPE_U8X4:
    case BRIG_TYPE_U16X2:
    case BRIG_TYPE_S8X4:
    case BRIG_TYPE_S16X2:
    case BRIG_TYPE_F16X2:
      return BRIG_TYPE_B32;

    case BRIG_TYPE_U64:
    case BRIG_TYPE_S64:
    case BRIG_TYPE_F64:
    case BRIG_TYPE_B64:
    case BRIG_TYPE_U8X8:
    case BRIG_TYPE_U16X4:
    case BRIG_TYPE_U32X2:
    case BRIG_TYPE_S8X8:
    case BRIG_TYPE_S16X4:
    case BRIG_TYPE_S32X2:
    case BRIG_TYPE_F16X4:
    case BRIG_TYPE_F32X2:

      return BRIG_TYPE_B64;

    case BRIG_TYPE_B128:
    case BRIG_TYPE_U8X16:
    case BRIG_TYPE_U16X8:
    case BRIG_TYPE_U32X4:
    case BRIG_TYPE_U64X2:
    case BRIG_TYPE_S8X16:
    case BRIG_TYPE_S16X8:
    case BRIG_TYPE_S32X4:
    case BRIG_TYPE_S64X2:
    case BRIG_TYPE_F16X8:
    case BRIG_TYPE_F32X4:
    case BRIG_TYPE_F64X2:
      return BRIG_TYPE_B128;

    default:
      gcc_assert (seen_error ());
      return t;
    }
}

/* Map a normal HSAIL type to the type of the equivalent BRIG operand
   holding such, for constants and registers.  */

static BrigType16_t
regtype_for_type (BrigType16_t t)
{
  switch (t)
    {
    case BRIG_TYPE_B1:
      return BRIG_TYPE_B1;

    case BRIG_TYPE_U8:
    case BRIG_TYPE_U16:
    case BRIG_TYPE_U32:
    case BRIG_TYPE_S8:
    case BRIG_TYPE_S16:
    case BRIG_TYPE_S32:
    case BRIG_TYPE_B8:
    case BRIG_TYPE_B16:
    case BRIG_TYPE_B32:
    case BRIG_TYPE_F16:
    case BRIG_TYPE_F32:
    case BRIG_TYPE_U8X4:
    case BRIG_TYPE_U16X2:
    case BRIG_TYPE_S8X4:
    case BRIG_TYPE_S16X2:
    case BRIG_TYPE_F16X2:
      return BRIG_TYPE_B32;

    case BRIG_TYPE_U64:
    case BRIG_TYPE_S64:
    case BRIG_TYPE_F64:
    case BRIG_TYPE_B64:
    case BRIG_TYPE_U8X8:
    case BRIG_TYPE_U16X4:
    case BRIG_TYPE_U32X2:
    case BRIG_TYPE_S8X8:
    case BRIG_TYPE_S16X4:
    case BRIG_TYPE_S32X2:
    case BRIG_TYPE_F16X4:
    case BRIG_TYPE_F32X2:

      return BRIG_TYPE_B64;

    case BRIG_TYPE_B128:
    case BRIG_TYPE_U8X16:
    case BRIG_TYPE_U16X8:
    case BRIG_TYPE_U32X4:
    case BRIG_TYPE_U64X2:
    case BRIG_TYPE_S8X16:
    case BRIG_TYPE_S16X8:
    case BRIG_TYPE_S32X4:
    case BRIG_TYPE_S64X2:
    case BRIG_TYPE_F16X8:
    case BRIG_TYPE_F32X4:
    case BRIG_TYPE_F64X2:
      return BRIG_TYPE_B128;

    default:
      gcc_unreachable ();
    }
}

/* Enqueue operation OP.  Return the offset at which it will be stored.  */

unsigned int
enqueue_op (hsa_op_base *op)
{
  unsigned ret;

  if (op->brig_op_offset)
    return op->brig_op_offset;

  ret = op_queue.projected_size;
  op->brig_op_offset = op_queue.projected_size;

  if (!op_queue.first_op)
    op_queue.first_op = op;
  else
    op_queue.last_op->next = op;
  op_queue.last_op = op;

  if (is_a <hsa_op_immed *> (op))
    op_queue.projected_size += sizeof (struct BrigOperandData);
  else if (is_a <hsa_op_reg *> (op))
    op_queue.projected_size += sizeof (struct BrigOperandReg);
  else if (is_a <hsa_op_address *> (op))
    {
    op_queue.projected_size += sizeof (struct BrigOperandAddress);
    }
  else if (is_a <hsa_op_code_ref *> (op))
    op_queue.projected_size += sizeof (struct BrigOperandCodeRef);
  else if (is_a <hsa_op_code_list *> (op))
    op_queue.projected_size += sizeof (struct BrigOperandCodeList);
  else
    gcc_unreachable ();
  return ret;
}

/* Emit an immediate BRIG operand IMM.  */

static void
emit_immediate_operand (hsa_op_immed *imm)
{
  struct BrigOperandData out;
  uint32_t byteCount;

  union
  {
    uint8_t b8;
    uint16_t b16;
    uint32_t b32;
    uint64_t b64;
  } bytes;
  unsigned len;

  switch (imm->type)
    {
    case BRIG_TYPE_U8:
    case BRIG_TYPE_S8:
      len = 1;
      bytes.b8 = (uint8_t) TREE_INT_CST_LOW (imm->value);
      break;
    case BRIG_TYPE_U16:
    case BRIG_TYPE_S16:
      bytes.b16 = (uint16_t) TREE_INT_CST_LOW (imm->value);
      len = 2;
      break;

    case BRIG_TYPE_F16:
      sorry ("Support for HSA does not implement immediate 16 bit FPU "
	     "operands");
      len = 2;
      break;

    case BRIG_TYPE_U32:
    case BRIG_TYPE_S32:
      bytes.b32 = (uint32_t) TREE_INT_CST_LOW (imm->value);
      len = 4;
      break;

    case BRIG_TYPE_U64:
    case BRIG_TYPE_S64:
      bytes.b64 = (uint64_t) int_cst_value (imm->value);
      len = 8;
      break;

    case BRIG_TYPE_F32:
    case BRIG_TYPE_F64:
      {
	tree expr = imm->value;
	tree type = TREE_TYPE (expr);

	len = GET_MODE_SIZE (TYPE_MODE (type));

	/* There are always 32 bits in each long, no matter the size of
	   the hosts long.  */
	long tmp[6];

	gcc_assert (len == 4 || len == 8);

	real_to_target (tmp, TREE_REAL_CST_PTR (expr), TYPE_MODE (type));

	if (len == 4)
	  bytes.b32 = (uint32_t) tmp[0];
	else
	  {
	    bytes.b64 = (uint64_t)(uint32_t) tmp[1];
	    bytes.b64 <<= 32;
	    bytes.b64 |= (uint32_t) tmp[0];
	  }

	break;
      }

    case BRIG_TYPE_U8X4:
    case BRIG_TYPE_S8X4:
    case BRIG_TYPE_U16X2:
    case BRIG_TYPE_S16X2:
    case BRIG_TYPE_F16X2:
      len = 4;
      sorry ("Support for HSA does not implement immediate 32bit "
	     "vector operands. ");
      break;

    case BRIG_TYPE_U8X8:
    case BRIG_TYPE_S8X8:
    case BRIG_TYPE_U16X4:
    case BRIG_TYPE_S16X4:
    case BRIG_TYPE_F16X4:
    case BRIG_TYPE_U32X2:
    case BRIG_TYPE_S32X2:
    case BRIG_TYPE_F32X2:
      len = 8;
      sorry ("Support for HSA does not implement immediate 32bit "
	     "vector operands. ");
      break;

    default:
      gcc_unreachable ();
    }

  out.base.byteCount = htole16 (sizeof (out));
  out.base.kind = htole16 (BRIG_KIND_OPERAND_DATA);
  byteCount = len ;

  out.data = brig_data.add (&byteCount, sizeof (byteCount));
  brig_data.add (&bytes, len);

  brig_operand.add (&out, sizeof(out));
  brig_data.round_size_up (4);
}

/* Emit a register BRIG operand REG.  */

static void
emit_register_operand (hsa_op_reg *reg)
{
  struct BrigOperandReg out;

  out.base.byteCount = htole16 (sizeof (out));
  out.base.kind = htole16 (BRIG_KIND_OPERAND_REG);
  out.regNum = htole32 (reg->hard_num);

  if (BRIG_TYPE_B32 == regtype_for_type (reg->type))
    out.regKind = BRIG_REGISTER_SINGLE;
  else if (BRIG_TYPE_B64 == regtype_for_type (reg->type))
    out.regKind = BRIG_REGISTER_DOUBLE;
  else if (BRIG_TYPE_B128 == regtype_for_type (reg->type))
    out.regKind = BRIG_REGISTER_QUAD;
  else if (BRIG_TYPE_B1 == regtype_for_type (reg->type))
    out.regKind = BRIG_REGISTER_CONTROL;
  else
    gcc_unreachable ();

  brig_operand.add (&out, sizeof (out));
}

/* Emit an address BRIG operand ADDR.  */

static void
emit_address_operand (hsa_op_address *addr)
{
  struct BrigOperandAddress out;

  out.base.byteCount = htole16 (sizeof (out));
  out.base.kind = htole16 (BRIG_KIND_OPERAND_ADDRESS);
  out.symbol = addr->symbol
    ? htole32 (emit_directive_variable (addr->symbol)) : 0;
  out.reg = addr->reg ? htole32 (enqueue_op (addr->reg)) : 0;

  /* FIXME: This is very clumsy.  */
  if (sizeof (addr->imm_offset) == 8)
    {
      out.offset.lo = htole32 ((uint32_t)addr->imm_offset);
      out.offset.hi = htole32 (((long long) addr->imm_offset) >> 32);
    }
  else
    {
      out.offset.lo = htole32 (addr->imm_offset);
      out.offset.hi = 0;
    }

  brig_operand.add (&out, sizeof (out));
}

/* Emit a code reference operand REF.  */

static void
emit_code_ref_operand (hsa_op_code_ref *ref)
{
  struct BrigOperandCodeRef out;

  out.base.byteCount = htole16 (sizeof (out));
  out.base.kind = htole16 (BRIG_KIND_OPERAND_CODE_REF);
  out.ref = htole32 (ref->directive_offset);
  brig_operand.add (&out, sizeof (out));
}

static void
emit_code_list_operand (hsa_op_code_list *code_list)
{
  struct BrigOperandCodeList out;
  unsigned args = code_list->offsets.length ();

  for (unsigned i = 0; i < args; i++)
    gcc_assert (code_list->offsets[i]);

  out.base.byteCount = htole16 (sizeof (out));
  out.base.kind = htole16 (BRIG_KIND_OPERAND_CODE_LIST);

  uint32_t byteCount = htole32 (4 * args);

  out.elements = htole32 (brig_data.add (&byteCount, sizeof (byteCount)));
  brig_data.add (code_list->offsets.address (), args * sizeof (uint32_t));
  brig_data.round_size_up (4);
  brig_operand.add (&out, sizeof (out));
}

/* Emit all operands queued for writing.  */

static void
emit_queued_operands (void)
{
  for (hsa_op_base *op = op_queue.first_op; op; op = op->next)
    {
      gcc_assert (op->brig_op_offset == brig_operand.total_size);
      if (hsa_op_immed *imm = dyn_cast <hsa_op_immed *> (op))
	emit_immediate_operand (imm);
      else if (hsa_op_reg *reg = dyn_cast <hsa_op_reg *> (op))
	emit_register_operand (reg);
      else if (hsa_op_address *addr = dyn_cast <hsa_op_address *> (op))
	emit_address_operand (addr);
      else if (hsa_op_code_ref *ref = dyn_cast <hsa_op_code_ref *> (op))
	emit_code_ref_operand (ref);
      else if (hsa_op_code_list *code_list = dyn_cast <hsa_op_code_list *> (op))
	emit_code_list_operand (code_list);
      else
	gcc_unreachable ();
    }
}

/* Emit an HSA memory instruction and all necessary directives, schedule
   necessary operands for writing .  */

static void
emit_memory_insn (hsa_insn_mem *mem)
{
  struct BrigInstMem repr;
  BrigOperandOffset32_t operand_offsets[2];
  uint32_t byteCount;

  hsa_op_address *addr = as_a <hsa_op_address *> (mem->operands[1]);

  /* This is necessary because of the errorneous typedef of
     BrigMemoryModifier8_t which introduces padding which may then contain
     random stuff (which we do not want so that we can test things don't
     change).  */
  memset (&repr, 0, sizeof (repr));
  repr.base.base.byteCount = htole16 (sizeof (repr));
  repr.base.base.kind = htole16 (BRIG_KIND_INST_MEM);
  repr.base.opcode = htole16 (mem->opcode);
  repr.base.type = htole16 (mem->type);

  operand_offsets[0] = htole32 (enqueue_op (mem->operands[0]));
  operand_offsets[1] = htole32 (enqueue_op (mem->operands[1]));
  /* We have two operands so use 4 * 2 for the byteCount */
  byteCount = htole32 (4 * 2);

  repr.base.operands = htole32 (brig_data.add (&byteCount, sizeof (byteCount)));
  brig_data.add (&operand_offsets, sizeof (operand_offsets));
  brig_data.round_size_up (4);

  if (addr->symbol)
    repr.segment = addr->symbol->segment;
  else
    repr.segment = BRIG_SEGMENT_FLAT;
  repr.modifier = 0 ;
  repr.equivClass = mem->equiv_class;
  repr.align = BRIG_ALIGNMENT_1;
  if (mem->opcode == BRIG_OPCODE_LD)
    repr.width = BRIG_WIDTH_1;
  else
    repr.width = BRIG_WIDTH_NONE;
  memset (&repr.reserved, 0, sizeof (repr.reserved));
  brig_code.add (&repr, sizeof (repr));
  brig_insn_count++;
}

/* Emit an HSA memory instruction and all necessary directives, schedule
   necessary operands for writing .  */

static void
emit_atomic_insn (hsa_insn_atomic *mem)
{
  struct BrigInstAtomic repr;
  BrigOperandOffset32_t operand_offsets[4];
  uint32_t byteCount;

  hsa_op_address *addr = as_a <hsa_op_address *> (mem->operands[1]);

  /* This is necessary because of the errorneous typedef of
     BrigMemoryModifier8_t which introduces padding which may then contain
     random stuff (which we do not want so that we can test things don't
     change).  */
  memset (&repr, 0, sizeof (repr));
  repr.base.base.byteCount = htole16 (sizeof (repr));
  repr.base.base.kind = htole16 (BRIG_KIND_INST_ATOMIC);
  repr.base.opcode = htole16 (mem->opcode);
  repr.base.type = htole16 (mem->type);

  operand_offsets[0] = htole32 (enqueue_op (mem->operands[0]));
  operand_offsets[1] = htole32 (enqueue_op (mem->operands[1]));
  operand_offsets[2] = htole32 (enqueue_op (mem->operands[2]));
  operand_offsets[3] = htole32 (enqueue_op (mem->operands[3]));

  /* We have 4 operands so use 4 * 4 for the byteCount */
  byteCount = htole32 (4 * 4);

  repr.base.operands = htole32 (brig_data.add (&byteCount, sizeof (byteCount)));
  brig_data.add (&operand_offsets, sizeof (operand_offsets));
  brig_data.round_size_up (4);

  if (addr->symbol)
    repr.segment = addr->symbol->segment;
  else
    repr.segment = BRIG_SEGMENT_FLAT;
  repr.memoryOrder = mem->memoryorder;
  repr.memoryScope = mem->memoryscope;
  repr.atomicOperation = mem->atomicop;

  brig_code.add (&repr, sizeof (repr));
  brig_insn_count++;
}

/* Emit an HSA LDA instruction and all necessary directives, schedule
   necessary operands for writing .  */

static void
emit_addr_insn (hsa_insn_addr *insn)
{
  struct BrigInstAddr repr;
  BrigOperandOffset32_t operand_offsets[2];
  uint32_t byteCount;

  hsa_op_address *addr = as_a <hsa_op_address *> (insn->operands[1]);

  repr.base.base.byteCount = htole16 (sizeof (repr));
  repr.base.base.kind = htole16 (BRIG_KIND_INST_ADDR);
  repr.base.opcode = htole16 (insn->opcode);
  repr.base.type = htole16 (insn->type);

  operand_offsets[0] = htole32 (enqueue_op (insn->operands[0]));
  operand_offsets[1] = htole32 (enqueue_op (insn->operands[1]));

  /* We have two operands so use 4 * 2 for the byteCount */
  byteCount = htole32 (4 * 2);

  repr.base.operands = htole32 (brig_data.add (&byteCount, sizeof (byteCount)));
  brig_data.add (&operand_offsets, sizeof (operand_offsets));
  brig_data.round_size_up (4);

  if (addr->symbol)
    repr.segment = addr->symbol->segment;
  else
    repr.segment = BRIG_SEGMENT_FLAT;
  memset (&repr.reserved, 0, sizeof (repr.reserved));

  brig_code.add (&repr, sizeof (repr));
  brig_insn_count++;
}

/* Emit an HSA segment conversion instruction and all necessary directives,
   schedule necessary operands for writing .  */

static void
emit_segment_insn (hsa_insn_seg *seg)
{
  struct BrigInstSegCvt repr;
  BrigOperandOffset32_t operand_offsets[2];
  uint32_t byteCount;

  repr.base.base.byteCount = htole16 (sizeof (repr));
  repr.base.base.kind = htole16 (BRIG_KIND_INST_SEG_CVT);
  repr.base.opcode = htole16 (seg->opcode);
  repr.base.type = htole16 (seg->type);

  operand_offsets[0] = htole32 (enqueue_op (seg->operands[0]));
  operand_offsets[1] = htole32 (enqueue_op (seg->operands[1]));

  /* We have two operands so use 4 * 2 for the byteCount */
  byteCount = htole32 (4 * 2);

  repr.base.operands = htole32 (brig_data.add (&byteCount, sizeof (byteCount)));
  brig_data.add (&operand_offsets, sizeof (operand_offsets));
  brig_data.round_size_up (4);

  repr.sourceType = htole16 (as_a <hsa_op_reg *> (seg->operands[1])->type);
  repr.segment = seg->segment;
  repr.modifier = 0;

  brig_code.add (&repr, sizeof (repr));

  brig_insn_count++;
}

/* Emit an HSA comparison instruction and all necessary directives,
   schedule necessary operands for writing .  */

static void
emit_cmp_insn (hsa_insn_cmp *cmp)
{
  struct BrigInstCmp repr;
  BrigOperandOffset32_t operand_offsets[3];
  uint32_t byteCount;

  repr.base.base.byteCount = htole16 (sizeof (repr));
  repr.base.base.kind = htole16 (BRIG_KIND_INST_CMP);
  repr.base.opcode = htole16 (cmp->opcode);
  repr.base.type = htole16 (cmp->type);

  operand_offsets[0] = htole32 (enqueue_op (cmp->operands[0]));
  operand_offsets[1] = htole32 (enqueue_op (cmp->operands[1]));
  operand_offsets[2] = htole32 (enqueue_op (cmp->operands[2]));
  /* We have three operands so use 4 * 3 for the byteCount */
  byteCount = htole32 (4 * 3);

  repr.base.operands = htole32 (brig_data.add (&byteCount, sizeof (byteCount)));
  brig_data.add (&operand_offsets, sizeof (operand_offsets));
  brig_data.round_size_up (4);

  if (is_a <hsa_op_reg *> (cmp->operands[1]))
    repr.sourceType = htole16 (as_a <hsa_op_reg *> (cmp->operands[1])->type);
  else
    repr.sourceType = htole16 (as_a <hsa_op_immed *> (cmp->operands[1])->type);
  repr.modifier = 0;
  repr.compare = cmp->compare;
  repr.pack = 0;
  repr.reserved = 0;

  brig_code.add (&repr, sizeof (repr));
  brig_insn_count++;
}

/* Emit an HSA branching instruction and all necessary directives, schedule
   necessary operands for writing .  */

static void
emit_branch_insn (hsa_insn_br *br)
{
  struct BrigInstBr repr;
  BrigOperandOffset32_t operand_offsets[2];
  uint32_t byteCount;

  basic_block target = NULL;
  edge_iterator ei;
  edge e;

  /* At the moment we only handle direct conditional jumps.  */
  gcc_assert (br->opcode == BRIG_OPCODE_CBR
	      && !br->operands[2]);
  repr.base.base.byteCount = htole16 (sizeof (repr));
  repr.base.base.kind = htole16 (BRIG_KIND_INST_BR);
  repr.base.opcode = htole16 (br->opcode);
  repr.width = BRIG_WIDTH_1;
  /* For Conditional jumps the type is always B1 */
  repr.base.type = htole16 (BRIG_TYPE_B1);

  operand_offsets[0] = htole32 (enqueue_op (br->operands[0]));

  FOR_EACH_EDGE (e, ei, br->bb->succs)
    if (e->flags & EDGE_TRUE_VALUE)
      {
	target = e->dest;
	break;
      }
  gcc_assert (target);
  operand_offsets[1] = htole32 (enqueue_op
				(&hsa_bb_for_bb (target)->label_ref));

  /* We have 2 operands so use 4 * 2 for the byteCount */
  byteCount = htole32 (4 * 2);
  repr.base.operands = htole32 (brig_data.add (&byteCount, sizeof (byteCount)));
  brig_data.add (&operand_offsets, sizeof (operand_offsets));
  brig_data.round_size_up (4);
  repr.width = BRIG_WIDTH_1;
  memset (&repr.reserved, 0, sizeof (repr.reserved));

  brig_code.add (&repr, sizeof (repr));
  brig_insn_count++;
}

static bool
float_type_p (BrigType16_t t)
{
  switch (t & BRIG_TYPE_BASE_MASK)
    {
    case BRIG_TYPE_F16:
    case BRIG_TYPE_F32:
    case BRIG_TYPE_F64:
      return true;
    default:
      return false;
    }
}

/* Emit a HSA convert instruction and all necessary directives, schedule
   necessary operands for writing.  */

static void
emit_cvt_insn (hsa_insn_basic *insn)
{
  struct BrigInstCvt repr;
  BrigType16_t srctype;
  BrigOperandOffset32_t operand_offsets[HSA_OPERANDS_PER_INSN];
  uint32_t byteCount, operand_count=0;

  repr.base.base.byteCount = htole16 (sizeof (repr));
  repr.base.base.kind = htole16 (BRIG_KIND_INST_CVT);
  repr.base.opcode = htole16 (insn->opcode);
  repr.base.type = htole16 (insn->type);

  for (int i = 0; i < HSA_OPERANDS_PER_INSN; i++)
    if (insn->operands[i])
      {
        operand_offsets[i] = htole32 (enqueue_op (insn->operands[i]));
        operand_count = operand_count + 1;
      }
    else
      operand_offsets[i] = 0;

  byteCount = htole32 (4 * operand_count) ;
  repr.base.operands = htole32 (brig_data.add (&byteCount, sizeof (byteCount)));
  brig_data.add (&operand_offsets,
		 operand_count * sizeof (BrigOperandOffset32_t));

  if (is_a <hsa_op_reg *> (insn->operands[1]))
    srctype = as_a <hsa_op_reg *> (insn->operands[1])->type;
  else
    srctype = as_a <hsa_op_immed *> (insn->operands[1])->type;
  repr.sourceType = htole16 (srctype);

  /* float to smaller float requires a rounding setting (we default
     to 'near'.  */
  if (float_type_p (insn->type) && float_type_p (srctype)
      && (insn->type & BRIG_TYPE_BASE_MASK) < (srctype & BRIG_TYPE_BASE_MASK))
    repr.modifier = BRIG_ROUND_FLOAT_NEAR_EVEN;
  else
    repr.modifier = 0;

  brig_code.add (&repr, sizeof (repr));
  brig_insn_count++;
}

/* Emit arg block to code segment.  */

static void
emit_arg_block (bool is_start)
{
  struct BrigDirectiveArgBlock repr;
  repr.base.byteCount = htole16 (sizeof (repr));

  BrigKinds16_t kind = is_start ? BRIG_KIND_DIRECTIVE_ARG_BLOCK_START
    : BRIG_KIND_DIRECTIVE_ARG_BLOCK_END;
  repr.base.kind = htole16 (kind);

  brig_code.add (&repr, sizeof (repr));
  brig_insn_count++;
}

static void
emit_call_insn (hsa_insn_basic *insn)
{
  hsa_insn_call *call = dyn_cast <hsa_insn_call *> (insn);
  struct BrigInstBr repr;
  uint32_t byteCount;

  BrigOperandOffset32_t operand_offsets[3];

  repr.base.base.byteCount = htole16 (sizeof (repr));
  repr.base.base.kind = htole16 (BRIG_KIND_INST_BR);
  repr.base.opcode = htole16 (BRIG_OPCODE_CALL);
  repr.base.type = htole16 (BRIG_TYPE_NONE);

  /* Operand 0: out-args.  */
  operand_offsets[0] = htole32 (enqueue_op (call->result_code_list));

  /* Operand 1: func */
  /* XXX: we have to save offset to operand section and
     called function offset is filled up after all functions are visited. */
  unsigned int offset = enqueue_op (&call->func);

  function_call_linkage.safe_push
    (function_linkage_pair (call->called_function, offset));

  operand_offsets[1] = htole32 (offset);
  /* Operand 2: in-args.  */
  operand_offsets[2] = htole32 (enqueue_op (call->args_code_list));

  /* We have 3 operands so use 3 * 4 for the byteCount */
  byteCount = htole32 (3 * 4);
  repr.base.operands = htole32 (brig_data.add (&byteCount, sizeof (byteCount)));
  brig_data.add (&operand_offsets, sizeof (operand_offsets));
  brig_data.round_size_up (4);
  repr.width = BRIG_WIDTH_ALL;
  memset (&repr.reserved, 0, sizeof (repr.reserved));

  brig_code.add (&repr, sizeof (repr));
  brig_insn_count++;
}

/* Emit call block instruction. This super instruction encapsulate all
   instructions needed for argument load/store and corresponding
   instruction.  */

static void
emit_call_block_insn (hsa_insn_call_block *insn)
{
  /* Argument scope start.  */
  emit_arg_block (true);

  for (unsigned i = 0; i < insn->input_args.length (); i++)
    {
      insn->call_insn->args_code_list->offsets[i] = htole32
	(emit_directive_variable (insn->input_args[i]));
      brig_insn_count++;
    }

  if (insn->call_insn->result_symbol)
    {
      insn->call_insn->result_code_list->offsets[0] = htole32
	(emit_directive_variable (insn->output_arg));
      brig_insn_count++;
    }

  for (unsigned i = 0; i < insn->input_arg_insns.length (); i++)
    emit_memory_insn (insn->input_arg_insns[i]);

  emit_call_insn (insn->call_insn);

  if (insn->output_arg_insn)
    emit_memory_insn (insn->output_arg_insn);

  /* Argument scope end.  */
  emit_arg_block (false);
}

/* Emit a basic HSA instruction and all necessary directives, schedule
   necessary operands for writing .  */

static void
emit_basic_insn (hsa_insn_basic *insn)
{
  /* We assume that BrigInstMod has a BrigInstBasic prefix.  */
  struct BrigInstMod repr;
  BrigType16_t type;
  BrigOperandOffset32_t operand_offsets[HSA_OPERANDS_PER_INSN];
  uint32_t byteCount, operand_count = 0;

  if (insn->opcode == BRIG_OPCODE_CVT)
    {
      emit_cvt_insn (insn);
      return;
    }

  repr.base.base.byteCount = htole16 (sizeof (BrigInstBasic));
  repr.base.base.kind = htole16 (BRIG_KIND_INST_BASIC);
  repr.base.opcode = htole16 (insn->opcode);
  switch (insn->opcode)
    {
      /* XXX The spec says mov can take all types.  But the LLVM based
	 simulator cries about "Mov_s32" not being defined.  */
      case BRIG_OPCODE_MOV:
      /* And the bit-logical operations need bit types and whine about
         arithmetic types :-/  */
      case BRIG_OPCODE_AND:
      case BRIG_OPCODE_OR:
      case BRIG_OPCODE_XOR:
      case BRIG_OPCODE_NOT:
	type = regtype_for_type (insn->type);
	break;
      default:
	type = insn->type;
	break;
    }
  repr.base.type = htole16 (type);

  for (int i = 0; i < HSA_OPERANDS_PER_INSN; i++)
    if (insn->operands[i])
      {
	operand_offsets[i] = htole32 (enqueue_op (insn->operands[i]));
	operand_count = operand_count + 1;
      }
    else
      operand_offsets[i] = 0;

  byteCount = htole32 (4 * operand_count) ;
  repr.base.operands = htole32 (brig_data.add (&byteCount, sizeof (byteCount)));
  brig_data.add (&operand_offsets,
		 operand_count * sizeof (BrigOperandOffset32_t));
  brig_data.round_size_up (4);

  if ((type & BRIG_TYPE_PACK_MASK) != BRIG_TYPE_PACK_NONE)
    {
      if (float_type_p (type))
	repr.modifier = BRIG_ROUND_FLOAT_NEAR_EVEN;
      else
	repr.modifier = 0;
      /* We assume that destination and sources agree in packing
         layout.  */
      if (insn->operands[2])
	repr.pack = BRIG_PACK_PP;
      else
	repr.pack = BRIG_PACK_P;
      repr.reserved = 0;
      repr.base.base.byteCount = htole16 (sizeof (BrigInstMod));
      repr.base.base.kind = htole16 (BRIG_KIND_INST_MOD);
      brig_code.add (&repr, sizeof (struct BrigInstMod));
    }
  else
    brig_code.add (&repr, sizeof (struct BrigInstBasic));
  brig_insn_count++;
}

/* Emit an HSA instruction and all necessary directives, schedule necessary
   operands for writing .  */

static void
emit_insn (hsa_insn_basic *insn)
{
  gcc_assert (!is_a <hsa_insn_phi *> (insn));
  if (hsa_insn_atomic *atom = dyn_cast <hsa_insn_atomic *> (insn))
    {
      emit_atomic_insn (atom);
      return;
    }
  if (hsa_insn_mem *mem = dyn_cast <hsa_insn_mem *> (insn))
    {
      emit_memory_insn (mem);
      return;
    }
  if (hsa_insn_addr *addr = dyn_cast <hsa_insn_addr *> (insn))
    {
      emit_addr_insn (addr);
      return;
    }
  if (hsa_insn_seg *seg = dyn_cast <hsa_insn_seg *> (insn))
    {
      emit_segment_insn (seg);
      return;
    }
  if (hsa_insn_cmp *cmp = dyn_cast <hsa_insn_cmp *> (insn))
    {
      emit_cmp_insn (cmp);
      return;
    }
  if (hsa_insn_br *br = dyn_cast <hsa_insn_br *> (insn))
    {
      emit_branch_insn (br);
      return;
    }
  if (hsa_insn_call_block *call_block = dyn_cast <hsa_insn_call_block *> (insn))
    {
      emit_call_block_insn (call_block);
      return;
    }
  if (hsa_insn_call *call = dyn_cast <hsa_insn_call *> (insn))
    {
      emit_call_insn (call);
      return;
    }
  emit_basic_insn (insn);
}

/* We have just finished emitting BB and are about to emit NEXT_BB if non-NULL,
   or we are about to finish emiting code, if it is NULL.  If the fall through
   edge from BB does not lead to NEXT_BB, emit an unconditional jump.  */

static void
perhaps_emit_branch (basic_block bb, basic_block next_bb)
{
  basic_block t_bb = NULL, ff = NULL;
  struct BrigInstBr repr;
  BrigOperandOffset32_t operand_offsets[1];
  uint32_t byteCount;

  edge_iterator ei;
  edge e;

  FOR_EACH_EDGE (e, ei, bb->succs)
    if (e->flags & EDGE_TRUE_VALUE)
      {
	gcc_assert (!t_bb);
	t_bb = e->dest;
      }
    else
      {
	gcc_assert (!ff);
	ff = e->dest;
      }
  gcc_assert (ff);
  if (ff == next_bb
      || ff == EXIT_BLOCK_PTR_FOR_FN (cfun))
    return;

  repr.base.base.byteCount = htole16 (sizeof (repr));
  repr.base.base.kind = htole16 (BRIG_KIND_INST_BR);
  repr.base.opcode = htole16 (BRIG_OPCODE_BR);
  repr.base.type = htole16 (BRIG_TYPE_NONE);
  /* Direct branches to labels must be width(all).  */
  repr.width = BRIG_WIDTH_ALL;

  operand_offsets[0] = htole32 (enqueue_op (&hsa_bb_for_bb (ff)->label_ref));
  /* We have 1 operand so use 4 * 1 for the byteCount */
  byteCount = htole32 (4 * 1);
  repr.base.operands = htole32 (brig_data.add (&byteCount, sizeof (byteCount)));
  brig_data.add (&operand_offsets, sizeof (operand_offsets));
  brig_data.round_size_up (4);
  memset (&repr.reserved, 0, sizeof (repr.reserved));
  brig_code.add (&repr, sizeof (repr));
  brig_insn_count++;
}

/* Emit the a function with name NAME to the various brig sections.  */

void
hsa_brig_emit_function (void)
{
  basic_block bb, prev_bb;
  hsa_insn_basic *insn;
  BrigDirectiveExecutable *ptr_to_fndir;

  brig_init ();

  brig_insn_count = 0;
  memset (&op_queue, 0, sizeof (op_queue));
  op_queue.projected_size = brig_operand.total_size;

  ptr_to_fndir = emit_function_directives ();
  for (insn = hsa_bb_for_bb (ENTRY_BLOCK_PTR_FOR_FN (cfun))->first_insn;
       insn;
       insn = insn->next)
    emit_insn (insn);
  prev_bb = ENTRY_BLOCK_PTR_FOR_FN (cfun);
  FOR_EACH_BB_FN (bb, cfun)
    {
      perhaps_emit_branch (prev_bb, bb);
      emit_bb_label_directive (hsa_bb_for_bb (bb));
      for (insn = hsa_bb_for_bb (bb)->first_insn; insn; insn = insn->next)
	emit_insn (insn);
      prev_bb = bb;
    }
  perhaps_emit_branch (prev_bb, NULL);
  ptr_to_fndir->codeBlockEntryCount = brig_insn_count ;
  ptr_to_fndir->nextModuleEntry = brig_code.total_size;

  emit_queued_operands ();
}

void
hsa_output_brig (void)
{
  section *saved_section;

  if (!brig_initialized)
    return;

  for (unsigned i = 0; i < function_call_linkage.length (); i++)
    {
      function_linkage_pair p = function_call_linkage[i];

      BrigCodeOffset32_t *func_offset = function_offsets.get (p.function_decl);
      if (*func_offset)
        {
	  BrigOperandCodeRef *code_ref = (BrigOperandCodeRef *)
	    (brig_operand.get_ptr_by_offset (p.offset));
	  gcc_assert (code_ref->base.kind == BRIG_KIND_OPERAND_CODE_REF);
	  code_ref->ref = htole32 (*func_offset);
	}
      else
	{
	  sorry ("Missing offset to a HSA function in call instruction");
	  return;
	}
    }

  saved_section = in_section;

  brig_data.output ();
  brig_code.output ();
  brig_operand.output ();

  if (saved_section)
    switch_to_section (saved_section);

  brig_release_data ();
  hsa_deinit_compilation_unit_data ();
}
