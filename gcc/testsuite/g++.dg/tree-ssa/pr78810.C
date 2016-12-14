/* { dg-do compile } */
/* { dg-options "-O2 -fdump-tree-optimized" } */

class NODE;
typedef unsigned long VALUE;

struct parser_params
{
};

struct parser_params *get_parser();
NODE *yycompile(parser_params *parser, VALUE a, VALUE b);

NODE*
rb_parser_compile_file_path(VALUE vparser, VALUE fname, VALUE file, int start)
{
    struct parser_params *parser;
    parser = get_parser(); 

    NODE *node = yycompile(parser, fname, start);
    (*({volatile VALUE *rb_gc_guarded_ptr = (&(vparser)); rb_gc_guarded_ptr;}));

    return node;
}

/* { dg-final { scan-tree-dump "MEM\\\[\\\(volatile\\\ VALUE\\\ \\\*\\\)" "optimized" } } */
