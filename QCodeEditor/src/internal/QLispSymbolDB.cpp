#include <QLispSymbolDB>

#include <QHash>

static QVector<LispSymbol> buildSymbolDB()
{
    QVector<LispSymbol> db;

    auto add = [&](const char *name,
                   std::initializer_list<const char *> params,
                   const char *doc) {
        LispSymbol s;
        s.name = QString::fromLatin1(name);
        for (const char *p : params) {
            s.params << QString::fromLatin1(p);
        }
        s.doc = QString::fromUtf8(doc);
        db << s;
    };

    add("define", {"name", "value"}, "Bind value to symbol in global scope");
    add("defun", {"name", "(args)", "body"}, "Define a named function");
    add("defmacro", {"name", "(args)", "body"}, "Define a macro");
    add("lambda", {"(args)", "body"}, "Create an anonymous closure");
    add("let", {"((var val) ...)", "body"}, "Bind local variables (parallel)");
    add("let*", {"((var val) ...)", "body"}, "Bind local variables (sequential)");
    add("setq", {"var", "value"}, "Update binding of variable");
    add("setvar", {"'var", "value"}, "Set variable value (quoted symbol)");
    add("set", {"sym", "value"}, "Set symbol value");
    add("progn", {"expr", "..."}, "Evaluate expressions in sequence, return last");
    add("begin", {"expr", "..."}, "Alias for progn");
    add("if", {"cond", "then", "[else]"}, "Conditional: evaluate then or else branch");
    add("cond", {"(test expr)", "..."}, "Multi-branch conditional");
    add("case", {"val", "((match expr) ...)", "def"}, "Pattern match on value with default");
    add("match", {"expr", "(pattern body)", "..."}, "Structural pattern matching");
    add("and", {"expr", "..."}, "Logical AND");
    add("or", {"expr", "..."}, "Logical OR");
    add("not", {"expr"}, "Logical NOT");
    add("quote", {"expr"}, "Return expression without evaluation");
    add("apply", {"f", "args"}, "Apply function f to argument list");
    add("eval", {"expr"}, "Evaluate an expression");
    add("read", {"string"}, "Parse LispBM expression from string");
    add("error", {"type", "msg"}, "Signal an error with type and message");
    add("gc", {}, "Force garbage collection");

    add("loopfor", {"var", "init", "cond", "step", "body"}, "C-style for loop");
    add("loopwhile", {"cond", "body"}, "While loop");
    add("looprange", {"var", "from", "to", "body"}, "Loop over integer range [from, to)");
    add("loopforeach", {"var", "list", "body"}, "Iterate over each element of a list");
    add("loop", {"body"}, "Infinite loop");
    add("break", {"[value]"}, "Break out of loop with optional return value");

    add("car", {"lst"}, "First element of list or cons pair");
    add("cdr", {"lst"}, "Rest of list or cons pair");
    add("cons", {"a", "b"}, "Construct a cons cell (a . b)");
    add("list", {"x", "..."}, "Create a list from arguments");
    add("append", {"lst1", "lst2"}, "Concatenate two lists");
    add("reverse", {"lst"}, "Reverse list");
    add("length", {"lst"}, "Number of elements in list");
    add("nth", {"lst", "n"}, "Get nth element (0-indexed)");
    add("ix", {"lst", "n"}, "Get element at index (0-indexed)");
    add("rest", {"lst"}, "All elements except the first");
    add("last", {"lst"}, "Last element of list");
    add("map", {"f", "lst"}, "Apply f to each element, return result list");
    add("filter", {"f", "lst"}, "Keep elements where (f x) is non-nil");
    add("foldl", {"f", "init", "lst"}, "Left fold");
    add("foldr", {"f", "init", "lst"}, "Right fold");
    add("assoc", {"lst", "key"}, "Find key in association list, return value");
    add("acons", {"key", "val", "lst"}, "Prepend (key . val) to association list");
    add("member", {"x", "lst"}, "Check if x is member of list");
    add("range", {"start", "end"}, "Create integer list [start, end)");
    add("flatten", {"lst"}, "Flatten nested list structure");
    add("sort", {"f", "lst"}, "Sort list using comparator function f");
    add("take", {"n", "lst"}, "Take first n elements");
    add("drop", {"n", "lst"}, "Drop first n elements");
    add("zip", {"lst1", "lst2"}, "Zip two lists into list of pairs");

    add("+", {"a", "b", "..."}, "Addition");
    add("-", {"a", "b", "..."}, "Subtraction");
    add("*", {"a", "b", "..."}, "Multiplication");
    add("/", {"a", "b"}, "Division");
    add("mod", {"a", "b"}, "Modulo");
    add("rem", {"a", "b"}, "Remainder");
    add("abs", {"x"}, "Absolute value");
    add("min", {"a", "b"}, "Minimum");
    add("max", {"a", "b"}, "Maximum");
    add("floor", {"x"}, "Largest integer <= x");
    add("ceil", {"x"}, "Smallest integer >= x");
    add("round", {"x"}, "Round to nearest integer");
    add("truncate", {"x"}, "Truncate toward zero");
    add("sqrt", {"x"}, "Square root");
    add("pow", {"base", "exp"}, "Power: base^exp");
    add("log", {"x"}, "Natural logarithm");
    add("log2", {"x"}, "Base-2 logarithm");
    add("exp", {"x"}, "Exponential: e^x");

    add("sin", {"x"}, "Sine");
    add("cos", {"x"}, "Cosine");
    add("tan", {"x"}, "Tangent");
    add("asin", {"x"}, "Arc sine");
    add("acos", {"x"}, "Arc cosine");
    add("atan", {"x"}, "Arc tangent");
    add("atan2", {"y", "x"}, "Arc tangent of y/x");

    add("shl", {"x", "n"}, "Shift x left by n bits");
    add("shr", {"x", "n"}, "Shift x right by n bits");
    add("bitwise-and", {"a", "b"}, "Bitwise AND");
    add("bitwise-or", {"a", "b"}, "Bitwise OR");
    add("bitwise-xor", {"a", "b"}, "Bitwise XOR");
    add("bitwise-not", {"x"}, "Bitwise NOT");

    add("=", {"a", "b"}, "Numeric equality");
    add("<", {"a", "b"}, "Less than");
    add(">", {"a", "b"}, "Greater than");
    add("<=", {"a", "b"}, "Less than or equal");
    add(">=", {"a", "b"}, "Greater than or equal");
    add("eq", {"a", "b"}, "Identity equality");
    add("neq", {"a", "b"}, "Identity inequality");

    add("type-of", {"x"}, "Return type tag of value");
    add("to-i", {"x"}, "Convert to signed integer (i)");
    add("to-u", {"x"}, "Convert to unsigned integer (u)");
    add("to-float", {"x"}, "Convert to 32-bit float");
    add("to-double", {"x"}, "Convert to 64-bit double");
    add("to-i32", {"x"}, "Convert to signed 32-bit integer");
    add("to-u32", {"x"}, "Convert to unsigned 32-bit integer");
    add("to-i64", {"x"}, "Convert to signed 64-bit integer");
    add("to-u64", {"x"}, "Convert to unsigned 64-bit integer");
    add("to-byte", {"x"}, "Convert to byte");
    add("numberp", {"x"}, "Is x a number?");
    add("symbolp", {"x"}, "Is x a symbol?");
    add("listp", {"x"}, "Is x a list?");
    add("consp", {"x"}, "Is x a cons pair?");
    add("nilp", {"x"}, "Is x nil?");
    add("null", {"x"}, "Is x nil or false?");
    add("boolp", {"x"}, "Is x a boolean?");
    add("stringp", {"x"}, "Is x a string?");
    add("closurep", {"x"}, "Is x a closure/function?");
    add("arrayp", {"x"}, "Is x a byte array?");
    add("bufferp", {"x"}, "Is x a byte buffer?");

    add("print", {"x", "..."}, "Print values without trailing newline");
    add("println", {"x", "..."}, "Print values followed by newline");
    add("str-from-n", {"n", "[fmt]"}, "Convert number to string");
    add("str-to-i", {"s", "[base]"}, "Parse string as integer");
    add("str-to-f", {"s"}, "Parse string as floating-point number");
    add("str-len", {"s"}, "Length of string in bytes");
    add("str-part", {"s", "start", "[len]"}, "Extract substring");
    add("str-find", {"s", "needle", "[start]"}, "Find substring");
    add("str-split", {"s", "delim"}, "Split string by delimiter");
    add("str-merge", {"s1", "s2", "..."}, "Concatenate strings");
    add("str-join", {"lst", "[sep]"}, "Join list elements into string");
    add("str-explode", {"s"}, "Convert string to list of byte values");
    add("str-cmp", {"s1", "s2"}, "Compare strings");
    add("str-contains", {"s", "needle"}, "Check if string contains needle");
    add("format", {"fmt", "args", "..."}, "Format string");

    add("spawn", {"f", "[args]"}, "Start a new concurrent LispBM process");
    add("sleep", {"seconds"}, "Pause current process for N seconds");
    add("yield", {"[value]"}, "Yield control to other processes");
    add("send", {"pid", "msg"}, "Send message to process identified by pid");
    add("receive", {}, "Receive message from mailbox (blocking)");
    add("recv", {"[timeout-ms]"}, "Receive with optional timeout in milliseconds");
    add("self", {}, "Return PID of current process");
    add("task-id", {}, "Return ID of current evaluation context");
    add("atomic", {"body"}, "Execute body without preemption");
    add("wait", {"x"}, "Wait for spawned process result");

    add("img-buffer", {"fmt", "width", "height"}, "Create image buffer");
    add("img-clear", {"img", "[color]"}, "Fill image with color");
    add("img-line", {"img", "x0", "y0", "x1", "y1", "color"}, "Draw line");
    add("img-circle", {"img", "x", "y", "r", "color", "[opts]"}, "Draw circle");
    add("img-arc", {"img", "x", "y", "r", "a1", "a2", "color", "[opts]"}, "Draw arc");
    add("img-rectangle", {"img", "x", "y", "width", "height", "color", "[opts]"}, "Draw rectangle");
    add("img-triangle", {"img", "x0", "y0", "x1", "y1", "x2", "y2", "color", "[opts]"}, "Draw triangle");
    add("img-text", {"img", "x", "y", "scale", "color", "str"}, "Draw text");
    add("img-blit", {"dst", "src", "x", "y", "[opts]"}, "Copy src image onto dst");
    add("img-rotate", {"img", "x", "y", "angle"}, "Rotate image");
    add("img-scale", {"img", "x", "y", "sx", "sy"}, "Scale image");
    add("img-dims", {"img"}, "Return (width height) as list");
    add("disp-render", {"img", "x", "y", "[color-list]"}, "Render image onto display");
    add("disp-clear", {"[color]"}, "Fill display with color");
    add("disp-reset", {}, "Reset display hardware to initial state");
    add("disp-render-jpg", {"img", "x", "y"}, "Render JPEG buffer");

    add("array-new", {"size", "[init]"}, "Create array of given size");
    add("array-read", {"arr", "index"}, "Read element from array at index");
    add("array-write", {"arr", "index", "value"}, "Write value to array at index");
    add("array-length", {"arr"}, "Number of elements in array");
    add("bufcreate", {"size"}, "Create byte buffer of given byte count");
    add("buflen", {"buf"}, "Length of byte buffer");
    add("buf8-read", {"buf", "index"}, "Read 8-bit byte from buffer");
    add("buf8-write", {"buf", "index", "val"}, "Write 8-bit byte to buffer");
    add("buf16-read", {"buf", "index", "[big-endian]"}, "Read 16-bit value from buffer");
    add("buf16-write", {"buf", "index", "val", "[big-endian]"}, "Write 16-bit value to buffer");
    add("buf32-read", {"buf", "index", "[big-endian]"}, "Read 32-bit value from buffer");
    add("buf32-write", {"buf", "index", "val", "[big-endian]"}, "Write 32-bit value to buffer");
    add("bufcopy", {"src", "dst", "[src-off]", "[dst-off]", "[n]"}, "Copy n bytes between buffers");
    add("bufto-string", {"buf"}, "Convert byte buffer to string");
    add("bufclear", {"buf", "[val]"}, "Fill buffer with value");

    add("random", {"[max]"}, "Random number in [0, max)");
    add("srand", {"seed"}, "Set random number generator seed");

    add("time", {}, "Return current time in seconds (float)");
    add("str-num", {"n", "[fmt]"}, "Alias for str-from-n");
    add("num-str", {"s"}, "Alias for str-to-f");

    return db;
}

const QVector<LispSymbol> &QLispSymbolDB::all()
{
    static const QVector<LispSymbol> db = buildSymbolDB();
    return db;
}

const LispSymbol *QLispSymbolDB::find(const QString &name)
{
    static const QHash<QString, const LispSymbol *> index = []() {
        QHash<QString, const LispSymbol *> h;
        for (const LispSymbol &sym : QLispSymbolDB::all()) {
            h[sym.name] = &sym;
        }
        return h;
    }();

    return index.value(name, nullptr);
}
