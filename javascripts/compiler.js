// compiler bootstraps down to the emcscripten compiled compiler
// and the execution

var std     = "gmqcc";
var example = "1";
function update() {
    var select = document.getElementById("std");
    var change = select.options[select.selectedIndex].value;
    if (change != std) {
        std = change;
    }
        
    select = document.getElementById("eg");
    change = select.options[select.selectedIndex].value;
    if (change != example) {
        example = change;
    }
}

function compile() {
    var string = '"' + document.getElementById("input").value + '"';
    string += " -std="+std+" ";
    
    document.getElementById("output").value = string;
}
