" Zox syntax highlighting
" Arquivo: ~/.vim/syntax/zox.vim
" Adicione ao ~/.vim/ftdetect/zox.vim:
"   au BufRead,BufNewFile *.zo set filetype=zox

if exists("b:current_syntax")
  finish
endif

" Comentarios
syntax match zoxComment "-#.*$"

" Strings
syntax region zoxString start=/"/ end=/"/ skip=/\\"/ contains=zoxEscape
syntax match  zoxEscape /\\[ntrb f"'\\]/ contained

" Numeros
syntax match zoxNumber "\b\d\+\(\.\d\+\)\?\([eE][+-]\?\d\+\)\?\b"

" Constantes
syntax keyword zoxConstant true false nil

" Declaracoes
syntax keyword zoxDecl let type
syntax match   zoxFunc  "\$"
syntax match   zoxImport "~>"
syntax match   zoxMatch "\?\?"

" Controle de fluxo
syntax match zoxConditional "[?:]"
syntax match zoxLoop        "[#@]"
syntax match zoxFlow        "\~!!"
syntax match zoxFlow        "__>"
syntax match zoxFlow        "_>>"
syntax match zoxFlow        "_>>@"
syntax match zoxFlow        "_>>!"

syntax match zoxMatchArm "=>"
syntax match zoxUnwrap   "!\?"
syntax match zoxPromo    "|\{|}\|"

" Builtins
syntax keyword zoxBuiltin println print len find keys values sum typeof copy random random_int has_key get setdefault ok err is_ok is_err get_ok get_err

" Operadores element-wise
syntax match zoxElemWise "&[+\-*/%|^e]"
syntax match zoxElemWise "&<<"
syntax match zoxElemWise "&>>"

" Pipe
syntax match zoxPipe "|>"
syntax match zoxPipe "<|"

" Operadores logicos
syntax match zoxLogical "&&"
syntax match zoxLogical "||"
syntax match zoxLogical "!"

" Comparacao
syntax match zoxCmp "==\|!=\|<=\|>=\|<\|>"

" Seta dict
syntax match zoxArrow "->"

" Shift / append
syntax match zoxShift "<<\|>>"

" Potencia
syntax match zoxArith "\*\*"

" Aritmeticos
syntax match zoxArith "[+\-*/%]"

" Set
syntax match zoxSet "[|&^]"

" Assign
syntax match zoxAssign "="

" Highlight links
highlight link zoxComment     Comment
highlight link zoxString      String
highlight link zoxEscape      SpecialChar
highlight link zoxNumber      Number
highlight link zoxConstant    Constant
highlight link zoxDecl        Keyword
highlight link zoxFunc        Function
highlight link zoxImport      Include
highlight link zoxConditional Conditional
highlight link zoxLoop        Repeat
highlight link zoxFlow        Statement
highlight link zoxBuiltin     Special
highlight link zoxMatch       Keyword
highlight link zoxMatchArm    Operator
highlight link zoxUnwrap      Operator
highlight link zoxPromo       Operator
highlight link zoxElemWise    Operator
highlight link zoxPipe        Operator
highlight link zoxLogical     Operator
highlight link zoxCmp         Operator
highlight link zoxArrow       Delimiter
highlight link zoxShift       Operator
highlight link zoxArith       Operator
highlight link zoxSet         Operator
highlight link zoxAssign      Operator

let b:current_syntax = "zox"
