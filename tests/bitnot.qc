void main() {
    float a; a = 1;
    float b; b = 1;
    float c; c = 1;
    float d; d = 1;
    vector e; e = '1 1 1';
    vector f; f = '1 1 1';

    a &~= 1; // 0
    b &= ~1; // 0
    c &= ~d; // 0
    f &~= e; // '0 0 0'
    e &= ~e; // '0 0 0'

    print("a: ", ftos(a), "\nb: ",
                 ftos(b), "\nc: ",
                 ftos(c), "\n");
    print("e: ", vtos(e), "\n");
    print("f: ", vtos(f), "\n");
}
