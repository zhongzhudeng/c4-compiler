// Block scoping and shadowing.
// exit: 42
int main(void) {
	int x;
	x = 1;
	{
		int x;
		x = 40;
		{
			int y;
			y = 1;
			x = x + y;
		}
	}
	return x + 41;
}
