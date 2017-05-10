/* Definitions for the shared dumpfile.
   Copyright (C) 2004-2017 Free Software Foundation, Inc.

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


#ifndef GCC_DUMPFILE_H
#define GCC_DUMPFILE_H 1


/* Different tree dump places.  When you add new tree dump places,
   extend the DUMP_FILES array in dumpfile.c.  */
enum tree_dump_index
{
  TDI_none,			/* No dump */
  TDI_cgraph,                   /* dump function call graph.  */
  TDI_inheritance,              /* dump type inheritance graph.  */
  TDI_clones,			/* dump IPA cloning decisions.  */
  TDI_tu,			/* dump the whole translation unit.  */
  TDI_class,			/* dump class hierarchy.  */
  TDI_original,			/* dump each function before optimizing it */
  TDI_generic,			/* dump each function after genericizing it */
  TDI_nested,			/* dump each function after unnesting it */
  TDI_tree_all,                 /* enable all the GENERIC/GIMPLE dumps.  */
  TDI_rtl_all,                  /* enable all the RTL dumps.  */
  TDI_ipa_all,                  /* enable all the IPA dumps.  */

  TDI_end
};

/* Dump option node is a tree structure that implements
   parsing of suboptions and provides mapping between a given enum type E
   and unsigned integer masks that are encapsulated in dump_flags_type type.  */

template <typename E>
struct dump_option_node
{
  /* Constructor.  */
  dump_option_node (const char *name, E enum_value);

  /* Initialize hierarchy and fill up a MASK_TRANLATION table.  */
  void initialize (uint64_t *mask_translation);

  /* Parse a given option string and return mask.  */
  uint64_t parse (const char *token);

  /* Parse a given option string and return mask.  */
  uint64_t parse (char *token);

  /* Register a SUBOPTION for a dump option node.  */
  void register_suboption (dump_option_node<E> *suboption)
  {
    m_children.safe_push (suboption);
  }

private:
  /* Initialize masks for internal nodes.  CURRENT is a counter with first
     free mask.  MASK_TRANSLATION is table that is filled up.  */
  uint64_t initialize_masks (unsigned *current, uint64_t *mask_translation);

  /* Parse a given option string and return mask.  */
  uint64_t parse_internal (vec<char *> &tokens);

  /* Name of the option.  */
  const char *m_name;

  /* Enum value of the option.  */
  E m_enum_value;

  /* Children options.  */
  vec<dump_option_node *> m_children;

  /* Mask that represents the option.  */
  uint64_t m_mask;
};

/* Size of possible valid leaf options.  */
#define OPT_MASK_SIZE (CHAR_BIT * sizeof (uint64_t))

/* Dump flags type represents a set of selected options for
   a given enum type E.  */

template <typename E>
struct dump_flags_type
{
  /* Constructor.  */
  dump_flags_type<E> (): m_mask (0)
  {}

  /* Constructor for a enum value E.  */
  dump_flags_type<E> (E enum_value)
  {
    gcc_checking_assert ((unsigned)enum_value <= OPT_MASK_SIZE);
    m_mask = m_mask_translation[enum_value];
    gcc_checking_assert (m_mask != 0);
  }

  /* Constructor for two enum values.  */
  dump_flags_type<E> (E enum_value, E enum_value2)
  {
    gcc_checking_assert ((unsigned)enum_value <= OPT_MASK_SIZE);
    gcc_checking_assert ((unsigned)enum_value2 <= OPT_MASK_SIZE);
    m_mask = m_mask_translation[enum_value] | m_mask_translation[enum_value2];
    gcc_checking_assert (m_mask != 0);
  }

  /* Constructor for three enum values.  */
  dump_flags_type<E> (E enum_value, E enum_value2, E enum_value3)
  {
    gcc_checking_assert ((unsigned)enum_value <= OPT_MASK_SIZE);
    gcc_checking_assert ((unsigned)enum_value2 <= OPT_MASK_SIZE);
    gcc_checking_assert ((unsigned)enum_value3 <= OPT_MASK_SIZE);
    m_mask = m_mask_translation[enum_value]
      | m_mask_translation[enum_value2]
      | m_mask_translation[enum_value3];
    gcc_checking_assert (m_mask != 0);
  }

  /* OR operator for OTHER dump_flags_type.  */
  inline dump_flags_type operator| (dump_flags_type other)
  {
    return dump_flags_type (m_mask | other.m_mask);
  }

  /* OR operator for OTHER dump_flags_type.  */
  inline void operator|= (const dump_flags_type other)
  {
    m_mask |= other.m_mask;
  }

  /* AND operator for OTHER dump_flags_type.  */
  inline void operator&= (const dump_flags_type other)
  {
    m_mask &= other.m_mask;
  }

  /* AND operator for OTHER dump_flags_type.  */
  inline bool operator& (const dump_flags_type other)
  {
    return m_mask & other.m_mask;
  }

  /* Intersect flags and return a new instance.  */
  inline dump_flags_type operator- (const dump_flags_type other)
  {
    return dump_flags_type (m_mask & other.m_mask);
  }

