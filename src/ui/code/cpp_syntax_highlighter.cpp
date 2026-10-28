#include "cpp_syntax_highlighter.hpp"
#include "ui/shared/theme.hpp"

#include <QPalette>
#include <QTextDocument>
#include <QSet>
#include <QStringList>
#include <QStringView>
#include <QTextBlock>

#include <algorithm>

namespace xray::ui {

namespace {

enum BlockState {
    NormalState = 0,
    BlockCommentState = 1,
    LineCommentState = 2,
    StringState = 3,
    CharacterState = 4,
    StringEscapeState = 5,
    CharacterEscapeState = 6,
    HeaderState = 7,
    BlockCommentStarState = 8,
    FirstRawStringState = 16
};

// The low bits identify a lexical mode (including the raw-string terminator).
// The flags preserve directive context across splices and Unicode Qt blocks.
constexpr int ModeMask = (1 << 24) - 1;
constexpr int DirectiveFlag = 1 << 24;
constexpr int LineStartFlag = 1 << 25;
constexpr int HeaderExpectedFlag = 1 << 26;
constexpr int DirectiveNameFlag = 1 << 27;

struct BlockContext {
    int mode = NormalState;
    bool directive = false;
    bool lineStart = true;
    bool headerExpected = false;
    bool directiveNameExpected = false;
};

BlockContext decodeContext(int state) {
    if (state < 0) return {};
    return {
        state & ModeMask,
        (state & DirectiveFlag) != 0,
        (state & LineStartFlag) != 0,
        (state & HeaderExpectedFlag) != 0,
        (state & DirectiveNameFlag) != 0
    };
}

int encodeContext(const BlockContext& context) {
    return context.mode
        | (context.directive ? DirectiveFlag : 0)
        | (context.lineStart ? LineStartFlag : 0)
        | (context.headerExpected ? HeaderExpectedFlag : 0)
        | (context.directiveNameExpected ? DirectiveNameFlag : 0);
}

bool isIdentifierStart(QChar character) {
    return character == QLatin1Char('_') || character.isLetter();
}

bool isIdentifierPart(QChar character) {
    const auto category = character.category();

    return isIdentifierStart(character)
        || character.isNumber()
        || category == QChar::Mark_NonSpacing
        || category == QChar::Mark_SpacingCombining
        || category == QChar::Mark_Enclosing;
}

bool isAsciiDigit(QChar character) {
    return character >= QLatin1Char('0')
        && character <= QLatin1Char('9');
}

bool isKeyword(const QString& word) {
    static const auto keywords = [] {
        const auto words = QStringLiteral(
            "alignas alignof and and_eq asm auto bitand bitor bool break "
            "case catch char char8_t char16_t char32_t class compl concept const "
            "consteval constexpr constinit const_cast continue "
            "co_await co_return co_yield decltype default delete do double "
            "dynamic_cast else enum explicit export extern false float "
            "for friend goto if inline int long mutable namespace new noexcept "
            "not not_eq nullptr operator or or_eq private protected public "
            "register reinterpret_cast requires return short signed sizeof "
            "static static_assert static_cast struct switch template this "
            "thread_local throw true try typedef typeid typename union "
            "unsigned using virtual void volatile wchar_t while xor xor_eq"
        ).split(QLatin1Char(' '), Qt::SkipEmptyParts);

        return QSet<QString>(words.begin(), words.end());
    }();

    return keywords.contains(word);
}

int numberEnd(const QString& text, int start) {
    const int length = static_cast<int>(text.size());
    int position = start + 1;

    while (position < length) {
        const QChar current = text.at(position);

        if (isIdentifierPart(current)
            || current == QLatin1Char('.')) {
            ++position;
            continue;
        }

        if (current == QLatin1Char('\'')
            && position + 1 < length
            && isIdentifierPart(text.at(position + 1))) {
            position += 2;
            continue;
        }

        if ((current == QLatin1Char('+')
             || current == QLatin1Char('-'))
            && QStringLiteral("eEpP").contains(
                text.at(position - 1)
            )) {
            ++position;
            continue;
        }

        break;
    }

    return position;
}

struct RawStringOpening {
    int contentStart = -1;
    QString ending;
};

RawStringOpening findRawStringOpening(
    const QString& text,
    int start
) {
    static const QStringList prefixes {
        QStringLiteral("u8R\""),
        QStringLiteral("uR\""),
        QStringLiteral("UR\""),
        QStringLiteral("LR\""),
        QStringLiteral("R\"")
    };

    const auto tail = QStringView(text).mid(start);
    const int length = static_cast<int>(text.size());

    for (const auto& prefix : prefixes) {
        if (!tail.startsWith(QStringView(prefix))) {
            continue;
        }

        const int delimiterStart =
            start + static_cast<int>(prefix.size());

        for (int position = delimiterStart;
             position < length && position - delimiterStart <= 16;
             ++position) {
            const QChar character = text.at(position);

            if (character == QLatin1Char('(')) {
                return {
                    position + 1,
                    QStringLiteral(")")
                        + text.mid(
                            delimiterStart,
                            position - delimiterStart
                        )
                        + QLatin1Char('"')
                };
            }

            const auto code = character.unicode();

            if (code <= 0x20 || code >= 0x7f
                || QStringLiteral(")\\$@`").contains(character)) {
                return {};
            }
        }

        return {};
    }

    return {};
}

int findQuotePosition(const QString& text, int start) {
    const auto tail = QStringView(text).mid(start);
    int prefixLength = 0;

    if (tail.startsWith(QStringLiteral("u8"))) {
        prefixLength = 2;
    } else if (tail.startsWith(QLatin1Char('u'))
               || tail.startsWith(QLatin1Char('U'))
               || tail.startsWith(QLatin1Char('L'))) {
        prefixLength = 1;
    }

    const int quotePosition = start + prefixLength;

    if (quotePosition >= text.size()) {
        return -1;
    }

    const QChar quote = text.at(quotePosition);

    if (quote == QLatin1Char('"')
        || quote == QLatin1Char('\'')) {
        return quotePosition;
    }

    return -1;
}

} // namespace

CppSyntaxHighlighter::CppSyntaxHighlighter(
    QTextDocument* document,
    const QPalette& palette
)
    : QSyntaxHighlighter(document) {
    setPalette(palette);
}

void CppSyntaxHighlighter::setPalette(const QPalette& palette) {
    const auto colors = syntaxColors(palette);

    if (keywordFormat_.foreground().color() == colors.keyword
        && numberFormat_.foreground().color() == colors.number
        && stringFormat_.foreground().color() == colors.stringLiteral
        && commentFormat_.foreground().color() == colors.comment
        && preprocessorFormat_.foreground().color() == colors.preprocessor) {
        return;
    }

    keywordFormat_.setForeground(colors.keyword);
    numberFormat_.setForeground(colors.number);
    stringFormat_.setForeground(colors.stringLiteral);
    commentFormat_.setForeground(colors.comment);
    preprocessorFormat_.setForeground(colors.preprocessor);

    rehighlight();
}

void CppSyntaxHighlighter::setSourceLineStarts(const std::vector<int>& starts) {
    sourceLineStarts_ = starts;
}

void CppSyntaxHighlighter::highlightBlock(const QString& text) {
    const int length = static_cast<int>(text.size());
    const auto block = currentBlock();
    const bool hasNextBlock = block.next().isValid();
    const bool sourceNewline = sourceLineStarts_.empty()
        ? hasNextBlock
        : std::binary_search(sourceLineStarts_.begin(), sourceLineStarts_.end(),
                             block.position() + length + 1);
    const bool spliced = sourceNewline && hasNextBlock
        && text.endsWith(QLatin1Char('\\'));
    const bool continuesLine = hasNextBlock && (!sourceNewline || spliced);
    // The final backslash participates in phase-2 line splicing even if it is
    // preceded by another backslash. Raw-string contents use the full length.
    const int limit = length - (spliced ? 1 : 0);
    const auto previous = decodeContext(previousBlockState());
    auto context = previous;
    context.mode = NormalState;

    auto saveState = [&] {
        if (sourceNewline && !spliced && context.mode < FirstRawStringState) {
            context.lineStart = true;
            context.directive = false;
            context.headerExpected = false;
            context.directiveNameExpected = false;
        }
        setCurrentBlockState(encodeContext(context));
    };

    if (context.directive) setFormat(0, length, preprocessorFormat_);

    auto consumeComment = [&](int start, int searchFrom, bool pendingStar = false) {
        int next = length;
        bool closed = false;
        if (pendingStar && limit > 0 && text.at(0) == QLatin1Char('/')) {
            next = 1;
            closed = true;
        } else {
            const auto end = text.indexOf(QStringLiteral("*/"), searchFrom);
            if (end >= 0 && end + 2 <= limit) {
                next = static_cast<int>(end + 2);
                closed = true;
            }
        }
        setFormat(start, next - start, commentFormat_);
        context.mode = closed ? NormalState : BlockCommentState;
        if (!closed && spliced
            && ((limit > 0 && text.at(limit - 1) == QLatin1Char('*'))
                || (limit == 0 && pendingStar))) {
            context.mode = BlockCommentStarState;
        }
        return next;
    };

    auto consumeRawString = [&](int start, int searchFrom, const QString& ending) {
        const auto end = text.indexOf(ending, searchFrom);
        const int next = end < 0 ? length : static_cast<int>(end + ending.size());
        setFormat(start, next - start, stringFormat_);
        context.lineStart = false;
        if (end < 0) {
            auto index = rawStringEndings_.indexOf(ending);
            if (index < 0) {
                index = rawStringEndings_.size();
                rawStringEndings_.append(ending);
            }
            context.mode = FirstRawStringState + static_cast<int>(index);
        } else {
            context.mode = NormalState;
        }
        return next;
    };

    auto consumeQuoted = [&](int start, int searchFrom, QChar quote, bool escaped) {
        int next = searchFrom;
        bool closed = false;
        while (next < limit) {
            const auto character = text.at(next++);
            if (escaped) {
                escaped = false;
            } else if (character == QLatin1Char('\\')) {
                escaped = true;
            } else if (character == quote) {
                closed = true;
                break;
            }
        }
        if (!closed) next = length;
        setFormat(start, next - start, stringFormat_);
        context.lineStart = false;
        context.mode = NormalState;
        if (!closed && continuesLine) {
            // A Unicode paragraph separator is an actual source character,
            // so it consumes a preceding escape; a spliced LF is removed.
            const bool pendingEscape = spliced && escaped;
            if (quote == QLatin1Char('"')) {
                context.mode = pendingEscape ? StringEscapeState : StringState;
            } else {
                context.mode = pendingEscape ? CharacterEscapeState : CharacterState;
            }
        }
        return next;
    };

    auto consumeHeader = [&](int start, int searchFrom) {
        const auto end = text.indexOf(QLatin1Char('>'), searchFrom);
        const bool closed = end >= 0 && end < limit;
        const int next = closed ? static_cast<int>(end + 1) : length;
        setFormat(start, next - start, stringFormat_);
        context.lineStart = false;
        context.headerExpected = false;
        context.mode = !closed && continuesLine ? HeaderState : NormalState;
        return next;
    };

    auto consumeLineComment = [&](int start) {
        setFormat(start, length - start, commentFormat_);
        context.mode = continuesLine ? LineCommentState : NormalState;
    };

    int position = 0;
    if (previous.mode == LineCommentState) {
        consumeLineComment(0);
        saveState();
        return;
    }
    if (previous.mode == BlockCommentState || previous.mode == BlockCommentStarState) {
        position = consumeComment(0, 0, previous.mode == BlockCommentStarState);
    } else if (previous.mode >= FirstRawStringState) {
        const int index = previous.mode - FirstRawStringState;
        if (index < rawStringEndings_.size())
            position = consumeRawString(0, 0, rawStringEndings_.at(index));
    } else if (previous.mode == StringState || previous.mode == StringEscapeState) {
        position = consumeQuoted(0, 0, QLatin1Char('"'), previous.mode == StringEscapeState);
    } else if (previous.mode == CharacterState || previous.mode == CharacterEscapeState) {
        position = consumeQuoted(0, 0, QLatin1Char('\''), previous.mode == CharacterEscapeState);
    } else if (previous.mode == HeaderState) {
        position = consumeHeader(0, 0);
    }

    while (position < limit) {
        const QChar current = text.at(position);
        if (current.isSpace()) {
            ++position;
            continue;
        }
        if (context.headerExpected && current == QLatin1Char('<')) {
            position = consumeHeader(position, position + 1);
            continue;
        }
        if (current == QLatin1Char('/') && position + 1 < limit) {
            const auto next = text.at(position + 1);
            if (next == QLatin1Char('/')) {
                consumeLineComment(position);
                break;
            }
            if (next == QLatin1Char('*')) {
                position = consumeComment(position, position + 2);
                continue;
            }
        }
        if (context.lineStart
            && (current == QLatin1Char('#')
                || (current == QLatin1Char('%') && position + 1 < limit
                    && text.at(position + 1) == QLatin1Char(':')))) {
            context.directive = true;
            context.directiveNameExpected = true;
            context.headerExpected = false;
            context.lineStart = false;
            setFormat(position, length - position, preprocessorFormat_);
            position += current == QLatin1Char('#') ? 1 : 2;
            continue;
        }

        const auto raw = findRawStringOpening(text, position);
        if (raw.contentStart >= 0) {
            context.headerExpected = false;
            context.directiveNameExpected = false;
            position = consumeRawString(position, raw.contentStart, raw.ending);
            continue;
        }
        if (isAsciiDigit(current)
            || (current == QLatin1Char('.') && position + 1 < limit
                && isAsciiDigit(text.at(position + 1)))) {
            const int start = position;
            position = numberEnd(text, start);
            setFormat(start, position - start, numberFormat_);
            context.lineStart = false;
            context.headerExpected = false;
            context.directiveNameExpected = false;
            continue;
        }
        const int quotePosition = findQuotePosition(text, position);
        if (quotePosition >= 0) {
            context.headerExpected = false;
            context.directiveNameExpected = false;
            position = consumeQuoted(position, quotePosition + 1, text.at(quotePosition), false);
            continue;
        }
        if (isIdentifierStart(current)) {
            const int start = position++;
            while (position < limit && isIdentifierPart(text.at(position))) ++position;
            const auto word = text.mid(start, position - start);
            if (context.directiveNameExpected) {
                setFormat(start, position - start, preprocessorFormat_);
                context.directiveNameExpected = false;
                context.headerExpected = word == QStringLiteral("include")
                    || word == QStringLiteral("include_next");
            } else {
                if (isKeyword(word)) setFormat(start, position - start, keywordFormat_);
                context.headerExpected = false;
            }
            context.lineStart = false;
            continue;
        }
        context.lineStart = false;
        context.headerExpected = false;
        context.directiveNameExpected = false;
        ++position;
    }
    saveState();
}

} // namespace xray::ui
