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
.vector memv;

.void() fun;

void(entity a, .float f) printfield = {
    ftos(3+4);
    print3("The field is ", ftos(a.f), "\n");
};

void() funny = {
    print("FUNNY\n");
};

void() main = {
    entity pawn;

    pawn = spawn();

    pawn.mema = 9;
    pawn.memv = '1 2 3';
    pawn.memb = 10;

    print3("x = ", ftos(pawn.memv_x), "\n");
    print3("y = ", ftos(pawn.memv_y), "\n");
    print3("z = ", ftos(pawn.memv_z), "\n");
    print3("a = ", ftos(pawn.mema), "\n");
    print3("b = ", ftos(pawn.memb), "\n");
    pawn.memv_y += 3;
    print3("x = ", ftos(pawn.memv_x), "\n");
    print3("y = ", ftos(pawn.memv_y), "\n");
    print3("z = ", ftos(pawn.memv_z), "\n");
    printfield(pawn, memv_z);

    pawn.fun = funny;

    (pawn.fun)();
};
