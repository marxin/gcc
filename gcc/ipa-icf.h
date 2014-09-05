/* Interprocedural semantic function equality pass
   Copyright (C) 2014 Free Software Foundation, Inc.

   Contributed by Jan Hubicka <hubicka@ucw.cz> and Martin Liska <mliska@suse.cz>

This file is part of GCC.

GCC is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 3, or (at your option) any later
version.

GCC is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License
along with GCC; see the file COPYING3.  If not see
<http://www.gnu.org/licenses/>.  */

/* Prints string STRING to a FILE with a given number of SPACE_COUNT.  */
#define FPUTS_SPACES(file, space_count, string) \
  fprintf (file, "%*s" string, space_count, " "); \
 
/* fprintf function wrapper that transforms given FORMAT to follow given
   number for SPACE_COUNT and call fprintf for a FILE.  */
#define FPRINTF_SPACES(file, space_count, format, ...) \
  fprintf (file, "%*s" format, space_count, " ", ##__VA_ARGS__);

/* Prints a MESSAGE to dump_file if exists. FUNC is name of function and
   LINE is location in the source file.  */

static inline void
dump_message (const char *message, const char *func, unsigned int line)
{
  if (dump_file && (dump_flags & TDF_DETAILS))
    fprintf (dump_file, "  debug message: %s (%s:%u)\n", message, func, line);
}

/* Prints a MESSAGE to dump_file if exists.  */
#define DUMP_MESSAGE(message) dump_message (message, __func__, __LINE__)

/* Logs a MESSAGE to dump_file if exists and returns false. FUNC is name
   of function and LINE is location in the source file.  */

static inline bool
return_false_with_message (const char *message, const char *func,
			   unsigned int line)
{
  if (dump_file && (dump_flags & TDF_DETAILS))
    fprintf (dump_file, "  false returned: '%s' (%s:%u)\n", message, func, line);
  return false;
}

/* Logs a MESSAGE to dump_file if exists and returns false.  */
#define RETURN_FALSE_WITH_MSG(message) \
  return_false_with_message (message, __func__, __LINE__)

/* Return false and log that false value is returned.  */
#define RETURN_FALSE() RETURN_FALSE_WITH_MSG ("")

/* Logs return value if RESULT is false. FUNC is name of function and LINE
   is location in the source file.  */

static inline bool
return_with_result (bool result, const char *func, unsigned int line)
{
  if (!result && dump_file && (dump_flags & TDF_DETAILS))
    fprintf (dump_file, "  false returned (%s:%u)\n", func, line);

  return result;
}

/* Logs return value if RESULT is false.  */
#define RETURN_WITH_DEBUG(result) return_with_result (result, __func__, __LINE__)

/* Verbose logging function logging statements S1 and S2 of a CODE.
   FUNC is name of function and LINE is location in the source file.  */

static inline bool
return_different_stmts (gimple s1, gimple s2, const char *code,
			const char *func, unsigned int line)
{
  if (dump_file && (dump_flags & TDF_DETAILS))
    {
      fprintf (dump_file, "  different statement for code: %s (%s:%u):\n",
	       code, func, line);

      print_gimple_stmt (dump_file, s1, 3, TDF_DETAILS);
      print_gimple_stmt (dump_file, s2, 3, TDF_DETAILS);
    }

  return false;
}

/* Verbose logging function logging statements S1 and S2 of a CODE.  */
#define RETURN_DIFFERENT_STMTS(s1, s2, code) \
  return_different_stmts (s1, s2, code, __func__, __LINE__)

namespace ipa_icf {

class sem_item;

/* A class aggregating all connections and semantic equivalents
   for a given pair of semantic function candidates.  */
class func_checker
{
public:
  /* Initialize internal structures according to given number of
     source and target SSA names. The number of source names is SSA_SOURCE,
     respectively SSA_TARGET.  */
  func_checker (unsigned ssa_source, unsigned ssa_target,
		bool compare_polymorphic);

  /* Memory release routine.  */
  ~func_checker();

  /* Verifies that trees T1 and T2 are equivalent from perspective of ICF.  */
  bool compare_ssa_name (tree t1, tree t2, bool strict = true);

  /* Verification function for edges E1 and E2.  */
  bool compare_edge (edge e1, edge e2);

  /* Verification function for declaration trees T1 and T2 that
     come from functions FUNC1 and FUNC2.  */
  bool compare_decl (tree t1, tree t2, tree func1, tree func2);

private:
  /* Vector mapping source SSA names to target ones.  */
  vec <int> m_source_ssa_names;

  /* Vector mapping target SSA names to source ones.  */
  vec <int> m_target_ssa_names;

  /* Source to target edge map.  */
  hash_map <edge, edge> m_edge_map;

  /* Source to target declaration map.  */
  hash_map <tree, tree> m_decl_map;

  /* Flag if polymorphic comparison should be executed.  */
  bool m_compare_polymorphic;
};

/* Congruence class encompasses a collection of either functions or
   read-only variables. These items are considered to be equivalent
   if not proved the oposite.  */
class congruence_class
{
public:
  /* Congruence class constructor for a new class with _ID.  */
  congruence_class (unsigned int _id): id(_id)
  {
    members.create (2);
  }

  /* Destructor.  */
  ~congruence_class ()
  {
    members.release ();
  }

  /* Dump function prints all class members to a FILE with an INDENT.  */
  void dump (FILE *file, unsigned int indent = 0) const;

  /* Returns true if there's a member that is used from another group.  */
  bool is_class_used (void);

  /* Vector of all group members.  */
  vec <sem_item *> members;

  /* Global unique class identifier.  */
  unsigned int id;
};

/* Semantic item type enum.  */
enum sem_item_type
{
  FUNC,
  VAR
};

/* Semantic item usage pair.  */
class sem_usage_pair
{
public:
  /* Constructor for key value pair, where _ITEM is key and _INDEX is a target.  */
  sem_usage_pair (sem_item *_item, unsigned int _index);

  /* Target semantic item where an item is used.  */
  sem_item *item;

  /* Index of usage of such an item.  */
  unsigned int index;
};

/* Basic block struct for semantic equality pass.  */
class sem_bb
{
public:
  sem_bb (basic_block bb_, unsigned nondbg_stmt_count_, unsigned nondbg_nonlocal_stmt_count_, unsigned edge_count_):
    bb (bb_), nondbg_stmt_count (nondbg_stmt_count_), nondbg_nonlocal_stmt_count(nondbg_nonlocal_stmt_count_), edge_count (edge_count_) {}

  /* Basic block the structure belongs to.  */
  basic_block bb;

  /* Number of non-debug statements in the basic block.  */
  unsigned nondbg_stmt_count;
  
  unsigned nondbg_nonlocal_stmt_count;

  /* Number of edges connected to the block.  */
  unsigned edge_count;
};

/* Semantic item is a base class that encapsulates all shared functionality
   for both semantic function and variable items.  */
class sem_item
{
public:
  /* Semantic item constructor for a node of _TYPE, where STACK is used
     for bitmap memory allocation.  */
  sem_item (sem_item_type _type, bitmap_obstack *stack);

  /* Semantic item constructor for a node of _TYPE, where STACK is used
     for bitmap memory allocation. The item is based on symtab node _NODE
     with computed _HASH.  */
  sem_item (sem_item_type _type, symtab_node *_node, hashval_t _hash,
	    bitmap_obstack *stack);

  virtual ~sem_item ();

  /* Dump function for debugging purpose.  */
  DEBUG_FUNCTION void dump (void);

  /* Initialize semantic item by info reachable during LTO WPA phase.  */
  virtual void init_wpa (void) = 0;

  /* Semantic item initialization function.  */
  virtual void init (void) = 0;

  /* Gets symbol name of the item.  */
  const char *name (void)
  {
    return node->name ();
  }

  /* Gets assembler name of the item.  */
  const char *asm_name (void)
  {
    return node->asm_name ();
  }

  /* Initialize references to other semantic functions/variables.  */
  virtual void init_refs () = 0;

  /* Fast equality function based on knowledge known in WPA.  */
  virtual bool equals_wpa (sem_item *item) = 0;

  /* Returns true if the item equals to ITEM given as arguemnt.  */
  virtual bool equals (sem_item *item) = 0;

  /* References independent hash function.  */
  virtual hashval_t get_hash (void) = 0;

  /* Merges instance with an ALIAS_ITEM, where alias, thunk or redirection can
     be applied.  */
  virtual bool merge (sem_item *alias_item) = 0;

  /* Dump symbol to FILE.  */
  virtual void dump_to_file (FILE *file) = 0;

  /* Return base tree that can be used for types_compatible_p and
     contains_polymorphic_type_p comparison.  */

  static bool get_base_types (tree *t1, tree *t2);

  /* Return true if types are compatible from perspective of ICF.
     FIRST_ARGUMENT indicates if the comparison is called for
     first parameter of a function.  */
  static bool types_are_compatible_p (tree t1, tree t2,
				      bool compare_polymorphic = true,
				      bool first_argument = false);

  /* Item type.  */
  sem_item_type type;

  /* Global unique function index.  */
  unsigned int index;

  /* Symtab node.  */
  symtab_node *node;

  /* Declaration tree node.  */
  tree decl;

  /* Semantic references used that generate congruence groups.  */
  vec <sem_item *> refs;

  /* Pointer to a congruence class the item belongs to.  */
  congruence_class *cls;

  /* Index of the item in a class belonging to.  */
  unsigned int index_in_class;

  /* List of semantic items where the instance is used.  */
  vec <sem_usage_pair *> usages;

  /* A bitmap with indices of all classes referencing this item.  */
  bitmap usage_index_bitmap;

  /* List of tree references (either FUNC_DECL or VAR_DECL).  */
  vec <tree> tree_refs;

  /* A set with tree references (either FUNC_DECL or VAR_DECL).  */
  hash_set <tree> tree_refs_set;

protected:
  /* Cached, once calculated hash for the item.  */
  hashval_t hash;

private:
  /* Initialize internal data structures. Bitmap STACK is used for
     bitmap memory allocation process.  */
  void setup (bitmap_obstack *stack);
}; // class sem_item

class sem_function: public sem_item
{
public:
  /* Semantic function constructor that uses STACK as bitmap memory stack.  */
  sem_function (bitmap_obstack *stack);

  /*  Constructor based on callgraph node _NODE with computed hash _HASH.
      Bitmap STACK is used for memory allocation.  */
  sem_function (cgraph_node *_node, hashval_t _hash, bitmap_obstack *stack);

  ~sem_function ();

  inline virtual void init_wpa (void)
  {
    parse_tree_args ();
  }

  virtual void init (void);
  virtual bool equals_wpa (sem_item *item);
  virtual hashval_t get_hash (void);
  virtual bool equals (sem_item *item);
  virtual void init_refs ();
  virtual bool merge (sem_item *alias_item);

  /* Dump symbol to FILE.  */
  virtual void dump_to_file (FILE *file)
  {
    gcc_assert (file);
    dump_function_to_file (decl, file, TDF_DETAILS);
  }

  /* Parses function arguments and result type.  */
  void parse_tree_args (void);

  /* Returns cgraph_node.  */
  inline cgraph_node *get_node (void)
  {
    return dyn_cast <cgraph_node *> (node);
  }

  /* Improve accumulated hash for HSTATE based on a gimple statement STMT.  */
  void improve_hash (inchash::hash *inchash, gimple stmt);

  /* Return true if polymorphic comparison must be processed.  */
  bool compare_polymorphic_p (void);

  /* For a given call graph NODE, the function constructs new
     semantic function item.  */
  static sem_function *parse (cgraph_node *node, bitmap_obstack *stack);

  /* Exception handling region tree.  */
  eh_region region_tree;

  /* Result type tree node.  */
  tree result_type;

  /* Array of argument tree types.  */
  vec <tree> arg_types;

  /* Number of function arguments.  */
  unsigned int arg_count;

  /* Total amount of edges in the function.  */
  unsigned int edge_count;

  /* Vector of sizes of all basic blocks.  */
  vec <unsigned int> bb_sizes;

  /* Control flow graph checksum.  */
  hashval_t cfg_checksum;

  /* GIMPLE codes hash value.  */
  hashval_t gcode_hash;

  /* Total number of SSA names used in the function.  */
  unsigned ssa_names_size;

  /* Array of structures for all basic blocks.  */
  vec <sem_bb *> bb_sorted;


  /* Function checker stores binding between functions.   */
  func_checker *m_checker;
  /* Basic block equivalence comparison function that returns true if
     basic blocks BB1 and BB2 (from functions FUNC1 and FUNC2) correspond.  */
  bool compare_bb (sem_bb *bb1, sem_bb *bb2, tree func1, tree func2, bool skip_local_defs = true);


private:
  /* Calculates hash value based on a BASIC_BLOCK.  */
  hashval_t get_bb_hash (const sem_bb *basic_block);


  /* For given basic blocks BB1 and BB2 (from functions FUNC1 and FUNC),
     true value is returned if phi nodes are semantically
     equivalent in these blocks .  */
  bool compare_phi_node (basic_block bb1, basic_block bb2, tree func1,
			 tree func2);

  /* For given basic blocks BB1 and BB2 (from functions FUNC1 and FUNC),
     true value is returned if exception handling regions are equivalent
     in these blocks.  */
  bool compare_eh_region (eh_region r1, eh_region r2, tree func1, tree func2);

  /* Verifies that trees T1 and T2, representing function declarations
     are equivalent from perspective of ICF.  */
  bool compare_function_decl (tree t1, tree t2);

  /* Verifies that trees T1 and T2 do correspond.  */
  bool compare_variable_decl (tree t1, tree t2, tree func1, tree func2);

  /* Verifies for given GIMPLEs S1 and S2 (from function FUNC1, resp. FUNC2) that
     call statements are semantically equivalent.  */
  bool compare_gimple_call (gimple s1, gimple s2,
			    tree func1, tree func2);

  /* Verifies for given GIMPLEs S1 and S2 (from function FUNC1, resp. FUNC2) that
     assignment statements are semantically equivalent.  */
  bool compare_gimple_assign (gimple s1, gimple s2, tree func1, tree func2);

  /* Verifies for given GIMPLEs S1 and S2 (from function FUNC1, resp. FUNC2) that
     condition statements are semantically equivalent.  */
  bool compare_gimple_cond (gimple s1, gimple s2, tree func1, tree func2);

  /* Verifies for given GIMPLEs S1 and S2 (from function FUNC1, resp. FUNC2) that
     label statements are semantically equivalent.  */
  bool compare_gimple_label (gimple s1, gimple s2, tree func1, tree func2);

  /* Verifies for given GIMPLEs S1 and S2 (from function FUNC1, resp. FUNC2) that
     switch statements are semantically equivalent.  */
  bool compare_gimple_switch (gimple s1, gimple s2, tree func1, tree func2);

  /* Verifies for given GIMPLEs S1 and S2 (from function FUNC1, resp. FUNC2) that
     return statements are semantically equivalent.  */
  bool compare_gimple_return (gimple s1, gimple s2, tree func1, tree func2);

  /* Verifies for given GIMPLEs S1 and S2 (from function FUNC1, resp. FUNC2) that
     goto statements are semantically equivalent.  */
  bool compare_gimple_goto (gimple s1, gimple s2, tree func1, tree func2);

  /* Verifies for given GIMPLEs S1 and S2 (from function FUNC1, resp. FUNC2) that
     resx statements are semantically equivalent.  */
  bool compare_gimple_resx (gimple s1, gimple s2);

  /* Verifies for given GIMPLEs S1 and S2 that ASM statements are equivalent.
     For the beginning, the pass only supports equality for
     '__asm__ __volatile__ ("", "", "", "memory")'.  */
  bool compare_gimple_asm (gimple s1, gimple s2);

  /* Verifies that tree labels T1 and T2 correspond in FUNC1 and FUNC2.  */
  bool compare_tree_ssa_label (tree t1, tree t2, tree func1, tree func2);

  /* Function compares two operands T1 and T2 and returns true if these
     two trees from FUNC1 (respectively FUNC2) are semantically equivalent.  */
  bool compare_operand (tree t1, tree t2, tree func1, tree func2, bool strict = true);

  /* If T1 and T2 are SSA names, dictionary comparison is processed. Otherwise,
     declaration comparasion is executed.  */
  bool compare_ssa_name (tree t1, tree t2, tree func1, tree func2, bool strict = true);

  /* Basic blocks dictionary BB_DICT returns true if SOURCE index BB
     corresponds to TARGET.  */
  bool bb_dict_test (int* bb_dict, int source, int target);

  /* Iterates all tree types in T1 and T2 and returns true if all types
     are compatible. If COMPARE_POLYMORPHIC is set to true,
     more strict comparison is executed.  */
  bool compare_type_list (tree t1, tree t2, bool compare_polymorphic);

  /* Processes function equality comparison.  */
  bool equals_private (sem_item *item);

  /* Initialize references to another sem_item for gimple STMT of type assign.  */
  void init_refs_for_assign (gimple stmt);

  /* Initialize references to another sem_item for tree T.  */
  void init_refs_for_tree (tree t);

  /* Returns true if tree T can be compared as a handled component.  */
  static bool icf_handled_component_p (tree t);

  /* COMPARED_FUNC is a function that we compare to.  */
  sem_function *m_compared_func;
}; // class sem_function

class sem_variable: public sem_item
{
public:
  /* Semantic variable constructor that uses STACK as bitmap memory stack.  */
  sem_variable (bitmap_obstack *stack);

  /*  Constructor based on callgraph node _NODE with computed hash _HASH.
      Bitmap STACK is used for memory allocation.  */

  sem_variable (varpool_node *_node, hashval_t _hash, bitmap_obstack *stack);

  inline virtual void init_wpa (void) {}

  /* Semantic variable initialization function.  */
  inline virtual void init (void)
  {
    decl = get_node ()->decl;
    ctor = ctor_for_folding (decl);
  }

  /* Initialize references to other semantic functions/variables.  */
  inline virtual void init_refs ()
  {
    parse_tree_refs (ctor);
  }

  virtual hashval_t get_hash (void);
  virtual bool merge (sem_item *alias_item);
  virtual void dump_to_file (FILE *file);
  virtual bool equals (sem_item *item);

  /* Fast equality variable based on knowledge known in WPA.  */
  inline virtual bool equals_wpa (sem_item *item)
  {
    gcc_assert (item->type == VAR);
    return true;
  }

  /* Returns varpool_node.  */
  inline varpool_node *get_node (void)
  {
    return dyn_cast <varpool_node *> (node);
  }

  /* Parser function that visits a varpool NODE.  */
  static sem_variable *parse (varpool_node *node, bitmap_obstack *stack);

  /* Variable constructor.  */
  tree ctor;

private:
  /* Iterates though a constructor and identifies tree references
     we are interested in semantic function equality.  */
  void parse_tree_refs (tree t);

  /* Compares trees T1 and T2 for semantic equality.  */
  static bool equals (tree t1, tree t2);

  /* Compare that symbol sections are either NULL or have same name.  */
  bool compare_sections (sem_variable *alias);

}; // class sem_variable

class sem_item_optimizer;

/* Congruence class set structure.  */
struct congruence_class_var_hash: typed_noop_remove <congruence_class>
{
  typedef congruence_class value_type;
  typedef congruence_class compare_type;

  static inline hashval_t hash (const value_type *item)
  {
    return htab_hash_pointer (item);
  }

  static inline int equal (const value_type *item1, const compare_type *item2)
  {
    return item1 == item2;
  }
};

struct congruence_class_group
{
  hashval_t hash;
  sem_item_type type;
  vec <congruence_class *> classes;
};

/* Congruence class set structure.  */
struct congruence_class_group_hash: typed_noop_remove <congruence_class_group>
{
  typedef congruence_class_group value_type;
  typedef congruence_class_group compare_type;

  static inline hashval_t hash (const value_type *item)
  {
    return item->hash;
  }

  static inline int equal (const value_type *item1, const compare_type *item2)
  {
    return item1->hash == item2->hash && item1->type == item2->type;
  }
};

struct traverse_split_pair
{
  sem_item_optimizer *optimizer;
  class congruence_class *cls;
};

/* Semantic item optimizer includes all top-level logic
   related to semantic equality comparison.  */
class sem_item_optimizer
{
public:
  sem_item_optimizer ();
  ~sem_item_optimizer ();

  /* Function responsible for visiting all potential functions and
     read-only variables that can be merged.  */
  void parse_funcs_and_vars (void);

  /* Optimizer entry point.  */
  void execute (void);

  /* Dump function. */
  void dump (void);

  /* Verify congruence classes if checking is enabled.  */
  void verify_classes (void);

  /* Write IPA ICF summary for symbols.  */
  void write_summary (void);

  /* Read IPA IPA ICF summary for symbols.  */
  void read_summary (void);

  /* Callgraph removal hook called for a NODE with a custom DATA.  */
  static void cgraph_removal_hook (cgraph_node *node, void *data);

  /* Varpool removal hook called for a NODE with a custom DATA.  */
  static void varpool_removal_hook (varpool_node *node, void *data);

  /* Worklist of congruence classes that can potentially
     refine classes of congruence.  */
  hash_table <congruence_class_var_hash> worklist;

  /* Remove symtab NODE triggered by symtab removal hooks.  */
  void remove_symtab_node (symtab_node *node);

  /* Register callgraph and varpool hooks.  */
  void register_hooks (void);

  /* Unregister callgraph and varpool hooks.  */
  void unregister_hooks (void);

  /* Adds a CLS to hashtable associated by hash value.  */
  void add_class (congruence_class *cls);

  /* Gets a congruence class group based on given HASH value and TYPE.  */
  congruence_class_group *get_group_by_hash (hashval_t hash,
      sem_item_type type);

private:

  /* Congruence classes are built by hash value.  */
  void build_hash_based_classes (void);

  /* Semantic items in classes having more than one element and initialized.
     In case of WPA, we load function body.  */
  void parse_nonsingleton_classes (void);

  /* Equality function for semantic items is used to subdivide existing
     classes. If IN_WPA, fast equality function is invoked.  */
  void subdivide_classes_by_equality (bool in_wpa = false);

  /* Debug function prints all informations about congruence classes.  */
  void dump_cong_classes (void);

  /* Iterative congruence reduction function.  */
  void process_cong_reduction (void);

  /* After reduction is done, we can declare all items in a group
     to be equal. PREV_CLASS_COUNT is start number of classes
     before reduction.  */
  void merge_classes (unsigned int prev_class_count);

  /* Adds a newly created congruence class CLS to worklist.  */
  void worklist_push (congruence_class *cls);

  /* Pops a class from worklist. */
  congruence_class *worklist_pop ();

  /* Returns true if a congruence class CLS is present in worklist.  */
  inline bool worklist_contains (const congruence_class *cls)
  {
    return worklist.find (cls);
  }

  /* Removes given congruence class CLS from worklist.  */
  inline void worklist_remove (const congruence_class *cls)
  {
    worklist.remove_elt (cls);
  }

  /* Every usage of a congruence class CLS is a candidate that can split the
     collection of classes. Bitmap stack BMSTACK is used for bitmap
     allocation.  */
  void do_congruence_step (congruence_class *cls);

  /* Tests if a class CLS used as INDEXth splits any congruence classes.
     Bitmap stack BMSTACK is used for bitmap allocation.  */
  void do_congruence_step_for_index (congruence_class *cls, unsigned int index);

  /* Makes pairing between a congruence class CLS and semantic ITEM.  */
  static void add_item_to_class (congruence_class *cls, sem_item *item);

  /* Disposes split map traverse function. CLS is congruence
     class, BSLOT is bitmap slot we want to release. DATA is mandatory,
     but unused argument.  */
  static bool release_split_map (congruence_class * const &cls, bitmap const &b,
				 traverse_split_pair *pair);

  /* Process split operation for a cognruence class CLS,
     where bitmap B splits congruence class members. DATA is used
     as argument of split pair.  */
  static bool traverse_congruence_split (congruence_class * const &cls,
					 bitmap const &b,
					 traverse_split_pair *pair);

  /* Reads a section from LTO stream file FILE_DATA. Input block for DATA
     contains LEN bytes.  */
  void read_section (lto_file_decl_data *file_data, const char *data,
		     size_t len);

  /* Removes all callgraph and varpool nodes that are marked by symtab
     as deleted.  */
  void filter_removed_items (void);

  /* Vector of semantic items.  */
  vec <sem_item *> m_items;

  /* A set containing all items removed by hooks.  */
  hash_set <symtab_node *> m_removed_items_set;

  /* Hashtable of congruence classes */
  hash_table <congruence_class_group_hash> m_classes;

  /* Count of congruence classes.  */
  unsigned int m_classes_count;

  /* Map data structure maps trees to semantic items.  */
  hash_map <tree, sem_item *> m_decl_map;

  /* Map data structure maps symtab nodes to semantic items.  */
  hash_map <symtab_node *, sem_item *> m_symtab_node_map;

  /* Set to true if a splitter class is removed.  */
  bool splitter_class_removed;

  /* Global unique class id counter.  */
  static unsigned int class_id;

  /* Callgraph node removal hook holder.  */
  cgraph_node_hook_list *m_cgraph_node_hooks;

  /* Varpool node removal hook holder.  */
  varpool_node_hook_list *m_varpool_node_hooks;

  /* Bitmap stack.  */
  bitmap_obstack m_bmstack;
}; // class sem_item_optimizer

} // ipa_icf namespace
