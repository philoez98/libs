/*  A simple C-like language tokenizer.

    This a STB-style library, so do this:
    
        #define TOKENIZER_IMPLEMENTATION
    in *one* C++ source file before including this header to create the implementation.
    Something like:

        #include ...
        #include ...
        #define TOKENIZER_IMPLEMENTATION
        #include "tokenizer.h"
     
    Additionally you can define, before including this header:
        #define TOKENIZER_STATIC
    to have all functions declared as static.

        
    Example usage:
    
    #define TOKENIZER_IMPLEMENTATION
    #include "tokenizer.h"

    int main() {

       char* Text = ... // Routine that loads a file (or any other source) as a string.

       tokenizer Tokenizer;
       InitTokenizer(&Tokenizer, Text, 0);

       while(Parsing(&Tokenizer)) {
           token Token = GetToken(&Tokenizer);

           if (Token.Type == TOKEN_EOS) {
               break;
           }
           else {
               // Do something with the token
               ...
           }

       }

       return 0;
    }

 */


#include <stdlib.h> // For strtoll and strtof

#ifdef TOKENIZER_LOG_PARSE_ERRORS
#include <stdio.h>
#endif

#ifndef TOKENIZER_DEF
#ifdef TOKENIZER_STATIC
#define TOKENIZER_DEF static
#else
#define TOKENIZER_DEF extern
#endif
#endif

enum token_type {
    TOKEN_UNKNOWN,
    TOKEN_IDENT,
    TOKEN_OPEN_PAREN,           // (
    TOKEN_CLOSE_PAREN,          // )
    TOKEN_COLON,                // :
    TOKEN_COLON_COLON,          // ::
    TOKEN_STRING,
    TOKEN_INTEGER,
    TOKEN_FLOAT,
    TOKEN_SEMICOLON,            // ;
    TOKEN_COMMA,                // ,
    TOKEN_ASTERISK,             // *
    TOKEN_MUL_EQUAL,            // *=
    TOKEN_HASHTAG,              // #
    TOKEN_AND,                  // &
    TOKEN_AND_AND,              // &&
    TOKEN_AND_EQUAL,            // &=
    TOKEN_OR,                   // |
    TOKEN_OR_OR,                // ||
    TOKEN_OR_EQUAL,             // |=
    TOKEN_XOR,                  // ^
    TOKEN_XOR_EQUAL,            // ^=
    TOKEN_OPEN_BRACKET,         // [
    TOKEN_CLOSE_BRACKET,        // ]
    TOKEN_OPEN_BRACE,           // {
    TOKEN_CLOSE_BRACE,          // }
    TOKEN_OPEN_ANG_BRACKET,     // <
    TOKEN_CLOSE_ANG_BRACKET,    // >
    TOKEN_RIGHT_SHIFT,          // >>
    TOKEN_RIGHT_SHIFT_EQUAL,    // >>=
    TOKEN_LEFT_SHIFT,           // <<
    TOKEN_LEFT_SHIFT_EQUAL,     // <<=
    TOKEN_GREATER_EQUAL,        // >=
    TOKEN_LESS_EQUAL,           // <=
    TOKEN_PLUS,                 // +
    TOKEN_MINUS,                // -
    TOKEN_EQUAL,                // =
    TOKEN_EQUAL_EQUAL,          // ==
    TOKEN_PLUS_PLUS,            // ++
    TOKEN_PLUS_EQUAL,           // +=
    TOKEN_MINUS_MINUS,          // --
    TOKEN_MINUS_EQUAL,          // -=
    TOKEN_ARROW,                // ->
    TOKEN_DOLLAR_SIGN,          // $
    TOKEN_FORWARD_SLASH,        // /
    TOKEN_BACKSLASH,            // '\'
    TOKEN_DIV_EQUAL,            // /=
    TOKEN_MOD,                  // %
    TOKEN_MOD_EQUAL,            // %=
    TOKEN_NOT,                  // !
    TOKEN_NOT_EQUAL,            // !=
    TOKEN_LOGIC_NOT,            // ~
    TOKEN_LOGIC_NOT_EQUAL,      // ~=
    TOKEN_EOS,                  // \0
    TOKEN_TYPE_COUNT
};


