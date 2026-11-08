// exit: 52
struct S { int a, b; };
struct T { int *p, q; };

int main(void) {
  struct S s;
  struct T t;
  int x;
  s.a = 20;
  s.b = 22;
  t.p = &x;
  t.q = 7;
  *t.p = 3;
  return s.a + s.b + t.q + x;
}
