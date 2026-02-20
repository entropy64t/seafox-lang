from __future__ import annotations

kwds: dict[str, dict] = {}
kwd_tokens: dict[str, str] = {}
keywords: list[str] = []

def add_keyword(name: str, token: str | None = None): 
    keywords.append(name)
       
    if token is None:
        token = name.upper()
        
    kwd_tokens[name] = "TOKEN_" + token
    
    return 
# unused
    
    print(f"adding keyword: {name} with token: {kwd_tokens[name]}")
    
    inner = kwds
    for char in name: 
        if char not in inner:
            inner[char] = {}
        inner = inner[char]
    inner['end'] = {}
    
    print("debug trie thingy: ")
    print(kwds)
    
    keywords.append(name)
    
T = "    "
def make_kwd(name: str, full_name: str, dct: dict[str, dict], depth: int = 1):
    tab = T * (depth - 1) * 2 + T
    print(tab + f"case '{name}':")
    
    if 'end' in dct:
        print(tab + T + "// check the length")
        print(tab + T + f"if (length == {depth}) return {kwd_tokens[full_name]};")
        dct.pop('end') # unsafe i know but whatever
    else:
        print(tab + T + "// no more length")
        print(tab + T + f"if (length == {depth}) return TOKEN_IDENTIFIER;")
    
    if len(dct) == 0: return
    print("\n" + tab + T + f"switch (scanner.start[{depth}]) {{")
    for begin, inner in dct.items():
        make_kwd(begin, full_name + begin, inner, depth + 1)
    print(tab + T + "}")
    
def compile_keywords_old():
    print("\n\n--- generated code ---")
    
    print("length = scanner.current - scanner.start;")
    print("switch (scanner.start[0]) {")
    for begin, inner in kwds.items():
        make_kwd(begin, begin, inner)
    print("}")
    
def compile_keywords():
    lines = []
    
    # group by len
    dl: dict[int, list[str]] = {}
    for kwd in keywords:
        if len(kwd) not in dl:
            dl[len(kwd)] = []
        dl[len(kwd)].append(kwd)
        
    lines.append(T + "int length = scanner.current - scanner.start;\n")
    lines.append(T + "switch (length) {\n")
    for ln in sorted(dl):
        kwds = dl[ln]
        lines.append(T + T + f"case {ln}:\n")
        for kwd in kwds:
            lines.append(T + T + T + f"if (memcmp(scanner.start, \"{kwd}\", {ln}) == 0) return {kwd_tokens[kwd]};\n")
        lines.append(T + T + T + "break;\n")
        lines.append("\n")
    lines.pop()
    lines.append(T + "}\n")
    lines.append("\n")
    lines.append(T + "return TOKEN_IDENTIFIER;\n")
    full = []
    with open('src/scanner.c', 'r') as scanner:
        code = scanner.readlines()
        line = code.index('// identifier lookup : BEGIN\n')
        end = code.index('// identifier lookup : END\n')
        before = code[:line+1]
        after = code[end:]
        full = before + lines + after
        
    with open('src/scanner.c', 'w') as scanner:
        scanner.write(''.join(full))

def main():
    """Keywords:
    TOKEN_AND, TOKEN_CLASS, TOKEN_ELSE, TOKEN_FALSE,
    TOKEN_FOR, TOKEN_FUNC, TOKEN_IF, TOKEN_NULL, TOKEN_OR,
    TOKEN_PRINT, TOKEN_RETURN, TOKEN_SUPER, TOKEN_THIS,
    TOKEN_TRUE, TOKEN_VAR, TOKEN_WHILE,
    TOKEN_VOID,
    TOKEN_STATIC,
    TOKEN_ELIF,
    TOKEN_BREAK, TOKEN_CONTINUE,
    TOKEN_USING,
    TOKEN_PROPERTY"""
    kl = [
        "and", "class", "else", "false",
        "for", "func", "if", "null", "or",
        "print", "return", "super", "this",
        "true", "var", "while",
        # impl added later
        "void", "static", "elif", "break", "continue", "using", "property"
    ]
    for keyword in kl:
        add_keyword(keyword)
    
    compile_keywords()
    
if __name__ == "__main__":
    main()