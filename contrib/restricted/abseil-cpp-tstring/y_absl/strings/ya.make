# Generated by devtools/yamaker.

LIBRARY()

LICENSE(Apache-2.0)

LICENSE_TEXTS(.yandex_meta/licenses.list.txt)

PEERDIR(
    contrib/restricted/abseil-cpp-tstring/y_absl/base
    contrib/restricted/abseil-cpp-tstring/y_absl/numeric
)

ADDINCL(
    GLOBAL contrib/restricted/abseil-cpp-tstring
)

NO_COMPILER_WARNINGS()

SRCDIR(contrib/restricted/abseil-cpp-tstring/y_absl)

SRCS(
    status/statusor.cc
    strings/ascii.cc
    strings/charconv.cc
    strings/cord.cc
    strings/cord_analysis.cc
    strings/cord_buffer.cc
    strings/escaping.cc
    strings/internal/charconv_bigint.cc
    strings/internal/charconv_parse.cc
    strings/internal/cord_internal.cc
    strings/internal/cord_rep_btree.cc
    strings/internal/cord_rep_btree_navigator.cc
    strings/internal/cord_rep_btree_reader.cc
    strings/internal/cord_rep_consume.cc
    strings/internal/cord_rep_crc.cc
    strings/internal/cord_rep_ring.cc
    strings/internal/cordz_functions.cc
    strings/internal/cordz_handle.cc
    strings/internal/cordz_info.cc
    strings/internal/cordz_sample_token.cc
    strings/internal/escaping.cc
    strings/internal/memutil.cc
    strings/internal/ostringstream.cc
    strings/internal/str_format/arg.cc
    strings/internal/str_format/bind.cc
    strings/internal/str_format/extension.cc
    strings/internal/str_format/float_conversion.cc
    strings/internal/str_format/output.cc
    strings/internal/str_format/parser.cc
    strings/internal/utf8.cc
    strings/match.cc
    strings/numbers.cc
    strings/str_cat.cc
    strings/str_replace.cc
    strings/str_split.cc
    strings/string_view.cc
    strings/substitute.cc
)

END()
