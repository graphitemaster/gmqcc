float a = 1000;
float b = 1001;
float c = 1002;

void test(float b) {
	float c = 3002;
	print(ftos(a), "\n");
	print(ftos(b), "\n");
	print(ftos(c), "\n");
	{
		float b = 4001;
		print(ftos(b), "\n");
	}
}

void main() {
	test(2001);
}
