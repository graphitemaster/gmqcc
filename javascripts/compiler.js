/*
 * Some example programs as strings:  Javascript is so lame I wanted to
 * just reference these as files, aparently that's not possible, w/e
 */
var examples = new Array();

/* example code */
examples[0] = 'void(string) print = #1;\nvoid(string what) main = {\n\tprint(what);\n\tprint("\\n");\n};\n';
examples[1] = '\
void(string, ...) print = #1;\n\
string(float) ftos = #2;\n\
\n\
float(float x, float y, float z) sum = {\n\
\treturn x + y + z;\n\
};\n\
\n\
void(float a, float b, float c) main = {\n\
\tlocal float f;\n\
\tf = sum(sum(a, sum(a, b, c), c),\n\
\tsum(sum(sum(a, b, c), b, sum(a, b, c)), b, sum(a, b, sum(a, b, c))),\n\
\tsum(sum(a, b, c), b, c));\n\
\tprint(ftos(f), \"\\n\");\n\
};\n';
examples[2] = '\
void(string, ...) print = #1;\n\
string(float) ftos = #2;\n\
\n\
void(float a, float b) main = {\n\
\tif (a == b) print("eq\\n");\n\
\tif (a != b) print("ne\\n");\n\
\tif (a >  b) print("gt\\n");\n\
\tif (a <  b) print("lt\\n");\n\
\tif (a >= b) print("ge\\n");\n\
\tif (a <= b) print("le\\n");\n\
};\n';
examples[3] = '\
void(string, string) print = #1;\n\
entity() spawn = #3;\n\
\n\
.string a;\n\
.string b;\n\
\n\
void(entity e, .string s) callout = {\n\
\tprint(e.s, "\\n");\n\
};\n\
\n\
void() main = {\n\
\tlocal entity e;\n\
\te = spawn();\n\
\te.a = "foo";\n\
\te.b = "bar";\n\
\tcallout(e, b);\n\
};\n';
examples[4] = '\
.float  globf;\n\
.vector globv;\n\
.string globs;\n\
.void() globfunc;\n';
examples[5] = '\
void(string, ...) print = #1;\n\
string(float) ftos = #2;\n\
entity() spawn = #3;\n\
string(vector) vtos = #5;\n\
void(string, ...) error = #6;\n\
\n\
entity self;\n\
\n\
.vector origin;\n\
.vector view;\n\
\n\
entity() make = {\n\
\tlocal entity e;\n\
\te = spawn();\n\
\te.view = \'0 0 25\';\n\
\treturn e;\n\
};\n\
\n\
float(entity targ) visible = {\n\
\tlocal vector spot1, spot2;\n\
\tspot1 = self.origin + self.view;\n\
\tspot2 = targ.origin + targ.view;\n\
\n\
\tprint("spot1 = ", vtos(spot1), "\\n");\n\
\tprint("spot2 = ", vtos(spot2), "\\n");\n\
\treturn 0;\n\
};\n;\
\n\
void(vector a, vector b) main = {\n\
\tlocal entity targ;\n\
\n\
\tself = make();\n\
\ttarg = make();\n\
\tif (self == targ)\n\
\t\terror("ERROR, self == targ\\n");\n\
\n\
\tself.origin = a;\n\
\ttarg.origin = b;\n\
\n\
\tprint("vis: ", ftos(visible(targ)), "\\n");\n\
};\n';
examples[6] = '\
void(string, string) print = #1;\n\
\n\
string() getter = {\n\
\treturn "correct";\n\
};\n\
\n\
void(string() f) printer = {\n\
\tprint(f(), "\\n");\n\
};\n\
\n\
void() main = {\n\
\tprinter(getter);\n\
};\n';
examples[7] = '\
.float  globf;\n\
.vector globv;\n\
.string globs;\n\
.void() globfunc;\n';
examples[8] = '\
void(string, ...) print = #1;\n\
\n\
void(float c) main = {\n\
\tif (c == 1)\n\
\t\tprint("One\\n");\n\
\telse if (c == 2)\n\
\t\tprint("Two\\n");\n\
\telse if (c == 3)\n\
\t\tprint("Three\\n");\n\
\telse\n\
\t\tprint("Else\\n");\n\
};\n';
examples[9] = '\
void(string, ...) print = #1;\n\
string(float) ftos = #2;\n\
\n\
void(float n) main = {\n\
\tlocal float i;\n\
\n\
\tfor (i = 0; i < n; i += 1) {\n\
\t\tprint("for ", ftos(i), "\\n");\n\
\t}\n\
\n\
\ti = 0;\n\
\twhile (i < n) {\n\
\t\tprint("while ", ftos(i), "\\n");\n\
\t\ti += 1;\n\
\t}\n\
\n\
\ti = 0;\n\
\tdo {\n\
\t\tprint("do ", ftos(i), "\\n");\n\
\t\ti += 1;\n\
\t} while (i < n);\n\
};\n';
examples[10] ='\
void(string, ...) print = #1;\n\
string(float) ftos = #2;\n\
string(vector) vtos = #5;\n\
\n\
void(float a, float b) main = {\n\
\tprint("input: ", ftos(a), " and ", ftos(b), "\\n");\n\
\tprint("+ ", ftos(a+b), "\\n");\n\
\tprint("* ", ftos(a*b), "\\n");\n\
\tprint("/ ", ftos(a/b), "\\n");\n\
\tprint("& ", ftos(a&b), "\\n");\n\
\tprint("| ", ftos(a|b), "\\n");\n\
\tprint("&& ", ftos(a&&b), "\\n");\n\
\tprint("|| ", ftos(a||b), "\\n");\n\
};\n';
examples[11] = '\
void(string, string) print = %:1;\n\
\n\
void() main = ??<\n\
\tprint("??=??\'??(??)??!??<??>??-??/??/%>", "??/n");\n\
\tprint("#^[]|{}~\\%>", "\\n");\n\
%>;\n';
examples[12] = '\
void(string, ...) print = #1;\n\
\n\
void(string what) main = {\n\
\tprint(what, "\\n");\n\
};\n';

/* ad-hoc eh? */
function update() {
    var sel = document.getElementById("eg");
    var doc = document.getElementById("input");
    
    doc.value = examples[sel[sel.selectedIndex].value - 1];
}

function compile() {
    //run ([
    //    document.getElementById("eg").selectedIndex.toString(),
    //    document.getElementById("args")
    //]);
    //document.write("Hello World\n");
    run (["hello.dat"]);
}

/* set initial */
document.getElementById("eg").selectedIndex = 0;
update();
