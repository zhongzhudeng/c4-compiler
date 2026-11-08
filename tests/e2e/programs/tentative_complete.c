// A tentative definition whose struct type is completed later in the file.
// exit: 12
struct S;
struct S g;
struct S { int x; };

int main(void) {
	g.x = 8;
	return g.x + sizeof(g);
}
