/* PR middle-end/81318 */

__attribute__((__cold__)) void a();
int d(void);
int e(void);

void b() { a(); }

void c() {
  b();
  if (d())
    e();
}
