// Command-line arguments.
// args: alpha beta
// exit: 31
int main(int argc, char **argv) {
	char *p;
	int n;
	n = 0;
	p = argv[0];
	while (*p) {
		n = n + 1;
		p = p + 1;
	}
	return argc * 10 + (0 < n) * 1;
}
