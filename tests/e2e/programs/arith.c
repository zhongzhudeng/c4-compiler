// Integer arithmetic, precedence, comparisons and unary operators.
// exit: 46
int main(void) {
	int a;
	int b;
	int c;
	a = 7;
	b = 3;
	c = a * b;
	c = c + 2 * (a - b);
	c = c - -4;
	c = c + (a < b) * 100;
	c = c + (a != b) * 10;
	c = c + (a == 7) * 1;
	c = c + (!0) * 2;
	c = c + (!a) * 100;
	return c;
}
