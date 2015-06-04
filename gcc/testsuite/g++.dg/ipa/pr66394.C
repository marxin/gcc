/* { dg-do compile } */
/* { dg-options "-fmerge-all-constants -flto -fpermissive -std=c++11" } */

template <typename Cvt> void CvtColorLoop(int, int, const Cvt);
enum { R2Y, G2Y, B2Y };
struct RGB2Gray {
  RGB2Gray(int, int, const int *coeffs) {
    const int coeffs0[]{G2Y, B2Y};
    coeffs = coeffs0;
    tab[2] = *coeffs;
  }
  int tab[];
};

void
cvtColor_bidx() {
  int src, dst;
  const int *a = &src;
  RGB2Gray(0, dst, a);
  CvtColorLoop(src, dst, 0);
}

