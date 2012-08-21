void(string) print = #1;
void(float) ftos = #2;
void() main = {
    local float uninit, unused, setonly;
    local vector invec;
    print("foo\n");

    setonly = 3;
    invec = '1 2 3';

    if (0)
        uninit = 3;
    ftos(uninit);
    ftos(invec_x);
};

void(float par) partest = {
    ftos(par);
};
