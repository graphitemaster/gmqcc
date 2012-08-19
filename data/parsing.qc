/* this is the WIP test for the parser...
 * constantly adding stuff here to see if things break
 */
void(string)        print  = #1;
void(string,string) print2 = #1;
void(string,string,string) print3 = #1;
string(float)       ftos   = #2;
entity()            spawn  = #3;
void(entity)        kill   = #4;

.float mema;
.float memb;

$framevalue 0
$frame stand1 stand2 standX
$framerestore stand2
$frame stand3
$modelname foobar
$modelname foobar3

void() main = {
    entity pawn;
    vector vec;
    float a;

    vec = '3 4 5';
    vec_z = 5;

    a = 5;

    if (a) {
        print("a != 0\n");
    } else {
        print("not a\n");
    }

    a = 19;
    print("Hello, World\n");

    pawn = spawn();
    pawn.mema = 3;
    pawn.memb = 5;
    print2(ftos(pawn.mema), "\n");
    print2(ftos(pawn.memb), "\n");

    print("SECOND TEST\n");
    for (a = 0; a < 3; a = a + 1) {
        print3("LOOP ", ftos(a), "\n");
    }

    print("DO-WHILE test\n");
    a = 2;
    do {
        print("Foo\n");
        a = a - 1;
    } while (a);

    float b;
    a = 5;
    print3("a = ", ftos(a), "\n");
    b = a += 7;
    print("adding\n");
    print3("a = ", ftos(a), "\n");
    print3("b = ", ftos(b), "\n");

    print3("memb = ", ftos(pawn.memb), "\n");
    pawn.memb += -1;
    print3("memb = ", ftos(pawn.memb), "\n");
    print3("Frame stand3 is ", ftos($stand3), " wooh\n");
};
