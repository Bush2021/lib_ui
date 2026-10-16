// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Bush2021, 2026
#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

struct TextWithEntities;
struct TextWithTags;

namespace AyuTextSpacing {

enum class SpanKind : std::uint8_t {
	Word,
	Object,
	Block,
};

struct Span {
	int from = 0;
	int till = 0;
	SpanKind kind = SpanKind::Word;
};

// Sorted text offsets, each of them gets a space right before it.
[[nodiscard]] std::vector<int> FindInsertions(
	std::u16string_view text,
	const std::vector<Span> &spans);
[[nodiscard]] std::vector<int> FindInsertions(const TextWithEntities &text);

void ShiftRange(const std::vector<int> &insertions, int &from, int &till);

[[nodiscard]] TextWithEntities AddSpaces(
	const TextWithEntities &text,
	const std::vector<int> &insertions);
[[nodiscard]] TextWithTags AddSpaces(
	const TextWithTags &text,
	const std::vector<int> &insertions);

} // namespace AyuTextSpacing