  /* Intersect flags with OTHER.  */
  inline void operator-= (const dump_flags_type other)
  {
    *this &= other;
  }

  /* Equality operator.  */
  inline bool operator== (const dump_flags_type other)
  {
    return m_mask == other.m_mask;
  }

  /* Non-equality operator.  */
  inline bool operator!= (const dump_flags_type other)
  {
    return m_mask != other.m_mask;
  }

  /* Return true if any option is selected.  */
  inline bool any ()
  {
    return m_mask;
  }

  /* Return dump_flags_type for a computed mask.  */
  static inline dump_flags_type from_mask (uint64_t mask)
  {
    return dump_flags_type (mask);
  }

  /* Return mask that represents all selected options.  */
  static inline dump_flags_type get_all ()
  {
    return m_mask_translation[0];
  }

  /* Initialize.  */
  static dump_flags_type parse (char *option);

  /* Selected mask of options.  */
  uint64_t m_mask;

  /* Translation table between enum values and masks.  */
  static uint64_t m_mask_translation[OPT_MASK_SIZE];

private:
  /* Constructor.  */
  dump_flags_type<E> (uint64_t mask): m_mask (mask)
  {}

};

/* Flags used for -fopt-info groups.  */

enum optgroup_types
{
  OPTGROUP_NONE,
  OPTGROUP_IPA,
  OPTGROUP_IPA_OPTIMIZED,
  OPTGROUP_IPA_MISSED,
  OPTGROUP_IPA_NOTE,
  OPTGROUP_LOOP,
  OPTGROUP_LOOP_OPTIMIZED,
  OPTGROUP_LOOP_MISSED,
  OPTGROUP_LOOP_NOTE,
  OPTGROUP_INLINE,
  OPTGROUP_INLINE_OPTIMIZED,
  OPTGROUP_INLINE_MISSED,
  OPTGROUP_INLINE_NOTE,
  OPTGROUP_OMP,
  OPTGROUP_OMP_OPTIMIZED,
  OPTGROUP_OMP_MISSED,
  OPTGROUP_OMP_NOTE,
  OPTGROUP_VEC,
  OPTGROUP_VEC_OPTIMIZED,
  OPTGROUP_VEC_MISSED,
  OPTGROUP_VEC_NOTE,
  OPTGROUP_OTHER,
  OPTGROUP_OTHER_OPTIMIZED,
  OPTGROUP_OTHER_MISSED,
  OPTGROUP_OTHER_NOTE,
  OPTGROUP_COUNT
};

template<typename E>
uint64_t
dump_flags_type<E>::m_mask_translation[OPT_MASK_SIZE];

/* Dump flags type for optgroup_types enum type.  */

typedef dump_flags_type<optgroup_types> optgroup_dump_flags_t;

enum suboption_types
{
  TDF_ALL,
  /* Globally used suboptions.  */
  TDF_SLIM,
  TDF_RAW,
  TDF_GRAPH,
  TDF_DETAILS,
  TDF_STATS,
  TDF_DIAGNOSTIC,
  TDF_VERBOSE,
  TDF_COMMENT,
  TDF_LINENO,
  TDF_BLOCKS,
  TDF_UID,
  TDF_NOUID,
  TDF_ADDRESS,
  TDF_ASMNAME,
  TDF_STMTADDR,
  TDF_LOCALS,
  TDF_ENUMERATE_LOCALS,
  /* TREE group.  */
  TDF_TREE,
  /* IPA group.  */
  TDF_IPA,
  /* GIMPLE group. */
  TDF_GIMPLE,
  TDF_VOPS,
  TDF_ALIAS,
  TDF_SCEV,
  TDF_RHS_ONLY,
  TDF_MEMSYMS,
  TDF_EH,
  /* RTL group.  */
  TDF_RTL,
  TDF_CSELIB,
};

/* Dump flags type for suboption enum type.  */

typedef dump_flags_type<suboption_types> dump_flags_t;

#define TDF_NONE dump_flags_t ()

/* Define a tree dump switch.  */
struct dump_file_info
{
  const char *suffix;		/* suffix to give output file.  */
  const char *swtch;		/* command line dump switch */
  const char *glob;		/* command line glob  */
  const char *pfilename;	/* filename for the pass-specific stream  */
  const char *alt_filename;     /* filename for the -fopt-info stream  */
  FILE *pstream;		/* pass-specific dump stream  */
  FILE *alt_stream;		/* -fopt-info stream */
  dump_flags_t pflags;		/* dump flags */
  optgroup_dump_flags_t pass_optgroup_flags; /* a pass flags for -fopt-info */
  optgroup_dump_flags_t optgroup_flags; /* flags for -fopt-info given
					   by a user */
  int pstate;			/* state of pass-specific stream */
  int alt_state;		/* state of the -fopt-info stream */
  int num;			/* dump file number */
  bool owns_strings;		/* fields "suffix", "swtch", "glob" can be
				   const strings, or can be dynamically
				   allocated, needing free.  */
  bool graph_dump_initialized;  /* When a given dump file is being initialized,
				   this flag is set to true if the corresponding
				   TDF_graph dump file has also been
				   initialized.  */
};

