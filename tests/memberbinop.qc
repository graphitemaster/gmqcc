.vector k;

void t(entity e)
{
    e.k = '0 0 0';
    e.k.x += 2;
    e.k.y += 4;
    e.k.z += 6;
}

void main() {
    entity e = spawn();
    t(e);
    print(vtos(e.k));
}
