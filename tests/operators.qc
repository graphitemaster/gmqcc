void   print(...)   = #1;
string ftos (float) = #2;
entity() spawn = #3;

.float mem;

void main() {
	float a;

	// regular binary+store
	a = 5;
	print(ftos(a += 1), " = ");
	print(ftos(a), "\n");

	entity e = spawn();
    e.mem = 10;
	print(ftos(e.mem += 1), " = ");
	print(ftos(e.mem), "\n");

    // prefix
	print(ftos(++a), " = ");
	print(ftos(a), "\n");
	print(ftos(--a), " = ");
	print(ftos(a), "\n");
	print(ftos(++e.mem), " = ");
	print(ftos(e.mem), "\n");

	// suffix
	print(ftos(a++), " = ");
	print(ftos(a-1), "\n");
	// the CLANG way:
	a = 3;
	print(ftos((a++ + a) + a), " = 11\n");

	// check if minus translates
	print(ftos(a--), "\n");
	print(ftos(--a), "\n");

	// postfix on members
	print(ftos(e.mem--), " = ");
	print(ftos(e.mem+1), "\n");
}
