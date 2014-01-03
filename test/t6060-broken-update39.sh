#! /bin/sh -e
# tup - A file-based build system
#
# Copyright (C) 2011-2014  Mike Shal <marfey@gmail.com>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

# Test for a circular dependency fatal tup error. This is triggered due to the
# transitive dependency detection, which originally circumvented other checks.

. ./tup.sh
check_no_windows shell

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
