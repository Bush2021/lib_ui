// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Bush2021, 2026
#include "ayu/ayu_text_spacing.h"

#include "ui/text/text_entity.h"

#include <algorithm>
#include <iterator>
#include <optional>

namespace AyuTextSpacing {
namespace {

enum class Kind : std::uint8_t {
	Space,
	Cjk,
	Letter,
	Symbol,
	Other,
};

struct Cell {
	int from = 0;
	int till = 0;
	char32_t ch = 0;
	Kind kind = Kind::Other;
	bool newline = false;
	bool plain = true;
};

enum PositionFlag : std::uint8_t {
	kWordStart = 0x01,
	kWordEnd = 0x02,
	kBarrier = 0x04,
};

[[nodiscard]] constexpr bool IsNewline(char32_t ch) {
	return (ch >= 0x0A && ch <= 0x0D)
		|| (ch == 0x85)
		|| (ch == 0x2028)
		|| (ch == 0x2029);
}

[[nodiscard]] constexpr bool IsSpace(char32_t ch) {
	return (ch == 0x09)
		|| (ch == 0x20)
		|| IsNewline(ch)
		|| (ch == 0xA0)
		|| (ch == 0x1680)
		|| (ch >= 0x2000 && ch <= 0x200B)
		|| (ch == 0x202F)
		|| (ch == 0x205F)
		|| (ch == 0x3000);
}

[[nodiscard]] constexpr bool IsExtending(char32_t ch) {
	return (ch >= 0x0300 && ch <= 0x036F)
		|| (ch >= 0x0483 && ch <= 0x0489)
		|| (ch >= 0x1AB0 && ch <= 0x1AFF)
		|| (ch >= 0x1DC0 && ch <= 0x1DFF)
		|| (ch >= 0x20D0 && ch <= 0x20FF)
		|| (ch >= 0xFE00 && ch <= 0xFE0F)
		|| (ch >= 0xFE20 && ch <= 0xFE2F)
		|| (ch >= 0xE0020 && ch <= 0xE007F)
		|| (ch >= 0xE0100 && ch <= 0xE01EF);
}

// Hangul stays out: Korean particles attach to Latin words without a space.
[[nodiscard]] constexpr bool IsCjk(char32_t ch) {
	return (ch >= 0x2E80 && ch <= 0x2FDF)
		|| (ch >= 0x3005 && ch <= 0x3007)
		|| (ch >= 0x3021 && ch <= 0x3029)
		|| (ch >= 0x3031 && ch <= 0x3035)
		|| (ch >= 0x3038 && ch <= 0x303C)
		|| (ch >= 0x3040 && ch <= 0x30FF && ch != 0x30FB)
		|| (ch >= 0x3100 && ch <= 0x312F)
		|| (ch >= 0x31A0 && ch <= 0x31FF)
		|| (ch >= 0x3200 && ch <= 0x32FF)
		|| (ch >= 0x3400 && ch <= 0x4DBF)
		|| (ch >= 0x4E00 && ch <= 0x9FFF)
		|| (ch >= 0xF900 && ch <= 0xFAFF)
		|| (ch >= 0x1B000 && ch <= 0x1B16F)
		|| (ch >= 0x20000 && ch <= 0x3FFFF);
}

[[nodiscard]] constexpr bool IsLetter(char32_t ch) {
	if (ch < 0x80) {
		return (ch >= '0' && ch <= '9')
			|| (ch >= 'A' && ch <= 'Z')
			|| (ch >= 'a' && ch <= 'z');
	}
	return (ch == 0xAA)
		|| (ch == 0xB5)
		|| (ch == 0xBA)
		|| (ch >= 0xBC && ch <= 0xBE)
		|| (ch >= 0xC0 && ch <= 0x2AF && ch != 0xD7 && ch != 0xF7)
		|| (ch >= 0x370 && ch <= 0x3FF && ch != 0x37E && ch != 0x387)
		|| (ch >= 0x400 && ch <= 0x52F)
		|| (ch >= 0x1E00 && ch <= 0x1FFF)
		|| (ch >= 0x2100 && ch <= 0x218F && ch != 0x2120 && ch != 0x2122)
		|| (ch >= 0x2C60 && ch <= 0x2C7F)
		|| (ch >= 0xA720 && ch <= 0xA7FF)
		|| (ch >= 0xAB30 && ch <= 0xAB6F)
		|| (ch >= 0xFB00 && ch <= 0xFB06);
}

// Superscripts, subscripts and trademark signs stick to what precedes them.
[[nodiscard]] constexpr bool IsSuperscript(char32_t ch) {
	return (ch == 0xAE)
		|| (ch == 0xB2)
		|| (ch == 0xB3)
		|| (ch == 0xB9)
		|| (ch == 0x2070)
		|| (ch == 0x2071)
		|| (ch >= 0x2074 && ch <= 0x207C)
		|| (ch >= 0x207E && ch <= 0x208C)
		|| (ch >= 0x208E && ch <= 0x209C)
		|| (ch == 0x2120)
		|| (ch == 0x2122);
}

[[nodiscard]] constexpr bool IsSymbol(char32_t ch) {
	if (ch < 0x80) {
		return (ch > 0x20) && (ch < 0x7F);
	}
	return (ch >= 0xA1 && ch <= 0xBF && ch != 0xAD && ch != 0xB7)
		|| (ch == 0xD7)
		|| (ch == 0xF7)
		|| (ch >= 0x2030 && ch <= 0x2034)
		|| (ch == 0x207D)
		|| (ch == 0x208D)
		|| (ch >= 0x20A0 && ch <= 0x20CF);
}

[[nodiscard]] constexpr Kind Classify(char32_t ch) {
	if (IsSpace(ch)) {
		return Kind::Space;
	} else if (IsCjk(ch)) {
		return Kind::Cjk;
	} else if (IsLetter(ch) || IsSuperscript(ch)) {
		return Kind::Letter;
	} else if (IsSymbol(ch)) {
		return Kind::Symbol;
	}
	return Kind::Other;
}

[[nodiscard]] constexpr bool IsCurrency(char32_t ch) {
	return (ch == '$')
		|| (ch >= 0xA2 && ch <= 0xA5)
		|| (ch >= 0x20A0 && ch <= 0x20CF);
}

[[nodiscard]] constexpr bool IsTrailingSymbol(char32_t ch) {
	switch (ch) {
	case '\'': case '%': case '!': case '?':
	case '.': case ',': case ';': case ':':
	case 0xA9: case 0xB0:
		return true;
	}
	return IsCurrency(ch) || (ch >= 0x2030 && ch <= 0x2034);
}

[[nodiscard]] constexpr bool StopsUrl(char32_t ch) {
	return (ch == '<')
		|| (ch == '>')
		|| (ch == '"')
		|| (ch == '`')
		|| (ch == 0x2018)
		|| (ch == 0x2019)
		|| (ch == 0x201C)
		|| (ch == 0x201D)
		|| (ch == 0x2026)
		|| (ch >= 0x3000 && ch <= 0x303F)
		|| (ch >= 0xE000 && ch <= 0xF8FF)
		|| (ch >= 0xFF00 && ch <= 0xFFEF);
}

[[nodiscard]] constexpr char32_t OpeningFor(char32_t ch) {
	switch (ch) {
	case ')': return '(';
	case ']': return '[';
	case '}': return '{';
	case '>': return '<';
	}
	return 0;
}

[[nodiscard]] bool MayHaveCjk(std::u16string_view text) {
	return std::any_of(text.begin(), text.end(), [](char16_t ch) {
		return (ch >= 0x2E80);
	});
}

[[nodiscard]] std::vector<Cell> BuildCells(std::u16string_view text) {
	auto result = std::vector<Cell>();
	result.reserve(text.size());
	const auto size = int(text.size());
	for (auto i = 0; i != size;) {
		auto ch = char32_t(text[i]);
		auto length = 1;
		if (ch >= 0xD800 && ch <= 0xDBFF && i + 1 < size) {
			const auto low = char32_t(text[i + 1]);
			if (low >= 0xDC00 && low <= 0xDFFF) {
				ch = 0x10000 + ((ch - 0xD800) << 10) + (low - 0xDC00);
				length = 2;
			}
		}
		if (IsExtending(ch)
			&& !result.empty()
			&& result.back().kind != Kind::Space) {
			auto &last = result.back();
			last.till = i + length;
			if (ch == 0xFE0F || ch == 0x20E3) {
				// Emoji are never spaced, and these marks make the base one.
				last.kind = Kind::Other;
			}
		} else {
			result.push_back({
				.from = i,
				.till = i + length,
				.ch = ch,
				.kind = Classify(ch),
				.newline = IsNewline(ch),
			});
		}
		i += length;
	}
	return result;
}

class Scanner final {
public:
	Scanner(std::u16string_view text, const std::vector<Span> &spans);

