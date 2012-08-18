/* this is the WIP test for the parser...
 * constantly adding stuff here to see if things break
 */
void(string)        print  = #1;
void(string,string) print2 = #1;
void(string,string,string) print3 = #1;
string(float)       ftos   = #2;
entity()            spawn  = #3;
void(entity)        kill   = #4;

.float vis;
.entity other;

float(entity targ) visible = {
    return targ.vis;
};

void() main = {
    local entity pawn, pawn2;

    pawn = spawn();
    pawn2 = spawn();

    pawn.other = pawn2;
    (pawn.other).vis = 0;

    if (!visible(pawn.other))
        print("Yes\n");
};
