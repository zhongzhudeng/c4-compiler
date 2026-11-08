// Consecutive definitions with identical local and label names.
// exit: 42
int f(void) {
	int x;
	x = 20;
	goto done;
	x = 99;
done:
	return x;
}

int g(void) {
	int x;
	x = 22;
	goto done;
	x = 99;
done:
	return x;
}

int main(void) {
	return f() + g();
}