	[[nodiscard]] std::vector<int> result() const;

private:
	void protect(int from, int till, SpanKind kind);
	void protectUrls();
	[[nodiscard]] int urlSchemeLength(int index) const;
	void pairBrackets();
	void prepareTokens();

	[[nodiscard]] Kind leftKind(int index) const;
	[[nodiscard]] Kind rightKind(int index) const;
	[[nodiscard]] bool hasLetters(int from, int till) const;
	[[nodiscard]] bool opensWord(int index) const;
	[[nodiscard]] bool closesWord(int index) const;
	[[nodiscard]] bool closesWithPlus(int index) const;
	[[nodiscard]] bool closesWithGrade(int index) const;
	[[nodiscard]] bool isFlag(int index) const;
	[[nodiscard]] int punctuationEnd(int index) const;
	[[nodiscard]] bool allowed(int position) const;

	std::vector<Cell> _cells;
	std::vector<int> _inside;
	std::vector<std::uint8_t> _flags;
	std::vector<int> _letters;
	std::vector<int> _tokenStart;
	std::vector<int> _tokenEnd;
	std::vector<int> _match;

};

Scanner::Scanner(std::u16string_view text, const std::vector<Span> &spans)
: _cells(BuildCells(text))
, _inside(text.size() + 2, 0)
, _flags(text.size() + 1, 0) {
	const auto size = int(text.size());
	for (const auto &span : spans) {
		const auto from = std::clamp(span.from, 0, size);
		const auto till = std::clamp(span.till, from, size);
		if (from == till) {
			continue;
		} else if (span.kind == SpanKind::Block) {
			_flags[from] |= kBarrier;
			_flags[till] |= kBarrier;
		} else {
			protect(from, till, span.kind);
		}
	}
	protectUrls();
	for (auto i = 1; i != int(_inside.size()); ++i) {
		_inside[i] += _inside[i - 1];
	}
	pairBrackets();
	prepareTokens();
}

void Scanner::protect(int from, int till, SpanKind kind) {
	++_inside[from + 1];
	--_inside[till];
	if (kind == SpanKind::Word) {
		_flags[from] |= kWordStart;
		_flags[till] |= kWordEnd;
	}
	const auto first = std::lower_bound(
		_cells.begin(),
		_cells.end(),
		from,
		[](const Cell &cell, int value) { return cell.from < value; });
	for (auto i = first; i != _cells.end() && i->from < till; ++i) {
		i->plain = false;
		if (kind == SpanKind::Object) {
			i->kind = Kind::Other;
		}
	}
}

int Scanner::urlSchemeLength(int index) const {
	const auto count = int(_cells.size());
	const auto lower = [&](int i) {
		const auto ch = _cells[i].ch;
		return (ch >= 'A' && ch <= 'Z') ? (ch + ('a' - 'A')) : ch;
	};
	if (index + 7 > count) {
		return 0;
	}
	for (auto i = 0; i != 4; ++i) {
		if (lower(index + i) != U"http"[i]) {
			return 0;
		}
	}
	auto i = index + 4;
	if (lower(i) == 's') {
		++i;
	}
	if (i + 3 > count
		|| _cells[i].ch != ':'
		|| _cells[i + 1].ch != '/'
		|| _cells[i + 2].ch != '/') {
		return 0;
	}
	return i + 3 - index;
}

// Previews and outgoing drafts may carry links without entities.
void Scanner::protectUrls() {
	const auto count = int(_cells.size());
	for (auto index = 0; index < count; ++index) {
		if (!_cells[index].plain
			|| (index > 0 && _cells[index - 1].kind == Kind::Letter)) {
			continue;
		}
		const auto scheme = urlSchemeLength(index);
		if (!scheme) {
			continue;
		}
		auto end = index + scheme;
		auto unbalanced = 0;
		while (end < count
			&& _cells[end].plain
			&& _cells[end].kind != Kind::Space
			&& !StopsUrl(_cells[end].ch)) {
			if (_cells[end].ch == '(') {
				--unbalanced;
			} else if (_cells[end].ch == ')') {
				++unbalanced;
			}
			++end;
		}
		while (end > index + scheme) {
			const auto ch = _cells[end - 1].ch;
			if (ch == ')' && unbalanced > 0) {
				--unbalanced;
			} else if (ch != '.'
				&& ch != ','
				&& ch != ';'
				&& ch != ':'
				&& ch != '!'
				&& ch != '?'
				&& ch != '\''
				&& ch != '"') {
				break;
			}
			--end;
		}
		if (end > index + scheme) {
			protect(_cells[index].from, _cells[end - 1].till, SpanKind::Word);
		}
		index = end - 1;
	}
}

void Scanner::pairBrackets() {
	const auto count = int(_cells.size());
	_match.assign(count, -1);
	auto brackets = std::vector<int>();
	auto quote = -1;
	auto backtick = -1;
	for (auto index = 0; index != count; ++index) {
		const auto &cell = _cells[index];
		if (cell.newline) {
			brackets.clear();
			quote = backtick = -1;
			continue;
		} else if (!cell.plain || cell.kind != Kind::Symbol) {
			continue;
		}
		const auto pair = [&](int &opened) {
			if (opened < 0) {
				opened = index;
			} else {
				_match[opened] = index;
				_match[index] = opened;
				opened = -1;
			}
		};
		switch (cell.ch) {
		case '(': case '[': case '{':
			brackets.push_back(index);
			break;
		case '<':
			// List<String> pairs up, while <p> markup stays as typed.
			if (index > 0 && _cells[index - 1].kind == Kind::Letter) {
				brackets.push_back(index);
			}
			break;
		case ')': case ']': case '}': case '>': {
			const auto opening = OpeningFor(cell.ch);
			const auto i = std::find_if(
				brackets.rbegin(),
				brackets.rend(),
				[&](int j) { return _cells[j].ch == opening; });
			if (i != brackets.rend()) {
				_match[*i] = index;
				_match[index] = *i;
				brackets.erase(std::prev(i.base()), brackets.end());
			}
		} break;
		case '"':
			pair(quote);
			break;
		case '`':
			pair(backtick);
			break;
		}
	}
}

void Scanner::prepareTokens() {
	const auto count = int(_cells.size());
	const auto inToken = [&](int i) {
		return (_cells[i].kind == Kind::Letter)
			|| (_cells[i].kind == Kind::Symbol);
	};
	_letters.assign(count + 1, 0);
	_tokenStart.assign(count, 0);
	_tokenEnd.assign(count, 0);
	for (auto i = 0; i != count; ++i) {
		_letters[i + 1] = _letters[i]
			+ ((_cells[i].kind == Kind::Letter) ? 1 : 0);
		_tokenStart[i] = (i > 0 && inToken(i) && inToken(i - 1))
			? _tokenStart[i - 1]
			: i;
	}
	for (auto i = count; i != 0;) {
		--i;
		_tokenEnd[i] = (i + 1 < count && inToken(i) && inToken(i + 1))
			? _tokenEnd[i + 1]
			: (i + 1);
	}
}

Kind Scanner::leftKind(int index) const {
	const auto &cell = _cells[index];
	return (cell.kind == Kind::Symbol && (_flags[cell.till] & kWordEnd))
		? Kind::Letter
		: cell.kind;
}

Kind Scanner::rightKind(int index) const {
	const auto &cell = _cells[index];
	return (cell.kind == Kind::Symbol && (_flags[cell.from] & kWordStart))
		? Kind::Letter
		: cell.kind;
}

bool Scanner::hasLetters(int from, int till) const {
	return (till > from) && (_letters[till] > _letters[from]);
}

bool Scanner::isFlag(int index) const {
	const auto count = int(_cells.size());
	const auto lowercase = [&](int i) {
		return (i < count) && (_cells[i].ch >= 'a') && (_cells[i].ch <= 'z');
	};
	if (index + 1 < count && _cells[index + 1].ch == '-') {
		return lowercase(index + 2);
	}
	return lowercase(index + 1)
		&& !(index + 2 < count && _cells[index + 2].kind == Kind::Letter);
}

// C++, 100+ and A+ keep the plus, Python+中文 is a separator.
bool Scanner::closesWithPlus(int index) const {
	const auto start = _tokenStart[index];
	if (index == start) {
		return false;
	}
	const auto previous = _cells[index - 1].ch;
	if (previous == '+') {
		return hasLetters(start, index);
	} else if (previous >= '0' && previous <= '9') {
		return true;
	}
	return (index - start == 1) && (_cells[start].kind == Kind::Letter);
}

bool Scanner::opensWord(int index) const {
	const auto count = int(_cells.size());
	const auto ch = _cells[index].ch;
	const auto tokenLetters = [&] {
		return hasLetters(index, _tokenEnd[index]);
	};
	switch (ch) {
	case '(': case '[': case '{': case '"': case '`':
		if (_match[index] > index) {
			return hasLetters(index + 1, _match[index]);
		}
		return (_match[index] < 0) && tokenLetters();
	case '\'':
		// 林依諾's keeps the possessive tight.
		if (index + 1 < count
			&& (_cells[index + 1].ch == 's' || _cells[index + 1].ch == 'S')
			&& !(index + 2 < count
				&& _cells[index + 2].kind == Kind::Letter)) {
			return false;
		}
		return tokenLetters();
	case '@': case '#': case 0xA9:
		return tokenLetters();
	case '+':
		return (index + 1 < count)
			&& (_cells[index + 1].ch >= '0')
			&& (_cells[index + 1].ch <= '9');
	case '-':
		return isFlag(index);
	}
	return IsCurrency(ch) && tokenLetters();
}

bool Scanner::closesWord(int index) const {
	const auto ch = _cells[index].ch;
	const auto start = _tokenStart[index];
	const auto tokenLetters = [&] {
		const auto group = _match[start];
		return hasLetters((group >= 0 && group < start) ? group : start, index);
	};
	switch (ch) {
	case ')': case ']': case '}':
		return (_match[index] >= 0)
			? hasLetters(_match[index] + 1, index)
			: tokenLetters();
	case '>': case '"': case '`':
		if (_match[index] >= 0) {
			return (_match[index] < index)
				&& hasLetters(_match[index] + 1, index);
		}
		return (ch != '>') && tokenLetters();
	case '#':
		return (index > start) && (_cells[index - 1].kind == Kind::Letter);
	case '+':
		return closesWithPlus(index);
	case '-':
		return closesWithGrade(index);
	}
	return IsTrailingSymbol(ch) && tokenLetters();
}

// D- and B-- are grades, Vinta-陳 is a separator.
bool Scanner::closesWithGrade(int index) const {
	const auto start = _tokenStart[index];
	if (index == start || _cells[start].kind != Kind::Letter) {
		return false;
	}
	for (auto i = start + 1; i != index; ++i) {
		if (_cells[i].ch != '-') {
			return false;
		}
	}
	return true;
}

// A comma or a mark after CJK takes the space after itself: 你好,Tom.
int Scanner::punctuationEnd(int index) const {
	const auto count = int(_cells.size());
	const auto run = [](char32_t ch) {
		return (ch == ',') || (ch == '!') || (ch == '?');
	};
	const auto ch = _cells[index].ch;
	if (run(ch)) {
		auto end = index;
		while (end < count
			&& _cells[end].plain
			&& _cells[end].kind == Kind::Symbol
			&& run(_cells[end].ch)) {
			++end;
		}
		return (end < count && rightKind(end) == Kind::Letter) ? end : 0;
	} else if (ch == ':' && index + 1 < count) {
		// One letter after a colon is an emoticon like :D, not a word.
		const auto next = index + 1;
		if (rightKind(next) != Kind::Letter) {
			return 0;
		} else if (_flags[_cells[next].from] & kWordStart) {
			return next;
		}
		return (next + 1 < count && _cells[next + 1].kind == Kind::Letter)
			? next
			: 0;
	}
	return 0;
}

bool Scanner::allowed(int position) const {
	return !_inside[position] && !(_flags[position] & kBarrier);
}

std::vector<int> Scanner::result() const {
	auto result = std::vector<int>();
	const auto count = int(_cells.size());
	for (auto index = 1; index < count; ++index) {
		const auto position = _cells[index].from;
		if (!allowed(position)) {
			continue;
		}
		const auto left = leftKind(index - 1);
		const auto right = rightKind(index);
		if (left == Kind::Cjk && right == Kind::Letter) {
			if (!IsSuperscript(_cells[index].ch)) {
				result.push_back(position);
			}
		} else if (left == Kind::Letter && right == Kind::Cjk) {
			result.push_back(position);
		} else if (left == Kind::Cjk && right == Kind::Symbol) {
			if (opensWord(index)) {
				result.push_back(position);
			} else if (const auto end = punctuationEnd(index)) {
				if (allowed(_cells[end].from)) {
					result.push_back(_cells[end].from);
				}
			}
		} else if (left == Kind::Symbol && right == Kind::Cjk) {
			if (closesWord(index - 1)) {
				result.push_back(position);
			}
		}
	}
	std::sort(result.begin(), result.end());
	result.erase(std::unique(result.begin(), result.end()), result.end());
	return result;
}

[[nodiscard]] std::optional<SpanKind> SpanKindFor(
		const EntityInText &entity,
		const QString &text) {
	switch (entity.type()) {
	case EntityType::Url:
	case EntityType::Email:
	case EntityType::Hashtag:
	case EntityType::Cashtag:
	case EntityType::Mention:
	case EntityType::MentionName:
	case EntityType::BotCommand:
	case EntityType::MediaTimestamp:
	case EntityType::Phone:
	case EntityType::BankCard:
	case EntityType::TonAddress:
	case EntityType::Code:
		return SpanKind::Word;
	case EntityType::CustomUrl:
		if (entity.data() == text.mid(entity.offset(), entity.length())) {
			return SpanKind::Word;
		}
		return std::nullopt;
	case EntityType::CustomEmoji:
	case EntityType::Pre:
	case EntityType::FormattedDate:
		return SpanKind::Object;
	case EntityType::Blockquote:
		return SpanKind::Block;
	default:
		return std::nullopt;
	}
}

[[nodiscard]] QString InsertSpaces(
		const QString &text,
		const std::vector<int> &insertions) {
	auto result = QString();
	result.reserve(text.size() + int(insertions.size()));
	auto from = 0;
	for (const auto position : insertions) {
		result.append(text.constData() + from, position - from);
		result.append(QChar::Space);
		from = position;
	}
	result.append(text.constData() + from, text.size() - from);
	return result;
}

} // namespace

std::vector<int> FindInsertions(
		std::u16string_view text,
		const std::vector<Span> &spans) {
	if (!MayHaveCjk(text)) {
		return {};
	}
	return Scanner(text, spans).result();
}

std::vector<int> FindInsertions(const TextWithEntities &text) {
	const auto view = std::u16string_view(
		reinterpret_cast<const char16_t*>(text.text.constData()),
		std::size_t(text.text.size()));
	if (!MayHaveCjk(view)) {
		return {};
	}
	auto spans = std::vector<Span>();
	for (const auto &entity : text.entities) {
		if (!entity.validForText(text.text.size())) {
			continue;
		} else if (const auto kind = SpanKindFor(entity, text.text)) {
			spans.push_back({
				.from = entity.offset(),
				.till = entity.offset() + entity.length(),
				.kind = *kind,
			});
		}
	}
	return Scanner(view, spans).result();
}

void ShiftRange(const std::vector<int> &insertions, int &from, int &till) {
	const auto empty = (till <= from);
	from += int(std::upper_bound(insertions.begin(), insertions.end(), from)
		- insertions.begin());
	till = empty
		? from
		: (till + int(std::lower_bound(
			insertions.begin(),
			insertions.end(),
			till) - insertions.begin()));
}

TextWithEntities AddSpaces(
		const TextWithEntities &text,
		const std::vector<int> &insertions) {
	if (insertions.empty()) {
		return text;
	}
	auto result = TextWithEntities{ InsertSpaces(text.text, insertions) };
	result.entities.reserve(text.entities.size());
	for (const auto &entity : text.entities) {
		auto from = entity.offset();
		auto till = from + entity.length();
		ShiftRange(insertions, from, till);
		auto shifted = EntityInText(
			entity.type(),
			from,
			till - from,
			entity.data());
		if (entity.isLocal()) {
			shifted.setLocal();
		}
		result.entities.push_back(std::move(shifted));
	}
	return result;
}

TextWithTags AddSpaces(
		const TextWithTags &text,
		const std::vector<int> &insertions) {
	if (insertions.empty()) {
		return text;
	}
	auto result = TextWithTags{ InsertSpaces(text.text, insertions) };
	result.tags.reserve(text.tags.size());
	for (const auto &tag : text.tags) {
		auto from = tag.offset;
		auto till = from + tag.length;
		ShiftRange(insertions, from, till);
		result.tags.push_back({ from, till - from, tag.id });
	}
	return result;
}

} // namespace AyuTextSpacing
