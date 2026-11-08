// Recursive struct type through a pointer.
// exit: 42
struct S {
	int a;
	struct S *self;
};
int main(void) {
	struct S s;
	s.self = &s;
	s.a = 42;
	return s.self->self->a;
}
