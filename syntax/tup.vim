" Vim syntax file
" Language:     Tupfile

" For version 5.x: Clear all syntax items
" For version 6.x: Quit when a syntax file was already loaded
if version < 600
  syntax clear
elseif exists("b:current_syntax")
  finish
endif

syntax case match

syntax match comment ,^#.*$,
syntax match rule ,^[:,],
syntax match separator /|>/
syntax match format display "%\([FfoBb]\)" contained
syntax match variable /[$@]([^)]*)/
syntax match variable /{[^}]*}/
syntax match control "^\(ifeq\>\|else\>\|endif\>\|include\>\|include_rules\>\)"
syntax keyword keys foreach
syntax region varsed matchgroup=rule start=/,/ end=/$/ contains=separator,format,variable
syntax region command matchgroup=separator start=/|>/ end=/|>/ end=/$/ contains=format,variable

highlight link comment Comment
highlight link command String
highlight link rule Operator
highlight link separator Keyword
highlight link keys Keyword
highlight link format Special
highlight link variable Special
highlight link control Include
