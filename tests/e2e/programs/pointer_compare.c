// Pointer comparisons and pointer arithmetic on one object.
// exit: 42
int main(void) {
	char *s;
	char *p;
	char *q;
	s = "abcdef";
	p = s;
	q = s + 3;
	if (p < q) {
		if (p == q)
			return 2;
		if (p != q)
			return 42;
	}
	return 4;
}
