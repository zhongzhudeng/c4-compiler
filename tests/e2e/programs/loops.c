// Nested loops with continue in the inner loop.
// exit: 42
int main(void) {
	int i;
	int j;
	int total;
	total = 0;
	i = 0;
	while (i < 3) {
		j = 0;
		while (j < 3) {
			j = j + 1;
			if (j == 2)
				continue;
			total = total + i * 10 + j;
		}
		i = i + 1;
	}
	return total - 30;
}
