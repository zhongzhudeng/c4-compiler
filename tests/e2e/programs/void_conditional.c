// Void conditional expressions in discarded-value contexts.
// exit: 42
void set(int *p, int v) {
	*p = v;
}

int main(void) {
	int g;
	int h;
	int x;
	g = 0;
	h = 0;
	x = 1;
	x ? set(&g, 40) : set(&g, 1);
	0 ? set(&h, 2) : set(&h, 0);
	return g + h + 2;
}
