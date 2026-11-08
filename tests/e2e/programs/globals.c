// File-scope objects, tentative definitions and global structs.
// exit: 42
int g;
int g;
struct P {
	int x;
	int y;
};
struct P gp;
int *ptr;

int main(void) {
	gp.x = 30;
	gp.y = 12;
	ptr = &gp.y;
	g = 5;
	return gp.x + *ptr - g + 5;
}
