void print(...) = #1;
string ftos(float) = #2;

void main(float param, float p2) {
    float i;
    float c80 = 80;
    switch(param) {
        case 3: print("Three, falling through to 2 - ");
        case 2: print("Two\n"); break;
        case 1: print("One\n"); break;
        case 4: print("Four, falling through to default - ");
        default: print("Other\n"); break;

        case c80:
            print("Enhanced 80\n");
            break;

        case 99:
            if (p2 > 5) {
                print("early break\n");
                break;
            }
            for (i = 0; i < 5; i += 1)
                print(ftos(i), " ");
            print(ftos(i), "\n");
    }
}
