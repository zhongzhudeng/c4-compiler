// Function pointers, address-of a function and void pointers.
// exit: 42
struct Node {
	int v;
	struct Node *next;
};
int id(int x) {
	return x;
}
int main(void) {
	struct Node a;
	struct Node b;
	void *vp;
	int *ip;
	int (*fp)(int);
	a.next = &b;
	b.v = 40;
	fp = &id;
	vp = &b.v;
	ip = vp;
	return (*fp)(2) + *ip;
}
