#include <sourcemeta/core/allocator.h>
#include <sourcemeta/core/benchmark.h>
#include <sourcemeta/core/json.h>
#include <sourcemeta/core/jsonpath.h>
#include <sourcemeta/core/jsonpointer.h>
#include <sourcemeta/core/memory.h>
#include <sourcemeta/core/numeric.h>
#include <sourcemeta/core/regex.h>
#include <sourcemeta/core/test.h>
#include <sourcemeta/core/time.h>
#include <sourcemeta/core/unicode.h>

#include <cmath>       // std::signbit
#include <cstddef>     // std::size_t
#include <functional>  // std::ref
#include <iostream>    // std::cout
#include <limits>      // std::numeric_limits
#include <sstream>     // std::istringstream
#include <string>      // std::string
#include <string_view> // std::string_view
#include <tuple>       // std::get
#include <type_traits> // std::is_copy_constructible_v
#include <utility>     // std::move

using sourcemeta::core::Decimal;
using sourcemeta::core::JSON;
using sourcemeta::core::parse_json;

namespace {
template <typename Error, typename Callback>
auto expect_error(Callback callback) -> void {
  bool caught{false};
  try {
    callback();
  } catch (const Error &) {
    caught = true;
  }
  EXPECT_TRUE(caught);
}
} // namespace

TEST(parsed_number_representations) {
  EXPECT_TRUE(parse_json("0.1").is_decimal());
  EXPECT_TRUE(parse_json("0.5").is_real());
  EXPECT_TRUE(parse_json("1").is_integer());
  EXPECT_TRUE(parse_json("1e0").is_decimal());
  EXPECT_TRUE(parse_json("9223372036854775808").is_decimal());
}

TEST(large_integer_ingress_preserves_adjacent_values) {
  const auto left{parse_json("9007199254740992")};
  const auto right{parse_json("9007199254740993")};
  EXPECT_NE(left, right);
  EXPECT_LT(left, right);
  EXPECT_NE(parse_json("9223372036854775808"),
            parse_json("9223372036854775809"));
}

TEST(parsed_decimal_addition_is_exact) {
  EXPECT_EQ(parse_json("0.1") + parse_json("0.2"), parse_json("0.3"));
  EXPECT_NE(JSON{0.1} + JSON{0.2}, JSON{0.3});
}

TEST(real_and_decimal_equality_uses_actual_values) {
  EXPECT_EQ(parse_json("0.5"), JSON{Decimal{"0.5"}});
  EXPECT_NE(parse_json("0.1"), JSON{0.1});
  EXPECT_EQ(Decimal::strict_from(0.1), Decimal{"0.1"});
  EXPECT_NE(Decimal::exact_from(0.1), Decimal{"0.1"});
}

TEST(integer_overflow_promotes_but_mixed_real_can_round) {
  EXPECT_EQ(parse_json("9223372036854775807") + JSON{1},
            parse_json("9223372036854775808"));
  EXPECT_EQ(parse_json("9007199254740993") + JSON{0.0},
            parse_json("9007199254740992"));
}

TEST(decimal_terminating_and_nonterminating_division) {
  EXPECT_EQ(Decimal{1} / Decimal{1048576}, Decimal{"0.00000095367431640625"});
  EXPECT_EQ(Decimal{1} / Decimal{3}, Decimal{"0.3333333333333333"});
  EXPECT_EQ(Decimal{"123456789012345678901234567890"} * Decimal{1},
            Decimal{"123456789012345678901234567890"});
  expect_error<sourcemeta::core::NumericDivisionByZeroError>(
      [] { static_cast<void>(Decimal{1} / Decimal{0}); });
}

TEST(nonfinite_intermediate_is_not_a_json_value) {
  expect_error<sourcemeta::core::JSONNonFiniteError>(
      [] { static_cast<void>(JSON{std::numeric_limits<double>::infinity()}); });
  expect_error<sourcemeta::core::JSONNonFiniteError>(
      [] { static_cast<void>(JSON{Decimal::nan()}); });
  EXPECT_TRUE(Decimal::infinity().is_infinite());
}

