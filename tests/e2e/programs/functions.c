// Recursion, multiple parameters, char parameters and void functions.
// exit: 31
int fact(int n) {
	if (n < 2)
		return 1;
	return n * fact(n - 1);
}

int add3(int a, int b, int c) {
	return a + b + c;
}

char next(char c) {
	return c + 1;
}

void set(int *p, int v) {
	*p = v;
}

int main(void) {
	int x;
	x = 0;
	set(&x, 20);
	return fact(3) + add3(1, 2, 3) + next('a') - 97 + x - 2;
}
