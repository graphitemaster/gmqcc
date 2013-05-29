float f_float() {
    return = 100.0f;
    return = 200.0f;
    return;
}

vector f_vector() {
    return = '1 2 3';
    return = '2 3 4';
    return;
}

string f_string() {
    return = "hello";
    return = "world";
    return;
}

void main() {
    print(ftos(f_float()), "\n");  // 200.0f
    print(vtos(f_vector()), "\n"); // '1 2 3'
    print(f_string(), "\n");       // world
}
