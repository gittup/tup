#! /bin/sh -e

# TODO

. ./tup.sh

cat > Tupfile << HERE
: pre_sexp.ml |> cat %f; touch %o |> pre_sexp.cmi
: foo.ml | pre_sexp.cmi |> cat %f; cat pre_sexp.cmi; touch %o |> foo.cmi
: conv.ml | pre_sexp.cmi foo.cmi |> cat conv.ml; cat pre_sexp.cmi; cat foo.cmi; touch %o |> conv.cmo
HERE
tup touch Tupfile pre_sexp.ml conv.ml foo.ml
update

cat > Tupfile << HERE
: conv.ml |> cat conv.ml; cat foo.cmi; cat pre_sexp.cmi; touch %o |> conv.cmo
: pre_sexp.ml | conv.cmo |> cat %f; touch %o |> pre_sexp.cmi
: foo.ml | pre_sexp.cmi |> cat %f; cat pre_sexp.cmi; touch %o |> foo.cmi
HERE
tup touch Tupfile
update_fail_msg 'tup error: Missing input dependency'

tup touch pre_sexp.ml

# This should fail with the same error message, rather than a fatal error for
# empty graph execution.
update_fail_msg 'tup error: Missing input dependency'

eotup
