// A block-scope function declaration hides an outer block-local object.
// exit: 42
int f(void) {
	return 42;
}

int main(void) {
	int f;
	f = 7;
	{
		int f(void);
		return f();
	}
}