struct token {
    token_type Type = TOKEN_UNKNOWN;

    int Length = 0;
    char *Text = NULL;

    union {
        double Float;
        long long int Int;
    };
};

struct tokenizer {

    char *At = 0;
    const char* File = 0;

    int Line = 1;

    bool Error = false;
    bool CountLines = true;
};

// 'Data' and 'Filename' must remain valid while the tokenizer is in use.
TOKENIZER_DEF void InitTokenizer(tokenizer* Tokenizer, char* Data, const char* Filename);

TOKENIZER_DEF token GetToken(tokenizer* Tokenizer);
TOKENIZER_DEF token PeekToken(tokenizer* Tokenizer);

// Get a token of given type optionally, i.e without erroring if not found.
TOKENIZER_DEF bool OptionalToken(tokenizer* Tokenizer, token_type Type, token* Optional);

TOKENIZER_DEF bool RequireToken(tokenizer* Tokenizer, token_type Type, token* Required);

TOKENIZER_DEF void SetError(tokenizer *Tokenizer, const char* Message);
TOKENIZER_DEF bool Parsing(tokenizer *Tokenizer);


#ifdef TOKENIZER_IMPLEMENTATION

#ifdef TOKENIZER_LOG_PARSE_ERRORS
static const char* TokenTypes[TOKEN_TYPE_COUNT]{
    "TOKEN_UNKNOWN",
    "TOKEN_IDENT",
    "TOKEN_OPEN_PAREN",           // (
    "TOKEN_CLOSE_PAREN",          // )
    "TOKEN_COLON",                // :
    "TOKEN_COLON_COLON",          // ::
    "TOKEN_STRING",
    "TOKEN_INTEGER",
    "TOKEN_FLOAT",
    "TOKEN_SEMICOLON",            // ;
    "TOKEN_COMMA",                // ,
    "TOKEN_ASTERISK",             // *
    "TOKEN_MUL_EQUAL",            // *=
    "TOKEN_HASHTAG",              // #
    "TOKEN_AND",                  // &
    "TOKEN_AND_AND",              // &&
    "TOKEN_AND_EQUAL",            // &=
    "TOKEN_OR",                   // |
    "TOKEN_OR_OR",                // ||
    "TOKEN_OR_EQUAL",             // |=
    "TOKEN_XOR",                  // ^
    "TOKEN_XOR_EQUAL",            // ^=
    "TOKEN_OPEN_BRACKET",         // [
    "TOKEN_CLOSE_BRACKET",        // ]
    "TOKEN_OPEN_BRACE",           // {
    "TOKEN_CLOSE_BRACE",          // }
    "TOKEN_OPEN_ANG_BRACKET",     // <
    "TOKEN_CLOSE_ANG_BRACKET",    // >
    "TOKEN_RIGHT_SHIFT",          // >>
    "TOKEN_RIGHT_SHIFT_EQUAL",    // >>=
    "TOKEN_LEFT_SHIFT",           // <<
    "TOKEN_LEFT_SHIFT_EQUAL",     // <<=
    "TOKEN_GREATER_EQUAL",        // >=
    "TOKEN_LESS_EQUAL",           // <=
    "TOKEN_PLUS",                 // +
    "TOKEN_MINUS",                // -
    "TOKEN_EQUAL",                // =
    "TOKEN_EQUAL_EQUAL",          // ==
    "TOKEN_PLUS_PLUS",            // ++
    "TOKEN_PLUS_EQUAL",           // +=
    "TOKEN_MINUS_MINUS",          // --
    "TOKEN_MINUS_EQUAL",          // -=
    "TOKEN_ARROW",                // ->
    "TOKEN_DOLLAR_SIGN",          // $
    "TOKEN_FORWARD_SLASH",        // /
    "TOKEN_BACKSLASH",            // '\'
    "TOKEN_DIV_EQUAL",            // /=
    "TOKEN_MOD",                  // %
    "TOKEN_MOD_EQUAL",            // %=
    "TOKEN_NOT",                  // !
    "TOKEN_NOT_EQUAL",            // !=
    "TOKEN_LOGIC_NOT",            // ~
    "TOKEN_LOGIC_NOT_EQUAL",      // ~=
    "TOKEN_EOS",                  // \0
};
#endif


