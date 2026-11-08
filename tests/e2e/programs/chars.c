// Character truncation, sign extension and sizeof.
// exit: 48
struct S {
	char a;
	int b;
};
int main(void) {
	char c;
	int n;
	c = 200;
	n = c;
	if (n < 0)
		n = -n;
	return n + sizeof(struct S) + sizeof(char) + sizeof(int) - 21;
}
