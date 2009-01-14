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

syntax match rule /:/
syntax match separator /|>/
syntax match format display "%\([Ffo]\)" contained
syntax keyword keys foreach
syntax region command matchgroup=separator start=/|>/ end=/|>/ contains=format

highlight link command String
highlight link rule Operator
highlight link separator Keyword
highlight link keys Keyword
highlight link format Special
