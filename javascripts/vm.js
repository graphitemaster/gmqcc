// Note: Some Emscripten settings will significantly limit the speed of the generated code.
// Note: Some Emscripten settings may limit the speed of the generated code.
// TODO: " u s e   s t r i c t ";

try {
  this['Module'] = Module;
} catch(e) {
  this['Module'] = Module = {};
}

// The environment setup code below is customized to use Module.
// *** Environment setup code ***
var ENVIRONMENT_IS_NODE = typeof process === 'object';
var ENVIRONMENT_IS_WEB = typeof window === 'object';
var ENVIRONMENT_IS_WORKER = typeof importScripts === 'function';
var ENVIRONMENT_IS_SHELL = !ENVIRONMENT_IS_WEB && !ENVIRONMENT_IS_NODE && !ENVIRONMENT_IS_WORKER;

if (ENVIRONMENT_IS_NODE) {
  // Expose functionality in the same simple way that the shells work
  // Note that we pollute the global namespace here, otherwise we break in node
  Module['print'] = function(x) {
    process['stdout'].write(x + '\n');
  };
  Module['printErr'] = function(x) {
    process['stderr'].write(x + '\n');
  };

  var nodeFS = require('fs');
  var nodePath = require('path');

  Module['read'] = function(filename) {
    filename = nodePath['normalize'](filename);
    var ret = nodeFS['readFileSync'](filename).toString();
    // The path is absolute if the normalized version is the same as the resolved.
    if (!ret && filename != nodePath['resolve'](filename)) {
      filename = path.join(__dirname, '..', 'src', filename);
      ret = nodeFS['readFileSync'](filename).toString();
    }
    return ret;
  };

  Module['load'] = function(f) {
    globalEval(read(f));
  };

  if (!Module['arguments']) {
    Module['arguments'] = process['argv'].slice(2);
  }
}

if (ENVIRONMENT_IS_SHELL) {
  Module['print'] = print;
  if (typeof printErr != 'undefined') Module['printErr'] = printErr; // not present in v8 or older sm

  // Polyfill over SpiderMonkey/V8 differences
  if (typeof read != 'undefined') {
    Module['read'] = read;
  } else {
    Module['read'] = function(f) { snarf(f) };
  }

  if (!Module['arguments']) {
    if (typeof scriptArgs != 'undefined') {
      Module['arguments'] = scriptArgs;
    } else if (typeof arguments != 'undefined') {
      Module['arguments'] = arguments;
    }
  }
}

if (ENVIRONMENT_IS_WEB && !ENVIRONMENT_IS_WORKER) {
  if (!Module['print']) {
    Module['print'] = function(x) {
      console.log(x);
    };
  }

  if (!Module['printErr']) {
    Module['printErr'] = function(x) {
      console.log(x);
    };
  }
}

if (ENVIRONMENT_IS_WEB || ENVIRONMENT_IS_WORKER) {
  Module['read'] = function(url) {
    var xhr = new XMLHttpRequest();
    xhr.open('GET', url, false);
    xhr.send(null);
    return xhr.responseText;
  };

  if (!Module['arguments']) {
    if (typeof arguments != 'undefined') {
      Module['arguments'] = arguments;
    }
  }
}

if (ENVIRONMENT_IS_WORKER) {
  // We can do very little here...
  var TRY_USE_DUMP = false;
  if (!Module['print']) {
    Module['print'] = (TRY_USE_DUMP && (typeof(dump) !== "undefined") ? (function(x) {
      dump(x);
    }) : (function(x) {
      // self.postMessage(x); // enable this if you want stdout to be sent as messages
    }));
  }

  Module['load'] = importScripts;
}

if (!ENVIRONMENT_IS_WORKER && !ENVIRONMENT_IS_WEB && !ENVIRONMENT_IS_NODE && !ENVIRONMENT_IS_SHELL) {
  // Unreachable because SHELL is dependant on the others
  throw 'Unknown runtime environment. Where are we?';
}

function globalEval(x) {
  eval.call(null, x);
}
if (!Module['load'] == 'undefined' && Module['read']) {
  Module['load'] = function(f) {
    globalEval(Module['read'](f));
  };
}
if (!Module['print']) {
  Module['print'] = function(){};
}
if (!Module['printErr']) {
  Module['printErr'] = Module['print'];
}
if (!Module['arguments']) {
  Module['arguments'] = [];
}
// *** Environment setup code ***

// Closure helpers
Module.print = Module['print'];
Module.printErr = Module['printErr'];

// Callbacks
if (!Module['preRun']) Module['preRun'] = [];
if (!Module['postRun']) Module['postRun'] = [];

  
// === Auto-generated preamble library stuff ===

//========================================
// Runtime code shared with compiler
//========================================

var Runtime = {
  stackSave: function () {
    return STACKTOP;
  },
  stackRestore: function (stackTop) {
    STACKTOP = stackTop;
  },
  forceAlign: function (target, quantum) {
    quantum = quantum || 4;
    if (quantum == 1) return target;
    if (isNumber(target) && isNumber(quantum)) {
      return Math.ceil(target/quantum)*quantum;
    } else if (isNumber(quantum) && isPowerOfTwo(quantum)) {
      var logg = log2(quantum);
      return '((((' +target + ')+' + (quantum-1) + ')>>' + logg + ')<<' + logg + ')';
    }
    return 'Math.ceil((' + target + ')/' + quantum + ')*' + quantum;
  },
  isNumberType: function (type) {
    return type in Runtime.INT_TYPES || type in Runtime.FLOAT_TYPES;
  },
  isPointerType: function isPointerType(type) {
  return type[type.length-1] == '*';
},
  isStructType: function isStructType(type) {
  if (isPointerType(type)) return false;
  if (/^\[\d+\ x\ (.*)\]/.test(type)) return true; // [15 x ?] blocks. Like structs
  if (/<?{ ?[^}]* ?}>?/.test(type)) return true; // { i32, i8 } etc. - anonymous struct types
  // See comment in isStructPointerType()
  return type[0] == '%';
},
  INT_TYPES: {"i1":0,"i8":0,"i16":0,"i32":0,"i64":0},
  FLOAT_TYPES: {"float":0,"double":0},
  bitshift64: function (low, high, op, bits) {
    var ander = Math.pow(2, bits)-1;
    if (bits < 32) {
      switch (op) {
        case 'shl':
          return [low << bits, (high << bits) | ((low&(ander << (32 - bits))) >>> (32 - bits))];
        case 'ashr':
          return [(((low >>> bits ) | ((high&ander) << (32 - bits))) >> 0) >>> 0, (high >> bits) >>> 0];
        case 'lshr':
          return [((low >>> bits) | ((high&ander) << (32 - bits))) >>> 0, high >>> bits];
      }
    } else if (bits == 32) {
      switch (op) {
        case 'shl':
          return [0, low];
        case 'ashr':
          return [high, (high|0) < 0 ? ander : 0];
        case 'lshr':
          return [high, 0];
      }
    } else { // bits > 32
      switch (op) {
        case 'shl':
          return [0, low << (bits - 32)];
        case 'ashr':
          return [(high >> (bits - 32)) >>> 0, (high|0) < 0 ? ander : 0];
        case 'lshr':
          return [high >>>  (bits - 32) , 0];
      }
    }
    abort('unknown bitshift64 op: ' + [value, op, bits]);
  },
  or64: function (x, y) {
    var l = (x | 0) | (y | 0);
    var h = (Math.round(x / 4294967296) | Math.round(y / 4294967296)) * 4294967296;
    return l + h;
  },
  and64: function (x, y) {
    var l = (x | 0) & (y | 0);
    var h = (Math.round(x / 4294967296) & Math.round(y / 4294967296)) * 4294967296;
    return l + h;
  },
  xor64: function (x, y) {
    var l = (x | 0) ^ (y | 0);
    var h = (Math.round(x / 4294967296) ^ Math.round(y / 4294967296)) * 4294967296;
    return l + h;
  },
  getNativeTypeSize: function (type, quantumSize) {
    if (Runtime.QUANTUM_SIZE == 1) return 1;
    var size = {
      '%i1': 1,
      '%i8': 1,
      '%i16': 2,
      '%i32': 4,
      '%i64': 8,
      "%float": 4,
      "%double": 8
    }['%'+type]; // add '%' since float and double confuse Closure compiler as keys, and also spidermonkey as a compiler will remove 's from '_i8' etc
    if (!size) {
      if (type.charAt(type.length-1) == '*') {
        size = Runtime.QUANTUM_SIZE; // A pointer
      } else if (type[0] == 'i') {
        var bits = parseInt(type.substr(1));
        assert(bits % 8 == 0);
        size = bits/8;
      }
    }
    return size;
  },
  getNativeFieldSize: function (type) {
    return Math.max(Runtime.getNativeTypeSize(type), Runtime.QUANTUM_SIZE);
  },
  dedup: function dedup(items, ident) {
  var seen = {};
  if (ident) {
    return items.filter(function(item) {
      if (seen[item[ident]]) return false;
      seen[item[ident]] = true;
      return true;
    });
  } else {
    return items.filter(function(item) {
      if (seen[item]) return false;
      seen[item] = true;
      return true;
    });
  }
},
  set: function set() {
  var args = typeof arguments[0] === 'object' ? arguments[0] : arguments;
  var ret = {};
  for (var i = 0; i < args.length; i++) {
    ret[args[i]] = 0;
  }
  return ret;
},
  calculateStructAlignment: function calculateStructAlignment(type) {
    type.flatSize = 0;
    type.alignSize = 0;
    var diffs = [];
    var prev = -1;
    type.flatIndexes = type.fields.map(function(field) {
      var size, alignSize;
      if (Runtime.isNumberType(field) || Runtime.isPointerType(field)) {
        size = Runtime.getNativeTypeSize(field); // pack char; char; in structs, also char[X]s.
        alignSize = size;
      } else if (Runtime.isStructType(field)) {
        size = Types.types[field].flatSize;
        alignSize = Types.types[field].alignSize;
      } else {
        throw 'Unclear type in struct: ' + field + ', in ' + type.name_ + ' :: ' + dump(Types.types[type.name_]);
      }
      alignSize = type.packed ? 1 : Math.min(alignSize, Runtime.QUANTUM_SIZE);
      type.alignSize = Math.max(type.alignSize, alignSize);
      var curr = Runtime.alignMemory(type.flatSize, alignSize); // if necessary, place this on aligned memory
      type.flatSize = curr + size;
      if (prev >= 0) {
        diffs.push(curr-prev);
      }
      prev = curr;
      return curr;
    });
    type.flatSize = Runtime.alignMemory(type.flatSize, type.alignSize);
    if (diffs.length == 0) {
      type.flatFactor = type.flatSize;
    } else if (Runtime.dedup(diffs).length == 1) {
      type.flatFactor = diffs[0];
    }
    type.needsFlattening = (type.flatFactor != 1);
    return type.flatIndexes;
  },
  generateStructInfo: function (struct, typeName, offset) {
    var type, alignment;
    if (typeName) {
      offset = offset || 0;
      type = (typeof Types === 'undefined' ? Runtime.typeInfo : Types.types)[typeName];
      if (!type) return null;
      if (type.fields.length != struct.length) {
        printErr('Number of named fields must match the type for ' + typeName + ': possibly duplicate struct names. Cannot return structInfo');
        return null;
      }
      alignment = type.flatIndexes;
    } else {
      var type = { fields: struct.map(function(item) { return item[0] }) };
      alignment = Runtime.calculateStructAlignment(type);
    }
    var ret = {
      __size__: type.flatSize
    };
    if (typeName) {
      struct.forEach(function(item, i) {
        if (typeof item === 'string') {
          ret[item] = alignment[i] + offset;
        } else {
          // embedded struct
          var key;
          for (var k in item) key = k;
          ret[key] = Runtime.generateStructInfo(item[key], type.fields[i], alignment[i]);
        }
      });
    } else {
      struct.forEach(function(item, i) {
        ret[item[1]] = alignment[i];
      });
    }
    return ret;
  },
  addFunction: function (func) {
    var ret = FUNCTION_TABLE.length;
    FUNCTION_TABLE.push(func);
    FUNCTION_TABLE.push(0);
    return ret;
  },
  warnOnce: function (text) {
    if (!Runtime.warnOnce.shown) Runtime.warnOnce.shown = {};
    if (!Runtime.warnOnce.shown[text]) {
      Runtime.warnOnce.shown[text] = 1;
      Module.printErr(text);
    }
  },
  funcWrappers: {},
  getFuncWrapper: function (func) {
    if (!Runtime.funcWrappers[func]) {
      Runtime.funcWrappers[func] = function() {
        FUNCTION_TABLE[func].apply(null, arguments);
      };
    }
    return Runtime.funcWrappers[func];
  },
  UTF8Processor: function () {
    var buffer = [];
    var needed = 0;
    this.processCChar = function (code) {
      code = code & 0xff;
      if (needed) {
        buffer.push(code);
        needed--;
      }
      if (buffer.length == 0) {
        if (code < 128) return String.fromCharCode(code);
        buffer.push(code);
        if (code > 191 && code < 224) {
          needed = 1;
        } else {
          needed = 2;
        }
        return '';
      }
      if (needed > 0) return '';
      var c1 = buffer[0];
      var c2 = buffer[1];
      var c3 = buffer[2];
      var ret;
      if (c1 > 191 && c1 < 224) {
        ret = String.fromCharCode(((c1 & 31) << 6) | (c2 & 63));
      } else {
        ret = String.fromCharCode(((c1 & 15) << 12) | ((c2 & 63) << 6) | (c3 & 63));
      }
      buffer.length = 0;
      return ret;
    }
    this.processJSString = function(string) {
      string = unescape(encodeURIComponent(string));
      var ret = [];
      for (var i = 0; i < string.length; i++) {
        ret.push(string.charCodeAt(i));
      }
      return ret;
    }
  },
  stackAlloc: function stackAlloc(size) { var ret = STACKTOP;STACKTOP += size;STACKTOP = ((((STACKTOP)+3)>>2)<<2);assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"); return ret; },
  staticAlloc: function staticAlloc(size) { var ret = STATICTOP;STATICTOP += size;STATICTOP = ((((STATICTOP)+3)>>2)<<2); if (STATICTOP >= TOTAL_MEMORY) enlargeMemory();; return ret; },
  alignMemory: function alignMemory(size,quantum) { var ret = size = Math.ceil((size)/(quantum ? quantum : 4))*(quantum ? quantum : 4); return ret; },
  makeBigInt: function makeBigInt(low,high,unsigned) { var ret = (unsigned ? (((low)>>>0)+(((high)>>>0)*4294967296)) : (((low)>>>0)+(((high)|0)*4294967296))); return ret; },
  QUANTUM_SIZE: 4,
  __dummy__: 0
}



var CorrectionsMonitor = {
  MAX_ALLOWED: 0, // XXX
  corrections: 0,
  sigs: {},

  note: function(type, succeed, sig) {
    if (!succeed) {
      this.corrections++;
      if (this.corrections >= this.MAX_ALLOWED) abort('\n\nToo many corrections!');
    }
  },

  print: function() {
  }
};





//========================================
// Runtime essentials
//========================================

var __THREW__ = false; // Used in checking for thrown exceptions.

var ABORT = false;

var undef = 0;
// tempInt is used for 32-bit signed values or smaller. tempBigInt is used
// for 32-bit unsigned values or more than 32 bits. TODO: audit all uses of tempInt
var tempValue, tempInt, tempBigInt, tempInt2, tempBigInt2, tempPair, tempBigIntI, tempBigIntR, tempBigIntS, tempBigIntP, tempBigIntD;
var tempI64, tempI64b;

function abort(text) {
  Module.print(text + ':\n' + (new Error).stack);
  ABORT = true;
  throw "Assertion: " + text;
}

function assert(condition, text) {
  if (!condition) {
    abort('Assertion failed: ' + text);
  }
}

var globalScope = this;

// C calling interface. A convenient way to call C functions (in C files, or
// defined with extern "C").
//
// Note: LLVM optimizations can inline and remove functions, after which you will not be
//       able to call them. Adding
//
//         __attribute__((used))
//
//       to the function definition will prevent that.
//
// Note: Closure optimizations will minify function names, making
//       functions no longer callable. If you run closure (on by default
//       in -O2 and above), you should export the functions you will call
//       by calling emcc with something like
//
//         -s EXPORTED_FUNCTIONS='["_func1","_func2"]'
//
// @param ident      The name of the C function (note that C++ functions will be name-mangled - use extern "C")
// @param returnType The return type of the function, one of the JS types 'number', 'string' or 'array' (use 'number' for any C pointer, and
//                   'array' for JavaScript arrays and typed arrays).
// @param argTypes   An array of the types of arguments for the function (if there are no arguments, this can be ommitted). Types are as in returnType,
//                   except that 'array' is not possible (there is no way for us to know the length of the array)
// @param args       An array of the arguments to the function, as native JS values (as in returnType)
//                   Note that string arguments will be stored on the stack (the JS string will become a C string on the stack).
// @return           The return value, as a native JS value (as in returnType)
function ccall(ident, returnType, argTypes, args) {
  var stack = 0;
  function toC(value, type) {
    if (type == 'string') {
      if (value === null || value === undefined || value === 0) return 0; // null string
      if (!stack) stack = Runtime.stackSave();
      var ret = Runtime.stackAlloc(value.length+1);
      writeStringToMemory(value, ret);
      return ret;
    } else if (type == 'array') {
      if (!stack) stack = Runtime.stackSave();
      var ret = Runtime.stackAlloc(value.length);
      writeArrayToMemory(value, ret);
      return ret;
    }
    return value;
  }
  function fromC(value, type) {
    if (type == 'string') {
      return Pointer_stringify(value);
    }
    assert(type != 'array');
    return value;
  }
  try {
    var func = eval('_' + ident);
  } catch(e) {
    try {
      func = globalScope['Module']['_' + ident]; // closure exported function
    } catch(e) {}
  }
  assert(func, 'Cannot call unknown function ' + ident + ' (perhaps LLVM optimizations or closure removed it?)');
  var i = 0;
  var cArgs = args ? args.map(function(arg) {
    return toC(arg, argTypes[i++]);
  }) : [];
  var ret = fromC(func.apply(null, cArgs), returnType);
  if (stack) Runtime.stackRestore(stack);
  return ret;
}
Module["ccall"] = ccall;

// Returns a native JS wrapper for a C function. This is similar to ccall, but
// returns a function you can call repeatedly in a normal way. For example:
//
//   var my_function = cwrap('my_c_function', 'number', ['number', 'number']);
//   alert(my_function(5, 22));
//   alert(my_function(99, 12));
//
function cwrap(ident, returnType, argTypes) {
  // TODO: optimize this, eval the whole function once instead of going through ccall each time
  return function() {
    return ccall(ident, returnType, argTypes, Array.prototype.slice.call(arguments));
  }
}
Module["cwrap"] = cwrap;

// Sets a value in memory in a dynamic way at run-time. Uses the
// type data. This is the same as makeSetValue, except that
// makeSetValue is done at compile-time and generates the needed
// code then, whereas this function picks the right code at
// run-time.
// Note that setValue and getValue only do *aligned* writes and reads!
// Note that ccall uses JS types as for defining types, while setValue and
// getValue need LLVM types ('i8', 'i32') - this is a lower-level operation
function setValue(ptr, value, type, noSafe) {
  type = type || 'i8';
  if (type.charAt(type.length-1) === '*') type = 'i32'; // pointers are 32-bit
    switch(type) {
      case 'i1': HEAP8[(ptr)]=value; break;
      case 'i8': HEAP8[(ptr)]=value; break;
      case 'i16': HEAP16[((ptr)>>1)]=value; break;
      case 'i32': HEAP32[((ptr)>>2)]=value; break;
      case 'i64': (tempI64 = [value>>>0,Math.min(Math.floor((value)/4294967296), 4294967295)],HEAP32[((ptr)>>2)]=tempI64[0],HEAP32[(((ptr)+(4))>>2)]=tempI64[1]); break;
      case 'float': HEAPF32[((ptr)>>2)]=value; break;
      case 'double': (tempDoubleF64[0]=value,HEAP32[((ptr)>>2)]=tempDoubleI32[0],HEAP32[(((ptr)+(4))>>2)]=tempDoubleI32[1]); break;
      default: abort('invalid type for setValue: ' + type);
    }
}
Module['setValue'] = setValue;

// Parallel to setValue.
function getValue(ptr, type, noSafe) {
  type = type || 'i8';
  if (type.charAt(type.length-1) === '*') type = 'i32'; // pointers are 32-bit
    switch(type) {
      case 'i1': return HEAP8[(ptr)];
      case 'i8': return HEAP8[(ptr)];
      case 'i16': return HEAP16[((ptr)>>1)];
      case 'i32': return HEAP32[((ptr)>>2)];
      case 'i64': return HEAP32[((ptr)>>2)];
      case 'float': return HEAPF32[((ptr)>>2)];
      case 'double': return (tempDoubleI32[0]=HEAP32[((ptr)>>2)],tempDoubleI32[1]=HEAP32[(((ptr)+(4))>>2)],tempDoubleF64[0]);
      default: abort('invalid type for setValue: ' + type);
    }
  return null;
}
Module['getValue'] = getValue;

var ALLOC_NORMAL = 0; // Tries to use _malloc()
var ALLOC_STACK = 1; // Lives for the duration of the current function call
var ALLOC_STATIC = 2; // Cannot be freed
Module['ALLOC_NORMAL'] = ALLOC_NORMAL;
Module['ALLOC_STACK'] = ALLOC_STACK;
Module['ALLOC_STATIC'] = ALLOC_STATIC;

// allocate(): This is for internal use. You can use it yourself as well, but the interface
//             is a little tricky (see docs right below). The reason is that it is optimized
//             for multiple syntaxes to save space in generated code. So you should
//             normally not use allocate(), and instead allocate memory using _malloc(),
//             initialize it with setValue(), and so forth.
// @slab: An array of data, or a number. If a number, then the size of the block to allocate,
//        in *bytes* (note that this is sometimes confusing: the next parameter does not
//        affect this!)
// @types: Either an array of types, one for each byte (or 0 if no type at that position),
//         or a single type which is used for the entire block. This only matters if there
//         is initial data - if @slab is a number, then this does not matter at all and is
//         ignored.
// @allocator: How to allocate memory, see ALLOC_*
function allocate(slab, types, allocator) {
  var zeroinit, size;
  if (typeof slab === 'number') {
    zeroinit = true;
    size = slab;
  } else {
    zeroinit = false;
    size = slab.length;
  }

  var singleType = typeof types === 'string' ? types : null;

  var ret = [_malloc, Runtime.stackAlloc, Runtime.staticAlloc][allocator === undefined ? ALLOC_STATIC : allocator](Math.max(size, singleType ? 1 : types.length));

  if (zeroinit) {
      _memset(ret, 0, size);
      return ret;
  }
  
  var i = 0, type;
  while (i < size) {
    var curr = slab[i];

    if (typeof curr === 'function') {
      curr = Runtime.getFunctionIndex(curr);
    }

    type = singleType || types[i];
    if (type === 0) {
      i++;
      continue;
    }
    assert(type, 'Must know what type to store in allocate!');

    if (type == 'i64') type = 'i32'; // special case: we have one i32 here, and one i32 later

    setValue(ret+i, curr, type);
    i += Runtime.getNativeTypeSize(type);
  }

  return ret;
}
Module['allocate'] = allocate;

function Pointer_stringify(ptr, /* optional */ length) {
  var utf8 = new Runtime.UTF8Processor();
  var nullTerminated = typeof(length) == "undefined";
  var ret = "";
  var i = 0;
  var t;
  while (1) {
    t = HEAPU8[((ptr)+(i))];
    if (nullTerminated && t == 0) break;
    ret += utf8.processCChar(t);
    i += 1;
    if (!nullTerminated && i == length) break;
  }
  return ret;
}
Module['Pointer_stringify'] = Pointer_stringify;

function Array_stringify(array) {
  var ret = "";
  for (var i = 0; i < array.length; i++) {
    ret += String.fromCharCode(array[i]);
  }
  return ret;
}
Module['Array_stringify'] = Array_stringify;

// Memory management

var FUNCTION_TABLE; // XXX: In theory the indexes here can be equal to pointers to stacked or malloced memory. Such comparisons should
                    //      be false, but can turn out true. We should probably set the top bit to prevent such issues.

var PAGE_SIZE = 4096;
function alignMemoryPage(x) {
  return ((x+4095)>>12)<<12;
}

var HEAP;
var HEAP8, HEAPU8, HEAP16, HEAPU16, HEAP32, HEAPU32, HEAPF32, HEAPF64;

var STACK_ROOT, STACKTOP, STACK_MAX;
var STATICTOP;
function enlargeMemory() {
  abort('Cannot enlarge memory arrays. Adjust TOTAL_MEMORY (currently ' + TOTAL_MEMORY + ') or compile with ALLOW_MEMORY_GROWTH');
}

var TOTAL_STACK = Module['TOTAL_STACK'] || 5242880;
var TOTAL_MEMORY = Module['TOTAL_MEMORY'] || 10485760;
var FAST_MEMORY = Module['FAST_MEMORY'] || 2097152;

// Initialize the runtime's memory
// check for full engine support (use string 'subarray' to avoid closure compiler confusion)
  assert(!!Int32Array && !!Float64Array && !!(new Int32Array(1)['subarray']) && !!(new Int32Array(1)['set']),
         'Cannot fallback to non-typed array case: Code is too specialized');

  var buffer = new ArrayBuffer(TOTAL_MEMORY);
  HEAP8 = new Int8Array(buffer);
  HEAP16 = new Int16Array(buffer);
  HEAP32 = new Int32Array(buffer);
  HEAPU8 = new Uint8Array(buffer);
  HEAPU16 = new Uint16Array(buffer);
  HEAPU32 = new Uint32Array(buffer);
  HEAPF32 = new Float32Array(buffer);
  HEAPF64 = new Float64Array(buffer);

  // Endianness check (note: assumes compiler arch was little-endian)
  HEAP32[0] = 255;
  assert(HEAPU8[0] === 255 && HEAPU8[3] === 0, 'Typed arrays 2 must be run on a little-endian system');

Module['HEAP'] = HEAP;
Module['HEAP8'] = HEAP8;
Module['HEAP16'] = HEAP16;
Module['HEAP32'] = HEAP32;
Module['HEAPU8'] = HEAPU8;
Module['HEAPU16'] = HEAPU16;
Module['HEAPU32'] = HEAPU32;
Module['HEAPF32'] = HEAPF32;
Module['HEAPF64'] = HEAPF64;

STACK_ROOT = STACKTOP = Runtime.alignMemory(1);
STACK_MAX = STACK_ROOT + TOTAL_STACK;

var tempDoublePtr = Runtime.alignMemory(STACK_MAX, 8);
var tempDoubleI8  = HEAP8.subarray(tempDoublePtr);
var tempDoubleI32 = HEAP32.subarray(tempDoublePtr >> 2);
var tempDoubleF32 = HEAPF32.subarray(tempDoublePtr >> 2);
var tempDoubleF64 = HEAPF64.subarray(tempDoublePtr >> 3);
function copyTempFloat(ptr) { // functions, because inlining this code is increases code size too much
  tempDoubleI8[0] = HEAP8[ptr];
  tempDoubleI8[1] = HEAP8[ptr+1];
  tempDoubleI8[2] = HEAP8[ptr+2];
  tempDoubleI8[3] = HEAP8[ptr+3];
}
function copyTempDouble(ptr) {
  tempDoubleI8[0] = HEAP8[ptr];
  tempDoubleI8[1] = HEAP8[ptr+1];
  tempDoubleI8[2] = HEAP8[ptr+2];
  tempDoubleI8[3] = HEAP8[ptr+3];
  tempDoubleI8[4] = HEAP8[ptr+4];
  tempDoubleI8[5] = HEAP8[ptr+5];
  tempDoubleI8[6] = HEAP8[ptr+6];
  tempDoubleI8[7] = HEAP8[ptr+7];
}
STACK_MAX = tempDoublePtr + 8;

STATICTOP = alignMemoryPage(STACK_MAX);

assert(STATICTOP < TOTAL_MEMORY); // Stack must fit in TOTAL_MEMORY; allocations from here on may enlarge TOTAL_MEMORY

var nullString = allocate(intArrayFromString('(null)'), 'i8', ALLOC_STATIC);

function callRuntimeCallbacks(callbacks) {
  while(callbacks.length > 0) {
    var callback = callbacks.shift();
    var func = callback.func;
    if (typeof func === 'number') {
      func = FUNCTION_TABLE[func];
    }
    func(callback.arg === undefined ? null : callback.arg);
  }
}

var __ATINIT__ = []; // functions called during startup
var __ATMAIN__ = []; // functions called when main() is to be run
var __ATEXIT__ = []; // functions called during shutdown

function initRuntime() {
  callRuntimeCallbacks(__ATINIT__);
}
function preMain() {
  callRuntimeCallbacks(__ATMAIN__);
}
function exitRuntime() {
  callRuntimeCallbacks(__ATEXIT__);

  // Print summary of correction activity
  CorrectionsMonitor.print();
}

function String_len(ptr) {
  var i = ptr;
  while (HEAP8[(i++)]) {}; // Note: should be |!= 0|, technically. But this helps catch bugs with undefineds
  return i - ptr - 1;
}
Module['String_len'] = String_len;

// Tools

// This processes a JS string into a C-line array of numbers, 0-terminated.
// For LLVM-originating strings, see parser.js:parseLLVMString function
function intArrayFromString(stringy, dontAddNull, length /* optional */) {
  var ret = (new Runtime.UTF8Processor()).processJSString(stringy);
  if (length) {
    ret.length = length;
  }
  if (!dontAddNull) {
    ret.push(0);
  }
  return ret;
}
Module['intArrayFromString'] = intArrayFromString;

function intArrayToString(array) {
  var ret = [];
  for (var i = 0; i < array.length; i++) {
    var chr = array[i];
    if (chr > 0xFF) {
        assert(false, 'Character code ' + chr + ' (' + String.fromCharCode(chr) + ')  at offset ' + i + ' not in 0x00-0xFF.');
      chr &= 0xFF;
    }
    ret.push(String.fromCharCode(chr));
  }
  return ret.join('');
}
Module['intArrayToString'] = intArrayToString;

// Write a Javascript array to somewhere in the heap
function writeStringToMemory(string, buffer, dontAddNull) {
  var array = intArrayFromString(string, dontAddNull);
  var i = 0;
  while (i < array.length) {
    var chr = array[i];
    HEAP8[((buffer)+(i))]=chr
    i = i + 1;
  }
}
Module['writeStringToMemory'] = writeStringToMemory;

function writeArrayToMemory(array, buffer) {
  for (var i = 0; i < array.length; i++) {
    HEAP8[((buffer)+(i))]=array[i];
  }
}
Module['writeArrayToMemory'] = writeArrayToMemory;

var STRING_TABLE = [];

function unSign(value, bits, ignore, sig) {
  if (value >= 0) {
    return value;
  }
  return bits <= 32 ? 2*Math.abs(1 << (bits-1)) + value // Need some trickery, since if bits == 32, we are right at the limit of the bits JS uses in bitshifts
                    : Math.pow(2, bits)         + value;
  // TODO: clean up previous line
}
function reSign(value, bits, ignore, sig) {
  if (value <= 0) {
    return value;
  }
  var half = bits <= 32 ? Math.abs(1 << (bits-1)) // abs is needed if bits == 32
                        : Math.pow(2, bits-1);
  if (value >= half && (bits <= 32 || value > half)) { // for huge values, we can hit the precision limit and always get true here. so don't do that
                                                       // but, in general there is no perfect solution here. With 64-bit ints, we get rounding and errors
                                                       // TODO: In i64 mode 1, resign the two parts separately and safely
    value = -2*half + value; // Cannot bitshift half, as it may be at the limit of the bits JS uses in bitshifts
  }
  return value;
}

// A counter of dependencies for calling run(). If we need to
// do asynchronous work before running, increment this and
// decrement it. Incrementing must happen in a place like
// PRE_RUN_ADDITIONS (used by emcc to add file preloading).
// Note that you can add dependencies in preRun, even though
// it happens right before run - run will be postponed until
// the dependencies are met.
var runDependencies = 0;
var runDependencyTracking = {};
var calledRun = false;
var runDependencyWatcher = null;
function addRunDependency(id) {
  runDependencies++;
  if (Module['monitorRunDependencies']) {
    Module['monitorRunDependencies'](runDependencies);
  }
  if (id) {
    assert(!runDependencyTracking[id]);
    runDependencyTracking[id] = 1;
    if (runDependencyWatcher === null && typeof setInterval !== 'undefined') {
      // Check for missing dependencies every few seconds
      runDependencyWatcher = setInterval(function() {
        var shown = false;
        for (var dep in runDependencyTracking) {
          if (!shown) {
            shown = true;
            Module.printErr('still waiting on run dependencies:');
          }
          Module.printErr('dependency: ' + dep);
        }
        if (shown) {
          Module.printErr('(end of list)');
        }
      }, 6000);
    }
  } else {
    Module.printErr('warning: run dependency added without ID');
  }
}
Module['addRunDependency'] = addRunDependency;
function removeRunDependency(id) {
  runDependencies--;
  if (Module['monitorRunDependencies']) {
    Module['monitorRunDependencies'](runDependencies);
  }
  if (id) {
    assert(runDependencyTracking[id]);
    delete runDependencyTracking[id];
  } else {
    Module.printErr('warning: run dependency removed without ID');
  }
  if (runDependencies == 0) {
    if (runDependencyWatcher !== null) {
      clearInterval(runDependencyWatcher);
      runDependencyWatcher = null;
    } 
    if (!calledRun) run();
  }
}
Module['removeRunDependency'] = removeRunDependency;

Module["preloadedImages"] = {}; // maps url to image data
Module["preloadedAudios"] = {}; // maps url to audio data

// === Body ===




// Note: Some Emscripten settings will significantly limit the speed of the generated code.
// Note: Some Emscripten settings may limit the speed of the generated code.

function _qc_program_code_remove($self, $idx) {
  ;
  var __label__;
  __label__ = 2; 
  while(1) switch(__label__) {
    case 2: 
      var $1;
      var $2;
      var $3;
      var $i;
      var $reall;
      $2=$self;
      $3=$idx;
      var $4=$3;
      var $5=$2;
      var $6=(($5+8)|0);
      var $7=HEAP32[(($6)>>2)];
      var $8=(($4)>>>0) >= (($7)>>>0);
      if ($8) { __label__ = 3; break; } else { __label__ = 4; break; }
    case 3: 
      $1=1;
      __label__ = 13; break;
    case 4: 
      var $11=$3;
      $i=$11;
      __label__ = 5; break;
    case 5: 
      var $13=$i;
      var $14=$2;
      var $15=(($14+8)|0);
      var $16=HEAP32[(($15)>>2)];
      var $17=((($16)-(1))|0);
      var $18=(($13)>>>0) < (($17)>>>0);
      if ($18) { __label__ = 6; break; } else { __label__ = 8; break; }
    case 6: 
      var $20=$i;
      var $21=$2;
      var $22=(($21+4)|0);
      var $23=HEAP32[(($22)>>2)];
      var $24=(($23+($20<<3))|0);
      var $25=$i;
      var $26=((($25)+(1))|0);
      var $27=$2;
      var $28=(($27+4)|0);
      var $29=HEAP32[(($28)>>2)];
      var $30=(($29+($26<<3))|0);
      var $31=$24;
      var $32=$30;
      assert(8 % 1 === 0, 'memcpy given ' + 8 + ' bytes to copy. Problem with quantum=1 corrections perhaps?');HEAP16[(($31)>>1)]=HEAP16[(($32)>>1)];HEAP16[((($31)+(2))>>1)]=HEAP16[((($32)+(2))>>1)];HEAP16[((($31)+(4))>>1)]=HEAP16[((($32)+(4))>>1)];HEAP16[((($31)+(6))>>1)]=HEAP16[((($32)+(6))>>1)];
      __label__ = 7; break;
    case 7: 
      var $34=$i;
      var $35=((($34)+(1))|0);
      $i=$35;
      __label__ = 5; break;
    case 8: 
      var $37=$2;
      var $38=(($37+8)|0);
      var $39=HEAP32[(($38)>>2)];
      var $40=((($39)-(1))|0);
      HEAP32[(($38)>>2)]=$40;
      var $41=$2;
      var $42=(($41+8)|0);
      var $43=HEAP32[(($42)>>2)];
      var $44=$2;
      var $45=(($44+8)|0);
      var $46=HEAP32[(($45)>>2)];
      var $47=Math.floor(((($46)>>>0))/(2));
      var $48=(($43)>>>0) < (($47)>>>0);
      if ($48) { __label__ = 9; break; } else { __label__ = 12; break; }
    case 9: 
      var $50=$2;
      var $51=(($50+12)|0);
      var $52=HEAP32[(($51)>>2)];
      var $53=Math.floor(((($52)>>>0))/(2));
      HEAP32[(($51)>>2)]=$53;
      var $54=$2;
      var $55=(($54+8)|0);
      var $56=HEAP32[(($55)>>2)];
      var $57=((($56<<3))|0);
      var $58=_util_memory_a($57, 32, ((STRING_TABLE.__str)|0));
      var $59=$58;
      $reall=$59;
      var $60=$reall;
      var $61=(($60)|0)!=0;
      if ($61) { __label__ = 11; break; } else { __label__ = 10; break; }
    case 10: 
      $1=0;
      __label__ = 13; break;
    case 11: 
      var $64=$reall;
      var $65=$64;
      var $66=$2;
      var $67=(($66+4)|0);
      var $68=HEAP32[(($67)>>2)];
      var $69=$68;
      var $70=$2;
      var $71=(($70+8)|0);
      var $72=HEAP32[(($71)>>2)];
      var $73=((($72<<3))|0);
      assert($73 % 1 === 0, 'memcpy given ' + $73 + ' bytes to copy. Problem with quantum=1 corrections perhaps?');_memcpy($65, $69, $73, 2);
      var $74=$2;
      var $75=(($74+4)|0);
      var $76=HEAP32[(($75)>>2)];
      var $77=$76;
      _util_memory_d($77, 32, ((STRING_TABLE.__str)|0));
      var $78=$reall;
      var $79=$2;
      var $80=(($79+4)|0);
      HEAP32[(($80)>>2)]=$78;
      __label__ = 12; break;
    case 12: 
      $1=1;
      __label__ = 13; break;
    case 13: 
      var $83=$1;
      ;
      return $83;
    default: assert(0, "bad label: " + __label__);
  }
}
_qc_program_code_remove["X"]=1;

function _qc_program_code_add($self, $f) {
  var __stackBase__  = STACKTOP; assert(STACKTOP % 4 == 0, "Stack is unaligned"); assert(STACKTOP < STACK_MAX, "Ran out of stack");
  var tempParam = $f; $f = STACKTOP;STACKTOP += 8;assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack");HEAP32[(($f)>>2)]=HEAP32[((tempParam)>>2)];HEAP32[((($f)+(4))>>2)]=HEAP32[(((tempParam)+(4))>>2)];
  var __label__;
  __label__ = 2; 
  while(1) switch(__label__) {
    case 2: 
      var $1;
      var $2;
      var $reall;
      $2=$self;
      var $3=$2;
      var $4=(($3+8)|0);
      var $5=HEAP32[(($4)>>2)];
      var $6=$2;
      var $7=(($6+12)|0);
      var $8=HEAP32[(($7)>>2)];
      var $9=(($5)|0)==(($8)|0);
      if ($9) { __label__ = 3; break; } else { __label__ = 9; break; }
    case 3: 
      var $11=$2;
      var $12=(($11+12)|0);
      var $13=HEAP32[(($12)>>2)];
      var $14=(($13)|0)!=0;
      if ($14) { __label__ = 5; break; } else { __label__ = 4; break; }
    case 4: 
      var $16=$2;
      var $17=(($16+12)|0);
      HEAP32[(($17)>>2)]=16;
      __label__ = 6; break;
    case 5: 
      var $19=$2;
      var $20=(($19+12)|0);
      var $21=HEAP32[(($20)>>2)];
      var $22=((($21<<1))|0);
      HEAP32[(($20)>>2)]=$22;
      __label__ = 6; break;
    case 6: 
      var $24=$2;
      var $25=(($24+12)|0);
      var $26=HEAP32[(($25)>>2)];
      var $27=((($26<<3))|0);
      var $28=_util_memory_a($27, 32, ((STRING_TABLE.__str)|0));
      var $29=$28;
      $reall=$29;
      var $30=$reall;
      var $31=(($30)|0)!=0;
      if ($31) { __label__ = 8; break; } else { __label__ = 7; break; }
    case 7: 
      $1=0;
      __label__ = 10; break;
    case 8: 
      var $34=$reall;
      var $35=$34;
      var $36=$2;
      var $37=(($36+4)|0);
      var $38=HEAP32[(($37)>>2)];
      var $39=$38;
      var $40=$2;
      var $41=(($40+8)|0);
      var $42=HEAP32[(($41)>>2)];
      var $43=((($42<<3))|0);
      assert($43 % 1 === 0, 'memcpy given ' + $43 + ' bytes to copy. Problem with quantum=1 corrections perhaps?');_memcpy($35, $39, $43, 2);
      var $44=$2;
      var $45=(($44+4)|0);
      var $46=HEAP32[(($45)>>2)];
      var $47=$46;
      _util_memory_d($47, 32, ((STRING_TABLE.__str)|0));
      var $48=$reall;
      var $49=$2;
      var $50=(($49+4)|0);
      HEAP32[(($50)>>2)]=$48;
      __label__ = 9; break;
    case 9: 
      var $52=$2;
      var $53=(($52+8)|0);
      var $54=HEAP32[(($53)>>2)];
      var $55=((($54)+(1))|0);
      HEAP32[(($53)>>2)]=$55;
      var $56=$2;
      var $57=(($56+4)|0);
      var $58=HEAP32[(($57)>>2)];
      var $59=(($58+($54<<3))|0);
      var $60=$59;
      var $61=$f;
      assert(8 % 1 === 0, 'memcpy given ' + 8 + ' bytes to copy. Problem with quantum=1 corrections perhaps?');HEAP16[(($60)>>1)]=HEAP16[(($61)>>1)];HEAP16[((($60)+(2))>>1)]=HEAP16[((($61)+(2))>>1)];HEAP16[((($60)+(4))>>1)]=HEAP16[((($61)+(4))>>1)];HEAP16[((($60)+(6))>>1)]=HEAP16[((($61)+(6))>>1)];
      $1=1;
      __label__ = 10; break;
    case 10: 
      var $63=$1;
      STACKTOP = __stackBase__;
      return $63;
    default: assert(0, "bad label: " + __label__);
  }
}
_qc_program_code_add["X"]=1;

function _qc_program_defs_remove($self, $idx) {
  ;
  var __label__;
  __label__ = 2; 
  while(1) switch(__label__) {
    case 2: 
      var $1;
      var $2;
      var $3;
      var $i;
      var $reall;
      $2=$self;
      $3=$idx;
      var $4=$3;
      var $5=$2;
      var $6=(($5+20)|0);
      var $7=HEAP32[(($6)>>2)];
      var $8=(($4)>>>0) >= (($7)>>>0);
      if ($8) { __label__ = 3; break; } else { __label__ = 4; break; }
    case 3: 
      $1=1;
      __label__ = 13; break;
    case 4: 
      var $11=$3;
      $i=$11;
      __label__ = 5; break;
    case 5: 
      var $13=$i;
      var $14=$2;
      var $15=(($14+20)|0);
      var $16=HEAP32[(($15)>>2)];
      var $17=((($16)-(1))|0);
      var $18=(($13)>>>0) < (($17)>>>0);
      if ($18) { __label__ = 6; break; } else { __label__ = 8; break; }
    case 6: 
      var $20=$i;
      var $21=$2;
      var $22=(($21+16)|0);
      var $23=HEAP32[(($22)>>2)];
      var $24=(($23+($20<<3))|0);
      var $25=$i;
      var $26=((($25)+(1))|0);
      var $27=$2;
      var $28=(($27+16)|0);
      var $29=HEAP32[(($28)>>2)];
      var $30=(($29+($26<<3))|0);
      var $31=$24;
      var $32=$30;
      assert(8 % 1 === 0, 'memcpy given ' + 8 + ' bytes to copy. Problem with quantum=1 corrections perhaps?');HEAP32[(($31)>>2)]=HEAP32[(($32)>>2)];HEAP32[((($31)+(4))>>2)]=HEAP32[((($32)+(4))>>2)];
      __label__ = 7; break;
    case 7: 
      var $34=$i;
      var $35=((($34)+(1))|0);
      $i=$35;
      __label__ = 5; break;
    case 8: 
      var $37=$2;
      var $38=(($37+20)|0);
      var $39=HEAP32[(($38)>>2)];
      var $40=((($39)-(1))|0);
      HEAP32[(($38)>>2)]=$40;
      var $41=$2;
      var $42=(($41+20)|0);
      var $43=HEAP32[(($42)>>2)];
      var $44=$2;
      var $45=(($44+20)|0);
      var $46=HEAP32[(($45)>>2)];
      var $47=Math.floor(((($46)>>>0))/(2));
      var $48=(($43)>>>0) < (($47)>>>0);
      if ($48) { __label__ = 9; break; } else { __label__ = 12; break; }
    case 9: 
      var $50=$2;
      var $51=(($50+24)|0);
      var $52=HEAP32[(($51)>>2)];
      var $53=Math.floor(((($52)>>>0))/(2));
      HEAP32[(($51)>>2)]=$53;
      var $54=$2;
      var $55=(($54+20)|0);
      var $56=HEAP32[(($55)>>2)];
      var $57=((($56<<3))|0);
      var $58=_util_memory_a($57, 33, ((STRING_TABLE.__str)|0));
      var $59=$58;
      $reall=$59;
      var $60=$reall;
      var $61=(($60)|0)!=0;
      if ($61) { __label__ = 11; break; } else { __label__ = 10; break; }
    case 10: 
      $1=0;
      __label__ = 13; break;
    case 11: 
      var $64=$reall;
      var $65=$64;
      var $66=$2;
      var $67=(($66+16)|0);
      var $68=HEAP32[(($67)>>2)];
      var $69=$68;
      var $70=$2;
      var $71=(($70+20)|0);
      var $72=HEAP32[(($71)>>2)];
      var $73=((($72<<3))|0);
      assert($73 % 1 === 0, 'memcpy given ' + $73 + ' bytes to copy. Problem with quantum=1 corrections perhaps?');_memcpy($65, $69, $73, 4);
      var $74=$2;
      var $75=(($74+16)|0);
      var $76=HEAP32[(($75)>>2)];
      var $77=$76;
      _util_memory_d($77, 33, ((STRING_TABLE.__str)|0));
      var $78=$reall;
      var $79=$2;
      var $80=(($79+16)|0);
      HEAP32[(($80)>>2)]=$78;
      __label__ = 12; break;
    case 12: 
      $1=1;
      __label__ = 13; break;
    case 13: 
      var $83=$1;
      ;
      return $83;
    default: assert(0, "bad label: " + __label__);
  }
}
_qc_program_defs_remove["X"]=1;

function _qc_program_defs_add($self, $f) {
  var __stackBase__  = STACKTOP; assert(STACKTOP % 4 == 0, "Stack is unaligned"); assert(STACKTOP < STACK_MAX, "Ran out of stack");
  var tempParam = $f; $f = STACKTOP;STACKTOP += 8;assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack");HEAP32[(($f)>>2)]=HEAP32[((tempParam)>>2)];HEAP32[((($f)+(4))>>2)]=HEAP32[(((tempParam)+(4))>>2)];
  var __label__;
  __label__ = 2; 
  while(1) switch(__label__) {
    case 2: 
      var $1;
      var $2;
      var $reall;
      $2=$self;
      var $3=$2;
      var $4=(($3+20)|0);
      var $5=HEAP32[(($4)>>2)];
      var $6=$2;
      var $7=(($6+24)|0);
      var $8=HEAP32[(($7)>>2)];
      var $9=(($5)|0)==(($8)|0);
      if ($9) { __label__ = 3; break; } else { __label__ = 9; break; }
    case 3: 
      var $11=$2;
      var $12=(($11+24)|0);
      var $13=HEAP32[(($12)>>2)];
      var $14=(($13)|0)!=0;
      if ($14) { __label__ = 5; break; } else { __label__ = 4; break; }
    case 4: 
      var $16=$2;
      var $17=(($16+24)|0);
      HEAP32[(($17)>>2)]=16;
      __label__ = 6; break;
    case 5: 
      var $19=$2;
      var $20=(($19+24)|0);
      var $21=HEAP32[(($20)>>2)];
      var $22=((($21<<1))|0);
      HEAP32[(($20)>>2)]=$22;
      __label__ = 6; break;
    case 6: 
      var $24=$2;
      var $25=(($24+24)|0);
      var $26=HEAP32[(($25)>>2)];
      var $27=((($26<<3))|0);
      var $28=_util_memory_a($27, 33, ((STRING_TABLE.__str)|0));
      var $29=$28;
      $reall=$29;
      var $30=$reall;
      var $31=(($30)|0)!=0;
      if ($31) { __label__ = 8; break; } else { __label__ = 7; break; }
    case 7: 
      $1=0;
      __label__ = 10; break;
    case 8: 
      var $34=$reall;
      var $35=$34;
      var $36=$2;
      var $37=(($36+16)|0);
      var $38=HEAP32[(($37)>>2)];
      var $39=$38;
      var $40=$2;
      var $41=(($40+20)|0);
      var $42=HEAP32[(($41)>>2)];
      var $43=((($42<<3))|0);
      assert($43 % 1 === 0, 'memcpy given ' + $43 + ' bytes to copy. Problem with quantum=1 corrections perhaps?');_memcpy($35, $39, $43, 4);
      var $44=$2;
      var $45=(($44+16)|0);
      var $46=HEAP32[(($45)>>2)];
      var $47=$46;
      _util_memory_d($47, 33, ((STRING_TABLE.__str)|0));
      var $48=$reall;
      var $49=$2;
      var $50=(($49+16)|0);
      HEAP32[(($50)>>2)]=$48;
      __label__ = 9; break;
    case 9: 
      var $52=$2;
      var $53=(($52+20)|0);
      var $54=HEAP32[(($53)>>2)];
      var $55=((($54)+(1))|0);
      HEAP32[(($53)>>2)]=$55;
      var $56=$2;
      var $57=(($56+16)|0);
      var $58=HEAP32[(($57)>>2)];
      var $59=(($58+($54<<3))|0);
      var $60=$59;
      var $61=$f;
      assert(8 % 1 === 0, 'memcpy given ' + 8 + ' bytes to copy. Problem with quantum=1 corrections perhaps?');HEAP32[(($60)>>2)]=HEAP32[(($61)>>2)];HEAP32[((($60)+(4))>>2)]=HEAP32[((($61)+(4))>>2)];
      $1=1;
      __label__ = 10; break;
    case 10: 
      var $63=$1;
      STACKTOP = __stackBase__;
      return $63;
    default: assert(0, "bad label: " + __label__);
  }
}
_qc_program_defs_add["X"]=1;

function _qc_program_fields_remove($self, $idx) {
  ;
  var __label__;
  __label__ = 2; 
  while(1) switch(__label__) {
    case 2: 
      var $1;
      var $2;
      var $3;
      var $i;
      var $reall;
      $2=$self;
      $3=$idx;
      var $4=$3;
      var $5=$2;
      var $6=(($5+32)|0);
      var $7=HEAP32[(($6)>>2)];
      var $8=(($4)>>>0) >= (($7)>>>0);
      if ($8) { __label__ = 3; break; } else { __label__ = 4; break; }
    case 3: 
      $1=1;
      __label__ = 13; break;
    case 4: 
      var $11=$3;
      $i=$11;
      __label__ = 5; break;
    case 5: 
      var $13=$i;
      var $14=$2;
      var $15=(($14+32)|0);
      var $16=HEAP32[(($15)>>2)];
      var $17=((($16)-(1))|0);
      var $18=(($13)>>>0) < (($17)>>>0);
      if ($18) { __label__ = 6; break; } else { __label__ = 8; break; }
    case 6: 
      var $20=$i;
      var $21=$2;
      var $22=(($21+28)|0);
      var $23=HEAP32[(($22)>>2)];
      var $24=(($23+($20<<3))|0);
      var $25=$i;
      var $26=((($25)+(1))|0);
      var $27=$2;
      var $28=(($27+28)|0);
      var $29=HEAP32[(($28)>>2)];
      var $30=(($29+($26<<3))|0);
      var $31=$24;
      var $32=$30;
      assert(8 % 1 === 0, 'memcpy given ' + 8 + ' bytes to copy. Problem with quantum=1 corrections perhaps?');HEAP32[(($31)>>2)]=HEAP32[(($32)>>2)];HEAP32[((($31)+(4))>>2)]=HEAP32[((($32)+(4))>>2)];
      __label__ = 7; break;
    case 7: 
      var $34=$i;
      var $35=((($34)+(1))|0);
      $i=$35;
      __label__ = 5; break;
    case 8: 
      var $37=$2;
      var $38=(($37+32)|0);
      var $39=HEAP32[(($38)>>2)];
      var $40=((($39)-(1))|0);
      HEAP32[(($38)>>2)]=$40;
      var $41=$2;
      var $42=(($41+32)|0);
      var $43=HEAP32[(($42)>>2)];
      var $44=$2;
      var $45=(($44+32)|0);
      var $46=HEAP32[(($45)>>2)];
      var $47=Math.floor(((($46)>>>0))/(2));
      var $48=(($43)>>>0) < (($47)>>>0);
      if ($48) { __label__ = 9; break; } else { __label__ = 12; break; }
    case 9: 
      var $50=$2;
      var $51=(($50+36)|0);
      var $52=HEAP32[(($51)>>2)];
      var $53=Math.floor(((($52)>>>0))/(2));
      HEAP32[(($51)>>2)]=$53;
      var $54=$2;
      var $55=(($54+32)|0);
      var $56=HEAP32[(($55)>>2)];
      var $57=((($56<<3))|0);
      var $58=_util_memory_a($57, 34, ((STRING_TABLE.__str)|0));
      var $59=$58;
      $reall=$59;
      var $60=$reall;
      var $61=(($60)|0)!=0;
      if ($61) { __label__ = 11; break; } else { __label__ = 10; break; }
    case 10: 
      $1=0;
      __label__ = 13; break;
    case 11: 
      var $64=$reall;
      var $65=$64;
      var $66=$2;
      var $67=(($66+28)|0);
      var $68=HEAP32[(($67)>>2)];
      var $69=$68;
      var $70=$2;
      var $71=(($70+32)|0);
      var $72=HEAP32[(($71)>>2)];
      var $73=((($72<<3))|0);
      assert($73 % 1 === 0, 'memcpy given ' + $73 + ' bytes to copy. Problem with quantum=1 corrections perhaps?');_memcpy($65, $69, $73, 4);
      var $74=$2;
      var $75=(($74+28)|0);
      var $76=HEAP32[(($75)>>2)];
      var $77=$76;
      _util_memory_d($77, 34, ((STRING_TABLE.__str)|0));
      var $78=$reall;
      var $79=$2;
      var $80=(($79+28)|0);
      HEAP32[(($80)>>2)]=$78;
      __label__ = 12; break;
    case 12: 
      $1=1;
      __label__ = 13; break;
    case 13: 
      var $83=$1;
      ;
      return $83;
    default: assert(0, "bad label: " + __label__);
  }
}
_qc_program_fields_remove["X"]=1;

function _qc_program_fields_add($self, $f) {
  var __stackBase__  = STACKTOP; assert(STACKTOP % 4 == 0, "Stack is unaligned"); assert(STACKTOP < STACK_MAX, "Ran out of stack");
  var tempParam = $f; $f = STACKTOP;STACKTOP += 8;assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack");HEAP32[(($f)>>2)]=HEAP32[((tempParam)>>2)];HEAP32[((($f)+(4))>>2)]=HEAP32[(((tempParam)+(4))>>2)];
  var __label__;
  __label__ = 2; 
  while(1) switch(__label__) {
    case 2: 
      var $1;
      var $2;
      var $reall;
      $2=$self;
      var $3=$2;
      var $4=(($3+32)|0);
      var $5=HEAP32[(($4)>>2)];
      var $6=$2;
      var $7=(($6+36)|0);
      var $8=HEAP32[(($7)>>2)];
      var $9=(($5)|0)==(($8)|0);
      if ($9) { __label__ = 3; break; } else { __label__ = 9; break; }
    case 3: 
      var $11=$2;
      var $12=(($11+36)|0);
      var $13=HEAP32[(($12)>>2)];
      var $14=(($13)|0)!=0;
      if ($14) { __label__ = 5; break; } else { __label__ = 4; break; }
    case 4: 
      var $16=$2;
      var $17=(($16+36)|0);
      HEAP32[(($17)>>2)]=16;
      __label__ = 6; break;
    case 5: 
      var $19=$2;
      var $20=(($19+36)|0);
      var $21=HEAP32[(($20)>>2)];
      var $22=((($21<<1))|0);
      HEAP32[(($20)>>2)]=$22;
      __label__ = 6; break;
    case 6: 
      var $24=$2;
      var $25=(($24+36)|0);
      var $26=HEAP32[(($25)>>2)];
      var $27=((($26<<3))|0);
      var $28=_util_memory_a($27, 34, ((STRING_TABLE.__str)|0));
      var $29=$28;
      $reall=$29;
      var $30=$reall;
      var $31=(($30)|0)!=0;
      if ($31) { __label__ = 8; break; } else { __label__ = 7; break; }
    case 7: 
      $1=0;
      __label__ = 10; break;
    case 8: 
      var $34=$reall;
      var $35=$34;
      var $36=$2;
      var $37=(($36+28)|0);
      var $38=HEAP32[(($37)>>2)];
      var $39=$38;
      var $40=$2;
      var $41=(($40+32)|0);
      var $42=HEAP32[(($41)>>2)];
      var $43=((($42<<3))|0);
      assert($43 % 1 === 0, 'memcpy given ' + $43 + ' bytes to copy. Problem with quantum=1 corrections perhaps?');_memcpy($35, $39, $43, 4);
      var $44=$2;
      var $45=(($44+28)|0);
      var $46=HEAP32[(($45)>>2)];
      var $47=$46;
      _util_memory_d($47, 34, ((STRING_TABLE.__str)|0));
      var $48=$reall;
      var $49=$2;
      var $50=(($49+28)|0);
      HEAP32[(($50)>>2)]=$48;
      __label__ = 9; break;
    case 9: 
      var $52=$2;
      var $53=(($52+32)|0);
      var $54=HEAP32[(($53)>>2)];
      var $55=((($54)+(1))|0);
      HEAP32[(($53)>>2)]=$55;
      var $56=$2;
      var $57=(($56+28)|0);
      var $58=HEAP32[(($57)>>2)];
      var $59=(($58+($54<<3))|0);
      var $60=$59;
      var $61=$f;
      assert(8 % 1 === 0, 'memcpy given ' + 8 + ' bytes to copy. Problem with quantum=1 corrections perhaps?');HEAP32[(($60)>>2)]=HEAP32[(($61)>>2)];HEAP32[((($60)+(4))>>2)]=HEAP32[((($61)+(4))>>2)];
      $1=1;
      __label__ = 10; break;
    case 10: 
      var $63=$1;
      STACKTOP = __stackBase__;
      return $63;
    default: assert(0, "bad label: " + __label__);
  }
}
_qc_program_fields_add["X"]=1;

function _qc_program_functions_remove($self, $idx) {
  ;
  var __label__;
  __label__ = 2; 
  while(1) switch(__label__) {
    case 2: 
      var $1;
      var $2;
      var $3;
      var $i;
      var $reall;
      $2=$self;
      $3=$idx;
      var $4=$3;
      var $5=$2;
      var $6=(($5+44)|0);
      var $7=HEAP32[(($6)>>2)];
      var $8=(($4)>>>0) >= (($7)>>>0);
      if ($8) { __label__ = 3; break; } else { __label__ = 4; break; }
    case 3: 
      $1=1;
      __label__ = 13; break;
    case 4: 
      var $11=$3;
      $i=$11;
      __label__ = 5; break;
    case 5: 
      var $13=$i;
      var $14=$2;
      var $15=(($14+44)|0);
      var $16=HEAP32[(($15)>>2)];
      var $17=((($16)-(1))|0);
      var $18=(($13)>>>0) < (($17)>>>0);
      if ($18) { __label__ = 6; break; } else { __label__ = 8; break; }
    case 6: 
      var $20=$i;
      var $21=$2;
      var $22=(($21+40)|0);
      var $23=HEAP32[(($22)>>2)];
      var $24=(($23+($20)*(36))|0);
      var $25=$i;
      var $26=((($25)+(1))|0);
      var $27=$2;
      var $28=(($27+40)|0);
      var $29=HEAP32[(($28)>>2)];
      var $30=(($29+($26)*(36))|0);
      var $31=$24;
      var $32=$30;
      assert(36 % 1 === 0, 'memcpy given ' + 36 + ' bytes to copy. Problem with quantum=1 corrections perhaps?');for (var $$src = $32>>2, $$dest = $31>>2, $$stop = $$src + 9; $$src < $$stop; $$src++, $$dest++) {
        HEAP32[$$dest] = HEAP32[$$src]
      };
      __label__ = 7; break;
    case 7: 
      var $34=$i;
      var $35=((($34)+(1))|0);
      $i=$35;
      __label__ = 5; break;
    case 8: 
      var $37=$2;
      var $38=(($37+44)|0);
      var $39=HEAP32[(($38)>>2)];
      var $40=((($39)-(1))|0);
      HEAP32[(($38)>>2)]=$40;
      var $41=$2;
      var $42=(($41+44)|0);
      var $43=HEAP32[(($42)>>2)];
      var $44=$2;
      var $45=(($44+44)|0);
      var $46=HEAP32[(($45)>>2)];
      var $47=Math.floor(((($46)>>>0))/(2));
      var $48=(($43)>>>0) < (($47)>>>0);
      if ($48) { __label__ = 9; break; } else { __label__ = 12; break; }
    case 9: 
      var $50=$2;
      var $51=(($50+48)|0);
      var $52=HEAP32[(($51)>>2)];
      var $53=Math.floor(((($52)>>>0))/(2));
      HEAP32[(($51)>>2)]=$53;
      var $54=$2;
      var $55=(($54+44)|0);
      var $56=HEAP32[(($55)>>2)];
      var $57=((($56)*(36))|0);
      var $58=_util_memory_a($57, 35, ((STRING_TABLE.__str)|0));
      var $59=$58;
      $reall=$59;
      var $60=$reall;
      var $61=(($60)|0)!=0;
      if ($61) { __label__ = 11; break; } else { __label__ = 10; break; }
    case 10: 
      $1=0;
      __label__ = 13; break;
    case 11: 
      var $64=$reall;
      var $65=$64;
      var $66=$2;
      var $67=(($66+40)|0);
      var $68=HEAP32[(($67)>>2)];
      var $69=$68;
      var $70=$2;
      var $71=(($70+44)|0);
      var $72=HEAP32[(($71)>>2)];
      var $73=((($72)*(36))|0);
      assert($73 % 1 === 0, 'memcpy given ' + $73 + ' bytes to copy. Problem with quantum=1 corrections perhaps?');_memcpy($65, $69, $73, 4);
      var $74=$2;
      var $75=(($74+40)|0);
      var $76=HEAP32[(($75)>>2)];
      var $77=$76;
      _util_memory_d($77, 35, ((STRING_TABLE.__str)|0));
      var $78=$reall;
      var $79=$2;
      var $80=(($79+40)|0);
      HEAP32[(($80)>>2)]=$78;
      __label__ = 12; break;
    case 12: 
      $1=1;
      __label__ = 13; break;
    case 13: 
      var $83=$1;
      ;
      return $83;
    default: assert(0, "bad label: " + __label__);
  }
}
_qc_program_functions_remove["X"]=1;

function _qc_program_functions_add($self, $f) {
  var __stackBase__  = STACKTOP; assert(STACKTOP % 4 == 0, "Stack is unaligned"); assert(STACKTOP < STACK_MAX, "Ran out of stack");
  var tempParam = $f; $f = STACKTOP;STACKTOP += 36;assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack");for (var $$src = tempParam>>2, $$dest = $f>>2, $$stop = $$src + 9; $$src < $$stop; $$src++, $$dest++) {
  HEAP32[$$dest] = HEAP32[$$src]
};
  var __label__;
  __label__ = 2; 
  while(1) switch(__label__) {
    case 2: 
      var $1;
      var $2;
      var $reall;
      $2=$self;
      var $3=$2;
      var $4=(($3+44)|0);
      var $5=HEAP32[(($4)>>2)];
      var $6=$2;
      var $7=(($6+48)|0);
      var $8=HEAP32[(($7)>>2)];
      var $9=(($5)|0)==(($8)|0);
      if ($9) { __label__ = 3; break; } else { __label__ = 9; break; }
    case 3: 
      var $11=$2;
      var $12=(($11+48)|0);
      var $13=HEAP32[(($12)>>2)];
      var $14=(($13)|0)!=0;
      if ($14) { __label__ = 5; break; } else { __label__ = 4; break; }
    case 4: 
      var $16=$2;
      var $17=(($16+48)|0);
      HEAP32[(($17)>>2)]=16;
      __label__ = 6; break;
    case 5: 
      var $19=$2;
      var $20=(($19+48)|0);
      var $21=HEAP32[(($20)>>2)];
      var $22=((($21<<1))|0);
      HEAP32[(($20)>>2)]=$22;
      __label__ = 6; break;
    case 6: 
      var $24=$2;
      var $25=(($24+48)|0);
      var $26=HEAP32[(($25)>>2)];
      var $27=((($26)*(36))|0);
      var $28=_util_memory_a($27, 35, ((STRING_TABLE.__str)|0));
      var $29=$28;
      $reall=$29;
      var $30=$reall;
      var $31=(($30)|0)!=0;
      if ($31) { __label__ = 8; break; } else { __label__ = 7; break; }
    case 7: 
      $1=0;
      __label__ = 10; break;
    case 8: 
      var $34=$reall;
      var $35=$34;
      var $36=$2;
      var $37=(($36+40)|0);
      var $38=HEAP32[(($37)>>2)];
      var $39=$38;
      var $40=$2;
      var $41=(($40+44)|0);
      var $42=HEAP32[(($41)>>2)];
      var $43=((($42)*(36))|0);
      assert($43 % 1 === 0, 'memcpy given ' + $43 + ' bytes to copy. Problem with quantum=1 corrections perhaps?');_memcpy($35, $39, $43, 4);
      var $44=$2;
      var $45=(($44+40)|0);
      var $46=HEAP32[(($45)>>2)];
      var $47=$46;
      _util_memory_d($47, 35, ((STRING_TABLE.__str)|0));
      var $48=$reall;
      var $49=$2;
      var $50=(($49+40)|0);
      HEAP32[(($50)>>2)]=$48;
      __label__ = 9; break;
    case 9: 
      var $52=$2;
      var $53=(($52+44)|0);
      var $54=HEAP32[(($53)>>2)];
      var $55=((($54)+(1))|0);
      HEAP32[(($53)>>2)]=$55;
      var $56=$2;
      var $57=(($56+40)|0);
      var $58=HEAP32[(($57)>>2)];
      var $59=(($58+($54)*(36))|0);
      var $60=$59;
      var $61=$f;
      assert(36 % 1 === 0, 'memcpy given ' + 36 + ' bytes to copy. Problem with quantum=1 corrections perhaps?');for (var $$src = $61>>2, $$dest = $60>>2, $$stop = $$src + 9; $$src < $$stop; $$src++, $$dest++) {
        HEAP32[$$dest] = HEAP32[$$src]
      };
      $1=1;
      __label__ = 10; break;
    case 10: 
      var $63=$1;
      STACKTOP = __stackBase__;
      return $63;
    default: assert(0, "bad label: " + __label__);
  }
}
_qc_program_functions_add["X"]=1;

function _qc_program_strings_remove($self, $idx) {
  ;
  var __label__;
  __label__ = 2; 
  while(1) switch(__label__) {
    case 2: 
      var $1;
      var $2;
      var $3;
      var $i;
      var $reall;
      $2=$self;
      $3=$idx;
      var $4=$3;
      var $5=$2;
      var $6=(($5+56)|0);
      var $7=HEAP32[(($6)>>2)];
      var $8=(($4)>>>0) >= (($7)>>>0);
      if ($8) { __label__ = 3; break; } else { __label__ = 4; break; }
    case 3: 
      $1=1;
      __label__ = 13; break;
    case 4: 
      var $11=$3;
      $i=$11;
      __label__ = 5; break;
    case 5: 
      var $13=$i;
      var $14=$2;
      var $15=(($14+56)|0);
      var $16=HEAP32[(($15)>>2)];
      var $17=((($16)-(1))|0);
      var $18=(($13)>>>0) < (($17)>>>0);
      if ($18) { __label__ = 6; break; } else { __label__ = 8; break; }
    case 6: 
      var $20=$i;
      var $21=((($20)+(1))|0);
      var $22=$2;
      var $23=(($22+52)|0);
      var $24=HEAP32[(($23)>>2)];
      var $25=(($24+$21)|0);
      var $26=HEAP8[($25)];
      var $27=$i;
      var $28=$2;
      var $29=(($28+52)|0);
      var $30=HEAP32[(($29)>>2)];
      var $31=(($30+$27)|0);
      HEAP8[($31)]=$26;
      __label__ = 7; break;
    case 7: 
      var $33=$i;
      var $34=((($33)+(1))|0);
      $i=$34;
      __label__ = 5; break;
    case 8: 
      var $36=$2;
      var $37=(($36+56)|0);
      var $38=HEAP32[(($37)>>2)];
      var $39=((($38)-(1))|0);
      HEAP32[(($37)>>2)]=$39;
      var $40=$2;
      var $41=(($40+56)|0);
      var $42=HEAP32[(($41)>>2)];
      var $43=$2;
      var $44=(($43+56)|0);
      var $45=HEAP32[(($44)>>2)];
      var $46=Math.floor(((($45)>>>0))/(2));
      var $47=(($42)>>>0) < (($46)>>>0);
      if ($47) { __label__ = 9; break; } else { __label__ = 12; break; }
    case 9: 
      var $49=$2;
      var $50=(($49+60)|0);
      var $51=HEAP32[(($50)>>2)];
      var $52=Math.floor(((($51)>>>0))/(2));
      HEAP32[(($50)>>2)]=$52;
      var $53=$2;
      var $54=(($53+56)|0);
      var $55=HEAP32[(($54)>>2)];
      var $56=(($55)|0);
      var $57=_util_memory_a($56, 36, ((STRING_TABLE.__str)|0));
      $reall=$57;
      var $58=$reall;
      var $59=(($58)|0)!=0;
      if ($59) { __label__ = 11; break; } else { __label__ = 10; break; }
    case 10: 
      $1=0;
      __label__ = 13; break;
    case 11: 
      var $62=$reall;
      var $63=$2;
      var $64=(($63+52)|0);
      var $65=HEAP32[(($64)>>2)];
      var $66=$2;
      var $67=(($66+56)|0);
      var $68=HEAP32[(($67)>>2)];
      var $69=(($68)|0);
      assert($69 % 1 === 0, 'memcpy given ' + $69 + ' bytes to copy. Problem with quantum=1 corrections perhaps?');_memcpy($62, $65, $69, 1);
      var $70=$2;
      var $71=(($70+52)|0);
      var $72=HEAP32[(($71)>>2)];
      _util_memory_d($72, 36, ((STRING_TABLE.__str)|0));
      var $73=$reall;
      var $74=$2;
      var $75=(($74+52)|0);
      HEAP32[(($75)>>2)]=$73;
      __label__ = 12; break;
    case 12: 
      $1=1;
      __label__ = 13; break;
    case 13: 
      var $78=$1;
      ;
      return $78;
    default: assert(0, "bad label: " + __label__);
  }
}
_qc_program_strings_remove["X"]=1;

function _qc_program_strings_add($self, $f) {
  ;
  var __label__;
  __label__ = 2; 
  while(1) switch(__label__) {
    case 2: 
      var $1;
      var $2;
      var $3;
      var $reall;
      $2=$self;
      $3=$f;
      var $4=$2;
      var $5=(($4+56)|0);
      var $6=HEAP32[(($5)>>2)];
      var $7=$2;
      var $8=(($7+60)|0);
      var $9=HEAP32[(($8)>>2)];
      var $10=(($6)|0)==(($9)|0);
      if ($10) { __label__ = 3; break; } else { __label__ = 9; break; }
    case 3: 
      var $12=$2;
      var $13=(($12+60)|0);
      var $14=HEAP32[(($13)>>2)];
      var $15=(($14)|0)!=0;
      if ($15) { __label__ = 5; break; } else { __label__ = 4; break; }
    case 4: 
      var $17=$2;
      var $18=(($17+60)|0);
      HEAP32[(($18)>>2)]=16;
      __label__ = 6; break;
    case 5: 
      var $20=$2;
      var $21=(($20+60)|0);
      var $22=HEAP32[(($21)>>2)];
      var $23=((($22<<1))|0);
      HEAP32[(($21)>>2)]=$23;
      __label__ = 6; break;
    case 6: 
      var $25=$2;
      var $26=(($25+60)|0);
      var $27=HEAP32[(($26)>>2)];
      var $28=(($27)|0);
      var $29=_util_memory_a($28, 36, ((STRING_TABLE.__str)|0));
      $reall=$29;
      var $30=$reall;
      var $31=(($30)|0)!=0;
      if ($31) { __label__ = 8; break; } else { __label__ = 7; break; }
    case 7: 
      $1=0;
      __label__ = 10; break;
    case 8: 
      var $34=$reall;
      var $35=$2;
      var $36=(($35+52)|0);
      var $37=HEAP32[(($36)>>2)];
      var $38=$2;
      var $39=(($38+56)|0);
      var $40=HEAP32[(($39)>>2)];
      var $41=(($40)|0);
      assert($41 % 1 === 0, 'memcpy given ' + $41 + ' bytes to copy. Problem with quantum=1 corrections perhaps?');_memcpy($34, $37, $41, 1);
      var $42=$2;
      var $43=(($42+52)|0);
      var $44=HEAP32[(($43)>>2)];
      _util_memory_d($44, 36, ((STRING_TABLE.__str)|0));
      var $45=$reall;
      var $46=$2;
      var $47=(($46+52)|0);
      HEAP32[(($47)>>2)]=$45;
      __label__ = 9; break;
    case 9: 
      var $49=$3;
      var $50=$2;
      var $51=(($50+56)|0);
      var $52=HEAP32[(($51)>>2)];
      var $53=((($52)+(1))|0);
      HEAP32[(($51)>>2)]=$53;
      var $54=$2;
      var $55=(($54+52)|0);
      var $56=HEAP32[(($55)>>2)];
      var $57=(($56+$52)|0);
      HEAP8[($57)]=$49;
      $1=1;
      __label__ = 10; break;
    case 10: 
      var $59=$1;
      ;
      return $59;
    default: assert(0, "bad label: " + __label__);
  }
}
_qc_program_strings_add["X"]=1;

function _qc_program_strings_append($s, $p, $c) {
  ;
  var __label__;
  __label__ = 2; 
  while(1) switch(__label__) {
    case 2: 
      var $1;
      var $2;
      var $3;
      var $4;
      var $reall;
      var $oldalloc;
      $2=$s;
      $3=$p;
      $4=$c;
      var $5=$2;
      var $6=(($5+56)|0);
      var $7=HEAP32[(($6)>>2)];
      var $8=$4;
      var $9=((($7)+($8))|0);
      var $10=$2;
      var $11=(($10+60)|0);
      var $12=HEAP32[(($11)>>2)];
      var $13=(($9)>>>0) > (($12)>>>0);
      if ($13) { __label__ = 3; break; } else { __label__ = 14; break; }
    case 3: 
      var $15=$2;
      var $16=(($15+60)|0);
      var $17=HEAP32[(($16)>>2)];
      var $18=(($17)|0)!=0;
      if ($18) { __label__ = 8; break; } else { __label__ = 4; break; }
    case 4: 
      var $20=$4;
      var $21=(($20)>>>0) < 16;
      if ($21) { __label__ = 5; break; } else { __label__ = 6; break; }
    case 5: 
      var $26 = 16;__label__ = 7; break;
    case 6: 
      var $24=$4;
      var $26 = $24;__label__ = 7; break;
    case 7: 
      var $26;
      var $27=$2;
      var $28=(($27+60)|0);
      HEAP32[(($28)>>2)]=$26;
      var $29=$2;
      var $30=(($29+60)|0);
      var $31=HEAP32[(($30)>>2)];
      var $32=(($31)|0);
      var $33=_util_memory_a($32, 37, ((STRING_TABLE.__str)|0));
      var $34=$2;
      var $35=(($34+52)|0);
      HEAP32[(($35)>>2)]=$33;
      __label__ = 13; break;
    case 8: 
      var $37=$2;
      var $38=(($37+60)|0);
      var $39=HEAP32[(($38)>>2)];
      $oldalloc=$39;
      var $40=$2;
      var $41=(($40+60)|0);
      var $42=HEAP32[(($41)>>2)];
      var $43=((($42<<1))|0);
      HEAP32[(($41)>>2)]=$43;
      var $44=$2;
      var $45=(($44+56)|0);
      var $46=HEAP32[(($45)>>2)];
      var $47=$4;
      var $48=((($46)+($47))|0);
      var $49=$2;
      var $50=(($49+60)|0);
      var $51=HEAP32[(($50)>>2)];
      var $52=(($48)>>>0) >= (($51)>>>0);
      if ($52) { __label__ = 9; break; } else { __label__ = 10; break; }
    case 9: 
      var $54=$2;
      var $55=(($54+56)|0);
      var $56=HEAP32[(($55)>>2)];
      var $57=$4;
      var $58=((($56)+($57))|0);
      var $59=$2;
      var $60=(($59+60)|0);
      HEAP32[(($60)>>2)]=$58;
      __label__ = 10; break;
    case 10: 
      var $62=$2;
      var $63=(($62+60)|0);
      var $64=HEAP32[(($63)>>2)];
      var $65=(($64)|0);
      var $66=_util_memory_a($65, 37, ((STRING_TABLE.__str)|0));
      $reall=$66;
      var $67=$reall;
      var $68=(($67)|0)!=0;
      if ($68) { __label__ = 12; break; } else { __label__ = 11; break; }
    case 11: 
      var $70=$oldalloc;
      var $71=$2;
      var $72=(($71+60)|0);
      HEAP32[(($72)>>2)]=$70;
      $1=0;
      __label__ = 15; break;
    case 12: 
      var $74=$reall;
      var $75=$2;
      var $76=(($75+52)|0);
      var $77=HEAP32[(($76)>>2)];
      var $78=$2;
      var $79=(($78+56)|0);
      var $80=HEAP32[(($79)>>2)];
      var $81=(($80)|0);
      assert($81 % 1 === 0, 'memcpy given ' + $81 + ' bytes to copy. Problem with quantum=1 corrections perhaps?');_memcpy($74, $77, $81, 1);
      var $82=$2;
      var $83=(($82+52)|0);
      var $84=HEAP32[(($83)>>2)];
      _util_memory_d($84, 37, ((STRING_TABLE.__str)|0));
      var $85=$reall;
      var $86=$2;
      var $87=(($86+52)|0);
      HEAP32[(($87)>>2)]=$85;
      __label__ = 13; break;
    case 13: 
      __label__ = 14; break;
    case 14: 
      var $90=$2;
      var $91=(($90+56)|0);
      var $92=HEAP32[(($91)>>2)];
      var $93=$2;
      var $94=(($93+52)|0);
      var $95=HEAP32[(($94)>>2)];
      var $96=(($95+$92)|0);
      var $97=$3;
      var $98=$4;
      var $99=(($98)|0);
      assert($99 % 1 === 0, 'memcpy given ' + $99 + ' bytes to copy. Problem with quantum=1 corrections perhaps?');_memcpy($96, $97, $99, 1);
      var $100=$4;
      var $101=$2;
      var $102=(($101+56)|0);
      var $103=HEAP32[(($102)>>2)];
      var $104=((($103)+($100))|0);
      HEAP32[(($102)>>2)]=$104;
      $1=1;
      __label__ = 15; break;
    case 15: 
      var $106=$1;
      ;
      return $106;
    default: assert(0, "bad label: " + __label__);
  }
}
_qc_program_strings_append["X"]=1;

function _qc_program_strings_resize($s, $c) {
  ;
  var __label__;
  __label__ = 2; 
  while(1) switch(__label__) {
    case 2: 
      var $1;
      var $2;
      var $3;
      var $reall;
      $2=$s;
      $3=$c;
      var $4=$3;
      var $5=$2;
      var $6=(($5+60)|0);
      var $7=HEAP32[(($6)>>2)];
      var $8=(($4)>>>0) > (($7)>>>0);
      if ($8) { __label__ = 3; break; } else { __label__ = 6; break; }
    case 3: 
      var $10=$3;
      var $11=(($10)|0);
      var $12=_util_memory_a($11, 38, ((STRING_TABLE.__str)|0));
      $reall=$12;
      var $13=$reall;
      var $14=(($13)|0)!=0;
      if ($14) { __label__ = 5; break; } else { __label__ = 4; break; }
    case 4: 
      $1=0;
      __label__ = 11; break;
    case 5: 
      var $17=$reall;
      var $18=$2;
      var $19=(($18+52)|0);
      var $20=HEAP32[(($19)>>2)];
      var $21=$2;
      var $22=(($21+56)|0);
      var $23=HEAP32[(($22)>>2)];
      var $24=(($23)|0);
      assert($24 % 1 === 0, 'memcpy given ' + $24 + ' bytes to copy. Problem with quantum=1 corrections perhaps?');_memcpy($17, $20, $24, 1);
      var $25=$3;
      var $26=$2;
      var $27=(($26+60)|0);
      HEAP32[(($27)>>2)]=$25;
      var $28=$3;
      var $29=$2;
      var $30=(($29+56)|0);
      HEAP32[(($30)>>2)]=$28;
      var $31=$2;
      var $32=(($31+52)|0);
      var $33=HEAP32[(($32)>>2)];
      _util_memory_d($33, 38, ((STRING_TABLE.__str)|0));
      var $34=$reall;
      var $35=$2;
      var $36=(($35+52)|0);
      HEAP32[(($36)>>2)]=$34;
      $1=1;
      __label__ = 11; break;
    case 6: 
      var $38=$3;
      var $39=$2;
      var $40=(($39+56)|0);
      HEAP32[(($40)>>2)]=$38;
      var $41=$3;
      var $42=$2;
      var $43=(($42+60)|0);
      var $44=HEAP32[(($43)>>2)];
      var $45=Math.floor(((($44)>>>0))/(2));
      var $46=(($41)>>>0) < (($45)>>>0);
      if ($46) { __label__ = 7; break; } else { __label__ = 10; break; }
    case 7: 
      var $48=$3;
      var $49=(($48)|0);
      var $50=_util_memory_a($49, 38, ((STRING_TABLE.__str)|0));
      $reall=$50;
      var $51=$reall;
      var $52=(($51)|0)!=0;
      if ($52) { __label__ = 9; break; } else { __label__ = 8; break; }
    case 8: 
      $1=0;
      __label__ = 11; break;
    case 9: 
      var $55=$reall;
      var $56=$2;
      var $57=(($56+52)|0);
      var $58=HEAP32[(($57)>>2)];
      var $59=$3;
      var $60=(($59)|0);
      assert($60 % 1 === 0, 'memcpy given ' + $60 + ' bytes to copy. Problem with quantum=1 corrections perhaps?');_memcpy($55, $58, $60, 1);
      var $61=$2;
      var $62=(($61+52)|0);
      var $63=HEAP32[(($62)>>2)];
      _util_memory_d($63, 38, ((STRING_TABLE.__str)|0));
      var $64=$reall;
      var $65=$2;
      var $66=(($65+52)|0);
      HEAP32[(($66)>>2)]=$64;
      var $67=$3;
      var $68=$2;
      var $69=(($68+60)|0);
      HEAP32[(($69)>>2)]=$67;
      __label__ = 10; break;
    case 10: 
      $1=1;
      __label__ = 11; break;
    case 11: 
      var $72=$1;
      ;
      return $72;
    default: assert(0, "bad label: " + __label__);
  }
}
_qc_program_strings_resize["X"]=1;

function _qc_program_globals_remove($self, $idx) {
  ;
  var __label__;
  __label__ = 2; 
  while(1) switch(__label__) {
    case 2: 
      var $1;
      var $2;
      var $3;
      var $i;
      var $reall;
      $2=$self;
      $3=$idx;
      var $4=$3;
      var $5=$2;
      var $6=(($5+68)|0);
      var $7=HEAP32[(($6)>>2)];
      var $8=(($4)>>>0) >= (($7)>>>0);
      if ($8) { __label__ = 3; break; } else { __label__ = 4; break; }
    case 3: 
      $1=1;
      __label__ = 13; break;
    case 4: 
      var $11=$3;
      $i=$11;
      __label__ = 5; break;
    case 5: 
      var $13=$i;
      var $14=$2;
      var $15=(($14+68)|0);
      var $16=HEAP32[(($15)>>2)];
      var $17=((($16)-(1))|0);
      var $18=(($13)>>>0) < (($17)>>>0);
      if ($18) { __label__ = 6; break; } else { __label__ = 8; break; }
    case 6: 
      var $20=$i;
      var $21=((($20)+(1))|0);
      var $22=$2;
      var $23=(($22+64)|0);
      var $24=HEAP32[(($23)>>2)];
      var $25=(($24+($21<<2))|0);
      var $26=HEAP32[(($25)>>2)];
      var $27=$i;
      var $28=$2;
      var $29=(($28+64)|0);
      var $30=HEAP32[(($29)>>2)];
      var $31=(($30+($27<<2))|0);
      HEAP32[(($31)>>2)]=$26;
      __label__ = 7; break;
    case 7: 
      var $33=$i;
      var $34=((($33)+(1))|0);
      $i=$34;
      __label__ = 5; break;
    case 8: 
      var $36=$2;
      var $37=(($36+68)|0);
      var $38=HEAP32[(($37)>>2)];
      var $39=((($38)-(1))|0);
      HEAP32[(($37)>>2)]=$39;
      var $40=$2;
      var $41=(($40+68)|0);
      var $42=HEAP32[(($41)>>2)];
      var $43=$2;
      var $44=(($43+68)|0);
      var $45=HEAP32[(($44)>>2)];
      var $46=Math.floor(((($45)>>>0))/(2));
      var $47=(($42)>>>0) < (($46)>>>0);
      if ($47) { __label__ = 9; break; } else { __label__ = 12; break; }
    case 9: 
      var $49=$2;
      var $50=(($49+72)|0);
      var $51=HEAP32[(($50)>>2)];
      var $52=Math.floor(((($51)>>>0))/(2));
      HEAP32[(($50)>>2)]=$52;
      var $53=$2;
      var $54=(($53+68)|0);
      var $55=HEAP32[(($54)>>2)];
      var $56=((($55<<2))|0);
      var $57=_util_memory_a($56, 39, ((STRING_TABLE.__str)|0));
      var $58=$57;
      $reall=$58;
      var $59=$reall;
      var $60=(($59)|0)!=0;
      if ($60) { __label__ = 11; break; } else { __label__ = 10; break; }
    case 10: 
      $1=0;
      __label__ = 13; break;
    case 11: 
      var $63=$reall;
      var $64=$63;
      var $65=$2;
      var $66=(($65+64)|0);
      var $67=HEAP32[(($66)>>2)];
      var $68=$67;
      var $69=$2;
      var $70=(($69+68)|0);
      var $71=HEAP32[(($70)>>2)];
      var $72=((($71<<2))|0);
      assert($72 % 1 === 0, 'memcpy given ' + $72 + ' bytes to copy. Problem with quantum=1 corrections perhaps?');_memcpy($64, $68, $72, 4);
      var $73=$2;
      var $74=(($73+64)|0);
      var $75=HEAP32[(($74)>>2)];
      var $76=$75;
      _util_memory_d($76, 39, ((STRING_TABLE.__str)|0));
      var $77=$reall;
      var $78=$2;
      var $79=(($78+64)|0);
      HEAP32[(($79)>>2)]=$77;
      __label__ = 12; break;
    case 12: 
      $1=1;
      __label__ = 13; break;
    case 13: 
      var $82=$1;
      ;
      return $82;
    default: assert(0, "bad label: " + __label__);
  }
}
_qc_program_globals_remove["X"]=1;

function _qc_program_globals_add($self, $f) {
  ;
  var __label__;
  __label__ = 2; 
  while(1) switch(__label__) {
    case 2: 
      var $1;
      var $2;
      var $3;
      var $reall;
      $2=$self;
      $3=$f;
      var $4=$2;
      var $5=(($4+68)|0);
      var $6=HEAP32[(($5)>>2)];
      var $7=$2;
      var $8=(($7+72)|0);
      var $9=HEAP32[(($8)>>2)];
      var $10=(($6)|0)==(($9)|0);
      if ($10) { __label__ = 3; break; } else { __label__ = 9; break; }
    case 3: 
      var $12=$2;
      var $13=(($12+72)|0);
      var $14=HEAP32[(($13)>>2)];
      var $15=(($14)|0)!=0;
      if ($15) { __label__ = 5; break; } else { __label__ = 4; break; }
    case 4: 
      var $17=$2;
      var $18=(($17+72)|0);
      HEAP32[(($18)>>2)]=16;
      __label__ = 6; break;
    case 5: 
      var $20=$2;
      var $21=(($20+72)|0);
      var $22=HEAP32[(($21)>>2)];
      var $23=((($22<<1))|0);
      HEAP32[(($21)>>2)]=$23;
      __label__ = 6; break;
    case 6: 
      var $25=$2;
      var $26=(($25+72)|0);
      var $27=HEAP32[(($26)>>2)];
      var $28=((($27<<2))|0);
      var $29=_util_memory_a($28, 39, ((STRING_TABLE.__str)|0));
      var $30=$29;
      $reall=$30;
      var $31=$reall;
      var $32=(($31)|0)!=0;
      if ($32) { __label__ = 8; break; } else { __label__ = 7; break; }
    case 7: 
      $1=0;
      __label__ = 10; break;
    case 8: 
      var $35=$reall;
      var $36=$35;
      var $37=$2;
      var $38=(($37+64)|0);
      var $39=HEAP32[(($38)>>2)];
      var $40=$39;
      var $41=$2;
      var $42=(($41+68)|0);
      var $43=HEAP32[(($42)>>2)];
      var $44=((($43<<2))|0);
      assert($44 % 1 === 0, 'memcpy given ' + $44 + ' bytes to copy. Problem with quantum=1 corrections perhaps?');_memcpy($36, $40, $44, 4);
      var $45=$2;
      var $46=(($45+64)|0);
      var $47=HEAP32[(($46)>>2)];
      var $48=$47;
      _util_memory_d($48, 39, ((STRING_TABLE.__str)|0));
      var $49=$reall;
      var $50=$2;
      var $51=(($50+64)|0);
      HEAP32[(($51)>>2)]=$49;
      __label__ = 9; break;
    case 9: 
      var $53=$3;
      var $54=$2;
      var $55=(($54+68)|0);
      var $56=HEAP32[(($55)>>2)];
      var $57=((($56)+(1))|0);
      HEAP32[(($55)>>2)]=$57;
      var $58=$2;
      var $59=(($58+64)|0);
      var $60=HEAP32[(($59)>>2)];
      var $61=(($60+($56<<2))|0);
      HEAP32[(($61)>>2)]=$53;
      $1=1;
      __label__ = 10; break;
    case 10: 
      var $63=$1;
      ;
      return $63;
    default: assert(0, "bad label: " + __label__);
  }
}
_qc_program_globals_add["X"]=1;

function _qc_program_entitydata_remove($self, $idx) {
  ;
  var __label__;
  __label__ = 2; 
  while(1) switch(__label__) {
    case 2: 
      var $1;
      var $2;
      var $3;
      var $i;
      var $reall;
      $2=$self;
      $3=$idx;
      var $4=$3;
      var $5=$2;
      var $6=(($5+80)|0);
      var $7=HEAP32[(($6)>>2)];
      var $8=(($4)>>>0) >= (($7)>>>0);
      if ($8) { __label__ = 3; break; } else { __label__ = 4; break; }
    case 3: 
      $1=1;
      __label__ = 13; break;
    case 4: 
      var $11=$3;
      $i=$11;
      __label__ = 5; break;
    case 5: 
      var $13=$i;
      var $14=$2;
      var $15=(($14+80)|0);
      var $16=HEAP32[(($15)>>2)];
      var $17=((($16)-(1))|0);
      var $18=(($13)>>>0) < (($17)>>>0);
      if ($18) { __label__ = 6; break; } else { __label__ = 8; break; }
    case 6: 
      var $20=$i;
      var $21=((($20)+(1))|0);
      var $22=$2;
      var $23=(($22+76)|0);
      var $24=HEAP32[(($23)>>2)];
      var $25=(($24+($21<<2))|0);
      var $26=HEAP32[(($25)>>2)];
      var $27=$i;
      var $28=$2;
      var $29=(($28+76)|0);
      var $30=HEAP32[(($29)>>2)];
      var $31=(($30+($27<<2))|0);
      HEAP32[(($31)>>2)]=$26;
      __label__ = 7; break;
    case 7: 
      var $33=$i;
      var $34=((($33)+(1))|0);
      $i=$34;
      __label__ = 5; break;
    case 8: 
      var $36=$2;
      var $37=(($36+80)|0);
      var $38=HEAP32[(($37)>>2)];
      var $39=((($38)-(1))|0);
      HEAP32[(($37)>>2)]=$39;
      var $40=$2;
      var $41=(($40+80)|0);
      var $42=HEAP32[(($41)>>2)];
      var $43=$2;
      var $44=(($43+80)|0);
      var $45=HEAP32[(($44)>>2)];
      var $46=Math.floor(((($45)>>>0))/(2));
      var $47=(($42)>>>0) < (($46)>>>0);
      if ($47) { __label__ = 9; break; } else { __label__ = 12; break; }
    case 9: 
      var $49=$2;
      var $50=(($49+84)|0);
      var $51=HEAP32[(($50)>>2)];
      var $52=Math.floor(((($51)>>>0))/(2));
      HEAP32[(($50)>>2)]=$52;
      var $53=$2;
      var $54=(($53+80)|0);
      var $55=HEAP32[(($54)>>2)];
      var $56=((($55<<2))|0);
      var $57=_util_memory_a($56, 40, ((STRING_TABLE.__str)|0));
      var $58=$57;
      $reall=$58;
      var $59=$reall;
      var $60=(($59)|0)!=0;
      if ($60) { __label__ = 11; break; } else { __label__ = 10; break; }
    case 10: 
      $1=0;
      __label__ = 13; break;
    case 11: 
      var $63=$reall;
      var $64=$63;
      var $65=$2;
      var $66=(($65+76)|0);
      var $67=HEAP32[(($66)>>2)];
      var $68=$67;
      var $69=$2;
      var $70=(($69+80)|0);
      var $71=HEAP32[(($70)>>2)];
      var $72=((($71<<2))|0);
      assert($72 % 1 === 0, 'memcpy given ' + $72 + ' bytes to copy. Problem with quantum=1 corrections perhaps?');_memcpy($64, $68, $72, 4);
      var $73=$2;
      var $74=(($73+76)|0);
      var $75=HEAP32[(($74)>>2)];
      var $76=$75;
      _util_memory_d($76, 40, ((STRING_TABLE.__str)|0));
      var $77=$reall;
      var $78=$2;
      var $79=(($78+76)|0);
      HEAP32[(($79)>>2)]=$77;
      __label__ = 12; break;
    case 12: 
      $1=1;
      __label__ = 13; break;
    case 13: 
      var $82=$1;
      ;
      return $82;
    default: assert(0, "bad label: " + __label__);
  }
}
_qc_program_entitydata_remove["X"]=1;

function _qc_program_entitydata_add($self, $f) {
  ;
  var __label__;
  __label__ = 2; 
  while(1) switch(__label__) {
    case 2: 
      var $1;
      var $2;
      var $3;
      var $reall;
      $2=$self;
      $3=$f;
      var $4=$2;
      var $5=(($4+80)|0);
      var $6=HEAP32[(($5)>>2)];
      var $7=$2;
      var $8=(($7+84)|0);
      var $9=HEAP32[(($8)>>2)];
      var $10=(($6)|0)==(($9)|0);
      if ($10) { __label__ = 3; break; } else { __label__ = 9; break; }
    case 3: 
      var $12=$2;
      var $13=(($12+84)|0);
      var $14=HEAP32[(($13)>>2)];
      var $15=(($14)|0)!=0;
      if ($15) { __label__ = 5; break; } else { __label__ = 4; break; }
    case 4: 
      var $17=$2;
      var $18=(($17+84)|0);
      HEAP32[(($18)>>2)]=16;
      __label__ = 6; break;
    case 5: 
      var $20=$2;
      var $21=(($20+84)|0);
      var $22=HEAP32[(($21)>>2)];
      var $23=((($22<<1))|0);
      HEAP32[(($21)>>2)]=$23;
      __label__ = 6; break;
    case 6: 
      var $25=$2;
      var $26=(($25+84)|0);
      var $27=HEAP32[(($26)>>2)];
      var $28=((($27<<2))|0);
      var $29=_util_memory_a($28, 40, ((STRING_TABLE.__str)|0));
      var $30=$29;
      $reall=$30;
      var $31=$reall;
      var $32=(($31)|0)!=0;
      if ($32) { __label__ = 8; break; } else { __label__ = 7; break; }
    case 7: 
      $1=0;
      __label__ = 10; break;
    case 8: 
      var $35=$reall;
      var $36=$35;
      var $37=$2;
      var $38=(($37+76)|0);
      var $39=HEAP32[(($38)>>2)];
      var $40=$39;
      var $41=$2;
      var $42=(($41+80)|0);
      var $43=HEAP32[(($42)>>2)];
      var $44=((($43<<2))|0);
      assert($44 % 1 === 0, 'memcpy given ' + $44 + ' bytes to copy. Problem with quantum=1 corrections perhaps?');_memcpy($36, $40, $44, 4);
      var $45=$2;
      var $46=(($45+76)|0);
      var $47=HEAP32[(($46)>>2)];
      var $48=$47;
      _util_memory_d($48, 40, ((STRING_TABLE.__str)|0));
      var $49=$reall;
      var $50=$2;
      var $51=(($50+76)|0);
      HEAP32[(($51)>>2)]=$49;
      __label__ = 9; break;
    case 9: 
      var $53=$3;
      var $54=$2;
      var $55=(($54+80)|0);
      var $56=HEAP32[(($55)>>2)];
      var $57=((($56)+(1))|0);
      HEAP32[(($55)>>2)]=$57;
      var $58=$2;
      var $59=(($58+76)|0);
      var $60=HEAP32[(($59)>>2)];
      var $61=(($60+($56<<2))|0);
      HEAP32[(($61)>>2)]=$53;
      $1=1;
      __label__ = 10; break;
    case 10: 
      var $63=$1;
      ;
      return $63;
    default: assert(0, "bad label: " + __label__);
  }
}
_qc_program_entitydata_add["X"]=1;

function _qc_program_entitypool_remove($self, $idx) {
  ;
  var __label__;
  __label__ = 2; 
  while(1) switch(__label__) {
    case 2: 
      var $1;
      var $2;
      var $3;
      var $i;
      var $reall;
      $2=$self;
      $3=$idx;
      var $4=$3;
      var $5=$2;
      var $6=(($5+92)|0);
      var $7=HEAP32[(($6)>>2)];
      var $8=(($4)>>>0) >= (($7)>>>0);
      if ($8) { __label__ = 3; break; } else { __label__ = 4; break; }
    case 3: 
      $1=1;
      __label__ = 13; break;
    case 4: 
      var $11=$3;
      $i=$11;
      __label__ = 5; break;
    case 5: 
      var $13=$i;
      var $14=$2;
      var $15=(($14+92)|0);
      var $16=HEAP32[(($15)>>2)];
      var $17=((($16)-(1))|0);
      var $18=(($13)>>>0) < (($17)>>>0);
      if ($18) { __label__ = 6; break; } else { __label__ = 8; break; }
    case 6: 
      var $20=$i;
      var $21=((($20)+(1))|0);
      var $22=$2;
      var $23=(($22+88)|0);
      var $24=HEAP32[(($23)>>2)];
      var $25=(($24+$21)|0);
      var $26=HEAP8[($25)];
      var $27=(($26) & 1);
      var $28=$i;
      var $29=$2;
      var $30=(($29+88)|0);
      var $31=HEAP32[(($30)>>2)];
      var $32=(($31+$28)|0);
      var $33=(($27)&1);
      HEAP8[($32)]=$33;
      __label__ = 7; break;
    case 7: 
      var $35=$i;
      var $36=((($35)+(1))|0);
      $i=$36;
      __label__ = 5; break;
    case 8: 
      var $38=$2;
      var $39=(($38+92)|0);
      var $40=HEAP32[(($39)>>2)];
      var $41=((($40)-(1))|0);
      HEAP32[(($39)>>2)]=$41;
      var $42=$2;
      var $43=(($42+92)|0);
      var $44=HEAP32[(($43)>>2)];
      var $45=$2;
      var $46=(($45+92)|0);
      var $47=HEAP32[(($46)>>2)];
      var $48=Math.floor(((($47)>>>0))/(2));
      var $49=(($44)>>>0) < (($48)>>>0);
      if ($49) { __label__ = 9; break; } else { __label__ = 12; break; }
    case 9: 
      var $51=$2;
      var $52=(($51+96)|0);
      var $53=HEAP32[(($52)>>2)];
      var $54=Math.floor(((($53)>>>0))/(2));
      HEAP32[(($52)>>2)]=$54;
      var $55=$2;
      var $56=(($55+92)|0);
      var $57=HEAP32[(($56)>>2)];
      var $58=(($57)|0);
      var $59=_util_memory_a($58, 41, ((STRING_TABLE.__str)|0));
      $reall=$59;
      var $60=$reall;
      var $61=(($60)|0)!=0;
      if ($61) { __label__ = 11; break; } else { __label__ = 10; break; }
    case 10: 
      $1=0;
      __label__ = 13; break;
    case 11: 
      var $64=$reall;
      var $65=$2;
      var $66=(($65+88)|0);
      var $67=HEAP32[(($66)>>2)];
      var $68=$2;
      var $69=(($68+92)|0);
      var $70=HEAP32[(($69)>>2)];
      var $71=(($70)|0);
      assert($71 % 1 === 0, 'memcpy given ' + $71 + ' bytes to copy. Problem with quantum=1 corrections perhaps?');_memcpy($64, $67, $71, 1);
      var $72=$2;
      var $73=(($72+88)|0);
      var $74=HEAP32[(($73)>>2)];
      _util_memory_d($74, 41, ((STRING_TABLE.__str)|0));
      var $75=$reall;
      var $76=$2;
      var $77=(($76+88)|0);
      HEAP32[(($77)>>2)]=$75;
      __label__ = 12; break;
    case 12: 
      $1=1;
      __label__ = 13; break;
    case 13: 
      var $80=$1;
      ;
      return $80;
    default: assert(0, "bad label: " + __label__);
  }
}
_qc_program_entitypool_remove["X"]=1;

function _qc_program_entitypool_add($self, $f) {
  ;
  var __label__;
  __label__ = 2; 
  while(1) switch(__label__) {
    case 2: 
      var $1;
      var $2;
      var $3;
      var $reall;
      $2=$self;
      var $4=(($f)&1);
      $3=$4;
      var $5=$2;
      var $6=(($5+92)|0);
      var $7=HEAP32[(($6)>>2)];
      var $8=$2;
      var $9=(($8+96)|0);
      var $10=HEAP32[(($9)>>2)];
      var $11=(($7)|0)==(($10)|0);
      if ($11) { __label__ = 3; break; } else { __label__ = 9; break; }
    case 3: 
      var $13=$2;
      var $14=(($13+96)|0);
      var $15=HEAP32[(($14)>>2)];
      var $16=(($15)|0)!=0;
      if ($16) { __label__ = 5; break; } else { __label__ = 4; break; }
    case 4: 
      var $18=$2;
      var $19=(($18+96)|0);
      HEAP32[(($19)>>2)]=16;
      __label__ = 6; break;
    case 5: 
      var $21=$2;
      var $22=(($21+96)|0);
      var $23=HEAP32[(($22)>>2)];
      var $24=((($23<<1))|0);
      HEAP32[(($22)>>2)]=$24;
      __label__ = 6; break;
    case 6: 
      var $26=$2;
      var $27=(($26+96)|0);
      var $28=HEAP32[(($27)>>2)];
      var $29=(($28)|0);
      var $30=_util_memory_a($29, 41, ((STRING_TABLE.__str)|0));
      $reall=$30;
      var $31=$reall;
      var $32=(($31)|0)!=0;
      if ($32) { __label__ = 8; break; } else { __label__ = 7; break; }
    case 7: 
      $1=0;
      __label__ = 10; break;
    case 8: 
      var $35=$reall;
      var $36=$2;
      var $37=(($36+88)|0);
      var $38=HEAP32[(($37)>>2)];
      var $39=$2;
      var $40=(($39+92)|0);
      var $41=HEAP32[(($40)>>2)];
      var $42=(($41)|0);
      assert($42 % 1 === 0, 'memcpy given ' + $42 + ' bytes to copy. Problem with quantum=1 corrections perhaps?');_memcpy($35, $38, $42, 1);
      var $43=$2;
      var $44=(($43+88)|0);
      var $45=HEAP32[(($44)>>2)];
      _util_memory_d($45, 41, ((STRING_TABLE.__str)|0));
      var $46=$reall;
      var $47=$2;
      var $48=(($47+88)|0);
      HEAP32[(($48)>>2)]=$46;
      __label__ = 9; break;
    case 9: 
      var $50=$3;
      var $51=(($50) & 1);
      var $52=$2;
      var $53=(($52+92)|0);
      var $54=HEAP32[(($53)>>2)];
      var $55=((($54)+(1))|0);
      HEAP32[(($53)>>2)]=$55;
      var $56=$2;
      var $57=(($56+88)|0);
      var $58=HEAP32[(($57)>>2)];
      var $59=(($58+$54)|0);
      var $60=(($51)&1);
      HEAP8[($59)]=$60;
      $1=1;
      __label__ = 10; break;
    case 10: 
      var $62=$1;
      ;
      return $62;
    default: assert(0, "bad label: " + __label__);
  }
}
_qc_program_entitypool_add["X"]=1;

function _qc_program_localstack_remove($self, $idx) {
  ;
  var __label__;
  __label__ = 2; 
  while(1) switch(__label__) {
    case 2: 
      var $1;
      var $2;
      var $3;
      var $i;
      var $reall;
      $2=$self;
      $3=$idx;
      var $4=$3;
      var $5=$2;
      var $6=(($5+156)|0);
      var $7=HEAP32[(($6)>>2)];
      var $8=(($4)>>>0) >= (($7)>>>0);
      if ($8) { __label__ = 3; break; } else { __label__ = 4; break; }
    case 3: 
      $1=1;
      __label__ = 13; break;
    case 4: 
      var $11=$3;
      $i=$11;
      __label__ = 5; break;
    case 5: 
      var $13=$i;
      var $14=$2;
      var $15=(($14+156)|0);
      var $16=HEAP32[(($15)>>2)];
      var $17=((($16)-(1))|0);
      var $18=(($13)>>>0) < (($17)>>>0);
      if ($18) { __label__ = 6; break; } else { __label__ = 8; break; }
    case 6: 
      var $20=$i;
      var $21=((($20)+(1))|0);
      var $22=$2;
      var $23=(($22+152)|0);
      var $24=HEAP32[(($23)>>2)];
      var $25=(($24+($21<<2))|0);
      var $26=HEAP32[(($25)>>2)];
      var $27=$i;
      var $28=$2;
      var $29=(($28+152)|0);
      var $30=HEAP32[(($29)>>2)];
      var $31=(($30+($27<<2))|0);
      HEAP32[(($31)>>2)]=$26;
      __label__ = 7; break;
    case 7: 
      var $33=$i;
      var $34=((($33)+(1))|0);
      $i=$34;
      __label__ = 5; break;
    case 8: 
      var $36=$2;
      var $37=(($36+156)|0);
      var $38=HEAP32[(($37)>>2)];
      var $39=((($38)-(1))|0);
      HEAP32[(($37)>>2)]=$39;
      var $40=$2;
      var $41=(($40+156)|0);
      var $42=HEAP32[(($41)>>2)];
      var $43=$2;
      var $44=(($43+156)|0);
      var $45=HEAP32[(($44)>>2)];
      var $46=Math.floor(((($45)>>>0))/(2));
      var $47=(($42)>>>0) < (($46)>>>0);
      if ($47) { __label__ = 9; break; } else { __label__ = 12; break; }
    case 9: 
      var $49=$2;
      var $50=(($49+160)|0);
      var $51=HEAP32[(($50)>>2)];
      var $52=Math.floor(((($51)>>>0))/(2));
      HEAP32[(($50)>>2)]=$52;
      var $53=$2;
      var $54=(($53+156)|0);
      var $55=HEAP32[(($54)>>2)];
      var $56=((($55<<2))|0);
      var $57=_util_memory_a($56, 43, ((STRING_TABLE.__str)|0));
      var $58=$57;
      $reall=$58;
      var $59=$reall;
      var $60=(($59)|0)!=0;
      if ($60) { __label__ = 11; break; } else { __label__ = 10; break; }
    case 10: 
      $1=0;
      __label__ = 13; break;
    case 11: 
      var $63=$reall;
      var $64=$63;
      var $65=$2;
      var $66=(($65+152)|0);
      var $67=HEAP32[(($66)>>2)];
      var $68=$67;
      var $69=$2;
      var $70=(($69+156)|0);
      var $71=HEAP32[(($70)>>2)];
      var $72=((($71<<2))|0);
      assert($72 % 1 === 0, 'memcpy given ' + $72 + ' bytes to copy. Problem with quantum=1 corrections perhaps?');_memcpy($64, $68, $72, 4);
      var $73=$2;
      var $74=(($73+152)|0);
      var $75=HEAP32[(($74)>>2)];
      var $76=$75;
      _util_memory_d($76, 43, ((STRING_TABLE.__str)|0));
      var $77=$reall;
      var $78=$2;
      var $79=(($78+152)|0);
      HEAP32[(($79)>>2)]=$77;
      __label__ = 12; break;
    case 12: 
      $1=1;
      __label__ = 13; break;
    case 13: 
      var $82=$1;
      ;
      return $82;
    default: assert(0, "bad label: " + __label__);
  }
}
_qc_program_localstack_remove["X"]=1;

function _qc_program_localstack_add($self, $f) {
  ;
  var __label__;
  __label__ = 2; 
  while(1) switch(__label__) {
    case 2: 
      var $1;
      var $2;
      var $3;
      var $reall;
      $2=$self;
      $3=$f;
      var $4=$2;
      var $5=(($4+156)|0);
      var $6=HEAP32[(($5)>>2)];
      var $7=$2;
      var $8=(($7+160)|0);
      var $9=HEAP32[(($8)>>2)];
      var $10=(($6)|0)==(($9)|0);
      if ($10) { __label__ = 3; break; } else { __label__ = 9; break; }
    case 3: 
      var $12=$2;
      var $13=(($12+160)|0);
      var $14=HEAP32[(($13)>>2)];
      var $15=(($14)|0)!=0;
      if ($15) { __label__ = 5; break; } else { __label__ = 4; break; }
    case 4: 
      var $17=$2;
      var $18=(($17+160)|0);
      HEAP32[(($18)>>2)]=16;
      __label__ = 6; break;
    case 5: 
      var $20=$2;
      var $21=(($20+160)|0);
      var $22=HEAP32[(($21)>>2)];
      var $23=((($22<<1))|0);
      HEAP32[(($21)>>2)]=$23;
      __label__ = 6; break;
    case 6: 
      var $25=$2;
      var $26=(($25+160)|0);
      var $27=HEAP32[(($26)>>2)];
      var $28=((($27<<2))|0);
      var $29=_util_memory_a($28, 43, ((STRING_TABLE.__str)|0));
      var $30=$29;
      $reall=$30;
      var $31=$reall;
      var $32=(($31)|0)!=0;
      if ($32) { __label__ = 8; break; } else { __label__ = 7; break; }
    case 7: 
      $1=0;
      __label__ = 10; break;
    case 8: 
      var $35=$reall;
      var $36=$35;
      var $37=$2;
      var $38=(($37+152)|0);
      var $39=HEAP32[(($38)>>2)];
      var $40=$39;
      var $41=$2;
      var $42=(($41+156)|0);
      var $43=HEAP32[(($42)>>2)];
      var $44=((($43<<2))|0);
      assert($44 % 1 === 0, 'memcpy given ' + $44 + ' bytes to copy. Problem with quantum=1 corrections perhaps?');_memcpy($36, $40, $44, 4);
      var $45=$2;
      var $46=(($45+152)|0);
      var $47=HEAP32[(($46)>>2)];
      var $48=$47;
      _util_memory_d($48, 43, ((STRING_TABLE.__str)|0));
      var $49=$reall;
      var $50=$2;
      var $51=(($50+152)|0);
      HEAP32[(($51)>>2)]=$49;
      __label__ = 9; break;
    case 9: 
      var $53=$3;
      var $54=$2;
      var $55=(($54+156)|0);
      var $56=HEAP32[(($55)>>2)];
      var $57=((($56)+(1))|0);
      HEAP32[(($55)>>2)]=$57;
      var $58=$2;
      var $59=(($58+152)|0);
      var $60=HEAP32[(($59)>>2)];
      var $61=(($60+($56<<2))|0);
      HEAP32[(($61)>>2)]=$53;
      $1=1;
      __label__ = 10; break;
    case 10: 
      var $63=$1;
      ;
      return $63;
    default: assert(0, "bad label: " + __label__);
  }
}
_qc_program_localstack_add["X"]=1;

function _qc_program_localstack_append($s, $p, $c) {
  ;
  var __label__;
  __label__ = 2; 
  while(1) switch(__label__) {
    case 2: 
      var $1;
      var $2;
      var $3;
      var $4;
      var $reall;
      var $oldalloc;
      $2=$s;
      $3=$p;
      $4=$c;
      var $5=$2;
      var $6=(($5+156)|0);
      var $7=HEAP32[(($6)>>2)];
      var $8=$4;
      var $9=((($7)+($8))|0);
      var $10=$2;
      var $11=(($10+160)|0);
      var $12=HEAP32[(($11)>>2)];
      var $13=(($9)>>>0) > (($12)>>>0);
      if ($13) { __label__ = 3; break; } else { __label__ = 14; break; }
    case 3: 
      var $15=$2;
      var $16=(($15+160)|0);
      var $17=HEAP32[(($16)>>2)];
      var $18=(($17)|0)!=0;
      if ($18) { __label__ = 8; break; } else { __label__ = 4; break; }
    case 4: 
      var $20=$4;
      var $21=(($20)>>>0) < 16;
      if ($21) { __label__ = 5; break; } else { __label__ = 6; break; }
    case 5: 
      var $26 = 16;__label__ = 7; break;
    case 6: 
      var $24=$4;
      var $26 = $24;__label__ = 7; break;
    case 7: 
      var $26;
      var $27=$2;
      var $28=(($27+160)|0);
      HEAP32[(($28)>>2)]=$26;
      var $29=$2;
      var $30=(($29+160)|0);
      var $31=HEAP32[(($30)>>2)];
      var $32=((($31<<2))|0);
      var $33=_util_memory_a($32, 44, ((STRING_TABLE.__str)|0));
      var $34=$33;
      var $35=$2;
      var $36=(($35+152)|0);
      HEAP32[(($36)>>2)]=$34;
      __label__ = 13; break;
    case 8: 
      var $38=$2;
      var $39=(($38+160)|0);
      var $40=HEAP32[(($39)>>2)];
      $oldalloc=$40;
      var $41=$2;
      var $42=(($41+160)|0);
      var $43=HEAP32[(($42)>>2)];
      var $44=((($43<<1))|0);
      HEAP32[(($42)>>2)]=$44;
      var $45=$2;
      var $46=(($45+156)|0);
      var $47=HEAP32[(($46)>>2)];
      var $48=$4;
      var $49=((($47)+($48))|0);
      var $50=$2;
      var $51=(($50+160)|0);
      var $52=HEAP32[(($51)>>2)];
      var $53=(($49)>>>0) >= (($52)>>>0);
      if ($53) { __label__ = 9; break; } else { __label__ = 10; break; }
    case 9: 
      var $55=$2;
      var $56=(($55+156)|0);
      var $57=HEAP32[(($56)>>2)];
      var $58=$4;
      var $59=((($57)+($58))|0);
      var $60=$2;
      var $61=(($60+160)|0);
      HEAP32[(($61)>>2)]=$59;
      __label__ = 10; break;
    case 10: 
      var $63=$2;
      var $64=(($63+160)|0);
      var $65=HEAP32[(($64)>>2)];
      var $66=((($65<<2))|0);
      var $67=_util_memory_a($66, 44, ((STRING_TABLE.__str)|0));
      var $68=$67;
      $reall=$68;
      var $69=$reall;
      var $70=(($69)|0)!=0;
      if ($70) { __label__ = 12; break; } else { __label__ = 11; break; }
    case 11: 
      var $72=$oldalloc;
      var $73=$2;
      var $74=(($73+160)|0);
      HEAP32[(($74)>>2)]=$72;
      $1=0;
      __label__ = 15; break;
    case 12: 
      var $76=$reall;
      var $77=$76;
      var $78=$2;
      var $79=(($78+152)|0);
      var $80=HEAP32[(($79)>>2)];
      var $81=$80;
      var $82=$2;
      var $83=(($82+156)|0);
      var $84=HEAP32[(($83)>>2)];
      var $85=((($84<<2))|0);
      assert($85 % 1 === 0, 'memcpy given ' + $85 + ' bytes to copy. Problem with quantum=1 corrections perhaps?');_memcpy($77, $81, $85, 4);
      var $86=$2;
      var $87=(($86+152)|0);
      var $88=HEAP32[(($87)>>2)];
      var $89=$88;
      _util_memory_d($89, 44, ((STRING_TABLE.__str)|0));
      var $90=$reall;
      var $91=$2;
      var $92=(($91+152)|0);
      HEAP32[(($92)>>2)]=$90;
      __label__ = 13; break;
    case 13: 
      __label__ = 14; break;
    case 14: 
      var $95=$2;
      var $96=(($95+156)|0);
      var $97=HEAP32[(($96)>>2)];
      var $98=$2;
      var $99=(($98+152)|0);
      var $100=HEAP32[(($99)>>2)];
      var $101=(($100+($97<<2))|0);
      var $102=$101;
      var $103=$3;
      var $104=$103;
      var $105=$4;
      var $106=((($105<<2))|0);
      assert($106 % 1 === 0, 'memcpy given ' + $106 + ' bytes to copy. Problem with quantum=1 corrections perhaps?');_memcpy($102, $104, $106, 4);
      var $107=$4;
      var $108=$2;
      var $109=(($108+156)|0);
      var $110=HEAP32[(($109)>>2)];
      var $111=((($110)+($107))|0);
      HEAP32[(($109)>>2)]=$111;
      $1=1;
      __label__ = 15; break;
    case 15: 
      var $113=$1;
      ;
      return $113;
    default: assert(0, "bad label: " + __label__);
  }
}
_qc_program_localstack_append["X"]=1;

function _qc_program_localstack_resize($s, $c) {
  ;
  var __label__;
  __label__ = 2; 
  while(1) switch(__label__) {
    case 2: 
      var $1;
      var $2;
      var $3;
      var $reall;
      $2=$s;
      $3=$c;
      var $4=$3;
      var $5=$2;
      var $6=(($5+160)|0);
      var $7=HEAP32[(($6)>>2)];
      var $8=(($4)>>>0) > (($7)>>>0);
      if ($8) { __label__ = 3; break; } else { __label__ = 6; break; }
    case 3: 
      var $10=$3;
      var $11=((($10<<2))|0);
      var $12=_util_memory_a($11, 45, ((STRING_TABLE.__str)|0));
      var $13=$12;
      $reall=$13;
      var $14=$reall;
      var $15=(($14)|0)!=0;
      if ($15) { __label__ = 5; break; } else { __label__ = 4; break; }
    case 4: 
      $1=0;
      __label__ = 11; break;
    case 5: 
      var $18=$reall;
      var $19=$18;
      var $20=$2;
      var $21=(($20+152)|0);
      var $22=HEAP32[(($21)>>2)];
      var $23=$22;
      var $24=$2;
      var $25=(($24+156)|0);
      var $26=HEAP32[(($25)>>2)];
      var $27=((($26<<2))|0);
      assert($27 % 1 === 0, 'memcpy given ' + $27 + ' bytes to copy. Problem with quantum=1 corrections perhaps?');_memcpy($19, $23, $27, 4);
      var $28=$3;
      var $29=$2;
      var $30=(($29+160)|0);
      HEAP32[(($30)>>2)]=$28;
      var $31=$3;
      var $32=$2;
      var $33=(($32+156)|0);
      HEAP32[(($33)>>2)]=$31;
      var $34=$2;
      var $35=(($34+152)|0);
      var $36=HEAP32[(($35)>>2)];
      var $37=$36;
      _util_memory_d($37, 45, ((STRING_TABLE.__str)|0));
      var $38=$reall;
      var $39=$2;
      var $40=(($39+152)|0);
      HEAP32[(($40)>>2)]=$38;
      $1=1;
      __label__ = 11; break;
    case 6: 
      var $42=$3;
      var $43=$2;
      var $44=(($43+156)|0);
      HEAP32[(($44)>>2)]=$42;
      var $45=$3;
      var $46=$2;
      var $47=(($46+160)|0);
      var $48=HEAP32[(($47)>>2)];
      var $49=Math.floor(((($48)>>>0))/(2));
      var $50=(($45)>>>0) < (($49)>>>0);
      if ($50) { __label__ = 7; break; } else { __label__ = 10; break; }
    case 7: 
      var $52=$3;
      var $53=((($52<<2))|0);
      var $54=_util_memory_a($53, 45, ((STRING_TABLE.__str)|0));
      var $55=$54;
      $reall=$55;
      var $56=$reall;
      var $57=(($56)|0)!=0;
      if ($57) { __label__ = 9; break; } else { __label__ = 8; break; }
    case 8: 
      $1=0;
      __label__ = 11; break;
    case 9: 
      var $60=$reall;
      var $61=$60;
      var $62=$2;
      var $63=(($62+152)|0);
      var $64=HEAP32[(($63)>>2)];
      var $65=$64;
      var $66=$3;
      var $67=((($66<<2))|0);
      assert($67 % 1 === 0, 'memcpy given ' + $67 + ' bytes to copy. Problem with quantum=1 corrections perhaps?');_memcpy($61, $65, $67, 4);
      var $68=$2;
      var $69=(($68+152)|0);
      var $70=HEAP32[(($69)>>2)];
      var $71=$70;
      _util_memory_d($71, 45, ((STRING_TABLE.__str)|0));
      var $72=$reall;
      var $73=$2;
      var $74=(($73+152)|0);
      HEAP32[(($74)>>2)]=$72;
      var $75=$3;
      var $76=$2;
      var $77=(($76+160)|0);
      HEAP32[(($77)>>2)]=$75;
      __label__ = 10; break;
    case 10: 
      $1=1;
      __label__ = 11; break;
    case 11: 
      var $80=$1;
      ;
      return $80;
    default: assert(0, "bad label: " + __label__);
  }
}
_qc_program_localstack_resize["X"]=1;

function _qc_program_stack_remove($self, $idx) {
  ;
  var __label__;
  __label__ = 2; 
  while(1) switch(__label__) {
    case 2: 
      var $1;
      var $2;
      var $3;
      var $i;
      var $reall;
      $2=$self;
      $3=$idx;
      var $4=$3;
      var $5=$2;
      var $6=(($5+168)|0);
      var $7=HEAP32[(($6)>>2)];
      var $8=(($4)>>>0) >= (($7)>>>0);
      if ($8) { __label__ = 3; break; } else { __label__ = 4; break; }
    case 3: 
      $1=1;
      __label__ = 13; break;
    case 4: 
      var $11=$3;
      $i=$11;
      __label__ = 5; break;
    case 5: 
      var $13=$i;
      var $14=$2;
      var $15=(($14+168)|0);
      var $16=HEAP32[(($15)>>2)];
      var $17=((($16)-(1))|0);
      var $18=(($13)>>>0) < (($17)>>>0);
      if ($18) { __label__ = 6; break; } else { __label__ = 8; break; }
    case 6: 
      var $20=$i;
      var $21=$2;
      var $22=(($21+164)|0);
      var $23=HEAP32[(($22)>>2)];
      var $24=(($23+($20)*(12))|0);
      var $25=$i;
      var $26=((($25)+(1))|0);
      var $27=$2;
      var $28=(($27+164)|0);
      var $29=HEAP32[(($28)>>2)];
      var $30=(($29+($26)*(12))|0);
      var $31=$24;
      var $32=$30;
      assert(12 % 1 === 0, 'memcpy given ' + 12 + ' bytes to copy. Problem with quantum=1 corrections perhaps?');HEAP32[(($31)>>2)]=HEAP32[(($32)>>2)];HEAP32[((($31)+(4))>>2)]=HEAP32[((($32)+(4))>>2)];HEAP32[((($31)+(8))>>2)]=HEAP32[((($32)+(8))>>2)];
      __label__ = 7; break;
    case 7: 
      var $34=$i;
      var $35=((($34)+(1))|0);
      $i=$35;
      __label__ = 5; break;
    case 8: 
      var $37=$2;
      var $38=(($37+168)|0);
      var $39=HEAP32[(($38)>>2)];
      var $40=((($39)-(1))|0);
      HEAP32[(($38)>>2)]=$40;
      var $41=$2;
      var $42=(($41+168)|0);
      var $43=HEAP32[(($42)>>2)];
      var $44=$2;
      var $45=(($44+168)|0);
      var $46=HEAP32[(($45)>>2)];
      var $47=Math.floor(((($46)>>>0))/(2));
      var $48=(($43)>>>0) < (($47)>>>0);
      if ($48) { __label__ = 9; break; } else { __label__ = 12; break; }
    case 9: 
      var $50=$2;
      var $51=(($50+172)|0);
      var $52=HEAP32[(($51)>>2)];
      var $53=Math.floor(((($52)>>>0))/(2));
      HEAP32[(($51)>>2)]=$53;
      var $54=$2;
      var $55=(($54+168)|0);
      var $56=HEAP32[(($55)>>2)];
      var $57=((($56)*(12))|0);
      var $58=_util_memory_a($57, 46, ((STRING_TABLE.__str)|0));
      var $59=$58;
      $reall=$59;
      var $60=$reall;
      var $61=(($60)|0)!=0;
      if ($61) { __label__ = 11; break; } else { __label__ = 10; break; }
    case 10: 
      $1=0;
      __label__ = 13; break;
    case 11: 
      var $64=$reall;
      var $65=$64;
      var $66=$2;
      var $67=(($66+164)|0);
      var $68=HEAP32[(($67)>>2)];
      var $69=$68;
      var $70=$2;
      var $71=(($70+168)|0);
      var $72=HEAP32[(($71)>>2)];
      var $73=((($72)*(12))|0);
      assert($73 % 1 === 0, 'memcpy given ' + $73 + ' bytes to copy. Problem with quantum=1 corrections perhaps?');_memcpy($65, $69, $73, 4);
      var $74=$2;
      var $75=(($74+164)|0);
      var $76=HEAP32[(($75)>>2)];
      var $77=$76;
      _util_memory_d($77, 46, ((STRING_TABLE.__str)|0));
      var $78=$reall;
      var $79=$2;
      var $80=(($79+164)|0);
      HEAP32[(($80)>>2)]=$78;
      __label__ = 12; break;
    case 12: 
      $1=1;
      __label__ = 13; break;
    case 13: 
      var $83=$1;
      ;
      return $83;
    default: assert(0, "bad label: " + __label__);
  }
}
_qc_program_stack_remove["X"]=1;

function _qc_program_stack_add($self, $f_0, $f_1, $f_2) {
  var __stackBase__  = STACKTOP; STACKTOP += 12; assert(STACKTOP % 4 == 0, "Stack is unaligned"); assert(STACKTOP < STACK_MAX, "Ran out of stack");
  var __label__;
  __label__ = 2; 
  while(1) switch(__label__) {
    case 2: 
      var $1;
      var $2;
      var $f=__stackBase__;
      var $reall;
      $2=$self;
      var $3=(($f)|0);
      HEAP32[(($3)>>2)]=$f_0;
      var $4=(($f+4)|0);
      HEAP32[(($4)>>2)]=$f_1;
      var $5=(($f+8)|0);
      HEAP32[(($5)>>2)]=$f_2;
      var $6=$2;
      var $7=(($6+168)|0);
      var $8=HEAP32[(($7)>>2)];
      var $9=$2;
      var $10=(($9+172)|0);
      var $11=HEAP32[(($10)>>2)];
      var $12=(($8)|0)==(($11)|0);
      if ($12) { __label__ = 3; break; } else { __label__ = 9; break; }
    case 3: 
      var $14=$2;
      var $15=(($14+172)|0);
      var $16=HEAP32[(($15)>>2)];
      var $17=(($16)|0)!=0;
      if ($17) { __label__ = 5; break; } else { __label__ = 4; break; }
    case 4: 
      var $19=$2;
      var $20=(($19+172)|0);
      HEAP32[(($20)>>2)]=16;
      __label__ = 6; break;
    case 5: 
      var $22=$2;
      var $23=(($22+172)|0);
      var $24=HEAP32[(($23)>>2)];
      var $25=((($24<<1))|0);
      HEAP32[(($23)>>2)]=$25;
      __label__ = 6; break;
    case 6: 
      var $27=$2;
      var $28=(($27+172)|0);
      var $29=HEAP32[(($28)>>2)];
      var $30=((($29)*(12))|0);
      var $31=_util_memory_a($30, 46, ((STRING_TABLE.__str)|0));
      var $32=$31;
      $reall=$32;
      var $33=$reall;
      var $34=(($33)|0)!=0;
      if ($34) { __label__ = 8; break; } else { __label__ = 7; break; }
    case 7: 
      $1=0;
      __label__ = 10; break;
    case 8: 
      var $37=$reall;
      var $38=$37;
      var $39=$2;
      var $40=(($39+164)|0);
      var $41=HEAP32[(($40)>>2)];
      var $42=$41;
      var $43=$2;
      var $44=(($43+168)|0);
      var $45=HEAP32[(($44)>>2)];
      var $46=((($45)*(12))|0);
      assert($46 % 1 === 0, 'memcpy given ' + $46 + ' bytes to copy. Problem with quantum=1 corrections perhaps?');_memcpy($38, $42, $46, 4);
      var $47=$2;
      var $48=(($47+164)|0);
      var $49=HEAP32[(($48)>>2)];
      var $50=$49;
      _util_memory_d($50, 46, ((STRING_TABLE.__str)|0));
      var $51=$reall;
      var $52=$2;
      var $53=(($52+164)|0);
      HEAP32[(($53)>>2)]=$51;
      __label__ = 9; break;
    case 9: 
      var $55=$2;
      var $56=(($55+168)|0);
      var $57=HEAP32[(($56)>>2)];
      var $58=((($57)+(1))|0);
      HEAP32[(($56)>>2)]=$58;
      var $59=$2;
      var $60=(($59+164)|0);
      var $61=HEAP32[(($60)>>2)];
      var $62=(($61+($57)*(12))|0);
      var $63=$62;
      var $64=$f;
      assert(12 % 1 === 0, 'memcpy given ' + 12 + ' bytes to copy. Problem with quantum=1 corrections perhaps?');HEAP32[(($63)>>2)]=HEAP32[(($64)>>2)];HEAP32[((($63)+(4))>>2)]=HEAP32[((($64)+(4))>>2)];HEAP32[((($63)+(8))>>2)]=HEAP32[((($64)+(8))>>2)];
      $1=1;
      __label__ = 10; break;
    case 10: 
      var $66=$1;
      STACKTOP = __stackBase__;
      return $66;
    default: assert(0, "bad label: " + __label__);
  }
}
_qc_program_stack_add["X"]=1;

function _qc_program_profile_remove($self, $idx) {
  ;
  var __label__;
  __label__ = 2; 
  while(1) switch(__label__) {
    case 2: 
      var $1;
      var $2;
      var $3;
      var $i;
      var $reall;
      $2=$self;
      $3=$idx;
      var $4=$3;
      var $5=$2;
      var $6=(($5+120)|0);
      var $7=HEAP32[(($6)>>2)];
      var $8=(($4)>>>0) >= (($7)>>>0);
      if ($8) { __label__ = 3; break; } else { __label__ = 4; break; }
    case 3: 
      $1=1;
      __label__ = 13; break;
    case 4: 
      var $11=$3;
      $i=$11;
      __label__ = 5; break;
    case 5: 
      var $13=$i;
      var $14=$2;
      var $15=(($14+120)|0);
      var $16=HEAP32[(($15)>>2)];
      var $17=((($16)-(1))|0);
      var $18=(($13)>>>0) < (($17)>>>0);
      if ($18) { __label__ = 6; break; } else { __label__ = 8; break; }
    case 6: 
      var $20=$i;
      var $21=((($20)+(1))|0);
      var $22=$2;
      var $23=(($22+116)|0);
      var $24=HEAP32[(($23)>>2)];
      var $25=(($24+($21<<2))|0);
      var $26=HEAP32[(($25)>>2)];
      var $27=$i;
      var $28=$2;
      var $29=(($28+116)|0);
      var $30=HEAP32[(($29)>>2)];
      var $31=(($30+($27<<2))|0);
      HEAP32[(($31)>>2)]=$26;
      __label__ = 7; break;
    case 7: 
      var $33=$i;
      var $34=((($33)+(1))|0);
      $i=$34;
      __label__ = 5; break;
    case 8: 
      var $36=$2;
      var $37=(($36+120)|0);
      var $38=HEAP32[(($37)>>2)];
      var $39=((($38)-(1))|0);
      HEAP32[(($37)>>2)]=$39;
      var $40=$2;
      var $41=(($40+120)|0);
      var $42=HEAP32[(($41)>>2)];
      var $43=$2;
      var $44=(($43+120)|0);
      var $45=HEAP32[(($44)>>2)];
      var $46=Math.floor(((($45)>>>0))/(2));
      var $47=(($42)>>>0) < (($46)>>>0);
      if ($47) { __label__ = 9; break; } else { __label__ = 12; break; }
    case 9: 
      var $49=$2;
      var $50=(($49+124)|0);
      var $51=HEAP32[(($50)>>2)];
      var $52=Math.floor(((($51)>>>0))/(2));
      HEAP32[(($50)>>2)]=$52;
      var $53=$2;
      var $54=(($53+120)|0);
      var $55=HEAP32[(($54)>>2)];
      var $56=((($55<<2))|0);
      var $57=_util_memory_a($56, 48, ((STRING_TABLE.__str)|0));
      var $58=$57;
      $reall=$58;
      var $59=$reall;
      var $60=(($59)|0)!=0;
      if ($60) { __label__ = 11; break; } else { __label__ = 10; break; }
    case 10: 
      $1=0;
      __label__ = 13; break;
    case 11: 
      var $63=$reall;
      var $64=$63;
      var $65=$2;
      var $66=(($65+116)|0);
      var $67=HEAP32[(($66)>>2)];
      var $68=$67;
      var $69=$2;
      var $70=(($69+120)|0);
      var $71=HEAP32[(($70)>>2)];
      var $72=((($71<<2))|0);
      assert($72 % 1 === 0, 'memcpy given ' + $72 + ' bytes to copy. Problem with quantum=1 corrections perhaps?');_memcpy($64, $68, $72, 4);
      var $73=$2;
      var $74=(($73+116)|0);
      var $75=HEAP32[(($74)>>2)];
      var $76=$75;
      _util_memory_d($76, 48, ((STRING_TABLE.__str)|0));
      var $77=$reall;
      var $78=$2;
      var $79=(($78+116)|0);
      HEAP32[(($79)>>2)]=$77;
      __label__ = 12; break;
    case 12: 
      $1=1;
      __label__ = 13; break;
    case 13: 
      var $82=$1;
      ;
      return $82;
    default: assert(0, "bad label: " + __label__);
  }
}
_qc_program_profile_remove["X"]=1;

function _qc_program_profile_add($self, $f) {
  ;
  var __label__;
  __label__ = 2; 
  while(1) switch(__label__) {
    case 2: 
      var $1;
      var $2;
      var $3;
      var $reall;
      $2=$self;
      $3=$f;
      var $4=$2;
      var $5=(($4+120)|0);
      var $6=HEAP32[(($5)>>2)];
      var $7=$2;
      var $8=(($7+124)|0);
      var $9=HEAP32[(($8)>>2)];
      var $10=(($6)|0)==(($9)|0);
      if ($10) { __label__ = 3; break; } else { __label__ = 9; break; }
    case 3: 
      var $12=$2;
      var $13=(($12+124)|0);
      var $14=HEAP32[(($13)>>2)];
      var $15=(($14)|0)!=0;
      if ($15) { __label__ = 5; break; } else { __label__ = 4; break; }
    case 4: 
      var $17=$2;
      var $18=(($17+124)|0);
      HEAP32[(($18)>>2)]=16;
      __label__ = 6; break;
    case 5: 
      var $20=$2;
      var $21=(($20+124)|0);
      var $22=HEAP32[(($21)>>2)];
      var $23=((($22<<1))|0);
      HEAP32[(($21)>>2)]=$23;
      __label__ = 6; break;
    case 6: 
      var $25=$2;
      var $26=(($25+124)|0);
      var $27=HEAP32[(($26)>>2)];
      var $28=((($27<<2))|0);
      var $29=_util_memory_a($28, 48, ((STRING_TABLE.__str)|0));
      var $30=$29;
      $reall=$30;
      var $31=$reall;
      var $32=(($31)|0)!=0;
      if ($32) { __label__ = 8; break; } else { __label__ = 7; break; }
    case 7: 
      $1=0;
      __label__ = 10; break;
    case 8: 
      var $35=$reall;
      var $36=$35;
      var $37=$2;
      var $38=(($37+116)|0);
      var $39=HEAP32[(($38)>>2)];
      var $40=$39;
      var $41=$2;
      var $42=(($41+120)|0);
      var $43=HEAP32[(($42)>>2)];
      var $44=((($43<<2))|0);
      assert($44 % 1 === 0, 'memcpy given ' + $44 + ' bytes to copy. Problem with quantum=1 corrections perhaps?');_memcpy($36, $40, $44, 4);
      var $45=$2;
      var $46=(($45+116)|0);
      var $47=HEAP32[(($46)>>2)];
      var $48=$47;
      _util_memory_d($48, 48, ((STRING_TABLE.__str)|0));
      var $49=$reall;
      var $50=$2;
      var $51=(($50+116)|0);
      HEAP32[(($51)>>2)]=$49;
      __label__ = 9; break;
    case 9: 
      var $53=$3;
      var $54=$2;
      var $55=(($54+120)|0);
      var $56=HEAP32[(($55)>>2)];
      var $57=((($56)+(1))|0);
      HEAP32[(($55)>>2)]=$57;
      var $58=$2;
      var $59=(($58+116)|0);
      var $60=HEAP32[(($59)>>2)];
      var $61=(($60+($56<<2))|0);
      HEAP32[(($61)>>2)]=$53;
      $1=1;
      __label__ = 10; break;
    case 10: 
      var $63=$1;
      ;
      return $63;
    default: assert(0, "bad label: " + __label__);
  }
}
_qc_program_profile_add["X"]=1;

function _qc_program_profile_resize($s, $c) {
  ;
  var __label__;
  __label__ = 2; 
  while(1) switch(__label__) {
    case 2: 
      var $1;
      var $2;
      var $3;
      var $reall;
      $2=$s;
      $3=$c;
      var $4=$3;
      var $5=$2;
      var $6=(($5+124)|0);
      var $7=HEAP32[(($6)>>2)];
      var $8=(($4)>>>0) > (($7)>>>0);
      if ($8) { __label__ = 3; break; } else { __label__ = 6; break; }
    case 3: 
      var $10=$3;
      var $11=((($10<<2))|0);
      var $12=_util_memory_a($11, 49, ((STRING_TABLE.__str)|0));
      var $13=$12;
      $reall=$13;
      var $14=$reall;
      var $15=(($14)|0)!=0;
      if ($15) { __label__ = 5; break; } else { __label__ = 4; break; }
    case 4: 
      $1=0;
      __label__ = 11; break;
    case 5: 
      var $18=$reall;
      var $19=$18;
      var $20=$2;
      var $21=(($20+116)|0);
      var $22=HEAP32[(($21)>>2)];
      var $23=$22;
      var $24=$2;
      var $25=(($24+120)|0);
      var $26=HEAP32[(($25)>>2)];
      var $27=((($26<<2))|0);
      assert($27 % 1 === 0, 'memcpy given ' + $27 + ' bytes to copy. Problem with quantum=1 corrections perhaps?');_memcpy($19, $23, $27, 4);
      var $28=$3;
      var $29=$2;
      var $30=(($29+124)|0);
      HEAP32[(($30)>>2)]=$28;
      var $31=$3;
      var $32=$2;
      var $33=(($32+120)|0);
      HEAP32[(($33)>>2)]=$31;
      var $34=$2;
      var $35=(($34+116)|0);
      var $36=HEAP32[(($35)>>2)];
      var $37=$36;
      _util_memory_d($37, 49, ((STRING_TABLE.__str)|0));
      var $38=$reall;
      var $39=$2;
      var $40=(($39+116)|0);
      HEAP32[(($40)>>2)]=$38;
      $1=1;
      __label__ = 11; break;
    case 6: 
      var $42=$3;
      var $43=$2;
      var $44=(($43+120)|0);
      HEAP32[(($44)>>2)]=$42;
      var $45=$3;
      var $46=$2;
      var $47=(($46+124)|0);
      var $48=HEAP32[(($47)>>2)];
      var $49=Math.floor(((($48)>>>0))/(2));
      var $50=(($45)>>>0) < (($49)>>>0);
      if ($50) { __label__ = 7; break; } else { __label__ = 10; break; }
    case 7: 
      var $52=$3;
      var $53=((($52<<2))|0);
      var $54=_util_memory_a($53, 49, ((STRING_TABLE.__str)|0));
      var $55=$54;
      $reall=$55;
      var $56=$reall;
      var $57=(($56)|0)!=0;
      if ($57) { __label__ = 9; break; } else { __label__ = 8; break; }
    case 8: 
      $1=0;
      __label__ = 11; break;
    case 9: 
      var $60=$reall;
      var $61=$60;
      var $62=$2;
      var $63=(($62+116)|0);
      var $64=HEAP32[(($63)>>2)];
      var $65=$64;
      var $66=$3;
      var $67=((($66<<2))|0);
      assert($67 % 1 === 0, 'memcpy given ' + $67 + ' bytes to copy. Problem with quantum=1 corrections perhaps?');_memcpy($61, $65, $67, 4);
      var $68=$2;
      var $69=(($68+116)|0);
      var $70=HEAP32[(($69)>>2)];
      var $71=$70;
      _util_memory_d($71, 49, ((STRING_TABLE.__str)|0));
      var $72=$reall;
      var $73=$2;
      var $74=(($73+116)|0);
      HEAP32[(($74)>>2)]=$72;
      var $75=$3;
      var $76=$2;
      var $77=(($76+124)|0);
      HEAP32[(($77)>>2)]=$75;
      __label__ = 10; break;
    case 10: 
      $1=1;
      __label__ = 11; break;
    case 11: 
      var $80=$1;
      ;
      return $80;
    default: assert(0, "bad label: " + __label__);
  }
}
_qc_program_profile_resize["X"]=1;

function _qc_program_builtins_remove($self, $idx) {
  ;
  var __label__;
  __label__ = 2; 
  while(1) switch(__label__) {
    case 2: 
      var $1;
      var $2;
      var $3;
      var $i;
      var $reall;
      $2=$self;
      $3=$idx;
      var $4=$3;
      var $5=$2;
      var $6=(($5+132)|0);
      var $7=HEAP32[(($6)>>2)];
      var $8=(($4)>>>0) >= (($7)>>>0);
      if ($8) { __label__ = 3; break; } else { __label__ = 4; break; }
    case 3: 
      $1=1;
      __label__ = 13; break;
    case 4: 
      var $11=$3;
      $i=$11;
      __label__ = 5; break;
    case 5: 
      var $13=$i;
      var $14=$2;
      var $15=(($14+132)|0);
      var $16=HEAP32[(($15)>>2)];
      var $17=((($16)-(1))|0);
      var $18=(($13)>>>0) < (($17)>>>0);
      if ($18) { __label__ = 6; break; } else { __label__ = 8; break; }
    case 6: 
      var $20=$i;
      var $21=((($20)+(1))|0);
      var $22=$2;
      var $23=(($22+128)|0);
      var $24=HEAP32[(($23)>>2)];
      var $25=(($24+($21<<2))|0);
      var $26=HEAP32[(($25)>>2)];
      var $27=$i;
      var $28=$2;
      var $29=(($28+128)|0);
      var $30=HEAP32[(($29)>>2)];
      var $31=(($30+($27<<2))|0);
      HEAP32[(($31)>>2)]=$26;
      __label__ = 7; break;
    case 7: 
      var $33=$i;
      var $34=((($33)+(1))|0);
      $i=$34;
      __label__ = 5; break;
    case 8: 
      var $36=$2;
      var $37=(($36+132)|0);
      var $38=HEAP32[(($37)>>2)];
      var $39=((($38)-(1))|0);
      HEAP32[(($37)>>2)]=$39;
      var $40=$2;
      var $41=(($40+132)|0);
      var $42=HEAP32[(($41)>>2)];
      var $43=$2;
      var $44=(($43+132)|0);
      var $45=HEAP32[(($44)>>2)];
      var $46=Math.floor(((($45)>>>0))/(2));
      var $47=(($42)>>>0) < (($46)>>>0);
      if ($47) { __label__ = 9; break; } else { __label__ = 12; break; }
    case 9: 
      var $49=$2;
      var $50=(($49+136)|0);
      var $51=HEAP32[(($50)>>2)];
      var $52=Math.floor(((($51)>>>0))/(2));
      HEAP32[(($50)>>2)]=$52;
      var $53=$2;
      var $54=(($53+132)|0);
      var $55=HEAP32[(($54)>>2)];
      var $56=((($55<<2))|0);
      var $57=_util_memory_a($56, 51, ((STRING_TABLE.__str)|0));
      var $58=$57;
      $reall=$58;
      var $59=$reall;
      var $60=(($59)|0)!=0;
      if ($60) { __label__ = 11; break; } else { __label__ = 10; break; }
    case 10: 
      $1=0;
      __label__ = 13; break;
    case 11: 
      var $63=$reall;
      var $64=$63;
      var $65=$2;
      var $66=(($65+128)|0);
      var $67=HEAP32[(($66)>>2)];
      var $68=$67;
      var $69=$2;
      var $70=(($69+132)|0);
      var $71=HEAP32[(($70)>>2)];
      var $72=((($71<<2))|0);
      assert($72 % 1 === 0, 'memcpy given ' + $72 + ' bytes to copy. Problem with quantum=1 corrections perhaps?');_memcpy($64, $68, $72, 4);
      var $73=$2;
      var $74=(($73+128)|0);
      var $75=HEAP32[(($74)>>2)];
      var $76=$75;
      _util_memory_d($76, 51, ((STRING_TABLE.__str)|0));
      var $77=$reall;
      var $78=$2;
      var $79=(($78+128)|0);
      HEAP32[(($79)>>2)]=$77;
      __label__ = 12; break;
    case 12: 
      $1=1;
      __label__ = 13; break;
    case 13: 
      var $82=$1;
      ;
      return $82;
    default: assert(0, "bad label: " + __label__);
  }
}
_qc_program_builtins_remove["X"]=1;

function _qc_program_builtins_add($self, $f) {
  ;
  var __label__;
  __label__ = 2; 
  while(1) switch(__label__) {
    case 2: 
      var $1;
      var $2;
      var $3;
      var $reall;
      $2=$self;
      $3=$f;
      var $4=$2;
      var $5=(($4+132)|0);
      var $6=HEAP32[(($5)>>2)];
      var $7=$2;
      var $8=(($7+136)|0);
      var $9=HEAP32[(($8)>>2)];
      var $10=(($6)|0)==(($9)|0);
      if ($10) { __label__ = 3; break; } else { __label__ = 9; break; }
    case 3: 
      var $12=$2;
      var $13=(($12+136)|0);
      var $14=HEAP32[(($13)>>2)];
      var $15=(($14)|0)!=0;
      if ($15) { __label__ = 5; break; } else { __label__ = 4; break; }
    case 4: 
      var $17=$2;
      var $18=(($17+136)|0);
      HEAP32[(($18)>>2)]=16;
      __label__ = 6; break;
    case 5: 
      var $20=$2;
      var $21=(($20+136)|0);
      var $22=HEAP32[(($21)>>2)];
      var $23=((($22<<1))|0);
      HEAP32[(($21)>>2)]=$23;
      __label__ = 6; break;
    case 6: 
      var $25=$2;
      var $26=(($25+136)|0);
      var $27=HEAP32[(($26)>>2)];
      var $28=((($27<<2))|0);
      var $29=_util_memory_a($28, 51, ((STRING_TABLE.__str)|0));
      var $30=$29;
      $reall=$30;
      var $31=$reall;
      var $32=(($31)|0)!=0;
      if ($32) { __label__ = 8; break; } else { __label__ = 7; break; }
    case 7: 
      $1=0;
      __label__ = 10; break;
    case 8: 
      var $35=$reall;
      var $36=$35;
      var $37=$2;
      var $38=(($37+128)|0);
      var $39=HEAP32[(($38)>>2)];
      var $40=$39;
      var $41=$2;
      var $42=(($41+132)|0);
      var $43=HEAP32[(($42)>>2)];
      var $44=((($43<<2))|0);
      assert($44 % 1 === 0, 'memcpy given ' + $44 + ' bytes to copy. Problem with quantum=1 corrections perhaps?');_memcpy($36, $40, $44, 4);
      var $45=$2;
      var $46=(($45+128)|0);
      var $47=HEAP32[(($46)>>2)];
      var $48=$47;
      _util_memory_d($48, 51, ((STRING_TABLE.__str)|0));
      var $49=$reall;
      var $50=$2;
      var $51=(($50+128)|0);
      HEAP32[(($51)>>2)]=$49;
      __label__ = 9; break;
    case 9: 
      var $53=$3;
      var $54=$2;
      var $55=(($54+132)|0);
      var $56=HEAP32[(($55)>>2)];
      var $57=((($56)+(1))|0);
      HEAP32[(($55)>>2)]=$57;
      var $58=$2;
      var $59=(($58+128)|0);
      var $60=HEAP32[(($59)>>2)];
      var $61=(($60+($56<<2))|0);
      HEAP32[(($61)>>2)]=$53;
      $1=1;
      __label__ = 10; break;
    case 10: 
      var $63=$1;
      ;
      return $63;
    default: assert(0, "bad label: " + __label__);
  }
}
_qc_program_builtins_add["X"]=1;

function _prog_load($filename) {
  var __stackBase__  = STACKTOP; STACKTOP += 60; assert(STACKTOP % 4 == 0, "Stack is unaligned"); assert(STACKTOP < STACK_MAX, "Ran out of stack");
  var __label__;
  __label__ = 2; 
  while(1) switch(__label__) {
    case 2: 
      var $1;
      var $2;
      var $prog;
      var $header=__stackBase__;
      var $i;
      var $file;
      $2=$filename;
      var $3=$2;
      var $4=_util_fopen($3, ((STRING_TABLE.__str1)|0));
      $file=$4;
      var $5=$file;
      var $6=(($5)|0)!=0;
      if ($6) { __label__ = 4; break; } else { __label__ = 3; break; }
    case 3: 
      $1=0;
      __label__ = 80; break;
    case 4: 
      var $9=$header;
      var $10=$file;
      var $11=_fread($9, 60, 1, $10);
      var $12=(($11)|0)!=1;
      if ($12) { __label__ = 5; break; } else { __label__ = 6; break; }
    case 5: 
      var $14=$2;
      _loaderror(((STRING_TABLE.__str2)|0), (tempInt=STACKTOP,STACKTOP += 4,assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),HEAP32[((tempInt)>>2)]=$14,tempInt));
      var $15=$file;
      var $16=_fclose($15);
      $1=0;
      __label__ = 80; break;
    case 6: 
      var $18=(($header)|0);
      var $19=HEAP32[(($18)>>2)];
      var $20=(($19)|0)!=6;
      if ($20) { __label__ = 7; break; } else { __label__ = 8; break; }
    case 7: 
      var $22=(($header)|0);
      var $23=HEAP32[(($22)>>2)];
      _loaderror(((STRING_TABLE.__str3)|0), (tempInt=STACKTOP,STACKTOP += 4,assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),HEAP32[((tempInt)>>2)]=$23,tempInt));
      var $24=$file;
      var $25=_fclose($24);
      $1=0;
      __label__ = 80; break;
    case 8: 
      var $27=_util_memory_a(188, 98, ((STRING_TABLE.__str)|0));
      var $28=$27;
      $prog=$28;
      var $29=$prog;
      var $30=(($29)|0)!=0;
      if ($30) { __label__ = 10; break; } else { __label__ = 9; break; }
    case 9: 
      var $32=$file;
      var $33=_fclose($32);
      var $34=_printf(((STRING_TABLE.__str4)|0), (tempInt=STACKTOP,STACKTOP += 1,STACKTOP = ((((STACKTOP)+3)>>2)<<2),assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),HEAP32[((tempInt)>>2)]=0,tempInt));
      $1=0;
      __label__ = 80; break;
    case 10: 
      var $36=$prog;
      var $37=$36;
      for (var $$dest = $37>>2, $$stop = $$dest + 47; $$dest < $$stop; $$dest++) {
        HEAP32[$$dest] = 0
      };
      var $38=(($header+56)|0);
      var $39=HEAP32[(($38)>>2)];
      var $40=$prog;
      var $41=(($40+144)|0);
      HEAP32[(($41)>>2)]=$39;
      var $42=(($header+4)|0);
      var $43=HEAP16[(($42)>>1)];
      var $44=$prog;
      var $45=(($44+100)|0);
      HEAP16[(($45)>>1)]=$43;
      var $46=$2;
      var $47=_util_strdup($46);
      var $48=$prog;
      var $49=(($48)|0);
      HEAP32[(($49)>>2)]=$47;
      var $50=$prog;
      var $51=(($50)|0);
      var $52=HEAP32[(($51)>>2)];
      var $53=(($52)|0)!=0;
      if ($53) { __label__ = 12; break; } else { __label__ = 11; break; }
    case 11: 
      _loaderror(((STRING_TABLE.__str5)|0), (tempInt=STACKTOP,STACKTOP += 1,STACKTOP = ((((STACKTOP)+3)>>2)<<2),assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),HEAP32[((tempInt)>>2)]=0,tempInt));
      __label__ = 61; break;
    case 12: 
      var $56=$file;
      var $57=(($header+8)|0);
      var $58=(($57)|0);
      var $59=HEAP32[(($58)>>2)];
      var $60=_fseek($56, $59, 0);
      var $61=(($60)|0)!=0;
      if ($61) { __label__ = 13; break; } else { __label__ = 14; break; }
    case 13: 
      _loaderror(((STRING_TABLE.__str6)|0), (tempInt=STACKTOP,STACKTOP += 1,STACKTOP = ((((STACKTOP)+3)>>2)<<2),assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),HEAP32[((tempInt)>>2)]=0,tempInt));
      __label__ = 61; break;
    case 14: 
      var $64=(($header+8)|0);
      var $65=(($64+4)|0);
      var $66=HEAP32[(($65)>>2)];
      var $67=$prog;
      var $68=(($67+12)|0);
      HEAP32[(($68)>>2)]=$66;
      var $69=(($header+8)|0);
      var $70=(($69+4)|0);
      var $71=HEAP32[(($70)>>2)];
      var $72=$prog;
      var $73=(($72+8)|0);
      HEAP32[(($73)>>2)]=$71;
      var $74=(($header+8)|0);
      var $75=(($74+4)|0);
      var $76=HEAP32[(($75)>>2)];
      var $77=((($76<<3))|0);
      var $78=_util_memory_a($77, 132, ((STRING_TABLE.__str)|0));
      var $79=$78;
      var $80=$prog;
      var $81=(($80+4)|0);
      HEAP32[(($81)>>2)]=$79;
      var $82=$prog;
      var $83=(($82+4)|0);
      var $84=HEAP32[(($83)>>2)];
      var $85=(($84)|0)!=0;
      if ($85) { __label__ = 16; break; } else { __label__ = 15; break; }
    case 15: 
      __label__ = 61; break;
    case 16: 
      var $88=$prog;
      var $89=(($88+4)|0);
      var $90=HEAP32[(($89)>>2)];
      var $91=$90;
      var $92=(($header+8)|0);
      var $93=(($92+4)|0);
      var $94=HEAP32[(($93)>>2)];
      var $95=$file;
      var $96=_fread($91, 8, $94, $95);
      var $97=(($header+8)|0);
      var $98=(($97+4)|0);
      var $99=HEAP32[(($98)>>2)];
      var $100=(($96)|0)!=(($99)|0);
      if ($100) { __label__ = 17; break; } else { __label__ = 18; break; }
    case 17: 
      _loaderror(((STRING_TABLE.__str7)|0), (tempInt=STACKTOP,STACKTOP += 1,STACKTOP = ((((STACKTOP)+3)>>2)<<2),assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),HEAP32[((tempInt)>>2)]=0,tempInt));
      __label__ = 61; break;
    case 18: 
      var $103=$file;
      var $104=(($header+16)|0);
      var $105=(($104)|0);
      var $106=HEAP32[(($105)>>2)];
      var $107=_fseek($103, $106, 0);
      var $108=(($107)|0)!=0;
      if ($108) { __label__ = 19; break; } else { __label__ = 20; break; }
    case 19: 
      _loaderror(((STRING_TABLE.__str6)|0), (tempInt=STACKTOP,STACKTOP += 1,STACKTOP = ((((STACKTOP)+3)>>2)<<2),assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),HEAP32[((tempInt)>>2)]=0,tempInt));
      __label__ = 61; break;
    case 20: 
      var $111=(($header+16)|0);
      var $112=(($111+4)|0);
      var $113=HEAP32[(($112)>>2)];
      var $114=$prog;
      var $115=(($114+24)|0);
      HEAP32[(($115)>>2)]=$113;
      var $116=(($header+16)|0);
      var $117=(($116+4)|0);
      var $118=HEAP32[(($117)>>2)];
      var $119=$prog;
      var $120=(($119+20)|0);
      HEAP32[(($120)>>2)]=$118;
      var $121=(($header+16)|0);
      var $122=(($121+4)|0);
      var $123=HEAP32[(($122)>>2)];
      var $124=((($123<<3))|0);
      var $125=_util_memory_a($124, 133, ((STRING_TABLE.__str)|0));
      var $126=$125;
      var $127=$prog;
      var $128=(($127+16)|0);
      HEAP32[(($128)>>2)]=$126;
      var $129=$prog;
      var $130=(($129+16)|0);
      var $131=HEAP32[(($130)>>2)];
      var $132=(($131)|0)!=0;
      if ($132) { __label__ = 22; break; } else { __label__ = 21; break; }
    case 21: 
      __label__ = 61; break;
    case 22: 
      var $135=$prog;
      var $136=(($135+16)|0);
      var $137=HEAP32[(($136)>>2)];
      var $138=$137;
      var $139=(($header+16)|0);
      var $140=(($139+4)|0);
      var $141=HEAP32[(($140)>>2)];
      var $142=$file;
      var $143=_fread($138, 8, $141, $142);
      var $144=(($header+16)|0);
      var $145=(($144+4)|0);
      var $146=HEAP32[(($145)>>2)];
      var $147=(($143)|0)!=(($146)|0);
      if ($147) { __label__ = 23; break; } else { __label__ = 24; break; }
    case 23: 
      _loaderror(((STRING_TABLE.__str7)|0), (tempInt=STACKTOP,STACKTOP += 1,STACKTOP = ((((STACKTOP)+3)>>2)<<2),assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),HEAP32[((tempInt)>>2)]=0,tempInt));
      __label__ = 61; break;
    case 24: 
      var $150=$file;
      var $151=(($header+24)|0);
      var $152=(($151)|0);
      var $153=HEAP32[(($152)>>2)];
      var $154=_fseek($150, $153, 0);
      var $155=(($154)|0)!=0;
      if ($155) { __label__ = 25; break; } else { __label__ = 26; break; }
    case 25: 
      _loaderror(((STRING_TABLE.__str6)|0), (tempInt=STACKTOP,STACKTOP += 1,STACKTOP = ((((STACKTOP)+3)>>2)<<2),assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),HEAP32[((tempInt)>>2)]=0,tempInt));
      __label__ = 61; break;
    case 26: 
      var $158=(($header+24)|0);
      var $159=(($158+4)|0);
      var $160=HEAP32[(($159)>>2)];
      var $161=$prog;
      var $162=(($161+36)|0);
      HEAP32[(($162)>>2)]=$160;
      var $163=(($header+24)|0);
      var $164=(($163+4)|0);
      var $165=HEAP32[(($164)>>2)];
      var $166=$prog;
      var $167=(($166+32)|0);
      HEAP32[(($167)>>2)]=$165;
      var $168=(($header+24)|0);
      var $169=(($168+4)|0);
      var $170=HEAP32[(($169)>>2)];
      var $171=((($170<<3))|0);
      var $172=_util_memory_a($171, 134, ((STRING_TABLE.__str)|0));
      var $173=$172;
      var $174=$prog;
      var $175=(($174+28)|0);
      HEAP32[(($175)>>2)]=$173;
      var $176=$prog;
      var $177=(($176+28)|0);
      var $178=HEAP32[(($177)>>2)];
      var $179=(($178)|0)!=0;
      if ($179) { __label__ = 28; break; } else { __label__ = 27; break; }
    case 27: 
      __label__ = 61; break;
    case 28: 
      var $182=$prog;
      var $183=(($182+28)|0);
      var $184=HEAP32[(($183)>>2)];
      var $185=$184;
      var $186=(($header+24)|0);
      var $187=(($186+4)|0);
      var $188=HEAP32[(($187)>>2)];
      var $189=$file;
      var $190=_fread($185, 8, $188, $189);
      var $191=(($header+24)|0);
      var $192=(($191+4)|0);
      var $193=HEAP32[(($192)>>2)];
      var $194=(($190)|0)!=(($193)|0);
      if ($194) { __label__ = 29; break; } else { __label__ = 30; break; }
    case 29: 
      _loaderror(((STRING_TABLE.__str7)|0), (tempInt=STACKTOP,STACKTOP += 1,STACKTOP = ((((STACKTOP)+3)>>2)<<2),assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),HEAP32[((tempInt)>>2)]=0,tempInt));
      __label__ = 61; break;
    case 30: 
      var $197=$file;
      var $198=(($header+32)|0);
      var $199=(($198)|0);
      var $200=HEAP32[(($199)>>2)];
      var $201=_fseek($197, $200, 0);
      var $202=(($201)|0)!=0;
      if ($202) { __label__ = 31; break; } else { __label__ = 32; break; }
    case 31: 
      _loaderror(((STRING_TABLE.__str6)|0), (tempInt=STACKTOP,STACKTOP += 1,STACKTOP = ((((STACKTOP)+3)>>2)<<2),assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),HEAP32[((tempInt)>>2)]=0,tempInt));
      __label__ = 61; break;
    case 32: 
      var $205=(($header+32)|0);
      var $206=(($205+4)|0);
      var $207=HEAP32[(($206)>>2)];
      var $208=$prog;
      var $209=(($208+48)|0);
      HEAP32[(($209)>>2)]=$207;
      var $210=(($header+32)|0);
      var $211=(($210+4)|0);
      var $212=HEAP32[(($211)>>2)];
      var $213=$prog;
      var $214=(($213+44)|0);
      HEAP32[(($214)>>2)]=$212;
      var $215=(($header+32)|0);
      var $216=(($215+4)|0);
      var $217=HEAP32[(($216)>>2)];
      var $218=((($217)*(36))|0);
      var $219=_util_memory_a($218, 135, ((STRING_TABLE.__str)|0));
      var $220=$219;
      var $221=$prog;
      var $222=(($221+40)|0);
      HEAP32[(($222)>>2)]=$220;
      var $223=$prog;
      var $224=(($223+40)|0);
      var $225=HEAP32[(($224)>>2)];
      var $226=(($225)|0)!=0;
      if ($226) { __label__ = 34; break; } else { __label__ = 33; break; }
    case 33: 
      __label__ = 61; break;
    case 34: 
      var $229=$prog;
      var $230=(($229+40)|0);
      var $231=HEAP32[(($230)>>2)];
      var $232=$231;
      var $233=(($header+32)|0);
      var $234=(($233+4)|0);
      var $235=HEAP32[(($234)>>2)];
      var $236=$file;
      var $237=_fread($232, 36, $235, $236);
      var $238=(($header+32)|0);
      var $239=(($238+4)|0);
      var $240=HEAP32[(($239)>>2)];
      var $241=(($237)|0)!=(($240)|0);
      if ($241) { __label__ = 35; break; } else { __label__ = 36; break; }
    case 35: 
      _loaderror(((STRING_TABLE.__str7)|0), (tempInt=STACKTOP,STACKTOP += 1,STACKTOP = ((((STACKTOP)+3)>>2)<<2),assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),HEAP32[((tempInt)>>2)]=0,tempInt));
      __label__ = 61; break;
    case 36: 
      var $244=$file;
      var $245=(($header+40)|0);
      var $246=(($245)|0);
      var $247=HEAP32[(($246)>>2)];
      var $248=_fseek($244, $247, 0);
      var $249=(($248)|0)!=0;
      if ($249) { __label__ = 37; break; } else { __label__ = 38; break; }
    case 37: 
      _loaderror(((STRING_TABLE.__str6)|0), (tempInt=STACKTOP,STACKTOP += 1,STACKTOP = ((((STACKTOP)+3)>>2)<<2),assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),HEAP32[((tempInt)>>2)]=0,tempInt));
      __label__ = 61; break;
    case 38: 
      var $252=(($header+40)|0);
      var $253=(($252+4)|0);
      var $254=HEAP32[(($253)>>2)];
      var $255=$prog;
      var $256=(($255+60)|0);
      HEAP32[(($256)>>2)]=$254;
      var $257=(($header+40)|0);
      var $258=(($257+4)|0);
      var $259=HEAP32[(($258)>>2)];
      var $260=$prog;
      var $261=(($260+56)|0);
      HEAP32[(($261)>>2)]=$259;
      var $262=(($header+40)|0);
      var $263=(($262+4)|0);
      var $264=HEAP32[(($263)>>2)];
      var $265=(($264)|0);
      var $266=_util_memory_a($265, 136, ((STRING_TABLE.__str)|0));
      var $267=$prog;
      var $268=(($267+52)|0);
      HEAP32[(($268)>>2)]=$266;
      var $269=$prog;
      var $270=(($269+52)|0);
      var $271=HEAP32[(($270)>>2)];
      var $272=(($271)|0)!=0;
      if ($272) { __label__ = 40; break; } else { __label__ = 39; break; }
    case 39: 
      __label__ = 61; break;
    case 40: 
      var $275=$prog;
      var $276=(($275+52)|0);
      var $277=HEAP32[(($276)>>2)];
      var $278=(($header+40)|0);
      var $279=(($278+4)|0);
      var $280=HEAP32[(($279)>>2)];
      var $281=$file;
      var $282=_fread($277, 1, $280, $281);
      var $283=(($header+40)|0);
      var $284=(($283+4)|0);
      var $285=HEAP32[(($284)>>2)];
      var $286=(($282)|0)!=(($285)|0);
      if ($286) { __label__ = 41; break; } else { __label__ = 42; break; }
    case 41: 
      _loaderror(((STRING_TABLE.__str7)|0), (tempInt=STACKTOP,STACKTOP += 1,STACKTOP = ((((STACKTOP)+3)>>2)<<2),assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),HEAP32[((tempInt)>>2)]=0,tempInt));
      __label__ = 61; break;
    case 42: 
      var $289=$file;
      var $290=(($header+48)|0);
      var $291=(($290)|0);
      var $292=HEAP32[(($291)>>2)];
      var $293=_fseek($289, $292, 0);
      var $294=(($293)|0)!=0;
      if ($294) { __label__ = 43; break; } else { __label__ = 44; break; }
    case 43: 
      _loaderror(((STRING_TABLE.__str6)|0), (tempInt=STACKTOP,STACKTOP += 1,STACKTOP = ((((STACKTOP)+3)>>2)<<2),assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),HEAP32[((tempInt)>>2)]=0,tempInt));
      __label__ = 61; break;
    case 44: 
      var $297=(($header+48)|0);
      var $298=(($297+4)|0);
      var $299=HEAP32[(($298)>>2)];
      var $300=$prog;
      var $301=(($300+72)|0);
      HEAP32[(($301)>>2)]=$299;
      var $302=(($header+48)|0);
      var $303=(($302+4)|0);
      var $304=HEAP32[(($303)>>2)];
      var $305=$prog;
      var $306=(($305+68)|0);
      HEAP32[(($306)>>2)]=$304;
      var $307=(($header+48)|0);
      var $308=(($307+4)|0);
      var $309=HEAP32[(($308)>>2)];
      var $310=((($309<<2))|0);
      var $311=_util_memory_a($310, 137, ((STRING_TABLE.__str)|0));
      var $312=$311;
      var $313=$prog;
      var $314=(($313+64)|0);
      HEAP32[(($314)>>2)]=$312;
      var $315=$prog;
      var $316=(($315+64)|0);
      var $317=HEAP32[(($316)>>2)];
      var $318=(($317)|0)!=0;
      if ($318) { __label__ = 46; break; } else { __label__ = 45; break; }
    case 45: 
      __label__ = 61; break;
    case 46: 
      var $321=$prog;
      var $322=(($321+64)|0);
      var $323=HEAP32[(($322)>>2)];
      var $324=$323;
      var $325=(($header+48)|0);
      var $326=(($325+4)|0);
      var $327=HEAP32[(($326)>>2)];
      var $328=$file;
      var $329=_fread($324, 4, $327, $328);
      var $330=(($header+48)|0);
      var $331=(($330+4)|0);
      var $332=HEAP32[(($331)>>2)];
      var $333=(($329)|0)!=(($332)|0);
      if ($333) { __label__ = 47; break; } else { __label__ = 48; break; }
    case 47: 
      _loaderror(((STRING_TABLE.__str7)|0), (tempInt=STACKTOP,STACKTOP += 1,STACKTOP = ((((STACKTOP)+3)>>2)<<2),assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),HEAP32[((tempInt)>>2)]=0,tempInt));
      __label__ = 61; break;
    case 48: 
      var $336=$file;
      var $337=_fclose($336);
      var $338=$prog;
      var $339=$prog;
      var $340=(($339+8)|0);
      var $341=HEAP32[(($340)>>2)];
      var $342=_qc_program_profile_resize($338, $341);
      if ($342) { __label__ = 50; break; } else { __label__ = 49; break; }
    case 49: 
      __label__ = 61; break;
    case 50: 
      var $345=$prog;
      var $346=(($345+56)|0);
      var $347=HEAP32[(($346)>>2)];
      var $348=$prog;
      var $349=(($348+104)|0);
      HEAP32[(($349)>>2)]=$347;
      var $350=$prog;
      var $351=(($350+56)|0);
      var $352=HEAP32[(($351)>>2)];
      var $353=$prog;
      var $354=(($353+108)|0);
      HEAP32[(($354)>>2)]=$352;
      var $355=$prog;
      var $356=$prog;
      var $357=(($356+56)|0);
      var $358=HEAP32[(($357)>>2)];
      var $359=((($358)+(16384))|0);
      var $360=_qc_program_strings_resize($355, $359);
      if ($360) { __label__ = 52; break; } else { __label__ = 51; break; }
    case 51: 
      __label__ = 61; break;
    case 52: 
      var $363=$prog;
      var $364=_qc_program_entitypool_add($363, 1);
      if ($364) { __label__ = 54; break; } else { __label__ = 53; break; }
    case 53: 
      _loaderror(((STRING_TABLE.__str8)|0), (tempInt=STACKTOP,STACKTOP += 1,STACKTOP = ((((STACKTOP)+3)>>2)<<2),assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),HEAP32[((tempInt)>>2)]=0,tempInt));
      __label__ = 61; break;
    case 54: 
      $i=0;
      __label__ = 55; break;
    case 55: 
      var $368=$i;
      var $369=$prog;
      var $370=(($369+144)|0);
      var $371=HEAP32[(($370)>>2)];
      var $372=(($368)>>>0) < (($371)>>>0);
      if ($372) { __label__ = 56; break; } else { __label__ = 60; break; }
    case 56: 
      var $374=$prog;
      var $375=_qc_program_entitydata_add($374, 0);
      if ($375) { __label__ = 58; break; } else { __label__ = 57; break; }
    case 57: 
      _loaderror(((STRING_TABLE.__str9)|0), (tempInt=STACKTOP,STACKTOP += 1,STACKTOP = ((((STACKTOP)+3)>>2)<<2),assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),HEAP32[((tempInt)>>2)]=0,tempInt));
      __label__ = 61; break;
    case 58: 
      __label__ = 59; break;
    case 59: 
      var $379=$i;
      var $380=((($379)+(1))|0);
      $i=$380;
      __label__ = 55; break;
    case 60: 
      var $382=$prog;
      var $383=(($382+140)|0);
      HEAP32[(($383)>>2)]=1;
      var $384=$prog;
      $1=$384;
      __label__ = 80; break;
    case 61: 
      var $386=$prog;
      var $387=(($386)|0);
      var $388=HEAP32[(($387)>>2)];
      var $389=(($388)|0)!=0;
      if ($389) { __label__ = 62; break; } else { __label__ = 63; break; }
    case 62: 
      var $391=$prog;
      var $392=(($391)|0);
      var $393=HEAP32[(($392)>>2)];
      _util_memory_d($393, 167, ((STRING_TABLE.__str)|0));
      __label__ = 63; break;
    case 63: 
      var $395=$prog;
      var $396=(($395+4)|0);
      var $397=HEAP32[(($396)>>2)];
      var $398=(($397)|0)!=0;
      if ($398) { __label__ = 64; break; } else { __label__ = 65; break; }
    case 64: 
      var $400=$prog;
      var $401=(($400+4)|0);
      var $402=HEAP32[(($401)>>2)];
      var $403=$402;
      _util_memory_d($403, 168, ((STRING_TABLE.__str)|0));
      __label__ = 65; break;
    case 65: 
      var $405=$prog;
      var $406=(($405+16)|0);
      var $407=HEAP32[(($406)>>2)];
      var $408=(($407)|0)!=0;
      if ($408) { __label__ = 66; break; } else { __label__ = 67; break; }
    case 66: 
      var $410=$prog;
      var $411=(($410+16)|0);
      var $412=HEAP32[(($411)>>2)];
      var $413=$412;
      _util_memory_d($413, 169, ((STRING_TABLE.__str)|0));
      __label__ = 67; break;
    case 67: 
      var $415=$prog;
      var $416=(($415+28)|0);
      var $417=HEAP32[(($416)>>2)];
      var $418=(($417)|0)!=0;
      if ($418) { __label__ = 68; break; } else { __label__ = 69; break; }
    case 68: 
      var $420=$prog;
      var $421=(($420+28)|0);
      var $422=HEAP32[(($421)>>2)];
      var $423=$422;
      _util_memory_d($423, 170, ((STRING_TABLE.__str)|0));
      __label__ = 69; break;
    case 69: 
      var $425=$prog;
      var $426=(($425+40)|0);
      var $427=HEAP32[(($426)>>2)];
      var $428=(($427)|0)!=0;
      if ($428) { __label__ = 70; break; } else { __label__ = 71; break; }
    case 70: 
      var $430=$prog;
      var $431=(($430+40)|0);
      var $432=HEAP32[(($431)>>2)];
      var $433=$432;
      _util_memory_d($433, 171, ((STRING_TABLE.__str)|0));
      __label__ = 71; break;
    case 71: 
      var $435=$prog;
      var $436=(($435+52)|0);
      var $437=HEAP32[(($436)>>2)];
      var $438=(($437)|0)!=0;
      if ($438) { __label__ = 72; break; } else { __label__ = 73; break; }
    case 72: 
      var $440=$prog;
      var $441=(($440+52)|0);
      var $442=HEAP32[(($441)>>2)];
      _util_memory_d($442, 172, ((STRING_TABLE.__str)|0));
      __label__ = 73; break;
    case 73: 
      var $444=$prog;
      var $445=(($444+64)|0);
      var $446=HEAP32[(($445)>>2)];
      var $447=(($446)|0)!=0;
      if ($447) { __label__ = 74; break; } else { __label__ = 75; break; }
    case 74: 
      var $449=$prog;
      var $450=(($449+64)|0);
      var $451=HEAP32[(($450)>>2)];
      var $452=$451;
      _util_memory_d($452, 173, ((STRING_TABLE.__str)|0));
      __label__ = 75; break;
    case 75: 
      var $454=$prog;
      var $455=(($454+76)|0);
      var $456=HEAP32[(($455)>>2)];
      var $457=(($456)|0)!=0;
      if ($457) { __label__ = 76; break; } else { __label__ = 77; break; }
    case 76: 
      var $459=$prog;
      var $460=(($459+76)|0);
      var $461=HEAP32[(($460)>>2)];
      var $462=$461;
      _util_memory_d($462, 174, ((STRING_TABLE.__str)|0));
      __label__ = 77; break;
    case 77: 
      var $464=$prog;
      var $465=(($464+88)|0);
      var $466=HEAP32[(($465)>>2)];
      var $467=(($466)|0)!=0;
      if ($467) { __label__ = 78; break; } else { __label__ = 79; break; }
    case 78: 
      var $469=$prog;
      var $470=(($469+88)|0);
      var $471=HEAP32[(($470)>>2)];
      _util_memory_d($471, 175, ((STRING_TABLE.__str)|0));
      __label__ = 79; break;
    case 79: 
      var $473=$prog;
      var $474=$473;
      _util_memory_d($474, 176, ((STRING_TABLE.__str)|0));
      $1=0;
      __label__ = 80; break;
    case 80: 
      var $476=$1;
      STACKTOP = __stackBase__;
      return $476;
    default: assert(0, "bad label: " + __label__);
  }
}
_prog_load["X"]=1;

function _loaderror($fmt) {
  var __stackBase__  = STACKTOP; STACKTOP += 4; assert(STACKTOP % 4 == 0, "Stack is unaligned"); assert(STACKTOP < STACK_MAX, "Ran out of stack");
  var __label__;

  var $1;
  var $err;
  var $ap=__stackBase__;
  $1=$fmt;
  var $2=___errno();
  var $3=HEAP32[(($2)>>2)];
  $err=$3;
  var $4=$ap;
  HEAP32[(($4)>>2)]=arguments[_loaderror.length];
  var $5=$1;
  var $6=HEAP32[(($ap)>>2)];
  var $7=_vprintf($5, $6);
  var $8=$ap;
  ;
  var $9=$err;
  var $10=_strerror($9);
  var $11=_printf(((STRING_TABLE.__str110)|0), (tempInt=STACKTOP,STACKTOP += 4,assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),HEAP32[((tempInt)>>2)]=$10,tempInt));
  STACKTOP = __stackBase__;
  return;
}


function _prog_getstring($prog, $str) {
  ;
  var __label__;
  __label__ = 2; 
  while(1) switch(__label__) {
    case 2: 
      var $1;
      var $2;
      var $3;
      $2=$prog;
      $3=$str;
      var $4=$3;
      var $5=(($4)|0) < 0;
      if ($5) { __label__ = 4; break; } else { __label__ = 3; break; }
    case 3: 
      var $7=$3;
      var $8=$2;
      var $9=(($8+56)|0);
      var $10=HEAP32[(($9)>>2)];
      var $11=(($7)>>>0) >= (($10)>>>0);
      if ($11) { __label__ = 4; break; } else { __label__ = 5; break; }
    case 4: 
      $1=((STRING_TABLE.__str10)|0);
      __label__ = 6; break;
    case 5: 
      var $14=$2;
      var $15=(($14+52)|0);
      var $16=HEAP32[(($15)>>2)];
      var $17=$3;
      var $18=(($16+$17)|0);
      $1=$18;
      __label__ = 6; break;
    case 6: 
      var $20=$1;
      ;
      return $20;
    default: assert(0, "bad label: " + __label__);
  }
}


function _prog_entfield($prog, $off) {
  ;
  var __label__;
  __label__ = 2; 
  while(1) switch(__label__) {
    case 2: 
      var $1;
      var $2;
      var $3;
      var $i;
      $2=$prog;
      $3=$off;
      $i=0;
      __label__ = 3; break;
    case 3: 
      var $5=$i;
      var $6=$2;
      var $7=(($6+32)|0);
      var $8=HEAP32[(($7)>>2)];
      var $9=(($5)>>>0) < (($8)>>>0);
      if ($9) { __label__ = 4; break; } else { __label__ = 8; break; }
    case 4: 
      var $11=$i;
      var $12=$2;
      var $13=(($12+28)|0);
      var $14=HEAP32[(($13)>>2)];
      var $15=(($14+($11<<3))|0);
      var $16=(($15+2)|0);
      var $17=HEAP16[(($16)>>1)];
      var $18=(($17)&65535);
      var $19=$3;
      var $20=(($18)|0)==(($19)|0);
      if ($20) { __label__ = 5; break; } else { __label__ = 6; break; }
    case 5: 
      var $22=$2;
      var $23=(($22+28)|0);
      var $24=HEAP32[(($23)>>2)];
      var $25=$i;
      var $26=(($24+($25<<3))|0);
      $1=$26;
      __label__ = 9; break;
    case 6: 
      __label__ = 7; break;
    case 7: 
      var $29=$i;
      var $30=((($29)+(1))|0);
      $i=$30;
      __label__ = 3; break;
    case 8: 
      $1=0;
      __label__ = 9; break;
    case 9: 
      var $33=$1;
      ;
      return $33;
    default: assert(0, "bad label: " + __label__);
  }
}


function _prog_getdef($prog, $off) {
  ;
  var __label__;
  __label__ = 2; 
  while(1) switch(__label__) {
    case 2: 
      var $1;
      var $2;
      var $3;
      var $i;
      $2=$prog;
      $3=$off;
      $i=0;
      __label__ = 3; break;
    case 3: 
      var $5=$i;
      var $6=$2;
      var $7=(($6+20)|0);
      var $8=HEAP32[(($7)>>2)];
      var $9=(($5)>>>0) < (($8)>>>0);
      if ($9) { __label__ = 4; break; } else { __label__ = 8; break; }
    case 4: 
      var $11=$i;
      var $12=$2;
      var $13=(($12+16)|0);
      var $14=HEAP32[(($13)>>2)];
      var $15=(($14+($11<<3))|0);
      var $16=(($15+2)|0);
      var $17=HEAP16[(($16)>>1)];
      var $18=(($17)&65535);
      var $19=$3;
      var $20=(($18)|0)==(($19)|0);
      if ($20) { __label__ = 5; break; } else { __label__ = 6; break; }
    case 5: 
      var $22=$2;
      var $23=(($22+16)|0);
      var $24=HEAP32[(($23)>>2)];
      var $25=$i;
      var $26=(($24+($25<<3))|0);
      $1=$26;
      __label__ = 9; break;
    case 6: 
      __label__ = 7; break;
    case 7: 
      var $29=$i;
      var $30=((($29)+(1))|0);
      $i=$30;
      __label__ = 3; break;
    case 8: 
      $1=0;
      __label__ = 9; break;
    case 9: 
      var $33=$1;
      ;
      return $33;
    default: assert(0, "bad label: " + __label__);
  }
}


function _prog_delete($prog) {
  ;
  var __label__;
  __label__ = 2; 
  while(1) switch(__label__) {
    case 2: 
      var $1;
      $1=$prog;
      var $2=$1;
      var $3=(($2)|0);
      var $4=HEAP32[(($3)>>2)];
      var $5=(($4)|0)!=0;
      if ($5) { __label__ = 3; break; } else { __label__ = 4; break; }
    case 3: 
      var $7=$1;
      var $8=(($7)|0);
      var $9=HEAP32[(($8)>>2)];
      _util_memory_d($9, 182, ((STRING_TABLE.__str)|0));
      __label__ = 4; break;
    case 4: 
      var $11=$1;
      var $12=(($11+4)|0);
      var $13=HEAP32[(($12)>>2)];
      var $14=(($13)|0)!=0;
      if ($14) { __label__ = 5; break; } else { __label__ = 6; break; }
    case 5: 
      var $16=$1;
      var $17=(($16+4)|0);
      var $18=HEAP32[(($17)>>2)];
      var $19=$18;
      _util_memory_d($19, 183, ((STRING_TABLE.__str)|0));
      __label__ = 6; break;
    case 6: 
      var $21=$1;
      var $22=(($21+4)|0);
      HEAP32[(($22)>>2)]=0;
      var $23=$1;
      var $24=(($23+8)|0);
      HEAP32[(($24)>>2)]=0;
      var $25=$1;
      var $26=(($25+12)|0);
      HEAP32[(($26)>>2)]=0;
      var $27=$1;
      var $28=(($27+16)|0);
      var $29=HEAP32[(($28)>>2)];
      var $30=(($29)|0)!=0;
      if ($30) { __label__ = 7; break; } else { __label__ = 8; break; }
    case 7: 
      var $32=$1;
      var $33=(($32+16)|0);
      var $34=HEAP32[(($33)>>2)];
      var $35=$34;
      _util_memory_d($35, 184, ((STRING_TABLE.__str)|0));
      __label__ = 8; break;
    case 8: 
      var $37=$1;
      var $38=(($37+16)|0);
      HEAP32[(($38)>>2)]=0;
      var $39=$1;
      var $40=(($39+20)|0);
      HEAP32[(($40)>>2)]=0;
      var $41=$1;
      var $42=(($41+24)|0);
      HEAP32[(($42)>>2)]=0;
      var $43=$1;
      var $44=(($43+28)|0);
      var $45=HEAP32[(($44)>>2)];
      var $46=(($45)|0)!=0;
      if ($46) { __label__ = 9; break; } else { __label__ = 10; break; }
    case 9: 
      var $48=$1;
      var $49=(($48+28)|0);
      var $50=HEAP32[(($49)>>2)];
      var $51=$50;
      _util_memory_d($51, 185, ((STRING_TABLE.__str)|0));
      __label__ = 10; break;
    case 10: 
      var $53=$1;
      var $54=(($53+28)|0);
      HEAP32[(($54)>>2)]=0;
      var $55=$1;
      var $56=(($55+32)|0);
      HEAP32[(($56)>>2)]=0;
      var $57=$1;
      var $58=(($57+36)|0);
      HEAP32[(($58)>>2)]=0;
      var $59=$1;
      var $60=(($59+40)|0);
      var $61=HEAP32[(($60)>>2)];
      var $62=(($61)|0)!=0;
      if ($62) { __label__ = 11; break; } else { __label__ = 12; break; }
    case 11: 
      var $64=$1;
      var $65=(($64+40)|0);
      var $66=HEAP32[(($65)>>2)];
      var $67=$66;
      _util_memory_d($67, 186, ((STRING_TABLE.__str)|0));
      __label__ = 12; break;
    case 12: 
      var $69=$1;
      var $70=(($69+40)|0);
      HEAP32[(($70)>>2)]=0;
      var $71=$1;
      var $72=(($71+44)|0);
      HEAP32[(($72)>>2)]=0;
      var $73=$1;
      var $74=(($73+48)|0);
      HEAP32[(($74)>>2)]=0;
      var $75=$1;
      var $76=(($75+52)|0);
      var $77=HEAP32[(($76)>>2)];
      var $78=(($77)|0)!=0;
      if ($78) { __label__ = 13; break; } else { __label__ = 14; break; }
    case 13: 
      var $80=$1;
      var $81=(($80+52)|0);
      var $82=HEAP32[(($81)>>2)];
      _util_memory_d($82, 187, ((STRING_TABLE.__str)|0));
      __label__ = 14; break;
    case 14: 
      var $84=$1;
      var $85=(($84+52)|0);
      HEAP32[(($85)>>2)]=0;
      var $86=$1;
      var $87=(($86+56)|0);
      HEAP32[(($87)>>2)]=0;
      var $88=$1;
      var $89=(($88+60)|0);
      HEAP32[(($89)>>2)]=0;
      var $90=$1;
      var $91=(($90+64)|0);
      var $92=HEAP32[(($91)>>2)];
      var $93=(($92)|0)!=0;
      if ($93) { __label__ = 15; break; } else { __label__ = 16; break; }
    case 15: 
      var $95=$1;
      var $96=(($95+64)|0);
      var $97=HEAP32[(($96)>>2)];
      var $98=$97;
      _util_memory_d($98, 188, ((STRING_TABLE.__str)|0));
      __label__ = 16; break;
    case 16: 
      var $100=$1;
      var $101=(($100+64)|0);
      HEAP32[(($101)>>2)]=0;
      var $102=$1;
      var $103=(($102+68)|0);
      HEAP32[(($103)>>2)]=0;
      var $104=$1;
      var $105=(($104+72)|0);
      HEAP32[(($105)>>2)]=0;
      var $106=$1;
      var $107=(($106+76)|0);
      var $108=HEAP32[(($107)>>2)];
      var $109=(($108)|0)!=0;
      if ($109) { __label__ = 17; break; } else { __label__ = 18; break; }
    case 17: 
      var $111=$1;
      var $112=(($111+76)|0);
      var $113=HEAP32[(($112)>>2)];
      var $114=$113;
      _util_memory_d($114, 189, ((STRING_TABLE.__str)|0));
      __label__ = 18; break;
    case 18: 
      var $116=$1;
      var $117=(($116+76)|0);
      HEAP32[(($117)>>2)]=0;
      var $118=$1;
      var $119=(($118+80)|0);
      HEAP32[(($119)>>2)]=0;
      var $120=$1;
      var $121=(($120+84)|0);
      HEAP32[(($121)>>2)]=0;
      var $122=$1;
      var $123=(($122+88)|0);
      var $124=HEAP32[(($123)>>2)];
      var $125=(($124)|0)!=0;
      if ($125) { __label__ = 19; break; } else { __label__ = 20; break; }
    case 19: 
      var $127=$1;
      var $128=(($127+88)|0);
      var $129=HEAP32[(($128)>>2)];
      _util_memory_d($129, 190, ((STRING_TABLE.__str)|0));
      __label__ = 20; break;
    case 20: 
      var $131=$1;
      var $132=(($131+88)|0);
      HEAP32[(($132)>>2)]=0;
      var $133=$1;
      var $134=(($133+92)|0);
      HEAP32[(($134)>>2)]=0;
      var $135=$1;
      var $136=(($135+96)|0);
      HEAP32[(($136)>>2)]=0;
      var $137=$1;
      var $138=(($137+152)|0);
      var $139=HEAP32[(($138)>>2)];
      var $140=(($139)|0)!=0;
      if ($140) { __label__ = 21; break; } else { __label__ = 22; break; }
    case 21: 
      var $142=$1;
      var $143=(($142+152)|0);
      var $144=HEAP32[(($143)>>2)];
      var $145=$144;
      _util_memory_d($145, 191, ((STRING_TABLE.__str)|0));
      __label__ = 22; break;
    case 22: 
      var $147=$1;
      var $148=(($147+152)|0);
      HEAP32[(($148)>>2)]=0;
      var $149=$1;
      var $150=(($149+156)|0);
      HEAP32[(($150)>>2)]=0;
      var $151=$1;
      var $152=(($151+160)|0);
      HEAP32[(($152)>>2)]=0;
      var $153=$1;
      var $154=(($153+164)|0);
      var $155=HEAP32[(($154)>>2)];
      var $156=(($155)|0)!=0;
      if ($156) { __label__ = 23; break; } else { __label__ = 24; break; }
    case 23: 
      var $158=$1;
      var $159=(($158+164)|0);
      var $160=HEAP32[(($159)>>2)];
      var $161=$160;
      _util_memory_d($161, 192, ((STRING_TABLE.__str)|0));
      __label__ = 24; break;
    case 24: 
      var $163=$1;
      var $164=(($163+164)|0);
      HEAP32[(($164)>>2)]=0;
      var $165=$1;
      var $166=(($165+168)|0);
      HEAP32[(($166)>>2)]=0;
      var $167=$1;
      var $168=(($167+172)|0);
      HEAP32[(($168)>>2)]=0;
      var $169=$1;
      var $170=(($169+116)|0);
      var $171=HEAP32[(($170)>>2)];
      var $172=(($171)|0)!=0;
      if ($172) { __label__ = 25; break; } else { __label__ = 26; break; }
    case 25: 
      var $174=$1;
      var $175=(($174+116)|0);
      var $176=HEAP32[(($175)>>2)];
      var $177=$176;
      _util_memory_d($177, 193, ((STRING_TABLE.__str)|0));
      __label__ = 26; break;
    case 26: 
      var $179=$1;
      var $180=(($179+116)|0);
      HEAP32[(($180)>>2)]=0;
      var $181=$1;
      var $182=(($181+120)|0);
      HEAP32[(($182)>>2)]=0;
      var $183=$1;
      var $184=(($183+124)|0);
      HEAP32[(($184)>>2)]=0;
      var $185=$1;
      var $186=(($185+136)|0);
      var $187=HEAP32[(($186)>>2)];
      var $188=(($187)|0)!=0;
      if ($188) { __label__ = 27; break; } else { __label__ = 30; break; }
    case 27: 
      var $190=$1;
      var $191=(($190+128)|0);
      var $192=HEAP32[(($191)>>2)];
      var $193=(($192)|0)!=0;
      if ($193) { __label__ = 28; break; } else { __label__ = 29; break; }
    case 28: 
      var $195=$1;
      var $196=(($195+128)|0);
      var $197=HEAP32[(($196)>>2)];
      var $198=$197;
      _util_memory_d($198, 196, ((STRING_TABLE.__str)|0));
      __label__ = 29; break;
    case 29: 
      var $200=$1;
      var $201=(($200+128)|0);
      HEAP32[(($201)>>2)]=0;
      var $202=$1;
      var $203=(($202+132)|0);
      HEAP32[(($203)>>2)]=0;
      var $204=$1;
      var $205=(($204+136)|0);
      HEAP32[(($205)>>2)]=0;
      __label__ = 30; break;
    case 30: 
      var $207=$1;
      var $208=$207;
      _util_memory_d($208, 199, ((STRING_TABLE.__str)|0));
      ;
      return;
    default: assert(0, "bad label: " + __label__);
  }
}
_prog_delete["X"]=1;

function _prog_getedict($prog, $e) {
  var __stackBase__  = STACKTOP; assert(STACKTOP % 4 == 0, "Stack is unaligned"); assert(STACKTOP < STACK_MAX, "Ran out of stack");
  var __label__;
  __label__ = 2; 
  while(1) switch(__label__) {
    case 2: 
      var $1;
      var $2;
      $1=$prog;
      $2=$e;
      var $3=$2;
      var $4=$1;
      var $5=(($4+92)|0);
      var $6=HEAP32[(($5)>>2)];
      var $7=(($3)>>>0) >= (($6)>>>0);
      if ($7) { __label__ = 3; break; } else { __label__ = 4; break; }
    case 3: 
      var $9=$1;
      var $10=(($9+112)|0);
      var $11=HEAP32[(($10)>>2)];
      var $12=((($11)+(1))|0);
      HEAP32[(($10)>>2)]=$12;
      var $13=$2;
      var $14=_printf(((STRING_TABLE.__str11)|0), (tempInt=STACKTOP,STACKTOP += 4,assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),HEAP32[((tempInt)>>2)]=$13,tempInt));
      $2=0;
      __label__ = 4; break;
    case 4: 
      var $16=$1;
      var $17=(($16+76)|0);
      var $18=HEAP32[(($17)>>2)];
      var $19=$1;
      var $20=(($19+144)|0);
      var $21=HEAP32[(($20)>>2)];
      var $22=$2;
      var $23=((($21)*($22))|0);
      var $24=(($18+($23<<2))|0);
      var $25=$24;
      STACKTOP = __stackBase__;
      return $25;
    default: assert(0, "bad label: " + __label__);
  }
}


function _prog_spawn_entity($prog) {
  var __stackBase__  = STACKTOP; assert(STACKTOP % 4 == 0, "Stack is unaligned"); assert(STACKTOP < STACK_MAX, "Ran out of stack");
  var __label__;
  __label__ = 2; 
  while(1) switch(__label__) {
    case 2: 
      var $1;
      var $2;
      var $data;
      var $i;
      var $e;
      $2=$prog;
      $e=0;
      __label__ = 3; break;
    case 3: 
      var $4=$e;
      var $5=$2;
      var $6=(($5+92)|0);
      var $7=HEAP32[(($6)>>2)];
      var $8=(($4)|0) < (($7)|0);
      if ($8) { __label__ = 4; break; } else { __label__ = 8; break; }
    case 4: 
      var $10=$e;
      var $11=$2;
      var $12=(($11+88)|0);
      var $13=HEAP32[(($12)>>2)];
      var $14=(($13+$10)|0);
      var $15=HEAP8[($14)];
      var $16=(($15) & 1);
      if ($16) { __label__ = 6; break; } else { __label__ = 5; break; }
    case 5: 
      var $18=$2;
      var $19=(($18+76)|0);
      var $20=HEAP32[(($19)>>2)];
      var $21=$2;
      var $22=(($21+144)|0);
      var $23=HEAP32[(($22)>>2)];
      var $24=$e;
      var $25=((($23)*($24))|0);
      var $26=(($20+($25<<2))|0);
      var $27=$26;
      $data=$27;
      var $28=$data;
      var $29=$2;
      var $30=(($29+144)|0);
      var $31=HEAP32[(($30)>>2)];
      var $32=((($31<<2))|0);
      _memset($28, 0, $32, 1);
      var $33=$e;
      $1=$33;
      __label__ = 17; break;
    case 6: 
      __label__ = 7; break;
    case 7: 
      var $36=$e;
      var $37=((($36)+(1))|0);
      $e=$37;
      __label__ = 3; break;
    case 8: 
      var $39=$2;
      var $40=_qc_program_entitypool_add($39, 1);
      if ($40) { __label__ = 10; break; } else { __label__ = 9; break; }
    case 9: 
      var $42=$2;
      var $43=(($42+112)|0);
      var $44=HEAP32[(($43)>>2)];
      var $45=((($44)+(1))|0);
      HEAP32[(($43)>>2)]=$45;
      var $46=_printf(((STRING_TABLE.__str12)|0), (tempInt=STACKTOP,STACKTOP += 1,STACKTOP = ((((STACKTOP)+3)>>2)<<2),assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),HEAP32[((tempInt)>>2)]=0,tempInt));
      $1=0;
      __label__ = 17; break;
    case 10: 
      var $48=$2;
      var $49=(($48+140)|0);
      var $50=HEAP32[(($49)>>2)];
      var $51=((($50)+(1))|0);
      HEAP32[(($49)>>2)]=$51;
      $i=0;
      __label__ = 11; break;
    case 11: 
      var $53=$i;
      var $54=$2;
      var $55=(($54+144)|0);
      var $56=HEAP32[(($55)>>2)];
      var $57=(($53)>>>0) < (($56)>>>0);
      if ($57) { __label__ = 12; break; } else { __label__ = 16; break; }
    case 12: 
      var $59=$2;
      var $60=_qc_program_entitydata_add($59, 0);
      if ($60) { __label__ = 14; break; } else { __label__ = 13; break; }
    case 13: 
      var $62=_printf(((STRING_TABLE.__str12)|0), (tempInt=STACKTOP,STACKTOP += 1,STACKTOP = ((((STACKTOP)+3)>>2)<<2),assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),HEAP32[((tempInt)>>2)]=0,tempInt));
      $1=0;
      __label__ = 17; break;
    case 14: 
      __label__ = 15; break;
    case 15: 
      var $65=$i;
      var $66=((($65)+(1))|0);
      $i=$66;
      __label__ = 11; break;
    case 16: 
      var $68=$2;
      var $69=(($68+76)|0);
      var $70=HEAP32[(($69)>>2)];
      var $71=$2;
      var $72=(($71+144)|0);
      var $73=HEAP32[(($72)>>2)];
      var $74=$e;
      var $75=((($73)*($74))|0);
      var $76=(($70+($75<<2))|0);
      var $77=$76;
      $data=$77;
      var $78=$data;
      var $79=$2;
      var $80=(($79+144)|0);
      var $81=HEAP32[(($80)>>2)];
      var $82=((($81<<2))|0);
      _memset($78, 0, $82, 1);
      var $83=$e;
      $1=$83;
      __label__ = 17; break;
    case 17: 
      var $85=$1;
      STACKTOP = __stackBase__;
      return $85;
    default: assert(0, "bad label: " + __label__);
  }
}
_prog_spawn_entity["X"]=1;

function _prog_free_entity($prog, $e) {
  var __stackBase__  = STACKTOP; assert(STACKTOP % 4 == 0, "Stack is unaligned"); assert(STACKTOP < STACK_MAX, "Ran out of stack");
  var __label__;
  __label__ = 2; 
  while(1) switch(__label__) {
    case 2: 
      var $1;
      var $2;
      $1=$prog;
      $2=$e;
      var $3=$2;
      var $4=(($3)|0)!=0;
      if ($4) { __label__ = 4; break; } else { __label__ = 3; break; }
    case 3: 
      var $6=$1;
      var $7=(($6+112)|0);
      var $8=HEAP32[(($7)>>2)];
      var $9=((($8)+(1))|0);
      HEAP32[(($7)>>2)]=$9;
      var $10=_printf(((STRING_TABLE.__str13)|0), (tempInt=STACKTOP,STACKTOP += 1,STACKTOP = ((((STACKTOP)+3)>>2)<<2),assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),HEAP32[((tempInt)>>2)]=0,tempInt));
      __label__ = 9; break;
    case 4: 
      var $12=$2;
      var $13=$1;
      var $14=(($13+92)|0);
      var $15=HEAP32[(($14)>>2)];
      var $16=(($12)>>>0) >= (($15)>>>0);
      if ($16) { __label__ = 5; break; } else { __label__ = 6; break; }
    case 5: 
      var $18=$1;
      var $19=(($18+112)|0);
      var $20=HEAP32[(($19)>>2)];
      var $21=((($20)+(1))|0);
      HEAP32[(($19)>>2)]=$21;
      var $22=_printf(((STRING_TABLE.__str14)|0), (tempInt=STACKTOP,STACKTOP += 1,STACKTOP = ((((STACKTOP)+3)>>2)<<2),assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),HEAP32[((tempInt)>>2)]=0,tempInt));
      __label__ = 9; break;
    case 6: 
      var $24=$2;
      var $25=$1;
      var $26=(($25+88)|0);
      var $27=HEAP32[(($26)>>2)];
      var $28=(($27+$24)|0);
      var $29=HEAP8[($28)];
      var $30=(($29) & 1);
      if ($30) { __label__ = 8; break; } else { __label__ = 7; break; }
    case 7: 
      var $32=$1;
      var $33=(($32+112)|0);
      var $34=HEAP32[(($33)>>2)];
      var $35=((($34)+(1))|0);
      HEAP32[(($33)>>2)]=$35;
      var $36=_printf(((STRING_TABLE.__str15)|0), (tempInt=STACKTOP,STACKTOP += 1,STACKTOP = ((((STACKTOP)+3)>>2)<<2),assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),HEAP32[((tempInt)>>2)]=0,tempInt));
      __label__ = 9; break;
    case 8: 
      var $38=$2;
      var $39=$1;
      var $40=(($39+88)|0);
      var $41=HEAP32[(($40)>>2)];
      var $42=(($41+$38)|0);
      HEAP8[($42)]=0;
      __label__ = 9; break;
    case 9: 
      STACKTOP = __stackBase__;
      return;
    default: assert(0, "bad label: " + __label__);
  }
}
_prog_free_entity["X"]=1;

function _prog_tempstring($prog, $_str) {
  ;
  var __label__;
  __label__ = 2; 
  while(1) switch(__label__) {
    case 2: 
      var $1;
      var $2;
      var $3;
      var $str;
      var $len;
      var $at;
      $2=$prog;
      $3=$_str;
      var $4=$3;
      $str=$4;
      var $5=$str;
      var $6=_strlen($5);
      $len=$6;
      var $7=$2;
      var $8=(($7+108)|0);
      var $9=HEAP32[(($8)>>2)];
      $at=$9;
      var $10=$at;
      var $11=$len;
      var $12=((($10)+($11))|0);
      var $13=$2;
      var $14=(($13+56)|0);
      var $15=HEAP32[(($14)>>2)];
      var $16=(($12)>>>0) >= (($15)>>>0);
      if ($16) { __label__ = 3; break; } else { __label__ = 4; break; }
    case 3: 
      var $18=$2;
      var $19=(($18+104)|0);
      var $20=HEAP32[(($19)>>2)];
      $at=$20;
      __label__ = 4; break;
    case 4: 
      var $22=$at;
      var $23=$len;
      var $24=((($22)+($23))|0);
      var $25=$2;
      var $26=(($25+56)|0);
      var $27=HEAP32[(($26)>>2)];
      var $28=(($24)>>>0) >= (($27)>>>0);
      if ($28) { __label__ = 5; break; } else { __label__ = 8; break; }
    case 5: 
      var $30=$at;
      var $31=$2;
      var $32=(($31+56)|0);
      HEAP32[(($32)>>2)]=$30;
      var $33=$2;
      var $34=$str;
      var $35=$len;
      var $36=((($35)+(1))|0);
      var $37=_qc_program_strings_append($33, $34, $36);
      if ($37) { __label__ = 7; break; } else { __label__ = 6; break; }
    case 6: 
      var $39=$2;
      var $40=(($39+112)|0);
      HEAP32[(($40)>>2)]=1;
      $1=0;
      __label__ = 9; break;
    case 7: 
      var $42=$at;
      $1=$42;
      __label__ = 9; break;
    case 8: 
      var $44=$2;
      var $45=(($44+52)|0);
      var $46=HEAP32[(($45)>>2)];
      var $47=$at;
      var $48=(($46+$47)|0);
      var $49=$str;
      var $50=$len;
      var $51=((($50)+(1))|0);
      assert($51 % 1 === 0, 'memcpy given ' + $51 + ' bytes to copy. Problem with quantum=1 corrections perhaps?');_memcpy($48, $49, $51, 1);
      var $52=$len;
      var $53=((($52)+(1))|0);
      var $54=$2;
      var $55=(($54+108)|0);
      var $56=HEAP32[(($55)>>2)];
      var $57=((($56)+($53))|0);
      HEAP32[(($55)>>2)]=$57;
      var $58=$at;
      $1=$58;
      __label__ = 9; break;
    case 9: 
      var $60=$1;
      ;
      return $60;
    default: assert(0, "bad label: " + __label__);
  }
}
_prog_tempstring["X"]=1;

function _prog_exec($prog, $func, $flags, $maxjumps) {
  var __stackBase__  = STACKTOP; assert(STACKTOP % 4 == 0, "Stack is unaligned"); assert(STACKTOP < STACK_MAX, "Ran out of stack");
  var __label__;
  __label__ = 2; 
  while(1) switch(__label__) {
    case 2: 
      var $1;
      var $2;
      var $3;
      var $4;
      var $5;
      var $jumpcount;
      var $oldxflags;
      var $st;
      var $newf;
      var $ed;
      var $ptr;
      var $builtinnumber;
      var $newf1;
      var $ed2;
      var $ptr3;
      var $builtinnumber4;
      var $newf5;
      var $ed6;
      var $ptr7;
      var $builtinnumber8;
      var $newf9;
      var $ed10;
      var $ptr11;
      var $builtinnumber12;
      $2=$prog;
      $3=$func;
      $4=$flags;
      $5=$maxjumps;
      $jumpcount=0;
      var $6=$2;
      var $7=(($6+180)|0);
      var $8=HEAP32[(($7)>>2)];
      $oldxflags=$8;
      var $9=$2;
      var $10=(($9+112)|0);
      HEAP32[(($10)>>2)]=0;
      var $11=$4;
      var $12=$2;
      var $13=(($12+180)|0);
      HEAP32[(($13)>>2)]=$11;
      var $14=$2;
      var $15=(($14+4)|0);
      var $16=HEAP32[(($15)>>2)];
      var $17=$2;
      var $18=$3;
      var $19=_prog_enterfunction($17, $18);
      var $20=(($16+($19<<3))|0);
      $st=$20;
      var $21=$st;
      var $22=((($21)-(8))|0);
      $st=$22;
      var $23=$4;
      if ((($23)|0) == 0) {
        __label__ = 4; break;
      }
      else if ((($23)|0) == 1) {
        __label__ = 125; break;
      }
      else if ((($23)|0) == 2) {
        __label__ = 246; break;
      }
      else if ((($23)|0) == 3) {
        __label__ = 367; break;
      }
      else {
      __label__ = 3; break;
      }
      
    case 3: 
      __label__ = 4; break;
    case 4: 
      __label__ = 5; break;
    case 5: 
      var $27=$st;
      var $28=(($27+8)|0);
      $st=$28;
      var $29=$st;
      var $30=(($29)|0);
      var $31=HEAP16[(($30)>>1)];
      var $32=(($31)&65535);
      if ((($32)|0) == 0 || (($32)|0) == 43) {
        __label__ = 7; break;
      }
      else if ((($32)|0) == 1) {
        __label__ = 10; break;
      }
      else if ((($32)|0) == 2) {
        __label__ = 11; break;
      }
      else if ((($32)|0) == 3) {
        __label__ = 12; break;
      }
      else if ((($32)|0) == 4) {
        __label__ = 13; break;
      }
      else if ((($32)|0) == 5) {
        __label__ = 14; break;
      }
      else if ((($32)|0) == 6) {
        __label__ = 18; break;
      }
      else if ((($32)|0) == 7) {
        __label__ = 19; break;
      }
      else if ((($32)|0) == 8) {
        __label__ = 20; break;
      }
      else if ((($32)|0) == 9) {
        __label__ = 21; break;
      }
      else if ((($32)|0) == 10) {
        __label__ = 22; break;
      }
      else if ((($32)|0) == 11) {
        __label__ = 23; break;
      }
      else if ((($32)|0) == 12) {
        __label__ = 27; break;
      }
      else if ((($32)|0) == 13) {
        __label__ = 28; break;
      }
      else if ((($32)|0) == 14) {
        __label__ = 29; break;
      }
      else if ((($32)|0) == 15) {
        __label__ = 30; break;
      }
      else if ((($32)|0) == 16) {
        __label__ = 31; break;
      }
      else if ((($32)|0) == 17) {
        __label__ = 35; break;
      }
      else if ((($32)|0) == 18) {
        __label__ = 36; break;
      }
      else if ((($32)|0) == 19) {
        __label__ = 37; break;
      }
      else if ((($32)|0) == 20) {
        __label__ = 38; break;
      }
      else if ((($32)|0) == 21) {
        __label__ = 39; break;
      }
      else if ((($32)|0) == 22) {
        __label__ = 40; break;
      }
      else if ((($32)|0) == 23) {
        __label__ = 41; break;
      }
      else if ((($32)|0) == 24 || (($32)|0) == 26 || (($32)|0) == 28 || (($32)|0) == 27 || (($32)|0) == 29) {
        __label__ = 42; break;
      }
      else if ((($32)|0) == 25) {
        __label__ = 48; break;
      }
      else if ((($32)|0) == 30) {
        __label__ = 55; break;
      }
      else if ((($32)|0) == 31 || (($32)|0) == 33 || (($32)|0) == 34 || (($32)|0) == 35 || (($32)|0) == 36) {
        __label__ = 61; break;
      }
      else if ((($32)|0) == 32) {
        __label__ = 62; break;
      }
      else if ((($32)|0) == 37 || (($32)|0) == 39 || (($32)|0) == 40 || (($32)|0) == 41 || (($32)|0) == 42) {
        __label__ = 63; break;
      }
      else if ((($32)|0) == 38) {
        __label__ = 70; break;
      }
      else if ((($32)|0) == 44) {
        __label__ = 77; break;
      }
      else if ((($32)|0) == 45) {
        __label__ = 78; break;
      }
      else if ((($32)|0) == 46) {
        __label__ = 82; break;
      }
      else if ((($32)|0) == 47) {
        __label__ = 85; break;
      }
      else if ((($32)|0) == 48) {
        __label__ = 86; break;
      }
      else if ((($32)|0) == 49) {
        __label__ = 87; break;
      }
      else if ((($32)|0) == 50) {
        __label__ = 92; break;
      }
      else if ((($32)|0) == 51 || (($32)|0) == 52 || (($32)|0) == 53 || (($32)|0) == 54 || (($32)|0) == 55 || (($32)|0) == 56 || (($32)|0) == 57 || (($32)|0) == 58 || (($32)|0) == 59) {
        __label__ = 97; break;
      }
      else if ((($32)|0) == 60) {
        __label__ = 112; break;
      }
      else if ((($32)|0) == 61) {
        __label__ = 113; break;
      }
      else if ((($32)|0) == 62) {
        __label__ = 116; break;
      }
      else if ((($32)|0) == 63) {
        __label__ = 119; break;
      }
      else if ((($32)|0) == 64) {
        __label__ = 122; break;
      }
      else if ((($32)|0) == 65) {
        __label__ = 123; break;
      }
      else {
      __label__ = 6; break;
      }
      
    case 6: 
      var $34=$2;
      var $35=$2;
      var $36=(($35)|0);
      var $37=HEAP32[(($36)>>2)];
      _qcvmerror($34, ((STRING_TABLE.__str16)|0), (tempInt=STACKTOP,STACKTOP += 4,assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),HEAP32[((tempInt)>>2)]=$37,tempInt));
      __label__ = 488; break;
    case 7: 
      var $39=$2;
      var $40=(($39+64)|0);
      var $41=HEAP32[(($40)>>2)];
      var $42=$st;
      var $43=(($42+2)|0);
      var $44=$43;
      var $45=HEAP16[(($44)>>1)];
      var $46=(($45)&65535);
      var $47=(($41+($46<<2))|0);
      var $48=$47;
      var $49=$48;
      var $50=(($49)|0);
      var $51=HEAP32[(($50)>>2)];
      var $52=$2;
      var $53=(($52+64)|0);
      var $54=HEAP32[(($53)>>2)];
      var $55=(($54+4)|0);
      var $56=$55;
      var $57=$56;
      var $58=(($57)|0);
      HEAP32[(($58)>>2)]=$51;
      var $59=$2;
      var $60=(($59+64)|0);
      var $61=HEAP32[(($60)>>2)];
      var $62=$st;
      var $63=(($62+2)|0);
      var $64=$63;
      var $65=HEAP16[(($64)>>1)];
      var $66=(($65)&65535);
      var $67=(($61+($66<<2))|0);
      var $68=$67;
      var $69=$68;
      var $70=(($69+4)|0);
      var $71=HEAP32[(($70)>>2)];
      var $72=$2;
      var $73=(($72+64)|0);
      var $74=HEAP32[(($73)>>2)];
      var $75=(($74+4)|0);
      var $76=$75;
      var $77=$76;
      var $78=(($77+4)|0);
      HEAP32[(($78)>>2)]=$71;
      var $79=$2;
      var $80=(($79+64)|0);
      var $81=HEAP32[(($80)>>2)];
      var $82=$st;
      var $83=(($82+2)|0);
      var $84=$83;
      var $85=HEAP16[(($84)>>1)];
      var $86=(($85)&65535);
      var $87=(($81+($86<<2))|0);
      var $88=$87;
      var $89=$88;
      var $90=(($89+8)|0);
      var $91=HEAP32[(($90)>>2)];
      var $92=$2;
      var $93=(($92+64)|0);
      var $94=HEAP32[(($93)>>2)];
      var $95=(($94+4)|0);
      var $96=$95;
      var $97=$96;
      var $98=(($97+8)|0);
      HEAP32[(($98)>>2)]=$91;
      var $99=$2;
      var $100=(($99+4)|0);
      var $101=HEAP32[(($100)>>2)];
      var $102=$2;
      var $103=_prog_leavefunction($102);
      var $104=(($101+($103<<3))|0);
      $st=$104;
      var $105=$2;
      var $106=(($105+168)|0);
      var $107=HEAP32[(($106)>>2)];
      var $108=(($107)|0)!=0;
      if ($108) { __label__ = 9; break; } else { __label__ = 8; break; }
    case 8: 
      __label__ = 488; break;
    case 9: 
      __label__ = 124; break;
    case 10: 
      var $112=$2;
      var $113=(($112+64)|0);
      var $114=HEAP32[(($113)>>2)];
      var $115=$st;
      var $116=(($115+2)|0);
      var $117=$116;
      var $118=HEAP16[(($117)>>1)];
      var $119=(($118)&65535);
      var $120=(($114+($119<<2))|0);
      var $121=$120;
      var $122=$121;
      var $123=HEAPF32[(($122)>>2)];
      var $124=$2;
      var $125=(($124+64)|0);
      var $126=HEAP32[(($125)>>2)];
      var $127=$st;
      var $128=(($127+4)|0);
      var $129=$128;
      var $130=HEAP16[(($129)>>1)];
      var $131=(($130)&65535);
      var $132=(($126+($131<<2))|0);
      var $133=$132;
      var $134=$133;
      var $135=HEAPF32[(($134)>>2)];
      var $136=($123)*($135);
      var $137=$2;
      var $138=(($137+64)|0);
      var $139=HEAP32[(($138)>>2)];
      var $140=$st;
      var $141=(($140+6)|0);
      var $142=$141;
      var $143=HEAP16[(($142)>>1)];
      var $144=(($143)&65535);
      var $145=(($139+($144<<2))|0);
      var $146=$145;
      var $147=$146;
      HEAPF32[(($147)>>2)]=$136;
      __label__ = 124; break;
    case 11: 
      var $149=$2;
      var $150=(($149+64)|0);
      var $151=HEAP32[(($150)>>2)];
      var $152=$st;
      var $153=(($152+2)|0);
      var $154=$153;
      var $155=HEAP16[(($154)>>1)];
      var $156=(($155)&65535);
      var $157=(($151+($156<<2))|0);
      var $158=$157;
      var $159=$158;
      var $160=(($159)|0);
      var $161=HEAPF32[(($160)>>2)];
      var $162=$2;
      var $163=(($162+64)|0);
      var $164=HEAP32[(($163)>>2)];
      var $165=$st;
      var $166=(($165+4)|0);
      var $167=$166;
      var $168=HEAP16[(($167)>>1)];
      var $169=(($168)&65535);
      var $170=(($164+($169<<2))|0);
      var $171=$170;
      var $172=$171;
      var $173=(($172)|0);
      var $174=HEAPF32[(($173)>>2)];
      var $175=($161)*($174);
      var $176=$2;
      var $177=(($176+64)|0);
      var $178=HEAP32[(($177)>>2)];
      var $179=$st;
      var $180=(($179+2)|0);
      var $181=$180;
      var $182=HEAP16[(($181)>>1)];
      var $183=(($182)&65535);
      var $184=(($178+($183<<2))|0);
      var $185=$184;
      var $186=$185;
      var $187=(($186+4)|0);
      var $188=HEAPF32[(($187)>>2)];
      var $189=$2;
      var $190=(($189+64)|0);
      var $191=HEAP32[(($190)>>2)];
      var $192=$st;
      var $193=(($192+4)|0);
      var $194=$193;
      var $195=HEAP16[(($194)>>1)];
      var $196=(($195)&65535);
      var $197=(($191+($196<<2))|0);
      var $198=$197;
      var $199=$198;
      var $200=(($199+4)|0);
      var $201=HEAPF32[(($200)>>2)];
      var $202=($188)*($201);
      var $203=($175)+($202);
      var $204=$2;
      var $205=(($204+64)|0);
      var $206=HEAP32[(($205)>>2)];
      var $207=$st;
      var $208=(($207+2)|0);
      var $209=$208;
      var $210=HEAP16[(($209)>>1)];
      var $211=(($210)&65535);
      var $212=(($206+($211<<2))|0);
      var $213=$212;
      var $214=$213;
      var $215=(($214+8)|0);
      var $216=HEAPF32[(($215)>>2)];
      var $217=$2;
      var $218=(($217+64)|0);
      var $219=HEAP32[(($218)>>2)];
      var $220=$st;
      var $221=(($220+4)|0);
      var $222=$221;
      var $223=HEAP16[(($222)>>1)];
      var $224=(($223)&65535);
      var $225=(($219+($224<<2))|0);
      var $226=$225;
      var $227=$226;
      var $228=(($227+8)|0);
      var $229=HEAPF32[(($228)>>2)];
      var $230=($216)*($229);
      var $231=($203)+($230);
      var $232=$2;
      var $233=(($232+64)|0);
      var $234=HEAP32[(($233)>>2)];
      var $235=$st;
      var $236=(($235+6)|0);
      var $237=$236;
      var $238=HEAP16[(($237)>>1)];
      var $239=(($238)&65535);
      var $240=(($234+($239<<2))|0);
      var $241=$240;
      var $242=$241;
      HEAPF32[(($242)>>2)]=$231;
      __label__ = 124; break;
    case 12: 
      var $244=$2;
      var $245=(($244+64)|0);
      var $246=HEAP32[(($245)>>2)];
      var $247=$st;
      var $248=(($247+2)|0);
      var $249=$248;
      var $250=HEAP16[(($249)>>1)];
      var $251=(($250)&65535);
      var $252=(($246+($251<<2))|0);
      var $253=$252;
      var $254=$253;
      var $255=HEAPF32[(($254)>>2)];
      var $256=$2;
      var $257=(($256+64)|0);
      var $258=HEAP32[(($257)>>2)];
      var $259=$st;
      var $260=(($259+4)|0);
      var $261=$260;
      var $262=HEAP16[(($261)>>1)];
      var $263=(($262)&65535);
      var $264=(($258+($263<<2))|0);
      var $265=$264;
      var $266=$265;
      var $267=(($266)|0);
      var $268=HEAPF32[(($267)>>2)];
      var $269=($255)*($268);
      var $270=$2;
      var $271=(($270+64)|0);
      var $272=HEAP32[(($271)>>2)];
      var $273=$st;
      var $274=(($273+6)|0);
      var $275=$274;
      var $276=HEAP16[(($275)>>1)];
      var $277=(($276)&65535);
      var $278=(($272+($277<<2))|0);
      var $279=$278;
      var $280=$279;
      var $281=(($280)|0);
      HEAPF32[(($281)>>2)]=$269;
      var $282=$2;
      var $283=(($282+64)|0);
      var $284=HEAP32[(($283)>>2)];
      var $285=$st;
      var $286=(($285+2)|0);
      var $287=$286;
      var $288=HEAP16[(($287)>>1)];
      var $289=(($288)&65535);
      var $290=(($284+($289<<2))|0);
      var $291=$290;
      var $292=$291;
      var $293=HEAPF32[(($292)>>2)];
      var $294=$2;
      var $295=(($294+64)|0);
      var $296=HEAP32[(($295)>>2)];
      var $297=$st;
      var $298=(($297+4)|0);
      var $299=$298;
      var $300=HEAP16[(($299)>>1)];
      var $301=(($300)&65535);
      var $302=(($296+($301<<2))|0);
      var $303=$302;
      var $304=$303;
      var $305=(($304+4)|0);
      var $306=HEAPF32[(($305)>>2)];
      var $307=($293)*($306);
      var $308=$2;
      var $309=(($308+64)|0);
      var $310=HEAP32[(($309)>>2)];
      var $311=$st;
      var $312=(($311+6)|0);
      var $313=$312;
      var $314=HEAP16[(($313)>>1)];
      var $315=(($314)&65535);
      var $316=(($310+($315<<2))|0);
      var $317=$316;
      var $318=$317;
      var $319=(($318+4)|0);
      HEAPF32[(($319)>>2)]=$307;
      var $320=$2;
      var $321=(($320+64)|0);
      var $322=HEAP32[(($321)>>2)];
      var $323=$st;
      var $324=(($323+2)|0);
      var $325=$324;
      var $326=HEAP16[(($325)>>1)];
      var $327=(($326)&65535);
      var $328=(($322+($327<<2))|0);
      var $329=$328;
      var $330=$329;
      var $331=HEAPF32[(($330)>>2)];
      var $332=$2;
      var $333=(($332+64)|0);
      var $334=HEAP32[(($333)>>2)];
      var $335=$st;
      var $336=(($335+4)|0);
      var $337=$336;
      var $338=HEAP16[(($337)>>1)];
      var $339=(($338)&65535);
      var $340=(($334+($339<<2))|0);
      var $341=$340;
      var $342=$341;
      var $343=(($342+8)|0);
      var $344=HEAPF32[(($343)>>2)];
      var $345=($331)*($344);
      var $346=$2;
      var $347=(($346+64)|0);
      var $348=HEAP32[(($347)>>2)];
      var $349=$st;
      var $350=(($349+6)|0);
      var $351=$350;
      var $352=HEAP16[(($351)>>1)];
      var $353=(($352)&65535);
      var $354=(($348+($353<<2))|0);
      var $355=$354;
      var $356=$355;
      var $357=(($356+8)|0);
      HEAPF32[(($357)>>2)]=$345;
      __label__ = 124; break;
    case 13: 
      var $359=$2;
      var $360=(($359+64)|0);
      var $361=HEAP32[(($360)>>2)];
      var $362=$st;
      var $363=(($362+4)|0);
      var $364=$363;
      var $365=HEAP16[(($364)>>1)];
      var $366=(($365)&65535);
      var $367=(($361+($366<<2))|0);
      var $368=$367;
      var $369=$368;
      var $370=HEAPF32[(($369)>>2)];
      var $371=$2;
      var $372=(($371+64)|0);
      var $373=HEAP32[(($372)>>2)];
      var $374=$st;
      var $375=(($374+2)|0);
      var $376=$375;
      var $377=HEAP16[(($376)>>1)];
      var $378=(($377)&65535);
      var $379=(($373+($378<<2))|0);
      var $380=$379;
      var $381=$380;
      var $382=(($381)|0);
      var $383=HEAPF32[(($382)>>2)];
      var $384=($370)*($383);
      var $385=$2;
      var $386=(($385+64)|0);
      var $387=HEAP32[(($386)>>2)];
      var $388=$st;
      var $389=(($388+6)|0);
      var $390=$389;
      var $391=HEAP16[(($390)>>1)];
      var $392=(($391)&65535);
      var $393=(($387+($392<<2))|0);
      var $394=$393;
      var $395=$394;
      var $396=(($395)|0);
      HEAPF32[(($396)>>2)]=$384;
      var $397=$2;
      var $398=(($397+64)|0);
      var $399=HEAP32[(($398)>>2)];
      var $400=$st;
      var $401=(($400+4)|0);
      var $402=$401;
      var $403=HEAP16[(($402)>>1)];
      var $404=(($403)&65535);
      var $405=(($399+($404<<2))|0);
      var $406=$405;
      var $407=$406;
      var $408=HEAPF32[(($407)>>2)];
      var $409=$2;
      var $410=(($409+64)|0);
      var $411=HEAP32[(($410)>>2)];
      var $412=$st;
      var $413=(($412+2)|0);
      var $414=$413;
      var $415=HEAP16[(($414)>>1)];
      var $416=(($415)&65535);
      var $417=(($411+($416<<2))|0);
      var $418=$417;
      var $419=$418;
      var $420=(($419+4)|0);
      var $421=HEAPF32[(($420)>>2)];
      var $422=($408)*($421);
      var $423=$2;
      var $424=(($423+64)|0);
      var $425=HEAP32[(($424)>>2)];
      var $426=$st;
      var $427=(($426+6)|0);
      var $428=$427;
      var $429=HEAP16[(($428)>>1)];
      var $430=(($429)&65535);
      var $431=(($425+($430<<2))|0);
      var $432=$431;
      var $433=$432;
      var $434=(($433+4)|0);
      HEAPF32[(($434)>>2)]=$422;
      var $435=$2;
      var $436=(($435+64)|0);
      var $437=HEAP32[(($436)>>2)];
      var $438=$st;
      var $439=(($438+4)|0);
      var $440=$439;
      var $441=HEAP16[(($440)>>1)];
      var $442=(($441)&65535);
      var $443=(($437+($442<<2))|0);
      var $444=$443;
      var $445=$444;
      var $446=HEAPF32[(($445)>>2)];
      var $447=$2;
      var $448=(($447+64)|0);
      var $449=HEAP32[(($448)>>2)];
      var $450=$st;
      var $451=(($450+2)|0);
      var $452=$451;
      var $453=HEAP16[(($452)>>1)];
      var $454=(($453)&65535);
      var $455=(($449+($454<<2))|0);
      var $456=$455;
      var $457=$456;
      var $458=(($457+8)|0);
      var $459=HEAPF32[(($458)>>2)];
      var $460=($446)*($459);
      var $461=$2;
      var $462=(($461+64)|0);
      var $463=HEAP32[(($462)>>2)];
      var $464=$st;
      var $465=(($464+6)|0);
      var $466=$465;
      var $467=HEAP16[(($466)>>1)];
      var $468=(($467)&65535);
      var $469=(($463+($468<<2))|0);
      var $470=$469;
      var $471=$470;
      var $472=(($471+8)|0);
      HEAPF32[(($472)>>2)]=$460;
      __label__ = 124; break;
    case 14: 
      var $474=$2;
      var $475=(($474+64)|0);
      var $476=HEAP32[(($475)>>2)];
      var $477=$st;
      var $478=(($477+4)|0);
      var $479=$478;
      var $480=HEAP16[(($479)>>1)];
      var $481=(($480)&65535);
      var $482=(($476+($481<<2))|0);
      var $483=$482;
      var $484=$483;
      var $485=HEAPF32[(($484)>>2)];
      var $486=$485 != 0;
      if ($486) { __label__ = 15; break; } else { __label__ = 16; break; }
    case 15: 
      var $488=$2;
      var $489=(($488+64)|0);
      var $490=HEAP32[(($489)>>2)];
      var $491=$st;
      var $492=(($491+2)|0);
      var $493=$492;
      var $494=HEAP16[(($493)>>1)];
      var $495=(($494)&65535);
      var $496=(($490+($495<<2))|0);
      var $497=$496;
      var $498=$497;
      var $499=HEAPF32[(($498)>>2)];
      var $500=$2;
      var $501=(($500+64)|0);
      var $502=HEAP32[(($501)>>2)];
      var $503=$st;
      var $504=(($503+4)|0);
      var $505=$504;
      var $506=HEAP16[(($505)>>1)];
      var $507=(($506)&65535);
      var $508=(($502+($507<<2))|0);
      var $509=$508;
      var $510=$509;
      var $511=HEAPF32[(($510)>>2)];
      var $512=($499)/($511);
      var $513=$2;
      var $514=(($513+64)|0);
      var $515=HEAP32[(($514)>>2)];
      var $516=$st;
      var $517=(($516+6)|0);
      var $518=$517;
      var $519=HEAP16[(($518)>>1)];
      var $520=(($519)&65535);
      var $521=(($515+($520<<2))|0);
      var $522=$521;
      var $523=$522;
      HEAPF32[(($523)>>2)]=$512;
      __label__ = 17; break;
    case 16: 
      var $525=$2;
      var $526=(($525+64)|0);
      var $527=HEAP32[(($526)>>2)];
      var $528=$st;
      var $529=(($528+6)|0);
      var $530=$529;
      var $531=HEAP16[(($530)>>1)];
      var $532=(($531)&65535);
      var $533=(($527+($532<<2))|0);
      var $534=$533;
      var $535=$534;
      HEAPF32[(($535)>>2)]=0;
      __label__ = 17; break;
    case 17: 
      __label__ = 124; break;
    case 18: 
      var $538=$2;
      var $539=(($538+64)|0);
      var $540=HEAP32[(($539)>>2)];
      var $541=$st;
      var $542=(($541+2)|0);
      var $543=$542;
      var $544=HEAP16[(($543)>>1)];
      var $545=(($544)&65535);
      var $546=(($540+($545<<2))|0);
      var $547=$546;
      var $548=$547;
      var $549=HEAPF32[(($548)>>2)];
      var $550=$2;
      var $551=(($550+64)|0);
      var $552=HEAP32[(($551)>>2)];
      var $553=$st;
      var $554=(($553+4)|0);
      var $555=$554;
      var $556=HEAP16[(($555)>>1)];
      var $557=(($556)&65535);
      var $558=(($552+($557<<2))|0);
      var $559=$558;
      var $560=$559;
      var $561=HEAPF32[(($560)>>2)];
      var $562=($549)+($561);
      var $563=$2;
      var $564=(($563+64)|0);
      var $565=HEAP32[(($564)>>2)];
      var $566=$st;
      var $567=(($566+6)|0);
      var $568=$567;
      var $569=HEAP16[(($568)>>1)];
      var $570=(($569)&65535);
      var $571=(($565+($570<<2))|0);
      var $572=$571;
      var $573=$572;
      HEAPF32[(($573)>>2)]=$562;
      __label__ = 124; break;
    case 19: 
      var $575=$2;
      var $576=(($575+64)|0);
      var $577=HEAP32[(($576)>>2)];
      var $578=$st;
      var $579=(($578+2)|0);
      var $580=$579;
      var $581=HEAP16[(($580)>>1)];
      var $582=(($581)&65535);
      var $583=(($577+($582<<2))|0);
      var $584=$583;
      var $585=$584;
      var $586=(($585)|0);
      var $587=HEAPF32[(($586)>>2)];
      var $588=$2;
      var $589=(($588+64)|0);
      var $590=HEAP32[(($589)>>2)];
      var $591=$st;
      var $592=(($591+4)|0);
      var $593=$592;
      var $594=HEAP16[(($593)>>1)];
      var $595=(($594)&65535);
      var $596=(($590+($595<<2))|0);
      var $597=$596;
      var $598=$597;
      var $599=(($598)|0);
      var $600=HEAPF32[(($599)>>2)];
      var $601=($587)+($600);
      var $602=$2;
      var $603=(($602+64)|0);
      var $604=HEAP32[(($603)>>2)];
      var $605=$st;
      var $606=(($605+6)|0);
      var $607=$606;
      var $608=HEAP16[(($607)>>1)];
      var $609=(($608)&65535);
      var $610=(($604+($609<<2))|0);
      var $611=$610;
      var $612=$611;
      var $613=(($612)|0);
      HEAPF32[(($613)>>2)]=$601;
      var $614=$2;
      var $615=(($614+64)|0);
      var $616=HEAP32[(($615)>>2)];
      var $617=$st;
      var $618=(($617+2)|0);
      var $619=$618;
      var $620=HEAP16[(($619)>>1)];
      var $621=(($620)&65535);
      var $622=(($616+($621<<2))|0);
      var $623=$622;
      var $624=$623;
      var $625=(($624+4)|0);
      var $626=HEAPF32[(($625)>>2)];
      var $627=$2;
      var $628=(($627+64)|0);
      var $629=HEAP32[(($628)>>2)];
      var $630=$st;
      var $631=(($630+4)|0);
      var $632=$631;
      var $633=HEAP16[(($632)>>1)];
      var $634=(($633)&65535);
      var $635=(($629+($634<<2))|0);
      var $636=$635;
      var $637=$636;
      var $638=(($637+4)|0);
      var $639=HEAPF32[(($638)>>2)];
      var $640=($626)+($639);
      var $641=$2;
      var $642=(($641+64)|0);
      var $643=HEAP32[(($642)>>2)];
      var $644=$st;
      var $645=(($644+6)|0);
      var $646=$645;
      var $647=HEAP16[(($646)>>1)];
      var $648=(($647)&65535);
      var $649=(($643+($648<<2))|0);
      var $650=$649;
      var $651=$650;
      var $652=(($651+4)|0);
      HEAPF32[(($652)>>2)]=$640;
      var $653=$2;
      var $654=(($653+64)|0);
      var $655=HEAP32[(($654)>>2)];
      var $656=$st;
      var $657=(($656+2)|0);
      var $658=$657;
      var $659=HEAP16[(($658)>>1)];
      var $660=(($659)&65535);
      var $661=(($655+($660<<2))|0);
      var $662=$661;
      var $663=$662;
      var $664=(($663+8)|0);
      var $665=HEAPF32[(($664)>>2)];
      var $666=$2;
      var $667=(($666+64)|0);
      var $668=HEAP32[(($667)>>2)];
      var $669=$st;
      var $670=(($669+4)|0);
      var $671=$670;
      var $672=HEAP16[(($671)>>1)];
      var $673=(($672)&65535);
      var $674=(($668+($673<<2))|0);
      var $675=$674;
      var $676=$675;
      var $677=(($676+8)|0);
      var $678=HEAPF32[(($677)>>2)];
      var $679=($665)+($678);
      var $680=$2;
      var $681=(($680+64)|0);
      var $682=HEAP32[(($681)>>2)];
      var $683=$st;
      var $684=(($683+6)|0);
      var $685=$684;
      var $686=HEAP16[(($685)>>1)];
      var $687=(($686)&65535);
      var $688=(($682+($687<<2))|0);
      var $689=$688;
      var $690=$689;
      var $691=(($690+8)|0);
      HEAPF32[(($691)>>2)]=$679;
      __label__ = 124; break;
    case 20: 
      var $693=$2;
      var $694=(($693+64)|0);
      var $695=HEAP32[(($694)>>2)];
      var $696=$st;
      var $697=(($696+2)|0);
      var $698=$697;
      var $699=HEAP16[(($698)>>1)];
      var $700=(($699)&65535);
      var $701=(($695+($700<<2))|0);
      var $702=$701;
      var $703=$702;
      var $704=HEAPF32[(($703)>>2)];
      var $705=$2;
      var $706=(($705+64)|0);
      var $707=HEAP32[(($706)>>2)];
      var $708=$st;
      var $709=(($708+4)|0);
      var $710=$709;
      var $711=HEAP16[(($710)>>1)];
      var $712=(($711)&65535);
      var $713=(($707+($712<<2))|0);
      var $714=$713;
      var $715=$714;
      var $716=HEAPF32[(($715)>>2)];
      var $717=($704)-($716);
      var $718=$2;
      var $719=(($718+64)|0);
      var $720=HEAP32[(($719)>>2)];
      var $721=$st;
      var $722=(($721+6)|0);
      var $723=$722;
      var $724=HEAP16[(($723)>>1)];
      var $725=(($724)&65535);
      var $726=(($720+($725<<2))|0);
      var $727=$726;
      var $728=$727;
      HEAPF32[(($728)>>2)]=$717;
      __label__ = 124; break;
    case 21: 
      var $730=$2;
      var $731=(($730+64)|0);
      var $732=HEAP32[(($731)>>2)];
      var $733=$st;
      var $734=(($733+2)|0);
      var $735=$734;
      var $736=HEAP16[(($735)>>1)];
      var $737=(($736)&65535);
      var $738=(($732+($737<<2))|0);
      var $739=$738;
      var $740=$739;
      var $741=(($740)|0);
      var $742=HEAPF32[(($741)>>2)];
      var $743=$2;
      var $744=(($743+64)|0);
      var $745=HEAP32[(($744)>>2)];
      var $746=$st;
      var $747=(($746+4)|0);
      var $748=$747;
      var $749=HEAP16[(($748)>>1)];
      var $750=(($749)&65535);
      var $751=(($745+($750<<2))|0);
      var $752=$751;
      var $753=$752;
      var $754=(($753)|0);
      var $755=HEAPF32[(($754)>>2)];
      var $756=($742)-($755);
      var $757=$2;
      var $758=(($757+64)|0);
      var $759=HEAP32[(($758)>>2)];
      var $760=$st;
      var $761=(($760+6)|0);
      var $762=$761;
      var $763=HEAP16[(($762)>>1)];
      var $764=(($763)&65535);
      var $765=(($759+($764<<2))|0);
      var $766=$765;
      var $767=$766;
      var $768=(($767)|0);
      HEAPF32[(($768)>>2)]=$756;
      var $769=$2;
      var $770=(($769+64)|0);
      var $771=HEAP32[(($770)>>2)];
      var $772=$st;
      var $773=(($772+2)|0);
      var $774=$773;
      var $775=HEAP16[(($774)>>1)];
      var $776=(($775)&65535);
      var $777=(($771+($776<<2))|0);
      var $778=$777;
      var $779=$778;
      var $780=(($779+4)|0);
      var $781=HEAPF32[(($780)>>2)];
      var $782=$2;
      var $783=(($782+64)|0);
      var $784=HEAP32[(($783)>>2)];
      var $785=$st;
      var $786=(($785+4)|0);
      var $787=$786;
      var $788=HEAP16[(($787)>>1)];
      var $789=(($788)&65535);
      var $790=(($784+($789<<2))|0);
      var $791=$790;
      var $792=$791;
      var $793=(($792+4)|0);
      var $794=HEAPF32[(($793)>>2)];
      var $795=($781)-($794);
      var $796=$2;
      var $797=(($796+64)|0);
      var $798=HEAP32[(($797)>>2)];
      var $799=$st;
      var $800=(($799+6)|0);
      var $801=$800;
      var $802=HEAP16[(($801)>>1)];
      var $803=(($802)&65535);
      var $804=(($798+($803<<2))|0);
      var $805=$804;
      var $806=$805;
      var $807=(($806+4)|0);
      HEAPF32[(($807)>>2)]=$795;
      var $808=$2;
      var $809=(($808+64)|0);
      var $810=HEAP32[(($809)>>2)];
      var $811=$st;
      var $812=(($811+2)|0);
      var $813=$812;
      var $814=HEAP16[(($813)>>1)];
      var $815=(($814)&65535);
      var $816=(($810+($815<<2))|0);
      var $817=$816;
      var $818=$817;
      var $819=(($818+8)|0);
      var $820=HEAPF32[(($819)>>2)];
      var $821=$2;
      var $822=(($821+64)|0);
      var $823=HEAP32[(($822)>>2)];
      var $824=$st;
      var $825=(($824+4)|0);
      var $826=$825;
      var $827=HEAP16[(($826)>>1)];
      var $828=(($827)&65535);
      var $829=(($823+($828<<2))|0);
      var $830=$829;
      var $831=$830;
      var $832=(($831+8)|0);
      var $833=HEAPF32[(($832)>>2)];
      var $834=($820)-($833);
      var $835=$2;
      var $836=(($835+64)|0);
      var $837=HEAP32[(($836)>>2)];
      var $838=$st;
      var $839=(($838+6)|0);
      var $840=$839;
      var $841=HEAP16[(($840)>>1)];
      var $842=(($841)&65535);
      var $843=(($837+($842<<2))|0);
      var $844=$843;
      var $845=$844;
      var $846=(($845+8)|0);
      HEAPF32[(($846)>>2)]=$834;
      __label__ = 124; break;
    case 22: 
      var $848=$2;
      var $849=(($848+64)|0);
      var $850=HEAP32[(($849)>>2)];
      var $851=$st;
      var $852=(($851+2)|0);
      var $853=$852;
      var $854=HEAP16[(($853)>>1)];
      var $855=(($854)&65535);
      var $856=(($850+($855<<2))|0);
      var $857=$856;
      var $858=$857;
      var $859=HEAPF32[(($858)>>2)];
      var $860=$2;
      var $861=(($860+64)|0);
      var $862=HEAP32[(($861)>>2)];
      var $863=$st;
      var $864=(($863+4)|0);
      var $865=$864;
      var $866=HEAP16[(($865)>>1)];
      var $867=(($866)&65535);
      var $868=(($862+($867<<2))|0);
      var $869=$868;
      var $870=$869;
      var $871=HEAPF32[(($870)>>2)];
      var $872=$859 == $871;
      var $873=(($872)&1);
      var $874=(($873)|0);
      var $875=$2;
      var $876=(($875+64)|0);
      var $877=HEAP32[(($876)>>2)];
      var $878=$st;
      var $879=(($878+6)|0);
      var $880=$879;
      var $881=HEAP16[(($880)>>1)];
      var $882=(($881)&65535);
      var $883=(($877+($882<<2))|0);
      var $884=$883;
      var $885=$884;
      HEAPF32[(($885)>>2)]=$874;
      __label__ = 124; break;
    case 23: 
      var $887=$2;
      var $888=(($887+64)|0);
      var $889=HEAP32[(($888)>>2)];
      var $890=$st;
      var $891=(($890+2)|0);
      var $892=$891;
      var $893=HEAP16[(($892)>>1)];
      var $894=(($893)&65535);
      var $895=(($889+($894<<2))|0);
      var $896=$895;
      var $897=$896;
      var $898=(($897)|0);
      var $899=HEAPF32[(($898)>>2)];
      var $900=$2;
      var $901=(($900+64)|0);
      var $902=HEAP32[(($901)>>2)];
      var $903=$st;
      var $904=(($903+4)|0);
      var $905=$904;
      var $906=HEAP16[(($905)>>1)];
      var $907=(($906)&65535);
      var $908=(($902+($907<<2))|0);
      var $909=$908;
      var $910=$909;
      var $911=(($910)|0);
      var $912=HEAPF32[(($911)>>2)];
      var $913=$899 == $912;
      if ($913) { __label__ = 24; break; } else { var $971 = 0;__label__ = 26; break; }
    case 24: 
      var $915=$2;
      var $916=(($915+64)|0);
      var $917=HEAP32[(($916)>>2)];
      var $918=$st;
      var $919=(($918+2)|0);
      var $920=$919;
      var $921=HEAP16[(($920)>>1)];
      var $922=(($921)&65535);
      var $923=(($917+($922<<2))|0);
      var $924=$923;
      var $925=$924;
      var $926=(($925+4)|0);
      var $927=HEAPF32[(($926)>>2)];
      var $928=$2;
      var $929=(($928+64)|0);
      var $930=HEAP32[(($929)>>2)];
      var $931=$st;
      var $932=(($931+4)|0);
      var $933=$932;
      var $934=HEAP16[(($933)>>1)];
      var $935=(($934)&65535);
      var $936=(($930+($935<<2))|0);
      var $937=$936;
      var $938=$937;
      var $939=(($938+4)|0);
      var $940=HEAPF32[(($939)>>2)];
      var $941=$927 == $940;
      if ($941) { __label__ = 25; break; } else { var $971 = 0;__label__ = 26; break; }
    case 25: 
      var $943=$2;
      var $944=(($943+64)|0);
      var $945=HEAP32[(($944)>>2)];
      var $946=$st;
      var $947=(($946+2)|0);
      var $948=$947;
      var $949=HEAP16[(($948)>>1)];
      var $950=(($949)&65535);
      var $951=(($945+($950<<2))|0);
      var $952=$951;
      var $953=$952;
      var $954=(($953+8)|0);
      var $955=HEAPF32[(($954)>>2)];
      var $956=$2;
      var $957=(($956+64)|0);
      var $958=HEAP32[(($957)>>2)];
      var $959=$st;
      var $960=(($959+4)|0);
      var $961=$960;
      var $962=HEAP16[(($961)>>1)];
      var $963=(($962)&65535);
      var $964=(($958+($963<<2))|0);
      var $965=$964;
      var $966=$965;
      var $967=(($966+8)|0);
      var $968=HEAPF32[(($967)>>2)];
      var $969=$955 == $968;
      var $971 = $969;__label__ = 26; break;
    case 26: 
      var $971;
      var $972=(($971)&1);
      var $973=(($972)|0);
      var $974=$2;
      var $975=(($974+64)|0);
      var $976=HEAP32[(($975)>>2)];
      var $977=$st;
      var $978=(($977+6)|0);
      var $979=$978;
      var $980=HEAP16[(($979)>>1)];
      var $981=(($980)&65535);
      var $982=(($976+($981<<2))|0);
      var $983=$982;
      var $984=$983;
      HEAPF32[(($984)>>2)]=$973;
      __label__ = 124; break;
    case 27: 
      var $986=$2;
      var $987=$2;
      var $988=(($987+64)|0);
      var $989=HEAP32[(($988)>>2)];
      var $990=$st;
      var $991=(($990+2)|0);
      var $992=$991;
      var $993=HEAP16[(($992)>>1)];
      var $994=(($993)&65535);
      var $995=(($989+($994<<2))|0);
      var $996=$995;
      var $997=$996;
      var $998=HEAP32[(($997)>>2)];
      var $999=_prog_getstring($986, $998);
      var $1000=$2;
      var $1001=$2;
      var $1002=(($1001+64)|0);
      var $1003=HEAP32[(($1002)>>2)];
      var $1004=$st;
      var $1005=(($1004+4)|0);
      var $1006=$1005;
      var $1007=HEAP16[(($1006)>>1)];
      var $1008=(($1007)&65535);
      var $1009=(($1003+($1008<<2))|0);
      var $1010=$1009;
      var $1011=$1010;
      var $1012=HEAP32[(($1011)>>2)];
      var $1013=_prog_getstring($1000, $1012);
      var $1014=_strcmp($999, $1013);
      var $1015=(($1014)|0)!=0;
      var $1016=$1015 ^ 1;
      var $1017=(($1016)&1);
      var $1018=(($1017)|0);
      var $1019=$2;
      var $1020=(($1019+64)|0);
      var $1021=HEAP32[(($1020)>>2)];
      var $1022=$st;
      var $1023=(($1022+6)|0);
      var $1024=$1023;
      var $1025=HEAP16[(($1024)>>1)];
      var $1026=(($1025)&65535);
      var $1027=(($1021+($1026<<2))|0);
      var $1028=$1027;
      var $1029=$1028;
      HEAPF32[(($1029)>>2)]=$1018;
      __label__ = 124; break;
    case 28: 
      var $1031=$2;
      var $1032=(($1031+64)|0);
      var $1033=HEAP32[(($1032)>>2)];
      var $1034=$st;
      var $1035=(($1034+2)|0);
      var $1036=$1035;
      var $1037=HEAP16[(($1036)>>1)];
      var $1038=(($1037)&65535);
      var $1039=(($1033+($1038<<2))|0);
      var $1040=$1039;
      var $1041=$1040;
      var $1042=HEAP32[(($1041)>>2)];
      var $1043=$2;
      var $1044=(($1043+64)|0);
      var $1045=HEAP32[(($1044)>>2)];
      var $1046=$st;
      var $1047=(($1046+4)|0);
      var $1048=$1047;
      var $1049=HEAP16[(($1048)>>1)];
      var $1050=(($1049)&65535);
      var $1051=(($1045+($1050<<2))|0);
      var $1052=$1051;
      var $1053=$1052;
      var $1054=HEAP32[(($1053)>>2)];
      var $1055=(($1042)|0)==(($1054)|0);
      var $1056=(($1055)&1);
      var $1057=(($1056)|0);
      var $1058=$2;
      var $1059=(($1058+64)|0);
      var $1060=HEAP32[(($1059)>>2)];
      var $1061=$st;
      var $1062=(($1061+6)|0);
      var $1063=$1062;
      var $1064=HEAP16[(($1063)>>1)];
      var $1065=(($1064)&65535);
      var $1066=(($1060+($1065<<2))|0);
      var $1067=$1066;
      var $1068=$1067;
      HEAPF32[(($1068)>>2)]=$1057;
      __label__ = 124; break;
    case 29: 
      var $1070=$2;
      var $1071=(($1070+64)|0);
      var $1072=HEAP32[(($1071)>>2)];
      var $1073=$st;
      var $1074=(($1073+2)|0);
      var $1075=$1074;
      var $1076=HEAP16[(($1075)>>1)];
      var $1077=(($1076)&65535);
      var $1078=(($1072+($1077<<2))|0);
      var $1079=$1078;
      var $1080=$1079;
      var $1081=HEAP32[(($1080)>>2)];
      var $1082=$2;
      var $1083=(($1082+64)|0);
      var $1084=HEAP32[(($1083)>>2)];
      var $1085=$st;
      var $1086=(($1085+4)|0);
      var $1087=$1086;
      var $1088=HEAP16[(($1087)>>1)];
      var $1089=(($1088)&65535);
      var $1090=(($1084+($1089<<2))|0);
      var $1091=$1090;
      var $1092=$1091;
      var $1093=HEAP32[(($1092)>>2)];
      var $1094=(($1081)|0)==(($1093)|0);
      var $1095=(($1094)&1);
      var $1096=(($1095)|0);
      var $1097=$2;
      var $1098=(($1097+64)|0);
      var $1099=HEAP32[(($1098)>>2)];
      var $1100=$st;
      var $1101=(($1100+6)|0);
      var $1102=$1101;
      var $1103=HEAP16[(($1102)>>1)];
      var $1104=(($1103)&65535);
      var $1105=(($1099+($1104<<2))|0);
      var $1106=$1105;
      var $1107=$1106;
      HEAPF32[(($1107)>>2)]=$1096;
      __label__ = 124; break;
    case 30: 
      var $1109=$2;
      var $1110=(($1109+64)|0);
      var $1111=HEAP32[(($1110)>>2)];
      var $1112=$st;
      var $1113=(($1112+2)|0);
      var $1114=$1113;
      var $1115=HEAP16[(($1114)>>1)];
      var $1116=(($1115)&65535);
      var $1117=(($1111+($1116<<2))|0);
      var $1118=$1117;
      var $1119=$1118;
      var $1120=HEAPF32[(($1119)>>2)];
      var $1121=$2;
      var $1122=(($1121+64)|0);
      var $1123=HEAP32[(($1122)>>2)];
      var $1124=$st;
      var $1125=(($1124+4)|0);
      var $1126=$1125;
      var $1127=HEAP16[(($1126)>>1)];
      var $1128=(($1127)&65535);
      var $1129=(($1123+($1128<<2))|0);
      var $1130=$1129;
      var $1131=$1130;
      var $1132=HEAPF32[(($1131)>>2)];
      var $1133=$1120 != $1132;
      var $1134=(($1133)&1);
      var $1135=(($1134)|0);
      var $1136=$2;
      var $1137=(($1136+64)|0);
      var $1138=HEAP32[(($1137)>>2)];
      var $1139=$st;
      var $1140=(($1139+6)|0);
      var $1141=$1140;
      var $1142=HEAP16[(($1141)>>1)];
      var $1143=(($1142)&65535);
      var $1144=(($1138+($1143<<2))|0);
      var $1145=$1144;
      var $1146=$1145;
      HEAPF32[(($1146)>>2)]=$1135;
      __label__ = 124; break;
    case 31: 
      var $1148=$2;
      var $1149=(($1148+64)|0);
      var $1150=HEAP32[(($1149)>>2)];
      var $1151=$st;
      var $1152=(($1151+2)|0);
      var $1153=$1152;
      var $1154=HEAP16[(($1153)>>1)];
      var $1155=(($1154)&65535);
      var $1156=(($1150+($1155<<2))|0);
      var $1157=$1156;
      var $1158=$1157;
      var $1159=(($1158)|0);
      var $1160=HEAPF32[(($1159)>>2)];
      var $1161=$2;
      var $1162=(($1161+64)|0);
      var $1163=HEAP32[(($1162)>>2)];
      var $1164=$st;
      var $1165=(($1164+4)|0);
      var $1166=$1165;
      var $1167=HEAP16[(($1166)>>1)];
      var $1168=(($1167)&65535);
      var $1169=(($1163+($1168<<2))|0);
      var $1170=$1169;
      var $1171=$1170;
      var $1172=(($1171)|0);
      var $1173=HEAPF32[(($1172)>>2)];
      var $1174=$1160 != $1173;
      if ($1174) { var $1232 = 1;__label__ = 34; break; } else { __label__ = 32; break; }
    case 32: 
      var $1176=$2;
      var $1177=(($1176+64)|0);
      var $1178=HEAP32[(($1177)>>2)];
      var $1179=$st;
      var $1180=(($1179+2)|0);
      var $1181=$1180;
      var $1182=HEAP16[(($1181)>>1)];
      var $1183=(($1182)&65535);
      var $1184=(($1178+($1183<<2))|0);
      var $1185=$1184;
      var $1186=$1185;
      var $1187=(($1186+4)|0);
      var $1188=HEAPF32[(($1187)>>2)];
      var $1189=$2;
      var $1190=(($1189+64)|0);
      var $1191=HEAP32[(($1190)>>2)];
      var $1192=$st;
      var $1193=(($1192+4)|0);
      var $1194=$1193;
      var $1195=HEAP16[(($1194)>>1)];
      var $1196=(($1195)&65535);
      var $1197=(($1191+($1196<<2))|0);
      var $1198=$1197;
      var $1199=$1198;
      var $1200=(($1199+4)|0);
      var $1201=HEAPF32[(($1200)>>2)];
      var $1202=$1188 != $1201;
      if ($1202) { var $1232 = 1;__label__ = 34; break; } else { __label__ = 33; break; }
    case 33: 
      var $1204=$2;
      var $1205=(($1204+64)|0);
      var $1206=HEAP32[(($1205)>>2)];
      var $1207=$st;
      var $1208=(($1207+2)|0);
      var $1209=$1208;
      var $1210=HEAP16[(($1209)>>1)];
      var $1211=(($1210)&65535);
      var $1212=(($1206+($1211<<2))|0);
      var $1213=$1212;
      var $1214=$1213;
      var $1215=(($1214+8)|0);
      var $1216=HEAPF32[(($1215)>>2)];
      var $1217=$2;
      var $1218=(($1217+64)|0);
      var $1219=HEAP32[(($1218)>>2)];
      var $1220=$st;
      var $1221=(($1220+4)|0);
      var $1222=$1221;
      var $1223=HEAP16[(($1222)>>1)];
      var $1224=(($1223)&65535);
      var $1225=(($1219+($1224<<2))|0);
      var $1226=$1225;
      var $1227=$1226;
      var $1228=(($1227+8)|0);
      var $1229=HEAPF32[(($1228)>>2)];
      var $1230=$1216 != $1229;
      var $1232 = $1230;__label__ = 34; break;
    case 34: 
      var $1232;
      var $1233=(($1232)&1);
      var $1234=(($1233)|0);
      var $1235=$2;
      var $1236=(($1235+64)|0);
      var $1237=HEAP32[(($1236)>>2)];
      var $1238=$st;
      var $1239=(($1238+6)|0);
      var $1240=$1239;
      var $1241=HEAP16[(($1240)>>1)];
      var $1242=(($1241)&65535);
      var $1243=(($1237+($1242<<2))|0);
      var $1244=$1243;
      var $1245=$1244;
      HEAPF32[(($1245)>>2)]=$1234;
      __label__ = 124; break;
    case 35: 
      var $1247=$2;
      var $1248=$2;
      var $1249=(($1248+64)|0);
      var $1250=HEAP32[(($1249)>>2)];
      var $1251=$st;
      var $1252=(($1251+2)|0);
      var $1253=$1252;
      var $1254=HEAP16[(($1253)>>1)];
      var $1255=(($1254)&65535);
      var $1256=(($1250+($1255<<2))|0);
      var $1257=$1256;
      var $1258=$1257;
      var $1259=HEAP32[(($1258)>>2)];
      var $1260=_prog_getstring($1247, $1259);
      var $1261=$2;
      var $1262=$2;
      var $1263=(($1262+64)|0);
      var $1264=HEAP32[(($1263)>>2)];
      var $1265=$st;
      var $1266=(($1265+4)|0);
      var $1267=$1266;
      var $1268=HEAP16[(($1267)>>1)];
      var $1269=(($1268)&65535);
      var $1270=(($1264+($1269<<2))|0);
      var $1271=$1270;
      var $1272=$1271;
      var $1273=HEAP32[(($1272)>>2)];
      var $1274=_prog_getstring($1261, $1273);
      var $1275=_strcmp($1260, $1274);
      var $1276=(($1275)|0)!=0;
      var $1277=$1276 ^ 1;
      var $1278=$1277 ^ 1;
      var $1279=(($1278)&1);
      var $1280=(($1279)|0);
      var $1281=$2;
      var $1282=(($1281+64)|0);
      var $1283=HEAP32[(($1282)>>2)];
      var $1284=$st;
      var $1285=(($1284+6)|0);
      var $1286=$1285;
      var $1287=HEAP16[(($1286)>>1)];
      var $1288=(($1287)&65535);
      var $1289=(($1283+($1288<<2))|0);
      var $1290=$1289;
      var $1291=$1290;
      HEAPF32[(($1291)>>2)]=$1280;
      __label__ = 124; break;
    case 36: 
      var $1293=$2;
      var $1294=(($1293+64)|0);
      var $1295=HEAP32[(($1294)>>2)];
      var $1296=$st;
      var $1297=(($1296+2)|0);
      var $1298=$1297;
      var $1299=HEAP16[(($1298)>>1)];
      var $1300=(($1299)&65535);
      var $1301=(($1295+($1300<<2))|0);
      var $1302=$1301;
      var $1303=$1302;
      var $1304=HEAP32[(($1303)>>2)];
      var $1305=$2;
      var $1306=(($1305+64)|0);
      var $1307=HEAP32[(($1306)>>2)];
      var $1308=$st;
      var $1309=(($1308+4)|0);
      var $1310=$1309;
      var $1311=HEAP16[(($1310)>>1)];
      var $1312=(($1311)&65535);
      var $1313=(($1307+($1312<<2))|0);
      var $1314=$1313;
      var $1315=$1314;
      var $1316=HEAP32[(($1315)>>2)];
      var $1317=(($1304)|0)!=(($1316)|0);
      var $1318=(($1317)&1);
      var $1319=(($1318)|0);
      var $1320=$2;
      var $1321=(($1320+64)|0);
      var $1322=HEAP32[(($1321)>>2)];
      var $1323=$st;
      var $1324=(($1323+6)|0);
      var $1325=$1324;
      var $1326=HEAP16[(($1325)>>1)];
      var $1327=(($1326)&65535);
      var $1328=(($1322+($1327<<2))|0);
      var $1329=$1328;
      var $1330=$1329;
      HEAPF32[(($1330)>>2)]=$1319;
      __label__ = 124; break;
    case 37: 
      var $1332=$2;
      var $1333=(($1332+64)|0);
      var $1334=HEAP32[(($1333)>>2)];
      var $1335=$st;
      var $1336=(($1335+2)|0);
      var $1337=$1336;
      var $1338=HEAP16[(($1337)>>1)];
      var $1339=(($1338)&65535);
      var $1340=(($1334+($1339<<2))|0);
      var $1341=$1340;
      var $1342=$1341;
      var $1343=HEAP32[(($1342)>>2)];
      var $1344=$2;
      var $1345=(($1344+64)|0);
      var $1346=HEAP32[(($1345)>>2)];
      var $1347=$st;
      var $1348=(($1347+4)|0);
      var $1349=$1348;
      var $1350=HEAP16[(($1349)>>1)];
      var $1351=(($1350)&65535);
      var $1352=(($1346+($1351<<2))|0);
      var $1353=$1352;
      var $1354=$1353;
      var $1355=HEAP32[(($1354)>>2)];
      var $1356=(($1343)|0)!=(($1355)|0);
      var $1357=(($1356)&1);
      var $1358=(($1357)|0);
      var $1359=$2;
      var $1360=(($1359+64)|0);
      var $1361=HEAP32[(($1360)>>2)];
      var $1362=$st;
      var $1363=(($1362+6)|0);
      var $1364=$1363;
      var $1365=HEAP16[(($1364)>>1)];
      var $1366=(($1365)&65535);
      var $1367=(($1361+($1366<<2))|0);
      var $1368=$1367;
      var $1369=$1368;
      HEAPF32[(($1369)>>2)]=$1358;
      __label__ = 124; break;
    case 38: 
      var $1371=$2;
      var $1372=(($1371+64)|0);
      var $1373=HEAP32[(($1372)>>2)];
      var $1374=$st;
      var $1375=(($1374+2)|0);
      var $1376=$1375;
      var $1377=HEAP16[(($1376)>>1)];
      var $1378=(($1377)&65535);
      var $1379=(($1373+($1378<<2))|0);
      var $1380=$1379;
      var $1381=$1380;
      var $1382=HEAPF32[(($1381)>>2)];
      var $1383=$2;
      var $1384=(($1383+64)|0);
      var $1385=HEAP32[(($1384)>>2)];
      var $1386=$st;
      var $1387=(($1386+4)|0);
      var $1388=$1387;
      var $1389=HEAP16[(($1388)>>1)];
      var $1390=(($1389)&65535);
      var $1391=(($1385+($1390<<2))|0);
      var $1392=$1391;
      var $1393=$1392;
      var $1394=HEAPF32[(($1393)>>2)];
      var $1395=$1382 <= $1394;
      var $1396=(($1395)&1);
      var $1397=(($1396)|0);
      var $1398=$2;
      var $1399=(($1398+64)|0);
      var $1400=HEAP32[(($1399)>>2)];
      var $1401=$st;
      var $1402=(($1401+6)|0);
      var $1403=$1402;
      var $1404=HEAP16[(($1403)>>1)];
      var $1405=(($1404)&65535);
      var $1406=(($1400+($1405<<2))|0);
      var $1407=$1406;
      var $1408=$1407;
      HEAPF32[(($1408)>>2)]=$1397;
      __label__ = 124; break;
    case 39: 
      var $1410=$2;
      var $1411=(($1410+64)|0);
      var $1412=HEAP32[(($1411)>>2)];
      var $1413=$st;
      var $1414=(($1413+2)|0);
      var $1415=$1414;
      var $1416=HEAP16[(($1415)>>1)];
      var $1417=(($1416)&65535);
      var $1418=(($1412+($1417<<2))|0);
      var $1419=$1418;
      var $1420=$1419;
      var $1421=HEAPF32[(($1420)>>2)];
      var $1422=$2;
      var $1423=(($1422+64)|0);
      var $1424=HEAP32[(($1423)>>2)];
      var $1425=$st;
      var $1426=(($1425+4)|0);
      var $1427=$1426;
      var $1428=HEAP16[(($1427)>>1)];
      var $1429=(($1428)&65535);
      var $1430=(($1424+($1429<<2))|0);
      var $1431=$1430;
      var $1432=$1431;
      var $1433=HEAPF32[(($1432)>>2)];
      var $1434=$1421 >= $1433;
      var $1435=(($1434)&1);
      var $1436=(($1435)|0);
      var $1437=$2;
      var $1438=(($1437+64)|0);
      var $1439=HEAP32[(($1438)>>2)];
      var $1440=$st;
      var $1441=(($1440+6)|0);
      var $1442=$1441;
      var $1443=HEAP16[(($1442)>>1)];
      var $1444=(($1443)&65535);
      var $1445=(($1439+($1444<<2))|0);
      var $1446=$1445;
      var $1447=$1446;
      HEAPF32[(($1447)>>2)]=$1436;
      __label__ = 124; break;
    case 40: 
      var $1449=$2;
      var $1450=(($1449+64)|0);
      var $1451=HEAP32[(($1450)>>2)];
      var $1452=$st;
      var $1453=(($1452+2)|0);
      var $1454=$1453;
      var $1455=HEAP16[(($1454)>>1)];
      var $1456=(($1455)&65535);
      var $1457=(($1451+($1456<<2))|0);
      var $1458=$1457;
      var $1459=$1458;
      var $1460=HEAPF32[(($1459)>>2)];
      var $1461=$2;
      var $1462=(($1461+64)|0);
      var $1463=HEAP32[(($1462)>>2)];
      var $1464=$st;
      var $1465=(($1464+4)|0);
      var $1466=$1465;
      var $1467=HEAP16[(($1466)>>1)];
      var $1468=(($1467)&65535);
      var $1469=(($1463+($1468<<2))|0);
      var $1470=$1469;
      var $1471=$1470;
      var $1472=HEAPF32[(($1471)>>2)];
      var $1473=$1460 < $1472;
      var $1474=(($1473)&1);
      var $1475=(($1474)|0);
      var $1476=$2;
      var $1477=(($1476+64)|0);
      var $1478=HEAP32[(($1477)>>2)];
      var $1479=$st;
      var $1480=(($1479+6)|0);
      var $1481=$1480;
      var $1482=HEAP16[(($1481)>>1)];
      var $1483=(($1482)&65535);
      var $1484=(($1478+($1483<<2))|0);
      var $1485=$1484;
      var $1486=$1485;
      HEAPF32[(($1486)>>2)]=$1475;
      __label__ = 124; break;
    case 41: 
      var $1488=$2;
      var $1489=(($1488+64)|0);
      var $1490=HEAP32[(($1489)>>2)];
      var $1491=$st;
      var $1492=(($1491+2)|0);
      var $1493=$1492;
      var $1494=HEAP16[(($1493)>>1)];
      var $1495=(($1494)&65535);
      var $1496=(($1490+($1495<<2))|0);
      var $1497=$1496;
      var $1498=$1497;
      var $1499=HEAPF32[(($1498)>>2)];
      var $1500=$2;
      var $1501=(($1500+64)|0);
      var $1502=HEAP32[(($1501)>>2)];
      var $1503=$st;
      var $1504=(($1503+4)|0);
      var $1505=$1504;
      var $1506=HEAP16[(($1505)>>1)];
      var $1507=(($1506)&65535);
      var $1508=(($1502+($1507<<2))|0);
      var $1509=$1508;
      var $1510=$1509;
      var $1511=HEAPF32[(($1510)>>2)];
      var $1512=$1499 > $1511;
      var $1513=(($1512)&1);
      var $1514=(($1513)|0);
      var $1515=$2;
      var $1516=(($1515+64)|0);
      var $1517=HEAP32[(($1516)>>2)];
      var $1518=$st;
      var $1519=(($1518+6)|0);
      var $1520=$1519;
      var $1521=HEAP16[(($1520)>>1)];
      var $1522=(($1521)&65535);
      var $1523=(($1517+($1522<<2))|0);
      var $1524=$1523;
      var $1525=$1524;
      HEAPF32[(($1525)>>2)]=$1514;
      __label__ = 124; break;
    case 42: 
      var $1527=$2;
      var $1528=(($1527+64)|0);
      var $1529=HEAP32[(($1528)>>2)];
      var $1530=$st;
      var $1531=(($1530+2)|0);
      var $1532=$1531;
      var $1533=HEAP16[(($1532)>>1)];
      var $1534=(($1533)&65535);
      var $1535=(($1529+($1534<<2))|0);
      var $1536=$1535;
      var $1537=$1536;
      var $1538=HEAP32[(($1537)>>2)];
      var $1539=(($1538)|0) < 0;
      if ($1539) { __label__ = 44; break; } else { __label__ = 43; break; }
    case 43: 
      var $1541=$2;
      var $1542=(($1541+64)|0);
      var $1543=HEAP32[(($1542)>>2)];
      var $1544=$st;
      var $1545=(($1544+2)|0);
      var $1546=$1545;
      var $1547=HEAP16[(($1546)>>1)];
      var $1548=(($1547)&65535);
      var $1549=(($1543+($1548<<2))|0);
      var $1550=$1549;
      var $1551=$1550;
      var $1552=HEAP32[(($1551)>>2)];
      var $1553=$2;
      var $1554=(($1553+140)|0);
      var $1555=HEAP32[(($1554)>>2)];
      var $1556=(($1552)|0) >= (($1555)|0);
      if ($1556) { __label__ = 44; break; } else { __label__ = 45; break; }
    case 44: 
      var $1558=$2;
      var $1559=$2;
      var $1560=(($1559)|0);
      var $1561=HEAP32[(($1560)>>2)];
      _qcvmerror($1558, ((STRING_TABLE.__str17)|0), (tempInt=STACKTOP,STACKTOP += 4,assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),HEAP32[((tempInt)>>2)]=$1561,tempInt));
      __label__ = 488; break;
    case 45: 
      var $1563=$2;
      var $1564=(($1563+64)|0);
      var $1565=HEAP32[(($1564)>>2)];
      var $1566=$st;
      var $1567=(($1566+4)|0);
      var $1568=$1567;
      var $1569=HEAP16[(($1568)>>1)];
      var $1570=(($1569)&65535);
      var $1571=(($1565+($1570<<2))|0);
      var $1572=$1571;
      var $1573=$1572;
      var $1574=HEAP32[(($1573)>>2)];
      var $1575=$2;
      var $1576=(($1575+144)|0);
      var $1577=HEAP32[(($1576)>>2)];
      var $1578=(($1574)>>>0) >= (($1577)>>>0);
      if ($1578) { __label__ = 46; break; } else { __label__ = 47; break; }
    case 46: 
      var $1580=$2;
      var $1581=$2;
      var $1582=(($1581)|0);
      var $1583=HEAP32[(($1582)>>2)];
      var $1584=$2;
      var $1585=(($1584+64)|0);
      var $1586=HEAP32[(($1585)>>2)];
      var $1587=$st;
      var $1588=(($1587+4)|0);
      var $1589=$1588;
      var $1590=HEAP16[(($1589)>>1)];
      var $1591=(($1590)&65535);
      var $1592=(($1586+($1591<<2))|0);
      var $1593=$1592;
      var $1594=$1593;
      var $1595=HEAP32[(($1594)>>2)];
      _qcvmerror($1580, ((STRING_TABLE.__str18)|0), (tempInt=STACKTOP,STACKTOP += 8,assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),HEAP32[((tempInt)>>2)]=$1583,HEAP32[(((tempInt)+(4))>>2)]=$1595,tempInt));
      __label__ = 488; break;
    case 47: 
      var $1597=$2;
      var $1598=$2;
      var $1599=(($1598+64)|0);
      var $1600=HEAP32[(($1599)>>2)];
      var $1601=$st;
      var $1602=(($1601+2)|0);
      var $1603=$1602;
      var $1604=HEAP16[(($1603)>>1)];
      var $1605=(($1604)&65535);
      var $1606=(($1600+($1605<<2))|0);
      var $1607=$1606;
      var $1608=$1607;
      var $1609=HEAP32[(($1608)>>2)];
      var $1610=_prog_getedict($1597, $1609);
      $ed=$1610;
      var $1611=$ed;
      var $1612=$1611;
      var $1613=$2;
      var $1614=(($1613+64)|0);
      var $1615=HEAP32[(($1614)>>2)];
      var $1616=$st;
      var $1617=(($1616+4)|0);
      var $1618=$1617;
      var $1619=HEAP16[(($1618)>>1)];
      var $1620=(($1619)&65535);
      var $1621=(($1615+($1620<<2))|0);
      var $1622=$1621;
      var $1623=$1622;
      var $1624=HEAP32[(($1623)>>2)];
      var $1625=(($1612+($1624<<2))|0);
      var $1626=$1625;
      var $1627=$1626;
      var $1628=HEAP32[(($1627)>>2)];
      var $1629=$2;
      var $1630=(($1629+64)|0);
      var $1631=HEAP32[(($1630)>>2)];
      var $1632=$st;
      var $1633=(($1632+6)|0);
      var $1634=$1633;
      var $1635=HEAP16[(($1634)>>1)];
      var $1636=(($1635)&65535);
      var $1637=(($1631+($1636<<2))|0);
      var $1638=$1637;
      var $1639=$1638;
      HEAP32[(($1639)>>2)]=$1628;
      __label__ = 124; break;
    case 48: 
      var $1641=$2;
      var $1642=(($1641+64)|0);
      var $1643=HEAP32[(($1642)>>2)];
      var $1644=$st;
      var $1645=(($1644+2)|0);
      var $1646=$1645;
      var $1647=HEAP16[(($1646)>>1)];
      var $1648=(($1647)&65535);
      var $1649=(($1643+($1648<<2))|0);
      var $1650=$1649;
      var $1651=$1650;
      var $1652=HEAP32[(($1651)>>2)];
      var $1653=(($1652)|0) < 0;
      if ($1653) { __label__ = 50; break; } else { __label__ = 49; break; }
    case 49: 
      var $1655=$2;
      var $1656=(($1655+64)|0);
      var $1657=HEAP32[(($1656)>>2)];
      var $1658=$st;
      var $1659=(($1658+2)|0);
      var $1660=$1659;
      var $1661=HEAP16[(($1660)>>1)];
      var $1662=(($1661)&65535);
      var $1663=(($1657+($1662<<2))|0);
      var $1664=$1663;
      var $1665=$1664;
      var $1666=HEAP32[(($1665)>>2)];
      var $1667=$2;
      var $1668=(($1667+140)|0);
      var $1669=HEAP32[(($1668)>>2)];
      var $1670=(($1666)|0) >= (($1669)|0);
      if ($1670) { __label__ = 50; break; } else { __label__ = 51; break; }
    case 50: 
      var $1672=$2;
      var $1673=$2;
      var $1674=(($1673)|0);
      var $1675=HEAP32[(($1674)>>2)];
      _qcvmerror($1672, ((STRING_TABLE.__str17)|0), (tempInt=STACKTOP,STACKTOP += 4,assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),HEAP32[((tempInt)>>2)]=$1675,tempInt));
      __label__ = 488; break;
    case 51: 
      var $1677=$2;
      var $1678=(($1677+64)|0);
      var $1679=HEAP32[(($1678)>>2)];
      var $1680=$st;
      var $1681=(($1680+4)|0);
      var $1682=$1681;
      var $1683=HEAP16[(($1682)>>1)];
      var $1684=(($1683)&65535);
      var $1685=(($1679+($1684<<2))|0);
      var $1686=$1685;
      var $1687=$1686;
      var $1688=HEAP32[(($1687)>>2)];
      var $1689=(($1688)|0) < 0;
      if ($1689) { __label__ = 53; break; } else { __label__ = 52; break; }
    case 52: 
      var $1691=$2;
      var $1692=(($1691+64)|0);
      var $1693=HEAP32[(($1692)>>2)];
      var $1694=$st;
      var $1695=(($1694+4)|0);
      var $1696=$1695;
      var $1697=HEAP16[(($1696)>>1)];
      var $1698=(($1697)&65535);
      var $1699=(($1693+($1698<<2))|0);
      var $1700=$1699;
      var $1701=$1700;
      var $1702=HEAP32[(($1701)>>2)];
      var $1703=((($1702)+(3))|0);
      var $1704=$2;
      var $1705=(($1704+144)|0);
      var $1706=HEAP32[(($1705)>>2)];
      var $1707=(($1703)>>>0) > (($1706)>>>0);
      if ($1707) { __label__ = 53; break; } else { __label__ = 54; break; }
    case 53: 
      var $1709=$2;
      var $1710=$2;
      var $1711=(($1710)|0);
      var $1712=HEAP32[(($1711)>>2)];
      var $1713=$2;
      var $1714=(($1713+64)|0);
      var $1715=HEAP32[(($1714)>>2)];
      var $1716=$st;
      var $1717=(($1716+4)|0);
      var $1718=$1717;
      var $1719=HEAP16[(($1718)>>1)];
      var $1720=(($1719)&65535);
      var $1721=(($1715+($1720<<2))|0);
      var $1722=$1721;
      var $1723=$1722;
      var $1724=HEAP32[(($1723)>>2)];
      var $1725=((($1724)+(2))|0);
      _qcvmerror($1709, ((STRING_TABLE.__str18)|0), (tempInt=STACKTOP,STACKTOP += 8,assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),HEAP32[((tempInt)>>2)]=$1712,HEAP32[(((tempInt)+(4))>>2)]=$1725,tempInt));
      __label__ = 488; break;
    case 54: 
      var $1727=$2;
      var $1728=$2;
      var $1729=(($1728+64)|0);
      var $1730=HEAP32[(($1729)>>2)];
      var $1731=$st;
      var $1732=(($1731+2)|0);
      var $1733=$1732;
      var $1734=HEAP16[(($1733)>>1)];
      var $1735=(($1734)&65535);
      var $1736=(($1730+($1735<<2))|0);
      var $1737=$1736;
      var $1738=$1737;
      var $1739=HEAP32[(($1738)>>2)];
      var $1740=_prog_getedict($1727, $1739);
      $ed=$1740;
      var $1741=$ed;
      var $1742=$1741;
      var $1743=$2;
      var $1744=(($1743+64)|0);
      var $1745=HEAP32[(($1744)>>2)];
      var $1746=$st;
      var $1747=(($1746+4)|0);
      var $1748=$1747;
      var $1749=HEAP16[(($1748)>>1)];
      var $1750=(($1749)&65535);
      var $1751=(($1745+($1750<<2))|0);
      var $1752=$1751;
      var $1753=$1752;
      var $1754=HEAP32[(($1753)>>2)];
      var $1755=(($1742+($1754<<2))|0);
      var $1756=$1755;
      var $1757=$1756;
      var $1758=(($1757)|0);
      var $1759=HEAP32[(($1758)>>2)];
      var $1760=$2;
      var $1761=(($1760+64)|0);
      var $1762=HEAP32[(($1761)>>2)];
      var $1763=$st;
      var $1764=(($1763+6)|0);
      var $1765=$1764;
      var $1766=HEAP16[(($1765)>>1)];
      var $1767=(($1766)&65535);
      var $1768=(($1762+($1767<<2))|0);
      var $1769=$1768;
      var $1770=$1769;
      var $1771=(($1770)|0);
      HEAP32[(($1771)>>2)]=$1759;
      var $1772=$ed;
      var $1773=$1772;
      var $1774=$2;
      var $1775=(($1774+64)|0);
      var $1776=HEAP32[(($1775)>>2)];
      var $1777=$st;
      var $1778=(($1777+4)|0);
      var $1779=$1778;
      var $1780=HEAP16[(($1779)>>1)];
      var $1781=(($1780)&65535);
      var $1782=(($1776+($1781<<2))|0);
      var $1783=$1782;
      var $1784=$1783;
      var $1785=HEAP32[(($1784)>>2)];
      var $1786=(($1773+($1785<<2))|0);
      var $1787=$1786;
      var $1788=$1787;
      var $1789=(($1788+4)|0);
      var $1790=HEAP32[(($1789)>>2)];
      var $1791=$2;
      var $1792=(($1791+64)|0);
      var $1793=HEAP32[(($1792)>>2)];
      var $1794=$st;
      var $1795=(($1794+6)|0);
      var $1796=$1795;
      var $1797=HEAP16[(($1796)>>1)];
      var $1798=(($1797)&65535);
      var $1799=(($1793+($1798<<2))|0);
      var $1800=$1799;
      var $1801=$1800;
      var $1802=(($1801+4)|0);
      HEAP32[(($1802)>>2)]=$1790;
      var $1803=$ed;
      var $1804=$1803;
      var $1805=$2;
      var $1806=(($1805+64)|0);
      var $1807=HEAP32[(($1806)>>2)];
      var $1808=$st;
      var $1809=(($1808+4)|0);
      var $1810=$1809;
      var $1811=HEAP16[(($1810)>>1)];
      var $1812=(($1811)&65535);
      var $1813=(($1807+($1812<<2))|0);
      var $1814=$1813;
      var $1815=$1814;
      var $1816=HEAP32[(($1815)>>2)];
      var $1817=(($1804+($1816<<2))|0);
      var $1818=$1817;
      var $1819=$1818;
      var $1820=(($1819+8)|0);
      var $1821=HEAP32[(($1820)>>2)];
      var $1822=$2;
      var $1823=(($1822+64)|0);
      var $1824=HEAP32[(($1823)>>2)];
      var $1825=$st;
      var $1826=(($1825+6)|0);
      var $1827=$1826;
      var $1828=HEAP16[(($1827)>>1)];
      var $1829=(($1828)&65535);
      var $1830=(($1824+($1829<<2))|0);
      var $1831=$1830;
      var $1832=$1831;
      var $1833=(($1832+8)|0);
      HEAP32[(($1833)>>2)]=$1821;
      __label__ = 124; break;
    case 55: 
      var $1835=$2;
      var $1836=(($1835+64)|0);
      var $1837=HEAP32[(($1836)>>2)];
      var $1838=$st;
      var $1839=(($1838+2)|0);
      var $1840=$1839;
      var $1841=HEAP16[(($1840)>>1)];
      var $1842=(($1841)&65535);
      var $1843=(($1837+($1842<<2))|0);
      var $1844=$1843;
      var $1845=$1844;
      var $1846=HEAP32[(($1845)>>2)];
      var $1847=(($1846)|0) < 0;
      if ($1847) { __label__ = 57; break; } else { __label__ = 56; break; }
    case 56: 
      var $1849=$2;
      var $1850=(($1849+64)|0);
      var $1851=HEAP32[(($1850)>>2)];
      var $1852=$st;
      var $1853=(($1852+2)|0);
      var $1854=$1853;
      var $1855=HEAP16[(($1854)>>1)];
      var $1856=(($1855)&65535);
      var $1857=(($1851+($1856<<2))|0);
      var $1858=$1857;
      var $1859=$1858;
      var $1860=HEAP32[(($1859)>>2)];
      var $1861=$2;
      var $1862=(($1861+140)|0);
      var $1863=HEAP32[(($1862)>>2)];
      var $1864=(($1860)|0) >= (($1863)|0);
      if ($1864) { __label__ = 57; break; } else { __label__ = 58; break; }
    case 57: 
      var $1866=$2;
      var $1867=$2;
      var $1868=(($1867)|0);
      var $1869=HEAP32[(($1868)>>2)];
      var $1870=$2;
      var $1871=(($1870+64)|0);
      var $1872=HEAP32[(($1871)>>2)];
      var $1873=$st;
      var $1874=(($1873+2)|0);
      var $1875=$1874;
      var $1876=HEAP16[(($1875)>>1)];
      var $1877=(($1876)&65535);
      var $1878=(($1872+($1877<<2))|0);
      var $1879=$1878;
      var $1880=$1879;
      var $1881=HEAP32[(($1880)>>2)];
      _qcvmerror($1866, ((STRING_TABLE.__str19)|0), (tempInt=STACKTOP,STACKTOP += 8,assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),HEAP32[((tempInt)>>2)]=$1869,HEAP32[(((tempInt)+(4))>>2)]=$1881,tempInt));
      __label__ = 488; break;
    case 58: 
      var $1883=$2;
      var $1884=(($1883+64)|0);
      var $1885=HEAP32[(($1884)>>2)];
      var $1886=$st;
      var $1887=(($1886+4)|0);
      var $1888=$1887;
      var $1889=HEAP16[(($1888)>>1)];
      var $1890=(($1889)&65535);
      var $1891=(($1885+($1890<<2))|0);
      var $1892=$1891;
      var $1893=$1892;
      var $1894=HEAP32[(($1893)>>2)];
      var $1895=$2;
      var $1896=(($1895+144)|0);
      var $1897=HEAP32[(($1896)>>2)];
      var $1898=(($1894)>>>0) >= (($1897)>>>0);
      if ($1898) { __label__ = 59; break; } else { __label__ = 60; break; }
    case 59: 
      var $1900=$2;
      var $1901=$2;
      var $1902=(($1901)|0);
      var $1903=HEAP32[(($1902)>>2)];
      var $1904=$2;
      var $1905=(($1904+64)|0);
      var $1906=HEAP32[(($1905)>>2)];
      var $1907=$st;
      var $1908=(($1907+4)|0);
      var $1909=$1908;
      var $1910=HEAP16[(($1909)>>1)];
      var $1911=(($1910)&65535);
      var $1912=(($1906+($1911<<2))|0);
      var $1913=$1912;
      var $1914=$1913;
      var $1915=HEAP32[(($1914)>>2)];
      _qcvmerror($1900, ((STRING_TABLE.__str18)|0), (tempInt=STACKTOP,STACKTOP += 8,assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),HEAP32[((tempInt)>>2)]=$1903,HEAP32[(((tempInt)+(4))>>2)]=$1915,tempInt));
      __label__ = 488; break;
    case 60: 
      var $1917=$2;
      var $1918=$2;
      var $1919=(($1918+64)|0);
      var $1920=HEAP32[(($1919)>>2)];
      var $1921=$st;
      var $1922=(($1921+2)|0);
      var $1923=$1922;
      var $1924=HEAP16[(($1923)>>1)];
      var $1925=(($1924)&65535);
      var $1926=(($1920+($1925<<2))|0);
      var $1927=$1926;
      var $1928=$1927;
      var $1929=HEAP32[(($1928)>>2)];
      var $1930=_prog_getedict($1917, $1929);
      $ed=$1930;
      var $1931=$ed;
      var $1932=$1931;
      var $1933=$2;
      var $1934=(($1933+76)|0);
      var $1935=HEAP32[(($1934)>>2)];
      var $1936=$1932;
      var $1937=$1935;
      var $1938=((($1936)-($1937))|0);
      var $1939=((((($1938)|0))/(4))&-1);
      var $1940=$2;
      var $1941=(($1940+64)|0);
      var $1942=HEAP32[(($1941)>>2)];
      var $1943=$st;
      var $1944=(($1943+6)|0);
      var $1945=$1944;
      var $1946=HEAP16[(($1945)>>1)];
      var $1947=(($1946)&65535);
      var $1948=(($1942+($1947<<2))|0);
      var $1949=$1948;
      var $1950=$1949;
      HEAP32[(($1950)>>2)]=$1939;
      var $1951=$2;
      var $1952=(($1951+64)|0);
      var $1953=HEAP32[(($1952)>>2)];
      var $1954=$st;
      var $1955=(($1954+4)|0);
      var $1956=$1955;
      var $1957=HEAP16[(($1956)>>1)];
      var $1958=(($1957)&65535);
      var $1959=(($1953+($1958<<2))|0);
      var $1960=$1959;
      var $1961=$1960;
      var $1962=HEAP32[(($1961)>>2)];
      var $1963=$2;
      var $1964=(($1963+64)|0);
      var $1965=HEAP32[(($1964)>>2)];
      var $1966=$st;
      var $1967=(($1966+6)|0);
      var $1968=$1967;
      var $1969=HEAP16[(($1968)>>1)];
      var $1970=(($1969)&65535);
      var $1971=(($1965+($1970<<2))|0);
      var $1972=$1971;
      var $1973=$1972;
      var $1974=HEAP32[(($1973)>>2)];
      var $1975=((($1974)+($1962))|0);
      HEAP32[(($1973)>>2)]=$1975;
      __label__ = 124; break;
    case 61: 
      var $1977=$2;
      var $1978=(($1977+64)|0);
      var $1979=HEAP32[(($1978)>>2)];
      var $1980=$st;
      var $1981=(($1980+2)|0);
      var $1982=$1981;
      var $1983=HEAP16[(($1982)>>1)];
      var $1984=(($1983)&65535);
      var $1985=(($1979+($1984<<2))|0);
      var $1986=$1985;
      var $1987=$1986;
      var $1988=HEAP32[(($1987)>>2)];
      var $1989=$2;
      var $1990=(($1989+64)|0);
      var $1991=HEAP32[(($1990)>>2)];
      var $1992=$st;
      var $1993=(($1992+4)|0);
      var $1994=$1993;
      var $1995=HEAP16[(($1994)>>1)];
      var $1996=(($1995)&65535);
      var $1997=(($1991+($1996<<2))|0);
      var $1998=$1997;
      var $1999=$1998;
      HEAP32[(($1999)>>2)]=$1988;
      __label__ = 124; break;
    case 62: 
      var $2001=$2;
      var $2002=(($2001+64)|0);
      var $2003=HEAP32[(($2002)>>2)];
      var $2004=$st;
      var $2005=(($2004+2)|0);
      var $2006=$2005;
      var $2007=HEAP16[(($2006)>>1)];
      var $2008=(($2007)&65535);
      var $2009=(($2003+($2008<<2))|0);
      var $2010=$2009;
      var $2011=$2010;
      var $2012=(($2011)|0);
      var $2013=HEAP32[(($2012)>>2)];
      var $2014=$2;
      var $2015=(($2014+64)|0);
      var $2016=HEAP32[(($2015)>>2)];
      var $2017=$st;
      var $2018=(($2017+4)|0);
      var $2019=$2018;
      var $2020=HEAP16[(($2019)>>1)];
      var $2021=(($2020)&65535);
      var $2022=(($2016+($2021<<2))|0);
      var $2023=$2022;
      var $2024=$2023;
      var $2025=(($2024)|0);
      HEAP32[(($2025)>>2)]=$2013;
      var $2026=$2;
      var $2027=(($2026+64)|0);
      var $2028=HEAP32[(($2027)>>2)];
      var $2029=$st;
      var $2030=(($2029+2)|0);
      var $2031=$2030;
      var $2032=HEAP16[(($2031)>>1)];
      var $2033=(($2032)&65535);
      var $2034=(($2028+($2033<<2))|0);
      var $2035=$2034;
      var $2036=$2035;
      var $2037=(($2036+4)|0);
      var $2038=HEAP32[(($2037)>>2)];
      var $2039=$2;
      var $2040=(($2039+64)|0);
      var $2041=HEAP32[(($2040)>>2)];
      var $2042=$st;
      var $2043=(($2042+4)|0);
      var $2044=$2043;
      var $2045=HEAP16[(($2044)>>1)];
      var $2046=(($2045)&65535);
      var $2047=(($2041+($2046<<2))|0);
      var $2048=$2047;
      var $2049=$2048;
      var $2050=(($2049+4)|0);
      HEAP32[(($2050)>>2)]=$2038;
      var $2051=$2;
      var $2052=(($2051+64)|0);
      var $2053=HEAP32[(($2052)>>2)];
      var $2054=$st;
      var $2055=(($2054+2)|0);
      var $2056=$2055;
      var $2057=HEAP16[(($2056)>>1)];
      var $2058=(($2057)&65535);
      var $2059=(($2053+($2058<<2))|0);
      var $2060=$2059;
      var $2061=$2060;
      var $2062=(($2061+8)|0);
      var $2063=HEAP32[(($2062)>>2)];
      var $2064=$2;
      var $2065=(($2064+64)|0);
      var $2066=HEAP32[(($2065)>>2)];
      var $2067=$st;
      var $2068=(($2067+4)|0);
      var $2069=$2068;
      var $2070=HEAP16[(($2069)>>1)];
      var $2071=(($2070)&65535);
      var $2072=(($2066+($2071<<2))|0);
      var $2073=$2072;
      var $2074=$2073;
      var $2075=(($2074+8)|0);
      HEAP32[(($2075)>>2)]=$2063;
      __label__ = 124; break;
    case 63: 
      var $2077=$2;
      var $2078=(($2077+64)|0);
      var $2079=HEAP32[(($2078)>>2)];
      var $2080=$st;
      var $2081=(($2080+4)|0);
      var $2082=$2081;
      var $2083=HEAP16[(($2082)>>1)];
      var $2084=(($2083)&65535);
      var $2085=(($2079+($2084<<2))|0);
      var $2086=$2085;
      var $2087=$2086;
      var $2088=HEAP32[(($2087)>>2)];
      var $2089=(($2088)|0) < 0;
      if ($2089) { __label__ = 65; break; } else { __label__ = 64; break; }
    case 64: 
      var $2091=$2;
      var $2092=(($2091+64)|0);
      var $2093=HEAP32[(($2092)>>2)];
      var $2094=$st;
      var $2095=(($2094+4)|0);
      var $2096=$2095;
      var $2097=HEAP16[(($2096)>>1)];
      var $2098=(($2097)&65535);
      var $2099=(($2093+($2098<<2))|0);
      var $2100=$2099;
      var $2101=$2100;
      var $2102=HEAP32[(($2101)>>2)];
      var $2103=$2;
      var $2104=(($2103+80)|0);
      var $2105=HEAP32[(($2104)>>2)];
      var $2106=(($2102)>>>0) >= (($2105)>>>0);
      if ($2106) { __label__ = 65; break; } else { __label__ = 66; break; }
    case 65: 
      var $2108=$2;
      var $2109=$2;
      var $2110=(($2109)|0);
      var $2111=HEAP32[(($2110)>>2)];
      var $2112=$2;
      var $2113=(($2112+64)|0);
      var $2114=HEAP32[(($2113)>>2)];
      var $2115=$st;
      var $2116=(($2115+4)|0);
      var $2117=$2116;
      var $2118=HEAP16[(($2117)>>1)];
      var $2119=(($2118)&65535);
      var $2120=(($2114+($2119<<2))|0);
      var $2121=$2120;
      var $2122=$2121;
      var $2123=HEAP32[(($2122)>>2)];
      _qcvmerror($2108, ((STRING_TABLE.__str20)|0), (tempInt=STACKTOP,STACKTOP += 8,assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),HEAP32[((tempInt)>>2)]=$2111,HEAP32[(((tempInt)+(4))>>2)]=$2123,tempInt));
      __label__ = 488; break;
    case 66: 
      var $2125=$2;
      var $2126=(($2125+64)|0);
      var $2127=HEAP32[(($2126)>>2)];
      var $2128=$st;
      var $2129=(($2128+4)|0);
      var $2130=$2129;
      var $2131=HEAP16[(($2130)>>1)];
      var $2132=(($2131)&65535);
      var $2133=(($2127+($2132<<2))|0);
      var $2134=$2133;
      var $2135=$2134;
      var $2136=HEAP32[(($2135)>>2)];
      var $2137=$2;
      var $2138=(($2137+144)|0);
      var $2139=HEAP32[(($2138)>>2)];
      var $2140=(($2136)>>>0) < (($2139)>>>0);
      if ($2140) { __label__ = 67; break; } else { __label__ = 69; break; }
    case 67: 
      var $2142=$2;
      var $2143=(($2142+148)|0);
      var $2144=HEAP8[($2143)];
      var $2145=(($2144) & 1);
      if ($2145) { __label__ = 69; break; } else { __label__ = 68; break; }
    case 68: 
      var $2147=$2;
      var $2148=$2;
      var $2149=(($2148)|0);
      var $2150=HEAP32[(($2149)>>2)];
      var $2151=$2;
      var $2152=$2;
      var $2153=$2;
      var $2154=(($2153+64)|0);
      var $2155=HEAP32[(($2154)>>2)];
      var $2156=$st;
      var $2157=(($2156+4)|0);
      var $2158=$2157;
      var $2159=HEAP16[(($2158)>>1)];
      var $2160=(($2159)&65535);
      var $2161=(($2155+($2160<<2))|0);
      var $2162=$2161;
      var $2163=$2162;
      var $2164=HEAP32[(($2163)>>2)];
      var $2165=_prog_entfield($2152, $2164);
      var $2166=(($2165+4)|0);
      var $2167=HEAP32[(($2166)>>2)];
      var $2168=_prog_getstring($2151, $2167);
      var $2169=$2;
      var $2170=(($2169+64)|0);
      var $2171=HEAP32[(($2170)>>2)];
      var $2172=$st;
      var $2173=(($2172+4)|0);
      var $2174=$2173;
      var $2175=HEAP16[(($2174)>>1)];
      var $2176=(($2175)&65535);
      var $2177=(($2171+($2176<<2))|0);
      var $2178=$2177;
      var $2179=$2178;
      var $2180=HEAP32[(($2179)>>2)];
      _qcvmerror($2147, ((STRING_TABLE.__str21)|0), (tempInt=STACKTOP,STACKTOP += 12,assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),HEAP32[((tempInt)>>2)]=$2150,HEAP32[(((tempInt)+(4))>>2)]=$2168,HEAP32[(((tempInt)+(8))>>2)]=$2180,tempInt));
      __label__ = 69; break;
    case 69: 
      var $2182=$2;
      var $2183=(($2182+76)|0);
      var $2184=HEAP32[(($2183)>>2)];
      var $2185=$2;
      var $2186=(($2185+64)|0);
      var $2187=HEAP32[(($2186)>>2)];
      var $2188=$st;
      var $2189=(($2188+4)|0);
      var $2190=$2189;
      var $2191=HEAP16[(($2190)>>1)];
      var $2192=(($2191)&65535);
      var $2193=(($2187+($2192<<2))|0);
      var $2194=$2193;
      var $2195=$2194;
      var $2196=HEAP32[(($2195)>>2)];
      var $2197=(($2184+($2196<<2))|0);
      var $2198=$2197;
      $ptr=$2198;
      var $2199=$2;
      var $2200=(($2199+64)|0);
      var $2201=HEAP32[(($2200)>>2)];
      var $2202=$st;
      var $2203=(($2202+2)|0);
      var $2204=$2203;
      var $2205=HEAP16[(($2204)>>1)];
      var $2206=(($2205)&65535);
      var $2207=(($2201+($2206<<2))|0);
      var $2208=$2207;
      var $2209=$2208;
      var $2210=HEAP32[(($2209)>>2)];
      var $2211=$ptr;
      var $2212=$2211;
      HEAP32[(($2212)>>2)]=$2210;
      __label__ = 124; break;
    case 70: 
      var $2214=$2;
      var $2215=(($2214+64)|0);
      var $2216=HEAP32[(($2215)>>2)];
      var $2217=$st;
      var $2218=(($2217+4)|0);
      var $2219=$2218;
      var $2220=HEAP16[(($2219)>>1)];
      var $2221=(($2220)&65535);
      var $2222=(($2216+($2221<<2))|0);
      var $2223=$2222;
      var $2224=$2223;
      var $2225=HEAP32[(($2224)>>2)];
      var $2226=(($2225)|0) < 0;
      if ($2226) { __label__ = 72; break; } else { __label__ = 71; break; }
    case 71: 
      var $2228=$2;
      var $2229=(($2228+64)|0);
      var $2230=HEAP32[(($2229)>>2)];
      var $2231=$st;
      var $2232=(($2231+4)|0);
      var $2233=$2232;
      var $2234=HEAP16[(($2233)>>1)];
      var $2235=(($2234)&65535);
      var $2236=(($2230+($2235<<2))|0);
      var $2237=$2236;
      var $2238=$2237;
      var $2239=HEAP32[(($2238)>>2)];
      var $2240=((($2239)+(2))|0);
      var $2241=$2;
      var $2242=(($2241+80)|0);
      var $2243=HEAP32[(($2242)>>2)];
      var $2244=(($2240)>>>0) >= (($2243)>>>0);
      if ($2244) { __label__ = 72; break; } else { __label__ = 73; break; }
    case 72: 
      var $2246=$2;
      var $2247=$2;
      var $2248=(($2247)|0);
      var $2249=HEAP32[(($2248)>>2)];
      var $2250=$2;
      var $2251=(($2250+64)|0);
      var $2252=HEAP32[(($2251)>>2)];
      var $2253=$st;
      var $2254=(($2253+4)|0);
      var $2255=$2254;
      var $2256=HEAP16[(($2255)>>1)];
      var $2257=(($2256)&65535);
      var $2258=(($2252+($2257<<2))|0);
      var $2259=$2258;
      var $2260=$2259;
      var $2261=HEAP32[(($2260)>>2)];
      _qcvmerror($2246, ((STRING_TABLE.__str20)|0), (tempInt=STACKTOP,STACKTOP += 8,assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),HEAP32[((tempInt)>>2)]=$2249,HEAP32[(((tempInt)+(4))>>2)]=$2261,tempInt));
      __label__ = 488; break;
    case 73: 
      var $2263=$2;
      var $2264=(($2263+64)|0);
      var $2265=HEAP32[(($2264)>>2)];
      var $2266=$st;
      var $2267=(($2266+4)|0);
      var $2268=$2267;
      var $2269=HEAP16[(($2268)>>1)];
      var $2270=(($2269)&65535);
      var $2271=(($2265+($2270<<2))|0);
      var $2272=$2271;
      var $2273=$2272;
      var $2274=HEAP32[(($2273)>>2)];
      var $2275=$2;
      var $2276=(($2275+144)|0);
      var $2277=HEAP32[(($2276)>>2)];
      var $2278=(($2274)>>>0) < (($2277)>>>0);
      if ($2278) { __label__ = 74; break; } else { __label__ = 76; break; }
    case 74: 
      var $2280=$2;
      var $2281=(($2280+148)|0);
      var $2282=HEAP8[($2281)];
      var $2283=(($2282) & 1);
      if ($2283) { __label__ = 76; break; } else { __label__ = 75; break; }
    case 75: 
      var $2285=$2;
      var $2286=$2;
      var $2287=(($2286)|0);
      var $2288=HEAP32[(($2287)>>2)];
      var $2289=$2;
      var $2290=$2;
      var $2291=$2;
      var $2292=(($2291+64)|0);
      var $2293=HEAP32[(($2292)>>2)];
      var $2294=$st;
      var $2295=(($2294+4)|0);
      var $2296=$2295;
      var $2297=HEAP16[(($2296)>>1)];
      var $2298=(($2297)&65535);
      var $2299=(($2293+($2298<<2))|0);
      var $2300=$2299;
      var $2301=$2300;
      var $2302=HEAP32[(($2301)>>2)];
      var $2303=_prog_entfield($2290, $2302);
      var $2304=(($2303+4)|0);
      var $2305=HEAP32[(($2304)>>2)];
      var $2306=_prog_getstring($2289, $2305);
      var $2307=$2;
      var $2308=(($2307+64)|0);
      var $2309=HEAP32[(($2308)>>2)];
      var $2310=$st;
      var $2311=(($2310+4)|0);
      var $2312=$2311;
      var $2313=HEAP16[(($2312)>>1)];
      var $2314=(($2313)&65535);
      var $2315=(($2309+($2314<<2))|0);
      var $2316=$2315;
      var $2317=$2316;
      var $2318=HEAP32[(($2317)>>2)];
      _qcvmerror($2285, ((STRING_TABLE.__str21)|0), (tempInt=STACKTOP,STACKTOP += 12,assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),HEAP32[((tempInt)>>2)]=$2288,HEAP32[(((tempInt)+(4))>>2)]=$2306,HEAP32[(((tempInt)+(8))>>2)]=$2318,tempInt));
      __label__ = 76; break;
    case 76: 
      var $2320=$2;
      var $2321=(($2320+76)|0);
      var $2322=HEAP32[(($2321)>>2)];
      var $2323=$2;
      var $2324=(($2323+64)|0);
      var $2325=HEAP32[(($2324)>>2)];
      var $2326=$st;
      var $2327=(($2326+4)|0);
      var $2328=$2327;
      var $2329=HEAP16[(($2328)>>1)];
      var $2330=(($2329)&65535);
      var $2331=(($2325+($2330<<2))|0);
      var $2332=$2331;
      var $2333=$2332;
      var $2334=HEAP32[(($2333)>>2)];
      var $2335=(($2322+($2334<<2))|0);
      var $2336=$2335;
      $ptr=$2336;
      var $2337=$2;
      var $2338=(($2337+64)|0);
      var $2339=HEAP32[(($2338)>>2)];
      var $2340=$st;
      var $2341=(($2340+2)|0);
      var $2342=$2341;
      var $2343=HEAP16[(($2342)>>1)];
      var $2344=(($2343)&65535);
      var $2345=(($2339+($2344<<2))|0);
      var $2346=$2345;
      var $2347=$2346;
      var $2348=(($2347)|0);
      var $2349=HEAP32[(($2348)>>2)];
      var $2350=$ptr;
      var $2351=$2350;
      var $2352=(($2351)|0);
      HEAP32[(($2352)>>2)]=$2349;
      var $2353=$2;
      var $2354=(($2353+64)|0);
      var $2355=HEAP32[(($2354)>>2)];
      var $2356=$st;
      var $2357=(($2356+2)|0);
      var $2358=$2357;
      var $2359=HEAP16[(($2358)>>1)];
      var $2360=(($2359)&65535);
      var $2361=(($2355+($2360<<2))|0);
      var $2362=$2361;
      var $2363=$2362;
      var $2364=(($2363+4)|0);
      var $2365=HEAP32[(($2364)>>2)];
      var $2366=$ptr;
      var $2367=$2366;
      var $2368=(($2367+4)|0);
      HEAP32[(($2368)>>2)]=$2365;
      var $2369=$2;
      var $2370=(($2369+64)|0);
      var $2371=HEAP32[(($2370)>>2)];
      var $2372=$st;
      var $2373=(($2372+2)|0);
      var $2374=$2373;
      var $2375=HEAP16[(($2374)>>1)];
      var $2376=(($2375)&65535);
      var $2377=(($2371+($2376<<2))|0);
      var $2378=$2377;
      var $2379=$2378;
      var $2380=(($2379+8)|0);
      var $2381=HEAP32[(($2380)>>2)];
      var $2382=$ptr;
      var $2383=$2382;
      var $2384=(($2383+8)|0);
      HEAP32[(($2384)>>2)]=$2381;
      __label__ = 124; break;
    case 77: 
      var $2386=$2;
      var $2387=(($2386+64)|0);
      var $2388=HEAP32[(($2387)>>2)];
      var $2389=$st;
      var $2390=(($2389+2)|0);
      var $2391=$2390;
      var $2392=HEAP16[(($2391)>>1)];
      var $2393=(($2392)&65535);
      var $2394=(($2388+($2393<<2))|0);
      var $2395=$2394;
      var $2396=$2395;
      var $2397=HEAP32[(($2396)>>2)];
      var $2398=$2397 & 2147483647;
      var $2399=(($2398)|0)!=0;
      var $2400=$2399 ^ 1;
      var $2401=(($2400)&1);
      var $2402=(($2401)|0);
      var $2403=$2;
      var $2404=(($2403+64)|0);
      var $2405=HEAP32[(($2404)>>2)];
      var $2406=$st;
      var $2407=(($2406+6)|0);
      var $2408=$2407;
      var $2409=HEAP16[(($2408)>>1)];
      var $2410=(($2409)&65535);
      var $2411=(($2405+($2410<<2))|0);
      var $2412=$2411;
      var $2413=$2412;
      HEAPF32[(($2413)>>2)]=$2402;
      __label__ = 124; break;
    case 78: 
      var $2415=$2;
      var $2416=(($2415+64)|0);
      var $2417=HEAP32[(($2416)>>2)];
      var $2418=$st;
      var $2419=(($2418+2)|0);
      var $2420=$2419;
      var $2421=HEAP16[(($2420)>>1)];
      var $2422=(($2421)&65535);
      var $2423=(($2417+($2422<<2))|0);
      var $2424=$2423;
      var $2425=$2424;
      var $2426=(($2425)|0);
      var $2427=HEAPF32[(($2426)>>2)];
      var $2428=$2427 != 0;
      if ($2428) { var $2461 = 0;__label__ = 81; break; } else { __label__ = 79; break; }
    case 79: 
      var $2430=$2;
      var $2431=(($2430+64)|0);
      var $2432=HEAP32[(($2431)>>2)];
      var $2433=$st;
      var $2434=(($2433+2)|0);
      var $2435=$2434;
      var $2436=HEAP16[(($2435)>>1)];
      var $2437=(($2436)&65535);
      var $2438=(($2432+($2437<<2))|0);
      var $2439=$2438;
      var $2440=$2439;
      var $2441=(($2440+4)|0);
      var $2442=HEAPF32[(($2441)>>2)];
      var $2443=$2442 != 0;
      if ($2443) { var $2461 = 0;__label__ = 81; break; } else { __label__ = 80; break; }
    case 80: 
      var $2445=$2;
      var $2446=(($2445+64)|0);
      var $2447=HEAP32[(($2446)>>2)];
      var $2448=$st;
      var $2449=(($2448+2)|0);
      var $2450=$2449;
      var $2451=HEAP16[(($2450)>>1)];
      var $2452=(($2451)&65535);
      var $2453=(($2447+($2452<<2))|0);
      var $2454=$2453;
      var $2455=$2454;
      var $2456=(($2455+8)|0);
      var $2457=HEAPF32[(($2456)>>2)];
      var $2458=$2457 != 0;
      var $2459=$2458 ^ 1;
      var $2461 = $2459;__label__ = 81; break;
    case 81: 
      var $2461;
      var $2462=(($2461)&1);
      var $2463=(($2462)|0);
      var $2464=$2;
      var $2465=(($2464+64)|0);
      var $2466=HEAP32[(($2465)>>2)];
      var $2467=$st;
      var $2468=(($2467+6)|0);
      var $2469=$2468;
      var $2470=HEAP16[(($2469)>>1)];
      var $2471=(($2470)&65535);
      var $2472=(($2466+($2471<<2))|0);
      var $2473=$2472;
      var $2474=$2473;
      HEAPF32[(($2474)>>2)]=$2463;
      __label__ = 124; break;
    case 82: 
      var $2476=$2;
      var $2477=(($2476+64)|0);
      var $2478=HEAP32[(($2477)>>2)];
      var $2479=$st;
      var $2480=(($2479+2)|0);
      var $2481=$2480;
      var $2482=HEAP16[(($2481)>>1)];
      var $2483=(($2482)&65535);
      var $2484=(($2478+($2483<<2))|0);
      var $2485=$2484;
      var $2486=$2485;
      var $2487=HEAP32[(($2486)>>2)];
      var $2488=(($2487)|0)!=0;
      if ($2488) { __label__ = 83; break; } else { var $2508 = 1;__label__ = 84; break; }
    case 83: 
      var $2490=$2;
      var $2491=$2;
      var $2492=(($2491+64)|0);
      var $2493=HEAP32[(($2492)>>2)];
      var $2494=$st;
      var $2495=(($2494+2)|0);
      var $2496=$2495;
      var $2497=HEAP16[(($2496)>>1)];
      var $2498=(($2497)&65535);
      var $2499=(($2493+($2498<<2))|0);
      var $2500=$2499;
      var $2501=$2500;
      var $2502=HEAP32[(($2501)>>2)];
      var $2503=_prog_getstring($2490, $2502);
      var $2504=HEAP8[($2503)];
      var $2505=(($2504 << 24) >> 24)!=0;
      var $2506=$2505 ^ 1;
      var $2508 = $2506;__label__ = 84; break;
    case 84: 
      var $2508;
      var $2509=(($2508)&1);
      var $2510=(($2509)|0);
      var $2511=$2;
      var $2512=(($2511+64)|0);
      var $2513=HEAP32[(($2512)>>2)];
      var $2514=$st;
      var $2515=(($2514+6)|0);
      var $2516=$2515;
      var $2517=HEAP16[(($2516)>>1)];
      var $2518=(($2517)&65535);
      var $2519=(($2513+($2518<<2))|0);
      var $2520=$2519;
      var $2521=$2520;
      HEAPF32[(($2521)>>2)]=$2510;
      __label__ = 124; break;
    case 85: 
      var $2523=$2;
      var $2524=(($2523+64)|0);
      var $2525=HEAP32[(($2524)>>2)];
      var $2526=$st;
      var $2527=(($2526+2)|0);
      var $2528=$2527;
      var $2529=HEAP16[(($2528)>>1)];
      var $2530=(($2529)&65535);
      var $2531=(($2525+($2530<<2))|0);
      var $2532=$2531;
      var $2533=$2532;
      var $2534=HEAP32[(($2533)>>2)];
      var $2535=(($2534)|0)==0;
      var $2536=(($2535)&1);
      var $2537=(($2536)|0);
      var $2538=$2;
      var $2539=(($2538+64)|0);
      var $2540=HEAP32[(($2539)>>2)];
      var $2541=$st;
      var $2542=(($2541+6)|0);
      var $2543=$2542;
      var $2544=HEAP16[(($2543)>>1)];
      var $2545=(($2544)&65535);
      var $2546=(($2540+($2545<<2))|0);
      var $2547=$2546;
      var $2548=$2547;
      HEAPF32[(($2548)>>2)]=$2537;
      __label__ = 124; break;
    case 86: 
      var $2550=$2;
      var $2551=(($2550+64)|0);
      var $2552=HEAP32[(($2551)>>2)];
      var $2553=$st;
      var $2554=(($2553+2)|0);
      var $2555=$2554;
      var $2556=HEAP16[(($2555)>>1)];
      var $2557=(($2556)&65535);
      var $2558=(($2552+($2557<<2))|0);
      var $2559=$2558;
      var $2560=$2559;
      var $2561=HEAP32[(($2560)>>2)];
      var $2562=(($2561)|0)!=0;
      var $2563=$2562 ^ 1;
      var $2564=(($2563)&1);
      var $2565=(($2564)|0);
      var $2566=$2;
      var $2567=(($2566+64)|0);
      var $2568=HEAP32[(($2567)>>2)];
      var $2569=$st;
      var $2570=(($2569+6)|0);
      var $2571=$2570;
      var $2572=HEAP16[(($2571)>>1)];
      var $2573=(($2572)&65535);
      var $2574=(($2568+($2573<<2))|0);
      var $2575=$2574;
      var $2576=$2575;
      HEAPF32[(($2576)>>2)]=$2565;
      __label__ = 124; break;
    case 87: 
      var $2578=$2;
      var $2579=(($2578+64)|0);
      var $2580=HEAP32[(($2579)>>2)];
      var $2581=$st;
      var $2582=(($2581+2)|0);
      var $2583=$2582;
      var $2584=HEAP16[(($2583)>>1)];
      var $2585=(($2584)&65535);
      var $2586=(($2580+($2585<<2))|0);
      var $2587=$2586;
      var $2588=$2587;
      var $2589=HEAP32[(($2588)>>2)];
      var $2590=$2589 & 2147483647;
      var $2591=(($2590)|0)!=0;
      if ($2591) { __label__ = 88; break; } else { __label__ = 91; break; }
    case 88: 
      var $2593=$st;
      var $2594=(($2593+4)|0);
      var $2595=$2594;
      var $2596=HEAP16[(($2595)>>1)];
      var $2597=(($2596 << 16) >> 16);
      var $2598=((($2597)-(1))|0);
      var $2599=$st;
      var $2600=(($2599+($2598<<3))|0);
      $st=$2600;
      var $2601=$jumpcount;
      var $2602=((($2601)+(1))|0);
      $jumpcount=$2602;
      var $2603=$5;
      var $2604=(($2602)|0) >= (($2603)|0);
      if ($2604) { __label__ = 89; break; } else { __label__ = 90; break; }
    case 89: 
      var $2606=$2;
      var $2607=$2;
      var $2608=(($2607)|0);
      var $2609=HEAP32[(($2608)>>2)];
      var $2610=$jumpcount;
      _qcvmerror($2606, ((STRING_TABLE.__str22)|0), (tempInt=STACKTOP,STACKTOP += 8,assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),HEAP32[((tempInt)>>2)]=$2609,HEAP32[(((tempInt)+(4))>>2)]=$2610,tempInt));
      __label__ = 90; break;
    case 90: 
      __label__ = 91; break;
    case 91: 
      __label__ = 124; break;
    case 92: 
      var $2614=$2;
      var $2615=(($2614+64)|0);
      var $2616=HEAP32[(($2615)>>2)];
      var $2617=$st;
      var $2618=(($2617+2)|0);
      var $2619=$2618;
      var $2620=HEAP16[(($2619)>>1)];
      var $2621=(($2620)&65535);
      var $2622=(($2616+($2621<<2))|0);
      var $2623=$2622;
      var $2624=$2623;
      var $2625=HEAP32[(($2624)>>2)];
      var $2626=$2625 & 2147483647;
      var $2627=(($2626)|0)!=0;
      if ($2627) { __label__ = 96; break; } else { __label__ = 93; break; }
    case 93: 
      var $2629=$st;
      var $2630=(($2629+4)|0);
      var $2631=$2630;
      var $2632=HEAP16[(($2631)>>1)];
      var $2633=(($2632 << 16) >> 16);
      var $2634=((($2633)-(1))|0);
      var $2635=$st;
      var $2636=(($2635+($2634<<3))|0);
      $st=$2636;
      var $2637=$jumpcount;
      var $2638=((($2637)+(1))|0);
      $jumpcount=$2638;
      var $2639=$5;
      var $2640=(($2638)|0) >= (($2639)|0);
      if ($2640) { __label__ = 94; break; } else { __label__ = 95; break; }
    case 94: 
      var $2642=$2;
      var $2643=$2;
      var $2644=(($2643)|0);
      var $2645=HEAP32[(($2644)>>2)];
      var $2646=$jumpcount;
      _qcvmerror($2642, ((STRING_TABLE.__str22)|0), (tempInt=STACKTOP,STACKTOP += 8,assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),HEAP32[((tempInt)>>2)]=$2645,HEAP32[(((tempInt)+(4))>>2)]=$2646,tempInt));
      __label__ = 95; break;
    case 95: 
      __label__ = 96; break;
    case 96: 
      __label__ = 124; break;
    case 97: 
      var $2650=$st;
      var $2651=(($2650)|0);
      var $2652=HEAP16[(($2651)>>1)];
      var $2653=(($2652)&65535);
      var $2654=((($2653)-(51))|0);
      var $2655=$2;
      var $2656=(($2655+184)|0);
      HEAP32[(($2656)>>2)]=$2654;
      var $2657=$2;
      var $2658=(($2657+64)|0);
      var $2659=HEAP32[(($2658)>>2)];
      var $2660=$st;
      var $2661=(($2660+2)|0);
      var $2662=$2661;
      var $2663=HEAP16[(($2662)>>1)];
      var $2664=(($2663)&65535);
      var $2665=(($2659+($2664<<2))|0);
      var $2666=$2665;
      var $2667=$2666;
      var $2668=HEAP32[(($2667)>>2)];
      var $2669=(($2668)|0)!=0;
      if ($2669) { __label__ = 99; break; } else { __label__ = 98; break; }
    case 98: 
      var $2671=$2;
      var $2672=$2;
      var $2673=(($2672)|0);
      var $2674=HEAP32[(($2673)>>2)];
      _qcvmerror($2671, ((STRING_TABLE.__str23)|0), (tempInt=STACKTOP,STACKTOP += 4,assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),HEAP32[((tempInt)>>2)]=$2674,tempInt));
      __label__ = 99; break;
    case 99: 
      var $2676=$2;
      var $2677=(($2676+64)|0);
      var $2678=HEAP32[(($2677)>>2)];
      var $2679=$st;
      var $2680=(($2679+2)|0);
      var $2681=$2680;
      var $2682=HEAP16[(($2681)>>1)];
      var $2683=(($2682)&65535);
      var $2684=(($2678+($2683<<2))|0);
      var $2685=$2684;
      var $2686=$2685;
      var $2687=HEAP32[(($2686)>>2)];
      var $2688=(($2687)|0)!=0;
      if ($2688) { __label__ = 100; break; } else { __label__ = 101; break; }
    case 100: 
      var $2690=$2;
      var $2691=(($2690+64)|0);
      var $2692=HEAP32[(($2691)>>2)];
      var $2693=$st;
      var $2694=(($2693+2)|0);
      var $2695=$2694;
      var $2696=HEAP16[(($2695)>>1)];
      var $2697=(($2696)&65535);
      var $2698=(($2692+($2697<<2))|0);
      var $2699=$2698;
      var $2700=$2699;
      var $2701=HEAP32[(($2700)>>2)];
      var $2702=$2;
      var $2703=(($2702+44)|0);
      var $2704=HEAP32[(($2703)>>2)];
      var $2705=(($2701)>>>0) >= (($2704)>>>0);
      if ($2705) { __label__ = 101; break; } else { __label__ = 102; break; }
    case 101: 
      var $2707=$2;
      var $2708=$2;
      var $2709=(($2708)|0);
      var $2710=HEAP32[(($2709)>>2)];
      _qcvmerror($2707, ((STRING_TABLE.__str24)|0), (tempInt=STACKTOP,STACKTOP += 4,assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),HEAP32[((tempInt)>>2)]=$2710,tempInt));
      __label__ = 488; break;
    case 102: 
      var $2712=$2;
      var $2713=(($2712+64)|0);
      var $2714=HEAP32[(($2713)>>2)];
      var $2715=$st;
      var $2716=(($2715+2)|0);
      var $2717=$2716;
      var $2718=HEAP16[(($2717)>>1)];
      var $2719=(($2718)&65535);
      var $2720=(($2714+($2719<<2))|0);
      var $2721=$2720;
      var $2722=$2721;
      var $2723=HEAP32[(($2722)>>2)];
      var $2724=$2;
      var $2725=(($2724+40)|0);
      var $2726=HEAP32[(($2725)>>2)];
      var $2727=(($2726+($2723)*(36))|0);
      $newf=$2727;
      var $2728=$newf;
      var $2729=(($2728+12)|0);
      var $2730=HEAP32[(($2729)>>2)];
      var $2731=((($2730)+(1))|0);
      HEAP32[(($2729)>>2)]=$2731;
      var $2732=$st;
      var $2733=$2;
      var $2734=(($2733+4)|0);
      var $2735=HEAP32[(($2734)>>2)];
      var $2736=$2732;
      var $2737=$2735;
      var $2738=((($2736)-($2737))|0);
      var $2739=((((($2738)|0))/(8))&-1);
      var $2740=((($2739)+(1))|0);
      var $2741=$2;
      var $2742=(($2741+176)|0);
      HEAP32[(($2742)>>2)]=$2740;
      var $2743=$newf;
      var $2744=(($2743)|0);
      var $2745=HEAP32[(($2744)>>2)];
      var $2746=(($2745)|0) < 0;
      if ($2746) { __label__ = 103; break; } else { __label__ = 108; break; }
    case 103: 
      var $2748=$newf;
      var $2749=(($2748)|0);
      var $2750=HEAP32[(($2749)>>2)];
      var $2751=(((-$2750))|0);
      $builtinnumber=$2751;
      var $2752=$builtinnumber;
      var $2753=$2;
      var $2754=(($2753+132)|0);
      var $2755=HEAP32[(($2754)>>2)];
      var $2756=(($2752)>>>0) < (($2755)>>>0);
      if ($2756) { __label__ = 104; break; } else { __label__ = 106; break; }
    case 104: 
      var $2758=$builtinnumber;
      var $2759=$2;
      var $2760=(($2759+128)|0);
      var $2761=HEAP32[(($2760)>>2)];
      var $2762=(($2761+($2758<<2))|0);
      var $2763=HEAP32[(($2762)>>2)];
      var $2764=(($2763)|0)!=0;
      if ($2764) { __label__ = 105; break; } else { __label__ = 106; break; }
    case 105: 
      var $2766=$builtinnumber;
      var $2767=$2;
      var $2768=(($2767+128)|0);
      var $2769=HEAP32[(($2768)>>2)];
      var $2770=(($2769+($2766<<2))|0);
      var $2771=HEAP32[(($2770)>>2)];
      var $2772=$2;
      var $2773=FUNCTION_TABLE[$2771]($2772);
      __label__ = 107; break;
    case 106: 
      var $2775=$2;
      var $2776=$builtinnumber;
      var $2777=$2;
      var $2778=(($2777)|0);
      var $2779=HEAP32[(($2778)>>2)];
      _qcvmerror($2775, ((STRING_TABLE.__str25)|0), (tempInt=STACKTOP,STACKTOP += 8,assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),HEAP32[((tempInt)>>2)]=$2776,HEAP32[(((tempInt)+(4))>>2)]=$2779,tempInt));
      __label__ = 107; break;
    case 107: 
      __label__ = 109; break;
    case 108: 
      var $2782=$2;
      var $2783=(($2782+4)|0);
      var $2784=HEAP32[(($2783)>>2)];
      var $2785=$2;
      var $2786=$newf;
      var $2787=_prog_enterfunction($2785, $2786);
      var $2788=(($2784+($2787<<3))|0);
      var $2789=((($2788)-(8))|0);
      $st=$2789;
      __label__ = 109; break;
    case 109: 
      var $2791=$2;
      var $2792=(($2791+112)|0);
      var $2793=HEAP32[(($2792)>>2)];
      var $2794=(($2793)|0)!=0;
      if ($2794) { __label__ = 110; break; } else { __label__ = 111; break; }
    case 110: 
      __label__ = 488; break;
    case 111: 
      __label__ = 124; break;
    case 112: 
      var $2798=$2;
      var $2799=$2;
      var $2800=(($2799)|0);
      var $2801=HEAP32[(($2800)>>2)];
      _qcvmerror($2798, ((STRING_TABLE.__str26)|0), (tempInt=STACKTOP,STACKTOP += 4,assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),HEAP32[((tempInt)>>2)]=$2801,tempInt));
      __label__ = 124; break;
    case 113: 
      var $2803=$st;
      var $2804=(($2803+2)|0);
      var $2805=$2804;
      var $2806=HEAP16[(($2805)>>1)];
      var $2807=(($2806 << 16) >> 16);
      var $2808=((($2807)-(1))|0);
      var $2809=$st;
      var $2810=(($2809+($2808<<3))|0);
      $st=$2810;
      var $2811=$jumpcount;
      var $2812=((($2811)+(1))|0);
      $jumpcount=$2812;
      var $2813=(($2812)|0)==10000000;
      if ($2813) { __label__ = 114; break; } else { __label__ = 115; break; }
    case 114: 
      var $2815=$2;
      var $2816=$2;
      var $2817=(($2816)|0);
      var $2818=HEAP32[(($2817)>>2)];
      var $2819=$jumpcount;
      _qcvmerror($2815, ((STRING_TABLE.__str22)|0), (tempInt=STACKTOP,STACKTOP += 8,assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),HEAP32[((tempInt)>>2)]=$2818,HEAP32[(((tempInt)+(4))>>2)]=$2819,tempInt));
      __label__ = 115; break;
    case 115: 
      __label__ = 124; break;
    case 116: 
      var $2822=$2;
      var $2823=(($2822+64)|0);
      var $2824=HEAP32[(($2823)>>2)];
      var $2825=$st;
      var $2826=(($2825+2)|0);
      var $2827=$2826;
      var $2828=HEAP16[(($2827)>>1)];
      var $2829=(($2828)&65535);
      var $2830=(($2824+($2829<<2))|0);
      var $2831=$2830;
      var $2832=$2831;
      var $2833=HEAP32[(($2832)>>2)];
      var $2834=$2833 & 2147483647;
      var $2835=(($2834)|0)!=0;
      if ($2835) { __label__ = 117; break; } else { var $2852 = 0;__label__ = 118; break; }
    case 117: 
      var $2837=$2;
      var $2838=(($2837+64)|0);
      var $2839=HEAP32[(($2838)>>2)];
      var $2840=$st;
      var $2841=(($2840+4)|0);
      var $2842=$2841;
      var $2843=HEAP16[(($2842)>>1)];
      var $2844=(($2843)&65535);
      var $2845=(($2839+($2844<<2))|0);
      var $2846=$2845;
      var $2847=$2846;
      var $2848=HEAP32[(($2847)>>2)];
      var $2849=$2848 & 2147483647;
      var $2850=(($2849)|0)!=0;
      var $2852 = $2850;__label__ = 118; break;
    case 118: 
      var $2852;
      var $2853=(($2852)&1);
      var $2854=(($2853)|0);
      var $2855=$2;
      var $2856=(($2855+64)|0);
      var $2857=HEAP32[(($2856)>>2)];
      var $2858=$st;
      var $2859=(($2858+6)|0);
      var $2860=$2859;
      var $2861=HEAP16[(($2860)>>1)];
      var $2862=(($2861)&65535);
      var $2863=(($2857+($2862<<2))|0);
      var $2864=$2863;
      var $2865=$2864;
      HEAPF32[(($2865)>>2)]=$2854;
      __label__ = 124; break;
    case 119: 
      var $2867=$2;
      var $2868=(($2867+64)|0);
      var $2869=HEAP32[(($2868)>>2)];
      var $2870=$st;
      var $2871=(($2870+2)|0);
      var $2872=$2871;
      var $2873=HEAP16[(($2872)>>1)];
      var $2874=(($2873)&65535);
      var $2875=(($2869+($2874<<2))|0);
      var $2876=$2875;
      var $2877=$2876;
      var $2878=HEAP32[(($2877)>>2)];
      var $2879=$2878 & 2147483647;
      var $2880=(($2879)|0)!=0;
      if ($2880) { var $2897 = 1;__label__ = 121; break; } else { __label__ = 120; break; }
    case 120: 
      var $2882=$2;
      var $2883=(($2882+64)|0);
      var $2884=HEAP32[(($2883)>>2)];
      var $2885=$st;
      var $2886=(($2885+4)|0);
      var $2887=$2886;
      var $2888=HEAP16[(($2887)>>1)];
      var $2889=(($2888)&65535);
      var $2890=(($2884+($2889<<2))|0);
      var $2891=$2890;
      var $2892=$2891;
      var $2893=HEAP32[(($2892)>>2)];
      var $2894=$2893 & 2147483647;
      var $2895=(($2894)|0)!=0;
      var $2897 = $2895;__label__ = 121; break;
    case 121: 
      var $2897;
      var $2898=(($2897)&1);
      var $2899=(($2898)|0);
      var $2900=$2;
      var $2901=(($2900+64)|0);
      var $2902=HEAP32[(($2901)>>2)];
      var $2903=$st;
      var $2904=(($2903+6)|0);
      var $2905=$2904;
      var $2906=HEAP16[(($2905)>>1)];
      var $2907=(($2906)&65535);
      var $2908=(($2902+($2907<<2))|0);
      var $2909=$2908;
      var $2910=$2909;
      HEAPF32[(($2910)>>2)]=$2899;
      __label__ = 124; break;
    case 122: 
      var $2912=$2;
      var $2913=(($2912+64)|0);
      var $2914=HEAP32[(($2913)>>2)];
      var $2915=$st;
      var $2916=(($2915+2)|0);
      var $2917=$2916;
      var $2918=HEAP16[(($2917)>>1)];
      var $2919=(($2918)&65535);
      var $2920=(($2914+($2919<<2))|0);
      var $2921=$2920;
      var $2922=$2921;
      var $2923=HEAPF32[(($2922)>>2)];
      var $2924=(($2923)&-1);
      var $2925=$2;
      var $2926=(($2925+64)|0);
      var $2927=HEAP32[(($2926)>>2)];
      var $2928=$st;
      var $2929=(($2928+4)|0);
      var $2930=$2929;
      var $2931=HEAP16[(($2930)>>1)];
      var $2932=(($2931)&65535);
      var $2933=(($2927+($2932<<2))|0);
      var $2934=$2933;
      var $2935=$2934;
      var $2936=HEAPF32[(($2935)>>2)];
      var $2937=(($2936)&-1);
      var $2938=$2924 & $2937;
      var $2939=(($2938)|0);
      var $2940=$2;
      var $2941=(($2940+64)|0);
      var $2942=HEAP32[(($2941)>>2)];
      var $2943=$st;
      var $2944=(($2943+6)|0);
      var $2945=$2944;
      var $2946=HEAP16[(($2945)>>1)];
      var $2947=(($2946)&65535);
      var $2948=(($2942+($2947<<2))|0);
      var $2949=$2948;
      var $2950=$2949;
      HEAPF32[(($2950)>>2)]=$2939;
      __label__ = 124; break;
    case 123: 
      var $2952=$2;
      var $2953=(($2952+64)|0);
      var $2954=HEAP32[(($2953)>>2)];
      var $2955=$st;
      var $2956=(($2955+2)|0);
      var $2957=$2956;
      var $2958=HEAP16[(($2957)>>1)];
      var $2959=(($2958)&65535);
      var $2960=(($2954+($2959<<2))|0);
      var $2961=$2960;
      var $2962=$2961;
      var $2963=HEAPF32[(($2962)>>2)];
      var $2964=(($2963)&-1);
      var $2965=$2;
      var $2966=(($2965+64)|0);
      var $2967=HEAP32[(($2966)>>2)];
      var $2968=$st;
      var $2969=(($2968+4)|0);
      var $2970=$2969;
      var $2971=HEAP16[(($2970)>>1)];
      var $2972=(($2971)&65535);
      var $2973=(($2967+($2972<<2))|0);
      var $2974=$2973;
      var $2975=$2974;
      var $2976=HEAPF32[(($2975)>>2)];
      var $2977=(($2976)&-1);
      var $2978=$2964 | $2977;
      var $2979=(($2978)|0);
      var $2980=$2;
      var $2981=(($2980+64)|0);
      var $2982=HEAP32[(($2981)>>2)];
      var $2983=$st;
      var $2984=(($2983+6)|0);
      var $2985=$2984;
      var $2986=HEAP16[(($2985)>>1)];
      var $2987=(($2986)&65535);
      var $2988=(($2982+($2987<<2))|0);
      var $2989=$2988;
      var $2990=$2989;
      HEAPF32[(($2990)>>2)]=$2979;
      __label__ = 124; break;
    case 124: 
      __label__ = 5; break;
    case 125: 
      __label__ = 126; break;
    case 126: 
      var $2994=$st;
      var $2995=(($2994+8)|0);
      $st=$2995;
      var $2996=$2;
      var $2997=$st;
      _prog_print_statement($2996, $2997);
      var $2998=$st;
      var $2999=(($2998)|0);
      var $3000=HEAP16[(($2999)>>1)];
      var $3001=(($3000)&65535);
      if ((($3001)|0) == 0 || (($3001)|0) == 43) {
        __label__ = 128; break;
      }
      else if ((($3001)|0) == 1) {
        __label__ = 131; break;
      }
      else if ((($3001)|0) == 2) {
        __label__ = 132; break;
      }
      else if ((($3001)|0) == 3) {
        __label__ = 133; break;
      }
      else if ((($3001)|0) == 4) {
        __label__ = 134; break;
      }
      else if ((($3001)|0) == 5) {
        __label__ = 135; break;
      }
      else if ((($3001)|0) == 6) {
        __label__ = 139; break;
      }
      else if ((($3001)|0) == 7) {
        __label__ = 140; break;
      }
      else if ((($3001)|0) == 8) {
        __label__ = 141; break;
      }
      else if ((($3001)|0) == 9) {
        __label__ = 142; break;
      }
      else if ((($3001)|0) == 10) {
        __label__ = 143; break;
      }
      else if ((($3001)|0) == 11) {
        __label__ = 144; break;
      }
      else if ((($3001)|0) == 12) {
        __label__ = 148; break;
      }
      else if ((($3001)|0) == 13) {
        __label__ = 149; break;
      }
      else if ((($3001)|0) == 14) {
        __label__ = 150; break;
      }
      else if ((($3001)|0) == 15) {
        __label__ = 151; break;
      }
      else if ((($3001)|0) == 16) {
        __label__ = 152; break;
      }
      else if ((($3001)|0) == 17) {
        __label__ = 156; break;
      }
      else if ((($3001)|0) == 18) {
        __label__ = 157; break;
      }
      else if ((($3001)|0) == 19) {
        __label__ = 158; break;
      }
      else if ((($3001)|0) == 20) {
        __label__ = 159; break;
      }
      else if ((($3001)|0) == 21) {
        __label__ = 160; break;
      }
      else if ((($3001)|0) == 22) {
        __label__ = 161; break;
      }
      else if ((($3001)|0) == 23) {
        __label__ = 162; break;
      }
      else if ((($3001)|0) == 24 || (($3001)|0) == 26 || (($3001)|0) == 28 || (($3001)|0) == 27 || (($3001)|0) == 29) {
        __label__ = 163; break;
      }
      else if ((($3001)|0) == 25) {
        __label__ = 169; break;
      }
      else if ((($3001)|0) == 30) {
        __label__ = 176; break;
      }
      else if ((($3001)|0) == 31 || (($3001)|0) == 33 || (($3001)|0) == 34 || (($3001)|0) == 35 || (($3001)|0) == 36) {
        __label__ = 182; break;
      }
      else if ((($3001)|0) == 32) {
        __label__ = 183; break;
      }
      else if ((($3001)|0) == 37 || (($3001)|0) == 39 || (($3001)|0) == 40 || (($3001)|0) == 41 || (($3001)|0) == 42) {
        __label__ = 184; break;
      }
      else if ((($3001)|0) == 38) {
        __label__ = 191; break;
      }
      else if ((($3001)|0) == 44) {
        __label__ = 198; break;
      }
      else if ((($3001)|0) == 45) {
        __label__ = 199; break;
      }
      else if ((($3001)|0) == 46) {
        __label__ = 203; break;
      }
      else if ((($3001)|0) == 47) {
        __label__ = 206; break;
      }
      else if ((($3001)|0) == 48) {
        __label__ = 207; break;
      }
      else if ((($3001)|0) == 49) {
        __label__ = 208; break;
      }
      else if ((($3001)|0) == 50) {
        __label__ = 213; break;
      }
      else if ((($3001)|0) == 51 || (($3001)|0) == 52 || (($3001)|0) == 53 || (($3001)|0) == 54 || (($3001)|0) == 55 || (($3001)|0) == 56 || (($3001)|0) == 57 || (($3001)|0) == 58 || (($3001)|0) == 59) {
        __label__ = 218; break;
      }
      else if ((($3001)|0) == 60) {
        __label__ = 233; break;
      }
      else if ((($3001)|0) == 61) {
        __label__ = 234; break;
      }
      else if ((($3001)|0) == 62) {
        __label__ = 237; break;
      }
      else if ((($3001)|0) == 63) {
        __label__ = 240; break;
      }
      else if ((($3001)|0) == 64) {
        __label__ = 243; break;
      }
      else if ((($3001)|0) == 65) {
        __label__ = 244; break;
      }
      else {
      __label__ = 127; break;
      }
      
    case 127: 
      var $3003=$2;
      var $3004=$2;
      var $3005=(($3004)|0);
      var $3006=HEAP32[(($3005)>>2)];
      _qcvmerror($3003, ((STRING_TABLE.__str16)|0), (tempInt=STACKTOP,STACKTOP += 4,assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),HEAP32[((tempInt)>>2)]=$3006,tempInt));
      __label__ = 488; break;
    case 128: 
      var $3008=$2;
      var $3009=(($3008+64)|0);
      var $3010=HEAP32[(($3009)>>2)];
      var $3011=$st;
      var $3012=(($3011+2)|0);
      var $3013=$3012;
      var $3014=HEAP16[(($3013)>>1)];
      var $3015=(($3014)&65535);
      var $3016=(($3010+($3015<<2))|0);
      var $3017=$3016;
      var $3018=$3017;
      var $3019=(($3018)|0);
      var $3020=HEAP32[(($3019)>>2)];
      var $3021=$2;
      var $3022=(($3021+64)|0);
      var $3023=HEAP32[(($3022)>>2)];
      var $3024=(($3023+4)|0);
      var $3025=$3024;
      var $3026=$3025;
      var $3027=(($3026)|0);
      HEAP32[(($3027)>>2)]=$3020;
      var $3028=$2;
      var $3029=(($3028+64)|0);
      var $3030=HEAP32[(($3029)>>2)];
      var $3031=$st;
      var $3032=(($3031+2)|0);
      var $3033=$3032;
      var $3034=HEAP16[(($3033)>>1)];
      var $3035=(($3034)&65535);
      var $3036=(($3030+($3035<<2))|0);
      var $3037=$3036;
      var $3038=$3037;
      var $3039=(($3038+4)|0);
      var $3040=HEAP32[(($3039)>>2)];
      var $3041=$2;
      var $3042=(($3041+64)|0);
      var $3043=HEAP32[(($3042)>>2)];
      var $3044=(($3043+4)|0);
      var $3045=$3044;
      var $3046=$3045;
      var $3047=(($3046+4)|0);
      HEAP32[(($3047)>>2)]=$3040;
      var $3048=$2;
      var $3049=(($3048+64)|0);
      var $3050=HEAP32[(($3049)>>2)];
      var $3051=$st;
      var $3052=(($3051+2)|0);
      var $3053=$3052;
      var $3054=HEAP16[(($3053)>>1)];
      var $3055=(($3054)&65535);
      var $3056=(($3050+($3055<<2))|0);
      var $3057=$3056;
      var $3058=$3057;
      var $3059=(($3058+8)|0);
      var $3060=HEAP32[(($3059)>>2)];
      var $3061=$2;
      var $3062=(($3061+64)|0);
      var $3063=HEAP32[(($3062)>>2)];
      var $3064=(($3063+4)|0);
      var $3065=$3064;
      var $3066=$3065;
      var $3067=(($3066+8)|0);
      HEAP32[(($3067)>>2)]=$3060;
      var $3068=$2;
      var $3069=(($3068+4)|0);
      var $3070=HEAP32[(($3069)>>2)];
      var $3071=$2;
      var $3072=_prog_leavefunction($3071);
      var $3073=(($3070+($3072<<3))|0);
      $st=$3073;
      var $3074=$2;
      var $3075=(($3074+168)|0);
      var $3076=HEAP32[(($3075)>>2)];
      var $3077=(($3076)|0)!=0;
      if ($3077) { __label__ = 130; break; } else { __label__ = 129; break; }
    case 129: 
      __label__ = 488; break;
    case 130: 
      __label__ = 245; break;
    case 131: 
      var $3081=$2;
      var $3082=(($3081+64)|0);
      var $3083=HEAP32[(($3082)>>2)];
      var $3084=$st;
      var $3085=(($3084+2)|0);
      var $3086=$3085;
      var $3087=HEAP16[(($3086)>>1)];
      var $3088=(($3087)&65535);
      var $3089=(($3083+($3088<<2))|0);
      var $3090=$3089;
      var $3091=$3090;
      var $3092=HEAPF32[(($3091)>>2)];
      var $3093=$2;
      var $3094=(($3093+64)|0);
      var $3095=HEAP32[(($3094)>>2)];
      var $3096=$st;
      var $3097=(($3096+4)|0);
      var $3098=$3097;
      var $3099=HEAP16[(($3098)>>1)];
      var $3100=(($3099)&65535);
      var $3101=(($3095+($3100<<2))|0);
      var $3102=$3101;
      var $3103=$3102;
      var $3104=HEAPF32[(($3103)>>2)];
      var $3105=($3092)*($3104);
      var $3106=$2;
      var $3107=(($3106+64)|0);
      var $3108=HEAP32[(($3107)>>2)];
      var $3109=$st;
      var $3110=(($3109+6)|0);
      var $3111=$3110;
      var $3112=HEAP16[(($3111)>>1)];
      var $3113=(($3112)&65535);
      var $3114=(($3108+($3113<<2))|0);
      var $3115=$3114;
      var $3116=$3115;
      HEAPF32[(($3116)>>2)]=$3105;
      __label__ = 245; break;
    case 132: 
      var $3118=$2;
      var $3119=(($3118+64)|0);
      var $3120=HEAP32[(($3119)>>2)];
      var $3121=$st;
      var $3122=(($3121+2)|0);
      var $3123=$3122;
      var $3124=HEAP16[(($3123)>>1)];
      var $3125=(($3124)&65535);
      var $3126=(($3120+($3125<<2))|0);
      var $3127=$3126;
      var $3128=$3127;
      var $3129=(($3128)|0);
      var $3130=HEAPF32[(($3129)>>2)];
      var $3131=$2;
      var $3132=(($3131+64)|0);
      var $3133=HEAP32[(($3132)>>2)];
      var $3134=$st;
      var $3135=(($3134+4)|0);
      var $3136=$3135;
      var $3137=HEAP16[(($3136)>>1)];
      var $3138=(($3137)&65535);
      var $3139=(($3133+($3138<<2))|0);
      var $3140=$3139;
      var $3141=$3140;
      var $3142=(($3141)|0);
      var $3143=HEAPF32[(($3142)>>2)];
      var $3144=($3130)*($3143);
      var $3145=$2;
      var $3146=(($3145+64)|0);
      var $3147=HEAP32[(($3146)>>2)];
      var $3148=$st;
      var $3149=(($3148+2)|0);
      var $3150=$3149;
      var $3151=HEAP16[(($3150)>>1)];
      var $3152=(($3151)&65535);
      var $3153=(($3147+($3152<<2))|0);
      var $3154=$3153;
      var $3155=$3154;
      var $3156=(($3155+4)|0);
      var $3157=HEAPF32[(($3156)>>2)];
      var $3158=$2;
      var $3159=(($3158+64)|0);
      var $3160=HEAP32[(($3159)>>2)];
      var $3161=$st;
      var $3162=(($3161+4)|0);
      var $3163=$3162;
      var $3164=HEAP16[(($3163)>>1)];
      var $3165=(($3164)&65535);
      var $3166=(($3160+($3165<<2))|0);
      var $3167=$3166;
      var $3168=$3167;
      var $3169=(($3168+4)|0);
      var $3170=HEAPF32[(($3169)>>2)];
      var $3171=($3157)*($3170);
      var $3172=($3144)+($3171);
      var $3173=$2;
      var $3174=(($3173+64)|0);
      var $3175=HEAP32[(($3174)>>2)];
      var $3176=$st;
      var $3177=(($3176+2)|0);
      var $3178=$3177;
      var $3179=HEAP16[(($3178)>>1)];
      var $3180=(($3179)&65535);
      var $3181=(($3175+($3180<<2))|0);
      var $3182=$3181;
      var $3183=$3182;
      var $3184=(($3183+8)|0);
      var $3185=HEAPF32[(($3184)>>2)];
      var $3186=$2;
      var $3187=(($3186+64)|0);
      var $3188=HEAP32[(($3187)>>2)];
      var $3189=$st;
      var $3190=(($3189+4)|0);
      var $3191=$3190;
      var $3192=HEAP16[(($3191)>>1)];
      var $3193=(($3192)&65535);
      var $3194=(($3188+($3193<<2))|0);
      var $3195=$3194;
      var $3196=$3195;
      var $3197=(($3196+8)|0);
      var $3198=HEAPF32[(($3197)>>2)];
      var $3199=($3185)*($3198);
      var $3200=($3172)+($3199);
      var $3201=$2;
      var $3202=(($3201+64)|0);
      var $3203=HEAP32[(($3202)>>2)];
      var $3204=$st;
      var $3205=(($3204+6)|0);
      var $3206=$3205;
      var $3207=HEAP16[(($3206)>>1)];
      var $3208=(($3207)&65535);
      var $3209=(($3203+($3208<<2))|0);
      var $3210=$3209;
      var $3211=$3210;
      HEAPF32[(($3211)>>2)]=$3200;
      __label__ = 245; break;
    case 133: 
      var $3213=$2;
      var $3214=(($3213+64)|0);
      var $3215=HEAP32[(($3214)>>2)];
      var $3216=$st;
      var $3217=(($3216+2)|0);
      var $3218=$3217;
      var $3219=HEAP16[(($3218)>>1)];
      var $3220=(($3219)&65535);
      var $3221=(($3215+($3220<<2))|0);
      var $3222=$3221;
      var $3223=$3222;
      var $3224=HEAPF32[(($3223)>>2)];
      var $3225=$2;
      var $3226=(($3225+64)|0);
      var $3227=HEAP32[(($3226)>>2)];
      var $3228=$st;
      var $3229=(($3228+4)|0);
      var $3230=$3229;
      var $3231=HEAP16[(($3230)>>1)];
      var $3232=(($3231)&65535);
      var $3233=(($3227+($3232<<2))|0);
      var $3234=$3233;
      var $3235=$3234;
      var $3236=(($3235)|0);
      var $3237=HEAPF32[(($3236)>>2)];
      var $3238=($3224)*($3237);
      var $3239=$2;
      var $3240=(($3239+64)|0);
      var $3241=HEAP32[(($3240)>>2)];
      var $3242=$st;
      var $3243=(($3242+6)|0);
      var $3244=$3243;
      var $3245=HEAP16[(($3244)>>1)];
      var $3246=(($3245)&65535);
      var $3247=(($3241+($3246<<2))|0);
      var $3248=$3247;
      var $3249=$3248;
      var $3250=(($3249)|0);
      HEAPF32[(($3250)>>2)]=$3238;
      var $3251=$2;
      var $3252=(($3251+64)|0);
      var $3253=HEAP32[(($3252)>>2)];
      var $3254=$st;
      var $3255=(($3254+2)|0);
      var $3256=$3255;
      var $3257=HEAP16[(($3256)>>1)];
      var $3258=(($3257)&65535);
      var $3259=(($3253+($3258<<2))|0);
      var $3260=$3259;
      var $3261=$3260;
      var $3262=HEAPF32[(($3261)>>2)];
      var $3263=$2;
      var $3264=(($3263+64)|0);
      var $3265=HEAP32[(($3264)>>2)];
      var $3266=$st;
      var $3267=(($3266+4)|0);
      var $3268=$3267;
      var $3269=HEAP16[(($3268)>>1)];
      var $3270=(($3269)&65535);
      var $3271=(($3265+($3270<<2))|0);
      var $3272=$3271;
      var $3273=$3272;
      var $3274=(($3273+4)|0);
      var $3275=HEAPF32[(($3274)>>2)];
      var $3276=($3262)*($3275);
      var $3277=$2;
      var $3278=(($3277+64)|0);
      var $3279=HEAP32[(($3278)>>2)];
      var $3280=$st;
      var $3281=(($3280+6)|0);
      var $3282=$3281;
      var $3283=HEAP16[(($3282)>>1)];
      var $3284=(($3283)&65535);
      var $3285=(($3279+($3284<<2))|0);
      var $3286=$3285;
      var $3287=$3286;
      var $3288=(($3287+4)|0);
      HEAPF32[(($3288)>>2)]=$3276;
      var $3289=$2;
      var $3290=(($3289+64)|0);
      var $3291=HEAP32[(($3290)>>2)];
      var $3292=$st;
      var $3293=(($3292+2)|0);
      var $3294=$3293;
      var $3295=HEAP16[(($3294)>>1)];
      var $3296=(($3295)&65535);
      var $3297=(($3291+($3296<<2))|0);
      var $3298=$3297;
      var $3299=$3298;
      var $3300=HEAPF32[(($3299)>>2)];
      var $3301=$2;
      var $3302=(($3301+64)|0);
      var $3303=HEAP32[(($3302)>>2)];
      var $3304=$st;
      var $3305=(($3304+4)|0);
      var $3306=$3305;
      var $3307=HEAP16[(($3306)>>1)];
      var $3308=(($3307)&65535);
      var $3309=(($3303+($3308<<2))|0);
      var $3310=$3309;
      var $3311=$3310;
      var $3312=(($3311+8)|0);
      var $3313=HEAPF32[(($3312)>>2)];
      var $3314=($3300)*($3313);
      var $3315=$2;
      var $3316=(($3315+64)|0);
      var $3317=HEAP32[(($3316)>>2)];
      var $3318=$st;
      var $3319=(($3318+6)|0);
      var $3320=$3319;
      var $3321=HEAP16[(($3320)>>1)];
      var $3322=(($3321)&65535);
      var $3323=(($3317+($3322<<2))|0);
      var $3324=$3323;
      var $3325=$3324;
      var $3326=(($3325+8)|0);
      HEAPF32[(($3326)>>2)]=$3314;
      __label__ = 245; break;
    case 134: 
      var $3328=$2;
      var $3329=(($3328+64)|0);
      var $3330=HEAP32[(($3329)>>2)];
      var $3331=$st;
      var $3332=(($3331+4)|0);
      var $3333=$3332;
      var $3334=HEAP16[(($3333)>>1)];
      var $3335=(($3334)&65535);
      var $3336=(($3330+($3335<<2))|0);
      var $3337=$3336;
      var $3338=$3337;
      var $3339=HEAPF32[(($3338)>>2)];
      var $3340=$2;
      var $3341=(($3340+64)|0);
      var $3342=HEAP32[(($3341)>>2)];
      var $3343=$st;
      var $3344=(($3343+2)|0);
      var $3345=$3344;
      var $3346=HEAP16[(($3345)>>1)];
      var $3347=(($3346)&65535);
      var $3348=(($3342+($3347<<2))|0);
      var $3349=$3348;
      var $3350=$3349;
      var $3351=(($3350)|0);
      var $3352=HEAPF32[(($3351)>>2)];
      var $3353=($3339)*($3352);
      var $3354=$2;
      var $3355=(($3354+64)|0);
      var $3356=HEAP32[(($3355)>>2)];
      var $3357=$st;
      var $3358=(($3357+6)|0);
      var $3359=$3358;
      var $3360=HEAP16[(($3359)>>1)];
      var $3361=(($3360)&65535);
      var $3362=(($3356+($3361<<2))|0);
      var $3363=$3362;
      var $3364=$3363;
      var $3365=(($3364)|0);
      HEAPF32[(($3365)>>2)]=$3353;
      var $3366=$2;
      var $3367=(($3366+64)|0);
      var $3368=HEAP32[(($3367)>>2)];
      var $3369=$st;
      var $3370=(($3369+4)|0);
      var $3371=$3370;
      var $3372=HEAP16[(($3371)>>1)];
      var $3373=(($3372)&65535);
      var $3374=(($3368+($3373<<2))|0);
      var $3375=$3374;
      var $3376=$3375;
      var $3377=HEAPF32[(($3376)>>2)];
      var $3378=$2;
      var $3379=(($3378+64)|0);
      var $3380=HEAP32[(($3379)>>2)];
      var $3381=$st;
      var $3382=(($3381+2)|0);
      var $3383=$3382;
      var $3384=HEAP16[(($3383)>>1)];
      var $3385=(($3384)&65535);
      var $3386=(($3380+($3385<<2))|0);
      var $3387=$3386;
      var $3388=$3387;
      var $3389=(($3388+4)|0);
      var $3390=HEAPF32[(($3389)>>2)];
      var $3391=($3377)*($3390);
      var $3392=$2;
      var $3393=(($3392+64)|0);
      var $3394=HEAP32[(($3393)>>2)];
      var $3395=$st;
      var $3396=(($3395+6)|0);
      var $3397=$3396;
      var $3398=HEAP16[(($3397)>>1)];
      var $3399=(($3398)&65535);
      var $3400=(($3394+($3399<<2))|0);
      var $3401=$3400;
      var $3402=$3401;
      var $3403=(($3402+4)|0);
      HEAPF32[(($3403)>>2)]=$3391;
      var $3404=$2;
      var $3405=(($3404+64)|0);
      var $3406=HEAP32[(($3405)>>2)];
      var $3407=$st;
      var $3408=(($3407+4)|0);
      var $3409=$3408;
      var $3410=HEAP16[(($3409)>>1)];
      var $3411=(($3410)&65535);
      var $3412=(($3406+($3411<<2))|0);
      var $3413=$3412;
      var $3414=$3413;
      var $3415=HEAPF32[(($3414)>>2)];
      var $3416=$2;
      var $3417=(($3416+64)|0);
      var $3418=HEAP32[(($3417)>>2)];
      var $3419=$st;
      var $3420=(($3419+2)|0);
      var $3421=$3420;
      var $3422=HEAP16[(($3421)>>1)];
      var $3423=(($3422)&65535);
      var $3424=(($3418+($3423<<2))|0);
      var $3425=$3424;
      var $3426=$3425;
      var $3427=(($3426+8)|0);
      var $3428=HEAPF32[(($3427)>>2)];
      var $3429=($3415)*($3428);
      var $3430=$2;
      var $3431=(($3430+64)|0);
      var $3432=HEAP32[(($3431)>>2)];
      var $3433=$st;
      var $3434=(($3433+6)|0);
      var $3435=$3434;
      var $3436=HEAP16[(($3435)>>1)];
      var $3437=(($3436)&65535);
      var $3438=(($3432+($3437<<2))|0);
      var $3439=$3438;
      var $3440=$3439;
      var $3441=(($3440+8)|0);
      HEAPF32[(($3441)>>2)]=$3429;
      __label__ = 245; break;
    case 135: 
      var $3443=$2;
      var $3444=(($3443+64)|0);
      var $3445=HEAP32[(($3444)>>2)];
      var $3446=$st;
      var $3447=(($3446+4)|0);
      var $3448=$3447;
      var $3449=HEAP16[(($3448)>>1)];
      var $3450=(($3449)&65535);
      var $3451=(($3445+($3450<<2))|0);
      var $3452=$3451;
      var $3453=$3452;
      var $3454=HEAPF32[(($3453)>>2)];
      var $3455=$3454 != 0;
      if ($3455) { __label__ = 136; break; } else { __label__ = 137; break; }
    case 136: 
      var $3457=$2;
      var $3458=(($3457+64)|0);
      var $3459=HEAP32[(($3458)>>2)];
      var $3460=$st;
      var $3461=(($3460+2)|0);
      var $3462=$3461;
      var $3463=HEAP16[(($3462)>>1)];
      var $3464=(($3463)&65535);
      var $3465=(($3459+($3464<<2))|0);
      var $3466=$3465;
      var $3467=$3466;
      var $3468=HEAPF32[(($3467)>>2)];
      var $3469=$2;
      var $3470=(($3469+64)|0);
      var $3471=HEAP32[(($3470)>>2)];
      var $3472=$st;
      var $3473=(($3472+4)|0);
      var $3474=$3473;
      var $3475=HEAP16[(($3474)>>1)];
      var $3476=(($3475)&65535);
      var $3477=(($3471+($3476<<2))|0);
      var $3478=$3477;
      var $3479=$3478;
      var $3480=HEAPF32[(($3479)>>2)];
      var $3481=($3468)/($3480);
      var $3482=$2;
      var $3483=(($3482+64)|0);
      var $3484=HEAP32[(($3483)>>2)];
      var $3485=$st;
      var $3486=(($3485+6)|0);
      var $3487=$3486;
      var $3488=HEAP16[(($3487)>>1)];
      var $3489=(($3488)&65535);
      var $3490=(($3484+($3489<<2))|0);
      var $3491=$3490;
      var $3492=$3491;
      HEAPF32[(($3492)>>2)]=$3481;
      __label__ = 138; break;
    case 137: 
      var $3494=$2;
      var $3495=(($3494+64)|0);
      var $3496=HEAP32[(($3495)>>2)];
      var $3497=$st;
      var $3498=(($3497+6)|0);
      var $3499=$3498;
      var $3500=HEAP16[(($3499)>>1)];
      var $3501=(($3500)&65535);
      var $3502=(($3496+($3501<<2))|0);
      var $3503=$3502;
      var $3504=$3503;
      HEAPF32[(($3504)>>2)]=0;
      __label__ = 138; break;
    case 138: 
      __label__ = 245; break;
    case 139: 
      var $3507=$2;
      var $3508=(($3507+64)|0);
      var $3509=HEAP32[(($3508)>>2)];
      var $3510=$st;
      var $3511=(($3510+2)|0);
      var $3512=$3511;
      var $3513=HEAP16[(($3512)>>1)];
      var $3514=(($3513)&65535);
      var $3515=(($3509+($3514<<2))|0);
      var $3516=$3515;
      var $3517=$3516;
      var $3518=HEAPF32[(($3517)>>2)];
      var $3519=$2;
      var $3520=(($3519+64)|0);
      var $3521=HEAP32[(($3520)>>2)];
      var $3522=$st;
      var $3523=(($3522+4)|0);
      var $3524=$3523;
      var $3525=HEAP16[(($3524)>>1)];
      var $3526=(($3525)&65535);
      var $3527=(($3521+($3526<<2))|0);
      var $3528=$3527;
      var $3529=$3528;
      var $3530=HEAPF32[(($3529)>>2)];
      var $3531=($3518)+($3530);
      var $3532=$2;
      var $3533=(($3532+64)|0);
      var $3534=HEAP32[(($3533)>>2)];
      var $3535=$st;
      var $3536=(($3535+6)|0);
      var $3537=$3536;
      var $3538=HEAP16[(($3537)>>1)];
      var $3539=(($3538)&65535);
      var $3540=(($3534+($3539<<2))|0);
      var $3541=$3540;
      var $3542=$3541;
      HEAPF32[(($3542)>>2)]=$3531;
      __label__ = 245; break;
    case 140: 
      var $3544=$2;
      var $3545=(($3544+64)|0);
      var $3546=HEAP32[(($3545)>>2)];
      var $3547=$st;
      var $3548=(($3547+2)|0);
      var $3549=$3548;
      var $3550=HEAP16[(($3549)>>1)];
      var $3551=(($3550)&65535);
      var $3552=(($3546+($3551<<2))|0);
      var $3553=$3552;
      var $3554=$3553;
      var $3555=(($3554)|0);
      var $3556=HEAPF32[(($3555)>>2)];
      var $3557=$2;
      var $3558=(($3557+64)|0);
      var $3559=HEAP32[(($3558)>>2)];
      var $3560=$st;
      var $3561=(($3560+4)|0);
      var $3562=$3561;
      var $3563=HEAP16[(($3562)>>1)];
      var $3564=(($3563)&65535);
      var $3565=(($3559+($3564<<2))|0);
      var $3566=$3565;
      var $3567=$3566;
      var $3568=(($3567)|0);
      var $3569=HEAPF32[(($3568)>>2)];
      var $3570=($3556)+($3569);
      var $3571=$2;
      var $3572=(($3571+64)|0);
      var $3573=HEAP32[(($3572)>>2)];
      var $3574=$st;
      var $3575=(($3574+6)|0);
      var $3576=$3575;
      var $3577=HEAP16[(($3576)>>1)];
      var $3578=(($3577)&65535);
      var $3579=(($3573+($3578<<2))|0);
      var $3580=$3579;
      var $3581=$3580;
      var $3582=(($3581)|0);
      HEAPF32[(($3582)>>2)]=$3570;
      var $3583=$2;
      var $3584=(($3583+64)|0);
      var $3585=HEAP32[(($3584)>>2)];
      var $3586=$st;
      var $3587=(($3586+2)|0);
      var $3588=$3587;
      var $3589=HEAP16[(($3588)>>1)];
      var $3590=(($3589)&65535);
      var $3591=(($3585+($3590<<2))|0);
      var $3592=$3591;
      var $3593=$3592;
      var $3594=(($3593+4)|0);
      var $3595=HEAPF32[(($3594)>>2)];
      var $3596=$2;
      var $3597=(($3596+64)|0);
      var $3598=HEAP32[(($3597)>>2)];
      var $3599=$st;
      var $3600=(($3599+4)|0);
      var $3601=$3600;
      var $3602=HEAP16[(($3601)>>1)];
      var $3603=(($3602)&65535);
      var $3604=(($3598+($3603<<2))|0);
      var $3605=$3604;
      var $3606=$3605;
      var $3607=(($3606+4)|0);
      var $3608=HEAPF32[(($3607)>>2)];
      var $3609=($3595)+($3608);
      var $3610=$2;
      var $3611=(($3610+64)|0);
      var $3612=HEAP32[(($3611)>>2)];
      var $3613=$st;
      var $3614=(($3613+6)|0);
      var $3615=$3614;
      var $3616=HEAP16[(($3615)>>1)];
      var $3617=(($3616)&65535);
      var $3618=(($3612+($3617<<2))|0);
      var $3619=$3618;
      var $3620=$3619;
      var $3621=(($3620+4)|0);
      HEAPF32[(($3621)>>2)]=$3609;
      var $3622=$2;
      var $3623=(($3622+64)|0);
      var $3624=HEAP32[(($3623)>>2)];
      var $3625=$st;
      var $3626=(($3625+2)|0);
      var $3627=$3626;
      var $3628=HEAP16[(($3627)>>1)];
      var $3629=(($3628)&65535);
      var $3630=(($3624+($3629<<2))|0);
      var $3631=$3630;
      var $3632=$3631;
      var $3633=(($3632+8)|0);
      var $3634=HEAPF32[(($3633)>>2)];
      var $3635=$2;
      var $3636=(($3635+64)|0);
      var $3637=HEAP32[(($3636)>>2)];
      var $3638=$st;
      var $3639=(($3638+4)|0);
      var $3640=$3639;
      var $3641=HEAP16[(($3640)>>1)];
      var $3642=(($3641)&65535);
      var $3643=(($3637+($3642<<2))|0);
      var $3644=$3643;
      var $3645=$3644;
      var $3646=(($3645+8)|0);
      var $3647=HEAPF32[(($3646)>>2)];
      var $3648=($3634)+($3647);
      var $3649=$2;
      var $3650=(($3649+64)|0);
      var $3651=HEAP32[(($3650)>>2)];
      var $3652=$st;
      var $3653=(($3652+6)|0);
      var $3654=$3653;
      var $3655=HEAP16[(($3654)>>1)];
      var $3656=(($3655)&65535);
      var $3657=(($3651+($3656<<2))|0);
      var $3658=$3657;
      var $3659=$3658;
      var $3660=(($3659+8)|0);
      HEAPF32[(($3660)>>2)]=$3648;
      __label__ = 245; break;
    case 141: 
      var $3662=$2;
      var $3663=(($3662+64)|0);
      var $3664=HEAP32[(($3663)>>2)];
      var $3665=$st;
      var $3666=(($3665+2)|0);
      var $3667=$3666;
      var $3668=HEAP16[(($3667)>>1)];
      var $3669=(($3668)&65535);
      var $3670=(($3664+($3669<<2))|0);
      var $3671=$3670;
      var $3672=$3671;
      var $3673=HEAPF32[(($3672)>>2)];
      var $3674=$2;
      var $3675=(($3674+64)|0);
      var $3676=HEAP32[(($3675)>>2)];
      var $3677=$st;
      var $3678=(($3677+4)|0);
      var $3679=$3678;
      var $3680=HEAP16[(($3679)>>1)];
      var $3681=(($3680)&65535);
      var $3682=(($3676+($3681<<2))|0);
      var $3683=$3682;
      var $3684=$3683;
      var $3685=HEAPF32[(($3684)>>2)];
      var $3686=($3673)-($3685);
      var $3687=$2;
      var $3688=(($3687+64)|0);
      var $3689=HEAP32[(($3688)>>2)];
      var $3690=$st;
      var $3691=(($3690+6)|0);
      var $3692=$3691;
      var $3693=HEAP16[(($3692)>>1)];
      var $3694=(($3693)&65535);
      var $3695=(($3689+($3694<<2))|0);
      var $3696=$3695;
      var $3697=$3696;
      HEAPF32[(($3697)>>2)]=$3686;
      __label__ = 245; break;
    case 142: 
      var $3699=$2;
      var $3700=(($3699+64)|0);
      var $3701=HEAP32[(($3700)>>2)];
      var $3702=$st;
      var $3703=(($3702+2)|0);
      var $3704=$3703;
      var $3705=HEAP16[(($3704)>>1)];
      var $3706=(($3705)&65535);
      var $3707=(($3701+($3706<<2))|0);
      var $3708=$3707;
      var $3709=$3708;
      var $3710=(($3709)|0);
      var $3711=HEAPF32[(($3710)>>2)];
      var $3712=$2;
      var $3713=(($3712+64)|0);
      var $3714=HEAP32[(($3713)>>2)];
      var $3715=$st;
      var $3716=(($3715+4)|0);
      var $3717=$3716;
      var $3718=HEAP16[(($3717)>>1)];
      var $3719=(($3718)&65535);
      var $3720=(($3714+($3719<<2))|0);
      var $3721=$3720;
      var $3722=$3721;
      var $3723=(($3722)|0);
      var $3724=HEAPF32[(($3723)>>2)];
      var $3725=($3711)-($3724);
      var $3726=$2;
      var $3727=(($3726+64)|0);
      var $3728=HEAP32[(($3727)>>2)];
      var $3729=$st;
      var $3730=(($3729+6)|0);
      var $3731=$3730;
      var $3732=HEAP16[(($3731)>>1)];
      var $3733=(($3732)&65535);
      var $3734=(($3728+($3733<<2))|0);
      var $3735=$3734;
      var $3736=$3735;
      var $3737=(($3736)|0);
      HEAPF32[(($3737)>>2)]=$3725;
      var $3738=$2;
      var $3739=(($3738+64)|0);
      var $3740=HEAP32[(($3739)>>2)];
      var $3741=$st;
      var $3742=(($3741+2)|0);
      var $3743=$3742;
      var $3744=HEAP16[(($3743)>>1)];
      var $3745=(($3744)&65535);
      var $3746=(($3740+($3745<<2))|0);
      var $3747=$3746;
      var $3748=$3747;
      var $3749=(($3748+4)|0);
      var $3750=HEAPF32[(($3749)>>2)];
      var $3751=$2;
      var $3752=(($3751+64)|0);
      var $3753=HEAP32[(($3752)>>2)];
      var $3754=$st;
      var $3755=(($3754+4)|0);
      var $3756=$3755;
      var $3757=HEAP16[(($3756)>>1)];
      var $3758=(($3757)&65535);
      var $3759=(($3753+($3758<<2))|0);
      var $3760=$3759;
      var $3761=$3760;
      var $3762=(($3761+4)|0);
      var $3763=HEAPF32[(($3762)>>2)];
      var $3764=($3750)-($3763);
      var $3765=$2;
      var $3766=(($3765+64)|0);
      var $3767=HEAP32[(($3766)>>2)];
      var $3768=$st;
      var $3769=(($3768+6)|0);
      var $3770=$3769;
      var $3771=HEAP16[(($3770)>>1)];
      var $3772=(($3771)&65535);
      var $3773=(($3767+($3772<<2))|0);
      var $3774=$3773;
      var $3775=$3774;
      var $3776=(($3775+4)|0);
      HEAPF32[(($3776)>>2)]=$3764;
      var $3777=$2;
      var $3778=(($3777+64)|0);
      var $3779=HEAP32[(($3778)>>2)];
      var $3780=$st;
      var $3781=(($3780+2)|0);
      var $3782=$3781;
      var $3783=HEAP16[(($3782)>>1)];
      var $3784=(($3783)&65535);
      var $3785=(($3779+($3784<<2))|0);
      var $3786=$3785;
      var $3787=$3786;
      var $3788=(($3787+8)|0);
      var $3789=HEAPF32[(($3788)>>2)];
      var $3790=$2;
      var $3791=(($3790+64)|0);
      var $3792=HEAP32[(($3791)>>2)];
      var $3793=$st;
      var $3794=(($3793+4)|0);
      var $3795=$3794;
      var $3796=HEAP16[(($3795)>>1)];
      var $3797=(($3796)&65535);
      var $3798=(($3792+($3797<<2))|0);
      var $3799=$3798;
      var $3800=$3799;
      var $3801=(($3800+8)|0);
      var $3802=HEAPF32[(($3801)>>2)];
      var $3803=($3789)-($3802);
      var $3804=$2;
      var $3805=(($3804+64)|0);
      var $3806=HEAP32[(($3805)>>2)];
      var $3807=$st;
      var $3808=(($3807+6)|0);
      var $3809=$3808;
      var $3810=HEAP16[(($3809)>>1)];
      var $3811=(($3810)&65535);
      var $3812=(($3806+($3811<<2))|0);
      var $3813=$3812;
      var $3814=$3813;
      var $3815=(($3814+8)|0);
      HEAPF32[(($3815)>>2)]=$3803;
      __label__ = 245; break;
    case 143: 
      var $3817=$2;
      var $3818=(($3817+64)|0);
      var $3819=HEAP32[(($3818)>>2)];
      var $3820=$st;
      var $3821=(($3820+2)|0);
      var $3822=$3821;
      var $3823=HEAP16[(($3822)>>1)];
      var $3824=(($3823)&65535);
      var $3825=(($3819+($3824<<2))|0);
      var $3826=$3825;
      var $3827=$3826;
      var $3828=HEAPF32[(($3827)>>2)];
      var $3829=$2;
      var $3830=(($3829+64)|0);
      var $3831=HEAP32[(($3830)>>2)];
      var $3832=$st;
      var $3833=(($3832+4)|0);
      var $3834=$3833;
      var $3835=HEAP16[(($3834)>>1)];
      var $3836=(($3835)&65535);
      var $3837=(($3831+($3836<<2))|0);
      var $3838=$3837;
      var $3839=$3838;
      var $3840=HEAPF32[(($3839)>>2)];
      var $3841=$3828 == $3840;
      var $3842=(($3841)&1);
      var $3843=(($3842)|0);
      var $3844=$2;
      var $3845=(($3844+64)|0);
      var $3846=HEAP32[(($3845)>>2)];
      var $3847=$st;
      var $3848=(($3847+6)|0);
      var $3849=$3848;
      var $3850=HEAP16[(($3849)>>1)];
      var $3851=(($3850)&65535);
      var $3852=(($3846+($3851<<2))|0);
      var $3853=$3852;
      var $3854=$3853;
      HEAPF32[(($3854)>>2)]=$3843;
      __label__ = 245; break;
    case 144: 
      var $3856=$2;
      var $3857=(($3856+64)|0);
      var $3858=HEAP32[(($3857)>>2)];
      var $3859=$st;
      var $3860=(($3859+2)|0);
      var $3861=$3860;
      var $3862=HEAP16[(($3861)>>1)];
      var $3863=(($3862)&65535);
      var $3864=(($3858+($3863<<2))|0);
      var $3865=$3864;
      var $3866=$3865;
      var $3867=(($3866)|0);
      var $3868=HEAPF32[(($3867)>>2)];
      var $3869=$2;
      var $3870=(($3869+64)|0);
      var $3871=HEAP32[(($3870)>>2)];
      var $3872=$st;
      var $3873=(($3872+4)|0);
      var $3874=$3873;
      var $3875=HEAP16[(($3874)>>1)];
      var $3876=(($3875)&65535);
      var $3877=(($3871+($3876<<2))|0);
      var $3878=$3877;
      var $3879=$3878;
      var $3880=(($3879)|0);
      var $3881=HEAPF32[(($3880)>>2)];
      var $3882=$3868 == $3881;
      if ($3882) { __label__ = 145; break; } else { var $3940 = 0;__label__ = 147; break; }
    case 145: 
      var $3884=$2;
      var $3885=(($3884+64)|0);
      var $3886=HEAP32[(($3885)>>2)];
      var $3887=$st;
      var $3888=(($3887+2)|0);
      var $3889=$3888;
      var $3890=HEAP16[(($3889)>>1)];
      var $3891=(($3890)&65535);
      var $3892=(($3886+($3891<<2))|0);
      var $3893=$3892;
      var $3894=$3893;
      var $3895=(($3894+4)|0);
      var $3896=HEAPF32[(($3895)>>2)];
      var $3897=$2;
      var $3898=(($3897+64)|0);
      var $3899=HEAP32[(($3898)>>2)];
      var $3900=$st;
      var $3901=(($3900+4)|0);
      var $3902=$3901;
      var $3903=HEAP16[(($3902)>>1)];
      var $3904=(($3903)&65535);
      var $3905=(($3899+($3904<<2))|0);
      var $3906=$3905;
      var $3907=$3906;
      var $3908=(($3907+4)|0);
      var $3909=HEAPF32[(($3908)>>2)];
      var $3910=$3896 == $3909;
      if ($3910) { __label__ = 146; break; } else { var $3940 = 0;__label__ = 147; break; }
    case 146: 
      var $3912=$2;
      var $3913=(($3912+64)|0);
      var $3914=HEAP32[(($3913)>>2)];
      var $3915=$st;
      var $3916=(($3915+2)|0);
      var $3917=$3916;
      var $3918=HEAP16[(($3917)>>1)];
      var $3919=(($3918)&65535);
      var $3920=(($3914+($3919<<2))|0);
      var $3921=$3920;
      var $3922=$3921;
      var $3923=(($3922+8)|0);
      var $3924=HEAPF32[(($3923)>>2)];
      var $3925=$2;
      var $3926=(($3925+64)|0);
      var $3927=HEAP32[(($3926)>>2)];
      var $3928=$st;
      var $3929=(($3928+4)|0);
      var $3930=$3929;
      var $3931=HEAP16[(($3930)>>1)];
      var $3932=(($3931)&65535);
      var $3933=(($3927+($3932<<2))|0);
      var $3934=$3933;
      var $3935=$3934;
      var $3936=(($3935+8)|0);
      var $3937=HEAPF32[(($3936)>>2)];
      var $3938=$3924 == $3937;
      var $3940 = $3938;__label__ = 147; break;
    case 147: 
      var $3940;
      var $3941=(($3940)&1);
      var $3942=(($3941)|0);
      var $3943=$2;
      var $3944=(($3943+64)|0);
      var $3945=HEAP32[(($3944)>>2)];
      var $3946=$st;
      var $3947=(($3946+6)|0);
      var $3948=$3947;
      var $3949=HEAP16[(($3948)>>1)];
      var $3950=(($3949)&65535);
      var $3951=(($3945+($3950<<2))|0);
      var $3952=$3951;
      var $3953=$3952;
      HEAPF32[(($3953)>>2)]=$3942;
      __label__ = 245; break;
    case 148: 
      var $3955=$2;
      var $3956=$2;
      var $3957=(($3956+64)|0);
      var $3958=HEAP32[(($3957)>>2)];
      var $3959=$st;
      var $3960=(($3959+2)|0);
      var $3961=$3960;
      var $3962=HEAP16[(($3961)>>1)];
      var $3963=(($3962)&65535);
      var $3964=(($3958+($3963<<2))|0);
      var $3965=$3964;
      var $3966=$3965;
      var $3967=HEAP32[(($3966)>>2)];
      var $3968=_prog_getstring($3955, $3967);
      var $3969=$2;
      var $3970=$2;
      var $3971=(($3970+64)|0);
      var $3972=HEAP32[(($3971)>>2)];
      var $3973=$st;
      var $3974=(($3973+4)|0);
      var $3975=$3974;
      var $3976=HEAP16[(($3975)>>1)];
      var $3977=(($3976)&65535);
      var $3978=(($3972+($3977<<2))|0);
      var $3979=$3978;
      var $3980=$3979;
      var $3981=HEAP32[(($3980)>>2)];
      var $3982=_prog_getstring($3969, $3981);
      var $3983=_strcmp($3968, $3982);
      var $3984=(($3983)|0)!=0;
      var $3985=$3984 ^ 1;
      var $3986=(($3985)&1);
      var $3987=(($3986)|0);
      var $3988=$2;
      var $3989=(($3988+64)|0);
      var $3990=HEAP32[(($3989)>>2)];
      var $3991=$st;
      var $3992=(($3991+6)|0);
      var $3993=$3992;
      var $3994=HEAP16[(($3993)>>1)];
      var $3995=(($3994)&65535);
      var $3996=(($3990+($3995<<2))|0);
      var $3997=$3996;
      var $3998=$3997;
      HEAPF32[(($3998)>>2)]=$3987;
      __label__ = 245; break;
    case 149: 
      var $4000=$2;
      var $4001=(($4000+64)|0);
      var $4002=HEAP32[(($4001)>>2)];
      var $4003=$st;
      var $4004=(($4003+2)|0);
      var $4005=$4004;
      var $4006=HEAP16[(($4005)>>1)];
      var $4007=(($4006)&65535);
      var $4008=(($4002+($4007<<2))|0);
      var $4009=$4008;
      var $4010=$4009;
      var $4011=HEAP32[(($4010)>>2)];
      var $4012=$2;
      var $4013=(($4012+64)|0);
      var $4014=HEAP32[(($4013)>>2)];
      var $4015=$st;
      var $4016=(($4015+4)|0);
      var $4017=$4016;
      var $4018=HEAP16[(($4017)>>1)];
      var $4019=(($4018)&65535);
      var $4020=(($4014+($4019<<2))|0);
      var $4021=$4020;
      var $4022=$4021;
      var $4023=HEAP32[(($4022)>>2)];
      var $4024=(($4011)|0)==(($4023)|0);
      var $4025=(($4024)&1);
      var $4026=(($4025)|0);
      var $4027=$2;
      var $4028=(($4027+64)|0);
      var $4029=HEAP32[(($4028)>>2)];
      var $4030=$st;
      var $4031=(($4030+6)|0);
      var $4032=$4031;
      var $4033=HEAP16[(($4032)>>1)];
      var $4034=(($4033)&65535);
      var $4035=(($4029+($4034<<2))|0);
      var $4036=$4035;
      var $4037=$4036;
      HEAPF32[(($4037)>>2)]=$4026;
      __label__ = 245; break;
    case 150: 
      var $4039=$2;
      var $4040=(($4039+64)|0);
      var $4041=HEAP32[(($4040)>>2)];
      var $4042=$st;
      var $4043=(($4042+2)|0);
      var $4044=$4043;
      var $4045=HEAP16[(($4044)>>1)];
      var $4046=(($4045)&65535);
      var $4047=(($4041+($4046<<2))|0);
      var $4048=$4047;
      var $4049=$4048;
      var $4050=HEAP32[(($4049)>>2)];
      var $4051=$2;
      var $4052=(($4051+64)|0);
      var $4053=HEAP32[(($4052)>>2)];
      var $4054=$st;
      var $4055=(($4054+4)|0);
      var $4056=$4055;
      var $4057=HEAP16[(($4056)>>1)];
      var $4058=(($4057)&65535);
      var $4059=(($4053+($4058<<2))|0);
      var $4060=$4059;
      var $4061=$4060;
      var $4062=HEAP32[(($4061)>>2)];
      var $4063=(($4050)|0)==(($4062)|0);
      var $4064=(($4063)&1);
      var $4065=(($4064)|0);
      var $4066=$2;
      var $4067=(($4066+64)|0);
      var $4068=HEAP32[(($4067)>>2)];
      var $4069=$st;
      var $4070=(($4069+6)|0);
      var $4071=$4070;
      var $4072=HEAP16[(($4071)>>1)];
      var $4073=(($4072)&65535);
      var $4074=(($4068+($4073<<2))|0);
      var $4075=$4074;
      var $4076=$4075;
      HEAPF32[(($4076)>>2)]=$4065;
      __label__ = 245; break;
    case 151: 
      var $4078=$2;
      var $4079=(($4078+64)|0);
      var $4080=HEAP32[(($4079)>>2)];
      var $4081=$st;
      var $4082=(($4081+2)|0);
      var $4083=$4082;
      var $4084=HEAP16[(($4083)>>1)];
      var $4085=(($4084)&65535);
      var $4086=(($4080+($4085<<2))|0);
      var $4087=$4086;
      var $4088=$4087;
      var $4089=HEAPF32[(($4088)>>2)];
      var $4090=$2;
      var $4091=(($4090+64)|0);
      var $4092=HEAP32[(($4091)>>2)];
      var $4093=$st;
      var $4094=(($4093+4)|0);
      var $4095=$4094;
      var $4096=HEAP16[(($4095)>>1)];
      var $4097=(($4096)&65535);
      var $4098=(($4092+($4097<<2))|0);
      var $4099=$4098;
      var $4100=$4099;
      var $4101=HEAPF32[(($4100)>>2)];
      var $4102=$4089 != $4101;
      var $4103=(($4102)&1);
      var $4104=(($4103)|0);
      var $4105=$2;
      var $4106=(($4105+64)|0);
      var $4107=HEAP32[(($4106)>>2)];
      var $4108=$st;
      var $4109=(($4108+6)|0);
      var $4110=$4109;
      var $4111=HEAP16[(($4110)>>1)];
      var $4112=(($4111)&65535);
      var $4113=(($4107+($4112<<2))|0);
      var $4114=$4113;
      var $4115=$4114;
      HEAPF32[(($4115)>>2)]=$4104;
      __label__ = 245; break;
    case 152: 
      var $4117=$2;
      var $4118=(($4117+64)|0);
      var $4119=HEAP32[(($4118)>>2)];
      var $4120=$st;
      var $4121=(($4120+2)|0);
      var $4122=$4121;
      var $4123=HEAP16[(($4122)>>1)];
      var $4124=(($4123)&65535);
      var $4125=(($4119+($4124<<2))|0);
      var $4126=$4125;
      var $4127=$4126;
      var $4128=(($4127)|0);
      var $4129=HEAPF32[(($4128)>>2)];
      var $4130=$2;
      var $4131=(($4130+64)|0);
      var $4132=HEAP32[(($4131)>>2)];
      var $4133=$st;
      var $4134=(($4133+4)|0);
      var $4135=$4134;
      var $4136=HEAP16[(($4135)>>1)];
      var $4137=(($4136)&65535);
      var $4138=(($4132+($4137<<2))|0);
      var $4139=$4138;
      var $4140=$4139;
      var $4141=(($4140)|0);
      var $4142=HEAPF32[(($4141)>>2)];
      var $4143=$4129 != $4142;
      if ($4143) { var $4201 = 1;__label__ = 155; break; } else { __label__ = 153; break; }
    case 153: 
      var $4145=$2;
      var $4146=(($4145+64)|0);
      var $4147=HEAP32[(($4146)>>2)];
      var $4148=$st;
      var $4149=(($4148+2)|0);
      var $4150=$4149;
      var $4151=HEAP16[(($4150)>>1)];
      var $4152=(($4151)&65535);
      var $4153=(($4147+($4152<<2))|0);
      var $4154=$4153;
      var $4155=$4154;
      var $4156=(($4155+4)|0);
      var $4157=HEAPF32[(($4156)>>2)];
      var $4158=$2;
      var $4159=(($4158+64)|0);
      var $4160=HEAP32[(($4159)>>2)];
      var $4161=$st;
      var $4162=(($4161+4)|0);
      var $4163=$4162;
      var $4164=HEAP16[(($4163)>>1)];
      var $4165=(($4164)&65535);
      var $4166=(($4160+($4165<<2))|0);
      var $4167=$4166;
      var $4168=$4167;
      var $4169=(($4168+4)|0);
      var $4170=HEAPF32[(($4169)>>2)];
      var $4171=$4157 != $4170;
      if ($4171) { var $4201 = 1;__label__ = 155; break; } else { __label__ = 154; break; }
    case 154: 
      var $4173=$2;
      var $4174=(($4173+64)|0);
      var $4175=HEAP32[(($4174)>>2)];
      var $4176=$st;
      var $4177=(($4176+2)|0);
      var $4178=$4177;
      var $4179=HEAP16[(($4178)>>1)];
      var $4180=(($4179)&65535);
      var $4181=(($4175+($4180<<2))|0);
      var $4182=$4181;
      var $4183=$4182;
      var $4184=(($4183+8)|0);
      var $4185=HEAPF32[(($4184)>>2)];
      var $4186=$2;
      var $4187=(($4186+64)|0);
      var $4188=HEAP32[(($4187)>>2)];
      var $4189=$st;
      var $4190=(($4189+4)|0);
      var $4191=$4190;
      var $4192=HEAP16[(($4191)>>1)];
      var $4193=(($4192)&65535);
      var $4194=(($4188+($4193<<2))|0);
      var $4195=$4194;
      var $4196=$4195;
      var $4197=(($4196+8)|0);
      var $4198=HEAPF32[(($4197)>>2)];
      var $4199=$4185 != $4198;
      var $4201 = $4199;__label__ = 155; break;
    case 155: 
      var $4201;
      var $4202=(($4201)&1);
      var $4203=(($4202)|0);
      var $4204=$2;
      var $4205=(($4204+64)|0);
      var $4206=HEAP32[(($4205)>>2)];
      var $4207=$st;
      var $4208=(($4207+6)|0);
      var $4209=$4208;
      var $4210=HEAP16[(($4209)>>1)];
      var $4211=(($4210)&65535);
      var $4212=(($4206+($4211<<2))|0);
      var $4213=$4212;
      var $4214=$4213;
      HEAPF32[(($4214)>>2)]=$4203;
      __label__ = 245; break;
    case 156: 
      var $4216=$2;
      var $4217=$2;
      var $4218=(($4217+64)|0);
      var $4219=HEAP32[(($4218)>>2)];
      var $4220=$st;
      var $4221=(($4220+2)|0);
      var $4222=$4221;
      var $4223=HEAP16[(($4222)>>1)];
      var $4224=(($4223)&65535);
      var $4225=(($4219+($4224<<2))|0);
      var $4226=$4225;
      var $4227=$4226;
      var $4228=HEAP32[(($4227)>>2)];
      var $4229=_prog_getstring($4216, $4228);
      var $4230=$2;
      var $4231=$2;
      var $4232=(($4231+64)|0);
      var $4233=HEAP32[(($4232)>>2)];
      var $4234=$st;
      var $4235=(($4234+4)|0);
      var $4236=$4235;
      var $4237=HEAP16[(($4236)>>1)];
      var $4238=(($4237)&65535);
      var $4239=(($4233+($4238<<2))|0);
      var $4240=$4239;
      var $4241=$4240;
      var $4242=HEAP32[(($4241)>>2)];
      var $4243=_prog_getstring($4230, $4242);
      var $4244=_strcmp($4229, $4243);
      var $4245=(($4244)|0)!=0;
      var $4246=$4245 ^ 1;
      var $4247=$4246 ^ 1;
      var $4248=(($4247)&1);
      var $4249=(($4248)|0);
      var $4250=$2;
      var $4251=(($4250+64)|0);
      var $4252=HEAP32[(($4251)>>2)];
      var $4253=$st;
      var $4254=(($4253+6)|0);
      var $4255=$4254;
      var $4256=HEAP16[(($4255)>>1)];
      var $4257=(($4256)&65535);
      var $4258=(($4252+($4257<<2))|0);
      var $4259=$4258;
      var $4260=$4259;
      HEAPF32[(($4260)>>2)]=$4249;
      __label__ = 245; break;
    case 157: 
      var $4262=$2;
      var $4263=(($4262+64)|0);
      var $4264=HEAP32[(($4263)>>2)];
      var $4265=$st;
      var $4266=(($4265+2)|0);
      var $4267=$4266;
      var $4268=HEAP16[(($4267)>>1)];
      var $4269=(($4268)&65535);
      var $4270=(($4264+($4269<<2))|0);
      var $4271=$4270;
      var $4272=$4271;
      var $4273=HEAP32[(($4272)>>2)];
      var $4274=$2;
      var $4275=(($4274+64)|0);
      var $4276=HEAP32[(($4275)>>2)];
      var $4277=$st;
      var $4278=(($4277+4)|0);
      var $4279=$4278;
      var $4280=HEAP16[(($4279)>>1)];
      var $4281=(($4280)&65535);
      var $4282=(($4276+($4281<<2))|0);
      var $4283=$4282;
      var $4284=$4283;
      var $4285=HEAP32[(($4284)>>2)];
      var $4286=(($4273)|0)!=(($4285)|0);
      var $4287=(($4286)&1);
      var $4288=(($4287)|0);
      var $4289=$2;
      var $4290=(($4289+64)|0);
      var $4291=HEAP32[(($4290)>>2)];
      var $4292=$st;
      var $4293=(($4292+6)|0);
      var $4294=$4293;
      var $4295=HEAP16[(($4294)>>1)];
      var $4296=(($4295)&65535);
      var $4297=(($4291+($4296<<2))|0);
      var $4298=$4297;
      var $4299=$4298;
      HEAPF32[(($4299)>>2)]=$4288;
      __label__ = 245; break;
    case 158: 
      var $4301=$2;
      var $4302=(($4301+64)|0);
      var $4303=HEAP32[(($4302)>>2)];
      var $4304=$st;
      var $4305=(($4304+2)|0);
      var $4306=$4305;
      var $4307=HEAP16[(($4306)>>1)];
      var $4308=(($4307)&65535);
      var $4309=(($4303+($4308<<2))|0);
      var $4310=$4309;
      var $4311=$4310;
      var $4312=HEAP32[(($4311)>>2)];
      var $4313=$2;
      var $4314=(($4313+64)|0);
      var $4315=HEAP32[(($4314)>>2)];
      var $4316=$st;
      var $4317=(($4316+4)|0);
      var $4318=$4317;
      var $4319=HEAP16[(($4318)>>1)];
      var $4320=(($4319)&65535);
      var $4321=(($4315+($4320<<2))|0);
      var $4322=$4321;
      var $4323=$4322;
      var $4324=HEAP32[(($4323)>>2)];
      var $4325=(($4312)|0)!=(($4324)|0);
      var $4326=(($4325)&1);
      var $4327=(($4326)|0);
      var $4328=$2;
      var $4329=(($4328+64)|0);
      var $4330=HEAP32[(($4329)>>2)];
      var $4331=$st;
      var $4332=(($4331+6)|0);
      var $4333=$4332;
      var $4334=HEAP16[(($4333)>>1)];
      var $4335=(($4334)&65535);
      var $4336=(($4330+($4335<<2))|0);
      var $4337=$4336;
      var $4338=$4337;
      HEAPF32[(($4338)>>2)]=$4327;
      __label__ = 245; break;
    case 159: 
      var $4340=$2;
      var $4341=(($4340+64)|0);
      var $4342=HEAP32[(($4341)>>2)];
      var $4343=$st;
      var $4344=(($4343+2)|0);
      var $4345=$4344;
      var $4346=HEAP16[(($4345)>>1)];
      var $4347=(($4346)&65535);
      var $4348=(($4342+($4347<<2))|0);
      var $4349=$4348;
      var $4350=$4349;
      var $4351=HEAPF32[(($4350)>>2)];
      var $4352=$2;
      var $4353=(($4352+64)|0);
      var $4354=HEAP32[(($4353)>>2)];
      var $4355=$st;
      var $4356=(($4355+4)|0);
      var $4357=$4356;
      var $4358=HEAP16[(($4357)>>1)];
      var $4359=(($4358)&65535);
      var $4360=(($4354+($4359<<2))|0);
      var $4361=$4360;
      var $4362=$4361;
      var $4363=HEAPF32[(($4362)>>2)];
      var $4364=$4351 <= $4363;
      var $4365=(($4364)&1);
      var $4366=(($4365)|0);
      var $4367=$2;
      var $4368=(($4367+64)|0);
      var $4369=HEAP32[(($4368)>>2)];
      var $4370=$st;
      var $4371=(($4370+6)|0);
      var $4372=$4371;
      var $4373=HEAP16[(($4372)>>1)];
      var $4374=(($4373)&65535);
      var $4375=(($4369+($4374<<2))|0);
      var $4376=$4375;
      var $4377=$4376;
      HEAPF32[(($4377)>>2)]=$4366;
      __label__ = 245; break;
    case 160: 
      var $4379=$2;
      var $4380=(($4379+64)|0);
      var $4381=HEAP32[(($4380)>>2)];
      var $4382=$st;
      var $4383=(($4382+2)|0);
      var $4384=$4383;
      var $4385=HEAP16[(($4384)>>1)];
      var $4386=(($4385)&65535);
      var $4387=(($4381+($4386<<2))|0);
      var $4388=$4387;
      var $4389=$4388;
      var $4390=HEAPF32[(($4389)>>2)];
      var $4391=$2;
      var $4392=(($4391+64)|0);
      var $4393=HEAP32[(($4392)>>2)];
      var $4394=$st;
      var $4395=(($4394+4)|0);
      var $4396=$4395;
      var $4397=HEAP16[(($4396)>>1)];
      var $4398=(($4397)&65535);
      var $4399=(($4393+($4398<<2))|0);
      var $4400=$4399;
      var $4401=$4400;
      var $4402=HEAPF32[(($4401)>>2)];
      var $4403=$4390 >= $4402;
      var $4404=(($4403)&1);
      var $4405=(($4404)|0);
      var $4406=$2;
      var $4407=(($4406+64)|0);
      var $4408=HEAP32[(($4407)>>2)];
      var $4409=$st;
      var $4410=(($4409+6)|0);
      var $4411=$4410;
      var $4412=HEAP16[(($4411)>>1)];
      var $4413=(($4412)&65535);
      var $4414=(($4408+($4413<<2))|0);
      var $4415=$4414;
      var $4416=$4415;
      HEAPF32[(($4416)>>2)]=$4405;
      __label__ = 245; break;
    case 161: 
      var $4418=$2;
      var $4419=(($4418+64)|0);
      var $4420=HEAP32[(($4419)>>2)];
      var $4421=$st;
      var $4422=(($4421+2)|0);
      var $4423=$4422;
      var $4424=HEAP16[(($4423)>>1)];
      var $4425=(($4424)&65535);
      var $4426=(($4420+($4425<<2))|0);
      var $4427=$4426;
      var $4428=$4427;
      var $4429=HEAPF32[(($4428)>>2)];
      var $4430=$2;
      var $4431=(($4430+64)|0);
      var $4432=HEAP32[(($4431)>>2)];
      var $4433=$st;
      var $4434=(($4433+4)|0);
      var $4435=$4434;
      var $4436=HEAP16[(($4435)>>1)];
      var $4437=(($4436)&65535);
      var $4438=(($4432+($4437<<2))|0);
      var $4439=$4438;
      var $4440=$4439;
      var $4441=HEAPF32[(($4440)>>2)];
      var $4442=$4429 < $4441;
      var $4443=(($4442)&1);
      var $4444=(($4443)|0);
      var $4445=$2;
      var $4446=(($4445+64)|0);
      var $4447=HEAP32[(($4446)>>2)];
      var $4448=$st;
      var $4449=(($4448+6)|0);
      var $4450=$4449;
      var $4451=HEAP16[(($4450)>>1)];
      var $4452=(($4451)&65535);
      var $4453=(($4447+($4452<<2))|0);
      var $4454=$4453;
      var $4455=$4454;
      HEAPF32[(($4455)>>2)]=$4444;
      __label__ = 245; break;
    case 162: 
      var $4457=$2;
      var $4458=(($4457+64)|0);
      var $4459=HEAP32[(($4458)>>2)];
      var $4460=$st;
      var $4461=(($4460+2)|0);
      var $4462=$4461;
      var $4463=HEAP16[(($4462)>>1)];
      var $4464=(($4463)&65535);
      var $4465=(($4459+($4464<<2))|0);
      var $4466=$4465;
      var $4467=$4466;
      var $4468=HEAPF32[(($4467)>>2)];
      var $4469=$2;
      var $4470=(($4469+64)|0);
      var $4471=HEAP32[(($4470)>>2)];
      var $4472=$st;
      var $4473=(($4472+4)|0);
      var $4474=$4473;
      var $4475=HEAP16[(($4474)>>1)];
      var $4476=(($4475)&65535);
      var $4477=(($4471+($4476<<2))|0);
      var $4478=$4477;
      var $4479=$4478;
      var $4480=HEAPF32[(($4479)>>2)];
      var $4481=$4468 > $4480;
      var $4482=(($4481)&1);
      var $4483=(($4482)|0);
      var $4484=$2;
      var $4485=(($4484+64)|0);
      var $4486=HEAP32[(($4485)>>2)];
      var $4487=$st;
      var $4488=(($4487+6)|0);
      var $4489=$4488;
      var $4490=HEAP16[(($4489)>>1)];
      var $4491=(($4490)&65535);
      var $4492=(($4486+($4491<<2))|0);
      var $4493=$4492;
      var $4494=$4493;
      HEAPF32[(($4494)>>2)]=$4483;
      __label__ = 245; break;
    case 163: 
      var $4496=$2;
      var $4497=(($4496+64)|0);
      var $4498=HEAP32[(($4497)>>2)];
      var $4499=$st;
      var $4500=(($4499+2)|0);
      var $4501=$4500;
      var $4502=HEAP16[(($4501)>>1)];
      var $4503=(($4502)&65535);
      var $4504=(($4498+($4503<<2))|0);
      var $4505=$4504;
      var $4506=$4505;
      var $4507=HEAP32[(($4506)>>2)];
      var $4508=(($4507)|0) < 0;
      if ($4508) { __label__ = 165; break; } else { __label__ = 164; break; }
    case 164: 
      var $4510=$2;
      var $4511=(($4510+64)|0);
      var $4512=HEAP32[(($4511)>>2)];
      var $4513=$st;
      var $4514=(($4513+2)|0);
      var $4515=$4514;
      var $4516=HEAP16[(($4515)>>1)];
      var $4517=(($4516)&65535);
      var $4518=(($4512+($4517<<2))|0);
      var $4519=$4518;
      var $4520=$4519;
      var $4521=HEAP32[(($4520)>>2)];
      var $4522=$2;
      var $4523=(($4522+140)|0);
      var $4524=HEAP32[(($4523)>>2)];
      var $4525=(($4521)|0) >= (($4524)|0);
      if ($4525) { __label__ = 165; break; } else { __label__ = 166; break; }
    case 165: 
      var $4527=$2;
      var $4528=$2;
      var $4529=(($4528)|0);
      var $4530=HEAP32[(($4529)>>2)];
      _qcvmerror($4527, ((STRING_TABLE.__str17)|0), (tempInt=STACKTOP,STACKTOP += 4,assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),HEAP32[((tempInt)>>2)]=$4530,tempInt));
      __label__ = 488; break;
    case 166: 
      var $4532=$2;
      var $4533=(($4532+64)|0);
      var $4534=HEAP32[(($4533)>>2)];
      var $4535=$st;
      var $4536=(($4535+4)|0);
      var $4537=$4536;
      var $4538=HEAP16[(($4537)>>1)];
      var $4539=(($4538)&65535);
      var $4540=(($4534+($4539<<2))|0);
      var $4541=$4540;
      var $4542=$4541;
      var $4543=HEAP32[(($4542)>>2)];
      var $4544=$2;
      var $4545=(($4544+144)|0);
      var $4546=HEAP32[(($4545)>>2)];
      var $4547=(($4543)>>>0) >= (($4546)>>>0);
      if ($4547) { __label__ = 167; break; } else { __label__ = 168; break; }
    case 167: 
      var $4549=$2;
      var $4550=$2;
      var $4551=(($4550)|0);
      var $4552=HEAP32[(($4551)>>2)];
      var $4553=$2;
      var $4554=(($4553+64)|0);
      var $4555=HEAP32[(($4554)>>2)];
      var $4556=$st;
      var $4557=(($4556+4)|0);
      var $4558=$4557;
      var $4559=HEAP16[(($4558)>>1)];
      var $4560=(($4559)&65535);
      var $4561=(($4555+($4560<<2))|0);
      var $4562=$4561;
      var $4563=$4562;
      var $4564=HEAP32[(($4563)>>2)];
      _qcvmerror($4549, ((STRING_TABLE.__str18)|0), (tempInt=STACKTOP,STACKTOP += 8,assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),HEAP32[((tempInt)>>2)]=$4552,HEAP32[(((tempInt)+(4))>>2)]=$4564,tempInt));
      __label__ = 488; break;
    case 168: 
      var $4566=$2;
      var $4567=$2;
      var $4568=(($4567+64)|0);
      var $4569=HEAP32[(($4568)>>2)];
      var $4570=$st;
      var $4571=(($4570+2)|0);
      var $4572=$4571;
      var $4573=HEAP16[(($4572)>>1)];
      var $4574=(($4573)&65535);
      var $4575=(($4569+($4574<<2))|0);
      var $4576=$4575;
      var $4577=$4576;
      var $4578=HEAP32[(($4577)>>2)];
      var $4579=_prog_getedict($4566, $4578);
      $ed2=$4579;
      var $4580=$ed2;
      var $4581=$4580;
      var $4582=$2;
      var $4583=(($4582+64)|0);
      var $4584=HEAP32[(($4583)>>2)];
      var $4585=$st;
      var $4586=(($4585+4)|0);
      var $4587=$4586;
      var $4588=HEAP16[(($4587)>>1)];
      var $4589=(($4588)&65535);
      var $4590=(($4584+($4589<<2))|0);
      var $4591=$4590;
      var $4592=$4591;
      var $4593=HEAP32[(($4592)>>2)];
      var $4594=(($4581+($4593<<2))|0);
      var $4595=$4594;
      var $4596=$4595;
      var $4597=HEAP32[(($4596)>>2)];
      var $4598=$2;
      var $4599=(($4598+64)|0);
      var $4600=HEAP32[(($4599)>>2)];
      var $4601=$st;
      var $4602=(($4601+6)|0);
      var $4603=$4602;
      var $4604=HEAP16[(($4603)>>1)];
      var $4605=(($4604)&65535);
      var $4606=(($4600+($4605<<2))|0);
      var $4607=$4606;
      var $4608=$4607;
      HEAP32[(($4608)>>2)]=$4597;
      __label__ = 245; break;
    case 169: 
      var $4610=$2;
      var $4611=(($4610+64)|0);
      var $4612=HEAP32[(($4611)>>2)];
      var $4613=$st;
      var $4614=(($4613+2)|0);
      var $4615=$4614;
      var $4616=HEAP16[(($4615)>>1)];
      var $4617=(($4616)&65535);
      var $4618=(($4612+($4617<<2))|0);
      var $4619=$4618;
      var $4620=$4619;
      var $4621=HEAP32[(($4620)>>2)];
      var $4622=(($4621)|0) < 0;
      if ($4622) { __label__ = 171; break; } else { __label__ = 170; break; }
    case 170: 
      var $4624=$2;
      var $4625=(($4624+64)|0);
      var $4626=HEAP32[(($4625)>>2)];
      var $4627=$st;
      var $4628=(($4627+2)|0);
      var $4629=$4628;
      var $4630=HEAP16[(($4629)>>1)];
      var $4631=(($4630)&65535);
      var $4632=(($4626+($4631<<2))|0);
      var $4633=$4632;
      var $4634=$4633;
      var $4635=HEAP32[(($4634)>>2)];
      var $4636=$2;
      var $4637=(($4636+140)|0);
      var $4638=HEAP32[(($4637)>>2)];
      var $4639=(($4635)|0) >= (($4638)|0);
      if ($4639) { __label__ = 171; break; } else { __label__ = 172; break; }
    case 171: 
      var $4641=$2;
      var $4642=$2;
      var $4643=(($4642)|0);
      var $4644=HEAP32[(($4643)>>2)];
      _qcvmerror($4641, ((STRING_TABLE.__str17)|0), (tempInt=STACKTOP,STACKTOP += 4,assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),HEAP32[((tempInt)>>2)]=$4644,tempInt));
      __label__ = 488; break;
    case 172: 
      var $4646=$2;
      var $4647=(($4646+64)|0);
      var $4648=HEAP32[(($4647)>>2)];
      var $4649=$st;
      var $4650=(($4649+4)|0);
      var $4651=$4650;
      var $4652=HEAP16[(($4651)>>1)];
      var $4653=(($4652)&65535);
      var $4654=(($4648+($4653<<2))|0);
      var $4655=$4654;
      var $4656=$4655;
      var $4657=HEAP32[(($4656)>>2)];
      var $4658=(($4657)|0) < 0;
      if ($4658) { __label__ = 174; break; } else { __label__ = 173; break; }
    case 173: 
      var $4660=$2;
      var $4661=(($4660+64)|0);
      var $4662=HEAP32[(($4661)>>2)];
      var $4663=$st;
      var $4664=(($4663+4)|0);
      var $4665=$4664;
      var $4666=HEAP16[(($4665)>>1)];
      var $4667=(($4666)&65535);
      var $4668=(($4662+($4667<<2))|0);
      var $4669=$4668;
      var $4670=$4669;
      var $4671=HEAP32[(($4670)>>2)];
      var $4672=((($4671)+(3))|0);
      var $4673=$2;
      var $4674=(($4673+144)|0);
      var $4675=HEAP32[(($4674)>>2)];
      var $4676=(($4672)>>>0) > (($4675)>>>0);
      if ($4676) { __label__ = 174; break; } else { __label__ = 175; break; }
    case 174: 
      var $4678=$2;
      var $4679=$2;
      var $4680=(($4679)|0);
      var $4681=HEAP32[(($4680)>>2)];
      var $4682=$2;
      var $4683=(($4682+64)|0);
      var $4684=HEAP32[(($4683)>>2)];
      var $4685=$st;
      var $4686=(($4685+4)|0);
      var $4687=$4686;
      var $4688=HEAP16[(($4687)>>1)];
      var $4689=(($4688)&65535);
      var $4690=(($4684+($4689<<2))|0);
      var $4691=$4690;
      var $4692=$4691;
      var $4693=HEAP32[(($4692)>>2)];
      var $4694=((($4693)+(2))|0);
      _qcvmerror($4678, ((STRING_TABLE.__str18)|0), (tempInt=STACKTOP,STACKTOP += 8,assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),HEAP32[((tempInt)>>2)]=$4681,HEAP32[(((tempInt)+(4))>>2)]=$4694,tempInt));
      __label__ = 488; break;
    case 175: 
      var $4696=$2;
      var $4697=$2;
      var $4698=(($4697+64)|0);
      var $4699=HEAP32[(($4698)>>2)];
      var $4700=$st;
      var $4701=(($4700+2)|0);
      var $4702=$4701;
      var $4703=HEAP16[(($4702)>>1)];
      var $4704=(($4703)&65535);
      var $4705=(($4699+($4704<<2))|0);
      var $4706=$4705;
      var $4707=$4706;
      var $4708=HEAP32[(($4707)>>2)];
      var $4709=_prog_getedict($4696, $4708);
      $ed2=$4709;
      var $4710=$ed2;
      var $4711=$4710;
      var $4712=$2;
      var $4713=(($4712+64)|0);
      var $4714=HEAP32[(($4713)>>2)];
      var $4715=$st;
      var $4716=(($4715+4)|0);
      var $4717=$4716;
      var $4718=HEAP16[(($4717)>>1)];
      var $4719=(($4718)&65535);
      var $4720=(($4714+($4719<<2))|0);
      var $4721=$4720;
      var $4722=$4721;
      var $4723=HEAP32[(($4722)>>2)];
      var $4724=(($4711+($4723<<2))|0);
      var $4725=$4724;
      var $4726=$4725;
      var $4727=(($4726)|0);
      var $4728=HEAP32[(($4727)>>2)];
      var $4729=$2;
      var $4730=(($4729+64)|0);
      var $4731=HEAP32[(($4730)>>2)];
      var $4732=$st;
      var $4733=(($4732+6)|0);
      var $4734=$4733;
      var $4735=HEAP16[(($4734)>>1)];
      var $4736=(($4735)&65535);
      var $4737=(($4731+($4736<<2))|0);
      var $4738=$4737;
      var $4739=$4738;
      var $4740=(($4739)|0);
      HEAP32[(($4740)>>2)]=$4728;
      var $4741=$ed2;
      var $4742=$4741;
      var $4743=$2;
      var $4744=(($4743+64)|0);
      var $4745=HEAP32[(($4744)>>2)];
      var $4746=$st;
      var $4747=(($4746+4)|0);
      var $4748=$4747;
      var $4749=HEAP16[(($4748)>>1)];
      var $4750=(($4749)&65535);
      var $4751=(($4745+($4750<<2))|0);
      var $4752=$4751;
      var $4753=$4752;
      var $4754=HEAP32[(($4753)>>2)];
      var $4755=(($4742+($4754<<2))|0);
      var $4756=$4755;
      var $4757=$4756;
      var $4758=(($4757+4)|0);
      var $4759=HEAP32[(($4758)>>2)];
      var $4760=$2;
      var $4761=(($4760+64)|0);
      var $4762=HEAP32[(($4761)>>2)];
      var $4763=$st;
      var $4764=(($4763+6)|0);
      var $4765=$4764;
      var $4766=HEAP16[(($4765)>>1)];
      var $4767=(($4766)&65535);
      var $4768=(($4762+($4767<<2))|0);
      var $4769=$4768;
      var $4770=$4769;
      var $4771=(($4770+4)|0);
      HEAP32[(($4771)>>2)]=$4759;
      var $4772=$ed2;
      var $4773=$4772;
      var $4774=$2;
      var $4775=(($4774+64)|0);
      var $4776=HEAP32[(($4775)>>2)];
      var $4777=$st;
      var $4778=(($4777+4)|0);
      var $4779=$4778;
      var $4780=HEAP16[(($4779)>>1)];
      var $4781=(($4780)&65535);
      var $4782=(($4776+($4781<<2))|0);
      var $4783=$4782;
      var $4784=$4783;
      var $4785=HEAP32[(($4784)>>2)];
      var $4786=(($4773+($4785<<2))|0);
      var $4787=$4786;
      var $4788=$4787;
      var $4789=(($4788+8)|0);
      var $4790=HEAP32[(($4789)>>2)];
      var $4791=$2;
      var $4792=(($4791+64)|0);
      var $4793=HEAP32[(($4792)>>2)];
      var $4794=$st;
      var $4795=(($4794+6)|0);
      var $4796=$4795;
      var $4797=HEAP16[(($4796)>>1)];
      var $4798=(($4797)&65535);
      var $4799=(($4793+($4798<<2))|0);
      var $4800=$4799;
      var $4801=$4800;
      var $4802=(($4801+8)|0);
      HEAP32[(($4802)>>2)]=$4790;
      __label__ = 245; break;
    case 176: 
      var $4804=$2;
      var $4805=(($4804+64)|0);
      var $4806=HEAP32[(($4805)>>2)];
      var $4807=$st;
      var $4808=(($4807+2)|0);
      var $4809=$4808;
      var $4810=HEAP16[(($4809)>>1)];
      var $4811=(($4810)&65535);
      var $4812=(($4806+($4811<<2))|0);
      var $4813=$4812;
      var $4814=$4813;
      var $4815=HEAP32[(($4814)>>2)];
      var $4816=(($4815)|0) < 0;
      if ($4816) { __label__ = 178; break; } else { __label__ = 177; break; }
    case 177: 
      var $4818=$2;
      var $4819=(($4818+64)|0);
      var $4820=HEAP32[(($4819)>>2)];
      var $4821=$st;
      var $4822=(($4821+2)|0);
      var $4823=$4822;
      var $4824=HEAP16[(($4823)>>1)];
      var $4825=(($4824)&65535);
      var $4826=(($4820+($4825<<2))|0);
      var $4827=$4826;
      var $4828=$4827;
      var $4829=HEAP32[(($4828)>>2)];
      var $4830=$2;
      var $4831=(($4830+140)|0);
      var $4832=HEAP32[(($4831)>>2)];
      var $4833=(($4829)|0) >= (($4832)|0);
      if ($4833) { __label__ = 178; break; } else { __label__ = 179; break; }
    case 178: 
      var $4835=$2;
      var $4836=$2;
      var $4837=(($4836)|0);
      var $4838=HEAP32[(($4837)>>2)];
      var $4839=$2;
      var $4840=(($4839+64)|0);
      var $4841=HEAP32[(($4840)>>2)];
      var $4842=$st;
      var $4843=(($4842+2)|0);
      var $4844=$4843;
      var $4845=HEAP16[(($4844)>>1)];
      var $4846=(($4845)&65535);
      var $4847=(($4841+($4846<<2))|0);
      var $4848=$4847;
      var $4849=$4848;
      var $4850=HEAP32[(($4849)>>2)];
      _qcvmerror($4835, ((STRING_TABLE.__str19)|0), (tempInt=STACKTOP,STACKTOP += 8,assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),HEAP32[((tempInt)>>2)]=$4838,HEAP32[(((tempInt)+(4))>>2)]=$4850,tempInt));
      __label__ = 488; break;
    case 179: 
      var $4852=$2;
      var $4853=(($4852+64)|0);
      var $4854=HEAP32[(($4853)>>2)];
      var $4855=$st;
      var $4856=(($4855+4)|0);
      var $4857=$4856;
      var $4858=HEAP16[(($4857)>>1)];
      var $4859=(($4858)&65535);
      var $4860=(($4854+($4859<<2))|0);
      var $4861=$4860;
      var $4862=$4861;
      var $4863=HEAP32[(($4862)>>2)];
      var $4864=$2;
      var $4865=(($4864+144)|0);
      var $4866=HEAP32[(($4865)>>2)];
      var $4867=(($4863)>>>0) >= (($4866)>>>0);
      if ($4867) { __label__ = 180; break; } else { __label__ = 181; break; }
    case 180: 
      var $4869=$2;
      var $4870=$2;
      var $4871=(($4870)|0);
      var $4872=HEAP32[(($4871)>>2)];
      var $4873=$2;
      var $4874=(($4873+64)|0);
      var $4875=HEAP32[(($4874)>>2)];
      var $4876=$st;
      var $4877=(($4876+4)|0);
      var $4878=$4877;
      var $4879=HEAP16[(($4878)>>1)];
      var $4880=(($4879)&65535);
      var $4881=(($4875+($4880<<2))|0);
      var $4882=$4881;
      var $4883=$4882;
      var $4884=HEAP32[(($4883)>>2)];
      _qcvmerror($4869, ((STRING_TABLE.__str18)|0), (tempInt=STACKTOP,STACKTOP += 8,assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),HEAP32[((tempInt)>>2)]=$4872,HEAP32[(((tempInt)+(4))>>2)]=$4884,tempInt));
      __label__ = 488; break;
    case 181: 
      var $4886=$2;
      var $4887=$2;
      var $4888=(($4887+64)|0);
      var $4889=HEAP32[(($4888)>>2)];
      var $4890=$st;
      var $4891=(($4890+2)|0);
      var $4892=$4891;
      var $4893=HEAP16[(($4892)>>1)];
      var $4894=(($4893)&65535);
      var $4895=(($4889+($4894<<2))|0);
      var $4896=$4895;
      var $4897=$4896;
      var $4898=HEAP32[(($4897)>>2)];
      var $4899=_prog_getedict($4886, $4898);
      $ed2=$4899;
      var $4900=$ed2;
      var $4901=$4900;
      var $4902=$2;
      var $4903=(($4902+76)|0);
      var $4904=HEAP32[(($4903)>>2)];
      var $4905=$4901;
      var $4906=$4904;
      var $4907=((($4905)-($4906))|0);
      var $4908=((((($4907)|0))/(4))&-1);
      var $4909=$2;
      var $4910=(($4909+64)|0);
      var $4911=HEAP32[(($4910)>>2)];
      var $4912=$st;
      var $4913=(($4912+6)|0);
      var $4914=$4913;
      var $4915=HEAP16[(($4914)>>1)];
      var $4916=(($4915)&65535);
      var $4917=(($4911+($4916<<2))|0);
      var $4918=$4917;
      var $4919=$4918;
      HEAP32[(($4919)>>2)]=$4908;
      var $4920=$2;
      var $4921=(($4920+64)|0);
      var $4922=HEAP32[(($4921)>>2)];
      var $4923=$st;
      var $4924=(($4923+4)|0);
      var $4925=$4924;
      var $4926=HEAP16[(($4925)>>1)];
      var $4927=(($4926)&65535);
      var $4928=(($4922+($4927<<2))|0);
      var $4929=$4928;
      var $4930=$4929;
      var $4931=HEAP32[(($4930)>>2)];
      var $4932=$2;
      var $4933=(($4932+64)|0);
      var $4934=HEAP32[(($4933)>>2)];
      var $4935=$st;
      var $4936=(($4935+6)|0);
      var $4937=$4936;
      var $4938=HEAP16[(($4937)>>1)];
      var $4939=(($4938)&65535);
      var $4940=(($4934+($4939<<2))|0);
      var $4941=$4940;
      var $4942=$4941;
      var $4943=HEAP32[(($4942)>>2)];
      var $4944=((($4943)+($4931))|0);
      HEAP32[(($4942)>>2)]=$4944;
      __label__ = 245; break;
    case 182: 
      var $4946=$2;
      var $4947=(($4946+64)|0);
      var $4948=HEAP32[(($4947)>>2)];
      var $4949=$st;
      var $4950=(($4949+2)|0);
      var $4951=$4950;
      var $4952=HEAP16[(($4951)>>1)];
      var $4953=(($4952)&65535);
      var $4954=(($4948+($4953<<2))|0);
      var $4955=$4954;
      var $4956=$4955;
      var $4957=HEAP32[(($4956)>>2)];
      var $4958=$2;
      var $4959=(($4958+64)|0);
      var $4960=HEAP32[(($4959)>>2)];
      var $4961=$st;
      var $4962=(($4961+4)|0);
      var $4963=$4962;
      var $4964=HEAP16[(($4963)>>1)];
      var $4965=(($4964)&65535);
      var $4966=(($4960+($4965<<2))|0);
      var $4967=$4966;
      var $4968=$4967;
      HEAP32[(($4968)>>2)]=$4957;
      __label__ = 245; break;
    case 183: 
      var $4970=$2;
      var $4971=(($4970+64)|0);
      var $4972=HEAP32[(($4971)>>2)];
      var $4973=$st;
      var $4974=(($4973+2)|0);
      var $4975=$4974;
      var $4976=HEAP16[(($4975)>>1)];
      var $4977=(($4976)&65535);
      var $4978=(($4972+($4977<<2))|0);
      var $4979=$4978;
      var $4980=$4979;
      var $4981=(($4980)|0);
      var $4982=HEAP32[(($4981)>>2)];
      var $4983=$2;
      var $4984=(($4983+64)|0);
      var $4985=HEAP32[(($4984)>>2)];
      var $4986=$st;
      var $4987=(($4986+4)|0);
      var $4988=$4987;
      var $4989=HEAP16[(($4988)>>1)];
      var $4990=(($4989)&65535);
      var $4991=(($4985+($4990<<2))|0);
      var $4992=$4991;
      var $4993=$4992;
      var $4994=(($4993)|0);
      HEAP32[(($4994)>>2)]=$4982;
      var $4995=$2;
      var $4996=(($4995+64)|0);
      var $4997=HEAP32[(($4996)>>2)];
      var $4998=$st;
      var $4999=(($4998+2)|0);
      var $5000=$4999;
      var $5001=HEAP16[(($5000)>>1)];
      var $5002=(($5001)&65535);
      var $5003=(($4997+($5002<<2))|0);
      var $5004=$5003;
      var $5005=$5004;
      var $5006=(($5005+4)|0);
      var $5007=HEAP32[(($5006)>>2)];
      var $5008=$2;
      var $5009=(($5008+64)|0);
      var $5010=HEAP32[(($5009)>>2)];
      var $5011=$st;
      var $5012=(($5011+4)|0);
      var $5013=$5012;
      var $5014=HEAP16[(($5013)>>1)];
      var $5015=(($5014)&65535);
      var $5016=(($5010+($5015<<2))|0);
      var $5017=$5016;
      var $5018=$5017;
      var $5019=(($5018+4)|0);
      HEAP32[(($5019)>>2)]=$5007;
      var $5020=$2;
      var $5021=(($5020+64)|0);
      var $5022=HEAP32[(($5021)>>2)];
      var $5023=$st;
      var $5024=(($5023+2)|0);
      var $5025=$5024;
      var $5026=HEAP16[(($5025)>>1)];
      var $5027=(($5026)&65535);
      var $5028=(($5022+($5027<<2))|0);
      var $5029=$5028;
      var $5030=$5029;
      var $5031=(($5030+8)|0);
      var $5032=HEAP32[(($5031)>>2)];
      var $5033=$2;
      var $5034=(($5033+64)|0);
      var $5035=HEAP32[(($5034)>>2)];
      var $5036=$st;
      var $5037=(($5036+4)|0);
      var $5038=$5037;
      var $5039=HEAP16[(($5038)>>1)];
      var $5040=(($5039)&65535);
      var $5041=(($5035+($5040<<2))|0);
      var $5042=$5041;
      var $5043=$5042;
      var $5044=(($5043+8)|0);
      HEAP32[(($5044)>>2)]=$5032;
      __label__ = 245; break;
    case 184: 
      var $5046=$2;
      var $5047=(($5046+64)|0);
      var $5048=HEAP32[(($5047)>>2)];
      var $5049=$st;
      var $5050=(($5049+4)|0);
      var $5051=$5050;
      var $5052=HEAP16[(($5051)>>1)];
      var $5053=(($5052)&65535);
      var $5054=(($5048+($5053<<2))|0);
      var $5055=$5054;
      var $5056=$5055;
      var $5057=HEAP32[(($5056)>>2)];
      var $5058=(($5057)|0) < 0;
      if ($5058) { __label__ = 186; break; } else { __label__ = 185; break; }
    case 185: 
      var $5060=$2;
      var $5061=(($5060+64)|0);
      var $5062=HEAP32[(($5061)>>2)];
      var $5063=$st;
      var $5064=(($5063+4)|0);
      var $5065=$5064;
      var $5066=HEAP16[(($5065)>>1)];
      var $5067=(($5066)&65535);
      var $5068=(($5062+($5067<<2))|0);
      var $5069=$5068;
      var $5070=$5069;
      var $5071=HEAP32[(($5070)>>2)];
      var $5072=$2;
      var $5073=(($5072+80)|0);
      var $5074=HEAP32[(($5073)>>2)];
      var $5075=(($5071)>>>0) >= (($5074)>>>0);
      if ($5075) { __label__ = 186; break; } else { __label__ = 187; break; }
    case 186: 
      var $5077=$2;
      var $5078=$2;
      var $5079=(($5078)|0);
      var $5080=HEAP32[(($5079)>>2)];
      var $5081=$2;
      var $5082=(($5081+64)|0);
      var $5083=HEAP32[(($5082)>>2)];
      var $5084=$st;
      var $5085=(($5084+4)|0);
      var $5086=$5085;
      var $5087=HEAP16[(($5086)>>1)];
      var $5088=(($5087)&65535);
      var $5089=(($5083+($5088<<2))|0);
      var $5090=$5089;
      var $5091=$5090;
      var $5092=HEAP32[(($5091)>>2)];
      _qcvmerror($5077, ((STRING_TABLE.__str20)|0), (tempInt=STACKTOP,STACKTOP += 8,assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),HEAP32[((tempInt)>>2)]=$5080,HEAP32[(((tempInt)+(4))>>2)]=$5092,tempInt));
      __label__ = 488; break;
    case 187: 
      var $5094=$2;
      var $5095=(($5094+64)|0);
      var $5096=HEAP32[(($5095)>>2)];
      var $5097=$st;
      var $5098=(($5097+4)|0);
      var $5099=$5098;
      var $5100=HEAP16[(($5099)>>1)];
      var $5101=(($5100)&65535);
      var $5102=(($5096+($5101<<2))|0);
      var $5103=$5102;
      var $5104=$5103;
      var $5105=HEAP32[(($5104)>>2)];
      var $5106=$2;
      var $5107=(($5106+144)|0);
      var $5108=HEAP32[(($5107)>>2)];
      var $5109=(($5105)>>>0) < (($5108)>>>0);
      if ($5109) { __label__ = 188; break; } else { __label__ = 190; break; }
    case 188: 
      var $5111=$2;
      var $5112=(($5111+148)|0);
      var $5113=HEAP8[($5112)];
      var $5114=(($5113) & 1);
      if ($5114) { __label__ = 190; break; } else { __label__ = 189; break; }
    case 189: 
      var $5116=$2;
      var $5117=$2;
      var $5118=(($5117)|0);
      var $5119=HEAP32[(($5118)>>2)];
      var $5120=$2;
      var $5121=$2;
      var $5122=$2;
      var $5123=(($5122+64)|0);
      var $5124=HEAP32[(($5123)>>2)];
      var $5125=$st;
      var $5126=(($5125+4)|0);
      var $5127=$5126;
      var $5128=HEAP16[(($5127)>>1)];
      var $5129=(($5128)&65535);
      var $5130=(($5124+($5129<<2))|0);
      var $5131=$5130;
      var $5132=$5131;
      var $5133=HEAP32[(($5132)>>2)];
      var $5134=_prog_entfield($5121, $5133);
      var $5135=(($5134+4)|0);
      var $5136=HEAP32[(($5135)>>2)];
      var $5137=_prog_getstring($5120, $5136);
      var $5138=$2;
      var $5139=(($5138+64)|0);
      var $5140=HEAP32[(($5139)>>2)];
      var $5141=$st;
      var $5142=(($5141+4)|0);
      var $5143=$5142;
      var $5144=HEAP16[(($5143)>>1)];
      var $5145=(($5144)&65535);
      var $5146=(($5140+($5145<<2))|0);
      var $5147=$5146;
      var $5148=$5147;
      var $5149=HEAP32[(($5148)>>2)];
      _qcvmerror($5116, ((STRING_TABLE.__str21)|0), (tempInt=STACKTOP,STACKTOP += 12,assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),HEAP32[((tempInt)>>2)]=$5119,HEAP32[(((tempInt)+(4))>>2)]=$5137,HEAP32[(((tempInt)+(8))>>2)]=$5149,tempInt));
      __label__ = 190; break;
    case 190: 
      var $5151=$2;
      var $5152=(($5151+76)|0);
      var $5153=HEAP32[(($5152)>>2)];
      var $5154=$2;
      var $5155=(($5154+64)|0);
      var $5156=HEAP32[(($5155)>>2)];
      var $5157=$st;
      var $5158=(($5157+4)|0);
      var $5159=$5158;
      var $5160=HEAP16[(($5159)>>1)];
      var $5161=(($5160)&65535);
      var $5162=(($5156+($5161<<2))|0);
      var $5163=$5162;
      var $5164=$5163;
      var $5165=HEAP32[(($5164)>>2)];
      var $5166=(($5153+($5165<<2))|0);
      var $5167=$5166;
      $ptr3=$5167;
      var $5168=$2;
      var $5169=(($5168+64)|0);
      var $5170=HEAP32[(($5169)>>2)];
      var $5171=$st;
      var $5172=(($5171+2)|0);
      var $5173=$5172;
      var $5174=HEAP16[(($5173)>>1)];
      var $5175=(($5174)&65535);
      var $5176=(($5170+($5175<<2))|0);
      var $5177=$5176;
      var $5178=$5177;
      var $5179=HEAP32[(($5178)>>2)];
      var $5180=$ptr3;
      var $5181=$5180;
      HEAP32[(($5181)>>2)]=$5179;
      __label__ = 245; break;
    case 191: 
      var $5183=$2;
      var $5184=(($5183+64)|0);
      var $5185=HEAP32[(($5184)>>2)];
      var $5186=$st;
      var $5187=(($5186+4)|0);
      var $5188=$5187;
      var $5189=HEAP16[(($5188)>>1)];
      var $5190=(($5189)&65535);
      var $5191=(($5185+($5190<<2))|0);
      var $5192=$5191;
      var $5193=$5192;
      var $5194=HEAP32[(($5193)>>2)];
      var $5195=(($5194)|0) < 0;
      if ($5195) { __label__ = 193; break; } else { __label__ = 192; break; }
    case 192: 
      var $5197=$2;
      var $5198=(($5197+64)|0);
      var $5199=HEAP32[(($5198)>>2)];
      var $5200=$st;
      var $5201=(($5200+4)|0);
      var $5202=$5201;
      var $5203=HEAP16[(($5202)>>1)];
      var $5204=(($5203)&65535);
      var $5205=(($5199+($5204<<2))|0);
      var $5206=$5205;
      var $5207=$5206;
      var $5208=HEAP32[(($5207)>>2)];
      var $5209=((($5208)+(2))|0);
      var $5210=$2;
      var $5211=(($5210+80)|0);
      var $5212=HEAP32[(($5211)>>2)];
      var $5213=(($5209)>>>0) >= (($5212)>>>0);
      if ($5213) { __label__ = 193; break; } else { __label__ = 194; break; }
    case 193: 
      var $5215=$2;
      var $5216=$2;
      var $5217=(($5216)|0);
      var $5218=HEAP32[(($5217)>>2)];
      var $5219=$2;
      var $5220=(($5219+64)|0);
      var $5221=HEAP32[(($5220)>>2)];
      var $5222=$st;
      var $5223=(($5222+4)|0);
      var $5224=$5223;
      var $5225=HEAP16[(($5224)>>1)];
      var $5226=(($5225)&65535);
      var $5227=(($5221+($5226<<2))|0);
      var $5228=$5227;
      var $5229=$5228;
      var $5230=HEAP32[(($5229)>>2)];
      _qcvmerror($5215, ((STRING_TABLE.__str20)|0), (tempInt=STACKTOP,STACKTOP += 8,assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),HEAP32[((tempInt)>>2)]=$5218,HEAP32[(((tempInt)+(4))>>2)]=$5230,tempInt));
      __label__ = 488; break;
    case 194: 
      var $5232=$2;
      var $5233=(($5232+64)|0);
      var $5234=HEAP32[(($5233)>>2)];
      var $5235=$st;
      var $5236=(($5235+4)|0);
      var $5237=$5236;
      var $5238=HEAP16[(($5237)>>1)];
      var $5239=(($5238)&65535);
      var $5240=(($5234+($5239<<2))|0);
      var $5241=$5240;
      var $5242=$5241;
      var $5243=HEAP32[(($5242)>>2)];
      var $5244=$2;
      var $5245=(($5244+144)|0);
      var $5246=HEAP32[(($5245)>>2)];
      var $5247=(($5243)>>>0) < (($5246)>>>0);
      if ($5247) { __label__ = 195; break; } else { __label__ = 197; break; }
    case 195: 
      var $5249=$2;
      var $5250=(($5249+148)|0);
      var $5251=HEAP8[($5250)];
      var $5252=(($5251) & 1);
      if ($5252) { __label__ = 197; break; } else { __label__ = 196; break; }
    case 196: 
      var $5254=$2;
      var $5255=$2;
      var $5256=(($5255)|0);
      var $5257=HEAP32[(($5256)>>2)];
      var $5258=$2;
      var $5259=$2;
      var $5260=$2;
      var $5261=(($5260+64)|0);
      var $5262=HEAP32[(($5261)>>2)];
      var $5263=$st;
      var $5264=(($5263+4)|0);
      var $5265=$5264;
      var $5266=HEAP16[(($5265)>>1)];
      var $5267=(($5266)&65535);
      var $5268=(($5262+($5267<<2))|0);
      var $5269=$5268;
      var $5270=$5269;
      var $5271=HEAP32[(($5270)>>2)];
      var $5272=_prog_entfield($5259, $5271);
      var $5273=(($5272+4)|0);
      var $5274=HEAP32[(($5273)>>2)];
      var $5275=_prog_getstring($5258, $5274);
      var $5276=$2;
      var $5277=(($5276+64)|0);
      var $5278=HEAP32[(($5277)>>2)];
      var $5279=$st;
      var $5280=(($5279+4)|0);
      var $5281=$5280;
      var $5282=HEAP16[(($5281)>>1)];
      var $5283=(($5282)&65535);
      var $5284=(($5278+($5283<<2))|0);
      var $5285=$5284;
      var $5286=$5285;
      var $5287=HEAP32[(($5286)>>2)];
      _qcvmerror($5254, ((STRING_TABLE.__str21)|0), (tempInt=STACKTOP,STACKTOP += 12,assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),HEAP32[((tempInt)>>2)]=$5257,HEAP32[(((tempInt)+(4))>>2)]=$5275,HEAP32[(((tempInt)+(8))>>2)]=$5287,tempInt));
      __label__ = 197; break;
    case 197: 
      var $5289=$2;
      var $5290=(($5289+76)|0);
      var $5291=HEAP32[(($5290)>>2)];
      var $5292=$2;
      var $5293=(($5292+64)|0);
      var $5294=HEAP32[(($5293)>>2)];
      var $5295=$st;
      var $5296=(($5295+4)|0);
      var $5297=$5296;
      var $5298=HEAP16[(($5297)>>1)];
      var $5299=(($5298)&65535);
      var $5300=(($5294+($5299<<2))|0);
      var $5301=$5300;
      var $5302=$5301;
      var $5303=HEAP32[(($5302)>>2)];
      var $5304=(($5291+($5303<<2))|0);
      var $5305=$5304;
      $ptr3=$5305;
      var $5306=$2;
      var $5307=(($5306+64)|0);
      var $5308=HEAP32[(($5307)>>2)];
      var $5309=$st;
      var $5310=(($5309+2)|0);
      var $5311=$5310;
      var $5312=HEAP16[(($5311)>>1)];
      var $5313=(($5312)&65535);
      var $5314=(($5308+($5313<<2))|0);
      var $5315=$5314;
      var $5316=$5315;
      var $5317=(($5316)|0);
      var $5318=HEAP32[(($5317)>>2)];
      var $5319=$ptr3;
      var $5320=$5319;
      var $5321=(($5320)|0);
      HEAP32[(($5321)>>2)]=$5318;
      var $5322=$2;
      var $5323=(($5322+64)|0);
      var $5324=HEAP32[(($5323)>>2)];
      var $5325=$st;
      var $5326=(($5325+2)|0);
      var $5327=$5326;
      var $5328=HEAP16[(($5327)>>1)];
      var $5329=(($5328)&65535);
      var $5330=(($5324+($5329<<2))|0);
      var $5331=$5330;
      var $5332=$5331;
      var $5333=(($5332+4)|0);
      var $5334=HEAP32[(($5333)>>2)];
      var $5335=$ptr3;
      var $5336=$5335;
      var $5337=(($5336+4)|0);
      HEAP32[(($5337)>>2)]=$5334;
      var $5338=$2;
      var $5339=(($5338+64)|0);
      var $5340=HEAP32[(($5339)>>2)];
      var $5341=$st;
      var $5342=(($5341+2)|0);
      var $5343=$5342;
      var $5344=HEAP16[(($5343)>>1)];
      var $5345=(($5344)&65535);
      var $5346=(($5340+($5345<<2))|0);
      var $5347=$5346;
      var $5348=$5347;
      var $5349=(($5348+8)|0);
      var $5350=HEAP32[(($5349)>>2)];
      var $5351=$ptr3;
      var $5352=$5351;
      var $5353=(($5352+8)|0);
      HEAP32[(($5353)>>2)]=$5350;
      __label__ = 245; break;
    case 198: 
      var $5355=$2;
      var $5356=(($5355+64)|0);
      var $5357=HEAP32[(($5356)>>2)];
      var $5358=$st;
      var $5359=(($5358+2)|0);
      var $5360=$5359;
      var $5361=HEAP16[(($5360)>>1)];
      var $5362=(($5361)&65535);
      var $5363=(($5357+($5362<<2))|0);
      var $5364=$5363;
      var $5365=$5364;
      var $5366=HEAP32[(($5365)>>2)];
      var $5367=$5366 & 2147483647;
      var $5368=(($5367)|0)!=0;
      var $5369=$5368 ^ 1;
      var $5370=(($5369)&1);
      var $5371=(($5370)|0);
      var $5372=$2;
      var $5373=(($5372+64)|0);
      var $5374=HEAP32[(($5373)>>2)];
      var $5375=$st;
      var $5376=(($5375+6)|0);
      var $5377=$5376;
      var $5378=HEAP16[(($5377)>>1)];
      var $5379=(($5378)&65535);
      var $5380=(($5374+($5379<<2))|0);
      var $5381=$5380;
      var $5382=$5381;
      HEAPF32[(($5382)>>2)]=$5371;
      __label__ = 245; break;
    case 199: 
      var $5384=$2;
      var $5385=(($5384+64)|0);
      var $5386=HEAP32[(($5385)>>2)];
      var $5387=$st;
      var $5388=(($5387+2)|0);
      var $5389=$5388;
      var $5390=HEAP16[(($5389)>>1)];
      var $5391=(($5390)&65535);
      var $5392=(($5386+($5391<<2))|0);
      var $5393=$5392;
      var $5394=$5393;
      var $5395=(($5394)|0);
      var $5396=HEAPF32[(($5395)>>2)];
      var $5397=$5396 != 0;
      if ($5397) { var $5430 = 0;__label__ = 202; break; } else { __label__ = 200; break; }
    case 200: 
      var $5399=$2;
      var $5400=(($5399+64)|0);
      var $5401=HEAP32[(($5400)>>2)];
      var $5402=$st;
      var $5403=(($5402+2)|0);
      var $5404=$5403;
      var $5405=HEAP16[(($5404)>>1)];
      var $5406=(($5405)&65535);
      var $5407=(($5401+($5406<<2))|0);
      var $5408=$5407;
      var $5409=$5408;
      var $5410=(($5409+4)|0);
      var $5411=HEAPF32[(($5410)>>2)];
      var $5412=$5411 != 0;
      if ($5412) { var $5430 = 0;__label__ = 202; break; } else { __label__ = 201; break; }
    case 201: 
      var $5414=$2;
      var $5415=(($5414+64)|0);
      var $5416=HEAP32[(($5415)>>2)];
      var $5417=$st;
      var $5418=(($5417+2)|0);
      var $5419=$5418;
      var $5420=HEAP16[(($5419)>>1)];
      var $5421=(($5420)&65535);
      var $5422=(($5416+($5421<<2))|0);
      var $5423=$5422;
      var $5424=$5423;
      var $5425=(($5424+8)|0);
      var $5426=HEAPF32[(($5425)>>2)];
      var $5427=$5426 != 0;
      var $5428=$5427 ^ 1;
      var $5430 = $5428;__label__ = 202; break;
    case 202: 
      var $5430;
      var $5431=(($5430)&1);
      var $5432=(($5431)|0);
      var $5433=$2;
      var $5434=(($5433+64)|0);
      var $5435=HEAP32[(($5434)>>2)];
      var $5436=$st;
      var $5437=(($5436+6)|0);
      var $5438=$5437;
      var $5439=HEAP16[(($5438)>>1)];
      var $5440=(($5439)&65535);
      var $5441=(($5435+($5440<<2))|0);
      var $5442=$5441;
      var $5443=$5442;
      HEAPF32[(($5443)>>2)]=$5432;
      __label__ = 245; break;
    case 203: 
      var $5445=$2;
      var $5446=(($5445+64)|0);
      var $5447=HEAP32[(($5446)>>2)];
      var $5448=$st;
      var $5449=(($5448+2)|0);
      var $5450=$5449;
      var $5451=HEAP16[(($5450)>>1)];
      var $5452=(($5451)&65535);
      var $5453=(($5447+($5452<<2))|0);
      var $5454=$5453;
      var $5455=$5454;
      var $5456=HEAP32[(($5455)>>2)];
      var $5457=(($5456)|0)!=0;
      if ($5457) { __label__ = 204; break; } else { var $5477 = 1;__label__ = 205; break; }
    case 204: 
      var $5459=$2;
      var $5460=$2;
      var $5461=(($5460+64)|0);
      var $5462=HEAP32[(($5461)>>2)];
      var $5463=$st;
      var $5464=(($5463+2)|0);
      var $5465=$5464;
      var $5466=HEAP16[(($5465)>>1)];
      var $5467=(($5466)&65535);
      var $5468=(($5462+($5467<<2))|0);
      var $5469=$5468;
      var $5470=$5469;
      var $5471=HEAP32[(($5470)>>2)];
      var $5472=_prog_getstring($5459, $5471);
      var $5473=HEAP8[($5472)];
      var $5474=(($5473 << 24) >> 24)!=0;
      var $5475=$5474 ^ 1;
      var $5477 = $5475;__label__ = 205; break;
    case 205: 
      var $5477;
      var $5478=(($5477)&1);
      var $5479=(($5478)|0);
      var $5480=$2;
      var $5481=(($5480+64)|0);
      var $5482=HEAP32[(($5481)>>2)];
      var $5483=$st;
      var $5484=(($5483+6)|0);
      var $5485=$5484;
      var $5486=HEAP16[(($5485)>>1)];
      var $5487=(($5486)&65535);
      var $5488=(($5482+($5487<<2))|0);
      var $5489=$5488;
      var $5490=$5489;
      HEAPF32[(($5490)>>2)]=$5479;
      __label__ = 245; break;
    case 206: 
      var $5492=$2;
      var $5493=(($5492+64)|0);
      var $5494=HEAP32[(($5493)>>2)];
      var $5495=$st;
      var $5496=(($5495+2)|0);
      var $5497=$5496;
      var $5498=HEAP16[(($5497)>>1)];
      var $5499=(($5498)&65535);
      var $5500=(($5494+($5499<<2))|0);
      var $5501=$5500;
      var $5502=$5501;
      var $5503=HEAP32[(($5502)>>2)];
      var $5504=(($5503)|0)==0;
      var $5505=(($5504)&1);
      var $5506=(($5505)|0);
      var $5507=$2;
      var $5508=(($5507+64)|0);
      var $5509=HEAP32[(($5508)>>2)];
      var $5510=$st;
      var $5511=(($5510+6)|0);
      var $5512=$5511;
      var $5513=HEAP16[(($5512)>>1)];
      var $5514=(($5513)&65535);
      var $5515=(($5509+($5514<<2))|0);
      var $5516=$5515;
      var $5517=$5516;
      HEAPF32[(($5517)>>2)]=$5506;
      __label__ = 245; break;
    case 207: 
      var $5519=$2;
      var $5520=(($5519+64)|0);
      var $5521=HEAP32[(($5520)>>2)];
      var $5522=$st;
      var $5523=(($5522+2)|0);
      var $5524=$5523;
      var $5525=HEAP16[(($5524)>>1)];
      var $5526=(($5525)&65535);
      var $5527=(($5521+($5526<<2))|0);
      var $5528=$5527;
      var $5529=$5528;
      var $5530=HEAP32[(($5529)>>2)];
      var $5531=(($5530)|0)!=0;
      var $5532=$5531 ^ 1;
      var $5533=(($5532)&1);
      var $5534=(($5533)|0);
      var $5535=$2;
      var $5536=(($5535+64)|0);
      var $5537=HEAP32[(($5536)>>2)];
      var $5538=$st;
      var $5539=(($5538+6)|0);
      var $5540=$5539;
      var $5541=HEAP16[(($5540)>>1)];
      var $5542=(($5541)&65535);
      var $5543=(($5537+($5542<<2))|0);
      var $5544=$5543;
      var $5545=$5544;
      HEAPF32[(($5545)>>2)]=$5534;
      __label__ = 245; break;
    case 208: 
      var $5547=$2;
      var $5548=(($5547+64)|0);
      var $5549=HEAP32[(($5548)>>2)];
      var $5550=$st;
      var $5551=(($5550+2)|0);
      var $5552=$5551;
      var $5553=HEAP16[(($5552)>>1)];
      var $5554=(($5553)&65535);
      var $5555=(($5549+($5554<<2))|0);
      var $5556=$5555;
      var $5557=$5556;
      var $5558=HEAP32[(($5557)>>2)];
      var $5559=$5558 & 2147483647;
      var $5560=(($5559)|0)!=0;
      if ($5560) { __label__ = 209; break; } else { __label__ = 212; break; }
    case 209: 
      var $5562=$st;
      var $5563=(($5562+4)|0);
      var $5564=$5563;
      var $5565=HEAP16[(($5564)>>1)];
      var $5566=(($5565 << 16) >> 16);
      var $5567=((($5566)-(1))|0);
      var $5568=$st;
      var $5569=(($5568+($5567<<3))|0);
      $st=$5569;
      var $5570=$jumpcount;
      var $5571=((($5570)+(1))|0);
      $jumpcount=$5571;
      var $5572=$5;
      var $5573=(($5571)|0) >= (($5572)|0);
      if ($5573) { __label__ = 210; break; } else { __label__ = 211; break; }
    case 210: 
      var $5575=$2;
      var $5576=$2;
      var $5577=(($5576)|0);
      var $5578=HEAP32[(($5577)>>2)];
      var $5579=$jumpcount;
      _qcvmerror($5575, ((STRING_TABLE.__str22)|0), (tempInt=STACKTOP,STACKTOP += 8,assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),HEAP32[((tempInt)>>2)]=$5578,HEAP32[(((tempInt)+(4))>>2)]=$5579,tempInt));
      __label__ = 211; break;
    case 211: 
      __label__ = 212; break;
    case 212: 
      __label__ = 245; break;
    case 213: 
      var $5583=$2;
      var $5584=(($5583+64)|0);
      var $5585=HEAP32[(($5584)>>2)];
      var $5586=$st;
      var $5587=(($5586+2)|0);
      var $5588=$5587;
      var $5589=HEAP16[(($5588)>>1)];
      var $5590=(($5589)&65535);
      var $5591=(($5585+($5590<<2))|0);
      var $5592=$5591;
      var $5593=$5592;
      var $5594=HEAP32[(($5593)>>2)];
      var $5595=$5594 & 2147483647;
      var $5596=(($5595)|0)!=0;
      if ($5596) { __label__ = 217; break; } else { __label__ = 214; break; }
    case 214: 
      var $5598=$st;
      var $5599=(($5598+4)|0);
      var $5600=$5599;
      var $5601=HEAP16[(($5600)>>1)];
      var $5602=(($5601 << 16) >> 16);
      var $5603=((($5602)-(1))|0);
      var $5604=$st;
      var $5605=(($5604+($5603<<3))|0);
      $st=$5605;
      var $5606=$jumpcount;
      var $5607=((($5606)+(1))|0);
      $jumpcount=$5607;
      var $5608=$5;
      var $5609=(($5607)|0) >= (($5608)|0);
      if ($5609) { __label__ = 215; break; } else { __label__ = 216; break; }
    case 215: 
      var $5611=$2;
      var $5612=$2;
      var $5613=(($5612)|0);
      var $5614=HEAP32[(($5613)>>2)];
      var $5615=$jumpcount;
      _qcvmerror($5611, ((STRING_TABLE.__str22)|0), (tempInt=STACKTOP,STACKTOP += 8,assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),HEAP32[((tempInt)>>2)]=$5614,HEAP32[(((tempInt)+(4))>>2)]=$5615,tempInt));
      __label__ = 216; break;
    case 216: 
      __label__ = 217; break;
    case 217: 
      __label__ = 245; break;
    case 218: 
      var $5619=$st;
      var $5620=(($5619)|0);
      var $5621=HEAP16[(($5620)>>1)];
      var $5622=(($5621)&65535);
      var $5623=((($5622)-(51))|0);
      var $5624=$2;
      var $5625=(($5624+184)|0);
      HEAP32[(($5625)>>2)]=$5623;
      var $5626=$2;
      var $5627=(($5626+64)|0);
      var $5628=HEAP32[(($5627)>>2)];
      var $5629=$st;
      var $5630=(($5629+2)|0);
      var $5631=$5630;
      var $5632=HEAP16[(($5631)>>1)];
      var $5633=(($5632)&65535);
      var $5634=(($5628+($5633<<2))|0);
      var $5635=$5634;
      var $5636=$5635;
      var $5637=HEAP32[(($5636)>>2)];
      var $5638=(($5637)|0)!=0;
      if ($5638) { __label__ = 220; break; } else { __label__ = 219; break; }
    case 219: 
      var $5640=$2;
      var $5641=$2;
      var $5642=(($5641)|0);
      var $5643=HEAP32[(($5642)>>2)];
      _qcvmerror($5640, ((STRING_TABLE.__str23)|0), (tempInt=STACKTOP,STACKTOP += 4,assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),HEAP32[((tempInt)>>2)]=$5643,tempInt));
      __label__ = 220; break;
    case 220: 
      var $5645=$2;
      var $5646=(($5645+64)|0);
      var $5647=HEAP32[(($5646)>>2)];
      var $5648=$st;
      var $5649=(($5648+2)|0);
      var $5650=$5649;
      var $5651=HEAP16[(($5650)>>1)];
      var $5652=(($5651)&65535);
      var $5653=(($5647+($5652<<2))|0);
      var $5654=$5653;
      var $5655=$5654;
      var $5656=HEAP32[(($5655)>>2)];
      var $5657=(($5656)|0)!=0;
      if ($5657) { __label__ = 221; break; } else { __label__ = 222; break; }
    case 221: 
      var $5659=$2;
      var $5660=(($5659+64)|0);
      var $5661=HEAP32[(($5660)>>2)];
      var $5662=$st;
      var $5663=(($5662+2)|0);
      var $5664=$5663;
      var $5665=HEAP16[(($5664)>>1)];
      var $5666=(($5665)&65535);
      var $5667=(($5661+($5666<<2))|0);
      var $5668=$5667;
      var $5669=$5668;
      var $5670=HEAP32[(($5669)>>2)];
      var $5671=$2;
      var $5672=(($5671+44)|0);
      var $5673=HEAP32[(($5672)>>2)];
      var $5674=(($5670)>>>0) >= (($5673)>>>0);
      if ($5674) { __label__ = 222; break; } else { __label__ = 223; break; }
    case 222: 
      var $5676=$2;
      var $5677=$2;
      var $5678=(($5677)|0);
      var $5679=HEAP32[(($5678)>>2)];
      _qcvmerror($5676, ((STRING_TABLE.__str24)|0), (tempInt=STACKTOP,STACKTOP += 4,assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),HEAP32[((tempInt)>>2)]=$5679,tempInt));
      __label__ = 488; break;
    case 223: 
      var $5681=$2;
      var $5682=(($5681+64)|0);
      var $5683=HEAP32[(($5682)>>2)];
      var $5684=$st;
      var $5685=(($5684+2)|0);
      var $5686=$5685;
      var $5687=HEAP16[(($5686)>>1)];
      var $5688=(($5687)&65535);
      var $5689=(($5683+($5688<<2))|0);
      var $5690=$5689;
      var $5691=$5690;
      var $5692=HEAP32[(($5691)>>2)];
      var $5693=$2;
      var $5694=(($5693+40)|0);
      var $5695=HEAP32[(($5694)>>2)];
      var $5696=(($5695+($5692)*(36))|0);
      $newf1=$5696;
      var $5697=$newf1;
      var $5698=(($5697+12)|0);
      var $5699=HEAP32[(($5698)>>2)];
      var $5700=((($5699)+(1))|0);
      HEAP32[(($5698)>>2)]=$5700;
      var $5701=$st;
      var $5702=$2;
      var $5703=(($5702+4)|0);
      var $5704=HEAP32[(($5703)>>2)];
      var $5705=$5701;
      var $5706=$5704;
      var $5707=((($5705)-($5706))|0);
      var $5708=((((($5707)|0))/(8))&-1);
      var $5709=((($5708)+(1))|0);
      var $5710=$2;
      var $5711=(($5710+176)|0);
      HEAP32[(($5711)>>2)]=$5709;
      var $5712=$newf1;
      var $5713=(($5712)|0);
      var $5714=HEAP32[(($5713)>>2)];
      var $5715=(($5714)|0) < 0;
      if ($5715) { __label__ = 224; break; } else { __label__ = 229; break; }
    case 224: 
      var $5717=$newf1;
      var $5718=(($5717)|0);
      var $5719=HEAP32[(($5718)>>2)];
      var $5720=(((-$5719))|0);
      $builtinnumber4=$5720;
      var $5721=$builtinnumber4;
      var $5722=$2;
      var $5723=(($5722+132)|0);
      var $5724=HEAP32[(($5723)>>2)];
      var $5725=(($5721)>>>0) < (($5724)>>>0);
      if ($5725) { __label__ = 225; break; } else { __label__ = 227; break; }
    case 225: 
      var $5727=$builtinnumber4;
      var $5728=$2;
      var $5729=(($5728+128)|0);
      var $5730=HEAP32[(($5729)>>2)];
      var $5731=(($5730+($5727<<2))|0);
      var $5732=HEAP32[(($5731)>>2)];
      var $5733=(($5732)|0)!=0;
      if ($5733) { __label__ = 226; break; } else { __label__ = 227; break; }
    case 226: 
      var $5735=$builtinnumber4;
      var $5736=$2;
      var $5737=(($5736+128)|0);
      var $5738=HEAP32[(($5737)>>2)];
      var $5739=(($5738+($5735<<2))|0);
      var $5740=HEAP32[(($5739)>>2)];
      var $5741=$2;
      var $5742=FUNCTION_TABLE[$5740]($5741);
      __label__ = 228; break;
    case 227: 
      var $5744=$2;
      var $5745=$builtinnumber4;
      var $5746=$2;
      var $5747=(($5746)|0);
      var $5748=HEAP32[(($5747)>>2)];
      _qcvmerror($5744, ((STRING_TABLE.__str25)|0), (tempInt=STACKTOP,STACKTOP += 8,assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),HEAP32[((tempInt)>>2)]=$5745,HEAP32[(((tempInt)+(4))>>2)]=$5748,tempInt));
      __label__ = 228; break;
    case 228: 
      __label__ = 230; break;
    case 229: 
      var $5751=$2;
      var $5752=(($5751+4)|0);
      var $5753=HEAP32[(($5752)>>2)];
      var $5754=$2;
      var $5755=$newf1;
      var $5756=_prog_enterfunction($5754, $5755);
      var $5757=(($5753+($5756<<3))|0);
      var $5758=((($5757)-(8))|0);
      $st=$5758;
      __label__ = 230; break;
    case 230: 
      var $5760=$2;
      var $5761=(($5760+112)|0);
      var $5762=HEAP32[(($5761)>>2)];
      var $5763=(($5762)|0)!=0;
      if ($5763) { __label__ = 231; break; } else { __label__ = 232; break; }
    case 231: 
      __label__ = 488; break;
    case 232: 
      __label__ = 245; break;
    case 233: 
      var $5767=$2;
      var $5768=$2;
      var $5769=(($5768)|0);
      var $5770=HEAP32[(($5769)>>2)];
      _qcvmerror($5767, ((STRING_TABLE.__str26)|0), (tempInt=STACKTOP,STACKTOP += 4,assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),HEAP32[((tempInt)>>2)]=$5770,tempInt));
      __label__ = 245; break;
    case 234: 
      var $5772=$st;
      var $5773=(($5772+2)|0);
      var $5774=$5773;
      var $5775=HEAP16[(($5774)>>1)];
      var $5776=(($5775 << 16) >> 16);
      var $5777=((($5776)-(1))|0);
      var $5778=$st;
      var $5779=(($5778+($5777<<3))|0);
      $st=$5779;
      var $5780=$jumpcount;
      var $5781=((($5780)+(1))|0);
      $jumpcount=$5781;
      var $5782=(($5781)|0)==10000000;
      if ($5782) { __label__ = 235; break; } else { __label__ = 236; break; }
    case 235: 
      var $5784=$2;
      var $5785=$2;
      var $5786=(($5785)|0);
      var $5787=HEAP32[(($5786)>>2)];
      var $5788=$jumpcount;
      _qcvmerror($5784, ((STRING_TABLE.__str22)|0), (tempInt=STACKTOP,STACKTOP += 8,assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),HEAP32[((tempInt)>>2)]=$5787,HEAP32[(((tempInt)+(4))>>2)]=$5788,tempInt));
      __label__ = 236; break;
    case 236: 
      __label__ = 245; break;
    case 237: 
      var $5791=$2;
      var $5792=(($5791+64)|0);
      var $5793=HEAP32[(($5792)>>2)];
      var $5794=$st;
      var $5795=(($5794+2)|0);
      var $5796=$5795;
      var $5797=HEAP16[(($5796)>>1)];
      var $5798=(($5797)&65535);
      var $5799=(($5793+($5798<<2))|0);
      var $5800=$5799;
      var $5801=$5800;
      var $5802=HEAP32[(($5801)>>2)];
      var $5803=$5802 & 2147483647;
      var $5804=(($5803)|0)!=0;
      if ($5804) { __label__ = 238; break; } else { var $5821 = 0;__label__ = 239; break; }
    case 238: 
      var $5806=$2;
      var $5807=(($5806+64)|0);
      var $5808=HEAP32[(($5807)>>2)];
      var $5809=$st;
      var $5810=(($5809+4)|0);
      var $5811=$5810;
      var $5812=HEAP16[(($5811)>>1)];
      var $5813=(($5812)&65535);
      var $5814=(($5808+($5813<<2))|0);
      var $5815=$5814;
      var $5816=$5815;
      var $5817=HEAP32[(($5816)>>2)];
      var $5818=$5817 & 2147483647;
      var $5819=(($5818)|0)!=0;
      var $5821 = $5819;__label__ = 239; break;
    case 239: 
      var $5821;
      var $5822=(($5821)&1);
      var $5823=(($5822)|0);
      var $5824=$2;
      var $5825=(($5824+64)|0);
      var $5826=HEAP32[(($5825)>>2)];
      var $5827=$st;
      var $5828=(($5827+6)|0);
      var $5829=$5828;
      var $5830=HEAP16[(($5829)>>1)];
      var $5831=(($5830)&65535);
      var $5832=(($5826+($5831<<2))|0);
      var $5833=$5832;
      var $5834=$5833;
      HEAPF32[(($5834)>>2)]=$5823;
      __label__ = 245; break;
    case 240: 
      var $5836=$2;
      var $5837=(($5836+64)|0);
      var $5838=HEAP32[(($5837)>>2)];
      var $5839=$st;
      var $5840=(($5839+2)|0);
      var $5841=$5840;
      var $5842=HEAP16[(($5841)>>1)];
      var $5843=(($5842)&65535);
      var $5844=(($5838+($5843<<2))|0);
      var $5845=$5844;
      var $5846=$5845;
      var $5847=HEAP32[(($5846)>>2)];
      var $5848=$5847 & 2147483647;
      var $5849=(($5848)|0)!=0;
      if ($5849) { var $5866 = 1;__label__ = 242; break; } else { __label__ = 241; break; }
    case 241: 
      var $5851=$2;
      var $5852=(($5851+64)|0);
      var $5853=HEAP32[(($5852)>>2)];
      var $5854=$st;
      var $5855=(($5854+4)|0);
      var $5856=$5855;
      var $5857=HEAP16[(($5856)>>1)];
      var $5858=(($5857)&65535);
      var $5859=(($5853+($5858<<2))|0);
      var $5860=$5859;
      var $5861=$5860;
      var $5862=HEAP32[(($5861)>>2)];
      var $5863=$5862 & 2147483647;
      var $5864=(($5863)|0)!=0;
      var $5866 = $5864;__label__ = 242; break;
    case 242: 
      var $5866;
      var $5867=(($5866)&1);
      var $5868=(($5867)|0);
      var $5869=$2;
      var $5870=(($5869+64)|0);
      var $5871=HEAP32[(($5870)>>2)];
      var $5872=$st;
      var $5873=(($5872+6)|0);
      var $5874=$5873;
      var $5875=HEAP16[(($5874)>>1)];
      var $5876=(($5875)&65535);
      var $5877=(($5871+($5876<<2))|0);
      var $5878=$5877;
      var $5879=$5878;
      HEAPF32[(($5879)>>2)]=$5868;
      __label__ = 245; break;
    case 243: 
      var $5881=$2;
      var $5882=(($5881+64)|0);
      var $5883=HEAP32[(($5882)>>2)];
      var $5884=$st;
      var $5885=(($5884+2)|0);
      var $5886=$5885;
      var $5887=HEAP16[(($5886)>>1)];
      var $5888=(($5887)&65535);
      var $5889=(($5883+($5888<<2))|0);
      var $5890=$5889;
      var $5891=$5890;
      var $5892=HEAPF32[(($5891)>>2)];
      var $5893=(($5892)&-1);
      var $5894=$2;
      var $5895=(($5894+64)|0);
      var $5896=HEAP32[(($5895)>>2)];
      var $5897=$st;
      var $5898=(($5897+4)|0);
      var $5899=$5898;
      var $5900=HEAP16[(($5899)>>1)];
      var $5901=(($5900)&65535);
      var $5902=(($5896+($5901<<2))|0);
      var $5903=$5902;
      var $5904=$5903;
      var $5905=HEAPF32[(($5904)>>2)];
      var $5906=(($5905)&-1);
      var $5907=$5893 & $5906;
      var $5908=(($5907)|0);
      var $5909=$2;
      var $5910=(($5909+64)|0);
      var $5911=HEAP32[(($5910)>>2)];
      var $5912=$st;
      var $5913=(($5912+6)|0);
      var $5914=$5913;
      var $5915=HEAP16[(($5914)>>1)];
      var $5916=(($5915)&65535);
      var $5917=(($5911+($5916<<2))|0);
      var $5918=$5917;
      var $5919=$5918;
      HEAPF32[(($5919)>>2)]=$5908;
      __label__ = 245; break;
    case 244: 
      var $5921=$2;
      var $5922=(($5921+64)|0);
      var $5923=HEAP32[(($5922)>>2)];
      var $5924=$st;
      var $5925=(($5924+2)|0);
      var $5926=$5925;
      var $5927=HEAP16[(($5926)>>1)];
      var $5928=(($5927)&65535);
      var $5929=(($5923+($5928<<2))|0);
      var $5930=$5929;
      var $5931=$5930;
      var $5932=HEAPF32[(($5931)>>2)];
      var $5933=(($5932)&-1);
      var $5934=$2;
      var $5935=(($5934+64)|0);
      var $5936=HEAP32[(($5935)>>2)];
      var $5937=$st;
      var $5938=(($5937+4)|0);
      var $5939=$5938;
      var $5940=HEAP16[(($5939)>>1)];
      var $5941=(($5940)&65535);
      var $5942=(($5936+($5941<<2))|0);
      var $5943=$5942;
      var $5944=$5943;
      var $5945=HEAPF32[(($5944)>>2)];
      var $5946=(($5945)&-1);
      var $5947=$5933 | $5946;
      var $5948=(($5947)|0);
      var $5949=$2;
      var $5950=(($5949+64)|0);
      var $5951=HEAP32[(($5950)>>2)];
      var $5952=$st;
      var $5953=(($5952+6)|0);
      var $5954=$5953;
      var $5955=HEAP16[(($5954)>>1)];
      var $5956=(($5955)&65535);
      var $5957=(($5951+($5956<<2))|0);
      var $5958=$5957;
      var $5959=$5958;
      HEAPF32[(($5959)>>2)]=$5948;
      __label__ = 245; break;
    case 245: 
      __label__ = 126; break;
    case 246: 
      __label__ = 247; break;
    case 247: 
      var $5963=$st;
      var $5964=(($5963+8)|0);
      $st=$5964;
      var $5965=$st;
      var $5966=$2;
      var $5967=(($5966+4)|0);
      var $5968=HEAP32[(($5967)>>2)];
      var $5969=$5965;
      var $5970=$5968;
      var $5971=((($5969)-($5970))|0);
      var $5972=((((($5971)|0))/(8))&-1);
      var $5973=$2;
      var $5974=(($5973+116)|0);
      var $5975=HEAP32[(($5974)>>2)];
      var $5976=(($5975+($5972<<2))|0);
      var $5977=HEAP32[(($5976)>>2)];
      var $5978=((($5977)+(1))|0);
      HEAP32[(($5976)>>2)]=$5978;
      var $5979=$st;
      var $5980=(($5979)|0);
      var $5981=HEAP16[(($5980)>>1)];
      var $5982=(($5981)&65535);
      if ((($5982)|0) == 0 || (($5982)|0) == 43) {
        __label__ = 249; break;
      }
      else if ((($5982)|0) == 1) {
        __label__ = 252; break;
      }
      else if ((($5982)|0) == 2) {
        __label__ = 253; break;
      }
      else if ((($5982)|0) == 3) {
        __label__ = 254; break;
      }
      else if ((($5982)|0) == 4) {
        __label__ = 255; break;
      }
      else if ((($5982)|0) == 5) {
        __label__ = 256; break;
      }
      else if ((($5982)|0) == 6) {
        __label__ = 260; break;
      }
      else if ((($5982)|0) == 7) {
        __label__ = 261; break;
      }
      else if ((($5982)|0) == 8) {
        __label__ = 262; break;
      }
      else if ((($5982)|0) == 9) {
        __label__ = 263; break;
      }
      else if ((($5982)|0) == 10) {
        __label__ = 264; break;
      }
      else if ((($5982)|0) == 11) {
        __label__ = 265; break;
      }
      else if ((($5982)|0) == 12) {
        __label__ = 269; break;
      }
      else if ((($5982)|0) == 13) {
        __label__ = 270; break;
      }
      else if ((($5982)|0) == 14) {
        __label__ = 271; break;
      }
      else if ((($5982)|0) == 15) {
        __label__ = 272; break;
      }
      else if ((($5982)|0) == 16) {
        __label__ = 273; break;
      }
      else if ((($5982)|0) == 17) {
        __label__ = 277; break;
      }
      else if ((($5982)|0) == 18) {
        __label__ = 278; break;
      }
      else if ((($5982)|0) == 19) {
        __label__ = 279; break;
      }
      else if ((($5982)|0) == 20) {
        __label__ = 280; break;
      }
      else if ((($5982)|0) == 21) {
        __label__ = 281; break;
      }
      else if ((($5982)|0) == 22) {
        __label__ = 282; break;
      }
      else if ((($5982)|0) == 23) {
        __label__ = 283; break;
      }
      else if ((($5982)|0) == 24 || (($5982)|0) == 26 || (($5982)|0) == 28 || (($5982)|0) == 27 || (($5982)|0) == 29) {
        __label__ = 284; break;
      }
      else if ((($5982)|0) == 25) {
        __label__ = 290; break;
      }
      else if ((($5982)|0) == 30) {
        __label__ = 297; break;
      }
      else if ((($5982)|0) == 31 || (($5982)|0) == 33 || (($5982)|0) == 34 || (($5982)|0) == 35 || (($5982)|0) == 36) {
        __label__ = 303; break;
      }
      else if ((($5982)|0) == 32) {
        __label__ = 304; break;
      }
      else if ((($5982)|0) == 37 || (($5982)|0) == 39 || (($5982)|0) == 40 || (($5982)|0) == 41 || (($5982)|0) == 42) {
        __label__ = 305; break;
      }
      else if ((($5982)|0) == 38) {
        __label__ = 312; break;
      }
      else if ((($5982)|0) == 44) {
        __label__ = 319; break;
      }
      else if ((($5982)|0) == 45) {
        __label__ = 320; break;
      }
      else if ((($5982)|0) == 46) {
        __label__ = 324; break;
      }
      else if ((($5982)|0) == 47) {
        __label__ = 327; break;
      }
      else if ((($5982)|0) == 48) {
        __label__ = 328; break;
      }
      else if ((($5982)|0) == 49) {
        __label__ = 329; break;
      }
      else if ((($5982)|0) == 50) {
        __label__ = 334; break;
      }
      else if ((($5982)|0) == 51 || (($5982)|0) == 52 || (($5982)|0) == 53 || (($5982)|0) == 54 || (($5982)|0) == 55 || (($5982)|0) == 56 || (($5982)|0) == 57 || (($5982)|0) == 58 || (($5982)|0) == 59) {
        __label__ = 339; break;
      }
      else if ((($5982)|0) == 60) {
        __label__ = 354; break;
      }
      else if ((($5982)|0) == 61) {
        __label__ = 355; break;
      }
      else if ((($5982)|0) == 62) {
        __label__ = 358; break;
      }
      else if ((($5982)|0) == 63) {
        __label__ = 361; break;
      }
      else if ((($5982)|0) == 64) {
        __label__ = 364; break;
      }
      else if ((($5982)|0) == 65) {
        __label__ = 365; break;
      }
      else {
      __label__ = 248; break;
      }
      
    case 248: 
      var $5984=$2;
      var $5985=$2;
      var $5986=(($5985)|0);
      var $5987=HEAP32[(($5986)>>2)];
      _qcvmerror($5984, ((STRING_TABLE.__str16)|0), (tempInt=STACKTOP,STACKTOP += 4,assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),HEAP32[((tempInt)>>2)]=$5987,tempInt));
      __label__ = 488; break;
    case 249: 
      var $5989=$2;
      var $5990=(($5989+64)|0);
      var $5991=HEAP32[(($5990)>>2)];
      var $5992=$st;
      var $5993=(($5992+2)|0);
      var $5994=$5993;
      var $5995=HEAP16[(($5994)>>1)];
      var $5996=(($5995)&65535);
      var $5997=(($5991+($5996<<2))|0);
      var $5998=$5997;
      var $5999=$5998;
      var $6000=(($5999)|0);
      var $6001=HEAP32[(($6000)>>2)];
      var $6002=$2;
      var $6003=(($6002+64)|0);
      var $6004=HEAP32[(($6003)>>2)];
      var $6005=(($6004+4)|0);
      var $6006=$6005;
      var $6007=$6006;
      var $6008=(($6007)|0);
      HEAP32[(($6008)>>2)]=$6001;
      var $6009=$2;
      var $6010=(($6009+64)|0);
      var $6011=HEAP32[(($6010)>>2)];
      var $6012=$st;
      var $6013=(($6012+2)|0);
      var $6014=$6013;
      var $6015=HEAP16[(($6014)>>1)];
      var $6016=(($6015)&65535);
      var $6017=(($6011+($6016<<2))|0);
      var $6018=$6017;
      var $6019=$6018;
      var $6020=(($6019+4)|0);
      var $6021=HEAP32[(($6020)>>2)];
      var $6022=$2;
      var $6023=(($6022+64)|0);
      var $6024=HEAP32[(($6023)>>2)];
      var $6025=(($6024+4)|0);
      var $6026=$6025;
      var $6027=$6026;
      var $6028=(($6027+4)|0);
      HEAP32[(($6028)>>2)]=$6021;
      var $6029=$2;
      var $6030=(($6029+64)|0);
      var $6031=HEAP32[(($6030)>>2)];
      var $6032=$st;
      var $6033=(($6032+2)|0);
      var $6034=$6033;
      var $6035=HEAP16[(($6034)>>1)];
      var $6036=(($6035)&65535);
      var $6037=(($6031+($6036<<2))|0);
      var $6038=$6037;
      var $6039=$6038;
      var $6040=(($6039+8)|0);
      var $6041=HEAP32[(($6040)>>2)];
      var $6042=$2;
      var $6043=(($6042+64)|0);
      var $6044=HEAP32[(($6043)>>2)];
      var $6045=(($6044+4)|0);
      var $6046=$6045;
      var $6047=$6046;
      var $6048=(($6047+8)|0);
      HEAP32[(($6048)>>2)]=$6041;
      var $6049=$2;
      var $6050=(($6049+4)|0);
      var $6051=HEAP32[(($6050)>>2)];
      var $6052=$2;
      var $6053=_prog_leavefunction($6052);
      var $6054=(($6051+($6053<<3))|0);
      $st=$6054;
      var $6055=$2;
      var $6056=(($6055+168)|0);
      var $6057=HEAP32[(($6056)>>2)];
      var $6058=(($6057)|0)!=0;
      if ($6058) { __label__ = 251; break; } else { __label__ = 250; break; }
    case 250: 
      __label__ = 488; break;
    case 251: 
      __label__ = 366; break;
    case 252: 
      var $6062=$2;
      var $6063=(($6062+64)|0);
      var $6064=HEAP32[(($6063)>>2)];
      var $6065=$st;
      var $6066=(($6065+2)|0);
      var $6067=$6066;
      var $6068=HEAP16[(($6067)>>1)];
      var $6069=(($6068)&65535);
      var $6070=(($6064+($6069<<2))|0);
      var $6071=$6070;
      var $6072=$6071;
      var $6073=HEAPF32[(($6072)>>2)];
      var $6074=$2;
      var $6075=(($6074+64)|0);
      var $6076=HEAP32[(($6075)>>2)];
      var $6077=$st;
      var $6078=(($6077+4)|0);
      var $6079=$6078;
      var $6080=HEAP16[(($6079)>>1)];
      var $6081=(($6080)&65535);
      var $6082=(($6076+($6081<<2))|0);
      var $6083=$6082;
      var $6084=$6083;
      var $6085=HEAPF32[(($6084)>>2)];
      var $6086=($6073)*($6085);
      var $6087=$2;
      var $6088=(($6087+64)|0);
      var $6089=HEAP32[(($6088)>>2)];
      var $6090=$st;
      var $6091=(($6090+6)|0);
      var $6092=$6091;
      var $6093=HEAP16[(($6092)>>1)];
      var $6094=(($6093)&65535);
      var $6095=(($6089+($6094<<2))|0);
      var $6096=$6095;
      var $6097=$6096;
      HEAPF32[(($6097)>>2)]=$6086;
      __label__ = 366; break;
    case 253: 
      var $6099=$2;
      var $6100=(($6099+64)|0);
      var $6101=HEAP32[(($6100)>>2)];
      var $6102=$st;
      var $6103=(($6102+2)|0);
      var $6104=$6103;
      var $6105=HEAP16[(($6104)>>1)];
      var $6106=(($6105)&65535);
      var $6107=(($6101+($6106<<2))|0);
      var $6108=$6107;
      var $6109=$6108;
      var $6110=(($6109)|0);
      var $6111=HEAPF32[(($6110)>>2)];
      var $6112=$2;
      var $6113=(($6112+64)|0);
      var $6114=HEAP32[(($6113)>>2)];
      var $6115=$st;
      var $6116=(($6115+4)|0);
      var $6117=$6116;
      var $6118=HEAP16[(($6117)>>1)];
      var $6119=(($6118)&65535);
      var $6120=(($6114+($6119<<2))|0);
      var $6121=$6120;
      var $6122=$6121;
      var $6123=(($6122)|0);
      var $6124=HEAPF32[(($6123)>>2)];
      var $6125=($6111)*($6124);
      var $6126=$2;
      var $6127=(($6126+64)|0);
      var $6128=HEAP32[(($6127)>>2)];
      var $6129=$st;
      var $6130=(($6129+2)|0);
      var $6131=$6130;
      var $6132=HEAP16[(($6131)>>1)];
      var $6133=(($6132)&65535);
      var $6134=(($6128+($6133<<2))|0);
      var $6135=$6134;
      var $6136=$6135;
      var $6137=(($6136+4)|0);
      var $6138=HEAPF32[(($6137)>>2)];
      var $6139=$2;
      var $6140=(($6139+64)|0);
      var $6141=HEAP32[(($6140)>>2)];
      var $6142=$st;
      var $6143=(($6142+4)|0);
      var $6144=$6143;
      var $6145=HEAP16[(($6144)>>1)];
      var $6146=(($6145)&65535);
      var $6147=(($6141+($6146<<2))|0);
      var $6148=$6147;
      var $6149=$6148;
      var $6150=(($6149+4)|0);
      var $6151=HEAPF32[(($6150)>>2)];
      var $6152=($6138)*($6151);
      var $6153=($6125)+($6152);
      var $6154=$2;
      var $6155=(($6154+64)|0);
      var $6156=HEAP32[(($6155)>>2)];
      var $6157=$st;
      var $6158=(($6157+2)|0);
      var $6159=$6158;
      var $6160=HEAP16[(($6159)>>1)];
      var $6161=(($6160)&65535);
      var $6162=(($6156+($6161<<2))|0);
      var $6163=$6162;
      var $6164=$6163;
      var $6165=(($6164+8)|0);
      var $6166=HEAPF32[(($6165)>>2)];
      var $6167=$2;
      var $6168=(($6167+64)|0);
      var $6169=HEAP32[(($6168)>>2)];
      var $6170=$st;
      var $6171=(($6170+4)|0);
      var $6172=$6171;
      var $6173=HEAP16[(($6172)>>1)];
      var $6174=(($6173)&65535);
      var $6175=(($6169+($6174<<2))|0);
      var $6176=$6175;
      var $6177=$6176;
      var $6178=(($6177+8)|0);
      var $6179=HEAPF32[(($6178)>>2)];
      var $6180=($6166)*($6179);
      var $6181=($6153)+($6180);
      var $6182=$2;
      var $6183=(($6182+64)|0);
      var $6184=HEAP32[(($6183)>>2)];
      var $6185=$st;
      var $6186=(($6185+6)|0);
      var $6187=$6186;
      var $6188=HEAP16[(($6187)>>1)];
      var $6189=(($6188)&65535);
      var $6190=(($6184+($6189<<2))|0);
      var $6191=$6190;
      var $6192=$6191;
      HEAPF32[(($6192)>>2)]=$6181;
      __label__ = 366; break;
    case 254: 
      var $6194=$2;
      var $6195=(($6194+64)|0);
      var $6196=HEAP32[(($6195)>>2)];
      var $6197=$st;
      var $6198=(($6197+2)|0);
      var $6199=$6198;
      var $6200=HEAP16[(($6199)>>1)];
      var $6201=(($6200)&65535);
      var $6202=(($6196+($6201<<2))|0);
      var $6203=$6202;
      var $6204=$6203;
      var $6205=HEAPF32[(($6204)>>2)];
      var $6206=$2;
      var $6207=(($6206+64)|0);
      var $6208=HEAP32[(($6207)>>2)];
      var $6209=$st;
      var $6210=(($6209+4)|0);
      var $6211=$6210;
      var $6212=HEAP16[(($6211)>>1)];
      var $6213=(($6212)&65535);
      var $6214=(($6208+($6213<<2))|0);
      var $6215=$6214;
      var $6216=$6215;
      var $6217=(($6216)|0);
      var $6218=HEAPF32[(($6217)>>2)];
      var $6219=($6205)*($6218);
      var $6220=$2;
      var $6221=(($6220+64)|0);
      var $6222=HEAP32[(($6221)>>2)];
      var $6223=$st;
      var $6224=(($6223+6)|0);
      var $6225=$6224;
      var $6226=HEAP16[(($6225)>>1)];
      var $6227=(($6226)&65535);
      var $6228=(($6222+($6227<<2))|0);
      var $6229=$6228;
      var $6230=$6229;
      var $6231=(($6230)|0);
      HEAPF32[(($6231)>>2)]=$6219;
      var $6232=$2;
      var $6233=(($6232+64)|0);
      var $6234=HEAP32[(($6233)>>2)];
      var $6235=$st;
      var $6236=(($6235+2)|0);
      var $6237=$6236;
      var $6238=HEAP16[(($6237)>>1)];
      var $6239=(($6238)&65535);
      var $6240=(($6234+($6239<<2))|0);
      var $6241=$6240;
      var $6242=$6241;
      var $6243=HEAPF32[(($6242)>>2)];
      var $6244=$2;
      var $6245=(($6244+64)|0);
      var $6246=HEAP32[(($6245)>>2)];
      var $6247=$st;
      var $6248=(($6247+4)|0);
      var $6249=$6248;
      var $6250=HEAP16[(($6249)>>1)];
      var $6251=(($6250)&65535);
      var $6252=(($6246+($6251<<2))|0);
      var $6253=$6252;
      var $6254=$6253;
      var $6255=(($6254+4)|0);
      var $6256=HEAPF32[(($6255)>>2)];
      var $6257=($6243)*($6256);
      var $6258=$2;
      var $6259=(($6258+64)|0);
      var $6260=HEAP32[(($6259)>>2)];
      var $6261=$st;
      var $6262=(($6261+6)|0);
      var $6263=$6262;
      var $6264=HEAP16[(($6263)>>1)];
      var $6265=(($6264)&65535);
      var $6266=(($6260+($6265<<2))|0);
      var $6267=$6266;
      var $6268=$6267;
      var $6269=(($6268+4)|0);
      HEAPF32[(($6269)>>2)]=$6257;
      var $6270=$2;
      var $6271=(($6270+64)|0);
      var $6272=HEAP32[(($6271)>>2)];
      var $6273=$st;
      var $6274=(($6273+2)|0);
      var $6275=$6274;
      var $6276=HEAP16[(($6275)>>1)];
      var $6277=(($6276)&65535);
      var $6278=(($6272+($6277<<2))|0);
      var $6279=$6278;
      var $6280=$6279;
      var $6281=HEAPF32[(($6280)>>2)];
      var $6282=$2;
      var $6283=(($6282+64)|0);
      var $6284=HEAP32[(($6283)>>2)];
      var $6285=$st;
      var $6286=(($6285+4)|0);
      var $6287=$6286;
      var $6288=HEAP16[(($6287)>>1)];
      var $6289=(($6288)&65535);
      var $6290=(($6284+($6289<<2))|0);
      var $6291=$6290;
      var $6292=$6291;
      var $6293=(($6292+8)|0);
      var $6294=HEAPF32[(($6293)>>2)];
      var $6295=($6281)*($6294);
      var $6296=$2;
      var $6297=(($6296+64)|0);
      var $6298=HEAP32[(($6297)>>2)];
      var $6299=$st;
      var $6300=(($6299+6)|0);
      var $6301=$6300;
      var $6302=HEAP16[(($6301)>>1)];
      var $6303=(($6302)&65535);
      var $6304=(($6298+($6303<<2))|0);
      var $6305=$6304;
      var $6306=$6305;
      var $6307=(($6306+8)|0);
      HEAPF32[(($6307)>>2)]=$6295;
      __label__ = 366; break;
    case 255: 
      var $6309=$2;
      var $6310=(($6309+64)|0);
      var $6311=HEAP32[(($6310)>>2)];
      var $6312=$st;
      var $6313=(($6312+4)|0);
      var $6314=$6313;
      var $6315=HEAP16[(($6314)>>1)];
      var $6316=(($6315)&65535);
      var $6317=(($6311+($6316<<2))|0);
      var $6318=$6317;
      var $6319=$6318;
      var $6320=HEAPF32[(($6319)>>2)];
      var $6321=$2;
      var $6322=(($6321+64)|0);
      var $6323=HEAP32[(($6322)>>2)];
      var $6324=$st;
      var $6325=(($6324+2)|0);
      var $6326=$6325;
      var $6327=HEAP16[(($6326)>>1)];
      var $6328=(($6327)&65535);
      var $6329=(($6323+($6328<<2))|0);
      var $6330=$6329;
      var $6331=$6330;
      var $6332=(($6331)|0);
      var $6333=HEAPF32[(($6332)>>2)];
      var $6334=($6320)*($6333);
      var $6335=$2;
      var $6336=(($6335+64)|0);
      var $6337=HEAP32[(($6336)>>2)];
      var $6338=$st;
      var $6339=(($6338+6)|0);
      var $6340=$6339;
      var $6341=HEAP16[(($6340)>>1)];
      var $6342=(($6341)&65535);
      var $6343=(($6337+($6342<<2))|0);
      var $6344=$6343;
      var $6345=$6344;
      var $6346=(($6345)|0);
      HEAPF32[(($6346)>>2)]=$6334;
      var $6347=$2;
      var $6348=(($6347+64)|0);
      var $6349=HEAP32[(($6348)>>2)];
      var $6350=$st;
      var $6351=(($6350+4)|0);
      var $6352=$6351;
      var $6353=HEAP16[(($6352)>>1)];
      var $6354=(($6353)&65535);
      var $6355=(($6349+($6354<<2))|0);
      var $6356=$6355;
      var $6357=$6356;
      var $6358=HEAPF32[(($6357)>>2)];
      var $6359=$2;
      var $6360=(($6359+64)|0);
      var $6361=HEAP32[(($6360)>>2)];
      var $6362=$st;
      var $6363=(($6362+2)|0);
      var $6364=$6363;
      var $6365=HEAP16[(($6364)>>1)];
      var $6366=(($6365)&65535);
      var $6367=(($6361+($6366<<2))|0);
      var $6368=$6367;
      var $6369=$6368;
      var $6370=(($6369+4)|0);
      var $6371=HEAPF32[(($6370)>>2)];
      var $6372=($6358)*($6371);
      var $6373=$2;
      var $6374=(($6373+64)|0);
      var $6375=HEAP32[(($6374)>>2)];
      var $6376=$st;
      var $6377=(($6376+6)|0);
      var $6378=$6377;
      var $6379=HEAP16[(($6378)>>1)];
      var $6380=(($6379)&65535);
      var $6381=(($6375+($6380<<2))|0);
      var $6382=$6381;
      var $6383=$6382;
      var $6384=(($6383+4)|0);
      HEAPF32[(($6384)>>2)]=$6372;
      var $6385=$2;
      var $6386=(($6385+64)|0);
      var $6387=HEAP32[(($6386)>>2)];
      var $6388=$st;
      var $6389=(($6388+4)|0);
      var $6390=$6389;
      var $6391=HEAP16[(($6390)>>1)];
      var $6392=(($6391)&65535);
      var $6393=(($6387+($6392<<2))|0);
      var $6394=$6393;
      var $6395=$6394;
      var $6396=HEAPF32[(($6395)>>2)];
      var $6397=$2;
      var $6398=(($6397+64)|0);
      var $6399=HEAP32[(($6398)>>2)];
      var $6400=$st;
      var $6401=(($6400+2)|0);
      var $6402=$6401;
      var $6403=HEAP16[(($6402)>>1)];
      var $6404=(($6403)&65535);
      var $6405=(($6399+($6404<<2))|0);
      var $6406=$6405;
      var $6407=$6406;
      var $6408=(($6407+8)|0);
      var $6409=HEAPF32[(($6408)>>2)];
      var $6410=($6396)*($6409);
      var $6411=$2;
      var $6412=(($6411+64)|0);
      var $6413=HEAP32[(($6412)>>2)];
      var $6414=$st;
      var $6415=(($6414+6)|0);
      var $6416=$6415;
      var $6417=HEAP16[(($6416)>>1)];
      var $6418=(($6417)&65535);
      var $6419=(($6413+($6418<<2))|0);
      var $6420=$6419;
      var $6421=$6420;
      var $6422=(($6421+8)|0);
      HEAPF32[(($6422)>>2)]=$6410;
      __label__ = 366; break;
    case 256: 
      var $6424=$2;
      var $6425=(($6424+64)|0);
      var $6426=HEAP32[(($6425)>>2)];
      var $6427=$st;
      var $6428=(($6427+4)|0);
      var $6429=$6428;
      var $6430=HEAP16[(($6429)>>1)];
      var $6431=(($6430)&65535);
      var $6432=(($6426+($6431<<2))|0);
      var $6433=$6432;
      var $6434=$6433;
      var $6435=HEAPF32[(($6434)>>2)];
      var $6436=$6435 != 0;
      if ($6436) { __label__ = 257; break; } else { __label__ = 258; break; }
    case 257: 
      var $6438=$2;
      var $6439=(($6438+64)|0);
      var $6440=HEAP32[(($6439)>>2)];
      var $6441=$st;
      var $6442=(($6441+2)|0);
      var $6443=$6442;
      var $6444=HEAP16[(($6443)>>1)];
      var $6445=(($6444)&65535);
      var $6446=(($6440+($6445<<2))|0);
      var $6447=$6446;
      var $6448=$6447;
      var $6449=HEAPF32[(($6448)>>2)];
      var $6450=$2;
      var $6451=(($6450+64)|0);
      var $6452=HEAP32[(($6451)>>2)];
      var $6453=$st;
      var $6454=(($6453+4)|0);
      var $6455=$6454;
      var $6456=HEAP16[(($6455)>>1)];
      var $6457=(($6456)&65535);
      var $6458=(($6452+($6457<<2))|0);
      var $6459=$6458;
      var $6460=$6459;
      var $6461=HEAPF32[(($6460)>>2)];
      var $6462=($6449)/($6461);
      var $6463=$2;
      var $6464=(($6463+64)|0);
      var $6465=HEAP32[(($6464)>>2)];
      var $6466=$st;
      var $6467=(($6466+6)|0);
      var $6468=$6467;
      var $6469=HEAP16[(($6468)>>1)];
      var $6470=(($6469)&65535);
      var $6471=(($6465+($6470<<2))|0);
      var $6472=$6471;
      var $6473=$6472;
      HEAPF32[(($6473)>>2)]=$6462;
      __label__ = 259; break;
    case 258: 
      var $6475=$2;
      var $6476=(($6475+64)|0);
      var $6477=HEAP32[(($6476)>>2)];
      var $6478=$st;
      var $6479=(($6478+6)|0);
      var $6480=$6479;
      var $6481=HEAP16[(($6480)>>1)];
      var $6482=(($6481)&65535);
      var $6483=(($6477+($6482<<2))|0);
      var $6484=$6483;
      var $6485=$6484;
      HEAPF32[(($6485)>>2)]=0;
      __label__ = 259; break;
    case 259: 
      __label__ = 366; break;
    case 260: 
      var $6488=$2;
      var $6489=(($6488+64)|0);
      var $6490=HEAP32[(($6489)>>2)];
      var $6491=$st;
      var $6492=(($6491+2)|0);
      var $6493=$6492;
      var $6494=HEAP16[(($6493)>>1)];
      var $6495=(($6494)&65535);
      var $6496=(($6490+($6495<<2))|0);
      var $6497=$6496;
      var $6498=$6497;
      var $6499=HEAPF32[(($6498)>>2)];
      var $6500=$2;
      var $6501=(($6500+64)|0);
      var $6502=HEAP32[(($6501)>>2)];
      var $6503=$st;
      var $6504=(($6503+4)|0);
      var $6505=$6504;
      var $6506=HEAP16[(($6505)>>1)];
      var $6507=(($6506)&65535);
      var $6508=(($6502+($6507<<2))|0);
      var $6509=$6508;
      var $6510=$6509;
      var $6511=HEAPF32[(($6510)>>2)];
      var $6512=($6499)+($6511);
      var $6513=$2;
      var $6514=(($6513+64)|0);
      var $6515=HEAP32[(($6514)>>2)];
      var $6516=$st;
      var $6517=(($6516+6)|0);
      var $6518=$6517;
      var $6519=HEAP16[(($6518)>>1)];
      var $6520=(($6519)&65535);
      var $6521=(($6515+($6520<<2))|0);
      var $6522=$6521;
      var $6523=$6522;
      HEAPF32[(($6523)>>2)]=$6512;
      __label__ = 366; break;
    case 261: 
      var $6525=$2;
      var $6526=(($6525+64)|0);
      var $6527=HEAP32[(($6526)>>2)];
      var $6528=$st;
      var $6529=(($6528+2)|0);
      var $6530=$6529;
      var $6531=HEAP16[(($6530)>>1)];
      var $6532=(($6531)&65535);
      var $6533=(($6527+($6532<<2))|0);
      var $6534=$6533;
      var $6535=$6534;
      var $6536=(($6535)|0);
      var $6537=HEAPF32[(($6536)>>2)];
      var $6538=$2;
      var $6539=(($6538+64)|0);
      var $6540=HEAP32[(($6539)>>2)];
      var $6541=$st;
      var $6542=(($6541+4)|0);
      var $6543=$6542;
      var $6544=HEAP16[(($6543)>>1)];
      var $6545=(($6544)&65535);
      var $6546=(($6540+($6545<<2))|0);
      var $6547=$6546;
      var $6548=$6547;
      var $6549=(($6548)|0);
      var $6550=HEAPF32[(($6549)>>2)];
      var $6551=($6537)+($6550);
      var $6552=$2;
      var $6553=(($6552+64)|0);
      var $6554=HEAP32[(($6553)>>2)];
      var $6555=$st;
      var $6556=(($6555+6)|0);
      var $6557=$6556;
      var $6558=HEAP16[(($6557)>>1)];
      var $6559=(($6558)&65535);
      var $6560=(($6554+($6559<<2))|0);
      var $6561=$6560;
      var $6562=$6561;
      var $6563=(($6562)|0);
      HEAPF32[(($6563)>>2)]=$6551;
      var $6564=$2;
      var $6565=(($6564+64)|0);
      var $6566=HEAP32[(($6565)>>2)];
      var $6567=$st;
      var $6568=(($6567+2)|0);
      var $6569=$6568;
      var $6570=HEAP16[(($6569)>>1)];
      var $6571=(($6570)&65535);
      var $6572=(($6566+($6571<<2))|0);
      var $6573=$6572;
      var $6574=$6573;
      var $6575=(($6574+4)|0);
      var $6576=HEAPF32[(($6575)>>2)];
      var $6577=$2;
      var $6578=(($6577+64)|0);
      var $6579=HEAP32[(($6578)>>2)];
      var $6580=$st;
      var $6581=(($6580+4)|0);
      var $6582=$6581;
      var $6583=HEAP16[(($6582)>>1)];
      var $6584=(($6583)&65535);
      var $6585=(($6579+($6584<<2))|0);
      var $6586=$6585;
      var $6587=$6586;
      var $6588=(($6587+4)|0);
      var $6589=HEAPF32[(($6588)>>2)];
      var $6590=($6576)+($6589);
      var $6591=$2;
      var $6592=(($6591+64)|0);
      var $6593=HEAP32[(($6592)>>2)];
      var $6594=$st;
      var $6595=(($6594+6)|0);
      var $6596=$6595;
      var $6597=HEAP16[(($6596)>>1)];
      var $6598=(($6597)&65535);
      var $6599=(($6593+($6598<<2))|0);
      var $6600=$6599;
      var $6601=$6600;
      var $6602=(($6601+4)|0);
      HEAPF32[(($6602)>>2)]=$6590;
      var $6603=$2;
      var $6604=(($6603+64)|0);
      var $6605=HEAP32[(($6604)>>2)];
      var $6606=$st;
      var $6607=(($6606+2)|0);
      var $6608=$6607;
      var $6609=HEAP16[(($6608)>>1)];
      var $6610=(($6609)&65535);
      var $6611=(($6605+($6610<<2))|0);
      var $6612=$6611;
      var $6613=$6612;
      var $6614=(($6613+8)|0);
      var $6615=HEAPF32[(($6614)>>2)];
      var $6616=$2;
      var $6617=(($6616+64)|0);
      var $6618=HEAP32[(($6617)>>2)];
      var $6619=$st;
      var $6620=(($6619+4)|0);
      var $6621=$6620;
      var $6622=HEAP16[(($6621)>>1)];
      var $6623=(($6622)&65535);
      var $6624=(($6618+($6623<<2))|0);
      var $6625=$6624;
      var $6626=$6625;
      var $6627=(($6626+8)|0);
      var $6628=HEAPF32[(($6627)>>2)];
      var $6629=($6615)+($6628);
      var $6630=$2;
      var $6631=(($6630+64)|0);
      var $6632=HEAP32[(($6631)>>2)];
      var $6633=$st;
      var $6634=(($6633+6)|0);
      var $6635=$6634;
      var $6636=HEAP16[(($6635)>>1)];
      var $6637=(($6636)&65535);
      var $6638=(($6632+($6637<<2))|0);
      var $6639=$6638;
      var $6640=$6639;
      var $6641=(($6640+8)|0);
      HEAPF32[(($6641)>>2)]=$6629;
      __label__ = 366; break;
    case 262: 
      var $6643=$2;
      var $6644=(($6643+64)|0);
      var $6645=HEAP32[(($6644)>>2)];
      var $6646=$st;
      var $6647=(($6646+2)|0);
      var $6648=$6647;
      var $6649=HEAP16[(($6648)>>1)];
      var $6650=(($6649)&65535);
      var $6651=(($6645+($6650<<2))|0);
      var $6652=$6651;
      var $6653=$6652;
      var $6654=HEAPF32[(($6653)>>2)];
      var $6655=$2;
      var $6656=(($6655+64)|0);
      var $6657=HEAP32[(($6656)>>2)];
      var $6658=$st;
      var $6659=(($6658+4)|0);
      var $6660=$6659;
      var $6661=HEAP16[(($6660)>>1)];
      var $6662=(($6661)&65535);
      var $6663=(($6657+($6662<<2))|0);
      var $6664=$6663;
      var $6665=$6664;
      var $6666=HEAPF32[(($6665)>>2)];
      var $6667=($6654)-($6666);
      var $6668=$2;
      var $6669=(($6668+64)|0);
      var $6670=HEAP32[(($6669)>>2)];
      var $6671=$st;
      var $6672=(($6671+6)|0);
      var $6673=$6672;
      var $6674=HEAP16[(($6673)>>1)];
      var $6675=(($6674)&65535);
      var $6676=(($6670+($6675<<2))|0);
      var $6677=$6676;
      var $6678=$6677;
      HEAPF32[(($6678)>>2)]=$6667;
      __label__ = 366; break;
    case 263: 
      var $6680=$2;
      var $6681=(($6680+64)|0);
      var $6682=HEAP32[(($6681)>>2)];
      var $6683=$st;
      var $6684=(($6683+2)|0);
      var $6685=$6684;
      var $6686=HEAP16[(($6685)>>1)];
      var $6687=(($6686)&65535);
      var $6688=(($6682+($6687<<2))|0);
      var $6689=$6688;
      var $6690=$6689;
      var $6691=(($6690)|0);
      var $6692=HEAPF32[(($6691)>>2)];
      var $6693=$2;
      var $6694=(($6693+64)|0);
      var $6695=HEAP32[(($6694)>>2)];
      var $6696=$st;
      var $6697=(($6696+4)|0);
      var $6698=$6697;
      var $6699=HEAP16[(($6698)>>1)];
      var $6700=(($6699)&65535);
      var $6701=(($6695+($6700<<2))|0);
      var $6702=$6701;
      var $6703=$6702;
      var $6704=(($6703)|0);
      var $6705=HEAPF32[(($6704)>>2)];
      var $6706=($6692)-($6705);
      var $6707=$2;
      var $6708=(($6707+64)|0);
      var $6709=HEAP32[(($6708)>>2)];
      var $6710=$st;
      var $6711=(($6710+6)|0);
      var $6712=$6711;
      var $6713=HEAP16[(($6712)>>1)];
      var $6714=(($6713)&65535);
      var $6715=(($6709+($6714<<2))|0);
      var $6716=$6715;
      var $6717=$6716;
      var $6718=(($6717)|0);
      HEAPF32[(($6718)>>2)]=$6706;
      var $6719=$2;
      var $6720=(($6719+64)|0);
      var $6721=HEAP32[(($6720)>>2)];
      var $6722=$st;
      var $6723=(($6722+2)|0);
      var $6724=$6723;
      var $6725=HEAP16[(($6724)>>1)];
      var $6726=(($6725)&65535);
      var $6727=(($6721+($6726<<2))|0);
      var $6728=$6727;
      var $6729=$6728;
      var $6730=(($6729+4)|0);
      var $6731=HEAPF32[(($6730)>>2)];
      var $6732=$2;
      var $6733=(($6732+64)|0);
      var $6734=HEAP32[(($6733)>>2)];
      var $6735=$st;
      var $6736=(($6735+4)|0);
      var $6737=$6736;
      var $6738=HEAP16[(($6737)>>1)];
      var $6739=(($6738)&65535);
      var $6740=(($6734+($6739<<2))|0);
      var $6741=$6740;
      var $6742=$6741;
      var $6743=(($6742+4)|0);
      var $6744=HEAPF32[(($6743)>>2)];
      var $6745=($6731)-($6744);
      var $6746=$2;
      var $6747=(($6746+64)|0);
      var $6748=HEAP32[(($6747)>>2)];
      var $6749=$st;
      var $6750=(($6749+6)|0);
      var $6751=$6750;
      var $6752=HEAP16[(($6751)>>1)];
      var $6753=(($6752)&65535);
      var $6754=(($6748+($6753<<2))|0);
      var $6755=$6754;
      var $6756=$6755;
      var $6757=(($6756+4)|0);
      HEAPF32[(($6757)>>2)]=$6745;
      var $6758=$2;
      var $6759=(($6758+64)|0);
      var $6760=HEAP32[(($6759)>>2)];
      var $6761=$st;
      var $6762=(($6761+2)|0);
      var $6763=$6762;
      var $6764=HEAP16[(($6763)>>1)];
      var $6765=(($6764)&65535);
      var $6766=(($6760+($6765<<2))|0);
      var $6767=$6766;
      var $6768=$6767;
      var $6769=(($6768+8)|0);
      var $6770=HEAPF32[(($6769)>>2)];
      var $6771=$2;
      var $6772=(($6771+64)|0);
      var $6773=HEAP32[(($6772)>>2)];
      var $6774=$st;
      var $6775=(($6774+4)|0);
      var $6776=$6775;
      var $6777=HEAP16[(($6776)>>1)];
      var $6778=(($6777)&65535);
      var $6779=(($6773+($6778<<2))|0);
      var $6780=$6779;
      var $6781=$6780;
      var $6782=(($6781+8)|0);
      var $6783=HEAPF32[(($6782)>>2)];
      var $6784=($6770)-($6783);
      var $6785=$2;
      var $6786=(($6785+64)|0);
      var $6787=HEAP32[(($6786)>>2)];
      var $6788=$st;
      var $6789=(($6788+6)|0);
      var $6790=$6789;
      var $6791=HEAP16[(($6790)>>1)];
      var $6792=(($6791)&65535);
      var $6793=(($6787+($6792<<2))|0);
      var $6794=$6793;
      var $6795=$6794;
      var $6796=(($6795+8)|0);
      HEAPF32[(($6796)>>2)]=$6784;
      __label__ = 366; break;
    case 264: 
      var $6798=$2;
      var $6799=(($6798+64)|0);
      var $6800=HEAP32[(($6799)>>2)];
      var $6801=$st;
      var $6802=(($6801+2)|0);
      var $6803=$6802;
      var $6804=HEAP16[(($6803)>>1)];
      var $6805=(($6804)&65535);
      var $6806=(($6800+($6805<<2))|0);
      var $6807=$6806;
      var $6808=$6807;
      var $6809=HEAPF32[(($6808)>>2)];
      var $6810=$2;
      var $6811=(($6810+64)|0);
      var $6812=HEAP32[(($6811)>>2)];
      var $6813=$st;
      var $6814=(($6813+4)|0);
      var $6815=$6814;
      var $6816=HEAP16[(($6815)>>1)];
      var $6817=(($6816)&65535);
      var $6818=(($6812+($6817<<2))|0);
      var $6819=$6818;
      var $6820=$6819;
      var $6821=HEAPF32[(($6820)>>2)];
      var $6822=$6809 == $6821;
      var $6823=(($6822)&1);
      var $6824=(($6823)|0);
      var $6825=$2;
      var $6826=(($6825+64)|0);
      var $6827=HEAP32[(($6826)>>2)];
      var $6828=$st;
      var $6829=(($6828+6)|0);
      var $6830=$6829;
      var $6831=HEAP16[(($6830)>>1)];
      var $6832=(($6831)&65535);
      var $6833=(($6827+($6832<<2))|0);
      var $6834=$6833;
      var $6835=$6834;
      HEAPF32[(($6835)>>2)]=$6824;
      __label__ = 366; break;
    case 265: 
      var $6837=$2;
      var $6838=(($6837+64)|0);
      var $6839=HEAP32[(($6838)>>2)];
      var $6840=$st;
      var $6841=(($6840+2)|0);
      var $6842=$6841;
      var $6843=HEAP16[(($6842)>>1)];
      var $6844=(($6843)&65535);
      var $6845=(($6839+($6844<<2))|0);
      var $6846=$6845;
      var $6847=$6846;
      var $6848=(($6847)|0);
      var $6849=HEAPF32[(($6848)>>2)];
      var $6850=$2;
      var $6851=(($6850+64)|0);
      var $6852=HEAP32[(($6851)>>2)];
      var $6853=$st;
      var $6854=(($6853+4)|0);
      var $6855=$6854;
      var $6856=HEAP16[(($6855)>>1)];
      var $6857=(($6856)&65535);
      var $6858=(($6852+($6857<<2))|0);
      var $6859=$6858;
      var $6860=$6859;
      var $6861=(($6860)|0);
      var $6862=HEAPF32[(($6861)>>2)];
      var $6863=$6849 == $6862;
      if ($6863) { __label__ = 266; break; } else { var $6921 = 0;__label__ = 268; break; }
    case 266: 
      var $6865=$2;
      var $6866=(($6865+64)|0);
      var $6867=HEAP32[(($6866)>>2)];
      var $6868=$st;
      var $6869=(($6868+2)|0);
      var $6870=$6869;
      var $6871=HEAP16[(($6870)>>1)];
      var $6872=(($6871)&65535);
      var $6873=(($6867+($6872<<2))|0);
      var $6874=$6873;
      var $6875=$6874;
      var $6876=(($6875+4)|0);
      var $6877=HEAPF32[(($6876)>>2)];
      var $6878=$2;
      var $6879=(($6878+64)|0);
      var $6880=HEAP32[(($6879)>>2)];
      var $6881=$st;
      var $6882=(($6881+4)|0);
      var $6883=$6882;
      var $6884=HEAP16[(($6883)>>1)];
      var $6885=(($6884)&65535);
      var $6886=(($6880+($6885<<2))|0);
      var $6887=$6886;
      var $6888=$6887;
      var $6889=(($6888+4)|0);
      var $6890=HEAPF32[(($6889)>>2)];
      var $6891=$6877 == $6890;
      if ($6891) { __label__ = 267; break; } else { var $6921 = 0;__label__ = 268; break; }
    case 267: 
      var $6893=$2;
      var $6894=(($6893+64)|0);
      var $6895=HEAP32[(($6894)>>2)];
      var $6896=$st;
      var $6897=(($6896+2)|0);
      var $6898=$6897;
      var $6899=HEAP16[(($6898)>>1)];
      var $6900=(($6899)&65535);
      var $6901=(($6895+($6900<<2))|0);
      var $6902=$6901;
      var $6903=$6902;
      var $6904=(($6903+8)|0);
      var $6905=HEAPF32[(($6904)>>2)];
      var $6906=$2;
      var $6907=(($6906+64)|0);
      var $6908=HEAP32[(($6907)>>2)];
      var $6909=$st;
      var $6910=(($6909+4)|0);
      var $6911=$6910;
      var $6912=HEAP16[(($6911)>>1)];
      var $6913=(($6912)&65535);
      var $6914=(($6908+($6913<<2))|0);
      var $6915=$6914;
      var $6916=$6915;
      var $6917=(($6916+8)|0);
      var $6918=HEAPF32[(($6917)>>2)];
      var $6919=$6905 == $6918;
      var $6921 = $6919;__label__ = 268; break;
    case 268: 
      var $6921;
      var $6922=(($6921)&1);
      var $6923=(($6922)|0);
      var $6924=$2;
      var $6925=(($6924+64)|0);
      var $6926=HEAP32[(($6925)>>2)];
      var $6927=$st;
      var $6928=(($6927+6)|0);
      var $6929=$6928;
      var $6930=HEAP16[(($6929)>>1)];
      var $6931=(($6930)&65535);
      var $6932=(($6926+($6931<<2))|0);
      var $6933=$6932;
      var $6934=$6933;
      HEAPF32[(($6934)>>2)]=$6923;
      __label__ = 366; break;
    case 269: 
      var $6936=$2;
      var $6937=$2;
      var $6938=(($6937+64)|0);
      var $6939=HEAP32[(($6938)>>2)];
      var $6940=$st;
      var $6941=(($6940+2)|0);
      var $6942=$6941;
      var $6943=HEAP16[(($6942)>>1)];
      var $6944=(($6943)&65535);
      var $6945=(($6939+($6944<<2))|0);
      var $6946=$6945;
      var $6947=$6946;
      var $6948=HEAP32[(($6947)>>2)];
      var $6949=_prog_getstring($6936, $6948);
      var $6950=$2;
      var $6951=$2;
      var $6952=(($6951+64)|0);
      var $6953=HEAP32[(($6952)>>2)];
      var $6954=$st;
      var $6955=(($6954+4)|0);
      var $6956=$6955;
      var $6957=HEAP16[(($6956)>>1)];
      var $6958=(($6957)&65535);
      var $6959=(($6953+($6958<<2))|0);
      var $6960=$6959;
      var $6961=$6960;
      var $6962=HEAP32[(($6961)>>2)];
      var $6963=_prog_getstring($6950, $6962);
      var $6964=_strcmp($6949, $6963);
      var $6965=(($6964)|0)!=0;
      var $6966=$6965 ^ 1;
      var $6967=(($6966)&1);
      var $6968=(($6967)|0);
      var $6969=$2;
      var $6970=(($6969+64)|0);
      var $6971=HEAP32[(($6970)>>2)];
      var $6972=$st;
      var $6973=(($6972+6)|0);
      var $6974=$6973;
      var $6975=HEAP16[(($6974)>>1)];
      var $6976=(($6975)&65535);
      var $6977=(($6971+($6976<<2))|0);
      var $6978=$6977;
      var $6979=$6978;
      HEAPF32[(($6979)>>2)]=$6968;
      __label__ = 366; break;
    case 270: 
      var $6981=$2;
      var $6982=(($6981+64)|0);
      var $6983=HEAP32[(($6982)>>2)];
      var $6984=$st;
      var $6985=(($6984+2)|0);
      var $6986=$6985;
      var $6987=HEAP16[(($6986)>>1)];
      var $6988=(($6987)&65535);
      var $6989=(($6983+($6988<<2))|0);
      var $6990=$6989;
      var $6991=$6990;
      var $6992=HEAP32[(($6991)>>2)];
      var $6993=$2;
      var $6994=(($6993+64)|0);
      var $6995=HEAP32[(($6994)>>2)];
      var $6996=$st;
      var $6997=(($6996+4)|0);
      var $6998=$6997;
      var $6999=HEAP16[(($6998)>>1)];
      var $7000=(($6999)&65535);
      var $7001=(($6995+($7000<<2))|0);
      var $7002=$7001;
      var $7003=$7002;
      var $7004=HEAP32[(($7003)>>2)];
      var $7005=(($6992)|0)==(($7004)|0);
      var $7006=(($7005)&1);
      var $7007=(($7006)|0);
      var $7008=$2;
      var $7009=(($7008+64)|0);
      var $7010=HEAP32[(($7009)>>2)];
      var $7011=$st;
      var $7012=(($7011+6)|0);
      var $7013=$7012;
      var $7014=HEAP16[(($7013)>>1)];
      var $7015=(($7014)&65535);
      var $7016=(($7010+($7015<<2))|0);
      var $7017=$7016;
      var $7018=$7017;
      HEAPF32[(($7018)>>2)]=$7007;
      __label__ = 366; break;
    case 271: 
      var $7020=$2;
      var $7021=(($7020+64)|0);
      var $7022=HEAP32[(($7021)>>2)];
      var $7023=$st;
      var $7024=(($7023+2)|0);
      var $7025=$7024;
      var $7026=HEAP16[(($7025)>>1)];
      var $7027=(($7026)&65535);
      var $7028=(($7022+($7027<<2))|0);
      var $7029=$7028;
      var $7030=$7029;
      var $7031=HEAP32[(($7030)>>2)];
      var $7032=$2;
      var $7033=(($7032+64)|0);
      var $7034=HEAP32[(($7033)>>2)];
      var $7035=$st;
      var $7036=(($7035+4)|0);
      var $7037=$7036;
      var $7038=HEAP16[(($7037)>>1)];
      var $7039=(($7038)&65535);
      var $7040=(($7034+($7039<<2))|0);
      var $7041=$7040;
      var $7042=$7041;
      var $7043=HEAP32[(($7042)>>2)];
      var $7044=(($7031)|0)==(($7043)|0);
      var $7045=(($7044)&1);
      var $7046=(($7045)|0);
      var $7047=$2;
      var $7048=(($7047+64)|0);
      var $7049=HEAP32[(($7048)>>2)];
      var $7050=$st;
      var $7051=(($7050+6)|0);
      var $7052=$7051;
      var $7053=HEAP16[(($7052)>>1)];
      var $7054=(($7053)&65535);
      var $7055=(($7049+($7054<<2))|0);
      var $7056=$7055;
      var $7057=$7056;
      HEAPF32[(($7057)>>2)]=$7046;
      __label__ = 366; break;
    case 272: 
      var $7059=$2;
      var $7060=(($7059+64)|0);
      var $7061=HEAP32[(($7060)>>2)];
      var $7062=$st;
      var $7063=(($7062+2)|0);
      var $7064=$7063;
      var $7065=HEAP16[(($7064)>>1)];
      var $7066=(($7065)&65535);
      var $7067=(($7061+($7066<<2))|0);
      var $7068=$7067;
      var $7069=$7068;
      var $7070=HEAPF32[(($7069)>>2)];
      var $7071=$2;
      var $7072=(($7071+64)|0);
      var $7073=HEAP32[(($7072)>>2)];
      var $7074=$st;
      var $7075=(($7074+4)|0);
      var $7076=$7075;
      var $7077=HEAP16[(($7076)>>1)];
      var $7078=(($7077)&65535);
      var $7079=(($7073+($7078<<2))|0);
      var $7080=$7079;
      var $7081=$7080;
      var $7082=HEAPF32[(($7081)>>2)];
      var $7083=$7070 != $7082;
      var $7084=(($7083)&1);
      var $7085=(($7084)|0);
      var $7086=$2;
      var $7087=(($7086+64)|0);
      var $7088=HEAP32[(($7087)>>2)];
      var $7089=$st;
      var $7090=(($7089+6)|0);
      var $7091=$7090;
      var $7092=HEAP16[(($7091)>>1)];
      var $7093=(($7092)&65535);
      var $7094=(($7088+($7093<<2))|0);
      var $7095=$7094;
      var $7096=$7095;
      HEAPF32[(($7096)>>2)]=$7085;
      __label__ = 366; break;
    case 273: 
      var $7098=$2;
      var $7099=(($7098+64)|0);
      var $7100=HEAP32[(($7099)>>2)];
      var $7101=$st;
      var $7102=(($7101+2)|0);
      var $7103=$7102;
      var $7104=HEAP16[(($7103)>>1)];
      var $7105=(($7104)&65535);
      var $7106=(($7100+($7105<<2))|0);
      var $7107=$7106;
      var $7108=$7107;
      var $7109=(($7108)|0);
      var $7110=HEAPF32[(($7109)>>2)];
      var $7111=$2;
      var $7112=(($7111+64)|0);
      var $7113=HEAP32[(($7112)>>2)];
      var $7114=$st;
      var $7115=(($7114+4)|0);
      var $7116=$7115;
      var $7117=HEAP16[(($7116)>>1)];
      var $7118=(($7117)&65535);
      var $7119=(($7113+($7118<<2))|0);
      var $7120=$7119;
      var $7121=$7120;
      var $7122=(($7121)|0);
      var $7123=HEAPF32[(($7122)>>2)];
      var $7124=$7110 != $7123;
      if ($7124) { var $7182 = 1;__label__ = 276; break; } else { __label__ = 274; break; }
    case 274: 
      var $7126=$2;
      var $7127=(($7126+64)|0);
      var $7128=HEAP32[(($7127)>>2)];
      var $7129=$st;
      var $7130=(($7129+2)|0);
      var $7131=$7130;
      var $7132=HEAP16[(($7131)>>1)];
      var $7133=(($7132)&65535);
      var $7134=(($7128+($7133<<2))|0);
      var $7135=$7134;
      var $7136=$7135;
      var $7137=(($7136+4)|0);
      var $7138=HEAPF32[(($7137)>>2)];
      var $7139=$2;
      var $7140=(($7139+64)|0);
      var $7141=HEAP32[(($7140)>>2)];
      var $7142=$st;
      var $7143=(($7142+4)|0);
      var $7144=$7143;
      var $7145=HEAP16[(($7144)>>1)];
      var $7146=(($7145)&65535);
      var $7147=(($7141+($7146<<2))|0);
      var $7148=$7147;
      var $7149=$7148;
      var $7150=(($7149+4)|0);
      var $7151=HEAPF32[(($7150)>>2)];
      var $7152=$7138 != $7151;
      if ($7152) { var $7182 = 1;__label__ = 276; break; } else { __label__ = 275; break; }
    case 275: 
      var $7154=$2;
      var $7155=(($7154+64)|0);
      var $7156=HEAP32[(($7155)>>2)];
      var $7157=$st;
      var $7158=(($7157+2)|0);
      var $7159=$7158;
      var $7160=HEAP16[(($7159)>>1)];
      var $7161=(($7160)&65535);
      var $7162=(($7156+($7161<<2))|0);
      var $7163=$7162;
      var $7164=$7163;
      var $7165=(($7164+8)|0);
      var $7166=HEAPF32[(($7165)>>2)];
      var $7167=$2;
      var $7168=(($7167+64)|0);
      var $7169=HEAP32[(($7168)>>2)];
      var $7170=$st;
      var $7171=(($7170+4)|0);
      var $7172=$7171;
      var $7173=HEAP16[(($7172)>>1)];
      var $7174=(($7173)&65535);
      var $7175=(($7169+($7174<<2))|0);
      var $7176=$7175;
      var $7177=$7176;
      var $7178=(($7177+8)|0);
      var $7179=HEAPF32[(($7178)>>2)];
      var $7180=$7166 != $7179;
      var $7182 = $7180;__label__ = 276; break;
    case 276: 
      var $7182;
      var $7183=(($7182)&1);
      var $7184=(($7183)|0);
      var $7185=$2;
      var $7186=(($7185+64)|0);
      var $7187=HEAP32[(($7186)>>2)];
      var $7188=$st;
      var $7189=(($7188+6)|0);
      var $7190=$7189;
      var $7191=HEAP16[(($7190)>>1)];
      var $7192=(($7191)&65535);
      var $7193=(($7187+($7192<<2))|0);
      var $7194=$7193;
      var $7195=$7194;
      HEAPF32[(($7195)>>2)]=$7184;
      __label__ = 366; break;
    case 277: 
      var $7197=$2;
      var $7198=$2;
      var $7199=(($7198+64)|0);
      var $7200=HEAP32[(($7199)>>2)];
      var $7201=$st;
      var $7202=(($7201+2)|0);
      var $7203=$7202;
      var $7204=HEAP16[(($7203)>>1)];
      var $7205=(($7204)&65535);
      var $7206=(($7200+($7205<<2))|0);
      var $7207=$7206;
      var $7208=$7207;
      var $7209=HEAP32[(($7208)>>2)];
      var $7210=_prog_getstring($7197, $7209);
      var $7211=$2;
      var $7212=$2;
      var $7213=(($7212+64)|0);
      var $7214=HEAP32[(($7213)>>2)];
      var $7215=$st;
      var $7216=(($7215+4)|0);
      var $7217=$7216;
      var $7218=HEAP16[(($7217)>>1)];
      var $7219=(($7218)&65535);
      var $7220=(($7214+($7219<<2))|0);
      var $7221=$7220;
      var $7222=$7221;
      var $7223=HEAP32[(($7222)>>2)];
      var $7224=_prog_getstring($7211, $7223);
      var $7225=_strcmp($7210, $7224);
      var $7226=(($7225)|0)!=0;
      var $7227=$7226 ^ 1;
      var $7228=$7227 ^ 1;
      var $7229=(($7228)&1);
      var $7230=(($7229)|0);
      var $7231=$2;
      var $7232=(($7231+64)|0);
      var $7233=HEAP32[(($7232)>>2)];
      var $7234=$st;
      var $7235=(($7234+6)|0);
      var $7236=$7235;
      var $7237=HEAP16[(($7236)>>1)];
      var $7238=(($7237)&65535);
      var $7239=(($7233+($7238<<2))|0);
      var $7240=$7239;
      var $7241=$7240;
      HEAPF32[(($7241)>>2)]=$7230;
      __label__ = 366; break;
    case 278: 
      var $7243=$2;
      var $7244=(($7243+64)|0);
      var $7245=HEAP32[(($7244)>>2)];
      var $7246=$st;
      var $7247=(($7246+2)|0);
      var $7248=$7247;
      var $7249=HEAP16[(($7248)>>1)];
      var $7250=(($7249)&65535);
      var $7251=(($7245+($7250<<2))|0);
      var $7252=$7251;
      var $7253=$7252;
      var $7254=HEAP32[(($7253)>>2)];
      var $7255=$2;
      var $7256=(($7255+64)|0);
      var $7257=HEAP32[(($7256)>>2)];
      var $7258=$st;
      var $7259=(($7258+4)|0);
      var $7260=$7259;
      var $7261=HEAP16[(($7260)>>1)];
      var $7262=(($7261)&65535);
      var $7263=(($7257+($7262<<2))|0);
      var $7264=$7263;
      var $7265=$7264;
      var $7266=HEAP32[(($7265)>>2)];
      var $7267=(($7254)|0)!=(($7266)|0);
      var $7268=(($7267)&1);
      var $7269=(($7268)|0);
      var $7270=$2;
      var $7271=(($7270+64)|0);
      var $7272=HEAP32[(($7271)>>2)];
      var $7273=$st;
      var $7274=(($7273+6)|0);
      var $7275=$7274;
      var $7276=HEAP16[(($7275)>>1)];
      var $7277=(($7276)&65535);
      var $7278=(($7272+($7277<<2))|0);
      var $7279=$7278;
      var $7280=$7279;
      HEAPF32[(($7280)>>2)]=$7269;
      __label__ = 366; break;
    case 279: 
      var $7282=$2;
      var $7283=(($7282+64)|0);
      var $7284=HEAP32[(($7283)>>2)];
      var $7285=$st;
      var $7286=(($7285+2)|0);
      var $7287=$7286;
      var $7288=HEAP16[(($7287)>>1)];
      var $7289=(($7288)&65535);
      var $7290=(($7284+($7289<<2))|0);
      var $7291=$7290;
      var $7292=$7291;
      var $7293=HEAP32[(($7292)>>2)];
      var $7294=$2;
      var $7295=(($7294+64)|0);
      var $7296=HEAP32[(($7295)>>2)];
      var $7297=$st;
      var $7298=(($7297+4)|0);
      var $7299=$7298;
      var $7300=HEAP16[(($7299)>>1)];
      var $7301=(($7300)&65535);
      var $7302=(($7296+($7301<<2))|0);
      var $7303=$7302;
      var $7304=$7303;
      var $7305=HEAP32[(($7304)>>2)];
      var $7306=(($7293)|0)!=(($7305)|0);
      var $7307=(($7306)&1);
      var $7308=(($7307)|0);
      var $7309=$2;
      var $7310=(($7309+64)|0);
      var $7311=HEAP32[(($7310)>>2)];
      var $7312=$st;
      var $7313=(($7312+6)|0);
      var $7314=$7313;
      var $7315=HEAP16[(($7314)>>1)];
      var $7316=(($7315)&65535);
      var $7317=(($7311+($7316<<2))|0);
      var $7318=$7317;
      var $7319=$7318;
      HEAPF32[(($7319)>>2)]=$7308;
      __label__ = 366; break;
    case 280: 
      var $7321=$2;
      var $7322=(($7321+64)|0);
      var $7323=HEAP32[(($7322)>>2)];
      var $7324=$st;
      var $7325=(($7324+2)|0);
      var $7326=$7325;
      var $7327=HEAP16[(($7326)>>1)];
      var $7328=(($7327)&65535);
      var $7329=(($7323+($7328<<2))|0);
      var $7330=$7329;
      var $7331=$7330;
      var $7332=HEAPF32[(($7331)>>2)];
      var $7333=$2;
      var $7334=(($7333+64)|0);
      var $7335=HEAP32[(($7334)>>2)];
      var $7336=$st;
      var $7337=(($7336+4)|0);
      var $7338=$7337;
      var $7339=HEAP16[(($7338)>>1)];
      var $7340=(($7339)&65535);
      var $7341=(($7335+($7340<<2))|0);
      var $7342=$7341;
      var $7343=$7342;
      var $7344=HEAPF32[(($7343)>>2)];
      var $7345=$7332 <= $7344;
      var $7346=(($7345)&1);
      var $7347=(($7346)|0);
      var $7348=$2;
      var $7349=(($7348+64)|0);
      var $7350=HEAP32[(($7349)>>2)];
      var $7351=$st;
      var $7352=(($7351+6)|0);
      var $7353=$7352;
      var $7354=HEAP16[(($7353)>>1)];
      var $7355=(($7354)&65535);
      var $7356=(($7350+($7355<<2))|0);
      var $7357=$7356;
      var $7358=$7357;
      HEAPF32[(($7358)>>2)]=$7347;
      __label__ = 366; break;
    case 281: 
      var $7360=$2;
      var $7361=(($7360+64)|0);
      var $7362=HEAP32[(($7361)>>2)];
      var $7363=$st;
      var $7364=(($7363+2)|0);
      var $7365=$7364;
      var $7366=HEAP16[(($7365)>>1)];
      var $7367=(($7366)&65535);
      var $7368=(($7362+($7367<<2))|0);
      var $7369=$7368;
      var $7370=$7369;
      var $7371=HEAPF32[(($7370)>>2)];
      var $7372=$2;
      var $7373=(($7372+64)|0);
      var $7374=HEAP32[(($7373)>>2)];
      var $7375=$st;
      var $7376=(($7375+4)|0);
      var $7377=$7376;
      var $7378=HEAP16[(($7377)>>1)];
      var $7379=(($7378)&65535);
      var $7380=(($7374+($7379<<2))|0);
      var $7381=$7380;
      var $7382=$7381;
      var $7383=HEAPF32[(($7382)>>2)];
      var $7384=$7371 >= $7383;
      var $7385=(($7384)&1);
      var $7386=(($7385)|0);
      var $7387=$2;
      var $7388=(($7387+64)|0);
      var $7389=HEAP32[(($7388)>>2)];
      var $7390=$st;
      var $7391=(($7390+6)|0);
      var $7392=$7391;
      var $7393=HEAP16[(($7392)>>1)];
      var $7394=(($7393)&65535);
      var $7395=(($7389+($7394<<2))|0);
      var $7396=$7395;
      var $7397=$7396;
      HEAPF32[(($7397)>>2)]=$7386;
      __label__ = 366; break;
    case 282: 
      var $7399=$2;
      var $7400=(($7399+64)|0);
      var $7401=HEAP32[(($7400)>>2)];
      var $7402=$st;
      var $7403=(($7402+2)|0);
      var $7404=$7403;
      var $7405=HEAP16[(($7404)>>1)];
      var $7406=(($7405)&65535);
      var $7407=(($7401+($7406<<2))|0);
      var $7408=$7407;
      var $7409=$7408;
      var $7410=HEAPF32[(($7409)>>2)];
      var $7411=$2;
      var $7412=(($7411+64)|0);
      var $7413=HEAP32[(($7412)>>2)];
      var $7414=$st;
      var $7415=(($7414+4)|0);
      var $7416=$7415;
      var $7417=HEAP16[(($7416)>>1)];
      var $7418=(($7417)&65535);
      var $7419=(($7413+($7418<<2))|0);
      var $7420=$7419;
      var $7421=$7420;
      var $7422=HEAPF32[(($7421)>>2)];
      var $7423=$7410 < $7422;
      var $7424=(($7423)&1);
      var $7425=(($7424)|0);
      var $7426=$2;
      var $7427=(($7426+64)|0);
      var $7428=HEAP32[(($7427)>>2)];
      var $7429=$st;
      var $7430=(($7429+6)|0);
      var $7431=$7430;
      var $7432=HEAP16[(($7431)>>1)];
      var $7433=(($7432)&65535);
      var $7434=(($7428+($7433<<2))|0);
      var $7435=$7434;
      var $7436=$7435;
      HEAPF32[(($7436)>>2)]=$7425;
      __label__ = 366; break;
    case 283: 
      var $7438=$2;
      var $7439=(($7438+64)|0);
      var $7440=HEAP32[(($7439)>>2)];
      var $7441=$st;
      var $7442=(($7441+2)|0);
      var $7443=$7442;
      var $7444=HEAP16[(($7443)>>1)];
      var $7445=(($7444)&65535);
      var $7446=(($7440+($7445<<2))|0);
      var $7447=$7446;
      var $7448=$7447;
      var $7449=HEAPF32[(($7448)>>2)];
      var $7450=$2;
      var $7451=(($7450+64)|0);
      var $7452=HEAP32[(($7451)>>2)];
      var $7453=$st;
      var $7454=(($7453+4)|0);
      var $7455=$7454;
      var $7456=HEAP16[(($7455)>>1)];
      var $7457=(($7456)&65535);
      var $7458=(($7452+($7457<<2))|0);
      var $7459=$7458;
      var $7460=$7459;
      var $7461=HEAPF32[(($7460)>>2)];
      var $7462=$7449 > $7461;
      var $7463=(($7462)&1);
      var $7464=(($7463)|0);
      var $7465=$2;
      var $7466=(($7465+64)|0);
      var $7467=HEAP32[(($7466)>>2)];
      var $7468=$st;
      var $7469=(($7468+6)|0);
      var $7470=$7469;
      var $7471=HEAP16[(($7470)>>1)];
      var $7472=(($7471)&65535);
      var $7473=(($7467+($7472<<2))|0);
      var $7474=$7473;
      var $7475=$7474;
      HEAPF32[(($7475)>>2)]=$7464;
      __label__ = 366; break;
    case 284: 
      var $7477=$2;
      var $7478=(($7477+64)|0);
      var $7479=HEAP32[(($7478)>>2)];
      var $7480=$st;
      var $7481=(($7480+2)|0);
      var $7482=$7481;
      var $7483=HEAP16[(($7482)>>1)];
      var $7484=(($7483)&65535);
      var $7485=(($7479+($7484<<2))|0);
      var $7486=$7485;
      var $7487=$7486;
      var $7488=HEAP32[(($7487)>>2)];
      var $7489=(($7488)|0) < 0;
      if ($7489) { __label__ = 286; break; } else { __label__ = 285; break; }
    case 285: 
      var $7491=$2;
      var $7492=(($7491+64)|0);
      var $7493=HEAP32[(($7492)>>2)];
      var $7494=$st;
      var $7495=(($7494+2)|0);
      var $7496=$7495;
      var $7497=HEAP16[(($7496)>>1)];
      var $7498=(($7497)&65535);
      var $7499=(($7493+($7498<<2))|0);
      var $7500=$7499;
      var $7501=$7500;
      var $7502=HEAP32[(($7501)>>2)];
      var $7503=$2;
      var $7504=(($7503+140)|0);
      var $7505=HEAP32[(($7504)>>2)];
      var $7506=(($7502)|0) >= (($7505)|0);
      if ($7506) { __label__ = 286; break; } else { __label__ = 287; break; }
    case 286: 
      var $7508=$2;
      var $7509=$2;
      var $7510=(($7509)|0);
      var $7511=HEAP32[(($7510)>>2)];
      _qcvmerror($7508, ((STRING_TABLE.__str17)|0), (tempInt=STACKTOP,STACKTOP += 4,assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),HEAP32[((tempInt)>>2)]=$7511,tempInt));
      __label__ = 488; break;
    case 287: 
      var $7513=$2;
      var $7514=(($7513+64)|0);
      var $7515=HEAP32[(($7514)>>2)];
      var $7516=$st;
      var $7517=(($7516+4)|0);
      var $7518=$7517;
      var $7519=HEAP16[(($7518)>>1)];
      var $7520=(($7519)&65535);
      var $7521=(($7515+($7520<<2))|0);
      var $7522=$7521;
      var $7523=$7522;
      var $7524=HEAP32[(($7523)>>2)];
      var $7525=$2;
      var $7526=(($7525+144)|0);
      var $7527=HEAP32[(($7526)>>2)];
      var $7528=(($7524)>>>0) >= (($7527)>>>0);
      if ($7528) { __label__ = 288; break; } else { __label__ = 289; break; }
    case 288: 
      var $7530=$2;
      var $7531=$2;
      var $7532=(($7531)|0);
      var $7533=HEAP32[(($7532)>>2)];
      var $7534=$2;
      var $7535=(($7534+64)|0);
      var $7536=HEAP32[(($7535)>>2)];
      var $7537=$st;
      var $7538=(($7537+4)|0);
      var $7539=$7538;
      var $7540=HEAP16[(($7539)>>1)];
      var $7541=(($7540)&65535);
      var $7542=(($7536+($7541<<2))|0);
      var $7543=$7542;
      var $7544=$7543;
      var $7545=HEAP32[(($7544)>>2)];
      _qcvmerror($7530, ((STRING_TABLE.__str18)|0), (tempInt=STACKTOP,STACKTOP += 8,assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),HEAP32[((tempInt)>>2)]=$7533,HEAP32[(((tempInt)+(4))>>2)]=$7545,tempInt));
      __label__ = 488; break;
    case 289: 
      var $7547=$2;
      var $7548=$2;
      var $7549=(($7548+64)|0);
      var $7550=HEAP32[(($7549)>>2)];
      var $7551=$st;
      var $7552=(($7551+2)|0);
      var $7553=$7552;
      var $7554=HEAP16[(($7553)>>1)];
      var $7555=(($7554)&65535);
      var $7556=(($7550+($7555<<2))|0);
      var $7557=$7556;
      var $7558=$7557;
      var $7559=HEAP32[(($7558)>>2)];
      var $7560=_prog_getedict($7547, $7559);
      $ed6=$7560;
      var $7561=$ed6;
      var $7562=$7561;
      var $7563=$2;
      var $7564=(($7563+64)|0);
      var $7565=HEAP32[(($7564)>>2)];
      var $7566=$st;
      var $7567=(($7566+4)|0);
      var $7568=$7567;
      var $7569=HEAP16[(($7568)>>1)];
      var $7570=(($7569)&65535);
      var $7571=(($7565+($7570<<2))|0);
      var $7572=$7571;
      var $7573=$7572;
      var $7574=HEAP32[(($7573)>>2)];
      var $7575=(($7562+($7574<<2))|0);
      var $7576=$7575;
      var $7577=$7576;
      var $7578=HEAP32[(($7577)>>2)];
      var $7579=$2;
      var $7580=(($7579+64)|0);
      var $7581=HEAP32[(($7580)>>2)];
      var $7582=$st;
      var $7583=(($7582+6)|0);
      var $7584=$7583;
      var $7585=HEAP16[(($7584)>>1)];
      var $7586=(($7585)&65535);
      var $7587=(($7581+($7586<<2))|0);
      var $7588=$7587;
      var $7589=$7588;
      HEAP32[(($7589)>>2)]=$7578;
      __label__ = 366; break;
    case 290: 
      var $7591=$2;
      var $7592=(($7591+64)|0);
      var $7593=HEAP32[(($7592)>>2)];
      var $7594=$st;
      var $7595=(($7594+2)|0);
      var $7596=$7595;
      var $7597=HEAP16[(($7596)>>1)];
      var $7598=(($7597)&65535);
      var $7599=(($7593+($7598<<2))|0);
      var $7600=$7599;
      var $7601=$7600;
      var $7602=HEAP32[(($7601)>>2)];
      var $7603=(($7602)|0) < 0;
      if ($7603) { __label__ = 292; break; } else { __label__ = 291; break; }
    case 291: 
      var $7605=$2;
      var $7606=(($7605+64)|0);
      var $7607=HEAP32[(($7606)>>2)];
      var $7608=$st;
      var $7609=(($7608+2)|0);
      var $7610=$7609;
      var $7611=HEAP16[(($7610)>>1)];
      var $7612=(($7611)&65535);
      var $7613=(($7607+($7612<<2))|0);
      var $7614=$7613;
      var $7615=$7614;
      var $7616=HEAP32[(($7615)>>2)];
      var $7617=$2;
      var $7618=(($7617+140)|0);
      var $7619=HEAP32[(($7618)>>2)];
      var $7620=(($7616)|0) >= (($7619)|0);
      if ($7620) { __label__ = 292; break; } else { __label__ = 293; break; }
    case 292: 
      var $7622=$2;
      var $7623=$2;
      var $7624=(($7623)|0);
      var $7625=HEAP32[(($7624)>>2)];
      _qcvmerror($7622, ((STRING_TABLE.__str17)|0), (tempInt=STACKTOP,STACKTOP += 4,assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),HEAP32[((tempInt)>>2)]=$7625,tempInt));
      __label__ = 488; break;
    case 293: 
      var $7627=$2;
      var $7628=(($7627+64)|0);
      var $7629=HEAP32[(($7628)>>2)];
      var $7630=$st;
      var $7631=(($7630+4)|0);
      var $7632=$7631;
      var $7633=HEAP16[(($7632)>>1)];
      var $7634=(($7633)&65535);
      var $7635=(($7629+($7634<<2))|0);
      var $7636=$7635;
      var $7637=$7636;
      var $7638=HEAP32[(($7637)>>2)];
      var $7639=(($7638)|0) < 0;
      if ($7639) { __label__ = 295; break; } else { __label__ = 294; break; }
    case 294: 
      var $7641=$2;
      var $7642=(($7641+64)|0);
      var $7643=HEAP32[(($7642)>>2)];
      var $7644=$st;
      var $7645=(($7644+4)|0);
      var $7646=$7645;
      var $7647=HEAP16[(($7646)>>1)];
      var $7648=(($7647)&65535);
      var $7649=(($7643+($7648<<2))|0);
      var $7650=$7649;
      var $7651=$7650;
      var $7652=HEAP32[(($7651)>>2)];
      var $7653=((($7652)+(3))|0);
      var $7654=$2;
      var $7655=(($7654+144)|0);
      var $7656=HEAP32[(($7655)>>2)];
      var $7657=(($7653)>>>0) > (($7656)>>>0);
      if ($7657) { __label__ = 295; break; } else { __label__ = 296; break; }
    case 295: 
      var $7659=$2;
      var $7660=$2;
      var $7661=(($7660)|0);
      var $7662=HEAP32[(($7661)>>2)];
      var $7663=$2;
      var $7664=(($7663+64)|0);
      var $7665=HEAP32[(($7664)>>2)];
      var $7666=$st;
      var $7667=(($7666+4)|0);
      var $7668=$7667;
      var $7669=HEAP16[(($7668)>>1)];
      var $7670=(($7669)&65535);
      var $7671=(($7665+($7670<<2))|0);
      var $7672=$7671;
      var $7673=$7672;
      var $7674=HEAP32[(($7673)>>2)];
      var $7675=((($7674)+(2))|0);
      _qcvmerror($7659, ((STRING_TABLE.__str18)|0), (tempInt=STACKTOP,STACKTOP += 8,assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),HEAP32[((tempInt)>>2)]=$7662,HEAP32[(((tempInt)+(4))>>2)]=$7675,tempInt));
      __label__ = 488; break;
    case 296: 
      var $7677=$2;
      var $7678=$2;
      var $7679=(($7678+64)|0);
      var $7680=HEAP32[(($7679)>>2)];
      var $7681=$st;
      var $7682=(($7681+2)|0);
      var $7683=$7682;
      var $7684=HEAP16[(($7683)>>1)];
      var $7685=(($7684)&65535);
      var $7686=(($7680+($7685<<2))|0);
      var $7687=$7686;
      var $7688=$7687;
      var $7689=HEAP32[(($7688)>>2)];
      var $7690=_prog_getedict($7677, $7689);
      $ed6=$7690;
      var $7691=$ed6;
      var $7692=$7691;
      var $7693=$2;
      var $7694=(($7693+64)|0);
      var $7695=HEAP32[(($7694)>>2)];
      var $7696=$st;
      var $7697=(($7696+4)|0);
      var $7698=$7697;
      var $7699=HEAP16[(($7698)>>1)];
      var $7700=(($7699)&65535);
      var $7701=(($7695+($7700<<2))|0);
      var $7702=$7701;
      var $7703=$7702;
      var $7704=HEAP32[(($7703)>>2)];
      var $7705=(($7692+($7704<<2))|0);
      var $7706=$7705;
      var $7707=$7706;
      var $7708=(($7707)|0);
      var $7709=HEAP32[(($7708)>>2)];
      var $7710=$2;
      var $7711=(($7710+64)|0);
      var $7712=HEAP32[(($7711)>>2)];
      var $7713=$st;
      var $7714=(($7713+6)|0);
      var $7715=$7714;
      var $7716=HEAP16[(($7715)>>1)];
      var $7717=(($7716)&65535);
      var $7718=(($7712+($7717<<2))|0);
      var $7719=$7718;
      var $7720=$7719;
      var $7721=(($7720)|0);
      HEAP32[(($7721)>>2)]=$7709;
      var $7722=$ed6;
      var $7723=$7722;
      var $7724=$2;
      var $7725=(($7724+64)|0);
      var $7726=HEAP32[(($7725)>>2)];
      var $7727=$st;
      var $7728=(($7727+4)|0);
      var $7729=$7728;
      var $7730=HEAP16[(($7729)>>1)];
      var $7731=(($7730)&65535);
      var $7732=(($7726+($7731<<2))|0);
      var $7733=$7732;
      var $7734=$7733;
      var $7735=HEAP32[(($7734)>>2)];
      var $7736=(($7723+($7735<<2))|0);
      var $7737=$7736;
      var $7738=$7737;
      var $7739=(($7738+4)|0);
      var $7740=HEAP32[(($7739)>>2)];
      var $7741=$2;
      var $7742=(($7741+64)|0);
      var $7743=HEAP32[(($7742)>>2)];
      var $7744=$st;
      var $7745=(($7744+6)|0);
      var $7746=$7745;
      var $7747=HEAP16[(($7746)>>1)];
      var $7748=(($7747)&65535);
      var $7749=(($7743+($7748<<2))|0);
      var $7750=$7749;
      var $7751=$7750;
      var $7752=(($7751+4)|0);
      HEAP32[(($7752)>>2)]=$7740;
      var $7753=$ed6;
      var $7754=$7753;
      var $7755=$2;
      var $7756=(($7755+64)|0);
      var $7757=HEAP32[(($7756)>>2)];
      var $7758=$st;
      var $7759=(($7758+4)|0);
      var $7760=$7759;
      var $7761=HEAP16[(($7760)>>1)];
      var $7762=(($7761)&65535);
      var $7763=(($7757+($7762<<2))|0);
      var $7764=$7763;
      var $7765=$7764;
      var $7766=HEAP32[(($7765)>>2)];
      var $7767=(($7754+($7766<<2))|0);
      var $7768=$7767;
      var $7769=$7768;
      var $7770=(($7769+8)|0);
      var $7771=HEAP32[(($7770)>>2)];
      var $7772=$2;
      var $7773=(($7772+64)|0);
      var $7774=HEAP32[(($7773)>>2)];
      var $7775=$st;
      var $7776=(($7775+6)|0);
      var $7777=$7776;
      var $7778=HEAP16[(($7777)>>1)];
      var $7779=(($7778)&65535);
      var $7780=(($7774+($7779<<2))|0);
      var $7781=$7780;
      var $7782=$7781;
      var $7783=(($7782+8)|0);
      HEAP32[(($7783)>>2)]=$7771;
      __label__ = 366; break;
    case 297: 
      var $7785=$2;
      var $7786=(($7785+64)|0);
      var $7787=HEAP32[(($7786)>>2)];
      var $7788=$st;
      var $7789=(($7788+2)|0);
      var $7790=$7789;
      var $7791=HEAP16[(($7790)>>1)];
      var $7792=(($7791)&65535);
      var $7793=(($7787+($7792<<2))|0);
      var $7794=$7793;
      var $7795=$7794;
      var $7796=HEAP32[(($7795)>>2)];
      var $7797=(($7796)|0) < 0;
      if ($7797) { __label__ = 299; break; } else { __label__ = 298; break; }
    case 298: 
      var $7799=$2;
      var $7800=(($7799+64)|0);
      var $7801=HEAP32[(($7800)>>2)];
      var $7802=$st;
      var $7803=(($7802+2)|0);
      var $7804=$7803;
      var $7805=HEAP16[(($7804)>>1)];
      var $7806=(($7805)&65535);
      var $7807=(($7801+($7806<<2))|0);
      var $7808=$7807;
      var $7809=$7808;
      var $7810=HEAP32[(($7809)>>2)];
      var $7811=$2;
      var $7812=(($7811+140)|0);
      var $7813=HEAP32[(($7812)>>2)];
      var $7814=(($7810)|0) >= (($7813)|0);
      if ($7814) { __label__ = 299; break; } else { __label__ = 300; break; }
    case 299: 
      var $7816=$2;
      var $7817=$2;
      var $7818=(($7817)|0);
      var $7819=HEAP32[(($7818)>>2)];
      var $7820=$2;
      var $7821=(($7820+64)|0);
      var $7822=HEAP32[(($7821)>>2)];
      var $7823=$st;
      var $7824=(($7823+2)|0);
      var $7825=$7824;
      var $7826=HEAP16[(($7825)>>1)];
      var $7827=(($7826)&65535);
      var $7828=(($7822+($7827<<2))|0);
      var $7829=$7828;
      var $7830=$7829;
      var $7831=HEAP32[(($7830)>>2)];
      _qcvmerror($7816, ((STRING_TABLE.__str19)|0), (tempInt=STACKTOP,STACKTOP += 8,assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),HEAP32[((tempInt)>>2)]=$7819,HEAP32[(((tempInt)+(4))>>2)]=$7831,tempInt));
      __label__ = 488; break;
    case 300: 
      var $7833=$2;
      var $7834=(($7833+64)|0);
      var $7835=HEAP32[(($7834)>>2)];
      var $7836=$st;
      var $7837=(($7836+4)|0);
      var $7838=$7837;
      var $7839=HEAP16[(($7838)>>1)];
      var $7840=(($7839)&65535);
      var $7841=(($7835+($7840<<2))|0);
      var $7842=$7841;
      var $7843=$7842;
      var $7844=HEAP32[(($7843)>>2)];
      var $7845=$2;
      var $7846=(($7845+144)|0);
      var $7847=HEAP32[(($7846)>>2)];
      var $7848=(($7844)>>>0) >= (($7847)>>>0);
      if ($7848) { __label__ = 301; break; } else { __label__ = 302; break; }
    case 301: 
      var $7850=$2;
      var $7851=$2;
      var $7852=(($7851)|0);
      var $7853=HEAP32[(($7852)>>2)];
      var $7854=$2;
      var $7855=(($7854+64)|0);
      var $7856=HEAP32[(($7855)>>2)];
      var $7857=$st;
      var $7858=(($7857+4)|0);
      var $7859=$7858;
      var $7860=HEAP16[(($7859)>>1)];
      var $7861=(($7860)&65535);
      var $7862=(($7856+($7861<<2))|0);
      var $7863=$7862;
      var $7864=$7863;
      var $7865=HEAP32[(($7864)>>2)];
      _qcvmerror($7850, ((STRING_TABLE.__str18)|0), (tempInt=STACKTOP,STACKTOP += 8,assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),HEAP32[((tempInt)>>2)]=$7853,HEAP32[(((tempInt)+(4))>>2)]=$7865,tempInt));
      __label__ = 488; break;
    case 302: 
      var $7867=$2;
      var $7868=$2;
      var $7869=(($7868+64)|0);
      var $7870=HEAP32[(($7869)>>2)];
      var $7871=$st;
      var $7872=(($7871+2)|0);
      var $7873=$7872;
      var $7874=HEAP16[(($7873)>>1)];
      var $7875=(($7874)&65535);
      var $7876=(($7870+($7875<<2))|0);
      var $7877=$7876;
      var $7878=$7877;
      var $7879=HEAP32[(($7878)>>2)];
      var $7880=_prog_getedict($7867, $7879);
      $ed6=$7880;
      var $7881=$ed6;
      var $7882=$7881;
      var $7883=$2;
      var $7884=(($7883+76)|0);
      var $7885=HEAP32[(($7884)>>2)];
      var $7886=$7882;
      var $7887=$7885;
      var $7888=((($7886)-($7887))|0);
      var $7889=((((($7888)|0))/(4))&-1);
      var $7890=$2;
      var $7891=(($7890+64)|0);
      var $7892=HEAP32[(($7891)>>2)];
      var $7893=$st;
      var $7894=(($7893+6)|0);
      var $7895=$7894;
      var $7896=HEAP16[(($7895)>>1)];
      var $7897=(($7896)&65535);
      var $7898=(($7892+($7897<<2))|0);
      var $7899=$7898;
      var $7900=$7899;
      HEAP32[(($7900)>>2)]=$7889;
      var $7901=$2;
      var $7902=(($7901+64)|0);
      var $7903=HEAP32[(($7902)>>2)];
      var $7904=$st;
      var $7905=(($7904+4)|0);
      var $7906=$7905;
      var $7907=HEAP16[(($7906)>>1)];
      var $7908=(($7907)&65535);
      var $7909=(($7903+($7908<<2))|0);
      var $7910=$7909;
      var $7911=$7910;
      var $7912=HEAP32[(($7911)>>2)];
      var $7913=$2;
      var $7914=(($7913+64)|0);
      var $7915=HEAP32[(($7914)>>2)];
      var $7916=$st;
      var $7917=(($7916+6)|0);
      var $7918=$7917;
      var $7919=HEAP16[(($7918)>>1)];
      var $7920=(($7919)&65535);
      var $7921=(($7915+($7920<<2))|0);
      var $7922=$7921;
      var $7923=$7922;
      var $7924=HEAP32[(($7923)>>2)];
      var $7925=((($7924)+($7912))|0);
      HEAP32[(($7923)>>2)]=$7925;
      __label__ = 366; break;
    case 303: 
      var $7927=$2;
      var $7928=(($7927+64)|0);
      var $7929=HEAP32[(($7928)>>2)];
      var $7930=$st;
      var $7931=(($7930+2)|0);
      var $7932=$7931;
      var $7933=HEAP16[(($7932)>>1)];
      var $7934=(($7933)&65535);
      var $7935=(($7929+($7934<<2))|0);
      var $7936=$7935;
      var $7937=$7936;
      var $7938=HEAP32[(($7937)>>2)];
      var $7939=$2;
      var $7940=(($7939+64)|0);
      var $7941=HEAP32[(($7940)>>2)];
      var $7942=$st;
      var $7943=(($7942+4)|0);
      var $7944=$7943;
      var $7945=HEAP16[(($7944)>>1)];
      var $7946=(($7945)&65535);
      var $7947=(($7941+($7946<<2))|0);
      var $7948=$7947;
      var $7949=$7948;
      HEAP32[(($7949)>>2)]=$7938;
      __label__ = 366; break;
    case 304: 
      var $7951=$2;
      var $7952=(($7951+64)|0);
      var $7953=HEAP32[(($7952)>>2)];
      var $7954=$st;
      var $7955=(($7954+2)|0);
      var $7956=$7955;
      var $7957=HEAP16[(($7956)>>1)];
      var $7958=(($7957)&65535);
      var $7959=(($7953+($7958<<2))|0);
      var $7960=$7959;
      var $7961=$7960;
      var $7962=(($7961)|0);
      var $7963=HEAP32[(($7962)>>2)];
      var $7964=$2;
      var $7965=(($7964+64)|0);
      var $7966=HEAP32[(($7965)>>2)];
      var $7967=$st;
      var $7968=(($7967+4)|0);
      var $7969=$7968;
      var $7970=HEAP16[(($7969)>>1)];
      var $7971=(($7970)&65535);
      var $7972=(($7966+($7971<<2))|0);
      var $7973=$7972;
      var $7974=$7973;
      var $7975=(($7974)|0);
      HEAP32[(($7975)>>2)]=$7963;
      var $7976=$2;
      var $7977=(($7976+64)|0);
      var $7978=HEAP32[(($7977)>>2)];
      var $7979=$st;
      var $7980=(($7979+2)|0);
      var $7981=$7980;
      var $7982=HEAP16[(($7981)>>1)];
      var $7983=(($7982)&65535);
      var $7984=(($7978+($7983<<2))|0);
      var $7985=$7984;
      var $7986=$7985;
      var $7987=(($7986+4)|0);
      var $7988=HEAP32[(($7987)>>2)];
      var $7989=$2;
      var $7990=(($7989+64)|0);
      var $7991=HEAP32[(($7990)>>2)];
      var $7992=$st;
      var $7993=(($7992+4)|0);
      var $7994=$7993;
      var $7995=HEAP16[(($7994)>>1)];
      var $7996=(($7995)&65535);
      var $7997=(($7991+($7996<<2))|0);
      var $7998=$7997;
      var $7999=$7998;
      var $8000=(($7999+4)|0);
      HEAP32[(($8000)>>2)]=$7988;
      var $8001=$2;
      var $8002=(($8001+64)|0);
      var $8003=HEAP32[(($8002)>>2)];
      var $8004=$st;
      var $8005=(($8004+2)|0);
      var $8006=$8005;
      var $8007=HEAP16[(($8006)>>1)];
      var $8008=(($8007)&65535);
      var $8009=(($8003+($8008<<2))|0);
      var $8010=$8009;
      var $8011=$8010;
      var $8012=(($8011+8)|0);
      var $8013=HEAP32[(($8012)>>2)];
      var $8014=$2;
      var $8015=(($8014+64)|0);
      var $8016=HEAP32[(($8015)>>2)];
      var $8017=$st;
      var $8018=(($8017+4)|0);
      var $8019=$8018;
      var $8020=HEAP16[(($8019)>>1)];
      var $8021=(($8020)&65535);
      var $8022=(($8016+($8021<<2))|0);
      var $8023=$8022;
      var $8024=$8023;
      var $8025=(($8024+8)|0);
      HEAP32[(($8025)>>2)]=$8013;
      __label__ = 366; break;
    case 305: 
      var $8027=$2;
      var $8028=(($8027+64)|0);
      var $8029=HEAP32[(($8028)>>2)];
      var $8030=$st;
      var $8031=(($8030+4)|0);
      var $8032=$8031;
      var $8033=HEAP16[(($8032)>>1)];
      var $8034=(($8033)&65535);
      var $8035=(($8029+($8034<<2))|0);
      var $8036=$8035;
      var $8037=$8036;
      var $8038=HEAP32[(($8037)>>2)];
      var $8039=(($8038)|0) < 0;
      if ($8039) { __label__ = 307; break; } else { __label__ = 306; break; }
    case 306: 
      var $8041=$2;
      var $8042=(($8041+64)|0);
      var $8043=HEAP32[(($8042)>>2)];
      var $8044=$st;
      var $8045=(($8044+4)|0);
      var $8046=$8045;
      var $8047=HEAP16[(($8046)>>1)];
      var $8048=(($8047)&65535);
      var $8049=(($8043+($8048<<2))|0);
      var $8050=$8049;
      var $8051=$8050;
      var $8052=HEAP32[(($8051)>>2)];
      var $8053=$2;
      var $8054=(($8053+80)|0);
      var $8055=HEAP32[(($8054)>>2)];
      var $8056=(($8052)>>>0) >= (($8055)>>>0);
      if ($8056) { __label__ = 307; break; } else { __label__ = 308; break; }
    case 307: 
      var $8058=$2;
      var $8059=$2;
      var $8060=(($8059)|0);
      var $8061=HEAP32[(($8060)>>2)];
      var $8062=$2;
      var $8063=(($8062+64)|0);
      var $8064=HEAP32[(($8063)>>2)];
      var $8065=$st;
      var $8066=(($8065+4)|0);
      var $8067=$8066;
      var $8068=HEAP16[(($8067)>>1)];
      var $8069=(($8068)&65535);
      var $8070=(($8064+($8069<<2))|0);
      var $8071=$8070;
      var $8072=$8071;
      var $8073=HEAP32[(($8072)>>2)];
      _qcvmerror($8058, ((STRING_TABLE.__str20)|0), (tempInt=STACKTOP,STACKTOP += 8,assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),HEAP32[((tempInt)>>2)]=$8061,HEAP32[(((tempInt)+(4))>>2)]=$8073,tempInt));
      __label__ = 488; break;
    case 308: 
      var $8075=$2;
      var $8076=(($8075+64)|0);
      var $8077=HEAP32[(($8076)>>2)];
      var $8078=$st;
      var $8079=(($8078+4)|0);
      var $8080=$8079;
      var $8081=HEAP16[(($8080)>>1)];
      var $8082=(($8081)&65535);
      var $8083=(($8077+($8082<<2))|0);
      var $8084=$8083;
      var $8085=$8084;
      var $8086=HEAP32[(($8085)>>2)];
      var $8087=$2;
      var $8088=(($8087+144)|0);
      var $8089=HEAP32[(($8088)>>2)];
      var $8090=(($8086)>>>0) < (($8089)>>>0);
      if ($8090) { __label__ = 309; break; } else { __label__ = 311; break; }
    case 309: 
      var $8092=$2;
      var $8093=(($8092+148)|0);
      var $8094=HEAP8[($8093)];
      var $8095=(($8094) & 1);
      if ($8095) { __label__ = 311; break; } else { __label__ = 310; break; }
    case 310: 
      var $8097=$2;
      var $8098=$2;
      var $8099=(($8098)|0);
      var $8100=HEAP32[(($8099)>>2)];
      var $8101=$2;
      var $8102=$2;
      var $8103=$2;
      var $8104=(($8103+64)|0);
      var $8105=HEAP32[(($8104)>>2)];
      var $8106=$st;
      var $8107=(($8106+4)|0);
      var $8108=$8107;
      var $8109=HEAP16[(($8108)>>1)];
      var $8110=(($8109)&65535);
      var $8111=(($8105+($8110<<2))|0);
      var $8112=$8111;
      var $8113=$8112;
      var $8114=HEAP32[(($8113)>>2)];
      var $8115=_prog_entfield($8102, $8114);
      var $8116=(($8115+4)|0);
      var $8117=HEAP32[(($8116)>>2)];
      var $8118=_prog_getstring($8101, $8117);
      var $8119=$2;
      var $8120=(($8119+64)|0);
      var $8121=HEAP32[(($8120)>>2)];
      var $8122=$st;
      var $8123=(($8122+4)|0);
      var $8124=$8123;
      var $8125=HEAP16[(($8124)>>1)];
      var $8126=(($8125)&65535);
      var $8127=(($8121+($8126<<2))|0);
      var $8128=$8127;
      var $8129=$8128;
      var $8130=HEAP32[(($8129)>>2)];
      _qcvmerror($8097, ((STRING_TABLE.__str21)|0), (tempInt=STACKTOP,STACKTOP += 12,assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),HEAP32[((tempInt)>>2)]=$8100,HEAP32[(((tempInt)+(4))>>2)]=$8118,HEAP32[(((tempInt)+(8))>>2)]=$8130,tempInt));
      __label__ = 311; break;
    case 311: 
      var $8132=$2;
      var $8133=(($8132+76)|0);
      var $8134=HEAP32[(($8133)>>2)];
      var $8135=$2;
      var $8136=(($8135+64)|0);
      var $8137=HEAP32[(($8136)>>2)];
      var $8138=$st;
      var $8139=(($8138+4)|0);
      var $8140=$8139;
      var $8141=HEAP16[(($8140)>>1)];
      var $8142=(($8141)&65535);
      var $8143=(($8137+($8142<<2))|0);
      var $8144=$8143;
      var $8145=$8144;
      var $8146=HEAP32[(($8145)>>2)];
      var $8147=(($8134+($8146<<2))|0);
      var $8148=$8147;
      $ptr7=$8148;
      var $8149=$2;
      var $8150=(($8149+64)|0);
      var $8151=HEAP32[(($8150)>>2)];
      var $8152=$st;
      var $8153=(($8152+2)|0);
      var $8154=$8153;
      var $8155=HEAP16[(($8154)>>1)];
      var $8156=(($8155)&65535);
      var $8157=(($8151+($8156<<2))|0);
      var $8158=$8157;
      var $8159=$8158;
      var $8160=HEAP32[(($8159)>>2)];
      var $8161=$ptr7;
      var $8162=$8161;
      HEAP32[(($8162)>>2)]=$8160;
      __label__ = 366; break;
    case 312: 
      var $8164=$2;
      var $8165=(($8164+64)|0);
      var $8166=HEAP32[(($8165)>>2)];
      var $8167=$st;
      var $8168=(($8167+4)|0);
      var $8169=$8168;
      var $8170=HEAP16[(($8169)>>1)];
      var $8171=(($8170)&65535);
      var $8172=(($8166+($8171<<2))|0);
      var $8173=$8172;
      var $8174=$8173;
      var $8175=HEAP32[(($8174)>>2)];
      var $8176=(($8175)|0) < 0;
      if ($8176) { __label__ = 314; break; } else { __label__ = 313; break; }
    case 313: 
      var $8178=$2;
      var $8179=(($8178+64)|0);
      var $8180=HEAP32[(($8179)>>2)];
      var $8181=$st;
      var $8182=(($8181+4)|0);
      var $8183=$8182;
      var $8184=HEAP16[(($8183)>>1)];
      var $8185=(($8184)&65535);
      var $8186=(($8180+($8185<<2))|0);
      var $8187=$8186;
      var $8188=$8187;
      var $8189=HEAP32[(($8188)>>2)];
      var $8190=((($8189)+(2))|0);
      var $8191=$2;
      var $8192=(($8191+80)|0);
      var $8193=HEAP32[(($8192)>>2)];
      var $8194=(($8190)>>>0) >= (($8193)>>>0);
      if ($8194) { __label__ = 314; break; } else { __label__ = 315; break; }
    case 314: 
      var $8196=$2;
      var $8197=$2;
      var $8198=(($8197)|0);
      var $8199=HEAP32[(($8198)>>2)];
      var $8200=$2;
      var $8201=(($8200+64)|0);
      var $8202=HEAP32[(($8201)>>2)];
      var $8203=$st;
      var $8204=(($8203+4)|0);
      var $8205=$8204;
      var $8206=HEAP16[(($8205)>>1)];
      var $8207=(($8206)&65535);
      var $8208=(($8202+($8207<<2))|0);
      var $8209=$8208;
      var $8210=$8209;
      var $8211=HEAP32[(($8210)>>2)];
      _qcvmerror($8196, ((STRING_TABLE.__str20)|0), (tempInt=STACKTOP,STACKTOP += 8,assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),HEAP32[((tempInt)>>2)]=$8199,HEAP32[(((tempInt)+(4))>>2)]=$8211,tempInt));
      __label__ = 488; break;
    case 315: 
      var $8213=$2;
      var $8214=(($8213+64)|0);
      var $8215=HEAP32[(($8214)>>2)];
      var $8216=$st;
      var $8217=(($8216+4)|0);
      var $8218=$8217;
      var $8219=HEAP16[(($8218)>>1)];
      var $8220=(($8219)&65535);
      var $8221=(($8215+($8220<<2))|0);
      var $8222=$8221;
      var $8223=$8222;
      var $8224=HEAP32[(($8223)>>2)];
      var $8225=$2;
      var $8226=(($8225+144)|0);
      var $8227=HEAP32[(($8226)>>2)];
      var $8228=(($8224)>>>0) < (($8227)>>>0);
      if ($8228) { __label__ = 316; break; } else { __label__ = 318; break; }
    case 316: 
      var $8230=$2;
      var $8231=(($8230+148)|0);
      var $8232=HEAP8[($8231)];
      var $8233=(($8232) & 1);
      if ($8233) { __label__ = 318; break; } else { __label__ = 317; break; }
    case 317: 
      var $8235=$2;
      var $8236=$2;
      var $8237=(($8236)|0);
      var $8238=HEAP32[(($8237)>>2)];
      var $8239=$2;
      var $8240=$2;
      var $8241=$2;
      var $8242=(($8241+64)|0);
      var $8243=HEAP32[(($8242)>>2)];
      var $8244=$st;
      var $8245=(($8244+4)|0);
      var $8246=$8245;
      var $8247=HEAP16[(($8246)>>1)];
      var $8248=(($8247)&65535);
      var $8249=(($8243+($8248<<2))|0);
      var $8250=$8249;
      var $8251=$8250;
      var $8252=HEAP32[(($8251)>>2)];
      var $8253=_prog_entfield($8240, $8252);
      var $8254=(($8253+4)|0);
      var $8255=HEAP32[(($8254)>>2)];
      var $8256=_prog_getstring($8239, $8255);
      var $8257=$2;
      var $8258=(($8257+64)|0);
      var $8259=HEAP32[(($8258)>>2)];
      var $8260=$st;
      var $8261=(($8260+4)|0);
      var $8262=$8261;
      var $8263=HEAP16[(($8262)>>1)];
      var $8264=(($8263)&65535);
      var $8265=(($8259+($8264<<2))|0);
      var $8266=$8265;
      var $8267=$8266;
      var $8268=HEAP32[(($8267)>>2)];
      _qcvmerror($8235, ((STRING_TABLE.__str21)|0), (tempInt=STACKTOP,STACKTOP += 12,assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),HEAP32[((tempInt)>>2)]=$8238,HEAP32[(((tempInt)+(4))>>2)]=$8256,HEAP32[(((tempInt)+(8))>>2)]=$8268,tempInt));
      __label__ = 318; break;
    case 318: 
      var $8270=$2;
      var $8271=(($8270+76)|0);
      var $8272=HEAP32[(($8271)>>2)];
      var $8273=$2;
      var $8274=(($8273+64)|0);
      var $8275=HEAP32[(($8274)>>2)];
      var $8276=$st;
      var $8277=(($8276+4)|0);
      var $8278=$8277;
      var $8279=HEAP16[(($8278)>>1)];
      var $8280=(($8279)&65535);
      var $8281=(($8275+($8280<<2))|0);
      var $8282=$8281;
      var $8283=$8282;
      var $8284=HEAP32[(($8283)>>2)];
      var $8285=(($8272+($8284<<2))|0);
      var $8286=$8285;
      $ptr7=$8286;
      var $8287=$2;
      var $8288=(($8287+64)|0);
      var $8289=HEAP32[(($8288)>>2)];
      var $8290=$st;
      var $8291=(($8290+2)|0);
      var $8292=$8291;
      var $8293=HEAP16[(($8292)>>1)];
      var $8294=(($8293)&65535);
      var $8295=(($8289+($8294<<2))|0);
      var $8296=$8295;
      var $8297=$8296;
      var $8298=(($8297)|0);
      var $8299=HEAP32[(($8298)>>2)];
      var $8300=$ptr7;
      var $8301=$8300;
      var $8302=(($8301)|0);
      HEAP32[(($8302)>>2)]=$8299;
      var $8303=$2;
      var $8304=(($8303+64)|0);
      var $8305=HEAP32[(($8304)>>2)];
      var $8306=$st;
      var $8307=(($8306+2)|0);
      var $8308=$8307;
      var $8309=HEAP16[(($8308)>>1)];
      var $8310=(($8309)&65535);
      var $8311=(($8305+($8310<<2))|0);
      var $8312=$8311;
      var $8313=$8312;
      var $8314=(($8313+4)|0);
      var $8315=HEAP32[(($8314)>>2)];
      var $8316=$ptr7;
      var $8317=$8316;
      var $8318=(($8317+4)|0);
      HEAP32[(($8318)>>2)]=$8315;
      var $8319=$2;
      var $8320=(($8319+64)|0);
      var $8321=HEAP32[(($8320)>>2)];
      var $8322=$st;
      var $8323=(($8322+2)|0);
      var $8324=$8323;
      var $8325=HEAP16[(($8324)>>1)];
      var $8326=(($8325)&65535);
      var $8327=(($8321+($8326<<2))|0);
      var $8328=$8327;
      var $8329=$8328;
      var $8330=(($8329+8)|0);
      var $8331=HEAP32[(($8330)>>2)];
      var $8332=$ptr7;
      var $8333=$8332;
      var $8334=(($8333+8)|0);
      HEAP32[(($8334)>>2)]=$8331;
      __label__ = 366; break;
    case 319: 
      var $8336=$2;
      var $8337=(($8336+64)|0);
      var $8338=HEAP32[(($8337)>>2)];
      var $8339=$st;
      var $8340=(($8339+2)|0);
      var $8341=$8340;
      var $8342=HEAP16[(($8341)>>1)];
      var $8343=(($8342)&65535);
      var $8344=(($8338+($8343<<2))|0);
      var $8345=$8344;
      var $8346=$8345;
      var $8347=HEAP32[(($8346)>>2)];
      var $8348=$8347 & 2147483647;
      var $8349=(($8348)|0)!=0;
      var $8350=$8349 ^ 1;
      var $8351=(($8350)&1);
      var $8352=(($8351)|0);
      var $8353=$2;
      var $8354=(($8353+64)|0);
      var $8355=HEAP32[(($8354)>>2)];
      var $8356=$st;
      var $8357=(($8356+6)|0);
      var $8358=$8357;
      var $8359=HEAP16[(($8358)>>1)];
      var $8360=(($8359)&65535);
      var $8361=(($8355+($8360<<2))|0);
      var $8362=$8361;
      var $8363=$8362;
      HEAPF32[(($8363)>>2)]=$8352;
      __label__ = 366; break;
    case 320: 
      var $8365=$2;
      var $8366=(($8365+64)|0);
      var $8367=HEAP32[(($8366)>>2)];
      var $8368=$st;
      var $8369=(($8368+2)|0);
      var $8370=$8369;
      var $8371=HEAP16[(($8370)>>1)];
      var $8372=(($8371)&65535);
      var $8373=(($8367+($8372<<2))|0);
      var $8374=$8373;
      var $8375=$8374;
      var $8376=(($8375)|0);
      var $8377=HEAPF32[(($8376)>>2)];
      var $8378=$8377 != 0;
      if ($8378) { var $8411 = 0;__label__ = 323; break; } else { __label__ = 321; break; }
    case 321: 
      var $8380=$2;
      var $8381=(($8380+64)|0);
      var $8382=HEAP32[(($8381)>>2)];
      var $8383=$st;
      var $8384=(($8383+2)|0);
      var $8385=$8384;
      var $8386=HEAP16[(($8385)>>1)];
      var $8387=(($8386)&65535);
      var $8388=(($8382+($8387<<2))|0);
      var $8389=$8388;
      var $8390=$8389;
      var $8391=(($8390+4)|0);
      var $8392=HEAPF32[(($8391)>>2)];
      var $8393=$8392 != 0;
      if ($8393) { var $8411 = 0;__label__ = 323; break; } else { __label__ = 322; break; }
    case 322: 
      var $8395=$2;
      var $8396=(($8395+64)|0);
      var $8397=HEAP32[(($8396)>>2)];
      var $8398=$st;
      var $8399=(($8398+2)|0);
      var $8400=$8399;
      var $8401=HEAP16[(($8400)>>1)];
      var $8402=(($8401)&65535);
      var $8403=(($8397+($8402<<2))|0);
      var $8404=$8403;
      var $8405=$8404;
      var $8406=(($8405+8)|0);
      var $8407=HEAPF32[(($8406)>>2)];
      var $8408=$8407 != 0;
      var $8409=$8408 ^ 1;
      var $8411 = $8409;__label__ = 323; break;
    case 323: 
      var $8411;
      var $8412=(($8411)&1);
      var $8413=(($8412)|0);
      var $8414=$2;
      var $8415=(($8414+64)|0);
      var $8416=HEAP32[(($8415)>>2)];
      var $8417=$st;
      var $8418=(($8417+6)|0);
      var $8419=$8418;
      var $8420=HEAP16[(($8419)>>1)];
      var $8421=(($8420)&65535);
      var $8422=(($8416+($8421<<2))|0);
      var $8423=$8422;
      var $8424=$8423;
      HEAPF32[(($8424)>>2)]=$8413;
      __label__ = 366; break;
    case 324: 
      var $8426=$2;
      var $8427=(($8426+64)|0);
      var $8428=HEAP32[(($8427)>>2)];
      var $8429=$st;
      var $8430=(($8429+2)|0);
      var $8431=$8430;
      var $8432=HEAP16[(($8431)>>1)];
      var $8433=(($8432)&65535);
      var $8434=(($8428+($8433<<2))|0);
      var $8435=$8434;
      var $8436=$8435;
      var $8437=HEAP32[(($8436)>>2)];
      var $8438=(($8437)|0)!=0;
      if ($8438) { __label__ = 325; break; } else { var $8458 = 1;__label__ = 326; break; }
    case 325: 
      var $8440=$2;
      var $8441=$2;
      var $8442=(($8441+64)|0);
      var $8443=HEAP32[(($8442)>>2)];
      var $8444=$st;
      var $8445=(($8444+2)|0);
      var $8446=$8445;
      var $8447=HEAP16[(($8446)>>1)];
      var $8448=(($8447)&65535);
      var $8449=(($8443+($8448<<2))|0);
      var $8450=$8449;
      var $8451=$8450;
      var $8452=HEAP32[(($8451)>>2)];
      var $8453=_prog_getstring($8440, $8452);
      var $8454=HEAP8[($8453)];
      var $8455=(($8454 << 24) >> 24)!=0;
      var $8456=$8455 ^ 1;
      var $8458 = $8456;__label__ = 326; break;
    case 326: 
      var $8458;
      var $8459=(($8458)&1);
      var $8460=(($8459)|0);
      var $8461=$2;
      var $8462=(($8461+64)|0);
      var $8463=HEAP32[(($8462)>>2)];
      var $8464=$st;
      var $8465=(($8464+6)|0);
      var $8466=$8465;
      var $8467=HEAP16[(($8466)>>1)];
      var $8468=(($8467)&65535);
      var $8469=(($8463+($8468<<2))|0);
      var $8470=$8469;
      var $8471=$8470;
      HEAPF32[(($8471)>>2)]=$8460;
      __label__ = 366; break;
    case 327: 
      var $8473=$2;
      var $8474=(($8473+64)|0);
      var $8475=HEAP32[(($8474)>>2)];
      var $8476=$st;
      var $8477=(($8476+2)|0);
      var $8478=$8477;
      var $8479=HEAP16[(($8478)>>1)];
      var $8480=(($8479)&65535);
      var $8481=(($8475+($8480<<2))|0);
      var $8482=$8481;
      var $8483=$8482;
      var $8484=HEAP32[(($8483)>>2)];
      var $8485=(($8484)|0)==0;
      var $8486=(($8485)&1);
      var $8487=(($8486)|0);
      var $8488=$2;
      var $8489=(($8488+64)|0);
      var $8490=HEAP32[(($8489)>>2)];
      var $8491=$st;
      var $8492=(($8491+6)|0);
      var $8493=$8492;
      var $8494=HEAP16[(($8493)>>1)];
      var $8495=(($8494)&65535);
      var $8496=(($8490+($8495<<2))|0);
      var $8497=$8496;
      var $8498=$8497;
      HEAPF32[(($8498)>>2)]=$8487;
      __label__ = 366; break;
    case 328: 
      var $8500=$2;
      var $8501=(($8500+64)|0);
      var $8502=HEAP32[(($8501)>>2)];
      var $8503=$st;
      var $8504=(($8503+2)|0);
      var $8505=$8504;
      var $8506=HEAP16[(($8505)>>1)];
      var $8507=(($8506)&65535);
      var $8508=(($8502+($8507<<2))|0);
      var $8509=$8508;
      var $8510=$8509;
      var $8511=HEAP32[(($8510)>>2)];
      var $8512=(($8511)|0)!=0;
      var $8513=$8512 ^ 1;
      var $8514=(($8513)&1);
      var $8515=(($8514)|0);
      var $8516=$2;
      var $8517=(($8516+64)|0);
      var $8518=HEAP32[(($8517)>>2)];
      var $8519=$st;
      var $8520=(($8519+6)|0);
      var $8521=$8520;
      var $8522=HEAP16[(($8521)>>1)];
      var $8523=(($8522)&65535);
      var $8524=(($8518+($8523<<2))|0);
      var $8525=$8524;
      var $8526=$8525;
      HEAPF32[(($8526)>>2)]=$8515;
      __label__ = 366; break;
    case 329: 
      var $8528=$2;
      var $8529=(($8528+64)|0);
      var $8530=HEAP32[(($8529)>>2)];
      var $8531=$st;
      var $8532=(($8531+2)|0);
      var $8533=$8532;
      var $8534=HEAP16[(($8533)>>1)];
      var $8535=(($8534)&65535);
      var $8536=(($8530+($8535<<2))|0);
      var $8537=$8536;
      var $8538=$8537;
      var $8539=HEAP32[(($8538)>>2)];
      var $8540=$8539 & 2147483647;
      var $8541=(($8540)|0)!=0;
      if ($8541) { __label__ = 330; break; } else { __label__ = 333; break; }
    case 330: 
      var $8543=$st;
      var $8544=(($8543+4)|0);
      var $8545=$8544;
      var $8546=HEAP16[(($8545)>>1)];
      var $8547=(($8546 << 16) >> 16);
      var $8548=((($8547)-(1))|0);
      var $8549=$st;
      var $8550=(($8549+($8548<<3))|0);
      $st=$8550;
      var $8551=$jumpcount;
      var $8552=((($8551)+(1))|0);
      $jumpcount=$8552;
      var $8553=$5;
      var $8554=(($8552)|0) >= (($8553)|0);
      if ($8554) { __label__ = 331; break; } else { __label__ = 332; break; }
    case 331: 
      var $8556=$2;
      var $8557=$2;
      var $8558=(($8557)|0);
      var $8559=HEAP32[(($8558)>>2)];
      var $8560=$jumpcount;
      _qcvmerror($8556, ((STRING_TABLE.__str22)|0), (tempInt=STACKTOP,STACKTOP += 8,assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),HEAP32[((tempInt)>>2)]=$8559,HEAP32[(((tempInt)+(4))>>2)]=$8560,tempInt));
      __label__ = 332; break;
    case 332: 
      __label__ = 333; break;
    case 333: 
      __label__ = 366; break;
    case 334: 
      var $8564=$2;
      var $8565=(($8564+64)|0);
      var $8566=HEAP32[(($8565)>>2)];
      var $8567=$st;
      var $8568=(($8567+2)|0);
      var $8569=$8568;
      var $8570=HEAP16[(($8569)>>1)];
      var $8571=(($8570)&65535);
      var $8572=(($8566+($8571<<2))|0);
      var $8573=$8572;
      var $8574=$8573;
      var $8575=HEAP32[(($8574)>>2)];
      var $8576=$8575 & 2147483647;
      var $8577=(($8576)|0)!=0;
      if ($8577) { __label__ = 338; break; } else { __label__ = 335; break; }
    case 335: 
      var $8579=$st;
      var $8580=(($8579+4)|0);
      var $8581=$8580;
      var $8582=HEAP16[(($8581)>>1)];
      var $8583=(($8582 << 16) >> 16);
      var $8584=((($8583)-(1))|0);
      var $8585=$st;
      var $8586=(($8585+($8584<<3))|0);
      $st=$8586;
      var $8587=$jumpcount;
      var $8588=((($8587)+(1))|0);
      $jumpcount=$8588;
      var $8589=$5;
      var $8590=(($8588)|0) >= (($8589)|0);
      if ($8590) { __label__ = 336; break; } else { __label__ = 337; break; }
    case 336: 
      var $8592=$2;
      var $8593=$2;
      var $8594=(($8593)|0);
      var $8595=HEAP32[(($8594)>>2)];
      var $8596=$jumpcount;
      _qcvmerror($8592, ((STRING_TABLE.__str22)|0), (tempInt=STACKTOP,STACKTOP += 8,assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),HEAP32[((tempInt)>>2)]=$8595,HEAP32[(((tempInt)+(4))>>2)]=$8596,tempInt));
      __label__ = 337; break;
    case 337: 
      __label__ = 338; break;
    case 338: 
      __label__ = 366; break;
    case 339: 
      var $8600=$st;
      var $8601=(($8600)|0);
      var $8602=HEAP16[(($8601)>>1)];
      var $8603=(($8602)&65535);
      var $8604=((($8603)-(51))|0);
      var $8605=$2;
      var $8606=(($8605+184)|0);
      HEAP32[(($8606)>>2)]=$8604;
      var $8607=$2;
      var $8608=(($8607+64)|0);
      var $8609=HEAP32[(($8608)>>2)];
      var $8610=$st;
      var $8611=(($8610+2)|0);
      var $8612=$8611;
      var $8613=HEAP16[(($8612)>>1)];
      var $8614=(($8613)&65535);
      var $8615=(($8609+($8614<<2))|0);
      var $8616=$8615;
      var $8617=$8616;
      var $8618=HEAP32[(($8617)>>2)];
      var $8619=(($8618)|0)!=0;
      if ($8619) { __label__ = 341; break; } else { __label__ = 340; break; }
    case 340: 
      var $8621=$2;
      var $8622=$2;
      var $8623=(($8622)|0);
      var $8624=HEAP32[(($8623)>>2)];
      _qcvmerror($8621, ((STRING_TABLE.__str23)|0), (tempInt=STACKTOP,STACKTOP += 4,assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),HEAP32[((tempInt)>>2)]=$8624,tempInt));
      __label__ = 341; break;
    case 341: 
      var $8626=$2;
      var $8627=(($8626+64)|0);
      var $8628=HEAP32[(($8627)>>2)];
      var $8629=$st;
      var $8630=(($8629+2)|0);
      var $8631=$8630;
      var $8632=HEAP16[(($8631)>>1)];
      var $8633=(($8632)&65535);
      var $8634=(($8628+($8633<<2))|0);
      var $8635=$8634;
      var $8636=$8635;
      var $8637=HEAP32[(($8636)>>2)];
      var $8638=(($8637)|0)!=0;
      if ($8638) { __label__ = 342; break; } else { __label__ = 343; break; }
    case 342: 
      var $8640=$2;
      var $8641=(($8640+64)|0);
      var $8642=HEAP32[(($8641)>>2)];
      var $8643=$st;
      var $8644=(($8643+2)|0);
      var $8645=$8644;
      var $8646=HEAP16[(($8645)>>1)];
      var $8647=(($8646)&65535);
      var $8648=(($8642+($8647<<2))|0);
      var $8649=$8648;
      var $8650=$8649;
      var $8651=HEAP32[(($8650)>>2)];
      var $8652=$2;
      var $8653=(($8652+44)|0);
      var $8654=HEAP32[(($8653)>>2)];
      var $8655=(($8651)>>>0) >= (($8654)>>>0);
      if ($8655) { __label__ = 343; break; } else { __label__ = 344; break; }
    case 343: 
      var $8657=$2;
      var $8658=$2;
      var $8659=(($8658)|0);
      var $8660=HEAP32[(($8659)>>2)];
      _qcvmerror($8657, ((STRING_TABLE.__str24)|0), (tempInt=STACKTOP,STACKTOP += 4,assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),HEAP32[((tempInt)>>2)]=$8660,tempInt));
      __label__ = 488; break;
    case 344: 
      var $8662=$2;
      var $8663=(($8662+64)|0);
      var $8664=HEAP32[(($8663)>>2)];
      var $8665=$st;
      var $8666=(($8665+2)|0);
      var $8667=$8666;
      var $8668=HEAP16[(($8667)>>1)];
      var $8669=(($8668)&65535);
      var $8670=(($8664+($8669<<2))|0);
      var $8671=$8670;
      var $8672=$8671;
      var $8673=HEAP32[(($8672)>>2)];
      var $8674=$2;
      var $8675=(($8674+40)|0);
      var $8676=HEAP32[(($8675)>>2)];
      var $8677=(($8676+($8673)*(36))|0);
      $newf5=$8677;
      var $8678=$newf5;
      var $8679=(($8678+12)|0);
      var $8680=HEAP32[(($8679)>>2)];
      var $8681=((($8680)+(1))|0);
      HEAP32[(($8679)>>2)]=$8681;
      var $8682=$st;
      var $8683=$2;
      var $8684=(($8683+4)|0);
      var $8685=HEAP32[(($8684)>>2)];
      var $8686=$8682;
      var $8687=$8685;
      var $8688=((($8686)-($8687))|0);
      var $8689=((((($8688)|0))/(8))&-1);
      var $8690=((($8689)+(1))|0);
      var $8691=$2;
      var $8692=(($8691+176)|0);
      HEAP32[(($8692)>>2)]=$8690;
      var $8693=$newf5;
      var $8694=(($8693)|0);
      var $8695=HEAP32[(($8694)>>2)];
      var $8696=(($8695)|0) < 0;
      if ($8696) { __label__ = 345; break; } else { __label__ = 350; break; }
    case 345: 
      var $8698=$newf5;
      var $8699=(($8698)|0);
      var $8700=HEAP32[(($8699)>>2)];
      var $8701=(((-$8700))|0);
      $builtinnumber8=$8701;
      var $8702=$builtinnumber8;
      var $8703=$2;
      var $8704=(($8703+132)|0);
      var $8705=HEAP32[(($8704)>>2)];
      var $8706=(($8702)>>>0) < (($8705)>>>0);
      if ($8706) { __label__ = 346; break; } else { __label__ = 348; break; }
    case 346: 
      var $8708=$builtinnumber8;
      var $8709=$2;
      var $8710=(($8709+128)|0);
      var $8711=HEAP32[(($8710)>>2)];
      var $8712=(($8711+($8708<<2))|0);
      var $8713=HEAP32[(($8712)>>2)];
      var $8714=(($8713)|0)!=0;
      if ($8714) { __label__ = 347; break; } else { __label__ = 348; break; }
    case 347: 
      var $8716=$builtinnumber8;
      var $8717=$2;
      var $8718=(($8717+128)|0);
      var $8719=HEAP32[(($8718)>>2)];
      var $8720=(($8719+($8716<<2))|0);
      var $8721=HEAP32[(($8720)>>2)];
      var $8722=$2;
      var $8723=FUNCTION_TABLE[$8721]($8722);
      __label__ = 349; break;
    case 348: 
      var $8725=$2;
      var $8726=$builtinnumber8;
      var $8727=$2;
      var $8728=(($8727)|0);
      var $8729=HEAP32[(($8728)>>2)];
      _qcvmerror($8725, ((STRING_TABLE.__str25)|0), (tempInt=STACKTOP,STACKTOP += 8,assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),HEAP32[((tempInt)>>2)]=$8726,HEAP32[(((tempInt)+(4))>>2)]=$8729,tempInt));
      __label__ = 349; break;
    case 349: 
      __label__ = 351; break;
    case 350: 
      var $8732=$2;
      var $8733=(($8732+4)|0);
      var $8734=HEAP32[(($8733)>>2)];
      var $8735=$2;
      var $8736=$newf5;
      var $8737=_prog_enterfunction($8735, $8736);
      var $8738=(($8734+($8737<<3))|0);
      var $8739=((($8738)-(8))|0);
      $st=$8739;
      __label__ = 351; break;
    case 351: 
      var $8741=$2;
      var $8742=(($8741+112)|0);
      var $8743=HEAP32[(($8742)>>2)];
      var $8744=(($8743)|0)!=0;
      if ($8744) { __label__ = 352; break; } else { __label__ = 353; break; }
    case 352: 
      __label__ = 488; break;
    case 353: 
      __label__ = 366; break;
    case 354: 
      var $8748=$2;
      var $8749=$2;
      var $8750=(($8749)|0);
      var $8751=HEAP32[(($8750)>>2)];
      _qcvmerror($8748, ((STRING_TABLE.__str26)|0), (tempInt=STACKTOP,STACKTOP += 4,assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),HEAP32[((tempInt)>>2)]=$8751,tempInt));
      __label__ = 366; break;
    case 355: 
      var $8753=$st;
      var $8754=(($8753+2)|0);
      var $8755=$8754;
      var $8756=HEAP16[(($8755)>>1)];
      var $8757=(($8756 << 16) >> 16);
      var $8758=((($8757)-(1))|0);
      var $8759=$st;
      var $8760=(($8759+($8758<<3))|0);
      $st=$8760;
      var $8761=$jumpcount;
      var $8762=((($8761)+(1))|0);
      $jumpcount=$8762;
      var $8763=(($8762)|0)==10000000;
      if ($8763) { __label__ = 356; break; } else { __label__ = 357; break; }
    case 356: 
      var $8765=$2;
      var $8766=$2;
      var $8767=(($8766)|0);
      var $8768=HEAP32[(($8767)>>2)];
      var $8769=$jumpcount;
      _qcvmerror($8765, ((STRING_TABLE.__str22)|0), (tempInt=STACKTOP,STACKTOP += 8,assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),HEAP32[((tempInt)>>2)]=$8768,HEAP32[(((tempInt)+(4))>>2)]=$8769,tempInt));
      __label__ = 357; break;
    case 357: 
      __label__ = 366; break;
    case 358: 
      var $8772=$2;
      var $8773=(($8772+64)|0);
      var $8774=HEAP32[(($8773)>>2)];
      var $8775=$st;
      var $8776=(($8775+2)|0);
      var $8777=$8776;
      var $8778=HEAP16[(($8777)>>1)];
      var $8779=(($8778)&65535);
      var $8780=(($8774+($8779<<2))|0);
      var $8781=$8780;
      var $8782=$8781;
      var $8783=HEAP32[(($8782)>>2)];
      var $8784=$8783 & 2147483647;
      var $8785=(($8784)|0)!=0;
      if ($8785) { __label__ = 359; break; } else { var $8802 = 0;__label__ = 360; break; }
    case 359: 
      var $8787=$2;
      var $8788=(($8787+64)|0);
      var $8789=HEAP32[(($8788)>>2)];
      var $8790=$st;
      var $8791=(($8790+4)|0);
      var $8792=$8791;
      var $8793=HEAP16[(($8792)>>1)];
      var $8794=(($8793)&65535);
      var $8795=(($8789+($8794<<2))|0);
      var $8796=$8795;
      var $8797=$8796;
      var $8798=HEAP32[(($8797)>>2)];
      var $8799=$8798 & 2147483647;
      var $8800=(($8799)|0)!=0;
      var $8802 = $8800;__label__ = 360; break;
    case 360: 
      var $8802;
      var $8803=(($8802)&1);
      var $8804=(($8803)|0);
      var $8805=$2;
      var $8806=(($8805+64)|0);
      var $8807=HEAP32[(($8806)>>2)];
      var $8808=$st;
      var $8809=(($8808+6)|0);
      var $8810=$8809;
      var $8811=HEAP16[(($8810)>>1)];
      var $8812=(($8811)&65535);
      var $8813=(($8807+($8812<<2))|0);
      var $8814=$8813;
      var $8815=$8814;
      HEAPF32[(($8815)>>2)]=$8804;
      __label__ = 366; break;
    case 361: 
      var $8817=$2;
      var $8818=(($8817+64)|0);
      var $8819=HEAP32[(($8818)>>2)];
      var $8820=$st;
      var $8821=(($8820+2)|0);
      var $8822=$8821;
      var $8823=HEAP16[(($8822)>>1)];
      var $8824=(($8823)&65535);
      var $8825=(($8819+($8824<<2))|0);
      var $8826=$8825;
      var $8827=$8826;
      var $8828=HEAP32[(($8827)>>2)];
      var $8829=$8828 & 2147483647;
      var $8830=(($8829)|0)!=0;
      if ($8830) { var $8847 = 1;__label__ = 363; break; } else { __label__ = 362; break; }
    case 362: 
      var $8832=$2;
      var $8833=(($8832+64)|0);
      var $8834=HEAP32[(($8833)>>2)];
      var $8835=$st;
      var $8836=(($8835+4)|0);
      var $8837=$8836;
      var $8838=HEAP16[(($8837)>>1)];
      var $8839=(($8838)&65535);
      var $8840=(($8834+($8839<<2))|0);
      var $8841=$8840;
      var $8842=$8841;
      var $8843=HEAP32[(($8842)>>2)];
      var $8844=$8843 & 2147483647;
      var $8845=(($8844)|0)!=0;
      var $8847 = $8845;__label__ = 363; break;
    case 363: 
      var $8847;
      var $8848=(($8847)&1);
      var $8849=(($8848)|0);
      var $8850=$2;
      var $8851=(($8850+64)|0);
      var $8852=HEAP32[(($8851)>>2)];
      var $8853=$st;
      var $8854=(($8853+6)|0);
      var $8855=$8854;
      var $8856=HEAP16[(($8855)>>1)];
      var $8857=(($8856)&65535);
      var $8858=(($8852+($8857<<2))|0);
      var $8859=$8858;
      var $8860=$8859;
      HEAPF32[(($8860)>>2)]=$8849;
      __label__ = 366; break;
    case 364: 
      var $8862=$2;
      var $8863=(($8862+64)|0);
      var $8864=HEAP32[(($8863)>>2)];
      var $8865=$st;
      var $8866=(($8865+2)|0);
      var $8867=$8866;
      var $8868=HEAP16[(($8867)>>1)];
      var $8869=(($8868)&65535);
      var $8870=(($8864+($8869<<2))|0);
      var $8871=$8870;
      var $8872=$8871;
      var $8873=HEAPF32[(($8872)>>2)];
      var $8874=(($8873)&-1);
      var $8875=$2;
      var $8876=(($8875+64)|0);
      var $8877=HEAP32[(($8876)>>2)];
      var $8878=$st;
      var $8879=(($8878+4)|0);
      var $8880=$8879;
      var $8881=HEAP16[(($8880)>>1)];
      var $8882=(($8881)&65535);
      var $8883=(($8877+($8882<<2))|0);
      var $8884=$8883;
      var $8885=$8884;
      var $8886=HEAPF32[(($8885)>>2)];
      var $8887=(($8886)&-1);
      var $8888=$8874 & $8887;
      var $8889=(($8888)|0);
      var $8890=$2;
      var $8891=(($8890+64)|0);
      var $8892=HEAP32[(($8891)>>2)];
      var $8893=$st;
      var $8894=(($8893+6)|0);
      var $8895=$8894;
      var $8896=HEAP16[(($8895)>>1)];
      var $8897=(($8896)&65535);
      var $8898=(($8892+($8897<<2))|0);
      var $8899=$8898;
      var $8900=$8899;
      HEAPF32[(($8900)>>2)]=$8889;
      __label__ = 366; break;
    case 365: 
      var $8902=$2;
      var $8903=(($8902+64)|0);
      var $8904=HEAP32[(($8903)>>2)];
      var $8905=$st;
      var $8906=(($8905+2)|0);
      var $8907=$8906;
      var $8908=HEAP16[(($8907)>>1)];
      var $8909=(($8908)&65535);
      var $8910=(($8904+($8909<<2))|0);
      var $8911=$8910;
      var $8912=$8911;
      var $8913=HEAPF32[(($8912)>>2)];
      var $8914=(($8913)&-1);
      var $8915=$2;
      var $8916=(($8915+64)|0);
      var $8917=HEAP32[(($8916)>>2)];
      var $8918=$st;
      var $8919=(($8918+4)|0);
      var $8920=$8919;
      var $8921=HEAP16[(($8920)>>1)];
      var $8922=(($8921)&65535);
      var $8923=(($8917+($8922<<2))|0);
      var $8924=$8923;
      var $8925=$8924;
      var $8926=HEAPF32[(($8925)>>2)];
      var $8927=(($8926)&-1);
      var $8928=$8914 | $8927;
      var $8929=(($8928)|0);
      var $8930=$2;
      var $8931=(($8930+64)|0);
      var $8932=HEAP32[(($8931)>>2)];
      var $8933=$st;
      var $8934=(($8933+6)|0);
      var $8935=$8934;
      var $8936=HEAP16[(($8935)>>1)];
      var $8937=(($8936)&65535);
      var $8938=(($8932+($8937<<2))|0);
      var $8939=$8938;
      var $8940=$8939;
      HEAPF32[(($8940)>>2)]=$8929;
      __label__ = 366; break;
    case 366: 
      __label__ = 247; break;
    case 367: 
      __label__ = 368; break;
    case 368: 
      var $8944=$st;
      var $8945=(($8944+8)|0);
      $st=$8945;
      var $8946=$st;
      var $8947=$2;
      var $8948=(($8947+4)|0);
      var $8949=HEAP32[(($8948)>>2)];
      var $8950=$8946;
      var $8951=$8949;
      var $8952=((($8950)-($8951))|0);
      var $8953=((((($8952)|0))/(8))&-1);
      var $8954=$2;
      var $8955=(($8954+116)|0);
      var $8956=HEAP32[(($8955)>>2)];
      var $8957=(($8956+($8953<<2))|0);
      var $8958=HEAP32[(($8957)>>2)];
      var $8959=((($8958)+(1))|0);
      HEAP32[(($8957)>>2)]=$8959;
      var $8960=$2;
      var $8961=$st;
      _prog_print_statement($8960, $8961);
      var $8962=$st;
      var $8963=(($8962)|0);
      var $8964=HEAP16[(($8963)>>1)];
      var $8965=(($8964)&65535);
      if ((($8965)|0) == 0 || (($8965)|0) == 43) {
        __label__ = 370; break;
      }
      else if ((($8965)|0) == 1) {
        __label__ = 373; break;
      }
      else if ((($8965)|0) == 2) {
        __label__ = 374; break;
      }
      else if ((($8965)|0) == 3) {
        __label__ = 375; break;
      }
      else if ((($8965)|0) == 4) {
        __label__ = 376; break;
      }
      else if ((($8965)|0) == 5) {
        __label__ = 377; break;
      }
      else if ((($8965)|0) == 6) {
        __label__ = 381; break;
      }
      else if ((($8965)|0) == 7) {
        __label__ = 382; break;
      }
      else if ((($8965)|0) == 8) {
        __label__ = 383; break;
      }
      else if ((($8965)|0) == 9) {
        __label__ = 384; break;
      }
      else if ((($8965)|0) == 10) {
        __label__ = 385; break;
      }
      else if ((($8965)|0) == 11) {
        __label__ = 386; break;
      }
      else if ((($8965)|0) == 12) {
        __label__ = 390; break;
      }
      else if ((($8965)|0) == 13) {
        __label__ = 391; break;
      }
      else if ((($8965)|0) == 14) {
        __label__ = 392; break;
      }
      else if ((($8965)|0) == 15) {
        __label__ = 393; break;
      }
      else if ((($8965)|0) == 16) {
        __label__ = 394; break;
      }
      else if ((($8965)|0) == 17) {
        __label__ = 398; break;
      }
      else if ((($8965)|0) == 18) {
        __label__ = 399; break;
      }
      else if ((($8965)|0) == 19) {
        __label__ = 400; break;
      }
      else if ((($8965)|0) == 20) {
        __label__ = 401; break;
      }
      else if ((($8965)|0) == 21) {
        __label__ = 402; break;
      }
      else if ((($8965)|0) == 22) {
        __label__ = 403; break;
      }
      else if ((($8965)|0) == 23) {
        __label__ = 404; break;
      }
      else if ((($8965)|0) == 24 || (($8965)|0) == 26 || (($8965)|0) == 28 || (($8965)|0) == 27 || (($8965)|0) == 29) {
        __label__ = 405; break;
      }
      else if ((($8965)|0) == 25) {
        __label__ = 411; break;
      }
      else if ((($8965)|0) == 30) {
        __label__ = 418; break;
      }
      else if ((($8965)|0) == 31 || (($8965)|0) == 33 || (($8965)|0) == 34 || (($8965)|0) == 35 || (($8965)|0) == 36) {
        __label__ = 424; break;
      }
      else if ((($8965)|0) == 32) {
        __label__ = 425; break;
      }
      else if ((($8965)|0) == 37 || (($8965)|0) == 39 || (($8965)|0) == 40 || (($8965)|0) == 41 || (($8965)|0) == 42) {
        __label__ = 426; break;
      }
      else if ((($8965)|0) == 38) {
        __label__ = 433; break;
      }
      else if ((($8965)|0) == 44) {
        __label__ = 440; break;
      }
      else if ((($8965)|0) == 45) {
        __label__ = 441; break;
      }
      else if ((($8965)|0) == 46) {
        __label__ = 445; break;
      }
      else if ((($8965)|0) == 47) {
        __label__ = 448; break;
      }
      else if ((($8965)|0) == 48) {
        __label__ = 449; break;
      }
      else if ((($8965)|0) == 49) {
        __label__ = 450; break;
      }
      else if ((($8965)|0) == 50) {
        __label__ = 455; break;
      }
      else if ((($8965)|0) == 51 || (($8965)|0) == 52 || (($8965)|0) == 53 || (($8965)|0) == 54 || (($8965)|0) == 55 || (($8965)|0) == 56 || (($8965)|0) == 57 || (($8965)|0) == 58 || (($8965)|0) == 59) {
        __label__ = 460; break;
      }
      else if ((($8965)|0) == 60) {
        __label__ = 475; break;
      }
      else if ((($8965)|0) == 61) {
        __label__ = 476; break;
      }
      else if ((($8965)|0) == 62) {
        __label__ = 479; break;
      }
      else if ((($8965)|0) == 63) {
        __label__ = 482; break;
      }
      else if ((($8965)|0) == 64) {
        __label__ = 485; break;
      }
      else if ((($8965)|0) == 65) {
        __label__ = 486; break;
      }
      else {
      __label__ = 369; break;
      }
      
    case 369: 
      var $8967=$2;
      var $8968=$2;
      var $8969=(($8968)|0);
      var $8970=HEAP32[(($8969)>>2)];
      _qcvmerror($8967, ((STRING_TABLE.__str16)|0), (tempInt=STACKTOP,STACKTOP += 4,assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),HEAP32[((tempInt)>>2)]=$8970,tempInt));
      __label__ = 488; break;
    case 370: 
      var $8972=$2;
      var $8973=(($8972+64)|0);
      var $8974=HEAP32[(($8973)>>2)];
      var $8975=$st;
      var $8976=(($8975+2)|0);
      var $8977=$8976;
      var $8978=HEAP16[(($8977)>>1)];
      var $8979=(($8978)&65535);
      var $8980=(($8974+($8979<<2))|0);
      var $8981=$8980;
      var $8982=$8981;
      var $8983=(($8982)|0);
      var $8984=HEAP32[(($8983)>>2)];
      var $8985=$2;
      var $8986=(($8985+64)|0);
      var $8987=HEAP32[(($8986)>>2)];
      var $8988=(($8987+4)|0);
      var $8989=$8988;
      var $8990=$8989;
      var $8991=(($8990)|0);
      HEAP32[(($8991)>>2)]=$8984;
      var $8992=$2;
      var $8993=(($8992+64)|0);
      var $8994=HEAP32[(($8993)>>2)];
      var $8995=$st;
      var $8996=(($8995+2)|0);
      var $8997=$8996;
      var $8998=HEAP16[(($8997)>>1)];
      var $8999=(($8998)&65535);
      var $9000=(($8994+($8999<<2))|0);
      var $9001=$9000;
      var $9002=$9001;
      var $9003=(($9002+4)|0);
      var $9004=HEAP32[(($9003)>>2)];
      var $9005=$2;
      var $9006=(($9005+64)|0);
      var $9007=HEAP32[(($9006)>>2)];
      var $9008=(($9007+4)|0);
      var $9009=$9008;
      var $9010=$9009;
      var $9011=(($9010+4)|0);
      HEAP32[(($9011)>>2)]=$9004;
      var $9012=$2;
      var $9013=(($9012+64)|0);
      var $9014=HEAP32[(($9013)>>2)];
      var $9015=$st;
      var $9016=(($9015+2)|0);
      var $9017=$9016;
      var $9018=HEAP16[(($9017)>>1)];
      var $9019=(($9018)&65535);
      var $9020=(($9014+($9019<<2))|0);
      var $9021=$9020;
      var $9022=$9021;
      var $9023=(($9022+8)|0);
      var $9024=HEAP32[(($9023)>>2)];
      var $9025=$2;
      var $9026=(($9025+64)|0);
      var $9027=HEAP32[(($9026)>>2)];
      var $9028=(($9027+4)|0);
      var $9029=$9028;
      var $9030=$9029;
      var $9031=(($9030+8)|0);
      HEAP32[(($9031)>>2)]=$9024;
      var $9032=$2;
      var $9033=(($9032+4)|0);
      var $9034=HEAP32[(($9033)>>2)];
      var $9035=$2;
      var $9036=_prog_leavefunction($9035);
      var $9037=(($9034+($9036<<3))|0);
      $st=$9037;
      var $9038=$2;
      var $9039=(($9038+168)|0);
      var $9040=HEAP32[(($9039)>>2)];
      var $9041=(($9040)|0)!=0;
      if ($9041) { __label__ = 372; break; } else { __label__ = 371; break; }
    case 371: 
      __label__ = 488; break;
    case 372: 
      __label__ = 487; break;
    case 373: 
      var $9045=$2;
      var $9046=(($9045+64)|0);
      var $9047=HEAP32[(($9046)>>2)];
      var $9048=$st;
      var $9049=(($9048+2)|0);
      var $9050=$9049;
      var $9051=HEAP16[(($9050)>>1)];
      var $9052=(($9051)&65535);
      var $9053=(($9047+($9052<<2))|0);
      var $9054=$9053;
      var $9055=$9054;
      var $9056=HEAPF32[(($9055)>>2)];
      var $9057=$2;
      var $9058=(($9057+64)|0);
      var $9059=HEAP32[(($9058)>>2)];
      var $9060=$st;
      var $9061=(($9060+4)|0);
      var $9062=$9061;
      var $9063=HEAP16[(($9062)>>1)];
      var $9064=(($9063)&65535);
      var $9065=(($9059+($9064<<2))|0);
      var $9066=$9065;
      var $9067=$9066;
      var $9068=HEAPF32[(($9067)>>2)];
      var $9069=($9056)*($9068);
      var $9070=$2;
      var $9071=(($9070+64)|0);
      var $9072=HEAP32[(($9071)>>2)];
      var $9073=$st;
      var $9074=(($9073+6)|0);
      var $9075=$9074;
      var $9076=HEAP16[(($9075)>>1)];
      var $9077=(($9076)&65535);
      var $9078=(($9072+($9077<<2))|0);
      var $9079=$9078;
      var $9080=$9079;
      HEAPF32[(($9080)>>2)]=$9069;
      __label__ = 487; break;
    case 374: 
      var $9082=$2;
      var $9083=(($9082+64)|0);
      var $9084=HEAP32[(($9083)>>2)];
      var $9085=$st;
      var $9086=(($9085+2)|0);
      var $9087=$9086;
      var $9088=HEAP16[(($9087)>>1)];
      var $9089=(($9088)&65535);
      var $9090=(($9084+($9089<<2))|0);
      var $9091=$9090;
      var $9092=$9091;
      var $9093=(($9092)|0);
      var $9094=HEAPF32[(($9093)>>2)];
      var $9095=$2;
      var $9096=(($9095+64)|0);
      var $9097=HEAP32[(($9096)>>2)];
      var $9098=$st;
      var $9099=(($9098+4)|0);
      var $9100=$9099;
      var $9101=HEAP16[(($9100)>>1)];
      var $9102=(($9101)&65535);
      var $9103=(($9097+($9102<<2))|0);
      var $9104=$9103;
      var $9105=$9104;
      var $9106=(($9105)|0);
      var $9107=HEAPF32[(($9106)>>2)];
      var $9108=($9094)*($9107);
      var $9109=$2;
      var $9110=(($9109+64)|0);
      var $9111=HEAP32[(($9110)>>2)];
      var $9112=$st;
      var $9113=(($9112+2)|0);
      var $9114=$9113;
      var $9115=HEAP16[(($9114)>>1)];
      var $9116=(($9115)&65535);
      var $9117=(($9111+($9116<<2))|0);
      var $9118=$9117;
      var $9119=$9118;
      var $9120=(($9119+4)|0);
      var $9121=HEAPF32[(($9120)>>2)];
      var $9122=$2;
      var $9123=(($9122+64)|0);
      var $9124=HEAP32[(($9123)>>2)];
      var $9125=$st;
      var $9126=(($9125+4)|0);
      var $9127=$9126;
      var $9128=HEAP16[(($9127)>>1)];
      var $9129=(($9128)&65535);
      var $9130=(($9124+($9129<<2))|0);
      var $9131=$9130;
      var $9132=$9131;
      var $9133=(($9132+4)|0);
      var $9134=HEAPF32[(($9133)>>2)];
      var $9135=($9121)*($9134);
      var $9136=($9108)+($9135);
      var $9137=$2;
      var $9138=(($9137+64)|0);
      var $9139=HEAP32[(($9138)>>2)];
      var $9140=$st;
      var $9141=(($9140+2)|0);
      var $9142=$9141;
      var $9143=HEAP16[(($9142)>>1)];
      var $9144=(($9143)&65535);
      var $9145=(($9139+($9144<<2))|0);
      var $9146=$9145;
      var $9147=$9146;
      var $9148=(($9147+8)|0);
      var $9149=HEAPF32[(($9148)>>2)];
      var $9150=$2;
      var $9151=(($9150+64)|0);
      var $9152=HEAP32[(($9151)>>2)];
      var $9153=$st;
      var $9154=(($9153+4)|0);
      var $9155=$9154;
      var $9156=HEAP16[(($9155)>>1)];
      var $9157=(($9156)&65535);
      var $9158=(($9152+($9157<<2))|0);
      var $9159=$9158;
      var $9160=$9159;
      var $9161=(($9160+8)|0);
      var $9162=HEAPF32[(($9161)>>2)];
      var $9163=($9149)*($9162);
      var $9164=($9136)+($9163);
      var $9165=$2;
      var $9166=(($9165+64)|0);
      var $9167=HEAP32[(($9166)>>2)];
      var $9168=$st;
      var $9169=(($9168+6)|0);
      var $9170=$9169;
      var $9171=HEAP16[(($9170)>>1)];
      var $9172=(($9171)&65535);
      var $9173=(($9167+($9172<<2))|0);
      var $9174=$9173;
      var $9175=$9174;
      HEAPF32[(($9175)>>2)]=$9164;
      __label__ = 487; break;
    case 375: 
      var $9177=$2;
      var $9178=(($9177+64)|0);
      var $9179=HEAP32[(($9178)>>2)];
      var $9180=$st;
      var $9181=(($9180+2)|0);
      var $9182=$9181;
      var $9183=HEAP16[(($9182)>>1)];
      var $9184=(($9183)&65535);
      var $9185=(($9179+($9184<<2))|0);
      var $9186=$9185;
      var $9187=$9186;
      var $9188=HEAPF32[(($9187)>>2)];
      var $9189=$2;
      var $9190=(($9189+64)|0);
      var $9191=HEAP32[(($9190)>>2)];
      var $9192=$st;
      var $9193=(($9192+4)|0);
      var $9194=$9193;
      var $9195=HEAP16[(($9194)>>1)];
      var $9196=(($9195)&65535);
      var $9197=(($9191+($9196<<2))|0);
      var $9198=$9197;
      var $9199=$9198;
      var $9200=(($9199)|0);
      var $9201=HEAPF32[(($9200)>>2)];
      var $9202=($9188)*($9201);
      var $9203=$2;
      var $9204=(($9203+64)|0);
      var $9205=HEAP32[(($9204)>>2)];
      var $9206=$st;
      var $9207=(($9206+6)|0);
      var $9208=$9207;
      var $9209=HEAP16[(($9208)>>1)];
      var $9210=(($9209)&65535);
      var $9211=(($9205+($9210<<2))|0);
      var $9212=$9211;
      var $9213=$9212;
      var $9214=(($9213)|0);
      HEAPF32[(($9214)>>2)]=$9202;
      var $9215=$2;
      var $9216=(($9215+64)|0);
      var $9217=HEAP32[(($9216)>>2)];
      var $9218=$st;
      var $9219=(($9218+2)|0);
      var $9220=$9219;
      var $9221=HEAP16[(($9220)>>1)];
      var $9222=(($9221)&65535);
      var $9223=(($9217+($9222<<2))|0);
      var $9224=$9223;
      var $9225=$9224;
      var $9226=HEAPF32[(($9225)>>2)];
      var $9227=$2;
      var $9228=(($9227+64)|0);
      var $9229=HEAP32[(($9228)>>2)];
      var $9230=$st;
      var $9231=(($9230+4)|0);
      var $9232=$9231;
      var $9233=HEAP16[(($9232)>>1)];
      var $9234=(($9233)&65535);
      var $9235=(($9229+($9234<<2))|0);
      var $9236=$9235;
      var $9237=$9236;
      var $9238=(($9237+4)|0);
      var $9239=HEAPF32[(($9238)>>2)];
      var $9240=($9226)*($9239);
      var $9241=$2;
      var $9242=(($9241+64)|0);
      var $9243=HEAP32[(($9242)>>2)];
      var $9244=$st;
      var $9245=(($9244+6)|0);
      var $9246=$9245;
      var $9247=HEAP16[(($9246)>>1)];
      var $9248=(($9247)&65535);
      var $9249=(($9243+($9248<<2))|0);
      var $9250=$9249;
      var $9251=$9250;
      var $9252=(($9251+4)|0);
      HEAPF32[(($9252)>>2)]=$9240;
      var $9253=$2;
      var $9254=(($9253+64)|0);
      var $9255=HEAP32[(($9254)>>2)];
      var $9256=$st;
      var $9257=(($9256+2)|0);
      var $9258=$9257;
      var $9259=HEAP16[(($9258)>>1)];
      var $9260=(($9259)&65535);
      var $9261=(($9255+($9260<<2))|0);
      var $9262=$9261;
      var $9263=$9262;
      var $9264=HEAPF32[(($9263)>>2)];
      var $9265=$2;
      var $9266=(($9265+64)|0);
      var $9267=HEAP32[(($9266)>>2)];
      var $9268=$st;
      var $9269=(($9268+4)|0);
      var $9270=$9269;
      var $9271=HEAP16[(($9270)>>1)];
      var $9272=(($9271)&65535);
      var $9273=(($9267+($9272<<2))|0);
      var $9274=$9273;
      var $9275=$9274;
      var $9276=(($9275+8)|0);
      var $9277=HEAPF32[(($9276)>>2)];
      var $9278=($9264)*($9277);
      var $9279=$2;
      var $9280=(($9279+64)|0);
      var $9281=HEAP32[(($9280)>>2)];
      var $9282=$st;
      var $9283=(($9282+6)|0);
      var $9284=$9283;
      var $9285=HEAP16[(($9284)>>1)];
      var $9286=(($9285)&65535);
      var $9287=(($9281+($9286<<2))|0);
      var $9288=$9287;
      var $9289=$9288;
      var $9290=(($9289+8)|0);
      HEAPF32[(($9290)>>2)]=$9278;
      __label__ = 487; break;
    case 376: 
      var $9292=$2;
      var $9293=(($9292+64)|0);
      var $9294=HEAP32[(($9293)>>2)];
      var $9295=$st;
      var $9296=(($9295+4)|0);
      var $9297=$9296;
      var $9298=HEAP16[(($9297)>>1)];
      var $9299=(($9298)&65535);
      var $9300=(($9294+($9299<<2))|0);
      var $9301=$9300;
      var $9302=$9301;
      var $9303=HEAPF32[(($9302)>>2)];
      var $9304=$2;
      var $9305=(($9304+64)|0);
      var $9306=HEAP32[(($9305)>>2)];
      var $9307=$st;
      var $9308=(($9307+2)|0);
      var $9309=$9308;
      var $9310=HEAP16[(($9309)>>1)];
      var $9311=(($9310)&65535);
      var $9312=(($9306+($9311<<2))|0);
      var $9313=$9312;
      var $9314=$9313;
      var $9315=(($9314)|0);
      var $9316=HEAPF32[(($9315)>>2)];
      var $9317=($9303)*($9316);
      var $9318=$2;
      var $9319=(($9318+64)|0);
      var $9320=HEAP32[(($9319)>>2)];
      var $9321=$st;
      var $9322=(($9321+6)|0);
      var $9323=$9322;
      var $9324=HEAP16[(($9323)>>1)];
      var $9325=(($9324)&65535);
      var $9326=(($9320+($9325<<2))|0);
      var $9327=$9326;
      var $9328=$9327;
      var $9329=(($9328)|0);
      HEAPF32[(($9329)>>2)]=$9317;
      var $9330=$2;
      var $9331=(($9330+64)|0);
      var $9332=HEAP32[(($9331)>>2)];
      var $9333=$st;
      var $9334=(($9333+4)|0);
      var $9335=$9334;
      var $9336=HEAP16[(($9335)>>1)];
      var $9337=(($9336)&65535);
      var $9338=(($9332+($9337<<2))|0);
      var $9339=$9338;
      var $9340=$9339;
      var $9341=HEAPF32[(($9340)>>2)];
      var $9342=$2;
      var $9343=(($9342+64)|0);
      var $9344=HEAP32[(($9343)>>2)];
      var $9345=$st;
      var $9346=(($9345+2)|0);
      var $9347=$9346;
      var $9348=HEAP16[(($9347)>>1)];
      var $9349=(($9348)&65535);
      var $9350=(($9344+($9349<<2))|0);
      var $9351=$9350;
      var $9352=$9351;
      var $9353=(($9352+4)|0);
      var $9354=HEAPF32[(($9353)>>2)];
      var $9355=($9341)*($9354);
      var $9356=$2;
      var $9357=(($9356+64)|0);
      var $9358=HEAP32[(($9357)>>2)];
      var $9359=$st;
      var $9360=(($9359+6)|0);
      var $9361=$9360;
      var $9362=HEAP16[(($9361)>>1)];
      var $9363=(($9362)&65535);
      var $9364=(($9358+($9363<<2))|0);
      var $9365=$9364;
      var $9366=$9365;
      var $9367=(($9366+4)|0);
      HEAPF32[(($9367)>>2)]=$9355;
      var $9368=$2;
      var $9369=(($9368+64)|0);
      var $9370=HEAP32[(($9369)>>2)];
      var $9371=$st;
      var $9372=(($9371+4)|0);
      var $9373=$9372;
      var $9374=HEAP16[(($9373)>>1)];
      var $9375=(($9374)&65535);
      var $9376=(($9370+($9375<<2))|0);
      var $9377=$9376;
      var $9378=$9377;
      var $9379=HEAPF32[(($9378)>>2)];
      var $9380=$2;
      var $9381=(($9380+64)|0);
      var $9382=HEAP32[(($9381)>>2)];
      var $9383=$st;
      var $9384=(($9383+2)|0);
      var $9385=$9384;
      var $9386=HEAP16[(($9385)>>1)];
      var $9387=(($9386)&65535);
      var $9388=(($9382+($9387<<2))|0);
      var $9389=$9388;
      var $9390=$9389;
      var $9391=(($9390+8)|0);
      var $9392=HEAPF32[(($9391)>>2)];
      var $9393=($9379)*($9392);
      var $9394=$2;
      var $9395=(($9394+64)|0);
      var $9396=HEAP32[(($9395)>>2)];
      var $9397=$st;
      var $9398=(($9397+6)|0);
      var $9399=$9398;
      var $9400=HEAP16[(($9399)>>1)];
      var $9401=(($9400)&65535);
      var $9402=(($9396+($9401<<2))|0);
      var $9403=$9402;
      var $9404=$9403;
      var $9405=(($9404+8)|0);
      HEAPF32[(($9405)>>2)]=$9393;
      __label__ = 487; break;
    case 377: 
      var $9407=$2;
      var $9408=(($9407+64)|0);
      var $9409=HEAP32[(($9408)>>2)];
      var $9410=$st;
      var $9411=(($9410+4)|0);
      var $9412=$9411;
      var $9413=HEAP16[(($9412)>>1)];
      var $9414=(($9413)&65535);
      var $9415=(($9409+($9414<<2))|0);
      var $9416=$9415;
      var $9417=$9416;
      var $9418=HEAPF32[(($9417)>>2)];
      var $9419=$9418 != 0;
      if ($9419) { __label__ = 378; break; } else { __label__ = 379; break; }
    case 378: 
      var $9421=$2;
      var $9422=(($9421+64)|0);
      var $9423=HEAP32[(($9422)>>2)];
      var $9424=$st;
      var $9425=(($9424+2)|0);
      var $9426=$9425;
      var $9427=HEAP16[(($9426)>>1)];
      var $9428=(($9427)&65535);
      var $9429=(($9423+($9428<<2))|0);
      var $9430=$9429;
      var $9431=$9430;
      var $9432=HEAPF32[(($9431)>>2)];
      var $9433=$2;
      var $9434=(($9433+64)|0);
      var $9435=HEAP32[(($9434)>>2)];
      var $9436=$st;
      var $9437=(($9436+4)|0);
      var $9438=$9437;
      var $9439=HEAP16[(($9438)>>1)];
      var $9440=(($9439)&65535);
      var $9441=(($9435+($9440<<2))|0);
      var $9442=$9441;
      var $9443=$9442;
      var $9444=HEAPF32[(($9443)>>2)];
      var $9445=($9432)/($9444);
      var $9446=$2;
      var $9447=(($9446+64)|0);
      var $9448=HEAP32[(($9447)>>2)];
      var $9449=$st;
      var $9450=(($9449+6)|0);
      var $9451=$9450;
      var $9452=HEAP16[(($9451)>>1)];
      var $9453=(($9452)&65535);
      var $9454=(($9448+($9453<<2))|0);
      var $9455=$9454;
      var $9456=$9455;
      HEAPF32[(($9456)>>2)]=$9445;
      __label__ = 380; break;
    case 379: 
      var $9458=$2;
      var $9459=(($9458+64)|0);
      var $9460=HEAP32[(($9459)>>2)];
      var $9461=$st;
      var $9462=(($9461+6)|0);
      var $9463=$9462;
      var $9464=HEAP16[(($9463)>>1)];
      var $9465=(($9464)&65535);
      var $9466=(($9460+($9465<<2))|0);
      var $9467=$9466;
      var $9468=$9467;
      HEAPF32[(($9468)>>2)]=0;
      __label__ = 380; break;
    case 380: 
      __label__ = 487; break;
    case 381: 
      var $9471=$2;
      var $9472=(($9471+64)|0);
      var $9473=HEAP32[(($9472)>>2)];
      var $9474=$st;
      var $9475=(($9474+2)|0);
      var $9476=$9475;
      var $9477=HEAP16[(($9476)>>1)];
      var $9478=(($9477)&65535);
      var $9479=(($9473+($9478<<2))|0);
      var $9480=$9479;
      var $9481=$9480;
      var $9482=HEAPF32[(($9481)>>2)];
      var $9483=$2;
      var $9484=(($9483+64)|0);
      var $9485=HEAP32[(($9484)>>2)];
      var $9486=$st;
      var $9487=(($9486+4)|0);
      var $9488=$9487;
      var $9489=HEAP16[(($9488)>>1)];
      var $9490=(($9489)&65535);
      var $9491=(($9485+($9490<<2))|0);
      var $9492=$9491;
      var $9493=$9492;
      var $9494=HEAPF32[(($9493)>>2)];
      var $9495=($9482)+($9494);
      var $9496=$2;
      var $9497=(($9496+64)|0);
      var $9498=HEAP32[(($9497)>>2)];
      var $9499=$st;
      var $9500=(($9499+6)|0);
      var $9501=$9500;
      var $9502=HEAP16[(($9501)>>1)];
      var $9503=(($9502)&65535);
      var $9504=(($9498+($9503<<2))|0);
      var $9505=$9504;
      var $9506=$9505;
      HEAPF32[(($9506)>>2)]=$9495;
      __label__ = 487; break;
    case 382: 
      var $9508=$2;
      var $9509=(($9508+64)|0);
      var $9510=HEAP32[(($9509)>>2)];
      var $9511=$st;
      var $9512=(($9511+2)|0);
      var $9513=$9512;
      var $9514=HEAP16[(($9513)>>1)];
      var $9515=(($9514)&65535);
      var $9516=(($9510+($9515<<2))|0);
      var $9517=$9516;
      var $9518=$9517;
      var $9519=(($9518)|0);
      var $9520=HEAPF32[(($9519)>>2)];
      var $9521=$2;
      var $9522=(($9521+64)|0);
      var $9523=HEAP32[(($9522)>>2)];
      var $9524=$st;
      var $9525=(($9524+4)|0);
      var $9526=$9525;
      var $9527=HEAP16[(($9526)>>1)];
      var $9528=(($9527)&65535);
      var $9529=(($9523+($9528<<2))|0);
      var $9530=$9529;
      var $9531=$9530;
      var $9532=(($9531)|0);
      var $9533=HEAPF32[(($9532)>>2)];
      var $9534=($9520)+($9533);
      var $9535=$2;
      var $9536=(($9535+64)|0);
      var $9537=HEAP32[(($9536)>>2)];
      var $9538=$st;
      var $9539=(($9538+6)|0);
      var $9540=$9539;
      var $9541=HEAP16[(($9540)>>1)];
      var $9542=(($9541)&65535);
      var $9543=(($9537+($9542<<2))|0);
      var $9544=$9543;
      var $9545=$9544;
      var $9546=(($9545)|0);
      HEAPF32[(($9546)>>2)]=$9534;
      var $9547=$2;
      var $9548=(($9547+64)|0);
      var $9549=HEAP32[(($9548)>>2)];
      var $9550=$st;
      var $9551=(($9550+2)|0);
      var $9552=$9551;
      var $9553=HEAP16[(($9552)>>1)];
      var $9554=(($9553)&65535);
      var $9555=(($9549+($9554<<2))|0);
      var $9556=$9555;
      var $9557=$9556;
      var $9558=(($9557+4)|0);
      var $9559=HEAPF32[(($9558)>>2)];
      var $9560=$2;
      var $9561=(($9560+64)|0);
      var $9562=HEAP32[(($9561)>>2)];
      var $9563=$st;
      var $9564=(($9563+4)|0);
      var $9565=$9564;
      var $9566=HEAP16[(($9565)>>1)];
      var $9567=(($9566)&65535);
      var $9568=(($9562+($9567<<2))|0);
      var $9569=$9568;
      var $9570=$9569;
      var $9571=(($9570+4)|0);
      var $9572=HEAPF32[(($9571)>>2)];
      var $9573=($9559)+($9572);
      var $9574=$2;
      var $9575=(($9574+64)|0);
      var $9576=HEAP32[(($9575)>>2)];
      var $9577=$st;
      var $9578=(($9577+6)|0);
      var $9579=$9578;
      var $9580=HEAP16[(($9579)>>1)];
      var $9581=(($9580)&65535);
      var $9582=(($9576+($9581<<2))|0);
      var $9583=$9582;
      var $9584=$9583;
      var $9585=(($9584+4)|0);
      HEAPF32[(($9585)>>2)]=$9573;
      var $9586=$2;
      var $9587=(($9586+64)|0);
      var $9588=HEAP32[(($9587)>>2)];
      var $9589=$st;
      var $9590=(($9589+2)|0);
      var $9591=$9590;
      var $9592=HEAP16[(($9591)>>1)];
      var $9593=(($9592)&65535);
      var $9594=(($9588+($9593<<2))|0);
      var $9595=$9594;
      var $9596=$9595;
      var $9597=(($9596+8)|0);
      var $9598=HEAPF32[(($9597)>>2)];
      var $9599=$2;
      var $9600=(($9599+64)|0);
      var $9601=HEAP32[(($9600)>>2)];
      var $9602=$st;
      var $9603=(($9602+4)|0);
      var $9604=$9603;
      var $9605=HEAP16[(($9604)>>1)];
      var $9606=(($9605)&65535);
      var $9607=(($9601+($9606<<2))|0);
      var $9608=$9607;
      var $9609=$9608;
      var $9610=(($9609+8)|0);
      var $9611=HEAPF32[(($9610)>>2)];
      var $9612=($9598)+($9611);
      var $9613=$2;
      var $9614=(($9613+64)|0);
      var $9615=HEAP32[(($9614)>>2)];
      var $9616=$st;
      var $9617=(($9616+6)|0);
      var $9618=$9617;
      var $9619=HEAP16[(($9618)>>1)];
      var $9620=(($9619)&65535);
      var $9621=(($9615+($9620<<2))|0);
      var $9622=$9621;
      var $9623=$9622;
      var $9624=(($9623+8)|0);
      HEAPF32[(($9624)>>2)]=$9612;
      __label__ = 487; break;
    case 383: 
      var $9626=$2;
      var $9627=(($9626+64)|0);
      var $9628=HEAP32[(($9627)>>2)];
      var $9629=$st;
      var $9630=(($9629+2)|0);
      var $9631=$9630;
      var $9632=HEAP16[(($9631)>>1)];
      var $9633=(($9632)&65535);
      var $9634=(($9628+($9633<<2))|0);
      var $9635=$9634;
      var $9636=$9635;
      var $9637=HEAPF32[(($9636)>>2)];
      var $9638=$2;
      var $9639=(($9638+64)|0);
      var $9640=HEAP32[(($9639)>>2)];
      var $9641=$st;
      var $9642=(($9641+4)|0);
      var $9643=$9642;
      var $9644=HEAP16[(($9643)>>1)];
      var $9645=(($9644)&65535);
      var $9646=(($9640+($9645<<2))|0);
      var $9647=$9646;
      var $9648=$9647;
      var $9649=HEAPF32[(($9648)>>2)];
      var $9650=($9637)-($9649);
      var $9651=$2;
      var $9652=(($9651+64)|0);
      var $9653=HEAP32[(($9652)>>2)];
      var $9654=$st;
      var $9655=(($9654+6)|0);
      var $9656=$9655;
      var $9657=HEAP16[(($9656)>>1)];
      var $9658=(($9657)&65535);
      var $9659=(($9653+($9658<<2))|0);
      var $9660=$9659;
      var $9661=$9660;
      HEAPF32[(($9661)>>2)]=$9650;
      __label__ = 487; break;
    case 384: 
      var $9663=$2;
      var $9664=(($9663+64)|0);
      var $9665=HEAP32[(($9664)>>2)];
      var $9666=$st;
      var $9667=(($9666+2)|0);
      var $9668=$9667;
      var $9669=HEAP16[(($9668)>>1)];
      var $9670=(($9669)&65535);
      var $9671=(($9665+($9670<<2))|0);
      var $9672=$9671;
      var $9673=$9672;
      var $9674=(($9673)|0);
      var $9675=HEAPF32[(($9674)>>2)];
      var $9676=$2;
      var $9677=(($9676+64)|0);
      var $9678=HEAP32[(($9677)>>2)];
      var $9679=$st;
      var $9680=(($9679+4)|0);
      var $9681=$9680;
      var $9682=HEAP16[(($9681)>>1)];
      var $9683=(($9682)&65535);
      var $9684=(($9678+($9683<<2))|0);
      var $9685=$9684;
      var $9686=$9685;
      var $9687=(($9686)|0);
      var $9688=HEAPF32[(($9687)>>2)];
      var $9689=($9675)-($9688);
      var $9690=$2;
      var $9691=(($9690+64)|0);
      var $9692=HEAP32[(($9691)>>2)];
      var $9693=$st;
      var $9694=(($9693+6)|0);
      var $9695=$9694;
      var $9696=HEAP16[(($9695)>>1)];
      var $9697=(($9696)&65535);
      var $9698=(($9692+($9697<<2))|0);
      var $9699=$9698;
      var $9700=$9699;
      var $9701=(($9700)|0);
      HEAPF32[(($9701)>>2)]=$9689;
      var $9702=$2;
      var $9703=(($9702+64)|0);
      var $9704=HEAP32[(($9703)>>2)];
      var $9705=$st;
      var $9706=(($9705+2)|0);
      var $9707=$9706;
      var $9708=HEAP16[(($9707)>>1)];
      var $9709=(($9708)&65535);
      var $9710=(($9704+($9709<<2))|0);
      var $9711=$9710;
      var $9712=$9711;
      var $9713=(($9712+4)|0);
      var $9714=HEAPF32[(($9713)>>2)];
      var $9715=$2;
      var $9716=(($9715+64)|0);
      var $9717=HEAP32[(($9716)>>2)];
      var $9718=$st;
      var $9719=(($9718+4)|0);
      var $9720=$9719;
      var $9721=HEAP16[(($9720)>>1)];
      var $9722=(($9721)&65535);
      var $9723=(($9717+($9722<<2))|0);
      var $9724=$9723;
      var $9725=$9724;
      var $9726=(($9725+4)|0);
      var $9727=HEAPF32[(($9726)>>2)];
      var $9728=($9714)-($9727);
      var $9729=$2;
      var $9730=(($9729+64)|0);
      var $9731=HEAP32[(($9730)>>2)];
      var $9732=$st;
      var $9733=(($9732+6)|0);
      var $9734=$9733;
      var $9735=HEAP16[(($9734)>>1)];
      var $9736=(($9735)&65535);
      var $9737=(($9731+($9736<<2))|0);
      var $9738=$9737;
      var $9739=$9738;
      var $9740=(($9739+4)|0);
      HEAPF32[(($9740)>>2)]=$9728;
      var $9741=$2;
      var $9742=(($9741+64)|0);
      var $9743=HEAP32[(($9742)>>2)];
      var $9744=$st;
      var $9745=(($9744+2)|0);
      var $9746=$9745;
      var $9747=HEAP16[(($9746)>>1)];
      var $9748=(($9747)&65535);
      var $9749=(($9743+($9748<<2))|0);
      var $9750=$9749;
      var $9751=$9750;
      var $9752=(($9751+8)|0);
      var $9753=HEAPF32[(($9752)>>2)];
      var $9754=$2;
      var $9755=(($9754+64)|0);
      var $9756=HEAP32[(($9755)>>2)];
      var $9757=$st;
      var $9758=(($9757+4)|0);
      var $9759=$9758;
      var $9760=HEAP16[(($9759)>>1)];
      var $9761=(($9760)&65535);
      var $9762=(($9756+($9761<<2))|0);
      var $9763=$9762;
      var $9764=$9763;
      var $9765=(($9764+8)|0);
      var $9766=HEAPF32[(($9765)>>2)];
      var $9767=($9753)-($9766);
      var $9768=$2;
      var $9769=(($9768+64)|0);
      var $9770=HEAP32[(($9769)>>2)];
      var $9771=$st;
      var $9772=(($9771+6)|0);
      var $9773=$9772;
      var $9774=HEAP16[(($9773)>>1)];
      var $9775=(($9774)&65535);
      var $9776=(($9770+($9775<<2))|0);
      var $9777=$9776;
      var $9778=$9777;
      var $9779=(($9778+8)|0);
      HEAPF32[(($9779)>>2)]=$9767;
      __label__ = 487; break;
    case 385: 
      var $9781=$2;
      var $9782=(($9781+64)|0);
      var $9783=HEAP32[(($9782)>>2)];
      var $9784=$st;
      var $9785=(($9784+2)|0);
      var $9786=$9785;
      var $9787=HEAP16[(($9786)>>1)];
      var $9788=(($9787)&65535);
      var $9789=(($9783+($9788<<2))|0);
      var $9790=$9789;
      var $9791=$9790;
      var $9792=HEAPF32[(($9791)>>2)];
      var $9793=$2;
      var $9794=(($9793+64)|0);
      var $9795=HEAP32[(($9794)>>2)];
      var $9796=$st;
      var $9797=(($9796+4)|0);
      var $9798=$9797;
      var $9799=HEAP16[(($9798)>>1)];
      var $9800=(($9799)&65535);
      var $9801=(($9795+($9800<<2))|0);
      var $9802=$9801;
      var $9803=$9802;
      var $9804=HEAPF32[(($9803)>>2)];
      var $9805=$9792 == $9804;
      var $9806=(($9805)&1);
      var $9807=(($9806)|0);
      var $9808=$2;
      var $9809=(($9808+64)|0);
      var $9810=HEAP32[(($9809)>>2)];
      var $9811=$st;
      var $9812=(($9811+6)|0);
      var $9813=$9812;
      var $9814=HEAP16[(($9813)>>1)];
      var $9815=(($9814)&65535);
      var $9816=(($9810+($9815<<2))|0);
      var $9817=$9816;
      var $9818=$9817;
      HEAPF32[(($9818)>>2)]=$9807;
      __label__ = 487; break;
    case 386: 
      var $9820=$2;
      var $9821=(($9820+64)|0);
      var $9822=HEAP32[(($9821)>>2)];
      var $9823=$st;
      var $9824=(($9823+2)|0);
      var $9825=$9824;
      var $9826=HEAP16[(($9825)>>1)];
      var $9827=(($9826)&65535);
      var $9828=(($9822+($9827<<2))|0);
      var $9829=$9828;
      var $9830=$9829;
      var $9831=(($9830)|0);
      var $9832=HEAPF32[(($9831)>>2)];
      var $9833=$2;
      var $9834=(($9833+64)|0);
      var $9835=HEAP32[(($9834)>>2)];
      var $9836=$st;
      var $9837=(($9836+4)|0);
      var $9838=$9837;
      var $9839=HEAP16[(($9838)>>1)];
      var $9840=(($9839)&65535);
      var $9841=(($9835+($9840<<2))|0);
      var $9842=$9841;
      var $9843=$9842;
      var $9844=(($9843)|0);
      var $9845=HEAPF32[(($9844)>>2)];
      var $9846=$9832 == $9845;
      if ($9846) { __label__ = 387; break; } else { var $9904 = 0;__label__ = 389; break; }
    case 387: 
      var $9848=$2;
      var $9849=(($9848+64)|0);
      var $9850=HEAP32[(($9849)>>2)];
      var $9851=$st;
      var $9852=(($9851+2)|0);
      var $9853=$9852;
      var $9854=HEAP16[(($9853)>>1)];
      var $9855=(($9854)&65535);
      var $9856=(($9850+($9855<<2))|0);
      var $9857=$9856;
      var $9858=$9857;
      var $9859=(($9858+4)|0);
      var $9860=HEAPF32[(($9859)>>2)];
      var $9861=$2;
      var $9862=(($9861+64)|0);
      var $9863=HEAP32[(($9862)>>2)];
      var $9864=$st;
      var $9865=(($9864+4)|0);
      var $9866=$9865;
      var $9867=HEAP16[(($9866)>>1)];
      var $9868=(($9867)&65535);
      var $9869=(($9863+($9868<<2))|0);
      var $9870=$9869;
      var $9871=$9870;
      var $9872=(($9871+4)|0);
      var $9873=HEAPF32[(($9872)>>2)];
      var $9874=$9860 == $9873;
      if ($9874) { __label__ = 388; break; } else { var $9904 = 0;__label__ = 389; break; }
    case 388: 
      var $9876=$2;
      var $9877=(($9876+64)|0);
      var $9878=HEAP32[(($9877)>>2)];
      var $9879=$st;
      var $9880=(($9879+2)|0);
      var $9881=$9880;
      var $9882=HEAP16[(($9881)>>1)];
      var $9883=(($9882)&65535);
      var $9884=(($9878+($9883<<2))|0);
      var $9885=$9884;
      var $9886=$9885;
      var $9887=(($9886+8)|0);
      var $9888=HEAPF32[(($9887)>>2)];
      var $9889=$2;
      var $9890=(($9889+64)|0);
      var $9891=HEAP32[(($9890)>>2)];
      var $9892=$st;
      var $9893=(($9892+4)|0);
      var $9894=$9893;
      var $9895=HEAP16[(($9894)>>1)];
      var $9896=(($9895)&65535);
      var $9897=(($9891+($9896<<2))|0);
      var $9898=$9897;
      var $9899=$9898;
      var $9900=(($9899+8)|0);
      var $9901=HEAPF32[(($9900)>>2)];
      var $9902=$9888 == $9901;
      var $9904 = $9902;__label__ = 389; break;
    case 389: 
      var $9904;
      var $9905=(($9904)&1);
      var $9906=(($9905)|0);
      var $9907=$2;
      var $9908=(($9907+64)|0);
      var $9909=HEAP32[(($9908)>>2)];
      var $9910=$st;
      var $9911=(($9910+6)|0);
      var $9912=$9911;
      var $9913=HEAP16[(($9912)>>1)];
      var $9914=(($9913)&65535);
      var $9915=(($9909+($9914<<2))|0);
      var $9916=$9915;
      var $9917=$9916;
      HEAPF32[(($9917)>>2)]=$9906;
      __label__ = 487; break;
    case 390: 
      var $9919=$2;
      var $9920=$2;
      var $9921=(($9920+64)|0);
      var $9922=HEAP32[(($9921)>>2)];
      var $9923=$st;
      var $9924=(($9923+2)|0);
      var $9925=$9924;
      var $9926=HEAP16[(($9925)>>1)];
      var $9927=(($9926)&65535);
      var $9928=(($9922+($9927<<2))|0);
      var $9929=$9928;
      var $9930=$9929;
      var $9931=HEAP32[(($9930)>>2)];
      var $9932=_prog_getstring($9919, $9931);
      var $9933=$2;
      var $9934=$2;
      var $9935=(($9934+64)|0);
      var $9936=HEAP32[(($9935)>>2)];
      var $9937=$st;
      var $9938=(($9937+4)|0);
      var $9939=$9938;
      var $9940=HEAP16[(($9939)>>1)];
      var $9941=(($9940)&65535);
      var $9942=(($9936+($9941<<2))|0);
      var $9943=$9942;
      var $9944=$9943;
      var $9945=HEAP32[(($9944)>>2)];
      var $9946=_prog_getstring($9933, $9945);
      var $9947=_strcmp($9932, $9946);
      var $9948=(($9947)|0)!=0;
      var $9949=$9948 ^ 1;
      var $9950=(($9949)&1);
      var $9951=(($9950)|0);
      var $9952=$2;
      var $9953=(($9952+64)|0);
      var $9954=HEAP32[(($9953)>>2)];
      var $9955=$st;
      var $9956=(($9955+6)|0);
      var $9957=$9956;
      var $9958=HEAP16[(($9957)>>1)];
      var $9959=(($9958)&65535);
      var $9960=(($9954+($9959<<2))|0);
      var $9961=$9960;
      var $9962=$9961;
      HEAPF32[(($9962)>>2)]=$9951;
      __label__ = 487; break;
    case 391: 
      var $9964=$2;
      var $9965=(($9964+64)|0);
      var $9966=HEAP32[(($9965)>>2)];
      var $9967=$st;
      var $9968=(($9967+2)|0);
      var $9969=$9968;
      var $9970=HEAP16[(($9969)>>1)];
      var $9971=(($9970)&65535);
      var $9972=(($9966+($9971<<2))|0);
      var $9973=$9972;
      var $9974=$9973;
      var $9975=HEAP32[(($9974)>>2)];
      var $9976=$2;
      var $9977=(($9976+64)|0);
      var $9978=HEAP32[(($9977)>>2)];
      var $9979=$st;
      var $9980=(($9979+4)|0);
      var $9981=$9980;
      var $9982=HEAP16[(($9981)>>1)];
      var $9983=(($9982)&65535);
      var $9984=(($9978+($9983<<2))|0);
      var $9985=$9984;
      var $9986=$9985;
      var $9987=HEAP32[(($9986)>>2)];
      var $9988=(($9975)|0)==(($9987)|0);
      var $9989=(($9988)&1);
      var $9990=(($9989)|0);
      var $9991=$2;
      var $9992=(($9991+64)|0);
      var $9993=HEAP32[(($9992)>>2)];
      var $9994=$st;
      var $9995=(($9994+6)|0);
      var $9996=$9995;
      var $9997=HEAP16[(($9996)>>1)];
      var $9998=(($9997)&65535);
      var $9999=(($9993+($9998<<2))|0);
      var $10000=$9999;
      var $10001=$10000;
      HEAPF32[(($10001)>>2)]=$9990;
      __label__ = 487; break;
    case 392: 
      var $10003=$2;
      var $10004=(($10003+64)|0);
      var $10005=HEAP32[(($10004)>>2)];
      var $10006=$st;
      var $10007=(($10006+2)|0);
      var $10008=$10007;
      var $10009=HEAP16[(($10008)>>1)];
      var $10010=(($10009)&65535);
      var $10011=(($10005+($10010<<2))|0);
      var $10012=$10011;
      var $10013=$10012;
      var $10014=HEAP32[(($10013)>>2)];
      var $10015=$2;
      var $10016=(($10015+64)|0);
      var $10017=HEAP32[(($10016)>>2)];
      var $10018=$st;
      var $10019=(($10018+4)|0);
      var $10020=$10019;
      var $10021=HEAP16[(($10020)>>1)];
      var $10022=(($10021)&65535);
      var $10023=(($10017+($10022<<2))|0);
      var $10024=$10023;
      var $10025=$10024;
      var $10026=HEAP32[(($10025)>>2)];
      var $10027=(($10014)|0)==(($10026)|0);
      var $10028=(($10027)&1);
      var $10029=(($10028)|0);
      var $10030=$2;
      var $10031=(($10030+64)|0);
      var $10032=HEAP32[(($10031)>>2)];
      var $10033=$st;
      var $10034=(($10033+6)|0);
      var $10035=$10034;
      var $10036=HEAP16[(($10035)>>1)];
      var $10037=(($10036)&65535);
      var $10038=(($10032+($10037<<2))|0);
      var $10039=$10038;
      var $10040=$10039;
      HEAPF32[(($10040)>>2)]=$10029;
      __label__ = 487; break;
    case 393: 
      var $10042=$2;
      var $10043=(($10042+64)|0);
      var $10044=HEAP32[(($10043)>>2)];
      var $10045=$st;
      var $10046=(($10045+2)|0);
      var $10047=$10046;
      var $10048=HEAP16[(($10047)>>1)];
      var $10049=(($10048)&65535);
      var $10050=(($10044+($10049<<2))|0);
      var $10051=$10050;
      var $10052=$10051;
      var $10053=HEAPF32[(($10052)>>2)];
      var $10054=$2;
      var $10055=(($10054+64)|0);
      var $10056=HEAP32[(($10055)>>2)];
      var $10057=$st;
      var $10058=(($10057+4)|0);
      var $10059=$10058;
      var $10060=HEAP16[(($10059)>>1)];
      var $10061=(($10060)&65535);
      var $10062=(($10056+($10061<<2))|0);
      var $10063=$10062;
      var $10064=$10063;
      var $10065=HEAPF32[(($10064)>>2)];
      var $10066=$10053 != $10065;
      var $10067=(($10066)&1);
      var $10068=(($10067)|0);
      var $10069=$2;
      var $10070=(($10069+64)|0);
      var $10071=HEAP32[(($10070)>>2)];
      var $10072=$st;
      var $10073=(($10072+6)|0);
      var $10074=$10073;
      var $10075=HEAP16[(($10074)>>1)];
      var $10076=(($10075)&65535);
      var $10077=(($10071+($10076<<2))|0);
      var $10078=$10077;
      var $10079=$10078;
      HEAPF32[(($10079)>>2)]=$10068;
      __label__ = 487; break;
    case 394: 
      var $10081=$2;
      var $10082=(($10081+64)|0);
      var $10083=HEAP32[(($10082)>>2)];
      var $10084=$st;
      var $10085=(($10084+2)|0);
      var $10086=$10085;
      var $10087=HEAP16[(($10086)>>1)];
      var $10088=(($10087)&65535);
      var $10089=(($10083+($10088<<2))|0);
      var $10090=$10089;
      var $10091=$10090;
      var $10092=(($10091)|0);
      var $10093=HEAPF32[(($10092)>>2)];
      var $10094=$2;
      var $10095=(($10094+64)|0);
      var $10096=HEAP32[(($10095)>>2)];
      var $10097=$st;
      var $10098=(($10097+4)|0);
      var $10099=$10098;
      var $10100=HEAP16[(($10099)>>1)];
      var $10101=(($10100)&65535);
      var $10102=(($10096+($10101<<2))|0);
      var $10103=$10102;
      var $10104=$10103;
      var $10105=(($10104)|0);
      var $10106=HEAPF32[(($10105)>>2)];
      var $10107=$10093 != $10106;
      if ($10107) { var $10165 = 1;__label__ = 397; break; } else { __label__ = 395; break; }
    case 395: 
      var $10109=$2;
      var $10110=(($10109+64)|0);
      var $10111=HEAP32[(($10110)>>2)];
      var $10112=$st;
      var $10113=(($10112+2)|0);
      var $10114=$10113;
      var $10115=HEAP16[(($10114)>>1)];
      var $10116=(($10115)&65535);
      var $10117=(($10111+($10116<<2))|0);
      var $10118=$10117;
      var $10119=$10118;
      var $10120=(($10119+4)|0);
      var $10121=HEAPF32[(($10120)>>2)];
      var $10122=$2;
      var $10123=(($10122+64)|0);
      var $10124=HEAP32[(($10123)>>2)];
      var $10125=$st;
      var $10126=(($10125+4)|0);
      var $10127=$10126;
      var $10128=HEAP16[(($10127)>>1)];
      var $10129=(($10128)&65535);
      var $10130=(($10124+($10129<<2))|0);
      var $10131=$10130;
      var $10132=$10131;
      var $10133=(($10132+4)|0);
      var $10134=HEAPF32[(($10133)>>2)];
      var $10135=$10121 != $10134;
      if ($10135) { var $10165 = 1;__label__ = 397; break; } else { __label__ = 396; break; }
    case 396: 
      var $10137=$2;
      var $10138=(($10137+64)|0);
      var $10139=HEAP32[(($10138)>>2)];
      var $10140=$st;
      var $10141=(($10140+2)|0);
      var $10142=$10141;
      var $10143=HEAP16[(($10142)>>1)];
      var $10144=(($10143)&65535);
      var $10145=(($10139+($10144<<2))|0);
      var $10146=$10145;
      var $10147=$10146;
      var $10148=(($10147+8)|0);
      var $10149=HEAPF32[(($10148)>>2)];
      var $10150=$2;
      var $10151=(($10150+64)|0);
      var $10152=HEAP32[(($10151)>>2)];
      var $10153=$st;
      var $10154=(($10153+4)|0);
      var $10155=$10154;
      var $10156=HEAP16[(($10155)>>1)];
      var $10157=(($10156)&65535);
      var $10158=(($10152+($10157<<2))|0);
      var $10159=$10158;
      var $10160=$10159;
      var $10161=(($10160+8)|0);
      var $10162=HEAPF32[(($10161)>>2)];
      var $10163=$10149 != $10162;
      var $10165 = $10163;__label__ = 397; break;
    case 397: 
      var $10165;
      var $10166=(($10165)&1);
      var $10167=(($10166)|0);
      var $10168=$2;
      var $10169=(($10168+64)|0);
      var $10170=HEAP32[(($10169)>>2)];
      var $10171=$st;
      var $10172=(($10171+6)|0);
      var $10173=$10172;
      var $10174=HEAP16[(($10173)>>1)];
      var $10175=(($10174)&65535);
      var $10176=(($10170+($10175<<2))|0);
      var $10177=$10176;
      var $10178=$10177;
      HEAPF32[(($10178)>>2)]=$10167;
      __label__ = 487; break;
    case 398: 
      var $10180=$2;
      var $10181=$2;
      var $10182=(($10181+64)|0);
      var $10183=HEAP32[(($10182)>>2)];
      var $10184=$st;
      var $10185=(($10184+2)|0);
      var $10186=$10185;
      var $10187=HEAP16[(($10186)>>1)];
      var $10188=(($10187)&65535);
      var $10189=(($10183+($10188<<2))|0);
      var $10190=$10189;
      var $10191=$10190;
      var $10192=HEAP32[(($10191)>>2)];
      var $10193=_prog_getstring($10180, $10192);
      var $10194=$2;
      var $10195=$2;
      var $10196=(($10195+64)|0);
      var $10197=HEAP32[(($10196)>>2)];
      var $10198=$st;
      var $10199=(($10198+4)|0);
      var $10200=$10199;
      var $10201=HEAP16[(($10200)>>1)];
      var $10202=(($10201)&65535);
      var $10203=(($10197+($10202<<2))|0);
      var $10204=$10203;
      var $10205=$10204;
      var $10206=HEAP32[(($10205)>>2)];
      var $10207=_prog_getstring($10194, $10206);
      var $10208=_strcmp($10193, $10207);
      var $10209=(($10208)|0)!=0;
      var $10210=$10209 ^ 1;
      var $10211=$10210 ^ 1;
      var $10212=(($10211)&1);
      var $10213=(($10212)|0);
      var $10214=$2;
      var $10215=(($10214+64)|0);
      var $10216=HEAP32[(($10215)>>2)];
      var $10217=$st;
      var $10218=(($10217+6)|0);
      var $10219=$10218;
      var $10220=HEAP16[(($10219)>>1)];
      var $10221=(($10220)&65535);
      var $10222=(($10216+($10221<<2))|0);
      var $10223=$10222;
      var $10224=$10223;
      HEAPF32[(($10224)>>2)]=$10213;
      __label__ = 487; break;
    case 399: 
      var $10226=$2;
      var $10227=(($10226+64)|0);
      var $10228=HEAP32[(($10227)>>2)];
      var $10229=$st;
      var $10230=(($10229+2)|0);
      var $10231=$10230;
      var $10232=HEAP16[(($10231)>>1)];
      var $10233=(($10232)&65535);
      var $10234=(($10228+($10233<<2))|0);
      var $10235=$10234;
      var $10236=$10235;
      var $10237=HEAP32[(($10236)>>2)];
      var $10238=$2;
      var $10239=(($10238+64)|0);
      var $10240=HEAP32[(($10239)>>2)];
      var $10241=$st;
      var $10242=(($10241+4)|0);
      var $10243=$10242;
      var $10244=HEAP16[(($10243)>>1)];
      var $10245=(($10244)&65535);
      var $10246=(($10240+($10245<<2))|0);
      var $10247=$10246;
      var $10248=$10247;
      var $10249=HEAP32[(($10248)>>2)];
      var $10250=(($10237)|0)!=(($10249)|0);
      var $10251=(($10250)&1);
      var $10252=(($10251)|0);
      var $10253=$2;
      var $10254=(($10253+64)|0);
      var $10255=HEAP32[(($10254)>>2)];
      var $10256=$st;
      var $10257=(($10256+6)|0);
      var $10258=$10257;
      var $10259=HEAP16[(($10258)>>1)];
      var $10260=(($10259)&65535);
      var $10261=(($10255+($10260<<2))|0);
      var $10262=$10261;
      var $10263=$10262;
      HEAPF32[(($10263)>>2)]=$10252;
      __label__ = 487; break;
    case 400: 
      var $10265=$2;
      var $10266=(($10265+64)|0);
      var $10267=HEAP32[(($10266)>>2)];
      var $10268=$st;
      var $10269=(($10268+2)|0);
      var $10270=$10269;
      var $10271=HEAP16[(($10270)>>1)];
      var $10272=(($10271)&65535);
      var $10273=(($10267+($10272<<2))|0);
      var $10274=$10273;
      var $10275=$10274;
      var $10276=HEAP32[(($10275)>>2)];
      var $10277=$2;
      var $10278=(($10277+64)|0);
      var $10279=HEAP32[(($10278)>>2)];
      var $10280=$st;
      var $10281=(($10280+4)|0);
      var $10282=$10281;
      var $10283=HEAP16[(($10282)>>1)];
      var $10284=(($10283)&65535);
      var $10285=(($10279+($10284<<2))|0);
      var $10286=$10285;
      var $10287=$10286;
      var $10288=HEAP32[(($10287)>>2)];
      var $10289=(($10276)|0)!=(($10288)|0);
      var $10290=(($10289)&1);
      var $10291=(($10290)|0);
      var $10292=$2;
      var $10293=(($10292+64)|0);
      var $10294=HEAP32[(($10293)>>2)];
      var $10295=$st;
      var $10296=(($10295+6)|0);
      var $10297=$10296;
      var $10298=HEAP16[(($10297)>>1)];
      var $10299=(($10298)&65535);
      var $10300=(($10294+($10299<<2))|0);
      var $10301=$10300;
      var $10302=$10301;
      HEAPF32[(($10302)>>2)]=$10291;
      __label__ = 487; break;
    case 401: 
      var $10304=$2;
      var $10305=(($10304+64)|0);
      var $10306=HEAP32[(($10305)>>2)];
      var $10307=$st;
      var $10308=(($10307+2)|0);
      var $10309=$10308;
      var $10310=HEAP16[(($10309)>>1)];
      var $10311=(($10310)&65535);
      var $10312=(($10306+($10311<<2))|0);
      var $10313=$10312;
      var $10314=$10313;
      var $10315=HEAPF32[(($10314)>>2)];
      var $10316=$2;
      var $10317=(($10316+64)|0);
      var $10318=HEAP32[(($10317)>>2)];
      var $10319=$st;
      var $10320=(($10319+4)|0);
      var $10321=$10320;
      var $10322=HEAP16[(($10321)>>1)];
      var $10323=(($10322)&65535);
      var $10324=(($10318+($10323<<2))|0);
      var $10325=$10324;
      var $10326=$10325;
      var $10327=HEAPF32[(($10326)>>2)];
      var $10328=$10315 <= $10327;
      var $10329=(($10328)&1);
      var $10330=(($10329)|0);
      var $10331=$2;
      var $10332=(($10331+64)|0);
      var $10333=HEAP32[(($10332)>>2)];
      var $10334=$st;
      var $10335=(($10334+6)|0);
      var $10336=$10335;
      var $10337=HEAP16[(($10336)>>1)];
      var $10338=(($10337)&65535);
      var $10339=(($10333+($10338<<2))|0);
      var $10340=$10339;
      var $10341=$10340;
      HEAPF32[(($10341)>>2)]=$10330;
      __label__ = 487; break;
    case 402: 
      var $10343=$2;
      var $10344=(($10343+64)|0);
      var $10345=HEAP32[(($10344)>>2)];
      var $10346=$st;
      var $10347=(($10346+2)|0);
      var $10348=$10347;
      var $10349=HEAP16[(($10348)>>1)];
      var $10350=(($10349)&65535);
      var $10351=(($10345+($10350<<2))|0);
      var $10352=$10351;
      var $10353=$10352;
      var $10354=HEAPF32[(($10353)>>2)];
      var $10355=$2;
      var $10356=(($10355+64)|0);
      var $10357=HEAP32[(($10356)>>2)];
      var $10358=$st;
      var $10359=(($10358+4)|0);
      var $10360=$10359;
      var $10361=HEAP16[(($10360)>>1)];
      var $10362=(($10361)&65535);
      var $10363=(($10357+($10362<<2))|0);
      var $10364=$10363;
      var $10365=$10364;
      var $10366=HEAPF32[(($10365)>>2)];
      var $10367=$10354 >= $10366;
      var $10368=(($10367)&1);
      var $10369=(($10368)|0);
      var $10370=$2;
      var $10371=(($10370+64)|0);
      var $10372=HEAP32[(($10371)>>2)];
      var $10373=$st;
      var $10374=(($10373+6)|0);
      var $10375=$10374;
      var $10376=HEAP16[(($10375)>>1)];
      var $10377=(($10376)&65535);
      var $10378=(($10372+($10377<<2))|0);
      var $10379=$10378;
      var $10380=$10379;
      HEAPF32[(($10380)>>2)]=$10369;
      __label__ = 487; break;
    case 403: 
      var $10382=$2;
      var $10383=(($10382+64)|0);
      var $10384=HEAP32[(($10383)>>2)];
      var $10385=$st;
      var $10386=(($10385+2)|0);
      var $10387=$10386;
      var $10388=HEAP16[(($10387)>>1)];
      var $10389=(($10388)&65535);
      var $10390=(($10384+($10389<<2))|0);
      var $10391=$10390;
      var $10392=$10391;
      var $10393=HEAPF32[(($10392)>>2)];
      var $10394=$2;
      var $10395=(($10394+64)|0);
      var $10396=HEAP32[(($10395)>>2)];
      var $10397=$st;
      var $10398=(($10397+4)|0);
      var $10399=$10398;
      var $10400=HEAP16[(($10399)>>1)];
      var $10401=(($10400)&65535);
      var $10402=(($10396+($10401<<2))|0);
      var $10403=$10402;
      var $10404=$10403;
      var $10405=HEAPF32[(($10404)>>2)];
      var $10406=$10393 < $10405;
      var $10407=(($10406)&1);
      var $10408=(($10407)|0);
      var $10409=$2;
      var $10410=(($10409+64)|0);
      var $10411=HEAP32[(($10410)>>2)];
      var $10412=$st;
      var $10413=(($10412+6)|0);
      var $10414=$10413;
      var $10415=HEAP16[(($10414)>>1)];
      var $10416=(($10415)&65535);
      var $10417=(($10411+($10416<<2))|0);
      var $10418=$10417;
      var $10419=$10418;
      HEAPF32[(($10419)>>2)]=$10408;
      __label__ = 487; break;
    case 404: 
      var $10421=$2;
      var $10422=(($10421+64)|0);
      var $10423=HEAP32[(($10422)>>2)];
      var $10424=$st;
      var $10425=(($10424+2)|0);
      var $10426=$10425;
      var $10427=HEAP16[(($10426)>>1)];
      var $10428=(($10427)&65535);
      var $10429=(($10423+($10428<<2))|0);
      var $10430=$10429;
      var $10431=$10430;
      var $10432=HEAPF32[(($10431)>>2)];
      var $10433=$2;
      var $10434=(($10433+64)|0);
      var $10435=HEAP32[(($10434)>>2)];
      var $10436=$st;
      var $10437=(($10436+4)|0);
      var $10438=$10437;
      var $10439=HEAP16[(($10438)>>1)];
      var $10440=(($10439)&65535);
      var $10441=(($10435+($10440<<2))|0);
      var $10442=$10441;
      var $10443=$10442;
      var $10444=HEAPF32[(($10443)>>2)];
      var $10445=$10432 > $10444;
      var $10446=(($10445)&1);
      var $10447=(($10446)|0);
      var $10448=$2;
      var $10449=(($10448+64)|0);
      var $10450=HEAP32[(($10449)>>2)];
      var $10451=$st;
      var $10452=(($10451+6)|0);
      var $10453=$10452;
      var $10454=HEAP16[(($10453)>>1)];
      var $10455=(($10454)&65535);
      var $10456=(($10450+($10455<<2))|0);
      var $10457=$10456;
      var $10458=$10457;
      HEAPF32[(($10458)>>2)]=$10447;
      __label__ = 487; break;
    case 405: 
      var $10460=$2;
      var $10461=(($10460+64)|0);
      var $10462=HEAP32[(($10461)>>2)];
      var $10463=$st;
      var $10464=(($10463+2)|0);
      var $10465=$10464;
      var $10466=HEAP16[(($10465)>>1)];
      var $10467=(($10466)&65535);
      var $10468=(($10462+($10467<<2))|0);
      var $10469=$10468;
      var $10470=$10469;
      var $10471=HEAP32[(($10470)>>2)];
      var $10472=(($10471)|0) < 0;
      if ($10472) { __label__ = 407; break; } else { __label__ = 406; break; }
    case 406: 
      var $10474=$2;
      var $10475=(($10474+64)|0);
      var $10476=HEAP32[(($10475)>>2)];
      var $10477=$st;
      var $10478=(($10477+2)|0);
      var $10479=$10478;
      var $10480=HEAP16[(($10479)>>1)];
      var $10481=(($10480)&65535);
      var $10482=(($10476+($10481<<2))|0);
      var $10483=$10482;
      var $10484=$10483;
      var $10485=HEAP32[(($10484)>>2)];
      var $10486=$2;
      var $10487=(($10486+140)|0);
      var $10488=HEAP32[(($10487)>>2)];
      var $10489=(($10485)|0) >= (($10488)|0);
      if ($10489) { __label__ = 407; break; } else { __label__ = 408; break; }
    case 407: 
      var $10491=$2;
      var $10492=$2;
      var $10493=(($10492)|0);
      var $10494=HEAP32[(($10493)>>2)];
      _qcvmerror($10491, ((STRING_TABLE.__str17)|0), (tempInt=STACKTOP,STACKTOP += 4,assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),HEAP32[((tempInt)>>2)]=$10494,tempInt));
      __label__ = 488; break;
    case 408: 
      var $10496=$2;
      var $10497=(($10496+64)|0);
      var $10498=HEAP32[(($10497)>>2)];
      var $10499=$st;
      var $10500=(($10499+4)|0);
      var $10501=$10500;
      var $10502=HEAP16[(($10501)>>1)];
      var $10503=(($10502)&65535);
      var $10504=(($10498+($10503<<2))|0);
      var $10505=$10504;
      var $10506=$10505;
      var $10507=HEAP32[(($10506)>>2)];
      var $10508=$2;
      var $10509=(($10508+144)|0);
      var $10510=HEAP32[(($10509)>>2)];
      var $10511=(($10507)>>>0) >= (($10510)>>>0);
      if ($10511) { __label__ = 409; break; } else { __label__ = 410; break; }
    case 409: 
      var $10513=$2;
      var $10514=$2;
      var $10515=(($10514)|0);
      var $10516=HEAP32[(($10515)>>2)];
      var $10517=$2;
      var $10518=(($10517+64)|0);
      var $10519=HEAP32[(($10518)>>2)];
      var $10520=$st;
      var $10521=(($10520+4)|0);
      var $10522=$10521;
      var $10523=HEAP16[(($10522)>>1)];
      var $10524=(($10523)&65535);
      var $10525=(($10519+($10524<<2))|0);
      var $10526=$10525;
      var $10527=$10526;
      var $10528=HEAP32[(($10527)>>2)];
      _qcvmerror($10513, ((STRING_TABLE.__str18)|0), (tempInt=STACKTOP,STACKTOP += 8,assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),HEAP32[((tempInt)>>2)]=$10516,HEAP32[(((tempInt)+(4))>>2)]=$10528,tempInt));
      __label__ = 488; break;
    case 410: 
      var $10530=$2;
      var $10531=$2;
      var $10532=(($10531+64)|0);
      var $10533=HEAP32[(($10532)>>2)];
      var $10534=$st;
      var $10535=(($10534+2)|0);
      var $10536=$10535;
      var $10537=HEAP16[(($10536)>>1)];
      var $10538=(($10537)&65535);
      var $10539=(($10533+($10538<<2))|0);
      var $10540=$10539;
      var $10541=$10540;
      var $10542=HEAP32[(($10541)>>2)];
      var $10543=_prog_getedict($10530, $10542);
      $ed10=$10543;
      var $10544=$ed10;
      var $10545=$10544;
      var $10546=$2;
      var $10547=(($10546+64)|0);
      var $10548=HEAP32[(($10547)>>2)];
      var $10549=$st;
      var $10550=(($10549+4)|0);
      var $10551=$10550;
      var $10552=HEAP16[(($10551)>>1)];
      var $10553=(($10552)&65535);
      var $10554=(($10548+($10553<<2))|0);
      var $10555=$10554;
      var $10556=$10555;
      var $10557=HEAP32[(($10556)>>2)];
      var $10558=(($10545+($10557<<2))|0);
      var $10559=$10558;
      var $10560=$10559;
      var $10561=HEAP32[(($10560)>>2)];
      var $10562=$2;
      var $10563=(($10562+64)|0);
      var $10564=HEAP32[(($10563)>>2)];
      var $10565=$st;
      var $10566=(($10565+6)|0);
      var $10567=$10566;
      var $10568=HEAP16[(($10567)>>1)];
      var $10569=(($10568)&65535);
      var $10570=(($10564+($10569<<2))|0);
      var $10571=$10570;
      var $10572=$10571;
      HEAP32[(($10572)>>2)]=$10561;
      __label__ = 487; break;
    case 411: 
      var $10574=$2;
      var $10575=(($10574+64)|0);
      var $10576=HEAP32[(($10575)>>2)];
      var $10577=$st;
      var $10578=(($10577+2)|0);
      var $10579=$10578;
      var $10580=HEAP16[(($10579)>>1)];
      var $10581=(($10580)&65535);
      var $10582=(($10576+($10581<<2))|0);
      var $10583=$10582;
      var $10584=$10583;
      var $10585=HEAP32[(($10584)>>2)];
      var $10586=(($10585)|0) < 0;
      if ($10586) { __label__ = 413; break; } else { __label__ = 412; break; }
    case 412: 
      var $10588=$2;
      var $10589=(($10588+64)|0);
      var $10590=HEAP32[(($10589)>>2)];
      var $10591=$st;
      var $10592=(($10591+2)|0);
      var $10593=$10592;
      var $10594=HEAP16[(($10593)>>1)];
      var $10595=(($10594)&65535);
      var $10596=(($10590+($10595<<2))|0);
      var $10597=$10596;
      var $10598=$10597;
      var $10599=HEAP32[(($10598)>>2)];
      var $10600=$2;
      var $10601=(($10600+140)|0);
      var $10602=HEAP32[(($10601)>>2)];
      var $10603=(($10599)|0) >= (($10602)|0);
      if ($10603) { __label__ = 413; break; } else { __label__ = 414; break; }
    case 413: 
      var $10605=$2;
      var $10606=$2;
      var $10607=(($10606)|0);
      var $10608=HEAP32[(($10607)>>2)];
      _qcvmerror($10605, ((STRING_TABLE.__str17)|0), (tempInt=STACKTOP,STACKTOP += 4,assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),HEAP32[((tempInt)>>2)]=$10608,tempInt));
      __label__ = 488; break;
    case 414: 
      var $10610=$2;
      var $10611=(($10610+64)|0);
      var $10612=HEAP32[(($10611)>>2)];
      var $10613=$st;
      var $10614=(($10613+4)|0);
      var $10615=$10614;
      var $10616=HEAP16[(($10615)>>1)];
      var $10617=(($10616)&65535);
      var $10618=(($10612+($10617<<2))|0);
      var $10619=$10618;
      var $10620=$10619;
      var $10621=HEAP32[(($10620)>>2)];
      var $10622=(($10621)|0) < 0;
      if ($10622) { __label__ = 416; break; } else { __label__ = 415; break; }
    case 415: 
      var $10624=$2;
      var $10625=(($10624+64)|0);
      var $10626=HEAP32[(($10625)>>2)];
      var $10627=$st;
      var $10628=(($10627+4)|0);
      var $10629=$10628;
      var $10630=HEAP16[(($10629)>>1)];
      var $10631=(($10630)&65535);
      var $10632=(($10626+($10631<<2))|0);
      var $10633=$10632;
      var $10634=$10633;
      var $10635=HEAP32[(($10634)>>2)];
      var $10636=((($10635)+(3))|0);
      var $10637=$2;
      var $10638=(($10637+144)|0);
      var $10639=HEAP32[(($10638)>>2)];
      var $10640=(($10636)>>>0) > (($10639)>>>0);
      if ($10640) { __label__ = 416; break; } else { __label__ = 417; break; }
    case 416: 
      var $10642=$2;
      var $10643=$2;
      var $10644=(($10643)|0);
      var $10645=HEAP32[(($10644)>>2)];
      var $10646=$2;
      var $10647=(($10646+64)|0);
      var $10648=HEAP32[(($10647)>>2)];
      var $10649=$st;
      var $10650=(($10649+4)|0);
      var $10651=$10650;
      var $10652=HEAP16[(($10651)>>1)];
      var $10653=(($10652)&65535);
      var $10654=(($10648+($10653<<2))|0);
      var $10655=$10654;
      var $10656=$10655;
      var $10657=HEAP32[(($10656)>>2)];
      var $10658=((($10657)+(2))|0);
      _qcvmerror($10642, ((STRING_TABLE.__str18)|0), (tempInt=STACKTOP,STACKTOP += 8,assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),HEAP32[((tempInt)>>2)]=$10645,HEAP32[(((tempInt)+(4))>>2)]=$10658,tempInt));
      __label__ = 488; break;
    case 417: 
      var $10660=$2;
      var $10661=$2;
      var $10662=(($10661+64)|0);
      var $10663=HEAP32[(($10662)>>2)];
      var $10664=$st;
      var $10665=(($10664+2)|0);
      var $10666=$10665;
      var $10667=HEAP16[(($10666)>>1)];
      var $10668=(($10667)&65535);
      var $10669=(($10663+($10668<<2))|0);
      var $10670=$10669;
      var $10671=$10670;
      var $10672=HEAP32[(($10671)>>2)];
      var $10673=_prog_getedict($10660, $10672);
      $ed10=$10673;
      var $10674=$ed10;
      var $10675=$10674;
      var $10676=$2;
      var $10677=(($10676+64)|0);
      var $10678=HEAP32[(($10677)>>2)];
      var $10679=$st;
      var $10680=(($10679+4)|0);
      var $10681=$10680;
      var $10682=HEAP16[(($10681)>>1)];
      var $10683=(($10682)&65535);
      var $10684=(($10678+($10683<<2))|0);
      var $10685=$10684;
      var $10686=$10685;
      var $10687=HEAP32[(($10686)>>2)];
      var $10688=(($10675+($10687<<2))|0);
      var $10689=$10688;
      var $10690=$10689;
      var $10691=(($10690)|0);
      var $10692=HEAP32[(($10691)>>2)];
      var $10693=$2;
      var $10694=(($10693+64)|0);
      var $10695=HEAP32[(($10694)>>2)];
      var $10696=$st;
      var $10697=(($10696+6)|0);
      var $10698=$10697;
      var $10699=HEAP16[(($10698)>>1)];
      var $10700=(($10699)&65535);
      var $10701=(($10695+($10700<<2))|0);
      var $10702=$10701;
      var $10703=$10702;
      var $10704=(($10703)|0);
      HEAP32[(($10704)>>2)]=$10692;
      var $10705=$ed10;
      var $10706=$10705;
      var $10707=$2;
      var $10708=(($10707+64)|0);
      var $10709=HEAP32[(($10708)>>2)];
      var $10710=$st;
      var $10711=(($10710+4)|0);
      var $10712=$10711;
      var $10713=HEAP16[(($10712)>>1)];
      var $10714=(($10713)&65535);
      var $10715=(($10709+($10714<<2))|0);
      var $10716=$10715;
      var $10717=$10716;
      var $10718=HEAP32[(($10717)>>2)];
      var $10719=(($10706+($10718<<2))|0);
      var $10720=$10719;
      var $10721=$10720;
      var $10722=(($10721+4)|0);
      var $10723=HEAP32[(($10722)>>2)];
      var $10724=$2;
      var $10725=(($10724+64)|0);
      var $10726=HEAP32[(($10725)>>2)];
      var $10727=$st;
      var $10728=(($10727+6)|0);
      var $10729=$10728;
      var $10730=HEAP16[(($10729)>>1)];
      var $10731=(($10730)&65535);
      var $10732=(($10726+($10731<<2))|0);
      var $10733=$10732;
      var $10734=$10733;
      var $10735=(($10734+4)|0);
      HEAP32[(($10735)>>2)]=$10723;
      var $10736=$ed10;
      var $10737=$10736;
      var $10738=$2;
      var $10739=(($10738+64)|0);
      var $10740=HEAP32[(($10739)>>2)];
      var $10741=$st;
      var $10742=(($10741+4)|0);
      var $10743=$10742;
      var $10744=HEAP16[(($10743)>>1)];
      var $10745=(($10744)&65535);
      var $10746=(($10740+($10745<<2))|0);
      var $10747=$10746;
      var $10748=$10747;
      var $10749=HEAP32[(($10748)>>2)];
      var $10750=(($10737+($10749<<2))|0);
      var $10751=$10750;
      var $10752=$10751;
      var $10753=(($10752+8)|0);
      var $10754=HEAP32[(($10753)>>2)];
      var $10755=$2;
      var $10756=(($10755+64)|0);
      var $10757=HEAP32[(($10756)>>2)];
      var $10758=$st;
      var $10759=(($10758+6)|0);
      var $10760=$10759;
      var $10761=HEAP16[(($10760)>>1)];
      var $10762=(($10761)&65535);
      var $10763=(($10757+($10762<<2))|0);
      var $10764=$10763;
      var $10765=$10764;
      var $10766=(($10765+8)|0);
      HEAP32[(($10766)>>2)]=$10754;
      __label__ = 487; break;
    case 418: 
      var $10768=$2;
      var $10769=(($10768+64)|0);
      var $10770=HEAP32[(($10769)>>2)];
      var $10771=$st;
      var $10772=(($10771+2)|0);
      var $10773=$10772;
      var $10774=HEAP16[(($10773)>>1)];
      var $10775=(($10774)&65535);
      var $10776=(($10770+($10775<<2))|0);
      var $10777=$10776;
      var $10778=$10777;
      var $10779=HEAP32[(($10778)>>2)];
      var $10780=(($10779)|0) < 0;
      if ($10780) { __label__ = 420; break; } else { __label__ = 419; break; }
    case 419: 
      var $10782=$2;
      var $10783=(($10782+64)|0);
      var $10784=HEAP32[(($10783)>>2)];
      var $10785=$st;
      var $10786=(($10785+2)|0);
      var $10787=$10786;
      var $10788=HEAP16[(($10787)>>1)];
      var $10789=(($10788)&65535);
      var $10790=(($10784+($10789<<2))|0);
      var $10791=$10790;
      var $10792=$10791;
      var $10793=HEAP32[(($10792)>>2)];
      var $10794=$2;
      var $10795=(($10794+140)|0);
      var $10796=HEAP32[(($10795)>>2)];
      var $10797=(($10793)|0) >= (($10796)|0);
      if ($10797) { __label__ = 420; break; } else { __label__ = 421; break; }
    case 420: 
      var $10799=$2;
      var $10800=$2;
      var $10801=(($10800)|0);
      var $10802=HEAP32[(($10801)>>2)];
      var $10803=$2;
      var $10804=(($10803+64)|0);
      var $10805=HEAP32[(($10804)>>2)];
      var $10806=$st;
      var $10807=(($10806+2)|0);
      var $10808=$10807;
      var $10809=HEAP16[(($10808)>>1)];
      var $10810=(($10809)&65535);
      var $10811=(($10805+($10810<<2))|0);
      var $10812=$10811;
      var $10813=$10812;
      var $10814=HEAP32[(($10813)>>2)];
      _qcvmerror($10799, ((STRING_TABLE.__str19)|0), (tempInt=STACKTOP,STACKTOP += 8,assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),HEAP32[((tempInt)>>2)]=$10802,HEAP32[(((tempInt)+(4))>>2)]=$10814,tempInt));
      __label__ = 488; break;
    case 421: 
      var $10816=$2;
      var $10817=(($10816+64)|0);
      var $10818=HEAP32[(($10817)>>2)];
      var $10819=$st;
      var $10820=(($10819+4)|0);
      var $10821=$10820;
      var $10822=HEAP16[(($10821)>>1)];
      var $10823=(($10822)&65535);
      var $10824=(($10818+($10823<<2))|0);
      var $10825=$10824;
      var $10826=$10825;
      var $10827=HEAP32[(($10826)>>2)];
      var $10828=$2;
      var $10829=(($10828+144)|0);
      var $10830=HEAP32[(($10829)>>2)];
      var $10831=(($10827)>>>0) >= (($10830)>>>0);
      if ($10831) { __label__ = 422; break; } else { __label__ = 423; break; }
    case 422: 
      var $10833=$2;
      var $10834=$2;
      var $10835=(($10834)|0);
      var $10836=HEAP32[(($10835)>>2)];
      var $10837=$2;
      var $10838=(($10837+64)|0);
      var $10839=HEAP32[(($10838)>>2)];
      var $10840=$st;
      var $10841=(($10840+4)|0);
      var $10842=$10841;
      var $10843=HEAP16[(($10842)>>1)];
      var $10844=(($10843)&65535);
      var $10845=(($10839+($10844<<2))|0);
      var $10846=$10845;
      var $10847=$10846;
      var $10848=HEAP32[(($10847)>>2)];
      _qcvmerror($10833, ((STRING_TABLE.__str18)|0), (tempInt=STACKTOP,STACKTOP += 8,assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),HEAP32[((tempInt)>>2)]=$10836,HEAP32[(((tempInt)+(4))>>2)]=$10848,tempInt));
      __label__ = 488; break;
    case 423: 
      var $10850=$2;
      var $10851=$2;
      var $10852=(($10851+64)|0);
      var $10853=HEAP32[(($10852)>>2)];
      var $10854=$st;
      var $10855=(($10854+2)|0);
      var $10856=$10855;
      var $10857=HEAP16[(($10856)>>1)];
      var $10858=(($10857)&65535);
      var $10859=(($10853+($10858<<2))|0);
      var $10860=$10859;
      var $10861=$10860;
      var $10862=HEAP32[(($10861)>>2)];
      var $10863=_prog_getedict($10850, $10862);
      $ed10=$10863;
      var $10864=$ed10;
      var $10865=$10864;
      var $10866=$2;
      var $10867=(($10866+76)|0);
      var $10868=HEAP32[(($10867)>>2)];
      var $10869=$10865;
      var $10870=$10868;
      var $10871=((($10869)-($10870))|0);
      var $10872=((((($10871)|0))/(4))&-1);
      var $10873=$2;
      var $10874=(($10873+64)|0);
      var $10875=HEAP32[(($10874)>>2)];
      var $10876=$st;
      var $10877=(($10876+6)|0);
      var $10878=$10877;
      var $10879=HEAP16[(($10878)>>1)];
      var $10880=(($10879)&65535);
      var $10881=(($10875+($10880<<2))|0);
      var $10882=$10881;
      var $10883=$10882;
      HEAP32[(($10883)>>2)]=$10872;
      var $10884=$2;
      var $10885=(($10884+64)|0);
      var $10886=HEAP32[(($10885)>>2)];
      var $10887=$st;
      var $10888=(($10887+4)|0);
      var $10889=$10888;
      var $10890=HEAP16[(($10889)>>1)];
      var $10891=(($10890)&65535);
      var $10892=(($10886+($10891<<2))|0);
      var $10893=$10892;
      var $10894=$10893;
      var $10895=HEAP32[(($10894)>>2)];
      var $10896=$2;
      var $10897=(($10896+64)|0);
      var $10898=HEAP32[(($10897)>>2)];
      var $10899=$st;
      var $10900=(($10899+6)|0);
      var $10901=$10900;
      var $10902=HEAP16[(($10901)>>1)];
      var $10903=(($10902)&65535);
      var $10904=(($10898+($10903<<2))|0);
      var $10905=$10904;
      var $10906=$10905;
      var $10907=HEAP32[(($10906)>>2)];
      var $10908=((($10907)+($10895))|0);
      HEAP32[(($10906)>>2)]=$10908;
      __label__ = 487; break;
    case 424: 
      var $10910=$2;
      var $10911=(($10910+64)|0);
      var $10912=HEAP32[(($10911)>>2)];
      var $10913=$st;
      var $10914=(($10913+2)|0);
      var $10915=$10914;
      var $10916=HEAP16[(($10915)>>1)];
      var $10917=(($10916)&65535);
      var $10918=(($10912+($10917<<2))|0);
      var $10919=$10918;
      var $10920=$10919;
      var $10921=HEAP32[(($10920)>>2)];
      var $10922=$2;
      var $10923=(($10922+64)|0);
      var $10924=HEAP32[(($10923)>>2)];
      var $10925=$st;
      var $10926=(($10925+4)|0);
      var $10927=$10926;
      var $10928=HEAP16[(($10927)>>1)];
      var $10929=(($10928)&65535);
      var $10930=(($10924+($10929<<2))|0);
      var $10931=$10930;
      var $10932=$10931;
      HEAP32[(($10932)>>2)]=$10921;
      __label__ = 487; break;
    case 425: 
      var $10934=$2;
      var $10935=(($10934+64)|0);
      var $10936=HEAP32[(($10935)>>2)];
      var $10937=$st;
      var $10938=(($10937+2)|0);
      var $10939=$10938;
      var $10940=HEAP16[(($10939)>>1)];
      var $10941=(($10940)&65535);
      var $10942=(($10936+($10941<<2))|0);
      var $10943=$10942;
      var $10944=$10943;
      var $10945=(($10944)|0);
      var $10946=HEAP32[(($10945)>>2)];
      var $10947=$2;
      var $10948=(($10947+64)|0);
      var $10949=HEAP32[(($10948)>>2)];
      var $10950=$st;
      var $10951=(($10950+4)|0);
      var $10952=$10951;
      var $10953=HEAP16[(($10952)>>1)];
      var $10954=(($10953)&65535);
      var $10955=(($10949+($10954<<2))|0);
      var $10956=$10955;
      var $10957=$10956;
      var $10958=(($10957)|0);
      HEAP32[(($10958)>>2)]=$10946;
      var $10959=$2;
      var $10960=(($10959+64)|0);
      var $10961=HEAP32[(($10960)>>2)];
      var $10962=$st;
      var $10963=(($10962+2)|0);
      var $10964=$10963;
      var $10965=HEAP16[(($10964)>>1)];
      var $10966=(($10965)&65535);
      var $10967=(($10961+($10966<<2))|0);
      var $10968=$10967;
      var $10969=$10968;
      var $10970=(($10969+4)|0);
      var $10971=HEAP32[(($10970)>>2)];
      var $10972=$2;
      var $10973=(($10972+64)|0);
      var $10974=HEAP32[(($10973)>>2)];
      var $10975=$st;
      var $10976=(($10975+4)|0);
      var $10977=$10976;
      var $10978=HEAP16[(($10977)>>1)];
      var $10979=(($10978)&65535);
      var $10980=(($10974+($10979<<2))|0);
      var $10981=$10980;
      var $10982=$10981;
      var $10983=(($10982+4)|0);
      HEAP32[(($10983)>>2)]=$10971;
      var $10984=$2;
      var $10985=(($10984+64)|0);
      var $10986=HEAP32[(($10985)>>2)];
      var $10987=$st;
      var $10988=(($10987+2)|0);
      var $10989=$10988;
      var $10990=HEAP16[(($10989)>>1)];
      var $10991=(($10990)&65535);
      var $10992=(($10986+($10991<<2))|0);
      var $10993=$10992;
      var $10994=$10993;
      var $10995=(($10994+8)|0);
      var $10996=HEAP32[(($10995)>>2)];
      var $10997=$2;
      var $10998=(($10997+64)|0);
      var $10999=HEAP32[(($10998)>>2)];
      var $11000=$st;
      var $11001=(($11000+4)|0);
      var $11002=$11001;
      var $11003=HEAP16[(($11002)>>1)];
      var $11004=(($11003)&65535);
      var $11005=(($10999+($11004<<2))|0);
      var $11006=$11005;
      var $11007=$11006;
      var $11008=(($11007+8)|0);
      HEAP32[(($11008)>>2)]=$10996;
      __label__ = 487; break;
    case 426: 
      var $11010=$2;
      var $11011=(($11010+64)|0);
      var $11012=HEAP32[(($11011)>>2)];
      var $11013=$st;
      var $11014=(($11013+4)|0);
      var $11015=$11014;
      var $11016=HEAP16[(($11015)>>1)];
      var $11017=(($11016)&65535);
      var $11018=(($11012+($11017<<2))|0);
      var $11019=$11018;
      var $11020=$11019;
      var $11021=HEAP32[(($11020)>>2)];
      var $11022=(($11021)|0) < 0;
      if ($11022) { __label__ = 428; break; } else { __label__ = 427; break; }
    case 427: 
      var $11024=$2;
      var $11025=(($11024+64)|0);
      var $11026=HEAP32[(($11025)>>2)];
      var $11027=$st;
      var $11028=(($11027+4)|0);
      var $11029=$11028;
      var $11030=HEAP16[(($11029)>>1)];
      var $11031=(($11030)&65535);
      var $11032=(($11026+($11031<<2))|0);
      var $11033=$11032;
      var $11034=$11033;
      var $11035=HEAP32[(($11034)>>2)];
      var $11036=$2;
      var $11037=(($11036+80)|0);
      var $11038=HEAP32[(($11037)>>2)];
      var $11039=(($11035)>>>0) >= (($11038)>>>0);
      if ($11039) { __label__ = 428; break; } else { __label__ = 429; break; }
    case 428: 
      var $11041=$2;
      var $11042=$2;
      var $11043=(($11042)|0);
      var $11044=HEAP32[(($11043)>>2)];
      var $11045=$2;
      var $11046=(($11045+64)|0);
      var $11047=HEAP32[(($11046)>>2)];
      var $11048=$st;
      var $11049=(($11048+4)|0);
      var $11050=$11049;
      var $11051=HEAP16[(($11050)>>1)];
      var $11052=(($11051)&65535);
      var $11053=(($11047+($11052<<2))|0);
      var $11054=$11053;
      var $11055=$11054;
      var $11056=HEAP32[(($11055)>>2)];
      _qcvmerror($11041, ((STRING_TABLE.__str20)|0), (tempInt=STACKTOP,STACKTOP += 8,assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),HEAP32[((tempInt)>>2)]=$11044,HEAP32[(((tempInt)+(4))>>2)]=$11056,tempInt));
      __label__ = 488; break;
    case 429: 
      var $11058=$2;
      var $11059=(($11058+64)|0);
      var $11060=HEAP32[(($11059)>>2)];
      var $11061=$st;
      var $11062=(($11061+4)|0);
      var $11063=$11062;
      var $11064=HEAP16[(($11063)>>1)];
      var $11065=(($11064)&65535);
      var $11066=(($11060+($11065<<2))|0);
      var $11067=$11066;
      var $11068=$11067;
      var $11069=HEAP32[(($11068)>>2)];
      var $11070=$2;
      var $11071=(($11070+144)|0);
      var $11072=HEAP32[(($11071)>>2)];
      var $11073=(($11069)>>>0) < (($11072)>>>0);
      if ($11073) { __label__ = 430; break; } else { __label__ = 432; break; }
    case 430: 
      var $11075=$2;
      var $11076=(($11075+148)|0);
      var $11077=HEAP8[($11076)];
      var $11078=(($11077) & 1);
      if ($11078) { __label__ = 432; break; } else { __label__ = 431; break; }
    case 431: 
      var $11080=$2;
      var $11081=$2;
      var $11082=(($11081)|0);
      var $11083=HEAP32[(($11082)>>2)];
      var $11084=$2;
      var $11085=$2;
      var $11086=$2;
      var $11087=(($11086+64)|0);
      var $11088=HEAP32[(($11087)>>2)];
      var $11089=$st;
      var $11090=(($11089+4)|0);
      var $11091=$11090;
      var $11092=HEAP16[(($11091)>>1)];
      var $11093=(($11092)&65535);
      var $11094=(($11088+($11093<<2))|0);
      var $11095=$11094;
      var $11096=$11095;
      var $11097=HEAP32[(($11096)>>2)];
      var $11098=_prog_entfield($11085, $11097);
      var $11099=(($11098+4)|0);
      var $11100=HEAP32[(($11099)>>2)];
      var $11101=_prog_getstring($11084, $11100);
      var $11102=$2;
      var $11103=(($11102+64)|0);
      var $11104=HEAP32[(($11103)>>2)];
      var $11105=$st;
      var $11106=(($11105+4)|0);
      var $11107=$11106;
      var $11108=HEAP16[(($11107)>>1)];
      var $11109=(($11108)&65535);
      var $11110=(($11104+($11109<<2))|0);
      var $11111=$11110;
      var $11112=$11111;
      var $11113=HEAP32[(($11112)>>2)];
      _qcvmerror($11080, ((STRING_TABLE.__str21)|0), (tempInt=STACKTOP,STACKTOP += 12,assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),HEAP32[((tempInt)>>2)]=$11083,HEAP32[(((tempInt)+(4))>>2)]=$11101,HEAP32[(((tempInt)+(8))>>2)]=$11113,tempInt));
      __label__ = 432; break;
    case 432: 
      var $11115=$2;
      var $11116=(($11115+76)|0);
      var $11117=HEAP32[(($11116)>>2)];
      var $11118=$2;
      var $11119=(($11118+64)|0);
      var $11120=HEAP32[(($11119)>>2)];
      var $11121=$st;
      var $11122=(($11121+4)|0);
      var $11123=$11122;
      var $11124=HEAP16[(($11123)>>1)];
      var $11125=(($11124)&65535);
      var $11126=(($11120+($11125<<2))|0);
      var $11127=$11126;
      var $11128=$11127;
      var $11129=HEAP32[(($11128)>>2)];
      var $11130=(($11117+($11129<<2))|0);
      var $11131=$11130;
      $ptr11=$11131;
      var $11132=$2;
      var $11133=(($11132+64)|0);
      var $11134=HEAP32[(($11133)>>2)];
      var $11135=$st;
      var $11136=(($11135+2)|0);
      var $11137=$11136;
      var $11138=HEAP16[(($11137)>>1)];
      var $11139=(($11138)&65535);
      var $11140=(($11134+($11139<<2))|0);
      var $11141=$11140;
      var $11142=$11141;
      var $11143=HEAP32[(($11142)>>2)];
      var $11144=$ptr11;
      var $11145=$11144;
      HEAP32[(($11145)>>2)]=$11143;
      __label__ = 487; break;
    case 433: 
      var $11147=$2;
      var $11148=(($11147+64)|0);
      var $11149=HEAP32[(($11148)>>2)];
      var $11150=$st;
      var $11151=(($11150+4)|0);
      var $11152=$11151;
      var $11153=HEAP16[(($11152)>>1)];
      var $11154=(($11153)&65535);
      var $11155=(($11149+($11154<<2))|0);
      var $11156=$11155;
      var $11157=$11156;
      var $11158=HEAP32[(($11157)>>2)];
      var $11159=(($11158)|0) < 0;
      if ($11159) { __label__ = 435; break; } else { __label__ = 434; break; }
    case 434: 
      var $11161=$2;
      var $11162=(($11161+64)|0);
      var $11163=HEAP32[(($11162)>>2)];
      var $11164=$st;
      var $11165=(($11164+4)|0);
      var $11166=$11165;
      var $11167=HEAP16[(($11166)>>1)];
      var $11168=(($11167)&65535);
      var $11169=(($11163+($11168<<2))|0);
      var $11170=$11169;
      var $11171=$11170;
      var $11172=HEAP32[(($11171)>>2)];
      var $11173=((($11172)+(2))|0);
      var $11174=$2;
      var $11175=(($11174+80)|0);
      var $11176=HEAP32[(($11175)>>2)];
      var $11177=(($11173)>>>0) >= (($11176)>>>0);
      if ($11177) { __label__ = 435; break; } else { __label__ = 436; break; }
    case 435: 
      var $11179=$2;
      var $11180=$2;
      var $11181=(($11180)|0);
      var $11182=HEAP32[(($11181)>>2)];
      var $11183=$2;
      var $11184=(($11183+64)|0);
      var $11185=HEAP32[(($11184)>>2)];
      var $11186=$st;
      var $11187=(($11186+4)|0);
      var $11188=$11187;
      var $11189=HEAP16[(($11188)>>1)];
      var $11190=(($11189)&65535);
      var $11191=(($11185+($11190<<2))|0);
      var $11192=$11191;
      var $11193=$11192;
      var $11194=HEAP32[(($11193)>>2)];
      _qcvmerror($11179, ((STRING_TABLE.__str20)|0), (tempInt=STACKTOP,STACKTOP += 8,assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),HEAP32[((tempInt)>>2)]=$11182,HEAP32[(((tempInt)+(4))>>2)]=$11194,tempInt));
      __label__ = 488; break;
    case 436: 
      var $11196=$2;
      var $11197=(($11196+64)|0);
      var $11198=HEAP32[(($11197)>>2)];
      var $11199=$st;
      var $11200=(($11199+4)|0);
      var $11201=$11200;
      var $11202=HEAP16[(($11201)>>1)];
      var $11203=(($11202)&65535);
      var $11204=(($11198+($11203<<2))|0);
      var $11205=$11204;
      var $11206=$11205;
      var $11207=HEAP32[(($11206)>>2)];
      var $11208=$2;
      var $11209=(($11208+144)|0);
      var $11210=HEAP32[(($11209)>>2)];
      var $11211=(($11207)>>>0) < (($11210)>>>0);
      if ($11211) { __label__ = 437; break; } else { __label__ = 439; break; }
    case 437: 
      var $11213=$2;
      var $11214=(($11213+148)|0);
      var $11215=HEAP8[($11214)];
      var $11216=(($11215) & 1);
      if ($11216) { __label__ = 439; break; } else { __label__ = 438; break; }
    case 438: 
      var $11218=$2;
      var $11219=$2;
      var $11220=(($11219)|0);
      var $11221=HEAP32[(($11220)>>2)];
      var $11222=$2;
      var $11223=$2;
      var $11224=$2;
      var $11225=(($11224+64)|0);
      var $11226=HEAP32[(($11225)>>2)];
      var $11227=$st;
      var $11228=(($11227+4)|0);
      var $11229=$11228;
      var $11230=HEAP16[(($11229)>>1)];
      var $11231=(($11230)&65535);
      var $11232=(($11226+($11231<<2))|0);
      var $11233=$11232;
      var $11234=$11233;
      var $11235=HEAP32[(($11234)>>2)];
      var $11236=_prog_entfield($11223, $11235);
      var $11237=(($11236+4)|0);
      var $11238=HEAP32[(($11237)>>2)];
      var $11239=_prog_getstring($11222, $11238);
      var $11240=$2;
      var $11241=(($11240+64)|0);
      var $11242=HEAP32[(($11241)>>2)];
      var $11243=$st;
      var $11244=(($11243+4)|0);
      var $11245=$11244;
      var $11246=HEAP16[(($11245)>>1)];
      var $11247=(($11246)&65535);
      var $11248=(($11242+($11247<<2))|0);
      var $11249=$11248;
      var $11250=$11249;
      var $11251=HEAP32[(($11250)>>2)];
      _qcvmerror($11218, ((STRING_TABLE.__str21)|0), (tempInt=STACKTOP,STACKTOP += 12,assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),HEAP32[((tempInt)>>2)]=$11221,HEAP32[(((tempInt)+(4))>>2)]=$11239,HEAP32[(((tempInt)+(8))>>2)]=$11251,tempInt));
      __label__ = 439; break;
    case 439: 
      var $11253=$2;
      var $11254=(($11253+76)|0);
      var $11255=HEAP32[(($11254)>>2)];
      var $11256=$2;
      var $11257=(($11256+64)|0);
      var $11258=HEAP32[(($11257)>>2)];
      var $11259=$st;
      var $11260=(($11259+4)|0);
      var $11261=$11260;
      var $11262=HEAP16[(($11261)>>1)];
      var $11263=(($11262)&65535);
      var $11264=(($11258+($11263<<2))|0);
      var $11265=$11264;
      var $11266=$11265;
      var $11267=HEAP32[(($11266)>>2)];
      var $11268=(($11255+($11267<<2))|0);
      var $11269=$11268;
      $ptr11=$11269;
      var $11270=$2;
      var $11271=(($11270+64)|0);
      var $11272=HEAP32[(($11271)>>2)];
      var $11273=$st;
      var $11274=(($11273+2)|0);
      var $11275=$11274;
      var $11276=HEAP16[(($11275)>>1)];
      var $11277=(($11276)&65535);
      var $11278=(($11272+($11277<<2))|0);
      var $11279=$11278;
      var $11280=$11279;
      var $11281=(($11280)|0);
      var $11282=HEAP32[(($11281)>>2)];
      var $11283=$ptr11;
      var $11284=$11283;
      var $11285=(($11284)|0);
      HEAP32[(($11285)>>2)]=$11282;
      var $11286=$2;
      var $11287=(($11286+64)|0);
      var $11288=HEAP32[(($11287)>>2)];
      var $11289=$st;
      var $11290=(($11289+2)|0);
      var $11291=$11290;
      var $11292=HEAP16[(($11291)>>1)];
      var $11293=(($11292)&65535);
      var $11294=(($11288+($11293<<2))|0);
      var $11295=$11294;
      var $11296=$11295;
      var $11297=(($11296+4)|0);
      var $11298=HEAP32[(($11297)>>2)];
      var $11299=$ptr11;
      var $11300=$11299;
      var $11301=(($11300+4)|0);
      HEAP32[(($11301)>>2)]=$11298;
      var $11302=$2;
      var $11303=(($11302+64)|0);
      var $11304=HEAP32[(($11303)>>2)];
      var $11305=$st;
      var $11306=(($11305+2)|0);
      var $11307=$11306;
      var $11308=HEAP16[(($11307)>>1)];
      var $11309=(($11308)&65535);
      var $11310=(($11304+($11309<<2))|0);
      var $11311=$11310;
      var $11312=$11311;
      var $11313=(($11312+8)|0);
      var $11314=HEAP32[(($11313)>>2)];
      var $11315=$ptr11;
      var $11316=$11315;
      var $11317=(($11316+8)|0);
      HEAP32[(($11317)>>2)]=$11314;
      __label__ = 487; break;
    case 440: 
      var $11319=$2;
      var $11320=(($11319+64)|0);
      var $11321=HEAP32[(($11320)>>2)];
      var $11322=$st;
      var $11323=(($11322+2)|0);
      var $11324=$11323;
      var $11325=HEAP16[(($11324)>>1)];
      var $11326=(($11325)&65535);
      var $11327=(($11321+($11326<<2))|0);
      var $11328=$11327;
      var $11329=$11328;
      var $11330=HEAP32[(($11329)>>2)];
      var $11331=$11330 & 2147483647;
      var $11332=(($11331)|0)!=0;
      var $11333=$11332 ^ 1;
      var $11334=(($11333)&1);
      var $11335=(($11334)|0);
      var $11336=$2;
      var $11337=(($11336+64)|0);
      var $11338=HEAP32[(($11337)>>2)];
      var $11339=$st;
      var $11340=(($11339+6)|0);
      var $11341=$11340;
      var $11342=HEAP16[(($11341)>>1)];
      var $11343=(($11342)&65535);
      var $11344=(($11338+($11343<<2))|0);
      var $11345=$11344;
      var $11346=$11345;
      HEAPF32[(($11346)>>2)]=$11335;
      __label__ = 487; break;
    case 441: 
      var $11348=$2;
      var $11349=(($11348+64)|0);
      var $11350=HEAP32[(($11349)>>2)];
      var $11351=$st;
      var $11352=(($11351+2)|0);
      var $11353=$11352;
      var $11354=HEAP16[(($11353)>>1)];
      var $11355=(($11354)&65535);
      var $11356=(($11350+($11355<<2))|0);
      var $11357=$11356;
      var $11358=$11357;
      var $11359=(($11358)|0);
      var $11360=HEAPF32[(($11359)>>2)];
      var $11361=$11360 != 0;
      if ($11361) { var $11394 = 0;__label__ = 444; break; } else { __label__ = 442; break; }
    case 442: 
      var $11363=$2;
      var $11364=(($11363+64)|0);
      var $11365=HEAP32[(($11364)>>2)];
      var $11366=$st;
      var $11367=(($11366+2)|0);
      var $11368=$11367;
      var $11369=HEAP16[(($11368)>>1)];
      var $11370=(($11369)&65535);
      var $11371=(($11365+($11370<<2))|0);
      var $11372=$11371;
      var $11373=$11372;
      var $11374=(($11373+4)|0);
      var $11375=HEAPF32[(($11374)>>2)];
      var $11376=$11375 != 0;
      if ($11376) { var $11394 = 0;__label__ = 444; break; } else { __label__ = 443; break; }
    case 443: 
      var $11378=$2;
      var $11379=(($11378+64)|0);
      var $11380=HEAP32[(($11379)>>2)];
      var $11381=$st;
      var $11382=(($11381+2)|0);
      var $11383=$11382;
      var $11384=HEAP16[(($11383)>>1)];
      var $11385=(($11384)&65535);
      var $11386=(($11380+($11385<<2))|0);
      var $11387=$11386;
      var $11388=$11387;
      var $11389=(($11388+8)|0);
      var $11390=HEAPF32[(($11389)>>2)];
      var $11391=$11390 != 0;
      var $11392=$11391 ^ 1;
      var $11394 = $11392;__label__ = 444; break;
    case 444: 
      var $11394;
      var $11395=(($11394)&1);
      var $11396=(($11395)|0);
      var $11397=$2;
      var $11398=(($11397+64)|0);
      var $11399=HEAP32[(($11398)>>2)];
      var $11400=$st;
      var $11401=(($11400+6)|0);
      var $11402=$11401;
      var $11403=HEAP16[(($11402)>>1)];
      var $11404=(($11403)&65535);
      var $11405=(($11399+($11404<<2))|0);
      var $11406=$11405;
      var $11407=$11406;
      HEAPF32[(($11407)>>2)]=$11396;
      __label__ = 487; break;
    case 445: 
      var $11409=$2;
      var $11410=(($11409+64)|0);
      var $11411=HEAP32[(($11410)>>2)];
      var $11412=$st;
      var $11413=(($11412+2)|0);
      var $11414=$11413;
      var $11415=HEAP16[(($11414)>>1)];
      var $11416=(($11415)&65535);
      var $11417=(($11411+($11416<<2))|0);
      var $11418=$11417;
      var $11419=$11418;
      var $11420=HEAP32[(($11419)>>2)];
      var $11421=(($11420)|0)!=0;
      if ($11421) { __label__ = 446; break; } else { var $11441 = 1;__label__ = 447; break; }
    case 446: 
      var $11423=$2;
      var $11424=$2;
      var $11425=(($11424+64)|0);
      var $11426=HEAP32[(($11425)>>2)];
      var $11427=$st;
      var $11428=(($11427+2)|0);
      var $11429=$11428;
      var $11430=HEAP16[(($11429)>>1)];
      var $11431=(($11430)&65535);
      var $11432=(($11426+($11431<<2))|0);
      var $11433=$11432;
      var $11434=$11433;
      var $11435=HEAP32[(($11434)>>2)];
      var $11436=_prog_getstring($11423, $11435);
      var $11437=HEAP8[($11436)];
      var $11438=(($11437 << 24) >> 24)!=0;
      var $11439=$11438 ^ 1;
      var $11441 = $11439;__label__ = 447; break;
    case 447: 
      var $11441;
      var $11442=(($11441)&1);
      var $11443=(($11442)|0);
      var $11444=$2;
      var $11445=(($11444+64)|0);
      var $11446=HEAP32[(($11445)>>2)];
      var $11447=$st;
      var $11448=(($11447+6)|0);
      var $11449=$11448;
      var $11450=HEAP16[(($11449)>>1)];
      var $11451=(($11450)&65535);
      var $11452=(($11446+($11451<<2))|0);
      var $11453=$11452;
      var $11454=$11453;
      HEAPF32[(($11454)>>2)]=$11443;
      __label__ = 487; break;
    case 448: 
      var $11456=$2;
      var $11457=(($11456+64)|0);
      var $11458=HEAP32[(($11457)>>2)];
      var $11459=$st;
      var $11460=(($11459+2)|0);
      var $11461=$11460;
      var $11462=HEAP16[(($11461)>>1)];
      var $11463=(($11462)&65535);
      var $11464=(($11458+($11463<<2))|0);
      var $11465=$11464;
      var $11466=$11465;
      var $11467=HEAP32[(($11466)>>2)];
      var $11468=(($11467)|0)==0;
      var $11469=(($11468)&1);
      var $11470=(($11469)|0);
      var $11471=$2;
      var $11472=(($11471+64)|0);
      var $11473=HEAP32[(($11472)>>2)];
      var $11474=$st;
      var $11475=(($11474+6)|0);
      var $11476=$11475;
      var $11477=HEAP16[(($11476)>>1)];
      var $11478=(($11477)&65535);
      var $11479=(($11473+($11478<<2))|0);
      var $11480=$11479;
      var $11481=$11480;
      HEAPF32[(($11481)>>2)]=$11470;
      __label__ = 487; break;
    case 449: 
      var $11483=$2;
      var $11484=(($11483+64)|0);
      var $11485=HEAP32[(($11484)>>2)];
      var $11486=$st;
      var $11487=(($11486+2)|0);
      var $11488=$11487;
      var $11489=HEAP16[(($11488)>>1)];
      var $11490=(($11489)&65535);
      var $11491=(($11485+($11490<<2))|0);
      var $11492=$11491;
      var $11493=$11492;
      var $11494=HEAP32[(($11493)>>2)];
      var $11495=(($11494)|0)!=0;
      var $11496=$11495 ^ 1;
      var $11497=(($11496)&1);
      var $11498=(($11497)|0);
      var $11499=$2;
      var $11500=(($11499+64)|0);
      var $11501=HEAP32[(($11500)>>2)];
      var $11502=$st;
      var $11503=(($11502+6)|0);
      var $11504=$11503;
      var $11505=HEAP16[(($11504)>>1)];
      var $11506=(($11505)&65535);
      var $11507=(($11501+($11506<<2))|0);
      var $11508=$11507;
      var $11509=$11508;
      HEAPF32[(($11509)>>2)]=$11498;
      __label__ = 487; break;
    case 450: 
      var $11511=$2;
      var $11512=(($11511+64)|0);
      var $11513=HEAP32[(($11512)>>2)];
      var $11514=$st;
      var $11515=(($11514+2)|0);
      var $11516=$11515;
      var $11517=HEAP16[(($11516)>>1)];
      var $11518=(($11517)&65535);
      var $11519=(($11513+($11518<<2))|0);
      var $11520=$11519;
      var $11521=$11520;
      var $11522=HEAP32[(($11521)>>2)];
      var $11523=$11522 & 2147483647;
      var $11524=(($11523)|0)!=0;
      if ($11524) { __label__ = 451; break; } else { __label__ = 454; break; }
    case 451: 
      var $11526=$st;
      var $11527=(($11526+4)|0);
      var $11528=$11527;
      var $11529=HEAP16[(($11528)>>1)];
      var $11530=(($11529 << 16) >> 16);
      var $11531=((($11530)-(1))|0);
      var $11532=$st;
      var $11533=(($11532+($11531<<3))|0);
      $st=$11533;
      var $11534=$jumpcount;
      var $11535=((($11534)+(1))|0);
      $jumpcount=$11535;
      var $11536=$5;
      var $11537=(($11535)|0) >= (($11536)|0);
      if ($11537) { __label__ = 452; break; } else { __label__ = 453; break; }
    case 452: 
      var $11539=$2;
      var $11540=$2;
      var $11541=(($11540)|0);
      var $11542=HEAP32[(($11541)>>2)];
      var $11543=$jumpcount;
      _qcvmerror($11539, ((STRING_TABLE.__str22)|0), (tempInt=STACKTOP,STACKTOP += 8,assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),HEAP32[((tempInt)>>2)]=$11542,HEAP32[(((tempInt)+(4))>>2)]=$11543,tempInt));
      __label__ = 453; break;
    case 453: 
      __label__ = 454; break;
    case 454: 
      __label__ = 487; break;
    case 455: 
      var $11547=$2;
      var $11548=(($11547+64)|0);
      var $11549=HEAP32[(($11548)>>2)];
      var $11550=$st;
      var $11551=(($11550+2)|0);
      var $11552=$11551;
      var $11553=HEAP16[(($11552)>>1)];
      var $11554=(($11553)&65535);
      var $11555=(($11549+($11554<<2))|0);
      var $11556=$11555;
      var $11557=$11556;
      var $11558=HEAP32[(($11557)>>2)];
      var $11559=$11558 & 2147483647;
      var $11560=(($11559)|0)!=0;
      if ($11560) { __label__ = 459; break; } else { __label__ = 456; break; }
    case 456: 
      var $11562=$st;
      var $11563=(($11562+4)|0);
      var $11564=$11563;
      var $11565=HEAP16[(($11564)>>1)];
      var $11566=(($11565 << 16) >> 16);
      var $11567=((($11566)-(1))|0);
      var $11568=$st;
      var $11569=(($11568+($11567<<3))|0);
      $st=$11569;
      var $11570=$jumpcount;
      var $11571=((($11570)+(1))|0);
      $jumpcount=$11571;
      var $11572=$5;
      var $11573=(($11571)|0) >= (($11572)|0);
      if ($11573) { __label__ = 457; break; } else { __label__ = 458; break; }
    case 457: 
      var $11575=$2;
      var $11576=$2;
      var $11577=(($11576)|0);
      var $11578=HEAP32[(($11577)>>2)];
      var $11579=$jumpcount;
      _qcvmerror($11575, ((STRING_TABLE.__str22)|0), (tempInt=STACKTOP,STACKTOP += 8,assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),HEAP32[((tempInt)>>2)]=$11578,HEAP32[(((tempInt)+(4))>>2)]=$11579,tempInt));
      __label__ = 458; break;
    case 458: 
      __label__ = 459; break;
    case 459: 
      __label__ = 487; break;
    case 460: 
      var $11583=$st;
      var $11584=(($11583)|0);
      var $11585=HEAP16[(($11584)>>1)];
      var $11586=(($11585)&65535);
      var $11587=((($11586)-(51))|0);
      var $11588=$2;
      var $11589=(($11588+184)|0);
      HEAP32[(($11589)>>2)]=$11587;
      var $11590=$2;
      var $11591=(($11590+64)|0);
      var $11592=HEAP32[(($11591)>>2)];
      var $11593=$st;
      var $11594=(($11593+2)|0);
      var $11595=$11594;
      var $11596=HEAP16[(($11595)>>1)];
      var $11597=(($11596)&65535);
      var $11598=(($11592+($11597<<2))|0);
      var $11599=$11598;
      var $11600=$11599;
      var $11601=HEAP32[(($11600)>>2)];
      var $11602=(($11601)|0)!=0;
      if ($11602) { __label__ = 462; break; } else { __label__ = 461; break; }
    case 461: 
      var $11604=$2;
      var $11605=$2;
      var $11606=(($11605)|0);
      var $11607=HEAP32[(($11606)>>2)];
      _qcvmerror($11604, ((STRING_TABLE.__str23)|0), (tempInt=STACKTOP,STACKTOP += 4,assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),HEAP32[((tempInt)>>2)]=$11607,tempInt));
      __label__ = 462; break;
    case 462: 
      var $11609=$2;
      var $11610=(($11609+64)|0);
      var $11611=HEAP32[(($11610)>>2)];
      var $11612=$st;
      var $11613=(($11612+2)|0);
      var $11614=$11613;
      var $11615=HEAP16[(($11614)>>1)];
      var $11616=(($11615)&65535);
      var $11617=(($11611+($11616<<2))|0);
      var $11618=$11617;
      var $11619=$11618;
      var $11620=HEAP32[(($11619)>>2)];
      var $11621=(($11620)|0)!=0;
      if ($11621) { __label__ = 463; break; } else { __label__ = 464; break; }
    case 463: 
      var $11623=$2;
      var $11624=(($11623+64)|0);
      var $11625=HEAP32[(($11624)>>2)];
      var $11626=$st;
      var $11627=(($11626+2)|0);
      var $11628=$11627;
      var $11629=HEAP16[(($11628)>>1)];
      var $11630=(($11629)&65535);
      var $11631=(($11625+($11630<<2))|0);
      var $11632=$11631;
      var $11633=$11632;
      var $11634=HEAP32[(($11633)>>2)];
      var $11635=$2;
      var $11636=(($11635+44)|0);
      var $11637=HEAP32[(($11636)>>2)];
      var $11638=(($11634)>>>0) >= (($11637)>>>0);
      if ($11638) { __label__ = 464; break; } else { __label__ = 465; break; }
    case 464: 
      var $11640=$2;
      var $11641=$2;
      var $11642=(($11641)|0);
      var $11643=HEAP32[(($11642)>>2)];
      _qcvmerror($11640, ((STRING_TABLE.__str24)|0), (tempInt=STACKTOP,STACKTOP += 4,assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),HEAP32[((tempInt)>>2)]=$11643,tempInt));
      __label__ = 488; break;
    case 465: 
      var $11645=$2;
      var $11646=(($11645+64)|0);
      var $11647=HEAP32[(($11646)>>2)];
      var $11648=$st;
      var $11649=(($11648+2)|0);
      var $11650=$11649;
      var $11651=HEAP16[(($11650)>>1)];
      var $11652=(($11651)&65535);
      var $11653=(($11647+($11652<<2))|0);
      var $11654=$11653;
      var $11655=$11654;
      var $11656=HEAP32[(($11655)>>2)];
      var $11657=$2;
      var $11658=(($11657+40)|0);
      var $11659=HEAP32[(($11658)>>2)];
      var $11660=(($11659+($11656)*(36))|0);
      $newf9=$11660;
      var $11661=$newf9;
      var $11662=(($11661+12)|0);
      var $11663=HEAP32[(($11662)>>2)];
      var $11664=((($11663)+(1))|0);
      HEAP32[(($11662)>>2)]=$11664;
      var $11665=$st;
      var $11666=$2;
      var $11667=(($11666+4)|0);
      var $11668=HEAP32[(($11667)>>2)];
      var $11669=$11665;
      var $11670=$11668;
      var $11671=((($11669)-($11670))|0);
      var $11672=((((($11671)|0))/(8))&-1);
      var $11673=((($11672)+(1))|0);
      var $11674=$2;
      var $11675=(($11674+176)|0);
      HEAP32[(($11675)>>2)]=$11673;
      var $11676=$newf9;
      var $11677=(($11676)|0);
      var $11678=HEAP32[(($11677)>>2)];
      var $11679=(($11678)|0) < 0;
      if ($11679) { __label__ = 466; break; } else { __label__ = 471; break; }
    case 466: 
      var $11681=$newf9;
      var $11682=(($11681)|0);
      var $11683=HEAP32[(($11682)>>2)];
      var $11684=(((-$11683))|0);
      $builtinnumber12=$11684;
      var $11685=$builtinnumber12;
      var $11686=$2;
      var $11687=(($11686+132)|0);
      var $11688=HEAP32[(($11687)>>2)];
      var $11689=(($11685)>>>0) < (($11688)>>>0);
      if ($11689) { __label__ = 467; break; } else { __label__ = 469; break; }
    case 467: 
      var $11691=$builtinnumber12;
      var $11692=$2;
      var $11693=(($11692+128)|0);
      var $11694=HEAP32[(($11693)>>2)];
      var $11695=(($11694+($11691<<2))|0);
      var $11696=HEAP32[(($11695)>>2)];
      var $11697=(($11696)|0)!=0;
      if ($11697) { __label__ = 468; break; } else { __label__ = 469; break; }
    case 468: 
      var $11699=$builtinnumber12;
      var $11700=$2;
      var $11701=(($11700+128)|0);
      var $11702=HEAP32[(($11701)>>2)];
      var $11703=(($11702+($11699<<2))|0);
      var $11704=HEAP32[(($11703)>>2)];
      var $11705=$2;
      var $11706=FUNCTION_TABLE[$11704]($11705);
      __label__ = 470; break;
    case 469: 
      var $11708=$2;
      var $11709=$builtinnumber12;
      var $11710=$2;
      var $11711=(($11710)|0);
      var $11712=HEAP32[(($11711)>>2)];
      _qcvmerror($11708, ((STRING_TABLE.__str25)|0), (tempInt=STACKTOP,STACKTOP += 8,assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),HEAP32[((tempInt)>>2)]=$11709,HEAP32[(((tempInt)+(4))>>2)]=$11712,tempInt));
      __label__ = 470; break;
    case 470: 
      __label__ = 472; break;
    case 471: 
      var $11715=$2;
      var $11716=(($11715+4)|0);
      var $11717=HEAP32[(($11716)>>2)];
      var $11718=$2;
      var $11719=$newf9;
      var $11720=_prog_enterfunction($11718, $11719);
      var $11721=(($11717+($11720<<3))|0);
      var $11722=((($11721)-(8))|0);
      $st=$11722;
      __label__ = 472; break;
    case 472: 
      var $11724=$2;
      var $11725=(($11724+112)|0);
      var $11726=HEAP32[(($11725)>>2)];
      var $11727=(($11726)|0)!=0;
      if ($11727) { __label__ = 473; break; } else { __label__ = 474; break; }
    case 473: 
      __label__ = 488; break;
    case 474: 
      __label__ = 487; break;
    case 475: 
      var $11731=$2;
      var $11732=$2;
      var $11733=(($11732)|0);
      var $11734=HEAP32[(($11733)>>2)];
      _qcvmerror($11731, ((STRING_TABLE.__str26)|0), (tempInt=STACKTOP,STACKTOP += 4,assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),HEAP32[((tempInt)>>2)]=$11734,tempInt));
      __label__ = 487; break;
    case 476: 
      var $11736=$st;
      var $11737=(($11736+2)|0);
      var $11738=$11737;
      var $11739=HEAP16[(($11738)>>1)];
      var $11740=(($11739 << 16) >> 16);
      var $11741=((($11740)-(1))|0);
      var $11742=$st;
      var $11743=(($11742+($11741<<3))|0);
      $st=$11743;
      var $11744=$jumpcount;
      var $11745=((($11744)+(1))|0);
      $jumpcount=$11745;
      var $11746=(($11745)|0)==10000000;
      if ($11746) { __label__ = 477; break; } else { __label__ = 478; break; }
    case 477: 
      var $11748=$2;
      var $11749=$2;
      var $11750=(($11749)|0);
      var $11751=HEAP32[(($11750)>>2)];
      var $11752=$jumpcount;
      _qcvmerror($11748, ((STRING_TABLE.__str22)|0), (tempInt=STACKTOP,STACKTOP += 8,assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),HEAP32[((tempInt)>>2)]=$11751,HEAP32[(((tempInt)+(4))>>2)]=$11752,tempInt));
      __label__ = 478; break;
    case 478: 
      __label__ = 487; break;
    case 479: 
      var $11755=$2;
      var $11756=(($11755+64)|0);
      var $11757=HEAP32[(($11756)>>2)];
      var $11758=$st;
      var $11759=(($11758+2)|0);
      var $11760=$11759;
      var $11761=HEAP16[(($11760)>>1)];
      var $11762=(($11761)&65535);
      var $11763=(($11757+($11762<<2))|0);
      var $11764=$11763;
      var $11765=$11764;
      var $11766=HEAP32[(($11765)>>2)];
      var $11767=$11766 & 2147483647;
      var $11768=(($11767)|0)!=0;
      if ($11768) { __label__ = 480; break; } else { var $11785 = 0;__label__ = 481; break; }
    case 480: 
      var $11770=$2;
      var $11771=(($11770+64)|0);
      var $11772=HEAP32[(($11771)>>2)];
      var $11773=$st;
      var $11774=(($11773+4)|0);
      var $11775=$11774;
      var $11776=HEAP16[(($11775)>>1)];
      var $11777=(($11776)&65535);
      var $11778=(($11772+($11777<<2))|0);
      var $11779=$11778;
      var $11780=$11779;
      var $11781=HEAP32[(($11780)>>2)];
      var $11782=$11781 & 2147483647;
      var $11783=(($11782)|0)!=0;
      var $11785 = $11783;__label__ = 481; break;
    case 481: 
      var $11785;
      var $11786=(($11785)&1);
      var $11787=(($11786)|0);
      var $11788=$2;
      var $11789=(($11788+64)|0);
      var $11790=HEAP32[(($11789)>>2)];
      var $11791=$st;
      var $11792=(($11791+6)|0);
      var $11793=$11792;
      var $11794=HEAP16[(($11793)>>1)];
      var $11795=(($11794)&65535);
      var $11796=(($11790+($11795<<2))|0);
      var $11797=$11796;
      var $11798=$11797;
      HEAPF32[(($11798)>>2)]=$11787;
      __label__ = 487; break;
    case 482: 
      var $11800=$2;
      var $11801=(($11800+64)|0);
      var $11802=HEAP32[(($11801)>>2)];
      var $11803=$st;
      var $11804=(($11803+2)|0);
      var $11805=$11804;
      var $11806=HEAP16[(($11805)>>1)];
      var $11807=(($11806)&65535);
      var $11808=(($11802+($11807<<2))|0);
      var $11809=$11808;
      var $11810=$11809;
      var $11811=HEAP32[(($11810)>>2)];
      var $11812=$11811 & 2147483647;
      var $11813=(($11812)|0)!=0;
      if ($11813) { var $11830 = 1;__label__ = 484; break; } else { __label__ = 483; break; }
    case 483: 
      var $11815=$2;
      var $11816=(($11815+64)|0);
      var $11817=HEAP32[(($11816)>>2)];
      var $11818=$st;
      var $11819=(($11818+4)|0);
      var $11820=$11819;
      var $11821=HEAP16[(($11820)>>1)];
      var $11822=(($11821)&65535);
      var $11823=(($11817+($11822<<2))|0);
      var $11824=$11823;
      var $11825=$11824;
      var $11826=HEAP32[(($11825)>>2)];
      var $11827=$11826 & 2147483647;
      var $11828=(($11827)|0)!=0;
      var $11830 = $11828;__label__ = 484; break;
    case 484: 
      var $11830;
      var $11831=(($11830)&1);
      var $11832=(($11831)|0);
      var $11833=$2;
      var $11834=(($11833+64)|0);
      var $11835=HEAP32[(($11834)>>2)];
      var $11836=$st;
      var $11837=(($11836+6)|0);
      var $11838=$11837;
      var $11839=HEAP16[(($11838)>>1)];
      var $11840=(($11839)&65535);
      var $11841=(($11835+($11840<<2))|0);
      var $11842=$11841;
      var $11843=$11842;
      HEAPF32[(($11843)>>2)]=$11832;
      __label__ = 487; break;
    case 485: 
      var $11845=$2;
      var $11846=(($11845+64)|0);
      var $11847=HEAP32[(($11846)>>2)];
      var $11848=$st;
      var $11849=(($11848+2)|0);
      var $11850=$11849;
      var $11851=HEAP16[(($11850)>>1)];
      var $11852=(($11851)&65535);
      var $11853=(($11847+($11852<<2))|0);
      var $11854=$11853;
      var $11855=$11854;
      var $11856=HEAPF32[(($11855)>>2)];
      var $11857=(($11856)&-1);
      var $11858=$2;
      var $11859=(($11858+64)|0);
      var $11860=HEAP32[(($11859)>>2)];
      var $11861=$st;
      var $11862=(($11861+4)|0);
      var $11863=$11862;
      var $11864=HEAP16[(($11863)>>1)];
      var $11865=(($11864)&65535);
      var $11866=(($11860+($11865<<2))|0);
      var $11867=$11866;
      var $11868=$11867;
      var $11869=HEAPF32[(($11868)>>2)];
      var $11870=(($11869)&-1);
      var $11871=$11857 & $11870;
      var $11872=(($11871)|0);
      var $11873=$2;
      var $11874=(($11873+64)|0);
      var $11875=HEAP32[(($11874)>>2)];
      var $11876=$st;
      var $11877=(($11876+6)|0);
      var $11878=$11877;
      var $11879=HEAP16[(($11878)>>1)];
      var $11880=(($11879)&65535);
      var $11881=(($11875+($11880<<2))|0);
      var $11882=$11881;
      var $11883=$11882;
      HEAPF32[(($11883)>>2)]=$11872;
      __label__ = 487; break;
    case 486: 
      var $11885=$2;
      var $11886=(($11885+64)|0);
      var $11887=HEAP32[(($11886)>>2)];
      var $11888=$st;
      var $11889=(($11888+2)|0);
      var $11890=$11889;
      var $11891=HEAP16[(($11890)>>1)];
      var $11892=(($11891)&65535);
      var $11893=(($11887+($11892<<2))|0);
      var $11894=$11893;
      var $11895=$11894;
      var $11896=HEAPF32[(($11895)>>2)];
      var $11897=(($11896)&-1);
      var $11898=$2;
      var $11899=(($11898+64)|0);
      var $11900=HEAP32[(($11899)>>2)];
      var $11901=$st;
      var $11902=(($11901+4)|0);
      var $11903=$11902;
      var $11904=HEAP16[(($11903)>>1)];
      var $11905=(($11904)&65535);
      var $11906=(($11900+($11905<<2))|0);
      var $11907=$11906;
      var $11908=$11907;
      var $11909=HEAPF32[(($11908)>>2)];
      var $11910=(($11909)&-1);
      var $11911=$11897 | $11910;
      var $11912=(($11911)|0);
      var $11913=$2;
      var $11914=(($11913+64)|0);
      var $11915=HEAP32[(($11914)>>2)];
      var $11916=$st;
      var $11917=(($11916+6)|0);
      var $11918=$11917;
      var $11919=HEAP16[(($11918)>>1)];
      var $11920=(($11919)&65535);
      var $11921=(($11915+($11920<<2))|0);
      var $11922=$11921;
      var $11923=$11922;
      HEAPF32[(($11923)>>2)]=$11912;
      __label__ = 487; break;
    case 487: 
      __label__ = 368; break;
    case 488: 
      var $11926=$oldxflags;
      var $11927=$2;
      var $11928=(($11927+180)|0);
      HEAP32[(($11928)>>2)]=$11926;
      var $11929=$2;
      var $11930=(($11929+156)|0);
      HEAP32[(($11930)>>2)]=0;
      var $11931=$2;
      var $11932=(($11931+168)|0);
      HEAP32[(($11932)>>2)]=0;
      var $11933=$2;
      var $11934=(($11933+112)|0);
      var $11935=HEAP32[(($11934)>>2)];
      var $11936=(($11935)|0)!=0;
      if ($11936) { __label__ = 489; break; } else { __label__ = 490; break; }
    case 489: 
      $1=0;
      __label__ = 491; break;
    case 490: 
      $1=1;
      __label__ = 491; break;
    case 491: 
      var $11940=$1;
      STACKTOP = __stackBase__;
      return $11940;
    default: assert(0, "bad label: " + __label__);
  }
}
_prog_exec["X"]=1;

function _qcvmerror($prog, $fmt) {
  var __stackBase__  = STACKTOP; STACKTOP += 4; assert(STACKTOP % 4 == 0, "Stack is unaligned"); assert(STACKTOP < STACK_MAX, "Ran out of stack");
  var __label__;

  var $1;
  var $2;
  var $ap=__stackBase__;
  $1=$prog;
  $2=$fmt;
  var $3=$1;
  var $4=(($3+112)|0);
  var $5=HEAP32[(($4)>>2)];
  var $6=((($5)+(1))|0);
  HEAP32[(($4)>>2)]=$6;
  var $7=$ap;
  HEAP32[(($7)>>2)]=arguments[_qcvmerror.length];
  var $8=$2;
  var $9=HEAP32[(($ap)>>2)];
  var $10=_vprintf($8, $9);
  var $11=$ap;
  ;
  var $12=HEAP32[((_stdout)>>2)];
  var $13=_putc(10, $12);
  STACKTOP = __stackBase__;
  return;
}


function _prog_print_statement($prog, $st) {
  var __stackBase__  = STACKTOP; STACKTOP += 12; assert(STACKTOP % 4 == 0, "Stack is unaligned"); assert(STACKTOP < STACK_MAX, "Ran out of stack");
  var __label__;
  __label__ = 2; 
  while(1) switch(__label__) {
    case 2: 
      var $1;
      var $2;
      var $t=__stackBase__;
      $1=$prog;
      $2=$st;
      var $3=$2;
      var $4=(($3)|0);
      var $5=HEAP16[(($4)>>1)];
      var $6=(($5)&65535);
      var $7=(($6)>>>0) >= 67;
      if ($7) { __label__ = 3; break; } else { __label__ = 4; break; }
    case 3: 
      var $9=$2;
      var $10=(($9)|0);
      var $11=HEAP16[(($10)>>1)];
      var $12=(($11)&65535);
      var $13=_printf(((STRING_TABLE.__str27)|0), (tempInt=STACKTOP,STACKTOP += 4,assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),HEAP32[((tempInt)>>2)]=$12,tempInt));
      __label__ = 42; break;
    case 4: 
      var $15=$2;
      var $16=(($15)|0);
      var $17=HEAP16[(($16)>>1)];
      var $18=(($17)&65535);
      var $19=((_asm_instr+($18)*(12))|0);
      var $20=(($19)|0);
      var $21=HEAP32[(($20)>>2)];
      var $22=_printf(((STRING_TABLE.__str28)|0), (tempInt=STACKTOP,STACKTOP += 4,assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),HEAP32[((tempInt)>>2)]=$21,tempInt));
      var $23=$2;
      var $24=(($23)|0);
      var $25=HEAP16[(($24)>>1)];
      var $26=(($25)&65535);
      var $27=(($26)|0) >= 49;
      if ($27) { __label__ = 5; break; } else { __label__ = 7; break; }
    case 5: 
      var $29=$2;
      var $30=(($29)|0);
      var $31=HEAP16[(($30)>>1)];
      var $32=(($31)&65535);
      var $33=(($32)|0) <= 50;
      if ($33) { __label__ = 6; break; } else { __label__ = 7; break; }
    case 6: 
      var $35=$1;
      var $36=$2;
      var $37=(($36+2)|0);
      var $38=$37;
      var $39=HEAP16[(($38)>>1)];
      var $40=(($39)&65535);
      _trace_print_global($35, $40, 2);
      var $41=$2;
      var $42=(($41+4)|0);
      var $43=$42;
      var $44=HEAP16[(($43)>>1)];
      var $45=(($44 << 16) >> 16);
      var $46=_printf(((STRING_TABLE.__str29)|0), (tempInt=STACKTOP,STACKTOP += 4,assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),HEAP32[((tempInt)>>2)]=$45,tempInt));
      __label__ = 41; break;
    case 7: 
      var $48=$2;
      var $49=(($48)|0);
      var $50=HEAP16[(($49)>>1)];
      var $51=(($50)&65535);
      var $52=(($51)|0) >= 51;
      if ($52) { __label__ = 8; break; } else { __label__ = 10; break; }
    case 8: 
      var $54=$2;
      var $55=(($54)|0);
      var $56=HEAP16[(($55)>>1)];
      var $57=(($56)&65535);
      var $58=(($57)|0) <= 59;
      if ($58) { __label__ = 9; break; } else { __label__ = 10; break; }
    case 9: 
      var $60=_printf(((STRING_TABLE.__str30)|0), (tempInt=STACKTOP,STACKTOP += 1,STACKTOP = ((((STACKTOP)+3)>>2)<<2),assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),HEAP32[((tempInt)>>2)]=0,tempInt));
      __label__ = 40; break;
    case 10: 
      var $62=$2;
      var $63=(($62)|0);
      var $64=HEAP16[(($63)>>1)];
      var $65=(($64)&65535);
      var $66=(($65)|0)==61;
      if ($66) { __label__ = 11; break; } else { __label__ = 12; break; }
    case 11: 
      var $68=$2;
      var $69=(($68+2)|0);
      var $70=$69;
      var $71=HEAP16[(($70)>>1)];
      var $72=(($71 << 16) >> 16);
      var $73=_printf(((STRING_TABLE.__str31)|0), (tempInt=STACKTOP,STACKTOP += 4,assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),HEAP32[((tempInt)>>2)]=$72,tempInt));
      __label__ = 39; break;
    case 12: 
      var $75=$t;
      assert(12 % 1 === 0, 'memcpy given ' + 12 + ' bytes to copy. Problem with quantum=1 corrections perhaps?');HEAP32[(($75)>>2)]=HEAP32[((_prog_print_statement_t)>>2)];HEAP32[((($75)+(4))>>2)]=HEAP32[(((_prog_print_statement_t)+(4))>>2)];HEAP32[((($75)+(8))>>2)]=HEAP32[(((_prog_print_statement_t)+(8))>>2)];
      var $76=$2;
      var $77=(($76)|0);
      var $78=HEAP16[(($77)>>1)];
      var $79=(($78)&65535);
      if ((($79)|0) == 3) {
        __label__ = 13; break;
      }
      else if ((($79)|0) == 4) {
        __label__ = 14; break;
      }
      else if ((($79)|0) == 2) {
        __label__ = 15; break;
      }
      else if ((($79)|0) == 7 || (($79)|0) == 9 || (($79)|0) == 11 || (($79)|0) == 16) {
        __label__ = 16; break;
      }
      else if ((($79)|0) == 12 || (($79)|0) == 17) {
        __label__ = 17; break;
      }
      else if ((($79)|0) == 31 || (($79)|0) == 37) {
        __label__ = 18; break;
      }
      else if ((($79)|0) == 32) {
        __label__ = 19; break;
      }
      else if ((($79)|0) == 33) {
        __label__ = 20; break;
      }
      else if ((($79)|0) == 34) {
        __label__ = 21; break;
      }
      else if ((($79)|0) == 35) {
        __label__ = 22; break;
      }
      else if ((($79)|0) == 36) {
        __label__ = 23; break;
      }
      else if ((($79)|0) == 38) {
        __label__ = 24; break;
      }
      else if ((($79)|0) == 39) {
        __label__ = 25; break;
      }
      else if ((($79)|0) == 40) {
        __label__ = 26; break;
      }
      else if ((($79)|0) == 41) {
        __label__ = 27; break;
      }
      else if ((($79)|0) == 42) {
        __label__ = 28; break;
      }
      else {
      __label__ = 29; break;
      }
      
    case 13: 
      var $81=(($t+8)|0);
      HEAP32[(($81)>>2)]=3;
      var $82=(($t+4)|0);
      HEAP32[(($82)>>2)]=3;
      __label__ = 29; break;
    case 14: 
      var $84=(($t+8)|0);
      HEAP32[(($84)>>2)]=3;
      var $85=(($t)|0);
      HEAP32[(($85)>>2)]=3;
      __label__ = 29; break;
    case 15: 
      var $87=(($t+4)|0);
      HEAP32[(($87)>>2)]=3;
      var $88=(($t)|0);
      HEAP32[(($88)>>2)]=3;
      __label__ = 29; break;
    case 16: 
      var $90=(($t+8)|0);
      HEAP32[(($90)>>2)]=3;
      var $91=(($t+4)|0);
      HEAP32[(($91)>>2)]=3;
      var $92=(($t)|0);
      HEAP32[(($92)>>2)]=3;
      __label__ = 29; break;
    case 17: 
      var $94=(($t+4)|0);
      HEAP32[(($94)>>2)]=1;
      var $95=(($t)|0);
      HEAP32[(($95)>>2)]=1;
      __label__ = 29; break;
    case 18: 
      var $97=(($t+8)|0);
      HEAP32[(($97)>>2)]=-1;
      __label__ = 29; break;
    case 19: 
      var $99=(($t+4)|0);
      HEAP32[(($99)>>2)]=3;
      var $100=(($t)|0);
      HEAP32[(($100)>>2)]=3;
      var $101=(($t+8)|0);
      HEAP32[(($101)>>2)]=-1;
      __label__ = 29; break;
    case 20: 
      var $103=(($t+4)|0);
      HEAP32[(($103)>>2)]=1;
      var $104=(($t)|0);
      HEAP32[(($104)>>2)]=1;
      var $105=(($t+8)|0);
      HEAP32[(($105)>>2)]=-1;
      __label__ = 29; break;
    case 21: 
      var $107=(($t+4)|0);
      HEAP32[(($107)>>2)]=4;
      var $108=(($t)|0);
      HEAP32[(($108)>>2)]=4;
      var $109=(($t+8)|0);
      HEAP32[(($109)>>2)]=-1;
      __label__ = 29; break;
    case 22: 
      var $111=(($t+4)|0);
      HEAP32[(($111)>>2)]=5;
      var $112=(($t)|0);
      HEAP32[(($112)>>2)]=5;
      var $113=(($t+8)|0);
      HEAP32[(($113)>>2)]=-1;
      __label__ = 29; break;
    case 23: 
      var $115=(($t+4)|0);
      HEAP32[(($115)>>2)]=6;
      var $116=(($t)|0);
      HEAP32[(($116)>>2)]=6;
      var $117=(($t+8)|0);
      HEAP32[(($117)>>2)]=-1;
      __label__ = 29; break;
    case 24: 
      var $119=(($t)|0);
      HEAP32[(($119)>>2)]=3;
      var $120=(($t+4)|0);
      HEAP32[(($120)>>2)]=4;
      var $121=(($t+8)|0);
      HEAP32[(($121)>>2)]=-1;
      __label__ = 29; break;
    case 25: 
      var $123=(($t)|0);
      HEAP32[(($123)>>2)]=1;
      var $124=(($t+4)|0);
      HEAP32[(($124)>>2)]=4;
      var $125=(($t+8)|0);
      HEAP32[(($125)>>2)]=-1;
      __label__ = 29; break;
    case 26: 
      var $127=(($t)|0);
      HEAP32[(($127)>>2)]=4;
      var $128=(($t+4)|0);
      HEAP32[(($128)>>2)]=4;
      var $129=(($t+8)|0);
      HEAP32[(($129)>>2)]=-1;
      __label__ = 29; break;
    case 27: 
      var $131=(($t)|0);
      HEAP32[(($131)>>2)]=5;
      var $132=(($t+4)|0);
      HEAP32[(($132)>>2)]=4;
      var $133=(($t+8)|0);
      HEAP32[(($133)>>2)]=-1;
      __label__ = 29; break;
    case 28: 
      var $135=(($t)|0);
      HEAP32[(($135)>>2)]=6;
      var $136=(($t+4)|0);
      HEAP32[(($136)>>2)]=4;
      var $137=(($t+8)|0);
      HEAP32[(($137)>>2)]=-1;
      __label__ = 29; break;
    case 29: 
      var $139=(($t)|0);
      var $140=HEAP32[(($139)>>2)];
      var $141=(($140)|0) >= 0;
      if ($141) { __label__ = 30; break; } else { __label__ = 31; break; }
    case 30: 
      var $143=$1;
      var $144=$2;
      var $145=(($144+2)|0);
      var $146=$145;
      var $147=HEAP16[(($146)>>1)];
      var $148=(($147)&65535);
      var $149=(($t)|0);
      var $150=HEAP32[(($149)>>2)];
      _trace_print_global($143, $148, $150);
      __label__ = 32; break;
    case 31: 
      var $152=_printf(((STRING_TABLE.__str32)|0), (tempInt=STACKTOP,STACKTOP += 1,STACKTOP = ((((STACKTOP)+3)>>2)<<2),assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),HEAP32[((tempInt)>>2)]=0,tempInt));
      __label__ = 32; break;
    case 32: 
      var $154=(($t+4)|0);
      var $155=HEAP32[(($154)>>2)];
      var $156=(($155)|0) >= 0;
      if ($156) { __label__ = 33; break; } else { __label__ = 34; break; }
    case 33: 
      var $158=$1;
      var $159=$2;
      var $160=(($159+4)|0);
      var $161=$160;
      var $162=HEAP16[(($161)>>1)];
      var $163=(($162)&65535);
      var $164=(($t+4)|0);
      var $165=HEAP32[(($164)>>2)];
      _trace_print_global($158, $163, $165);
      __label__ = 35; break;
    case 34: 
      var $167=_printf(((STRING_TABLE.__str32)|0), (tempInt=STACKTOP,STACKTOP += 1,STACKTOP = ((((STACKTOP)+3)>>2)<<2),assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),HEAP32[((tempInt)>>2)]=0,tempInt));
      __label__ = 35; break;
    case 35: 
      var $169=(($t+8)|0);
      var $170=HEAP32[(($169)>>2)];
      var $171=(($170)|0) >= 0;
      if ($171) { __label__ = 36; break; } else { __label__ = 37; break; }
    case 36: 
      var $173=$1;
      var $174=$2;
      var $175=(($174+6)|0);
      var $176=$175;
      var $177=HEAP16[(($176)>>1)];
      var $178=(($177)&65535);
      var $179=(($t+8)|0);
      var $180=HEAP32[(($179)>>2)];
      _trace_print_global($173, $178, $180);
      __label__ = 38; break;
    case 37: 
      var $182=_printf(((STRING_TABLE.__str33)|0), (tempInt=STACKTOP,STACKTOP += 1,STACKTOP = ((((STACKTOP)+3)>>2)<<2),assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),HEAP32[((tempInt)>>2)]=0,tempInt));
      __label__ = 38; break;
    case 38: 
      var $184=_printf(((STRING_TABLE.__str30)|0), (tempInt=STACKTOP,STACKTOP += 1,STACKTOP = ((((STACKTOP)+3)>>2)<<2),assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),HEAP32[((tempInt)>>2)]=0,tempInt));
      __label__ = 39; break;
    case 39: 
      __label__ = 40; break;
    case 40: 
      __label__ = 41; break;
    case 41: 
      var $188=HEAP32[((_stdout)>>2)];
      var $189=_fflush($188);
      __label__ = 42; break;
    case 42: 
      STACKTOP = __stackBase__;
      return;
    default: assert(0, "bad label: " + __label__);
  }
}
_prog_print_statement["X"]=1;

function _trace_print_global($prog, $glob, $vtype) {
  var __stackBase__  = STACKTOP; assert(STACKTOP % 4 == 0, "Stack is unaligned"); assert(STACKTOP < STACK_MAX, "Ran out of stack");
  var __label__;
  __label__ = 2; 
  while(1) switch(__label__) {
    case 2: 
      var $1;
      var $2;
      var $3;
      var $def;
      var $value;
      var $len;
      var $name;
      $1=$prog;
      $2=$glob;
      $3=$vtype;
      var $4=$2;
      var $5=(($4)|0)!=0;
      if ($5) { __label__ = 4; break; } else { __label__ = 3; break; }
    case 3: 
      var $7=_printf(((STRING_TABLE.__str34)|0), (tempInt=STACKTOP,STACKTOP += 1,STACKTOP = ((((STACKTOP)+3)>>2)<<2),assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),HEAP32[((tempInt)>>2)]=0,tempInt));
      $len=$7;
      __label__ = 17; break;
    case 4: 
      var $9=$1;
      var $10=$2;
      var $11=_prog_getdef($9, $10);
      $def=$11;
      var $12=$2;
      var $13=$1;
      var $14=(($13+64)|0);
      var $15=HEAP32[(($14)>>2)];
      var $16=(($15+($12<<2))|0);
      var $17=$16;
      $value=$17;
      var $18=$def;
      var $19=(($18)|0)!=0;
      if ($19) { __label__ = 5; break; } else { __label__ = 9; break; }
    case 5: 
      var $21=$1;
      var $22=$def;
      var $23=(($22+4)|0);
      var $24=HEAP32[(($23)>>2)];
      var $25=_prog_getstring($21, $24);
      $name=$25;
      var $26=$name;
      var $27=(($26)|0);
      var $28=HEAP8[($27)];
      var $29=(($28 << 24) >> 24);
      var $30=(($29)|0)==35;
      if ($30) { __label__ = 6; break; } else { __label__ = 7; break; }
    case 6: 
      var $32=_printf(((STRING_TABLE.__str35)|0), (tempInt=STACKTOP,STACKTOP += 1,STACKTOP = ((((STACKTOP)+3)>>2)<<2),assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),HEAP32[((tempInt)>>2)]=0,tempInt));
      $len=$32;
      __label__ = 8; break;
    case 7: 
      var $34=$name;
      var $35=_printf(((STRING_TABLE.__str36)|0), (tempInt=STACKTOP,STACKTOP += 4,assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),HEAP32[((tempInt)>>2)]=$34,tempInt));
      $len=$35;
      __label__ = 8; break;
    case 8: 
      var $37=$def;
      var $38=(($37)|0);
      var $39=HEAP16[(($38)>>1)];
      var $40=(($39)&65535);
      $3=$40;
      __label__ = 10; break;
    case 9: 
      var $42=$2;
      var $43=_printf(((STRING_TABLE.__str37)|0), (tempInt=STACKTOP,STACKTOP += 4,assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),HEAP32[((tempInt)>>2)]=$42,tempInt));
      $len=$43;
      __label__ = 10; break;
    case 10: 
      var $45=$3;
      if ((($45)|0) == 0 || (($45)|0) == 4 || (($45)|0) == 5 || (($45)|0) == 6 || (($45)|0) == 7) {
        __label__ = 11; break;
      }
      else if ((($45)|0) == 3) {
        __label__ = 12; break;
      }
      else if ((($45)|0) == 1) {
        __label__ = 13; break;
      }
      else if ((($45)|0) == 2) {
        __label__ = 14; break;
      }
      else {
      __label__ = 15; break;
      }
      
    case 11: 
      var $47=$value;
      var $48=$47;
      var $49=HEAP32[(($48)>>2)];
      var $50=_printf(((STRING_TABLE.__str38)|0), (tempInt=STACKTOP,STACKTOP += 4,assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),HEAP32[((tempInt)>>2)]=$49,tempInt));
      var $51=$len;
      var $52=((($51)+($50))|0);
      $len=$52;
      __label__ = 16; break;
    case 12: 
      var $54=$value;
      var $55=$54;
      var $56=(($55)|0);
      var $57=HEAPF32[(($56)>>2)];
      var $58=$57;
      var $59=$value;
      var $60=$59;
      var $61=(($60+4)|0);
      var $62=HEAPF32[(($61)>>2)];
      var $63=$62;
      var $64=$value;
      var $65=$64;
      var $66=(($65+8)|0);
      var $67=HEAPF32[(($66)>>2)];
      var $68=$67;
      var $69=_printf(((STRING_TABLE.__str39)|0), (tempInt=STACKTOP,STACKTOP += 24,assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),(tempDoubleF64[0]=$58,HEAP32[((tempInt)>>2)]=tempDoubleI32[0],HEAP32[(((tempInt)+(4))>>2)]=tempDoubleI32[1]),(tempDoubleF64[0]=$63,HEAP32[(((tempInt)+(8))>>2)]=tempDoubleI32[0],HEAP32[((((tempInt)+(8))+(4))>>2)]=tempDoubleI32[1]),(tempDoubleF64[0]=$68,HEAP32[(((tempInt)+(16))>>2)]=tempDoubleI32[0],HEAP32[((((tempInt)+(16))+(4))>>2)]=tempDoubleI32[1]),tempInt));
      var $70=$len;
      var $71=((($70)+($69))|0);
      $len=$71;
      __label__ = 16; break;
    case 13: 
      var $73=$1;
      var $74=$value;
      var $75=$74;
      var $76=HEAP32[(($75)>>2)];
      var $77=_prog_getstring($73, $76);
      var $78=$len;
      var $79=(((29)-($78))|0);
      var $80=((($79)-(5))|0);
      var $81=_print_escaped_string($77, $80);
      var $82=$len;
      var $83=((($82)+($81))|0);
      $len=$83;
      var $84=_printf(((STRING_TABLE.__str40)|0), (tempInt=STACKTOP,STACKTOP += 1,STACKTOP = ((((STACKTOP)+3)>>2)<<2),assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),HEAP32[((tempInt)>>2)]=0,tempInt));
      var $85=$len;
      var $86=((($85)+($84))|0);
      $len=$86;
      __label__ = 16; break;
    case 14: 
      __label__ = 15; break;
    case 15: 
      var $89=$value;
      var $90=$89;
      var $91=HEAPF32[(($90)>>2)];
      var $92=$91;
      var $93=_printf(((STRING_TABLE.__str41)|0), (tempInt=STACKTOP,STACKTOP += 8,assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),(tempDoubleF64[0]=$92,HEAP32[((tempInt)>>2)]=tempDoubleI32[0],HEAP32[(((tempInt)+(4))>>2)]=tempDoubleI32[1]),tempInt));
      var $94=$len;
      var $95=((($94)+($93))|0);
      $len=$95;
      __label__ = 16; break;
    case 16: 
      __label__ = 17; break;
    case 17: 
      var $98=$len;
      var $99=(($98)>>>0) < 28;
      if ($99) { __label__ = 18; break; } else { __label__ = 19; break; }
    case 18: 
      var $101=$len;
      var $102=(((28)-($101))|0);
      var $103=((STRING_TABLE._trace_print_global_spaces+$102)|0);
      HEAP8[($103)]=0;
      var $104=_printf(((STRING_TABLE._trace_print_global_spaces)|0), (tempInt=STACKTOP,STACKTOP += 1,STACKTOP = ((((STACKTOP)+3)>>2)<<2),assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),HEAP32[((tempInt)>>2)]=0,tempInt));
      var $105=$len;
      var $106=(((28)-($105))|0);
      var $107=((STRING_TABLE._trace_print_global_spaces+$106)|0);
      HEAP8[($107)]=32;
      __label__ = 19; break;
    case 19: 
      STACKTOP = __stackBase__;
      return;
    default: assert(0, "bad label: " + __label__);
  }
}
_trace_print_global["X"]=1;

function _print_escaped_string($str, $maxlen) {
  ;
  var __label__;
  __label__ = 2; 
  while(1) switch(__label__) {
    case 2: 
      var $1;
      var $2;
      var $len;
      $1=$str;
      $2=$maxlen;
      $len=2;
      var $3=HEAP32[((_stdout)>>2)];
      var $4=_putc(34, $3);
      var $5=$2;
      var $6=((($5)-(1))|0);
      $2=$6;
      __label__ = 3; break;
    case 3: 
      var $8=$1;
      var $9=HEAP8[($8)];
      var $10=(($9 << 24) >> 24)!=0;
      if ($10) { __label__ = 4; break; } else { __label__ = 18; break; }
    case 4: 
      var $12=$len;
      var $13=$2;
      var $14=(($12)>>>0) >= (($13)>>>0);
      if ($14) { __label__ = 5; break; } else { __label__ = 6; break; }
    case 5: 
      var $16=HEAP32[((_stdout)>>2)];
      var $17=_putc(46, $16);
      var $18=HEAP32[((_stdout)>>2)];
      var $19=_putc(46, $18);
      var $20=HEAP32[((_stdout)>>2)];
      var $21=_putc(46, $20);
      var $22=$len;
      var $23=((($22)+(3))|0);
      $len=$23;
      __label__ = 18; break;
    case 6: 
      var $25=$1;
      var $26=HEAP8[($25)];
      var $27=(($26 << 24) >> 24);
      if ((($27)|0) == 7) {
        __label__ = 7; break;
      }
      else if ((($27)|0) == 8) {
        __label__ = 8; break;
      }
      else if ((($27)|0) == 13) {
        __label__ = 9; break;
      }
      else if ((($27)|0) == 10) {
        __label__ = 10; break;
      }
      else if ((($27)|0) == 9) {
        __label__ = 11; break;
      }
      else if ((($27)|0) == 12) {
        __label__ = 12; break;
      }
      else if ((($27)|0) == 11) {
        __label__ = 13; break;
      }
      else if ((($27)|0) == 92) {
        __label__ = 14; break;
      }
      else if ((($27)|0) == 34) {
        __label__ = 15; break;
      }
      else {
      __label__ = 16; break;
      }
      
    case 7: 
      var $29=$len;
      var $30=((($29)+(2))|0);
      $len=$30;
      var $31=HEAP32[((_stdout)>>2)];
      var $32=_putc(92, $31);
      var $33=HEAP32[((_stdout)>>2)];
      var $34=_putc(97, $33);
      __label__ = 17; break;
    case 8: 
      var $36=$len;
      var $37=((($36)+(2))|0);
      $len=$37;
      var $38=HEAP32[((_stdout)>>2)];
      var $39=_putc(92, $38);
      var $40=HEAP32[((_stdout)>>2)];
      var $41=_putc(98, $40);
      __label__ = 17; break;
    case 9: 
      var $43=$len;
      var $44=((($43)+(2))|0);
      $len=$44;
      var $45=HEAP32[((_stdout)>>2)];
      var $46=_putc(92, $45);
      var $47=HEAP32[((_stdout)>>2)];
      var $48=_putc(114, $47);
      __label__ = 17; break;
    case 10: 
      var $50=$len;
      var $51=((($50)+(2))|0);
      $len=$51;
      var $52=HEAP32[((_stdout)>>2)];
      var $53=_putc(92, $52);
      var $54=HEAP32[((_stdout)>>2)];
      var $55=_putc(110, $54);
      __label__ = 17; break;
    case 11: 
      var $57=$len;
      var $58=((($57)+(2))|0);
      $len=$58;
      var $59=HEAP32[((_stdout)>>2)];
      var $60=_putc(92, $59);
      var $61=HEAP32[((_stdout)>>2)];
      var $62=_putc(116, $61);
      __label__ = 17; break;
    case 12: 
      var $64=$len;
      var $65=((($64)+(2))|0);
      $len=$65;
      var $66=HEAP32[((_stdout)>>2)];
      var $67=_putc(92, $66);
      var $68=HEAP32[((_stdout)>>2)];
      var $69=_putc(102, $68);
      __label__ = 17; break;
    case 13: 
      var $71=$len;
      var $72=((($71)+(2))|0);
      $len=$72;
      var $73=HEAP32[((_stdout)>>2)];
      var $74=_putc(92, $73);
      var $75=HEAP32[((_stdout)>>2)];
      var $76=_putc(118, $75);
      __label__ = 17; break;
    case 14: 
      var $78=$len;
      var $79=((($78)+(2))|0);
      $len=$79;
      var $80=HEAP32[((_stdout)>>2)];
      var $81=_putc(92, $80);
      var $82=HEAP32[((_stdout)>>2)];
      var $83=_putc(92, $82);
      __label__ = 17; break;
    case 15: 
      var $85=$len;
      var $86=((($85)+(2))|0);
      $len=$86;
      var $87=HEAP32[((_stdout)>>2)];
      var $88=_putc(92, $87);
      var $89=HEAP32[((_stdout)>>2)];
      var $90=_putc(34, $89);
      __label__ = 17; break;
    case 16: 
      var $92=$len;
      var $93=((($92)+(1))|0);
      $len=$93;
      var $94=$1;
      var $95=HEAP8[($94)];
      var $96=(($95 << 24) >> 24);
      var $97=HEAP32[((_stdout)>>2)];
      var $98=_putc($96, $97);
      __label__ = 17; break;
    case 17: 
      var $100=$1;
      var $101=(($100+1)|0);
      $1=$101;
      __label__ = 3; break;
    case 18: 
      var $103=HEAP32[((_stdout)>>2)];
      var $104=_putc(34, $103);
      var $105=$len;
      ;
      return $105;
    default: assert(0, "bad label: " + __label__);
  }
}
_print_escaped_string["X"]=1;

function _prog_enterfunction($prog, $func) {
  var __stackBase__  = STACKTOP; STACKTOP += 12; assert(STACKTOP % 4 == 0, "Stack is unaligned"); assert(STACKTOP < STACK_MAX, "Ran out of stack");
  var __label__;
  __label__ = 2; 
  while(1) switch(__label__) {
    case 2: 
      var $1;
      var $2;
      var $st=__stackBase__;
      var $p;
      var $parampos;
      var $globals;
      var $s;
      $1=$prog;
      $2=$func;
      var $3=$1;
      var $4=(($3+156)|0);
      var $5=HEAP32[(($4)>>2)];
      var $6=(($st+4)|0);
      HEAP32[(($6)>>2)]=$5;
      var $7=$1;
      var $8=(($7+176)|0);
      var $9=HEAP32[(($8)>>2)];
      var $10=(($st)|0);
      HEAP32[(($10)>>2)]=$9;
      var $11=$2;
      var $12=(($st+8)|0);
      HEAP32[(($12)>>2)]=$11;
      var $13=$1;
      var $14=(($13+64)|0);
      var $15=HEAP32[(($14)>>2)];
      var $16=$2;
      var $17=(($16+4)|0);
      var $18=HEAP32[(($17)>>2)];
      var $19=(($15+($18<<2))|0);
      $globals=$19;
      var $20=$1;
      var $21=$globals;
      var $22=$2;
      var $23=(($22+8)|0);
      var $24=HEAP32[(($23)>>2)];
      var $25=_qc_program_localstack_append($20, $21, $24);
      if ($25) { __label__ = 4; break; } else { __label__ = 3; break; }
    case 3: 
      var $27=_printf(((STRING_TABLE.__str109)|0), (tempInt=STACKTOP,STACKTOP += 1,STACKTOP = ((((STACKTOP)+3)>>2)<<2),assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),HEAP32[((tempInt)>>2)]=0,tempInt));
      _exit(1);
      throw "Reached an unreachable!"
    case 4: 
      var $29=$2;
      var $30=(($29+4)|0);
      var $31=HEAP32[(($30)>>2)];
      $parampos=$31;
      $p=0;
      __label__ = 5; break;
    case 5: 
      var $33=$p;
      var $34=$2;
      var $35=(($34+24)|0);
      var $36=HEAP32[(($35)>>2)];
      var $37=(($33)>>>0) < (($36)>>>0);
      if ($37) { __label__ = 6; break; } else { __label__ = 12; break; }
    case 6: 
      $s=0;
      __label__ = 7; break;
    case 7: 
      var $40=$s;
      var $41=$p;
      var $42=$2;
      var $43=(($42+28)|0);
      var $44=(($43+$41)|0);
      var $45=HEAP8[($44)];
      var $46=(($45)&255);
      var $47=(($40)>>>0) < (($46)>>>0);
      if ($47) { __label__ = 8; break; } else { __label__ = 10; break; }
    case 8: 
      var $49=$p;
      var $50=((($49)*(3))|0);
      var $51=((($50)+(4))|0);
      var $52=$s;
      var $53=((($51)+($52))|0);
      var $54=$1;
      var $55=(($54+64)|0);
      var $56=HEAP32[(($55)>>2)];
      var $57=(($56+($53<<2))|0);
      var $58=HEAP32[(($57)>>2)];
      var $59=$parampos;
      var $60=$1;
      var $61=(($60+64)|0);
      var $62=HEAP32[(($61)>>2)];
      var $63=(($62+($59<<2))|0);
      HEAP32[(($63)>>2)]=$58;
      var $64=$parampos;
      var $65=((($64)+(1))|0);
      $parampos=$65;
      __label__ = 9; break;
    case 9: 
      var $67=$s;
      var $68=((($67)+(1))|0);
      $s=$68;
      __label__ = 7; break;
    case 10: 
      __label__ = 11; break;
    case 11: 
      var $71=$p;
      var $72=((($71)+(1))|0);
      $p=$72;
      __label__ = 5; break;
    case 12: 
      var $74=$1;
      var $75=(($st)|0);
      var $76=HEAP32[(($75)>>2)];
      var $77=(($st+4)|0);
      var $78=HEAP32[(($77)>>2)];
      var $79=(($st+8)|0);
      var $80=HEAP32[(($79)>>2)];
      var $81=_qc_program_stack_add($74, $76, $78, $80);
      if ($81) { __label__ = 14; break; } else { __label__ = 13; break; }
    case 13: 
      var $83=_printf(((STRING_TABLE.__str109)|0), (tempInt=STACKTOP,STACKTOP += 1,STACKTOP = ((((STACKTOP)+3)>>2)<<2),assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),HEAP32[((tempInt)>>2)]=0,tempInt));
      _exit(1);
      throw "Reached an unreachable!"
    case 14: 
      var $85=$2;
      var $86=(($85)|0);
      var $87=HEAP32[(($86)>>2)];
      STACKTOP = __stackBase__;
      return $87;
    default: assert(0, "bad label: " + __label__);
  }
}
_prog_enterfunction["X"]=1;

function _prog_leavefunction($prog) {
  var __stackBase__  = STACKTOP; STACKTOP += 12; assert(STACKTOP % 4 == 0, "Stack is unaligned"); assert(STACKTOP < STACK_MAX, "Ran out of stack");
  var __label__;
  __label__ = 2; 
  while(1) switch(__label__) {
    case 2: 
      var $1;
      var $prev;
      var $oldsp;
      var $st=__stackBase__;
      var $globals;
      $1=$prog;
      $prev=0;
      var $2=$1;
      var $3=(($2+168)|0);
      var $4=HEAP32[(($3)>>2)];
      var $5=((($4)-(1))|0);
      var $6=$1;
      var $7=(($6+164)|0);
      var $8=HEAP32[(($7)>>2)];
      var $9=(($8+($5)*(12))|0);
      var $10=$st;
      var $11=$9;
      assert(12 % 1 === 0, 'memcpy given ' + 12 + ' bytes to copy. Problem with quantum=1 corrections perhaps?');HEAP32[(($10)>>2)]=HEAP32[(($11)>>2)];HEAP32[((($10)+(4))>>2)]=HEAP32[((($11)+(4))>>2)];HEAP32[((($10)+(8))>>2)]=HEAP32[((($11)+(8))>>2)];
      var $12=$1;
      var $13=(($12+168)|0);
      var $14=HEAP32[(($13)>>2)];
      var $15=((($14)-(1))|0);
      var $16=$1;
      var $17=(($16+164)|0);
      var $18=HEAP32[(($17)>>2)];
      var $19=(($18+($15)*(12))|0);
      var $20=(($19+8)|0);
      var $21=HEAP32[(($20)>>2)];
      $prev=$21;
      var $22=$1;
      var $23=(($22+168)|0);
      var $24=HEAP32[(($23)>>2)];
      var $25=((($24)-(1))|0);
      var $26=$1;
      var $27=(($26+164)|0);
      var $28=HEAP32[(($27)>>2)];
      var $29=(($28+($25)*(12))|0);
      var $30=(($29+4)|0);
      var $31=HEAP32[(($30)>>2)];
      $oldsp=$31;
      var $32=$prev;
      var $33=(($32)|0)!=0;
      if ($33) { __label__ = 3; break; } else { __label__ = 6; break; }
    case 3: 
      var $35=$1;
      var $36=(($35+64)|0);
      var $37=HEAP32[(($36)>>2)];
      var $38=$prev;
      var $39=(($38+4)|0);
      var $40=HEAP32[(($39)>>2)];
      var $41=(($37+($40<<2))|0);
      $globals=$41;
      var $42=$globals;
      var $43=$42;
      var $44=$1;
      var $45=(($44+152)|0);
      var $46=HEAP32[(($45)>>2)];
      var $47=$oldsp;
      var $48=(($46+($47<<2))|0);
      var $49=$48;
      var $50=$prev;
      var $51=(($50+8)|0);
      var $52=HEAP32[(($51)>>2)];
      assert($52 % 1 === 0, 'memcpy given ' + $52 + ' bytes to copy. Problem with quantum=1 corrections perhaps?');_memcpy($43, $49, $52, 4);
      var $53=$1;
      var $54=$oldsp;
      var $55=_qc_program_localstack_resize($53, $54);
      if ($55) { __label__ = 5; break; } else { __label__ = 4; break; }
    case 4: 
      var $57=_printf(((STRING_TABLE.__str109)|0), (tempInt=STACKTOP,STACKTOP += 1,STACKTOP = ((((STACKTOP)+3)>>2)<<2),assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),HEAP32[((tempInt)>>2)]=0,tempInt));
      _exit(1);
      throw "Reached an unreachable!"
    case 5: 
      __label__ = 6; break;
    case 6: 
      var $60=$1;
      var $61=$1;
      var $62=(($61+168)|0);
      var $63=HEAP32[(($62)>>2)];
      var $64=((($63)-(1))|0);
      var $65=_qc_program_stack_remove($60, $64);
      if ($65) { __label__ = 8; break; } else { __label__ = 7; break; }
    case 7: 
      var $67=_printf(((STRING_TABLE.__str109)|0), (tempInt=STACKTOP,STACKTOP += 1,STACKTOP = ((((STACKTOP)+3)>>2)<<2),assert(STACKTOP < STACK_ROOT + STACK_MAX, "Ran out of stack"),HEAP32[((tempInt)>>2)]=0,tempInt));
      _exit(1);
      throw "Reached an unreachable!"
    case 8: 
      var $69=(($st)|0);
      var $70=HEAP32[(($69)>>2)];
      var $71=((($70)-(1))|0);
      STACKTOP = __stackBase__;
      return $71;
    default: assert(0, "bad label: " + __label__);
  }
}
_prog_leavefunction["X"]=1;

// Note: Some Emscripten settings will significantly limit the speed of the generated code.
// Note: Some Emscripten settings may limit the speed of the generated code.
// Warning: printing of i64 values may be slightly rounded! No deep i64 math used, so precise i64 code not included
var i64Math = null;

  
  function _memcpy(dest, src, num, align) {
      assert(num % 1 === 0, 'memcpy given ' + num + ' bytes to copy. Problem with quantum=1 corrections perhaps?');
      if (num >= 20 && src % 2 == dest % 2) {
        // This is unaligned, but quite large, and potentially alignable, so work hard to get to aligned settings
        if (src % 4 == dest % 4) {
          var stop = src + num;
          while (src % 4) { // no need to check for stop, since we have large num
            HEAP8[dest++] = HEAP8[src++];
          }
          var src4 = src >> 2, dest4 = dest >> 2, stop4 = stop >> 2;
          while (src4 < stop4) {
            HEAP32[dest4++] = HEAP32[src4++];
          }
          src = src4 << 2;
          dest = dest4 << 2;
          while (src < stop) {
            HEAP8[dest++] = HEAP8[src++];
          }
        } else {
          var stop = src + num;
          if (src % 2) { // no need to check for stop, since we have large num
            HEAP8[dest++] = HEAP8[src++];
          }
          var src2 = src >> 1, dest2 = dest >> 1, stop2 = stop >> 1;
          while (src2 < stop2) {
            HEAP16[dest2++] = HEAP16[src2++];
          }
          src = src2 << 1;
          dest = dest2 << 1;
          if (src < stop) {
            HEAP8[dest++] = HEAP8[src++];
          }
        }
      } else {
        while (num--) {
          HEAP8[dest++] = HEAP8[src++];
        }
      }
    }var _llvm_memcpy_p0i8_p0i8_i32=_memcpy;
var _util_memory_a; // stub for _util_memory_a
var _util_memory_d; // stub for _util_memory_d
var _util_fopen; // stub for _util_fopen

  
  
  var ERRNO_CODES={E2BIG:7,EACCES:13,EADDRINUSE:98,EADDRNOTAVAIL:99,EAFNOSUPPORT:97,EAGAIN:11,EALREADY:114,EBADF:9,EBADMSG:74,EBUSY:16,ECANCELED:125,ECHILD:10,ECONNABORTED:103,ECONNREFUSED:111,ECONNRESET:104,EDEADLK:35,EDESTADDRREQ:89,EDOM:33,EDQUOT:122,EEXIST:17,EFAULT:14,EFBIG:27,EHOSTUNREACH:113,EIDRM:43,EILSEQ:84,EINPROGRESS:115,EINTR:4,EINVAL:22,EIO:5,EISCONN:106,EISDIR:21,ELOOP:40,EMFILE:24,EMLINK:31,EMSGSIZE:90,EMULTIHOP:72,ENAMETOOLONG:36,ENETDOWN:100,ENETRESET:102,ENETUNREACH:101,ENFILE:23,ENOBUFS:105,ENODATA:61,ENODEV:19,ENOENT:2,ENOEXEC:8,ENOLCK:37,ENOLINK:67,ENOMEM:12,ENOMSG:42,ENOPROTOOPT:92,ENOSPC:28,ENOSR:63,ENOSTR:60,ENOSYS:38,ENOTCONN:107,ENOTDIR:20,ENOTEMPTY:39,ENOTRECOVERABLE:131,ENOTSOCK:88,ENOTSUP:95,ENOTTY:25,ENXIO:6,EOVERFLOW:75,EOWNERDEAD:130,EPERM:1,EPIPE:32,EPROTO:71,EPROTONOSUPPORT:93,EPROTOTYPE:91,ERANGE:34,EROFS:30,ESPIPE:29,ESRCH:3,ESTALE:116,ETIME:62,ETIMEDOUT:110,ETXTBSY:26,EWOULDBLOCK:11,EXDEV:18};
  
  function ___setErrNo(value) {
      // For convenient setting and returning of errno.
      if (!___setErrNo.ret) ___setErrNo.ret = allocate([0], 'i32', ALLOC_STATIC);
      HEAP32[((___setErrNo.ret)>>2)]=value
      return value;
    }
  
  var _stdin=0;
  
  var _stdout=0;
  
  var _stderr=0;
  
  var __impure_ptr=0;var FS={currentPath:"/",nextInode:2,streams:[null],checkStreams:function () {
        for (var i in FS.streams) assert(i >= 0 && i < FS.streams.length); // no keys not in dense span
        for (var i = 0; i < FS.streams.length; i++) assert(typeof FS.streams[i] == 'object'); // no non-null holes in dense span
      },ignorePermissions:true,joinPath:function (parts, forceRelative) {
        var ret = parts[0];
        for (var i = 1; i < parts.length; i++) {
          if (ret[ret.length-1] != '/') ret += '/';
          ret += parts[i];
        }
        if (forceRelative && ret[0] == '/') ret = ret.substr(1);
        return ret;
      },absolutePath:function (relative, base) {
        if (typeof relative !== 'string') return null;
        if (base === undefined) base = FS.currentPath;
        if (relative && relative[0] == '/') base = '';
        var full = base + '/' + relative;
        var parts = full.split('/').reverse();
        var absolute = [''];
        while (parts.length) {
          var part = parts.pop();
          if (part == '' || part == '.') {
            // Nothing.
          } else if (part == '..') {
            if (absolute.length > 1) absolute.pop();
          } else {
            absolute.push(part);
          }
        }
        return absolute.length == 1 ? '/' : absolute.join('/');
      },analyzePath:function (path, dontResolveLastLink, linksVisited) {
        var ret = {
          isRoot: false,
          exists: false,
          error: 0,
          name: null,
          path: null,
          object: null,
          parentExists: false,
          parentPath: null,
          parentObject: null
        };
        path = FS.absolutePath(path);
        if (path == '/') {
          ret.isRoot = true;
          ret.exists = ret.parentExists = true;
          ret.name = '/';
          ret.path = ret.parentPath = '/';
          ret.object = ret.parentObject = FS.root;
        } else if (path !== null) {
          linksVisited = linksVisited || 0;
          path = path.slice(1).split('/');
          var current = FS.root;
          var traversed = [''];
          while (path.length) {
            if (path.length == 1 && current.isFolder) {
              ret.parentExists = true;
              ret.parentPath = traversed.length == 1 ? '/' : traversed.join('/');
              ret.parentObject = current;
              ret.name = path[0];
            }
            var target = path.shift();
            if (!current.isFolder) {
              ret.error = ERRNO_CODES.ENOTDIR;
              break;
            } else if (!current.read) {
              ret.error = ERRNO_CODES.EACCES;
              break;
            } else if (!current.contents.hasOwnProperty(target)) {
              ret.error = ERRNO_CODES.ENOENT;
              break;
            }
            current = current.contents[target];
            if (current.link && !(dontResolveLastLink && path.length == 0)) {
              if (linksVisited > 40) { // Usual Linux SYMLOOP_MAX.
                ret.error = ERRNO_CODES.ELOOP;
                break;
              }
              var link = FS.absolutePath(current.link, traversed.join('/'));
              ret = FS.analyzePath([link].concat(path).join('/'),
                                   dontResolveLastLink, linksVisited + 1);
              return ret;
            }
            traversed.push(target);
            if (path.length == 0) {
              ret.exists = true;
              ret.path = traversed.join('/');
              ret.object = current;
            }
          }
        }
        return ret;
      },findObject:function (path, dontResolveLastLink) {
        FS.ensureRoot();
        var ret = FS.analyzePath(path, dontResolveLastLink);
        if (ret.exists) {
          return ret.object;
        } else {
          ___setErrNo(ret.error);
          return null;
        }
      },createObject:function (parent, name, properties, canRead, canWrite) {
        if (!parent) parent = '/';
        if (typeof parent === 'string') parent = FS.findObject(parent);
  
        if (!parent) {
          ___setErrNo(ERRNO_CODES.EACCES);
          throw new Error('Parent path must exist.');
        }
        if (!parent.isFolder) {
          ___setErrNo(ERRNO_CODES.ENOTDIR);
          throw new Error('Parent must be a folder.');
        }
        if (!parent.write && !FS.ignorePermissions) {
          ___setErrNo(ERRNO_CODES.EACCES);
          throw new Error('Parent folder must be writeable.');
        }
        if (!name || name == '.' || name == '..') {
          ___setErrNo(ERRNO_CODES.ENOENT);
          throw new Error('Name must not be empty.');
        }
        if (parent.contents.hasOwnProperty(name)) {
          ___setErrNo(ERRNO_CODES.EEXIST);
          throw new Error("Can't overwrite object.");
        }
  
        parent.contents[name] = {
          read: canRead === undefined ? true : canRead,
          write: canWrite === undefined ? false : canWrite,
          timestamp: Date.now(),
          inodeNumber: FS.nextInode++
        };
        for (var key in properties) {
          if (properties.hasOwnProperty(key)) {
            parent.contents[name][key] = properties[key];
          }
        }
  
        return parent.contents[name];
      },createFolder:function (parent, name, canRead, canWrite) {
        var properties = {isFolder: true, isDevice: false, contents: {}};
        return FS.createObject(parent, name, properties, canRead, canWrite);
      },createPath:function (parent, path, canRead, canWrite) {
        var current = FS.findObject(parent);
        if (current === null) throw new Error('Invalid parent.');
        path = path.split('/').reverse();
        while (path.length) {
          var part = path.pop();
          if (!part) continue;
          if (!current.contents.hasOwnProperty(part)) {
            FS.createFolder(current, part, canRead, canWrite);
          }
          current = current.contents[part];
        }
        return current;
      },createFile:function (parent, name, properties, canRead, canWrite) {
        properties.isFolder = false;
        return FS.createObject(parent, name, properties, canRead, canWrite);
      },createDataFile:function (parent, name, data, canRead, canWrite) {
        if (typeof data === 'string') {
          var dataArray = new Array(data.length);
          for (var i = 0, len = data.length; i < len; ++i) dataArray[i] = data.charCodeAt(i);
          data = dataArray;
        }
        var properties = {
          isDevice: false,
          contents: data.subarray ? data.subarray(0) : data // as an optimization, create a new array wrapper (not buffer) here, to help JS engines understand this object
        };
        return FS.createFile(parent, name, properties, canRead, canWrite);
      },createLazyFile:function (parent, name, url, canRead, canWrite) {
  
        if (typeof XMLHttpRequest !== 'undefined') {
          if (!ENVIRONMENT_IS_WORKER) throw 'Cannot do synchronous binary XHRs outside webworkers in modern browsers. Use --embed-file or --preload-file in emcc';
          // Lazy chunked Uint8Array (implements get and length from Uint8Array). Actual getting is abstracted away for eventual reuse.
          var LazyUint8Array = function(chunkSize, length) {
            this.length = length;
            this.chunkSize = chunkSize;
            this.chunks = []; // Loaded chunks. Index is the chunk number
          }
          LazyUint8Array.prototype.get = function(idx) {
            if (idx > this.length-1 || idx < 0) {
              return undefined;
            }
            var chunkOffset = idx % chunkSize;
            var chunkNum = Math.floor(idx / chunkSize);
            return this.getter(chunkNum)[chunkOffset];
          }
          LazyUint8Array.prototype.setDataGetter = function(getter) {
            this.getter = getter;
          }
    
          // Find length
          var xhr = new XMLHttpRequest();
          xhr.open('HEAD', url, false);
          xhr.send(null);
          if (!(xhr.status >= 200 && xhr.status < 300 || xhr.status === 304)) throw new Error("Couldn't load " + url + ". Status: " + xhr.status);
          var datalength = Number(xhr.getResponseHeader("Content-length"));
          var header;
          var hasByteServing = (header = xhr.getResponseHeader("Accept-Ranges")) && header === "bytes";
          var chunkSize = 1024*1024; // Chunk size in bytes
          if (!hasByteServing) chunkSize = datalength;
    
          // Function to get a range from the remote URL.
          var doXHR = (function(from, to) {
            if (from > to) throw new Error("invalid range (" + from + ", " + to + ") or no bytes requested!");
            if (to > datalength-1) throw new Error("only " + datalength + " bytes available! programmer error!");
    
            // TODO: Use mozResponseArrayBuffer, responseStream, etc. if available.
            var xhr = new XMLHttpRequest();
            xhr.open('GET', url, false);
            if (datalength !== chunkSize) xhr.setRequestHeader("Range", "bytes=" + from + "-" + to);
    
            // Some hints to the browser that we want binary data.
            if (typeof Uint8Array != 'undefined') xhr.responseType = 'arraybuffer';
            if (xhr.overrideMimeType) {
              xhr.overrideMimeType('text/plain; charset=x-user-defined');
            }
    
            xhr.send(null);
            if (!(xhr.status >= 200 && xhr.status < 300 || xhr.status === 304)) throw new Error("Couldn't load " + url + ". Status: " + xhr.status);
            if (xhr.response !== undefined) {
              return new Uint8Array(xhr.response || []);
            } else {
              return intArrayFromString(xhr.responseText || '', true);
            }
          });
    
          var lazyArray = new LazyUint8Array(chunkSize, datalength);
          lazyArray.setDataGetter(function(chunkNum) {
            var start = chunkNum * lazyArray.chunkSize;
            var end = (chunkNum+1) * lazyArray.chunkSize - 1; // including this byte
            end = Math.min(end, datalength-1); // if datalength-1 is selected, this is the last block
            if (typeof(lazyArray.chunks[chunkNum]) === "undefined") {
              lazyArray.chunks[chunkNum] = doXHR(start, end);
            }
            if (typeof(lazyArray.chunks[chunkNum]) === "undefined") throw new Error("doXHR failed!");
            return lazyArray.chunks[chunkNum];
          });
          var properties = { isDevice: false, contents: lazyArray };
        } else {
          var properties = { isDevice: false, url: url };
        }
  
        return FS.createFile(parent, name, properties, canRead, canWrite);
      },createPreloadedFile:function (parent, name, url, canRead, canWrite, onload, onerror, dontCreateFile) {
        Browser.ensureObjects();
        var fullname = FS.joinPath([parent, name], true);
        function processData(byteArray) {
          function finish(byteArray) {
            if (!dontCreateFile) {
              FS.createDataFile(parent, name, byteArray, canRead, canWrite);
            }
            if (onload) onload();
            removeRunDependency('cp ' + fullname);
          }
          var handled = false;
          Module['preloadPlugins'].forEach(function(plugin) {
            if (handled) return;
            if (plugin['canHandle'](fullname)) {
              plugin['handle'](byteArray, fullname, finish, function() {
                if (onerror) onerror();
                removeRunDependency('cp ' + fullname);
              });
              handled = true;
            }
          });
          if (!handled) finish(byteArray);
        }
        addRunDependency('cp ' + fullname);
        if (typeof url == 'string') {
          Browser.asyncLoad(url, function(byteArray) {
            processData(byteArray);
          }, onerror);
        } else {
          processData(url);
        }
      },createLink:function (parent, name, target, canRead, canWrite) {
        var properties = {isDevice: false, link: target};
        return FS.createFile(parent, name, properties, canRead, canWrite);
      },createDevice:function (parent, name, input, output) {
        if (!(input || output)) {
          throw new Error('A device must have at least one callback defined.');
        }
        var ops = {isDevice: true, input: input, output: output};
        return FS.createFile(parent, name, ops, Boolean(input), Boolean(output));
      },forceLoadFile:function (obj) {
        if (obj.isDevice || obj.isFolder || obj.link || obj.contents) return true;
        var success = true;
        if (typeof XMLHttpRequest !== 'undefined') {
          throw new Error("Lazy loading should have been performed (contents set) in createLazyFile, but it was not. Lazy loading only works in web workers. Use --embed-file or --preload-file in emcc on the main thread.");
        } else if (Module['read']) {
          // Command-line.
          try {
            // WARNING: Can't read binary files in V8's d8 or tracemonkey's js, as
            //          read() will try to parse UTF8.
            obj.contents = intArrayFromString(Module['read'](obj.url), true);
          } catch (e) {
            success = false;
          }
        } else {
          throw new Error('Cannot load without read() or XMLHttpRequest.');
        }
        if (!success) ___setErrNo(ERRNO_CODES.EIO);
        return success;
      },ensureRoot:function () {
        if (FS.root) return;
        // The main file system tree. All the contents are inside this.
        FS.root = {
          read: true,
          write: true,
          isFolder: true,
          isDevice: false,
          timestamp: Date.now(),
          inodeNumber: 1,
          contents: {}
        };
      },init:function (input, output, error) {
        // Make sure we initialize only once.
        assert(!FS.init.initialized, 'FS.init was previously called. If you want to initialize later with custom parameters, remove any earlier calls (note that one is automatically added to the generated code)');
        FS.init.initialized = true;
  
        FS.ensureRoot();
  
        // Allow Module.stdin etc. to provide defaults, if none explicitly passed to us here
        input = input || Module['stdin'];
        output = output || Module['stdout'];
        error = error || Module['stderr'];
  
        // Default handlers.
        var stdinOverridden = true, stdoutOverridden = true, stderrOverridden = true;
        if (!input) {
          stdinOverridden = false;
          input = function() {
            if (!input.cache || !input.cache.length) {
              var result;
              if (typeof window != 'undefined' &&
                  typeof window.prompt == 'function') {
                // Browser.
                result = window.prompt('Input: ');
                if (result === null) result = String.fromCharCode(0); // cancel ==> EOF
              } else if (typeof readline == 'function') {
                // Command line.
                result = readline();
              }
              if (!result) result = '';
              input.cache = intArrayFromString(result + '\n', true);
            }
            return input.cache.shift();
          };
        }
        var utf8 = new Runtime.UTF8Processor();
        function simpleOutput(val) {
          if (val === null || val === '\n'.charCodeAt(0)) {
            output.printer(output.buffer.join(''));
            output.buffer = [];
          } else {
            output.buffer.push(utf8.processCChar(val));
          }
        }
        if (!output) {
          stdoutOverridden = false;
          output = simpleOutput;
        }
        if (!output.printer) output.printer = Module['print'];
        if (!output.buffer) output.buffer = [];
        if (!error) {
          stderrOverridden = false;
          error = simpleOutput;
        }
        if (!error.printer) error.printer = Module['print'];
        if (!error.buffer) error.buffer = [];
  
        // Create the temporary folder, if not already created
        try {
          FS.createFolder('/', 'tmp', true, true);
        } catch(e) {}
  
        // Create the I/O devices.
        var devFolder = FS.createFolder('/', 'dev', true, true);
        var stdin = FS.createDevice(devFolder, 'stdin', input);
        var stdout = FS.createDevice(devFolder, 'stdout', null, output);
        var stderr = FS.createDevice(devFolder, 'stderr', null, error);
        FS.createDevice(devFolder, 'tty', input, output);
  
        // Create default streams.
        FS.streams[1] = {
          path: '/dev/stdin',
          object: stdin,
          position: 0,
          isRead: true,
          isWrite: false,
          isAppend: false,
          isTerminal: !stdinOverridden,
          error: false,
          eof: false,
          ungotten: []
        };
        FS.streams[2] = {
          path: '/dev/stdout',
          object: stdout,
          position: 0,
          isRead: false,
          isWrite: true,
          isAppend: false,
          isTerminal: !stdoutOverridden,
          error: false,
          eof: false,
          ungotten: []
        };
        FS.streams[3] = {
          path: '/dev/stderr',
          object: stderr,
          position: 0,
          isRead: false,
          isWrite: true,
          isAppend: false,
          isTerminal: !stderrOverridden,
          error: false,
          eof: false,
          ungotten: []
        };
        // Allocate these on the stack (and never free, we are called from ATINIT or earlier), to keep their locations low
        _stdin = allocate([1], 'void*', ALLOC_STACK);
        _stdout = allocate([2], 'void*', ALLOC_STACK);
        _stderr = allocate([3], 'void*', ALLOC_STACK);
  
        // Other system paths
        FS.createPath('/', 'dev/shm/tmp', true, true); // temp files
  
        // Newlib initialization
        for (var i = FS.streams.length; i < Math.max(_stdin, _stdout, _stderr) + 4; i++) {
          FS.streams[i] = null; // Make sure to keep FS.streams dense
        }
        FS.streams[_stdin] = FS.streams[1];
        FS.streams[_stdout] = FS.streams[2];
        FS.streams[_stderr] = FS.streams[3];
        FS.checkStreams();
        assert(FS.streams.length < 1024); // at this early stage, we should not have a large set of file descriptors - just a few
        __impure_ptr = allocate([ allocate(
          [0, 0, 0, 0, _stdin, 0, 0, 0, _stdout, 0, 0, 0, _stderr, 0, 0, 0],
          'void*', ALLOC_STATIC) ], 'void*', ALLOC_STATIC);
      },quit:function () {
        if (!FS.init.initialized) return;
        // Flush any partially-printed lines in stdout and stderr. Careful, they may have been closed
        if (FS.streams[2] && FS.streams[2].object.output.buffer.length > 0) FS.streams[2].object.output('\n'.charCodeAt(0));
        if (FS.streams[3] && FS.streams[3].object.output.buffer.length > 0) FS.streams[3].object.output('\n'.charCodeAt(0));
      },standardizePath:function (path) {
        if (path.substr(0, 2) == './') path = path.substr(2);
        return path;
      },deleteFile:function (path) {
        var path = FS.analyzePath(path);
        if (!path.parentExists || !path.exists) {
          throw 'Invalid path ' + path;
        }
        delete path.parentObject.contents[path.name];
      }};
  
  
  function _pread(fildes, buf, nbyte, offset) {
      // ssize_t pread(int fildes, void *buf, size_t nbyte, off_t offset);
      // http://pubs.opengroup.org/onlinepubs/000095399/functions/read.html
      var stream = FS.streams[fildes];
      if (!stream || stream.object.isDevice) {
        ___setErrNo(ERRNO_CODES.EBADF);
        return -1;
      } else if (!stream.isRead) {
        ___setErrNo(ERRNO_CODES.EACCES);
        return -1;
      } else if (stream.object.isFolder) {
        ___setErrNo(ERRNO_CODES.EISDIR);
        return -1;
      } else if (nbyte < 0 || offset < 0) {
        ___setErrNo(ERRNO_CODES.EINVAL);
        return -1;
      } else {
        var bytesRead = 0;
        while (stream.ungotten.length && nbyte > 0) {
          HEAP8[(buf++)]=stream.ungotten.pop()
          nbyte--;
          bytesRead++;
        }
        var contents = stream.object.contents;
        var size = Math.min(contents.length - offset, nbyte);
        if (contents.subarray || contents.slice) { // typed array or normal array
          for (var i = 0; i < size; i++) {
            HEAP8[((buf)+(i))]=contents[offset + i]
          }
        } else {
          for (var i = 0; i < size; i++) { // LazyUint8Array from sync binary XHR
            HEAP8[((buf)+(i))]=contents.get(offset + i)
          }
        }
        bytesRead += size;
        return bytesRead;
      }
    }function _read(fildes, buf, nbyte) {
      // ssize_t read(int fildes, void *buf, size_t nbyte);
      // http://pubs.opengroup.org/onlinepubs/000095399/functions/read.html
      var stream = FS.streams[fildes];
      if (!stream) {
        ___setErrNo(ERRNO_CODES.EBADF);
        return -1;
      } else if (!stream.isRead) {
        ___setErrNo(ERRNO_CODES.EACCES);
        return -1;
      } else if (nbyte < 0) {
        ___setErrNo(ERRNO_CODES.EINVAL);
        return -1;
      } else {
        var bytesRead;
        if (stream.object.isDevice) {
          if (stream.object.input) {
            bytesRead = 0;
            while (stream.ungotten.length && nbyte > 0) {
              HEAP8[(buf++)]=stream.ungotten.pop()
              nbyte--;
              bytesRead++;
            }
            for (var i = 0; i < nbyte; i++) {
              try {
                var result = stream.object.input();
              } catch (e) {
                ___setErrNo(ERRNO_CODES.EIO);
                return -1;
              }
              if (result === null || result === undefined) break;
              bytesRead++;
              HEAP8[((buf)+(i))]=result
            }
            return bytesRead;
          } else {
            ___setErrNo(ERRNO_CODES.ENXIO);
            return -1;
          }
        } else {
          var ungotSize = stream.ungotten.length;
          bytesRead = _pread(fildes, buf, nbyte, stream.position);
          if (bytesRead != -1) {
            stream.position += (stream.ungotten.length - ungotSize) + bytesRead;
          }
          return bytesRead;
        }
      }
    }function _fread(ptr, size, nitems, stream) {
      // size_t fread(void *restrict ptr, size_t size, size_t nitems, FILE *restrict stream);
      // http://pubs.opengroup.org/onlinepubs/000095399/functions/fread.html
      var bytesToRead = nitems * size;
      if (bytesToRead == 0) return 0;
      var bytesRead = _read(stream, ptr, bytesToRead);
      var streamObj = FS.streams[stream];
      if (bytesRead == -1) {
        if (streamObj) streamObj.error = true;
        return 0;
      } else {
        if (bytesRead < bytesToRead) streamObj.eof = true;
        return Math.floor(bytesRead / size);
      }
    }

  
  function _close(fildes) {
      // int close(int fildes);
      // http://pubs.opengroup.org/onlinepubs/000095399/functions/close.html
      if (FS.streams[fildes]) {
        if (FS.streams[fildes].currentEntry) {
          _free(FS.streams[fildes].currentEntry);
        }
        FS.streams[fildes] = null;
        return 0;
      } else {
        ___setErrNo(ERRNO_CODES.EBADF);
        return -1;
      }
    }
  
  function _fsync(fildes) {
      // int fsync(int fildes);
      // http://pubs.opengroup.org/onlinepubs/000095399/functions/fsync.html
      if (FS.streams[fildes]) {
        // We write directly to the file system, so there's nothing to do here.
        return 0;
      } else {
        ___setErrNo(ERRNO_CODES.EBADF);
        return -1;
      }
    }function _fclose(stream) {
      // int fclose(FILE *stream);
      // http://pubs.opengroup.org/onlinepubs/000095399/functions/fclose.html
      _fsync(stream);
      return _close(stream);
    }

  
  
  
  
  function _pwrite(fildes, buf, nbyte, offset) {
      // ssize_t pwrite(int fildes, const void *buf, size_t nbyte, off_t offset);
      // http://pubs.opengroup.org/onlinepubs/000095399/functions/write.html
      var stream = FS.streams[fildes];
      if (!stream || stream.object.isDevice) {
        ___setErrNo(ERRNO_CODES.EBADF);
        return -1;
      } else if (!stream.isWrite) {
        ___setErrNo(ERRNO_CODES.EACCES);
        return -1;
      } else if (stream.object.isFolder) {
        ___setErrNo(ERRNO_CODES.EISDIR);
        return -1;
      } else if (nbyte < 0 || offset < 0) {
        ___setErrNo(ERRNO_CODES.EINVAL);
        return -1;
      } else {
        var contents = stream.object.contents;
        while (contents.length < offset) contents.push(0);
        for (var i = 0; i < nbyte; i++) {
          contents[offset + i] = HEAPU8[((buf)+(i))];
        }
        stream.object.timestamp = Date.now();
        return i;
      }
    }function _write(fildes, buf, nbyte) {
      // ssize_t write(int fildes, const void *buf, size_t nbyte);
      // http://pubs.opengroup.org/onlinepubs/000095399/functions/write.html
      var stream = FS.streams[fildes];
      if (!stream) {
        ___setErrNo(ERRNO_CODES.EBADF);
        return -1;
      } else if (!stream.isWrite) {
        ___setErrNo(ERRNO_CODES.EACCES);
        return -1;
      } else if (nbyte < 0) {
        ___setErrNo(ERRNO_CODES.EINVAL);
        return -1;
      } else {
        if (stream.object.isDevice) {
          if (stream.object.output) {
            for (var i = 0; i < nbyte; i++) {
              try {
                stream.object.output(HEAP8[((buf)+(i))]);
              } catch (e) {
                ___setErrNo(ERRNO_CODES.EIO);
                return -1;
              }
            }
            stream.object.timestamp = Date.now();
            return i;
          } else {
            ___setErrNo(ERRNO_CODES.ENXIO);
            return -1;
          }
        } else {
          var bytesWritten = _pwrite(fildes, buf, nbyte, stream.position);
          if (bytesWritten != -1) stream.position += bytesWritten;
          return bytesWritten;
        }
      }
    }function _fwrite(ptr, size, nitems, stream) {
      // size_t fwrite(const void *restrict ptr, size_t size, size_t nitems, FILE *restrict stream);
      // http://pubs.opengroup.org/onlinepubs/000095399/functions/fwrite.html
      var bytesToWrite = nitems * size;
      if (bytesToWrite == 0) return 0;
      var bytesWritten = _write(stream, ptr, bytesToWrite);
      if (bytesWritten == -1) {
        if (FS.streams[stream]) FS.streams[stream].error = true;
        return 0;
      } else {
        return Math.floor(bytesWritten / size);
      }
    }
  
  function __formatString(format, varargs) {
      var textIndex = format;
      var argIndex = 0;
      function getNextArg(type) {
        // NOTE: Explicitly ignoring type safety. Otherwise this fails:
        //       int x = 4; printf("%c\n", (char)x);
        var ret;
        if (type === 'double') {
          ret = (tempDoubleI32[0]=HEAP32[(((varargs)+(argIndex))>>2)],tempDoubleI32[1]=HEAP32[(((varargs)+((argIndex)+(4)))>>2)],tempDoubleF64[0]);
        } else if (type == 'i64') {
          ret = [HEAP32[(((varargs)+(argIndex))>>2)],
                 HEAP32[(((varargs)+(argIndex+4))>>2)]];
        } else {
          type = 'i32'; // varargs are always i32, i64, or double
          ret = HEAP32[(((varargs)+(argIndex))>>2)];
        }
        argIndex += Runtime.getNativeFieldSize(type);
        return ret;
      }
  
      var ret = [];
      var curr, next, currArg;
      while(1) {
        var startTextIndex = textIndex;
        curr = HEAP8[(textIndex)];
        if (curr === 0) break;
        next = HEAP8[(textIndex+1)];
        if (curr == '%'.charCodeAt(0)) {
          // Handle flags.
          var flagAlwaysSigned = false;
          var flagLeftAlign = false;
          var flagAlternative = false;
          var flagZeroPad = false;
          flagsLoop: while (1) {
            switch (next) {
              case '+'.charCodeAt(0):
                flagAlwaysSigned = true;
                break;
              case '-'.charCodeAt(0):
                flagLeftAlign = true;
                break;
              case '#'.charCodeAt(0):
                flagAlternative = true;
                break;
              case '0'.charCodeAt(0):
                if (flagZeroPad) {
                  break flagsLoop;
                } else {
                  flagZeroPad = true;
                  break;
                }
              default:
                break flagsLoop;
            }
            textIndex++;
            next = HEAP8[(textIndex+1)];
          }
  
          // Handle width.
          var width = 0;
          if (next == '*'.charCodeAt(0)) {
            width = getNextArg('i32');
            textIndex++;
            next = HEAP8[(textIndex+1)];
          } else {
            while (next >= '0'.charCodeAt(0) && next <= '9'.charCodeAt(0)) {
              width = width * 10 + (next - '0'.charCodeAt(0));
              textIndex++;
              next = HEAP8[(textIndex+1)];
            }
          }
  
          // Handle precision.
          var precisionSet = false;
          if (next == '.'.charCodeAt(0)) {
            var precision = 0;
            precisionSet = true;
            textIndex++;
            next = HEAP8[(textIndex+1)];
            if (next == '*'.charCodeAt(0)) {
              precision = getNextArg('i32');
              textIndex++;
            } else {
              while(1) {
                var precisionChr = HEAP8[(textIndex+1)];
                if (precisionChr < '0'.charCodeAt(0) ||
                    precisionChr > '9'.charCodeAt(0)) break;
                precision = precision * 10 + (precisionChr - '0'.charCodeAt(0));
                textIndex++;
              }
            }
            next = HEAP8[(textIndex+1)];
          } else {
            var precision = 6; // Standard default.
          }
  
          // Handle integer sizes. WARNING: These assume a 32-bit architecture!
          var argSize;
          switch (String.fromCharCode(next)) {
            case 'h':
              var nextNext = HEAP8[(textIndex+2)];
              if (nextNext == 'h'.charCodeAt(0)) {
                textIndex++;
                argSize = 1; // char (actually i32 in varargs)
              } else {
                argSize = 2; // short (actually i32 in varargs)
              }
              break;
            case 'l':
              var nextNext = HEAP8[(textIndex+2)];
              if (nextNext == 'l'.charCodeAt(0)) {
                textIndex++;
                argSize = 8; // long long
              } else {
                argSize = 4; // long
              }
              break;
            case 'L': // long long
            case 'q': // int64_t
            case 'j': // intmax_t
              argSize = 8;
              break;
            case 'z': // size_t
            case 't': // ptrdiff_t
            case 'I': // signed ptrdiff_t or unsigned size_t
              argSize = 4;
              break;
            default:
              argSize = null;
          }
          if (argSize) textIndex++;
          next = HEAP8[(textIndex+1)];
  
          // Handle type specifier.
          if (['d', 'i', 'u', 'o', 'x', 'X', 'p'].indexOf(String.fromCharCode(next)) != -1) {
            // Integer.
            var signed = next == 'd'.charCodeAt(0) || next == 'i'.charCodeAt(0);
            argSize = argSize || 4;
            var currArg = getNextArg('i' + (argSize * 8));
            var origArg = currArg;
            var argText;
            // Flatten i64-1 [low, high] into a (slightly rounded) double
            if (argSize == 8) {
              currArg = Runtime.makeBigInt(currArg[0], currArg[1], next == 'u'.charCodeAt(0));
            }
            // Truncate to requested size.
            if (argSize <= 4) {
              var limit = Math.pow(256, argSize) - 1;
              currArg = (signed ? reSign : unSign)(currArg & limit, argSize * 8);
            }
            // Format the number.
            var currAbsArg = Math.abs(currArg);
            var prefix = '';
            if (next == 'd'.charCodeAt(0) || next == 'i'.charCodeAt(0)) {
              if (argSize == 8 && i64Math) argText = i64Math.stringify(origArg[0], origArg[1]); else
              argText = reSign(currArg, 8 * argSize, 1).toString(10);
            } else if (next == 'u'.charCodeAt(0)) {
              if (argSize == 8 && i64Math) argText = i64Math.stringify(origArg[0], origArg[1], true); else
              argText = unSign(currArg, 8 * argSize, 1).toString(10);
              currArg = Math.abs(currArg);
            } else if (next == 'o'.charCodeAt(0)) {
              argText = (flagAlternative ? '0' : '') + currAbsArg.toString(8);
            } else if (next == 'x'.charCodeAt(0) || next == 'X'.charCodeAt(0)) {
              prefix = flagAlternative ? '0x' : '';
              if (currArg < 0) {
                // Represent negative numbers in hex as 2's complement.
                currArg = -currArg;
                argText = (currAbsArg - 1).toString(16);
                var buffer = [];
                for (var i = 0; i < argText.length; i++) {
                  buffer.push((0xF - parseInt(argText[i], 16)).toString(16));
                }
                argText = buffer.join('');
                while (argText.length < argSize * 2) argText = 'f' + argText;
              } else {
                argText = currAbsArg.toString(16);
              }
              if (next == 'X'.charCodeAt(0)) {
                prefix = prefix.toUpperCase();
                argText = argText.toUpperCase();
              }
            } else if (next == 'p'.charCodeAt(0)) {
              if (currAbsArg === 0) {
                argText = '(nil)';
              } else {
                prefix = '0x';
                argText = currAbsArg.toString(16);
              }
            }
            if (precisionSet) {
              while (argText.length < precision) {
                argText = '0' + argText;
              }
            }
  
            // Add sign if needed
            if (flagAlwaysSigned) {
              if (currArg < 0) {
                prefix = '-' + prefix;
              } else {
                prefix = '+' + prefix;
              }
            }
  
            // Add padding.
            while (prefix.length + argText.length < width) {
              if (flagLeftAlign) {
                argText += ' ';
              } else {
                if (flagZeroPad) {
                  argText = '0' + argText;
                } else {
                  prefix = ' ' + prefix;
                }
              }
            }
  
            // Insert the result into the buffer.
            argText = prefix + argText;
            argText.split('').forEach(function(chr) {
              ret.push(chr.charCodeAt(0));
            });
          } else if (['f', 'F', 'e', 'E', 'g', 'G'].indexOf(String.fromCharCode(next)) != -1) {
            // Float.
            var currArg = getNextArg('double');
            var argText;
  
            if (isNaN(currArg)) {
              argText = 'nan';
              flagZeroPad = false;
            } else if (!isFinite(currArg)) {
              argText = (currArg < 0 ? '-' : '') + 'inf';
              flagZeroPad = false;
            } else {
              var isGeneral = false;
              var effectivePrecision = Math.min(precision, 20);
  
              // Convert g/G to f/F or e/E, as per:
              // http://pubs.opengroup.org/onlinepubs/9699919799/functions/printf.html
              if (next == 'g'.charCodeAt(0) || next == 'G'.charCodeAt(0)) {
                isGeneral = true;
                precision = precision || 1;
                var exponent = parseInt(currArg.toExponential(effectivePrecision).split('e')[1], 10);
                if (precision > exponent && exponent >= -4) {
                  next = ((next == 'g'.charCodeAt(0)) ? 'f' : 'F').charCodeAt(0);
                  precision -= exponent + 1;
                } else {
                  next = ((next == 'g'.charCodeAt(0)) ? 'e' : 'E').charCodeAt(0);
                  precision--;
                }
                effectivePrecision = Math.min(precision, 20);
              }
  
              if (next == 'e'.charCodeAt(0) || next == 'E'.charCodeAt(0)) {
                argText = currArg.toExponential(effectivePrecision);
                // Make sure the exponent has at least 2 digits.
                if (/[eE][-+]\d$/.test(argText)) {
                  argText = argText.slice(0, -1) + '0' + argText.slice(-1);
                }
              } else if (next == 'f'.charCodeAt(0) || next == 'F'.charCodeAt(0)) {
                argText = currArg.toFixed(effectivePrecision);
              }
  
              var parts = argText.split('e');
              if (isGeneral && !flagAlternative) {
                // Discard trailing zeros and periods.
                while (parts[0].length > 1 && parts[0].indexOf('.') != -1 &&
                       (parts[0].slice(-1) == '0' || parts[0].slice(-1) == '.')) {
                  parts[0] = parts[0].slice(0, -1);
                }
              } else {
                // Make sure we have a period in alternative mode.
                if (flagAlternative && argText.indexOf('.') == -1) parts[0] += '.';
                // Zero pad until required precision.
                while (precision > effectivePrecision++) parts[0] += '0';
              }
              argText = parts[0] + (parts.length > 1 ? 'e' + parts[1] : '');
  
              // Capitalize 'E' if needed.
              if (next == 'E'.charCodeAt(0)) argText = argText.toUpperCase();
  
              // Add sign.
              if (flagAlwaysSigned && currArg >= 0) {
                argText = '+' + argText;
              }
            }
  
            // Add padding.
            while (argText.length < width) {
              if (flagLeftAlign) {
                argText += ' ';
              } else {
                if (flagZeroPad && (argText[0] == '-' || argText[0] == '+')) {
                  argText = argText[0] + '0' + argText.slice(1);
                } else {
                  argText = (flagZeroPad ? '0' : ' ') + argText;
                }
              }
            }
  
            // Adjust case.
            if (next < 'a'.charCodeAt(0)) argText = argText.toUpperCase();
  
            // Insert the result into the buffer.
            argText.split('').forEach(function(chr) {
              ret.push(chr.charCodeAt(0));
            });
          } else if (next == 's'.charCodeAt(0)) {
            // String.
            var arg = getNextArg('i8*') || nullString;
            var argLength = String_len(arg);
            if (precisionSet) argLength = Math.min(argLength, precision);
            if (!flagLeftAlign) {
              while (argLength < width--) {
                ret.push(' '.charCodeAt(0));
              }
            }
            for (var i = 0; i < argLength; i++) {
              ret.push(HEAPU8[(arg++)]);
            }
            if (flagLeftAlign) {
              while (argLength < width--) {
                ret.push(' '.charCodeAt(0));
              }
            }
          } else if (next == 'c'.charCodeAt(0)) {
            // Character.
            if (flagLeftAlign) ret.push(getNextArg('i8'));
            while (--width > 0) {
              ret.push(' '.charCodeAt(0));
            }
            if (!flagLeftAlign) ret.push(getNextArg('i8'));
          } else if (next == 'n'.charCodeAt(0)) {
            // Write the length written so far to the next parameter.
            var ptr = getNextArg('i32*');
            HEAP32[((ptr)>>2)]=ret.length
          } else if (next == '%'.charCodeAt(0)) {
            // Literal percent sign.
            ret.push(curr);
          } else {
            // Unknown specifiers remain untouched.
            for (var i = startTextIndex; i < textIndex + 2; i++) {
              ret.push(HEAP8[(i)]);
            }
          }
          textIndex += 2;
          // TODO: Support a/A (hex float) and m (last error) specifiers.
          // TODO: Support %1${specifier} for arg selection.
        } else {
          ret.push(curr);
          textIndex += 1;
        }
      }
      return ret;
    }function _fprintf(stream, format, varargs) {
      // int fprintf(FILE *restrict stream, const char *restrict format, ...);
      // http://pubs.opengroup.org/onlinepubs/000095399/functions/printf.html
      var result = __formatString(format, varargs);
      var stack = Runtime.stackSave();
      var ret = _fwrite(allocate(result, 'i8', ALLOC_STACK), 1, result.length, stream);
      Runtime.stackRestore(stack);
      return ret;
    }function _printf(format, varargs) {
      // int printf(const char *restrict format, ...);
      // http://pubs.opengroup.org/onlinepubs/000095399/functions/printf.html
      var stdout = HEAP32[((_stdout)>>2)];
      return _fprintf(stdout, format, varargs);
    }

  
  function _memset(ptr, value, num, align) {
      // TODO: make these settings, and in memcpy, {{'s
      if (num >= 20) {
        // This is unaligned, but quite large, so work hard to get to aligned settings
        var stop = ptr + num;
        while (ptr % 4) { // no need to check for stop, since we have large num
          HEAP8[ptr++] = value;
        }
        if (value < 0) value += 256; // make it unsigned
        var ptr4 = ptr >> 2, stop4 = stop >> 2, value4 = value | (value << 8) | (value << 16) | (value << 24);
        while (ptr4 < stop4) {
          HEAP32[ptr4++] = value4;
        }
        ptr = ptr4 << 2;
        while (ptr < stop) {
          HEAP8[ptr++] = value;
        }
      } else {
        while (num--) {
          HEAP8[ptr++] = value;
        }
      }
    }var _llvm_memset_p0i8_i32=_memset;
var _util_strdup; // stub for _util_strdup

  
  function _lseek(fildes, offset, whence) {
      // off_t lseek(int fildes, off_t offset, int whence);
      // http://pubs.opengroup.org/onlinepubs/000095399/functions/lseek.html
      if (FS.streams[fildes] && !FS.streams[fildes].object.isDevice) {
        var stream = FS.streams[fildes];
        var position = offset;
        if (whence === 1) {  // SEEK_CUR.
          position += stream.position;
        } else if (whence === 2) {  // SEEK_END.
          position += stream.object.contents.length;
        }
        if (position < 0) {
          ___setErrNo(ERRNO_CODES.EINVAL);
          return -1;
        } else {
          stream.ungotten = [];
          stream.position = position;
          return position;
        }
      } else {
        ___setErrNo(ERRNO_CODES.EBADF);
        return -1;
      }
    }function _fseek(stream, offset, whence) {
      // int fseek(FILE *stream, long offset, int whence);
      // http://pubs.opengroup.org/onlinepubs/000095399/functions/fseek.html
      var ret = _lseek(stream, offset, whence);
      if (ret == -1) {
        return -1;
      } else {
        FS.streams[stream].eof = false;
        return 0;
      }
    }

  function _strlen(ptr) {
      return String_len(ptr);
    }

  
  function _strncmp(px, py, n) {
      var i = 0;
      while (i < n) {
        var x = HEAPU8[((px)+(i))];
        var y = HEAPU8[((py)+(i))];
        if (x == y && x == 0) return 0;
        if (x == 0) return -1;
        if (y == 0) return 1;
        if (x == y) {
          i ++;
          continue;
        } else {
          return x > y ? 1 : -1;
        }
      }
      return 0;
    }function _strcmp(px, py) {
      return _strncmp(px, py, TOTAL_MEMORY);
    }

  function _fflush(stream) {
      // int fflush(FILE *stream);
      // http://pubs.opengroup.org/onlinepubs/000095399/functions/fflush.html
      var flush = function(filedes) {
        // Right now we write all data directly, except for output devices.
        if (FS.streams[filedes] && FS.streams[filedes].object.output) {
          if (!FS.streams[filedes].isTerminal) { // don't flush terminals, it would cause a \n to also appear
            FS.streams[filedes].object.output(null);
          }
        }
      };
      try {
        if (stream === 0) {
          for (var i = 0; i < FS.streams.length; i++) if (FS.streams[i]) flush(i);
        } else {
          flush(stream);
        }
        return 0;
      } catch (e) {
        ___setErrNo(ERRNO_CODES.EIO);
        return -1;
      }
    }

  
  function _fputc(c, stream) {
      // int fputc(int c, FILE *stream);
      // http://pubs.opengroup.org/onlinepubs/000095399/functions/fputc.html
      var chr = unSign(c & 0xFF);
      HEAP8[(_fputc.ret)]=chr
      var ret = _write(stream, _fputc.ret, 1);
      if (ret == -1) {
        if (FS.streams[stream]) FS.streams[stream].error = true;
        return -1;
      } else {
        return chr;
      }
    }var _putc=_fputc;

  
  function __exit(status) {
      // void _exit(int status);
      // http://pubs.opengroup.org/onlinepubs/000095399/functions/exit.html
  
  
      exitRuntime();
      ABORT = true;
  
      throw 'exit(' + status + ') called, at ' + new Error().stack;
    }function _exit(status) {
      __exit(status);
    }
var _llvm_va_start; // stub for _llvm_va_start

  var _vprintf=_printf;

  function _llvm_va_end() {}

  
  function ___errno_location() {
      return ___setErrNo.ret;
    }var ___errno=___errno_location;

  
  
  var ERRNO_MESSAGES={1:"Operation not permitted",2:"No such file or directory",3:"No such process",4:"Interrupted system call",5:"Input/output error",6:"No such device or address",8:"Exec format error",9:"Bad file descriptor",10:"No child processes",11:"Resource temporarily unavailable",12:"Cannot allocate memory",13:"Permission denied",14:"Bad address",16:"Device or resource busy",17:"File exists",18:"Invalid cross-device link",19:"No such device",20:"Not a directory",21:"Is a directory",22:"Invalid argument",23:"Too many open files in system",24:"Too many open files",25:"Inappropriate ioctl for device",26:"Text file busy",27:"File too large",28:"No space left on device",29:"Illegal seek",30:"Read-only file system",31:"Too many links",32:"Broken pipe",33:"Numerical argument out of domain",34:"Numerical result out of range",35:"Resource deadlock avoided",36:"File name too long",37:"No locks available",38:"Function not implemented",39:"Directory not empty",40:"Too many levels of symbolic links",42:"No message of desired type",43:"Identifier removed",60:"Device not a stream",61:"No data available",62:"Timer expired",63:"Out of streams resources",67:"Link has been severed",71:"Protocol error",72:"Multihop attempted",74:"Bad message",75:"Value too large for defined data type",84:"Invalid or incomplete multibyte or wide character",88:"Socket operation on non-socket",89:"Destination address required",90:"Message too long",91:"Protocol wrong type for socket",92:"Protocol not available",93:"Protocol not supported",95:"Operation not supported",97:"Address family not supported by protocol",98:"Address already in use",99:"Cannot assign requested address",100:"Network is down",101:"Network is unreachable",102:"Network dropped connection on reset",103:"Software caused connection abort",104:"Connection reset by peer",105:"No buffer space available",106:"Transport endpoint is already connected",107:"Transport endpoint is not connected",110:"Connection timed out",111:"Connection refused",113:"No route to host",114:"Operation already in progress",115:"Operation now in progress",116:"Stale NFS file handle",122:"Disk quota exceeded",125:"Operation canceled",130:"Owner died",131:"State not recoverable"};function _strerror_r(errnum, strerrbuf, buflen) {
      if (errnum in ERRNO_MESSAGES) {
        if (ERRNO_MESSAGES[errnum].length > buflen - 1) {
          return ___setErrNo(ERRNO_CODES.ERANGE);
        } else {
          var msg = ERRNO_MESSAGES[errnum];
          for (var i = 0; i < msg.length; i++) {
            HEAP8[((strerrbuf)+(i))]=msg.charCodeAt(i)
          }
          HEAP8[((strerrbuf)+(i))]=0
          return 0;
        }
      } else {
        return ___setErrNo(ERRNO_CODES.EINVAL);
      }
    }function _strerror(errnum) {
      if (!_strerror.buffer) _strerror.buffer = _malloc(256);
      _strerror_r(errnum, _strerror.buffer, 256);
      return _strerror.buffer;
    }



  function _malloc(bytes) {
      /* Over-allocate to make sure it is byte-aligned by 8.
       * This will leak memory, but this is only the dummy
       * implementation (replaced by dlmalloc normally) so
       * not an issue.
       */
      ptr = Runtime.staticAlloc(bytes + 8);
      return (ptr+8) & 0xFFFFFFF8;
    }
  Module["_malloc"] = _malloc;

  function _free(){}
  Module["_free"] = _free;

  var Browser={mainLoop:{scheduler:null,shouldPause:false,paused:false,queue:[],pause:function () {
          Browser.mainLoop.shouldPause = true;
        },resume:function () {
          if (Browser.mainLoop.paused) {
            Browser.mainLoop.paused = false;
            Browser.mainLoop.scheduler();
          }
          Browser.mainLoop.shouldPause = false;
        },updateStatus:function () {
          if (Module['setStatus']) {
            var message = Module['statusMessage'] || 'Please wait...';
            var remaining = Browser.mainLoop.remainingBlockers;
            var expected = Browser.mainLoop.expectedBlockers;
            if (remaining) {
              if (remaining < expected) {
                Module['setStatus'](message + ' (' + (expected - remaining) + '/' + expected + ')');
              } else {
                Module['setStatus'](message);
              }
            } else {
              Module['setStatus']('');
            }
          }
        }},pointerLock:false,moduleContextCreatedCallbacks:[],workers:[],ensureObjects:function () {
        if (Browser.ensured) return;
        Browser.ensured = true;
        try {
          new Blob();
          Browser.hasBlobConstructor = true;
        } catch(e) {
          Browser.hasBlobConstructor = false;
          console.log("warning: no blob constructor, cannot create blobs with mimetypes");
        }
        Browser.BlobBuilder = typeof MozBlobBuilder != "undefined" ? MozBlobBuilder : (typeof WebKitBlobBuilder != "undefined" ? WebKitBlobBuilder : (!Browser.hasBlobConstructor ? console.log("warning: no BlobBuilder") : null));
        Browser.URLObject = typeof window != "undefined" ? (window.URL ? window.URL : window.webkitURL) : console.log("warning: cannot create object URLs");
  
        // Support for plugins that can process preloaded files. You can add more of these to
        // your app by creating and appending to Module.preloadPlugins.
        //
        // Each plugin is asked if it can handle a file based on the file's name. If it can,
        // it is given the file's raw data. When it is done, it calls a callback with the file's
        // (possibly modified) data. For example, a plugin might decompress a file, or it
        // might create some side data structure for use later (like an Image element, etc.).
  
        function getMimetype(name) {
          return {
            'jpg': 'image/jpeg',
            'png': 'image/png',
            'bmp': 'image/bmp',
            'ogg': 'audio/ogg',
            'wav': 'audio/wav',
            'mp3': 'audio/mpeg'
          }[name.substr(-3)];
          return ret;
        }
  
        if (!Module["preloadPlugins"]) Module["preloadPlugins"] = [];
  
        var imagePlugin = {};
        imagePlugin['canHandle'] = function(name) {
          return name.substr(-4) in { '.jpg': 1, '.png': 1, '.bmp': 1 };
        };
        imagePlugin['handle'] = function(byteArray, name, onload, onerror) {
          var b = null;
          if (Browser.hasBlobConstructor) {
            try {
              b = new Blob([byteArray], { type: getMimetype(name) });
            } catch(e) {
              Runtime.warnOnce('Blob constructor present but fails: ' + e + '; falling back to blob builder');
            }
          }
          if (!b) {
            var bb = new Browser.BlobBuilder();
            bb.append((new Uint8Array(byteArray)).buffer); // we need to pass a buffer, and must copy the array to get the right data range
            b = bb.getBlob();
          }
          var url = Browser.URLObject.createObjectURL(b);
          assert(typeof url == 'string', 'createObjectURL must return a url as a string');
          var img = new Image();
          img.onload = function() {
            assert(img.complete, 'Image ' + name + ' could not be decoded');
            var canvas = document.createElement('canvas');
            canvas.width = img.width;
            canvas.height = img.height;
            var ctx = canvas.getContext('2d');
            ctx.drawImage(img, 0, 0);
            Module["preloadedImages"][name] = canvas;
            Browser.URLObject.revokeObjectURL(url);
            if (onload) onload(byteArray);
          };
          img.onerror = function(event) {
            console.log('Image ' + url + ' could not be decoded');
            if (onerror) onerror();
          };
          img.src = url;
        };
        Module['preloadPlugins'].push(imagePlugin);
  
        var audioPlugin = {};
        audioPlugin['canHandle'] = function(name) {
          return name.substr(-4) in { '.ogg': 1, '.wav': 1, '.mp3': 1 };
        };
        audioPlugin['handle'] = function(byteArray, name, onload, onerror) {
          var done = false;
          function finish(audio) {
            if (done) return;
            done = true;
            Module["preloadedAudios"][name] = audio;
            if (onload) onload(byteArray);
          }
          function fail() {
            if (done) return;
            done = true;
            Module["preloadedAudios"][name] = new Audio(); // empty shim
            if (onerror) onerror();
          }
          if (Browser.hasBlobConstructor) {
            try {
              var b = new Blob([byteArray], { type: getMimetype(name) });
            } catch(e) {
              return fail();
            }
            var url = Browser.URLObject.createObjectURL(b); // XXX we never revoke this!
            assert(typeof url == 'string', 'createObjectURL must return a url as a string');
            var audio = new Audio();
            audio.addEventListener('canplaythrough', function() { finish(audio) }, false); // use addEventListener due to chromium bug 124926
            audio.onerror = function(event) {
              if (done) return;
              console.log('warning: browser could not fully decode audio ' + name + ', trying slower base64 approach');
              function encode64(data) {
                var BASE = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/';
                var PAD = '=';
                var ret = '';
                var leftchar = 0;
                var leftbits = 0;
                for (var i = 0; i < data.length; i++) {
                  leftchar = (leftchar << 8) | data[i];
                  leftbits += 8;
                  while (leftbits >= 6) {
                    var curr = (leftchar >> (leftbits-6)) & 0x3f;
                    leftbits -= 6;
                    ret += BASE[curr];
                  }
                }
                if (leftbits == 2) {
                  ret += BASE[(leftchar&3) << 4];
                  ret += PAD + PAD;
                } else if (leftbits == 4) {
                  ret += BASE[(leftchar&0xf) << 2];
                  ret += PAD;
                }
                return ret;
              }
              audio.src = 'data:audio/x-' + name.substr(-3) + ';base64,' + encode64(byteArray);
              finish(audio); // we don't wait for confirmation this worked - but it's worth trying
            };
            audio.src = url;
            // workaround for chrome bug 124926 - we do not always get oncanplaythrough or onerror
            setTimeout(function() {
              finish(audio); // try to use it even though it is not necessarily ready to play
            }, 10000);
          } else {
            return fail();
          }
        };
        Module['preloadPlugins'].push(audioPlugin);
      },createContext:function (canvas, useWebGL, setInModule) {
        try {
          var ctx = canvas.getContext(useWebGL ? 'experimental-webgl' : '2d');
          if (!ctx) throw ':(';
        } catch (e) {
          Module.print('Could not create canvas - ' + e);
          return null;
        }
        if (useWebGL) {
          // Set the background of the WebGL canvas to black
          canvas.style.backgroundColor = "black";
  
          // Warn on context loss
          canvas.addEventListener('webglcontextlost', function(event) {
            alert('WebGL context lost. You will need to reload the page.');
          }, false);
        }
        if (setInModule) {
          Module.ctx = ctx;
          Module.useWebGL = useWebGL;
          Browser.moduleContextCreatedCallbacks.forEach(function(callback) { callback() });
        }
        return ctx;
      },requestFullScreen:function () {
        var canvas = Module['canvas'];
        function fullScreenChange() {
          var isFullScreen = false;
          if ((document['webkitFullScreenElement'] || document['webkitFullscreenElement'] ||
               document['mozFullScreenElement'] || document['mozFullscreenElement'] ||
               document['fullScreenElement'] || document['fullscreenElement']) === canvas) {
            canvas.requestPointerLock = canvas['requestPointerLock'] ||
                                        canvas['mozRequestPointerLock'] ||
                                        canvas['webkitRequestPointerLock'];
            canvas.requestPointerLock();
            isFullScreen = true;
          }
          if (Module['onFullScreen']) Module['onFullScreen'](isFullScreen);
        }
  
        document.addEventListener('fullscreenchange', fullScreenChange, false);
        document.addEventListener('mozfullscreenchange', fullScreenChange, false);
        document.addEventListener('webkitfullscreenchange', fullScreenChange, false);
  
        function pointerLockChange() {
          Browser.pointerLock = document['pointerLockElement'] === canvas ||
                                document['mozPointerLockElement'] === canvas ||
                                document['webkitPointerLockElement'] === canvas;
        }
  
        document.addEventListener('pointerlockchange', pointerLockChange, false);
        document.addEventListener('mozpointerlockchange', pointerLockChange, false);
        document.addEventListener('webkitpointerlockchange', pointerLockChange, false);
  
        canvas.requestFullScreen = canvas['requestFullScreen'] ||
                                   canvas['mozRequestFullScreen'] ||
                                   (canvas['webkitRequestFullScreen'] ? function() { canvas['webkitRequestFullScreen'](Element['ALLOW_KEYBOARD_INPUT']) } : null);
        canvas.requestFullScreen(); 
      },requestAnimationFrame:function (func) {
        if (!window.requestAnimationFrame) {
          window.requestAnimationFrame = window['requestAnimationFrame'] ||
                                         window['mozRequestAnimationFrame'] ||
                                         window['webkitRequestAnimationFrame'] ||
                                         window['msRequestAnimationFrame'] ||
                                         window['oRequestAnimationFrame'] ||
                                         window['setTimeout'];
        }
        window.requestAnimationFrame(func);
      },getMovementX:function (event) {
        return event['movementX'] ||
               event['mozMovementX'] ||
               event['webkitMovementX'] ||
               0;
      },getMovementY:function (event) {
        return event['movementY'] ||
               event['mozMovementY'] ||
               event['webkitMovementY'] ||
               0;
      },xhrLoad:function (url, onload, onerror) {
        var xhr = new XMLHttpRequest();
        xhr.open('GET', url, true);
        xhr.responseType = 'arraybuffer';
        xhr.onload = function() {
          if (xhr.status == 200) {
            onload(xhr.response);
          } else {
            onerror();
          }
        };
        xhr.onerror = onerror;
        xhr.send(null);
      },asyncLoad:function (url, onload, onerror) {
        Browser.xhrLoad(url, function(arrayBuffer) {
          assert(arrayBuffer, 'Loading data file "' + url + '" failed (no arrayBuffer).');
          onload(new Uint8Array(arrayBuffer));
          removeRunDependency('al ' + url);
        }, function(event) {
          if (onerror) {
            onerror();
          } else {
            throw 'Loading data file "' + url + '" failed.';
          }
        });
        addRunDependency('al ' + url);
      },resizeListeners:[],updateResizeListeners:function () {
        var canvas = Module['canvas'];
        Browser.resizeListeners.forEach(function(listener) {
          listener(canvas.width, canvas.height);
        });
      },setCanvasSize:function (width, height, noUpdates) {
        var canvas = Module['canvas'];
        canvas.width = width;
        canvas.height = height;
        if (!noUpdates) Browser.updateResizeListeners();
      }};
__ATINIT__.unshift({ func: function() { if (!Module["noFSInit"] && !FS.init.initialized) FS.init() } });__ATMAIN__.push({ func: function() { FS.ignorePermissions = false } });__ATEXIT__.push({ func: function() { FS.quit() } });Module["FS_createFolder"] = FS.createFolder;Module["FS_createPath"] = FS.createPath;Module["FS_createDataFile"] = FS.createDataFile;Module["FS_createPreloadedFile"] = FS.createPreloadedFile;Module["FS_createLazyFile"] = FS.createLazyFile;Module["FS_createLink"] = FS.createLink;Module["FS_createDevice"] = FS.createDevice;
___setErrNo(0);
_fputc.ret = allocate([0], "i8", ALLOC_STATIC);
Module["requestFullScreen"] = function() { Browser.requestFullScreen() };
  Module["requestAnimationFrame"] = function(func) { Browser.requestAnimationFrame(func) };
  Module["pauseMainLoop"] = function() { Browser.mainLoop.pause() };
  Module["resumeMainLoop"] = function() { Browser.mainLoop.resume() };
  

// === Auto-generated postamble setup entry stuff ===

Module.callMain = function callMain(args) {
  var argc = args.length+1;
  function pad() {
    for (var i = 0; i < 4-1; i++) {
      argv.push(0);
    }
  }
  var argv = [allocate(intArrayFromString("/bin/this.program"), 'i8', ALLOC_STATIC) ];
  pad();
  for (var i = 0; i < argc-1; i = i + 1) {
    argv.push(allocate(intArrayFromString(args[i]), 'i8', ALLOC_STATIC));
    pad();
  }
  argv.push(0);
  argv = allocate(argv, 'i32', ALLOC_STATIC);

  return _main(argc, argv, 0);
}































var _asm_instr;



var _prog_print_statement_t;


var _stdout;














































































STRING_TABLE.__str=allocate([101,120,101,99,46,99,0] /* exec.c\00 */, "i8", ALLOC_STATIC);
STRING_TABLE.__str1=allocate([114,98,0] /* rb\00 */, "i8", ALLOC_STATIC);
STRING_TABLE.__str2=allocate([102,97,105,108,101,100,32,116,111,32,114,101,97,100,32,104,101,97,100,101,114,32,102,114,111,109,32,39,37,115,39,0] /* failed to read heade */, "i8", ALLOC_STATIC);
STRING_TABLE.__str3=allocate([104,101,97,100,101,114,32,115,97,121,115,32,116,104,105,115,32,105,115,32,97,32,118,101,114,115,105,111,110,32,37,105,32,112,114,111,103,115,44,32,119,101,32,110,101,101,100,32,118,101,114,115,105,111,110,32,54,10,0] /* header says this is  */, "i8", ALLOC_STATIC);
STRING_TABLE.__str4=allocate([102,97,105,108,101,100,32,116,111,32,97,108,108,111,99,97,116,101,32,112,114,111,103,114,97,109,32,100,97,116,97,10,0] /* failed to allocate p */, "i8", ALLOC_STATIC);
STRING_TABLE.__str5=allocate([102,97,105,108,101,100,32,116,111,32,115,116,111,114,101,32,112,114,111,103,114,97,109,32,110,97,109,101,0] /* failed to store prog */, "i8", ALLOC_STATIC);
STRING_TABLE.__str6=allocate([115,101,101,107,32,102,97,105,108,101,100,0] /* seek failed\00 */, "i8", ALLOC_STATIC);
STRING_TABLE.__str7=allocate([114,101,97,100,32,102,97,105,108,101,100,0] /* read failed\00 */, "i8", ALLOC_STATIC);
STRING_TABLE.__str8=allocate([102,97,105,108,101,100,32,116,111,32,97,108,108,111,99,97,116,101,32,119,111,114,108,100,32,101,110,116,105,116,121,10,0] /* failed to allocate w */, "i8", ALLOC_STATIC);
STRING_TABLE.__str9=allocate([102,97,105,108,101,100,32,116,111,32,97,108,108,111,99,97,116,101,32,119,111,114,108,100,32,100,97,116,97,10,0] /* failed to allocate w */, "i8", ALLOC_STATIC);
STRING_TABLE.__str10=allocate([60,60,60,105,110,118,97,108,105,100,32,115,116,114,105,110,103,62,62,62,0] /* ___invalid string___ */, "i8", ALLOC_STATIC);
STRING_TABLE.__str11=allocate([65,99,99,101,115,115,105,110,103,32,111,117,116,32,111,102,32,98,111,117,110,100,115,32,101,100,105,99,116,32,37,105,10,0] /* Accessing out of bou */, "i8", ALLOC_STATIC);
STRING_TABLE.__str12=allocate([70,97,105,108,101,100,32,116,111,32,97,108,108,111,99,97,116,101,32,101,110,116,105,116,121,10,0] /* Failed to allocate e */, "i8", ALLOC_STATIC);
STRING_TABLE.__str13=allocate([84,114,121,105,110,103,32,116,111,32,102,114,101,101,32,119,111,114,108,100,32,101,110,116,105,116,121,10,0] /* Trying to free world */, "i8", ALLOC_STATIC);
STRING_TABLE.__str14=allocate([84,114,121,105,110,103,32,116,111,32,102,114,101,101,32,111,117,116,32,111,102,32,98,111,117,110,100,115,32,101,110,116,105,116,121,10,0] /* Trying to free out o */, "i8", ALLOC_STATIC);
STRING_TABLE.__str15=allocate([68,111,117,98,108,101,32,102,114,101,101,32,111,110,32,101,110,116,105,116,121,10,0] /* Double free on entit */, "i8", ALLOC_STATIC);
STRING_TABLE.__str16=allocate([73,108,108,101,103,97,108,32,105,110,115,116,114,117,99,116,105,111,110,32,105,110,32,37,115,10,0] /* Illegal instruction  */, "i8", ALLOC_STATIC);
STRING_TABLE.__str17=allocate([112,114,111,103,115,32,96,37,115,96,32,97,116,116,101,109,112,116,101,100,32,116,111,32,114,101,97,100,32,97,110,32,111,117,116,32,111,102,32,98,111,117,110,100,115,32,101,110,116,105,116,121,0] /* progs `%s` attempted */, "i8", ALLOC_STATIC);
STRING_TABLE.__str18=allocate([112,114,111,103,32,96,37,115,96,32,97,116,116,101,109,112,116,101,100,32,116,111,32,114,101,97,100,32,97,110,32,105,110,118,97,108,105,100,32,102,105,101,108,100,32,102,114,111,109,32,101,110,116,105,116,121,32,40,37,105,41,0] /* prog `%s` attempted  */, "i8", ALLOC_STATIC);
STRING_TABLE.__str19=allocate([112,114,111,103,32,96,37,115,96,32,97,116,116,101,109,112,116,101,100,32,116,111,32,97,100,100,114,101,115,115,32,97,110,32,111,117,116,32,111,102,32,98,111,117,110,100,115,32,101,110,116,105,116,121,32,37,105,0] /* prog `%s` attempted  */, "i8", ALLOC_STATIC);
STRING_TABLE.__str20=allocate([96,37,115,96,32,97,116,116,101,109,112,116,101,100,32,116,111,32,119,114,105,116,101,32,116,111,32,97,110,32,111,117,116,32,111,102,32,98,111,117,110,100,115,32,101,100,105,99,116,32,40,37,105,41,0] /* `%s` attempted to wr */, "i8", ALLOC_STATIC);
STRING_TABLE.__str21=allocate([96,37,115,96,32,116,114,105,101,100,32,116,111,32,97,115,115,105,103,110,32,116,111,32,119,111,114,108,100,46,37,115,32,40,102,105,101,108,100,32,37,105,41,10,0] /* `%s` tried to assign */, "i8", ALLOC_STATIC);
STRING_TABLE.__str22=allocate([96,37,115,96,32,104,105,116,32,116,104,101,32,114,117,110,97,119,97,121,32,108,111,111,112,32,99,111,117,110,116,101,114,32,108,105,109,105,116,32,111,102,32,37,108,105,32,106,117,109,112,115,0] /* `%s` hit the runaway */, "i8", ALLOC_STATIC);
STRING_TABLE.__str23=allocate([78,85,76,76,32,102,117,110,99,116,105,111,110,32,105,110,32,96,37,115,96,0] /* NULL function in `%s */, "i8", ALLOC_STATIC);
STRING_TABLE.__str24=allocate([67,65,76,76,32,111,117,116,115,105,100,101,32,116,104,101,32,112,114,111,103,114,97,109,32,105,110,32,96,37,115,96,0] /* CALL outside the pro */, "i8", ALLOC_STATIC);
STRING_TABLE.__str25=allocate([78,111,32,115,117,99,104,32,98,117,105,108,116,105,110,32,35,37,105,32,105,110,32,37,115,33,32,84,114,121,32,117,112,100,97,116,105,110,103,32,121,111,117,114,32,103,109,113,99,99,32,115,111,117,114,99,101,115,0] /* No such builtin #%i  */, "i8", ALLOC_STATIC);
STRING_TABLE.__str26=allocate([96,37,115,96,32,116,114,105,101,100,32,116,111,32,101,120,101,99,117,116,101,32,97,32,83,84,65,84,69,32,111,112,101,114,97,116,105,111,110,0] /* `%s` tried to execut */, "i8", ALLOC_STATIC);
STRING_TABLE.__str27=allocate([60,105,108,108,101,103,97,108,32,105,110,115,116,114,117,99,116,105,111,110,32,37,100,62,10,0] /* _illegal instruction */, "i8", ALLOC_STATIC);
STRING_TABLE.__str28=allocate([32,60,62,32,37,45,49,50,115,0] /*  __ %-12s\00 */, "i8", ALLOC_STATIC);
_asm_instr=allocate([0, 0, 0, 0, 1, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 6, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 5, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 6, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 7, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 8, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 9, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0], ["*",0,0,0,"i32",0,0,0,"i32",0,0,0,"*",0,0,0,"i32",0,0,0,"i32",0,0,0,"*",0,0,0,"i32",0,0,0,"i32",0,0,0,"*",0,0,0,"i32",0,0,0,"i32",0,0,0,"*",0,0,0,"i32",0,0,0,"i32",0,0,0,"*",0,0,0,"i32",0,0,0,"i32",0,0,0,"*",0,0,0,"i32",0,0,0,"i32",0,0,0,"*",0,0,0,"i32",0,0,0,"i32",0,0,0,"*",0,0,0,"i32",0,0,0,"i32",0,0,0,"*",0,0,0,"i32",0,0,0,"i32",0,0,0,"*",0,0,0,"i32",0,0,0,"i32",0,0,0,"*",0,0,0,"i32",0,0,0,"i32",0,0,0,"*",0,0,0,"i32",0,0,0,"i32",0,0,0,"*",0,0,0,"i32",0,0,0,"i32",0,0,0,"*",0,0,0,"i32",0,0,0,"i32",0,0,0,"*",0,0,0,"i32",0,0,0,"i32",0,0,0,"*",0,0,0,"i32",0,0,0,"i32",0,0,0,"*",0,0,0,"i32",0,0,0,"i32",0,0,0,"*",0,0,0,"i32",0,0,0,"i32",0,0,0,"*",0,0,0,"i32",0,0,0,"i32",0,0,0,"*",0,0,0,"i32",0,0,0,"i32",0,0,0,"*",0,0,0,"i32",0,0,0,"i32",0,0,0,"*",0,0,0,"i32",0,0,0,"i32",0,0,0,"*",0,0,0,"i32",0,0,0,"i32",0,0,0,"*",0,0,0,"i32",0,0,0,"i32",0,0,0,"*",0,0,0,"i32",0,0,0,"i32",0,0,0,"*",0,0,0,"i32",0,0,0,"i32",0,0,0,"*",0,0,0,"i32",0,0,0,"i32",0,0,0,"*",0,0,0,"i32",0,0,0,"i32",0,0,0,"*",0,0,0,"i32",0,0,0,"i32",0,0,0,"*",0,0,0,"i32",0,0,0,"i32",0,0,0,"*",0,0,0,"i32",0,0,0,"i32",0,0,0,"*",0,0,0,"i32",0,0,0,"i32",0,0,0,"*",0,0,0,"i32",0,0,0,"i32",0,0,0,"*",0,0,0,"i32",0,0,0,"i32",0,0,0,"*",0,0,0,"i32",0,0,0,"i32",0,0,0,"*",0,0,0,"i32",0,0,0,"i32",0,0,0,"*",0,0,0,"i32",0,0,0,"i32",0,0,0,"*",0,0,0,"i32",0,0,0,"i32",0,0,0,"*",0,0,0,"i32",0,0,0,"i32",0,0,0,"*",0,0,0,"i32",0,0,0,"i32",0,0,0,"*",0,0,0,"i32",0,0,0,"i32",0,0,0,"*",0,0,0,"i32",0,0,0,"i32",0,0,0,"*",0,0,0,"i32",0,0,0,"i32",0,0,0,"*",0,0,0,"i32",0,0,0,"i32",0,0,0,"*",0,0,0,"i32",0,0,0,"i32",0,0,0,"*",0,0,0,"i32",0,0,0,"i32",0,0,0,"*",0,0,0,"i32",0,0,0,"i32",0,0,0,"*",0,0,0,"i32",0,0,0,"i32",0,0,0,"*",0,0,0,"i32",0,0,0,"i32",0,0,0,"*",0,0,0,"i32",0,0,0,"i32",0,0,0,"*",0,0,0,"i32",0,0,0,"i32",0,0,0,"*",0,0,0,"i32",0,0,0,"i32",0,0,0,"*",0,0,0,"i32",0,0,0,"i32",0,0,0,"*",0,0,0,"i32",0,0,0,"i32",0,0,0,"*",0,0,0,"i32",0,0,0,"i32",0,0,0,"*",0,0,0,"i32",0,0,0,"i32",0,0,0,"*",0,0,0,"i32",0,0,0,"i32",0,0,0,"*",0,0,0,"i32",0,0,0,"i32",0,0,0,"*",0,0,0,"i32",0,0,0,"i32",0,0,0,"*",0,0,0,"i32",0,0,0,"i32",0,0,0,"*",0,0,0,"i32",0,0,0,"i32",0,0,0,"*",0,0,0,"i32",0,0,0,"i32",0,0,0,"*",0,0,0,"i32",0,0,0,"i32",0,0,0,"*",0,0,0,"i32",0,0,0,"i32",0,0,0,"*",0,0,0,"i32",0,0,0,"i32",0,0,0,"*",0,0,0,"i32",0,0,0,"i32",0,0,0], ALLOC_STATIC);
STRING_TABLE.__str29=allocate([37,100,10,0] /* %d\0A\00 */, "i8", ALLOC_STATIC);
STRING_TABLE.__str30=allocate([10,0] /* \0A\00 */, "i8", ALLOC_STATIC);
STRING_TABLE.__str31=allocate([37,105,10,0] /* %i\0A\00 */, "i8", ALLOC_STATIC);
_prog_print_statement_t=allocate([2, 0, 0, 0, 2, 0, 0, 0, 2, 0, 0, 0], ["i32",0,0,0,"i32",0,0,0,"i32",0,0,0], ALLOC_STATIC);
STRING_TABLE.__str32=allocate([40,110,111,110,101,41,44,32,32,32,32,32,32,32,32,32,32,0] /* (none),          \00 */, "i8", ALLOC_STATIC);
STRING_TABLE.__str33=allocate([40,110,111,110,101,41,0] /* (none)\00 */, "i8", ALLOC_STATIC);
STRING_TABLE._trace_print_global_spaces=allocate([32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,0] /*                      */, "i8", ALLOC_STATIC);
STRING_TABLE.__str34=allocate([60,110,117,108,108,62,44,0] /* _null_,\00 */, "i8", ALLOC_STATIC);
STRING_TABLE.__str35=allocate([36,0] /* $\00 */, "i8", ALLOC_STATIC);
STRING_TABLE.__str36=allocate([37,115,32,0] /* %s \00 */, "i8", ALLOC_STATIC);
STRING_TABLE.__str37=allocate([91,64,37,117,93,32,0] /* [@%u] \00 */, "i8", ALLOC_STATIC);
STRING_TABLE.__str38=allocate([40,37,105,41,44,0] /* (%i),\00 */, "i8", ALLOC_STATIC);
STRING_TABLE.__str39=allocate([39,37,103,32,37,103,32,37,103,39,44,0] /* '%g %g %g',\00 */, "i8", ALLOC_STATIC);
STRING_TABLE.__str40=allocate([44,0] /* ,\00 */, "i8", ALLOC_STATIC);
STRING_TABLE.__str41=allocate([37,103,44,0] /* %g,\00 */, "i8", ALLOC_STATIC);
STRING_TABLE.__str42=allocate([68,79,78,69,0] /* DONE\00 */, "i8", ALLOC_STATIC);
STRING_TABLE.__str43=allocate([77,85,76,95,70,0] /* MUL_F\00 */, "i8", ALLOC_STATIC);
STRING_TABLE.__str44=allocate([77,85,76,95,86,0] /* MUL_V\00 */, "i8", ALLOC_STATIC);
STRING_TABLE.__str45=allocate([77,85,76,95,70,86,0] /* MUL_FV\00 */, "i8", ALLOC_STATIC);
STRING_TABLE.__str46=allocate([77,85,76,95,86,70,0] /* MUL_VF\00 */, "i8", ALLOC_STATIC);
STRING_TABLE.__str47=allocate([68,73,86,0] /* DIV\00 */, "i8", ALLOC_STATIC);
STRING_TABLE.__str48=allocate([65,68,68,95,70,0] /* ADD_F\00 */, "i8", ALLOC_STATIC);
STRING_TABLE.__str49=allocate([65,68,68,95,86,0] /* ADD_V\00 */, "i8", ALLOC_STATIC);
STRING_TABLE.__str50=allocate([83,85,66,95,70,0] /* SUB_F\00 */, "i8", ALLOC_STATIC);
STRING_TABLE.__str51=allocate([83,85,66,95,86,0] /* SUB_V\00 */, "i8", ALLOC_STATIC);
STRING_TABLE.__str52=allocate([69,81,95,70,0] /* EQ_F\00 */, "i8", ALLOC_STATIC);
STRING_TABLE.__str53=allocate([69,81,95,86,0] /* EQ_V\00 */, "i8", ALLOC_STATIC);
STRING_TABLE.__str54=allocate([69,81,95,83,0] /* EQ_S\00 */, "i8", ALLOC_STATIC);
STRING_TABLE.__str55=allocate([69,81,95,69,0] /* EQ_E\00 */, "i8", ALLOC_STATIC);
STRING_TABLE.__str56=allocate([69,81,95,70,78,67,0] /* EQ_FNC\00 */, "i8", ALLOC_STATIC);
STRING_TABLE.__str57=allocate([78,69,95,70,0] /* NE_F\00 */, "i8", ALLOC_STATIC);
STRING_TABLE.__str58=allocate([78,69,95,86,0] /* NE_V\00 */, "i8", ALLOC_STATIC);
STRING_TABLE.__str59=allocate([78,69,95,83,0] /* NE_S\00 */, "i8", ALLOC_STATIC);
STRING_TABLE.__str60=allocate([78,69,95,69,0] /* NE_E\00 */, "i8", ALLOC_STATIC);
STRING_TABLE.__str61=allocate([78,69,95,70,78,67,0] /* NE_FNC\00 */, "i8", ALLOC_STATIC);
STRING_TABLE.__str62=allocate([76,69,0] /* LE\00 */, "i8", ALLOC_STATIC);
STRING_TABLE.__str63=allocate([71,69,0] /* GE\00 */, "i8", ALLOC_STATIC);
STRING_TABLE.__str64=allocate([76,84,0] /* LT\00 */, "i8", ALLOC_STATIC);
STRING_TABLE.__str65=allocate([71,84,0] /* GT\00 */, "i8", ALLOC_STATIC);
STRING_TABLE.__str66=allocate([70,73,69,76,68,95,70,0] /* FIELD_F\00 */, "i8", ALLOC_STATIC);
STRING_TABLE.__str67=allocate([70,73,69,76,68,95,86,0] /* FIELD_V\00 */, "i8", ALLOC_STATIC);
STRING_TABLE.__str68=allocate([70,73,69,76,68,95,83,0] /* FIELD_S\00 */, "i8", ALLOC_STATIC);
STRING_TABLE.__str69=allocate([70,73,69,76,68,95,69,78,84,0] /* FIELD_ENT\00 */, "i8", ALLOC_STATIC);
STRING_TABLE.__str70=allocate([70,73,69,76,68,95,70,76,68,0] /* FIELD_FLD\00 */, "i8", ALLOC_STATIC);
STRING_TABLE.__str71=allocate([70,73,69,76,68,95,70,78,67,0] /* FIELD_FNC\00 */, "i8", ALLOC_STATIC);
STRING_TABLE.__str72=allocate([65,68,68,82,69,83,83,0] /* ADDRESS\00 */, "i8", ALLOC_STATIC);
STRING_TABLE.__str73=allocate([83,84,79,82,69,95,70,0] /* STORE_F\00 */, "i8", ALLOC_STATIC);
STRING_TABLE.__str74=allocate([83,84,79,82,69,95,86,0] /* STORE_V\00 */, "i8", ALLOC_STATIC);
STRING_TABLE.__str75=allocate([83,84,79,82,69,95,83,0] /* STORE_S\00 */, "i8", ALLOC_STATIC);
STRING_TABLE.__str76=allocate([83,84,79,82,69,95,69,78,84,0] /* STORE_ENT\00 */, "i8", ALLOC_STATIC);
STRING_TABLE.__str77=allocate([83,84,79,82,69,95,70,76,68,0] /* STORE_FLD\00 */, "i8", ALLOC_STATIC);
STRING_TABLE.__str78=allocate([83,84,79,82,69,95,70,78,67,0] /* STORE_FNC\00 */, "i8", ALLOC_STATIC);
STRING_TABLE.__str79=allocate([83,84,79,82,69,80,95,70,0] /* STOREP_F\00 */, "i8", ALLOC_STATIC);
STRING_TABLE.__str80=allocate([83,84,79,82,69,80,95,86,0] /* STOREP_V\00 */, "i8", ALLOC_STATIC);
STRING_TABLE.__str81=allocate([83,84,79,82,69,80,95,83,0] /* STOREP_S\00 */, "i8", ALLOC_STATIC);
STRING_TABLE.__str82=allocate([83,84,79,82,69,80,95,69,78,84,0] /* STOREP_ENT\00 */, "i8", ALLOC_STATIC);
STRING_TABLE.__str83=allocate([83,84,79,82,69,80,95,70,76,68,0] /* STOREP_FLD\00 */, "i8", ALLOC_STATIC);
STRING_TABLE.__str84=allocate([83,84,79,82,69,80,95,70,78,67,0] /* STOREP_FNC\00 */, "i8", ALLOC_STATIC);
STRING_TABLE.__str85=allocate([82,69,84,85,82,78,0] /* RETURN\00 */, "i8", ALLOC_STATIC);
STRING_TABLE.__str86=allocate([78,79,84,95,70,0] /* NOT_F\00 */, "i8", ALLOC_STATIC);
STRING_TABLE.__str87=allocate([78,79,84,95,86,0] /* NOT_V\00 */, "i8", ALLOC_STATIC);
STRING_TABLE.__str88=allocate([78,79,84,95,83,0] /* NOT_S\00 */, "i8", ALLOC_STATIC);
STRING_TABLE.__str89=allocate([78,79,84,95,69,78,84,0] /* NOT_ENT\00 */, "i8", ALLOC_STATIC);
STRING_TABLE.__str90=allocate([78,79,84,95,70,78,67,0] /* NOT_FNC\00 */, "i8", ALLOC_STATIC);
STRING_TABLE.__str91=allocate([73,70,0] /* IF\00 */, "i8", ALLOC_STATIC);
STRING_TABLE.__str92=allocate([73,70,78,79,84,0] /* IFNOT\00 */, "i8", ALLOC_STATIC);
STRING_TABLE.__str93=allocate([67,65,76,76,48,0] /* CALL0\00 */, "i8", ALLOC_STATIC);
STRING_TABLE.__str94=allocate([67,65,76,76,49,0] /* CALL1\00 */, "i8", ALLOC_STATIC);
STRING_TABLE.__str95=allocate([67,65,76,76,50,0] /* CALL2\00 */, "i8", ALLOC_STATIC);
STRING_TABLE.__str96=allocate([67,65,76,76,51,0] /* CALL3\00 */, "i8", ALLOC_STATIC);
STRING_TABLE.__str97=allocate([67,65,76,76,52,0] /* CALL4\00 */, "i8", ALLOC_STATIC);
STRING_TABLE.__str98=allocate([67,65,76,76,53,0] /* CALL5\00 */, "i8", ALLOC_STATIC);
STRING_TABLE.__str99=allocate([67,65,76,76,54,0] /* CALL6\00 */, "i8", ALLOC_STATIC);
STRING_TABLE.__str100=allocate([67,65,76,76,55,0] /* CALL7\00 */, "i8", ALLOC_STATIC);
STRING_TABLE.__str101=allocate([67,65,76,76,56,0] /* CALL8\00 */, "i8", ALLOC_STATIC);
STRING_TABLE.__str102=allocate([83,84,65,84,69,0] /* STATE\00 */, "i8", ALLOC_STATIC);
STRING_TABLE.__str103=allocate([71,79,84,79,0] /* GOTO\00 */, "i8", ALLOC_STATIC);
STRING_TABLE.__str104=allocate([65,78,68,0] /* AND\00 */, "i8", ALLOC_STATIC);
STRING_TABLE.__str105=allocate([79,82,0] /* OR\00 */, "i8", ALLOC_STATIC);
STRING_TABLE.__str106=allocate([66,73,84,65,78,68,0] /* BITAND\00 */, "i8", ALLOC_STATIC);
STRING_TABLE.__str107=allocate([66,73,84,79,82,0] /* BITOR\00 */, "i8", ALLOC_STATIC);
STRING_TABLE.__str108=allocate([69,78,68,0] /* END\00 */, "i8", ALLOC_STATIC);
STRING_TABLE.__str109=allocate([111,117,116,32,111,102,32,109,101,109,111,114,121,10,0] /* out of memory\0A\00 */, "i8", ALLOC_STATIC);
STRING_TABLE.__str110=allocate([58,32,37,115,10,0] /* : %s\0A\00 */, "i8", ALLOC_STATIC);
HEAP32[((_asm_instr)>>2)]=((STRING_TABLE.__str42)|0);
HEAP32[(((_asm_instr)+(12))>>2)]=((STRING_TABLE.__str43)|0);
HEAP32[(((_asm_instr)+(24))>>2)]=((STRING_TABLE.__str44)|0);
HEAP32[(((_asm_instr)+(36))>>2)]=((STRING_TABLE.__str45)|0);
HEAP32[(((_asm_instr)+(48))>>2)]=((STRING_TABLE.__str46)|0);
HEAP32[(((_asm_instr)+(60))>>2)]=((STRING_TABLE.__str47)|0);
HEAP32[(((_asm_instr)+(72))>>2)]=((STRING_TABLE.__str48)|0);
HEAP32[(((_asm_instr)+(84))>>2)]=((STRING_TABLE.__str49)|0);
HEAP32[(((_asm_instr)+(96))>>2)]=((STRING_TABLE.__str50)|0);
HEAP32[(((_asm_instr)+(108))>>2)]=((STRING_TABLE.__str51)|0);
HEAP32[(((_asm_instr)+(120))>>2)]=((STRING_TABLE.__str52)|0);
HEAP32[(((_asm_instr)+(132))>>2)]=((STRING_TABLE.__str53)|0);
HEAP32[(((_asm_instr)+(144))>>2)]=((STRING_TABLE.__str54)|0);
HEAP32[(((_asm_instr)+(156))>>2)]=((STRING_TABLE.__str55)|0);
HEAP32[(((_asm_instr)+(168))>>2)]=((STRING_TABLE.__str56)|0);
HEAP32[(((_asm_instr)+(180))>>2)]=((STRING_TABLE.__str57)|0);
HEAP32[(((_asm_instr)+(192))>>2)]=((STRING_TABLE.__str58)|0);
HEAP32[(((_asm_instr)+(204))>>2)]=((STRING_TABLE.__str59)|0);
HEAP32[(((_asm_instr)+(216))>>2)]=((STRING_TABLE.__str60)|0);
HEAP32[(((_asm_instr)+(228))>>2)]=((STRING_TABLE.__str61)|0);
HEAP32[(((_asm_instr)+(240))>>2)]=((STRING_TABLE.__str62)|0);
HEAP32[(((_asm_instr)+(252))>>2)]=((STRING_TABLE.__str63)|0);
HEAP32[(((_asm_instr)+(264))>>2)]=((STRING_TABLE.__str64)|0);
HEAP32[(((_asm_instr)+(276))>>2)]=((STRING_TABLE.__str65)|0);
HEAP32[(((_asm_instr)+(288))>>2)]=((STRING_TABLE.__str66)|0);
HEAP32[(((_asm_instr)+(300))>>2)]=((STRING_TABLE.__str67)|0);
HEAP32[(((_asm_instr)+(312))>>2)]=((STRING_TABLE.__str68)|0);
HEAP32[(((_asm_instr)+(324))>>2)]=((STRING_TABLE.__str69)|0);
HEAP32[(((_asm_instr)+(336))>>2)]=((STRING_TABLE.__str70)|0);
HEAP32[(((_asm_instr)+(348))>>2)]=((STRING_TABLE.__str71)|0);
HEAP32[(((_asm_instr)+(360))>>2)]=((STRING_TABLE.__str72)|0);
HEAP32[(((_asm_instr)+(372))>>2)]=((STRING_TABLE.__str73)|0);
HEAP32[(((_asm_instr)+(384))>>2)]=((STRING_TABLE.__str74)|0);
HEAP32[(((_asm_instr)+(396))>>2)]=((STRING_TABLE.__str75)|0);
HEAP32[(((_asm_instr)+(408))>>2)]=((STRING_TABLE.__str76)|0);
HEAP32[(((_asm_instr)+(420))>>2)]=((STRING_TABLE.__str77)|0);
HEAP32[(((_asm_instr)+(432))>>2)]=((STRING_TABLE.__str78)|0);
HEAP32[(((_asm_instr)+(444))>>2)]=((STRING_TABLE.__str79)|0);
HEAP32[(((_asm_instr)+(456))>>2)]=((STRING_TABLE.__str80)|0);
HEAP32[(((_asm_instr)+(468))>>2)]=((STRING_TABLE.__str81)|0);
HEAP32[(((_asm_instr)+(480))>>2)]=((STRING_TABLE.__str82)|0);
HEAP32[(((_asm_instr)+(492))>>2)]=((STRING_TABLE.__str83)|0);
HEAP32[(((_asm_instr)+(504))>>2)]=((STRING_TABLE.__str84)|0);
HEAP32[(((_asm_instr)+(516))>>2)]=((STRING_TABLE.__str85)|0);
HEAP32[(((_asm_instr)+(528))>>2)]=((STRING_TABLE.__str86)|0);
HEAP32[(((_asm_instr)+(540))>>2)]=((STRING_TABLE.__str87)|0);
HEAP32[(((_asm_instr)+(552))>>2)]=((STRING_TABLE.__str88)|0);
HEAP32[(((_asm_instr)+(564))>>2)]=((STRING_TABLE.__str89)|0);
HEAP32[(((_asm_instr)+(576))>>2)]=((STRING_TABLE.__str90)|0);
HEAP32[(((_asm_instr)+(588))>>2)]=((STRING_TABLE.__str91)|0);
HEAP32[(((_asm_instr)+(600))>>2)]=((STRING_TABLE.__str92)|0);
HEAP32[(((_asm_instr)+(612))>>2)]=((STRING_TABLE.__str93)|0);
HEAP32[(((_asm_instr)+(624))>>2)]=((STRING_TABLE.__str94)|0);
HEAP32[(((_asm_instr)+(636))>>2)]=((STRING_TABLE.__str95)|0);
HEAP32[(((_asm_instr)+(648))>>2)]=((STRING_TABLE.__str96)|0);
HEAP32[(((_asm_instr)+(660))>>2)]=((STRING_TABLE.__str97)|0);
HEAP32[(((_asm_instr)+(672))>>2)]=((STRING_TABLE.__str98)|0);
HEAP32[(((_asm_instr)+(684))>>2)]=((STRING_TABLE.__str99)|0);
HEAP32[(((_asm_instr)+(696))>>2)]=((STRING_TABLE.__str100)|0);
HEAP32[(((_asm_instr)+(708))>>2)]=((STRING_TABLE.__str101)|0);
HEAP32[(((_asm_instr)+(720))>>2)]=((STRING_TABLE.__str102)|0);
HEAP32[(((_asm_instr)+(732))>>2)]=((STRING_TABLE.__str103)|0);
HEAP32[(((_asm_instr)+(744))>>2)]=((STRING_TABLE.__str104)|0);
HEAP32[(((_asm_instr)+(756))>>2)]=((STRING_TABLE.__str105)|0);
HEAP32[(((_asm_instr)+(768))>>2)]=((STRING_TABLE.__str106)|0);
HEAP32[(((_asm_instr)+(780))>>2)]=((STRING_TABLE.__str107)|0);
HEAP32[(((_asm_instr)+(792))>>2)]=((STRING_TABLE.__str108)|0);
FUNCTION_TABLE = [0,0]; Module["FUNCTION_TABLE"] = FUNCTION_TABLE;


function run(args) {
  args = args || Module['arguments'];

  if (runDependencies > 0) {
    Module.printErr('run() called, but dependencies remain, so not running');
    return 0;
  }

  if (Module['preRun']) {
    if (typeof Module['preRun'] == 'function') Module['preRun'] = [Module['preRun']];
    var toRun = Module['preRun'];
    Module['preRun'] = [];
    for (var i = toRun.length-1; i >= 0; i--) {
      toRun[i]();
    }
    if (runDependencies > 0) {
      // a preRun added a dependency, run will be called later
      return 0;
    }
  }

  function doRun() {
    var ret = 0;
    calledRun = true;
    if (Module['_main']) {
      preMain();
      ret = Module.callMain(args);
      if (!Module['noExitRuntime']) {
        exitRuntime();
      }
    }
    if (Module['postRun']) {
      if (typeof Module['postRun'] == 'function') Module['postRun'] = [Module['postRun']];
      while (Module['postRun'].length > 0) {
        Module['postRun'].pop()();
      }
    }
    return ret;
  }

  if (Module['setStatus']) {
    Module['setStatus']('Running...');
    setTimeout(function() {
      setTimeout(function() {
        Module['setStatus']('');
      }, 1);
      doRun();
    }, 1);
    return 0;
  } else {
    return doRun();
  }
}
Module['run'] = run;

// {{PRE_RUN_ADDITIONS}}

if (Module['preInit']) {
  if (typeof Module['preInit'] == 'function') Module['preInit'] = [Module['preInit']];
  while (Module['preInit'].length > 0) {
    Module['preInit'].pop()();
  }
}

initRuntime();

var shouldRunNow = true;
if (Module['noInitialRun']) {
  shouldRunNow = false;
}

if (shouldRunNow) {
  var ret = run();
}

// {{POST_RUN_ADDITIONS}}






  // {{MODULE_ADDITIONS}}


// EMSCRIPTEN_GENERATED_FUNCTIONS: ["_qc_program_entitydata_add","_qc_program_stack_remove","_qc_program_strings_add","_qc_program_fields_remove","_qc_program_profile_remove","_qc_program_globals_add","_qc_program_functions_add","_prog_getstring","_loaderror","_qc_program_localstack_remove","_qc_program_localstack_resize","_trace_print_global","_qc_program_localstack_add","_qc_program_globals_remove","_qc_program_strings_resize","_prog_free_entity","_qcvmerror","_qc_program_entitydata_remove","_qc_program_functions_remove","_prog_tempstring","_qc_program_fields_add","_qc_program_builtins_add","_qc_program_builtins_remove","_prog_load","_prog_spawn_entity","_prog_getdef","_prog_entfield","_prog_delete","_qc_program_defs_add","_qc_program_strings_remove","_prog_exec","_qc_program_entitypool_remove","_qc_program_code_remove","_qc_program_defs_remove","_qc_program_entitypool_add","_qc_program_stack_add","_prog_print_statement","_qc_program_strings_append","_prog_getedict","_print_escaped_string","_qc_program_localstack_append","_qc_program_profile_resize","_prog_enterfunction","_qc_program_profile_add","_prog_leavefunction","_qc_program_code_add"]

