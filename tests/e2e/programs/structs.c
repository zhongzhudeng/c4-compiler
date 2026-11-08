// Struct members, nested structs and arrow access.
// exit: 42
struct Inner {
	int v;
	char c;
};

struct Outer {
	struct Inner in;
	int w;
};

int main(void) {
	struct Outer o;
	struct Outer *p;
	p = &o;
	o.in.v = 20;
	p->in.c = 'A';
	p->w = 2;
	return p->in.v + p->w + 20;
}