#define IS_WHITE(c)  ( (c) == ' ' || (c) == '\t' || (c) == '\r' || (c) == '\n' || (c) == '\f' )
#define IS_LETTER(c) ( ((c) >= 'a' && (c) <= 'z') || ((c) >= 'A' && (c) <= 'Z') )
#define IS_DIGIT(c)  ( (c) >= '0' && (c) <= '9' )
#define IS_SUFFIX(c) ( (c) == 'e' || (c) == 'E' || (c) == 'f' || (c) == 'F' || \
    (c) == 'x' || (c) == 'X' || (c) == 'u' || (c) == 'l' || (c) == 'U' || \
    (c) == 'L' || (c) == '-' || (c) == '+')

#define IS_HEX(c) ( (c) == 'x' || (c) == 'X' || (c) >= 'a' && (c) <= 'f' || (c) >= 'A' && (c) <= 'F' )



TOKENIZER_DEF void InitTokenizer(tokenizer* Tokenizer, char* Data, const char* Filename) {
    Tokenizer->At = Data;
    Tokenizer->File = Filename;
}


static bool FindChar(char* String, int Length, char c) {

    for (int i = 0; i < Length; ++i) {
        if (String[i] == c) return true;
    }

    return false;
}


TOKENIZER_DEF void SkipLine(tokenizer *Tokenizer) {
    while (*Tokenizer->At && *Tokenizer->At++ != '\n');
    ++Tokenizer->Line;
}

