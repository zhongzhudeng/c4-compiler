// Mixed char/int conditional arms, both operand orders and nesting.
// exit: 7
int f(int x) {
	char c;
	c = 1;
	return x ? c : 2;
}

int g(int x) {
	char c;
	c = 3;
	return x ? 2 : c;
}

int h(int x) {
	char c;
	c = 4;
	return x ? (x ? c : 2) : 5;
}

int main(void) {
	int r;
	r = 0;
	r = r + f(1);
	r = r + f(0);
	r = r + g(1);
	r = r + g(0);
	r = r + h(1);
	r = r + h(0);
	return r - 10;
}
