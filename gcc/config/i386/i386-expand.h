#ifndef GCC_I386_EXPAND_H
#define GCC_I386_EXPAND_H

/* AVX512F does support 64-byte integer vector operations,
   thus the longest vector we are faced with is V64QImode.  */
#define MAX_VECT_LEN	64

struct expand_vec_perm_d
{
  rtx target, op0, op1;
  unsigned char perm[MAX_VECT_LEN];
  machine_mode vmode;
  unsigned char nelt;
  bool one_operand_p;
  bool testing_p;
};

rtx legitimize_tls_address (rtx x, enum tls_model model, bool for_mov);
alias_set_type ix86_GOT_alias_set (void);
rtx legitimize_pic_address (rtx orig, rtx reg);
rtx legitimize_pe_coff_symbol (rtx addr, bool inreg);
void predict_jump (int prob);
bool insn_defines_reg (unsigned int regno1, unsigned int regno2,
		       rtx_insn *insn);
void ix86_emit_binop (enum rtx_code code, machine_mode mode, rtx dst, rtx src);
bool ix86_expand_vector_init_one_nonzero (bool mmx_ok, machine_mode mode,
					  rtx target, rtx var, int one_var);
rtx ix86_expand_compare (enum rtx_code code, rtx op0, rtx op1);
rtx_code_label *ix86_expand_sse_compare_and_jump (enum rtx_code code, rtx op0,
						  rtx op1, bool swap_operands);
bool ix86_unordered_fp_compare (enum rtx_code code);
// TODO: maybe remove
rtx ix86_expand_sse_cmp (rtx dest, enum rtx_code code, rtx cmp_op0,
			 rtx cmp_op1, rtx op_true, rtx op_false);
// TODO
bool
ix86_expand_vec_perm_vpermt2 (rtx target, rtx mask, rtx op0, rtx op1,
			      struct expand_vec_perm_d *d);
enum calling_abi ix86_function_abi (const_tree fndecl);
bool ix86_function_ms_hook_prologue (const_tree fn);
void warn_once_call_ms2sysv_xlogues (const char *feature);
rtx gen_push (rtx arg);
rtx gen_pop (rtx arg);
tree fold_builtin_cpu (tree fndecl, tree *args);
rtx ix86_expand_builtin (tree exp, rtx target, rtx subtarget,
			 machine_mode mode, int ignore);
bool expand_vec_perm_1 (struct expand_vec_perm_d *d);
// TODO
bool expand_vec_perm_broadcast_1 (struct expand_vec_perm_d *d);
bool expand_vec_perm_broadcast (struct expand_vec_perm_d *d);
// TODO
machine_mode get_mode_wider_vector (machine_mode o);
// TODO
rtx ix86_expand_sse_fabs (rtx op0, rtx *smask);
// TODO
rtx ix86_expand_sse_compare_mask (enum rtx_code code, rtx op0, rtx op1,
				  bool swap_operands);
bool ix86_vectorize_vec_perm_const (machine_mode vmode, rtx target, rtx op0,
				    rtx op1, const vec_perm_indices &sel);
bool ix86_notrack_prefixed_insn_p (rtx insn);
machine_mode ix86_split_reduction (machine_mode mode);
void ix86_expand_divmod_libfunc (rtx libfunc, machine_mode mode, rtx op0,
				 rtx op1, rtx *quot_p, rtx *rem_p);

#endif  /* GCC_I386_EXPAND_H */
