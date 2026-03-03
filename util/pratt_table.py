prefix_tab = {}
infix_tab = {}
postfix_tab = {}
precedence_tab = {}

last_pref = "NULL"
last_in = "NULL"
last_post = "NULL"
last_prec = "NONE"

def add_token(call_name: str, prefix: str | None = None, infix: str | None = None, postfix: str | None = None, precedence: str | None = None):
    global last_pref, last_in, last_post, last_prec
    prefix = prefix or last_pref
    infix = infix or last_in
    postfix = postfix or last_post
    precedence = precedence or last_prec
    name = "TOKEN_" + call_name.upper()
    prefix_tab[name] = prefix
    infix_tab[name] = infix
    precedence_tab[name] = "PREC_" + precedence.upper()
    postfix_tab[name] = postfix
    last_pref = prefix
    last_in = infix
    last_prec = precedence
    
def add_tokens(*names: str, prefix: str | None = None, infix: str | None = None, postfix: str| None = None, precedence: str | None = None):
    global last_pref, last_in, last_post, last_prec
    prefix = prefix or last_pref
    infix = infix or last_in
    postfix = postfix or last_post
    precedence = precedence or last_prec
    for name in names:
        name = "TOKEN_" + name.upper()
        prefix_tab[name] = prefix
        infix_tab[name] = infix
        postfix_tab[name] = postfix
        precedence_tab[name] = "PREC_" + precedence.upper()
    last_pref = prefix
    last_in = infix
    last_prec = precedence

def main():
    with open('include/scanner.h', 'r') as scanner:
        code = scanner.readlines()
        start = code.index('// token type : BEGIN\n')
        end = code.index('// token type : END\n')
        tokens = []
        for line in code[start:end]:
            if not line.strip().startswith("//"):
                tokens.extend([token.strip() for token in line.split(',') if len(token.strip()) > 0])
    
    print(tokens)
    
    add_token("number", prefix="number")
    add_token("left_paren", prefix="grouping")
    add_token("bang", prefix="unary")
    add_token("minus", infix="binary", precedence="term")
    add_token("plus", prefix="NULL")
    add_tokens("slash", "star", precedence="factor")
    add_tokens("true", "false", "null", prefix="literal", infix="NULL", precedence="none")
    add_tokens("equal_equal", "bang_equal", prefix="NULL", infix="binary", precedence="equality")
    add_tokens("greater", "less", "greater_equal", "less_equal", precedence="comparison")
    add_token("string", prefix="string", infix="NULL", precedence="none")
    add_token("left_brace", prefix="array")
    add_token("left_bracket", postfix="indexAccess", prefix="NULL") # precedence ?
    
    lines = []
    lines.append("ParseRule rules[] = {\n")
    
    longest_name = len(max(tokens, key=len))
    longest_pref = len(max(prefix_tab.values(), key=len)) + 1
    longest_in = len(max(infix_tab.values(), key=len)) + 1
    longest_post = len(max(postfix_tab.values(), key=len)) + 1
    longest_prec = len(max(precedence_tab.values(), key=len))
    
    for token in tokens:
        prefix = prefix_tab.get(token, "NULL")
        infix = infix_tab.get(token, "NULL")
        postfix = postfix_tab.get(token, "NULL")
        prec = precedence_tab.get(token, "PREC_NONE")
        lines.append(f"    {('[' + token + ']'):<{longest_name + 2}} = {{ {(prefix + ','):<{longest_pref}} {(infix + ','):<{longest_in}} {(postfix + ','):<{longest_post}} {prec:<{longest_prec}} }},\n")
    lines.append("};\n")
    
    text = ''.join(lines)
    print(text)
    
    with open('src/compiler.c', 'r') as scanner:
        code = scanner.readlines()
        line = code.index('// pratt table : BEGIN\n')
        end = code.index('// pratt table : END\n')
        before = code[:line+1]
        after = code[end:]
        full = before + lines + after
        
    with open('src/compiler.c', 'w') as scanner:
        scanner.write(''.join(full))
        
if __name__ == "__main__":
    main()