static token NextToken(tokenizer *Tokenizer) {

    char *c = Tokenizer->At;

    // Clean the string
    for (;;) {

        // Remove all whitespaces
        while (*c && IS_WHITE(*c)) {

            // Count lines
            if (Tokenizer->CountLines && *c == '\n') ++Tokenizer->Line;
            ++c;
        }

        // C++ Style Comment
        if (*c && *c == '/' && c[1] == '/') {
            while (*c && *c != '\r' && *c != '\n')
                ++c;
        }

        // C Style Comment
        else if (*c && *c == '/' && c[1] == '*') {
            c += 2;
            while (*c && (*c != '*' || c[1] != '/')) {
                if (Tokenizer->CountLines && *c == '\n') ++Tokenizer->Line;
                ++c;
            }

            if (*c == '*') {
                c += 2;
            }
        }
        else break;
    }


    token Token;
    Token.Type = TOKEN_UNKNOWN;
    Token.Length = 1;
    Token.Text = c;

    switch(*c) {

        case '\0': { Token.Type = TOKEN_EOS; }           break;
        case '(':  { Token.Type = TOKEN_OPEN_PAREN; }    break;
        case ')':  { Token.Type = TOKEN_CLOSE_PAREN; }   break;
        case ';':  { Token.Type = TOKEN_SEMICOLON; }     break;
        case '[':  { Token.Type = TOKEN_OPEN_BRACKET; }  break;
        case ']':  { Token.Type = TOKEN_CLOSE_BRACKET; } break;
        case '{':  { Token.Type = TOKEN_OPEN_BRACE; }    break;
        case '}':  { Token.Type = TOKEN_CLOSE_BRACE; }   break;
        case ',':  { Token.Type = TOKEN_COMMA; }         break;
        case '$':  { Token.Type = TOKEN_DOLLAR_SIGN; }   break;
        case '#':  { Token.Type = TOKEN_HASHTAG; }       break;
        case '\\': { Token.Type = TOKEN_BACKSLASH; }     break;

        case ':':
        {
            ++Token.Length;
            if (c[1] && c[1] == ':') Token.Type = TOKEN_COLON_COLON;
            else { Token.Type = TOKEN_COLON; Token.Length = 1; }

        } break;
        case '=':
        {
            ++Token.Length;
            if (c[1] && c[1] == '=') Token.Type = TOKEN_EQUAL_EQUAL;
            else { Token.Type = TOKEN_EQUAL; Token.Length = 1; }

        } break;
        case '>':
        {
            ++Token.Length;
            if (c[1] && c[1] == '=') Token.Type = TOKEN_GREATER_EQUAL;
            else if (c[1] && c[1] == '>') {
                Token.Type = TOKEN_RIGHT_SHIFT;

                if (c[2] && c[2] == '=') {
                    Token.Type = TOKEN_RIGHT_SHIFT_EQUAL;
                    Token.Length = 3;
                }
            }
            else { Token.Type = TOKEN_CLOSE_ANG_BRACKET; Token.Length = 1; }

        } break;
        case '<':
        {
            ++Token.Length;
            if (c[1] && c[1] == '=') Token.Type = TOKEN_LESS_EQUAL;
            else if (c[1] && c[1] == '<') {
                Token.Type = TOKEN_LEFT_SHIFT;

                if (c[2] && c[2] == '=') {
                    Token.Type = TOKEN_LEFT_SHIFT_EQUAL;
                    Token.Length = 3;
                }
            }
            else { Token.Type = TOKEN_OPEN_ANG_BRACKET; Token.Length = 1; }

        } break;
        case '/': {

            ++Token.Length;
            if (c[1] && c[1] == '=') Token.Type = TOKEN_DIV_EQUAL;
            else { Token.Type = TOKEN_FORWARD_SLASH; Token.Length = 1; }

        } break;
        case '+':
        {
            ++Token.Length;
            if (c[1] && c[1] == '=') Token.Type = TOKEN_PLUS_EQUAL;
            else if (c[1] && c[1] == '+') Token.Type = TOKEN_PLUS_PLUS;
            else { Token.Type = TOKEN_PLUS; Token.Length = 1; }

        } break;
        case '-':
        {
            ++Token.Length;
            if (c[1] && c[1] == '=') Token.Type = TOKEN_MINUS_EQUAL;
            else if (c[1] && c[1] == '-') Token.Type = TOKEN_MINUS_MINUS;
            else if (c[1] && c[1] == '>') Token.Type = TOKEN_ARROW;
            else { Token.Type = TOKEN_MINUS; Token.Length = 1; }

        } break;
        case '*':
        {
            ++Token.Length;
            if (c[1] && c[1] == '=') Token.Type = TOKEN_MUL_EQUAL;
            else { Token.Type = TOKEN_ASTERISK; Token.Length = 1; }

        } break;
        case '^':
        {
            ++Token.Length;
            if (c[1] && c[1] == '=') Token.Type = TOKEN_XOR_EQUAL;
            else { Token.Type = TOKEN_XOR; Token.Length = 1; }

        } break;
        case '&':
        {
            ++Token.Length;
            if (c[1] && c[1] == '=') Token.Type = TOKEN_AND_EQUAL;
            else if (c[1] && c[1] == '&') Token.Type = TOKEN_AND_AND;
            else { Token.Type = TOKEN_AND; Token.Length = 1; }

        } break;
        case '|':
        {
            ++Token.Length;
            if (c[1] && c[1] == '=') Token.Type = TOKEN_OR_EQUAL;
            else if (c[1] && c[1] == '|') Token.Type = TOKEN_OR_OR;
            else { Token.Type = TOKEN_OR; Token.Length = 1; }

        } break;
        case '~':
        {
            ++Token.Length;
            if (c[1] && c[1] == '=') Token.Type = TOKEN_LOGIC_NOT_EQUAL;
            else { Token.Type = TOKEN_LOGIC_NOT; Token.Length = 1; }

        } break;
        case '%':
        {
            ++Token.Length;
            if (c[1] && c[1] == '=') Token.Type = TOKEN_MOD_EQUAL;
            else { Token.Type = TOKEN_MOD; Token.Length = 1; }

        } break;
        case '!':
        {
            ++Token.Length;
            if (c[1] && c[1] == '=') Token.Type = TOKEN_NOT_EQUAL;
            else { Token.Type = TOKEN_NOT; Token.Length = 1; }

        } break;


        case '"':
        {
            char* Start = c;
            ++c;

            Token.Type = TOKEN_STRING;
            Token.Text = Start;

            while (*c && *c != '"') {
                if (c[1] && c[1] == '\\') {
                    ++c;
                }
                ++c;
            }

            if (*c && *c == '"') {
                ++c;
            }

            Token.Length = c - Start;

        } break;
        case '.':
        {
            // Fallthrough as we could parse either a number or a string
            ++c;
        }

        default:
        {
            if (IS_LETTER(*c) || *c == '_') {

                Token.Type = TOKEN_IDENT;

                while (IS_LETTER(*c) || IS_DIGIT(*c) || *c == '_') {
                    ++c;
                }
                Token.Length = c - Token.Text;

            } else if (IS_DIGIT(*c) || *c == '.') {

                while (IS_DIGIT(*c) || *c == '.' || IS_SUFFIX(*c)) {
                    ++c;
                }

                Token.Length = c - Token.Text;

                if (FindChar(Token.Text, Token.Length, '.') ||
                    FindChar(Token.Text, Token.Length, 'f'))
                {
                    Token.Type = TOKEN_FLOAT;
                    Token.Float = strtod(Token.Text, &c);
                }
                else {

                    bool IsHex = false;

                    for (int i = 0; i < Token.Length; ++i) {
                        if (IS_HEX(Token.Text[i])) {
                            IsHex = true;
                            break;
                        }
                    }

                    Token.Type = TOKEN_INTEGER;
                    Token.Int = strtoll(Token.Text, &c, IsHex ? 16 : 10);
                }

            } else {

                Token.Type = TOKEN_UNKNOWN;
            }
        } break;
    }

    return Token;
}

