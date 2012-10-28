void(string, ...) print = #1;
string(float)     ftos  = #2;
entity()          spawn = #3;
string(vector)    vtos  = #5;
void(string, ...) error = #6;

entity self;

.vector origin;
.vector view;

entity() make = {
    local entity e;
    e = spawn();
    e.view = '0 0 25';
    return e;
};

float(entity targ) visible = {
    local vector spot1, spot2;
    spot1 = self.origin + self.view;
    spot2 = targ.origin + targ.view;

    print("spot1 = ", vtos(spot1), "\n");
    print("spot2 = ", vtos(spot2), "\n");
    // This was part of some QC code which had a bug
    // we don't actually return anything important here.
    return 0;
};

void(vector a, vector b) main = {
    local entity targ;

    self = make();
    targ = make();
    if (self == targ)
        error("ERROR, self == targ\n");

    self.origin = a;
    targ.origin = b;

    print("vis: ", ftos(visible(targ)), "\n");
};
