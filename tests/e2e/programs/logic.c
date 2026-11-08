// Short-circuit evaluation and the conditional operator.
// exit: 14
int calls;
int bump(void) {
	calls = calls + 1;
	return 1;
}
int zero(void) {
	calls = calls + 1;
	return 0;
}
int main(void) {
	int a;
	a = 0;
	if (a && bump())
		return 1;
	if (1 || bump())
		a = calls;
	if (zero() || bump())
		a = a + 10;
	a = a + (a ? 1 : 2);
	a = a + (0 ? 100 : 1);
	return a + calls;
}
