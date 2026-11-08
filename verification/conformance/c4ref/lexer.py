"""Reference lexer for the C4 subset.

Derived from verification/spec/lexer.md (Assignment 1, Task B).
This module is intentionally independent of the C++ implementation.
"""

from dataclasses import dataclass
from typing import List, Optional


KEYWORDS = {
    "auto", "break", "case", "char", "const", "continue", "default", "do",
    "double", "else", "enum", "extern", "float", "for", "goto", "if",
    "inline", "int", "long", "register", "restrict", "return", "short",
    "signed", "sizeof", "static", "struct", "switch", "typedef", "union",
    "unsigned", "void", "volatile", "while",
    "_Alignas", "_Alignof", "_Atomic", "_Bool", "_Complex", "_Generic",
    "_Imaginary", "_Noreturn", "_Static_assert", "_Thread_local",
}

PUNCTUATORS = [
    "%:%:", "...", "<<=", ">>=",
    "->", "++", "--", "<<", ">>", "<=", ">=", "==", "!=", "&&", "||",
    "*=", "/=", "%=", "+=", "-=", "&=", "^=", "|=", "<:", ":>", "<%",
    "%>", "%:", "##",
    "[", "]", "(", ")", "{", "}", ".", "&", "*", "+", "-", "~", "!",
    "/", "%", "<", ">", "^", "|", "?", ":", ";", "=", ",", "#",
]
# Longest first, so the first matching punctuator is the maximal munch.
PUNCTUATORS.sort(key=len, reverse=True)

SIMPLE_ESCAPES = set("'\"?\\abfnrtv")
WHITESPACE = set(" \t\v\f")
NEWLINES = set("\r\n")


@dataclass(frozen=True)
class Token:
    kind: str          # keyword, identifier, constant, string-literal, punctuator, eof
    text: str
    line: int
    column: int
    offset: int


class LexError(Exception):
    def __init__(self, line: int, column: int, message: str):
        super().__init__(message)
        self.line = line
        self.column = column
        self.message = message


class Cursor:
    def __init__(self, text: str):
        self.text = text
        self.i = 0
        self.line = 1
        self.column = 1

    def eof(self) -> bool:
        return self.i >= len(self.text)

    def peek(self, k: int = 0) -> str:
        j = self.i + k
        return self.text[j] if j < len(self.text) else ""

    def advance(self) -> str:
        ch = self.text[self.i]
        if ch == "\r":
            self.i += 1
            if self.i < len(self.text) and self.text[self.i] == "\n":
                self.i += 1
            self.line += 1
            self.column = 1
        elif ch == "\n":
            self.i += 1
            self.line += 1
            self.column = 1
        else:
            self.i += 1
            self.column += 1
        return ch


def is_ident_start(ch: str) -> bool:
    return ch == "_" or ("a" <= ch <= "z") or ("A" <= ch <= "Z")


def is_ident_char(ch: str) -> bool:
    return is_ident_start(ch) or ("0" <= ch <= "9")


def lex(source: str) -> List[Token]:
    """Lex source (a str whose code points are ISO-8859-1 bytes)."""
    cur = Cursor(source)
    tokens: List[Token] = []

    def error(message: str, line: Optional[int] = None, column: Optional[int] = None):
        raise LexError(cur.line if line is None else line,
                       cur.column if column is None else column, message)

    def scan_escape():
        # cur is at the backslash.
        bs_line, bs_col = cur.line, cur.column
        cur.advance()  # backslash
        if cur.eof():
            error("invalid escape sequence", bs_line, bs_col)
        ch = cur.peek()
        if ch not in SIMPLE_ESCAPES:
            error("invalid escape sequence", bs_line, bs_col)
        cur.advance()

    while not cur.eof():
        ch = cur.peek()

        if ch in WHITESPACE:
            cur.advance()
            continue
        if ch in NEWLINES:
            cur.advance()
            continue

        # Comments.
        if ch == "/" and cur.peek(1) == "*":
            start_line, start_col = cur.line, cur.column
            cur.advance()
            cur.advance()
            while True:
                if cur.eof():
                    error("unterminated comment", start_line, start_col)
                if cur.peek() == "*" and cur.peek(1) == "/":
                    cur.advance()
                    cur.advance()
                    break
                cur.advance()
            continue
        if ch == "/" and cur.peek(1) == "/":
            cur.advance()
            cur.advance()
            while not cur.eof() and cur.peek() not in NEWLINES:
                cur.advance()
            continue

        start_i, start_line, start_col = cur.i, cur.line, cur.column

        # Identifier or keyword.
        if is_ident_start(ch):
            while not cur.eof() and is_ident_char(cur.peek()):
                cur.advance()
            text = source[start_i:cur.i]
            tokens.append(Token("keyword" if text in KEYWORDS else "identifier",
                                text, start_line, start_col, start_i))
            continue

        # Integer or character constant.
        if "0" <= ch <= "9":
            if ch == "0":
                cur.advance()
                if cur.peek() and ("0" <= cur.peek() <= "9"):
                    error("octal constants are not supported")
                if cur.peek() in ("x", "X", ".") or is_ident_start(cur.peek()):
                    error("invalid suffix on integer constant")
            else:
                while not cur.eof() and "0" <= cur.peek() <= "9":
                    cur.advance()
                if cur.peek() == "." or is_ident_start(cur.peek()):
                    error("invalid suffix on integer constant")
            tokens.append(Token("constant", source[start_i:cur.i],
                                start_line, start_col, start_i))
            continue

        if ch == "'":
            cur.advance()  # opening quote
            if cur.eof() or cur.peek() in NEWLINES:
                error("missing terminating ' character", start_line, start_col)
            if cur.peek() == "'":
                error("empty character constant")
            if cur.peek() == "\\":
                scan_escape()
            else:
                cur.advance()
            if cur.eof() or cur.peek() in NEWLINES:
                error("missing terminating ' character", start_line, start_col)
            if cur.peek() != "'":
                error("multi-character character constant")
            cur.advance()  # closing quote
            tokens.append(Token("constant", source[start_i:cur.i],
                                start_line, start_col, start_i))
            continue

        if ch == '"':
            cur.advance()  # opening quote
            while True:
                if cur.eof() or cur.peek() in NEWLINES:
                    error('missing terminating " character', start_line, start_col)
                if cur.peek() == '"':
                    cur.advance()
                    break
                if cur.peek() == "\\":
                    scan_escape()
                else:
                    cur.advance()
            tokens.append(Token("string-literal", source[start_i:cur.i],
                                start_line, start_col, start_i))
            continue

        # Punctuator, maximal munch.
        for punct in PUNCTUATORS:
            if source.startswith(punct, cur.i):
                for _ in punct:
                    cur.advance()
                tokens.append(Token("punctuator", punct,
                                    start_line, start_col, start_i))
                break
        else:
            error("invalid character %r" % ch)

    tokens.append(Token("eof", "", cur.line, cur.column, cur.i))
    return tokens


def token_lines(tokens: List[Token], filename: str) -> List[str]:
    out = []
    for tok in tokens:
        if tok.kind == "eof":
            continue
        out.append("%s:%d:%d: %s %s" % (filename, tok.line, tok.column,
                                        tok.kind, tok.text))
    return out
