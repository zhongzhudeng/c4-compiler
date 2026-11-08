// Address-of, dereference and pointer assignment.
// exit: 42
int main(void) {
	int a;
	int b;
	int *p;
	int *q;
	a = 10;
	b = 32;
	p = &a;
	*p = *p + 5;
	q = &b;
	p = q;
	*p = *p - 2;
	return a + b - 3;
}
