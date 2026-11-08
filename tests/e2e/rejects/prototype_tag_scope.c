// error: conflicting types
// line: 5
int f(struct S *p);
struct S { int x; };
int f(struct S *p) { return 0; }
