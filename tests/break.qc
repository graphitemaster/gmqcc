void   print(...)   = #1;
string ftos (float) = #2;

void main(float brkat, float contat) {
    float i;

    for (i = 0; i < 10; i += 1) {
        if (i == contat) {
            print("ct ");
            continue;
        }
        print(ftos(i), " ");
        if (i == brkat) {
            print("brk ");
            break;
        }
    }
    print("end\n");
}