TEST(integer_negative_zero_loses_its_sign) {
  EXPECT_TRUE(parse_json("-0").is_integer());
  EXPECT_EQ(parse_json("-0"), JSON{0});
  EXPECT_FALSE(std::signbit(parse_json("-0").as_real()));
}

TEST(structural_equality_and_type_separation) {
  EXPECT_EQ(parse_json(R"({"a":1,"b":[2]})"), parse_json(R"({"b":[2],"a":1})"));
  EXPECT_EQ(parse_json("[1]"), parse_json("[1]"));
  EXPECT_NE(parse_json("[1,2]"), parse_json("[2,1]"));
  EXPECT_NE(JSON{false}, JSON{0});
}

TEST(native_ordering_is_not_jsonlogic_coercion) {
  EXPECT_LT(JSON{nullptr}, JSON{5});
  EXPECT_LT(JSON{nullptr}, JSON{-5});
  EXPECT_FALSE(JSON{"4"} < JSON{5});
  EXPECT_TRUE(JSON{"10"} < JSON{"2"});
  EXPECT_TRUE(JSON{"10"} <= JSON{"2"});
}

TEST(strict_numeric_parsers_do_not_consume_prefixes) {
  EXPECT_FALSE(sourcemeta::core::to_double("4kg").has_value());
  EXPECT_FALSE(sourcemeta::core::to_int64_t("4kg").has_value());
  EXPECT_FALSE(sourcemeta::core::to_double(" 4 ").has_value());
  EXPECT_EQ(sourcemeta::core::to_int64_t("4").value(), 4);
}

TEST(missing_null_and_empty_are_distinct) {
  const auto document{parse_json(R"({"null":null,"empty":""})")};
  EXPECT_TRUE(document.try_at("missing") == nullptr);
  const auto *present{document.try_at("null")};
  EXPECT_TRUE(present != nullptr);
  EXPECT_TRUE(present->is_null());
  EXPECT_TRUE(document.at("empty").is_string());
  EXPECT_TRUE(document.at("empty").to_string().empty());
  EXPECT_TRUE(sourcemeta::core::try_get(
                  JSON{0}, sourcemeta::core::Pointer{"missing"}) == nullptr);
}

TEST(pointer_addressing_is_not_dot_notation) {
  const auto document{
      parse_json(R"({"a.b":1,"a":{"b":2},"":3,"a/b":4,"~":5})")};
  EXPECT_EQ(
      sourcemeta::core::get(document, sourcemeta::core::to_pointer("/a.b")),
      JSON{1});
  EXPECT_EQ(
      sourcemeta::core::get(document, sourcemeta::core::to_pointer("/a/b")),
      JSON{2});
  EXPECT_EQ(sourcemeta::core::get(document, sourcemeta::core::to_pointer("/")),
            JSON{3});
  EXPECT_EQ(
      sourcemeta::core::get(document, sourcemeta::core::to_pointer("/a~1b")),
      JSON{4});
  EXPECT_EQ(
      sourcemeta::core::get(document, sourcemeta::core::to_pointer("/~0")),
      JSON{5});
}

TEST(cached_hash_lookup_preserves_literal_keys) {
  const auto document{parse_json(R"({"x":1,"x\u0000":2})")};
  const std::string_view key{"x\0", 2};
  const auto hash{JSON::Object::hash(key)};
  EXPECT_EQ(*document.try_at(key, hash), JSON{2});
  EXPECT_EQ(document.at("x"), JSON{1});
  EXPECT_FALSE(JSON::Object::hash(std::string_view{"x"}) == hash);
}

