// String literals, pointer walks and output.
// stdout: Hello, C4!
// exit: 10
int putchar(int c);

int main(void) {
	char *s;
	char *p;
	s = "Hello, C4!";
	p = s;
	while (*p) {
		putchar(*p);
		p = p + 1;
	}
	putchar('\n');
	return p - s;
}
