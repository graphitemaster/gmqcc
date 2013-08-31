void main(vector v) {
    print(vtos(v), "\n");
    v /= 2;
    print(vtos(v), "\n");
    print(vtos(v / 2), "\n");
    print(vtos(v), "\n");
    print(vtos(v | 16), "\n");
    print(vtos(v & 16), "\n");
    print(vtos(v | '25 42 51'), "\n");
    print(vtos(v & '25 42 51'), "\n");
    print(vtos('1 2 3' >< '3 2 1'));
}
