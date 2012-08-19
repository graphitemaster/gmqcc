/* this is the WIP test for the parser...
 * constantly adding stuff here to see if things break
 */
void(string)        print  = #1;
void(string,string) print2 = #1;
void(string,string,string) print3 = #1;
string(float)       ftos   = #2;
entity()            spawn  = #3;
void(entity)        kill   = #4;

$frame stand1 stand2 standX
/*
.float  frame;
.float  nextthink;
.void() think;

entity self;
float  time;

void() stand2;

void() stand1 = [ 0, stand2 ] {
    // expands to:
    //self.frame = 0;
    //self.nextthink = time + 0.1;
    //self.think = stand2
    print("In stand 1...\n");
    print3("--> self.frame should be 0, is ", ftos(self.frame), "\n");
};

void() stand2 = [ 1, stand1 ] {
    print("In stand 2...\n");
    print3("--> self.frame should be 1, is ", ftos(self.frame), "\n");
};

void() standm = {
    print3("Foo ", ftos(self), "\n");
    self.frame = 0;
    self.nextthink = time + 0.1;
    self.think = stand2;
};

void() main = {
    self = spawn();

    time = 10;

    print("Setting think\n");
    self.think = stand1;

    print("Running think\n");
    standm();
    print("Running from 'self'\n");
    self.think();
    self.think();
    self.think();
};
*/

entity self;

.float frame;
.float nextthink;

void(float a, float b) foo = {};

void() main = {
    self = spawn();
    self.frame = 1;
    self.nextthink = 2;
    print2(ftos(self.frame), "\n");
    print2(ftos(self.nextthink), "\n");
    foo(self.frame, self.nextthink);
    kill(self);
};

