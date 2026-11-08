// if/else, while, break, continue and goto.
// exit: 30
int main(void) {
	int i;
	int sum;
	sum = 0;
	i = 0;
	while (i < 10) {
		i = i + 1;
		if (i == 2)
			continue;
		if (i == 8)
			break;
		sum = sum + i;
	}
	if (sum == 26)
		sum = sum + 4;
	else
		sum = 0;
	goto finish;
	sum = 99;
finish:
	return sum;
}
