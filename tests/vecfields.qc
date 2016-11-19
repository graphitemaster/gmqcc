.vector v1;

float set(entity e, float v) {
	e.v1.y = v;
	return e.v1.y;
}

void main() {
	entity e = spawn();
	e.v1 = '1 2 3';
	print(ftos(set(e, 42)), " => ");
	print(vtos(e.v1), "\n");
}
