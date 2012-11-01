void(string, string) print = #1;
entity()             spawn = #3;

.string a;
.string b;

void(entity e, .string s) callout = {
	print(e.s, "\n");
};

void() main = {
	local entity e;
	e = spawn();
	e.a = "foo";
	e.b = "bar";
	callout(e, b);
};
