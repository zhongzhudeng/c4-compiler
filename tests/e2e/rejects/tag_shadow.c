// error: incompatible type
// line: 9
struct S { int a; };
int main(void) {
	struct S *p;
	{
		struct S;
		struct S *q;
		p = q;
	}
	return 0;
}