TEST(long_property_hash_collision_still_checks_the_key) {
  const std::string first(40, 'a');
  auto second{first};
  second.at(35) = 'b';
  const auto hash{JSON::Object::hash(first)};
  EXPECT_TRUE(hash == JSON::Object::hash(second));
  auto document{JSON::make_object()};
  document.assign(first, JSON{1});
  document.assign(second, JSON{2});
  EXPECT_EQ(*document.try_at(first, hash), JSON{1});
  EXPECT_EQ(*document.try_at(second, hash), JSON{2});
}

TEST(duplicate_members_are_retained_and_lookup_selects_first) {
  const auto document{parse_json(R"({"x":1,"x":2})")};
  EXPECT_EQ(document.size(), 2);
  EXPECT_EQ(document.at("x"), JSON{1});
}

TEST(owned_and_borrowed_results_are_available) {
  static_assert(
      !std::is_copy_constructible_v<sourcemeta::core::OwnedOrReference<JSON>>);
  const JSON original{42};
  const sourcemeta::core::OwnedOrReference<JSON> borrowed{original};
  EXPECT_TRUE(&borrowed.value() == &original);
  sourcemeta::core::OwnedOrReference<JSON> owned{JSON{7}};
  EXPECT_EQ(std::move(owned).to_owned(), JSON{7});
  const sourcemeta::core::OwnedOrReference<JSON> missing;
  EXPECT_FALSE(missing.has_value());
}

TEST(json_copy_is_independent) {
  auto original{parse_json(R"({"items":[1]})")};
  auto copy{original};
  copy.at("items").at(0) = JSON{9};
  EXPECT_EQ(original.at("items").at(0), JSON{1});
  EXPECT_EQ(copy.at("items").at(0), JSON{9});
}

TEST(unicode_units_and_validation_are_explicit) {
  const auto text{parse_json(R"("\ud83d\ude00X")")};
  EXPECT_EQ(text.to_string().size(), 5);
  EXPECT_EQ(JSON::size(text.to_string()), 2);
  EXPECT_EQ(sourcemeta::core::utf8_to_utf32(text.to_string()).value().size(),
            2);
  expect_error<sourcemeta::core::JSONParseError>(
      [] { static_cast<void>(parse_json(R"("\ud800")")); });
  std::string malformed{"\""};
  malformed.push_back(static_cast<char>(0xFF));
  malformed += '"';
  const auto raw{parse_json(malformed)};
  EXPECT_EQ(raw.to_string().size(), 1);
  EXPECT_FALSE(sourcemeta::core::utf8_to_utf32(raw.to_string()).has_value());
}

TEST(parse_limits_and_document_boundaries) {
  EXPECT_FALSE(sourcemeta::core::try_parse_json("[1]", 2).has_value());
  EXPECT_TRUE(sourcemeta::core::try_parse_json("[1]", 3).has_value());
  EXPECT_FALSE(sourcemeta::core::try_parse_json("true false").has_value());
  std::istringstream stream{"true false"};
  EXPECT_EQ(parse_json(stream), JSON{true});
  EXPECT_EQ(parse_json(stream), JSON{false});
}

TEST(position_tracking_and_parse_errors) {
  sourcemeta::core::PointerPositionTracker tracker;
  JSON document{nullptr};
  parse_json("{\n  \"x\": 1\n}", document, std::ref(tracker));
  const auto position{tracker.get(sourcemeta::core::Pointer{"x"})};
  EXPECT_TRUE(position.has_value());
  EXPECT_EQ(std::get<0>(position.value()), 2);
  bool caught{false};
  try {
    static_cast<void>(parse_json("{\n  \"x\": ?\n}"));
  } catch (const sourcemeta::core::JSONParseError &error) {
    caught = true;
    EXPECT_EQ(error.line(), 2);
    EXPECT_GT(error.column(), 0);
  }
  EXPECT_TRUE(caught);
}

TEST(empty_containers_are_not_a_generic_truthiness_function) {
  EXPECT_TRUE(JSON::make_array().empty());
  EXPECT_TRUE(JSON::make_object().empty());
  // JsonLogic gives these two empty containers different truthiness.
}