TOKENIZER_DEF token GetToken(tokenizer* Tokenizer) {
    token Token = NextToken(Tokenizer);
    Tokenizer->At = Token.Text + Token.Length;
    return Token;
}

TOKENIZER_DEF token PeekToken(tokenizer* Tokenizer) {
    Tokenizer->CountLines = false;
    token Next = NextToken(Tokenizer);
    Tokenizer->CountLines = true;
    return Next;
}

TOKENIZER_DEF bool OptionalToken(tokenizer* Tokenizer, token_type Type, token* Optional) {
    token Token = PeekToken(Tokenizer);

    if (Token.Type != Type) {
        return false;
    }
    else {
        GetToken(Tokenizer); // TODO: redundant but we need the line numbers to show correctly
        if (Optional) *Optional = Token;
    }

    return true;
}

TOKENIZER_DEF bool RequireToken(tokenizer* Tokenizer, token_type Type, token* Required) {
    token Token = PeekToken(Tokenizer);

    if (Token.Type != Type) {
        Tokenizer->Error = true;
#ifdef TOKENIZER_LOG_PARSE_ERRORS
        fprintf(stderr, "Token type mismatch at line %d: required token type is %s but current token type is %s.\n", Tokenizer->Line, TokenTypes[Type], TokenTypes[Token.Type]);
#endif
        return false;
    }
    else {
        GetToken(Tokenizer); // TODO: redundant but we need the line numbers to show correctly
        if (Required) *Required = Token;
    }

    return true;
}

TOKENIZER_DEF void SetError(tokenizer *Tokenizer, const char* Message) {
    Tokenizer->Error = true;
#ifdef TOKENIZER_LOG_PARSE_ERRORS
    fprintf(stderr, "Error at line %d: %s.\n", Tokenizer->Line, Message);
#endif
}

TOKENIZER_DEF bool Parsing(tokenizer *Tokenizer) {
    return (*Tokenizer->At && !Tokenizer->Error);
}

#endif
