/* PR tree-optimization/92005 */
/* { dg-options "-O2 -fdump-tree-optimized -std=c++17" } */

template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

struct T0 {};
struct T1 {};
struct T2 {};
struct T3 {};
struct T4 {};

struct variant
{
    unsigned index_;

    union
    {
        T0 t0_;
        T1 t1_;
        T2 t2_;
        T3 t3_;
        T4 t4_;
    };
};

template<class F> int visit( F f, variant const& v )
{
    switch( v.index_ )
    {
        case 0: return f( v.t0_ );
        case 1: return f( v.t1_ );
        case 2: return f( v.t2_ );
        case 3: return f( v.t3_ );
        case 4: return f( v.t4_ );
        default: __builtin_unreachable();
    }
}

int do_visit(variant const& v) {
     return visit(overloaded{
        [](T0 val) { return 3; },
        [](T1 val) { return 5; },
        [](T2 val) { return 8; },
        [](T3 val) { return 9; },
        [](T4 val) { return 10; }
    }, v);
}

/* { dg-final { scan-tree-dump "CSWTCH" "optimized" } } */