TEST(jsonpath_prepared_query_reuses_root_and_candidate_scope) {
  const sourcemeta::core::JSONPath query{"$.items[?@.quantity <= $.limit]"};
  const auto first{
      parse_json(R"({"limit":3,"items":[{"quantity":2},{"quantity":4}]})")};
  std::size_t count{0};
  query.evaluate(
      first, [&](const JSON &value, const sourcemeta::core::WeakPointer &path) {
        EXPECT_EQ(value.at("quantity"), JSON{2});
        EXPECT_EQ(sourcemeta::core::JSONPath::normalize(path), "$['items'][0]");
        ++count;
      });
  EXPECT_EQ(count, 1);
  count = 0;
  query.evaluate(
      parse_json(R"({"limit":0,"items":[{"quantity":2}]})"),
      [&](const JSON &, const sourcemeta::core::WeakPointer &) { ++count; });
  EXPECT_EQ(count, 0);
  EXPECT_FALSE(sourcemeta::core::JSONPath::from_json(JSON{"$[?"}).has_value());
}

TEST(regex_facilities_have_an_explicit_dialect) {
  const auto permissive{sourcemeta::core::to_regex("(?=ab)ab")};
  EXPECT_TRUE(permissive.has_value());
  EXPECT_TRUE(sourcemeta::core::matches(permissive.value(), "ab"));
  EXPECT_FALSE(sourcemeta::core::to_regex("[").has_value());
  EXPECT_FALSE(sourcemeta::core::to_regex(
                   "(?=ab)ab", sourcemeta::core::RegexDialect::IRegexp)
                   .has_value());
}

TEST(time_facilities_validate_calendar_input) {
  EXPECT_FALSE(sourcemeta::core::is_rfc3339_fulldate("2025-02-29"));
  EXPECT_TRUE(sourcemeta::core::is_rfc3339_fulldate("2024-02-29"));
  EXPECT_TRUE(
      sourcemeta::core::is_rfc3339_datetime("2025-01-01T00:30:00+01:00"));
  EXPECT_TRUE(sourcemeta::core::is_rfc3339_datetime("2025-01-01T00:00:00Z"));
}

TEST(benchmark_state_and_escape_barrier_smoke) {
  sourcemeta::core::BenchmarkState state{2};
  std::size_t count{0};
  for (auto iteration : state) {
    sourcemeta::core::benchmark_do_not_optimize(iteration);
    ++count;
  }
  EXPECT_EQ(count, 2);
  EXPECT_TRUE(state.measured());
}

auto main(int argc, char **argv) -> int {
  if (argc == 2 && std::string_view{argv[1]} == "--negative-control") {
    sourcemeta::core::test_register(
        "control", "deliberately_wrong_real_equality", __FILE__, __LINE__,
        [] { EXPECT_EQ(parse_json("0.1") + parse_json("0.2"), JSON{0.3}); });
    return sourcemeta::core::test_run(1, argv);
  }
  if (argc == 2 && std::string_view{argv[1]} == "--facts") {
    auto facts{JSON::make_object()};
    facts.assign("sizeofJSON", JSON{sizeof(JSON)});
    facts.assign("sizeofDecimal", JSON{sizeof(Decimal)});
    facts.assign("sizeofPropertyHash", JSON{sizeof(JSON::Object::hash_type)});
    facts.assign("decimalDivisionRendering",
                 JSON{(Decimal{1} / Decimal{1048576}).to_string()});
    facts.assign("allocator", JSON{sourcemeta::core::allocator() ==
                                           sourcemeta::core::Allocator::System
                                       ? "system"
                                       : "mimalloc"});
    sourcemeta::core::stringify(facts, std::cout);
    std::cout << '\n';
    return 0;
  }
  return sourcemeta::core::test_run(argc, argv);
}