/* In dumpfile.c */
extern FILE *dump_begin (int, dump_flags_t *);
extern void dump_end (int, FILE *);
extern int opt_info_switch_p (const char *);
extern const char *dump_flag_name (int);
extern void dump_printf (optgroup_dump_flags_t,
			 const char *, ...) ATTRIBUTE_PRINTF_2;
extern void dump_printf_loc (optgroup_dump_flags_t, source_location,
                             const char *, ...) ATTRIBUTE_PRINTF_3;
extern void dump_basic_block (optgroup_dump_flags_t, basic_block, int);
extern void dump_generic_expr_loc (optgroup_dump_flags_t, source_location,
				   int, tree);
extern void dump_generic_expr (optgroup_dump_flags_t, dump_flags_t, tree);
extern void dump_gimple_stmt_loc (optgroup_dump_flags_t, source_location,
				  dump_flags_t, gimple *, int);
extern void dump_gimple_stmt (optgroup_dump_flags_t, dump_flags_t, gimple *,
			      int);
extern void print_combine_total_stats (void);
extern bool enable_rtl_dump_file (void);

/* In tree-dump.c  */
extern void dump_node (const_tree, dump_flags_t, FILE *);

/* In combine.c  */
extern void dump_combine_total_stats (FILE *);
/* In cfghooks.c  */
extern void dump_bb (FILE *, basic_block, int, dump_flags_t = TDF_NONE);

/* Global variables used to communicate with passes.  */
extern FILE *dump_file;
extern FILE *alt_dump_file;
extern dump_flags_t dump_flags;
extern const char *dump_file_name;

/* Return true if any of the dumps is enabled, false otherwise. */
static inline bool
dump_enabled_p (void)
{
  return (dump_file || alt_dump_file);
}

/* Suboptions hierarchy.  */

struct suboptions_hierarchy
{
  /* Contructor.  */
  suboptions_hierarchy();

  /* Initialize optgroup options.  */
  typedef dump_option_node<suboption_types> node;

  /* Root option node.  */
  node *root;
};

/* Optgroup option hierarchy.  */

struct optgroup_option_hierarchy
{
  /* Contructor.  */
  optgroup_option_hierarchy();

  /* Initialize optgroup options.  */
  typedef dump_option_node<optgroup_types> node;

  /* Root option node.  */
  node *root;
};

namespace gcc {

class dump_manager
{
public:

  dump_manager ();
  ~dump_manager ();

  /* Register a dumpfile.

     TAKE_OWNERSHIP determines whether callee takes ownership of strings
     SUFFIX, SWTCH, and GLOB. */
  unsigned int
  dump_register (const char *suffix, const char *swtch, const char *glob,
		 dump_flags_t flags, optgroup_dump_flags_t  optgroup_flags,
		 bool take_ownership);

  /* Return the dump_file_info for the given phase.  */
  struct dump_file_info *
  get_dump_file_info (int phase) const;

  struct dump_file_info *
  get_dump_file_info_by_switch (const char *swtch) const;

  /* Return the name of the dump file for the given phase.
     If the dump is not enabled, returns NULL.  */
  char *
  get_dump_file_name (int phase) const;

  char *
  get_dump_file_name (struct dump_file_info *dfi) const;

  int
  dump_switch_p (const char *arg);

  /* Start a dump for PHASE. Store user-supplied dump flags in
     *FLAG_PTR.  Return the number of streams opened.  Set globals
     DUMP_FILE, and ALT_DUMP_FILE to point to the opened streams, and
     set dump_flags appropriately for both pass dump stream and
     -fopt-info stream. */
  int
  dump_start (int phase, dump_flags_t *flag_ptr);

  /* Finish a tree dump for PHASE and close associated dump streams.  Also
     reset the globals DUMP_FILE, ALT_DUMP_FILE, and DUMP_FLAGS.  */
  void
  dump_finish (int phase);

  FILE *
  dump_begin (int phase, dump_flags_t *flag_ptr);

  /* Returns nonzero if tree dump PHASE has been initialized.  */
  int
  dump_initialized_p (int phase) const;

  /* Returns the switch name of PHASE.  */
  const char *
  dump_flag_name (int phase) const;

private:

  int
  dump_phase_enabled_p (int phase) const;

  int
  dump_switch_p_1 (const char *arg, struct dump_file_info *dfi, bool doglob);

  int
  dump_enable_all (dump_flags_t flags, const char *filename);

  int
  opt_info_enable_passes (optgroup_dump_flags_t optgroup_flags,
			  const char *filename);

private:

  /* Dynamically registered dump files and switches.  */
  int m_next_dump;
  struct dump_file_info *m_extra_dump_files;
  size_t m_extra_dump_files_in_use;
  size_t m_extra_dump_files_alloced;

  /* Grant access to dump_enable_all.  */
  friend bool ::enable_rtl_dump_file (void);

  /* Grant access to opt_info_enable_passes.  */
  friend int ::opt_info_switch_p (const char *arg);
}; // class dump_manager

} // namespace gcc

#endif /* GCC_DUMPFILE_H */
