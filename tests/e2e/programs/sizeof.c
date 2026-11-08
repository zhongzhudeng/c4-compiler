// sizeof of types, expressions, structs and pointers.
// exit: 42
struct S {
	char a;
	int b;
	char c;
};
int main(void) {
	int x;
	struct S s;
	int *p;
	char *q;
	x = 0;
	x = x + sizeof(int) * 100;
	x = x + sizeof(char) * 10;
	x = x + sizeof(struct S) * 10;
	x = x + sizeof(s) * 10;
	x = x + sizeof(p) * 10;
	x = x + sizeof(q) * 10;
	x = x + sizeof x;
	return x - 772;
}
