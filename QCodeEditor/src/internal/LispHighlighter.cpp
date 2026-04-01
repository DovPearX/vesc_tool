// QCodeEditor
#include <LispHighlighter>
#include <QLispSymbolDB>
#include <QSyntaxStyle>
#include <QLanguage>

// Qt
#include <QFile>
#include <QSet>

namespace {

QString symbolPattern(const QStringList &symbols)
{
    QStringList escaped;
    escaped.reserve(symbols.size());
    for (const QString &symbol : symbols) {
        escaped << QRegularExpression::escape(symbol);
    }

    return QStringLiteral("(?<![A-Za-z0-9_\\-+*/<>=!?@])(?:%1)(?![A-Za-z0-9_\\-+*/<>=!?@])")
        .arg(escaped.join(QStringLiteral("|")));
}

}

LispHighlighter::LispHighlighter(QTextDocument* document) :
    QStyleSyntaxHighlighter(document),
    m_highlightRules(),
    m_blockRules(),
    m_functionPattern(QStringLiteral("\\(([^\\s()]+)")),
    m_definitionPattern(QStringLiteral("\\((?:def|define|defun|defmacro)\\s+([^\\s()]+)"))
{
    QSet<QString> keywords;
    QSet<QString> functions;

    Q_INIT_RESOURCE(qcodeeditor_resources);
    QFile fl(":/languages/lisp.xml");

    if (fl.open(QIODevice::ReadOnly)) {
        QLanguage language(&fl);
        if (language.isLoaded()) {
            for (const QString &name : language.names(QStringLiteral("Keyword"))) {
                keywords.insert(name);
            }
            for (const QString &name : language.names(QStringLiteral("Type"))) {
                functions.insert(name);
            }
            for (const QString &name : language.names(QStringLiteral("Label"))) {
                keywords.insert(name);
            }
            for (const QString &name : language.names(QStringLiteral("Global"))) {
                keywords.insert(name);
            }
        }
        fl.close();
    }

    for (const LispSymbol &sym : QLispSymbolDB::all()) {
        if (sym.name == QStringLiteral("if")
            || sym.name == QStringLiteral("cond")
            || sym.name == QStringLiteral("case")
            || sym.name == QStringLiteral("lambda")
            || sym.name == QStringLiteral("define")
            || sym.name == QStringLiteral("def")
            || sym.name == QStringLiteral("defun")
            || sym.name == QStringLiteral("defmacro")
            || sym.name == QStringLiteral("let")
            || sym.name == QStringLiteral("let*")
            || sym.name == QStringLiteral("setq")
            || sym.name == QStringLiteral("progn")
            || sym.name == QStringLiteral("quote")
            || sym.name == QStringLiteral("quasiquote")
            || sym.name == QStringLiteral("unquote")
            || sym.name == QStringLiteral("unquote-splicing")
            || sym.name == QStringLiteral("loop")
            || sym.name == QStringLiteral("foreach")
            || sym.name == QStringLiteral("while")
            || sym.name == QStringLiteral("and")
            || sym.name == QStringLiteral("or")
            || sym.name == QStringLiteral("not")
            || sym.name == QStringLiteral("match")
            || sym.name == QStringLiteral("trap")
            || sym.name == QStringLiteral("spawn")
            || sym.name == QStringLiteral("yield")
            || sym.name == QStringLiteral("self")
            || sym.name == QStringLiteral("receive")
            || sym.name == QStringLiteral("send")
            || sym.name == QStringLiteral("call")) {
            keywords.insert(sym.name);
        } else {
            functions.insert(sym.name);
        }
    }

    m_highlightRules.append({QRegularExpression(QStringLiteral(";[^\\n]*")), QStringLiteral("Comment")});
    m_highlightRules.append({QRegularExpression(QStringLiteral("\"([^\"\\\\]|\\\\.)*\"")), QStringLiteral("String")});
    m_highlightRules.append({QRegularExpression(QStringLiteral("'[^\\s()]+")), QStringLiteral("Preprocessor")});
    m_highlightRules.append({QRegularExpression(QStringLiteral("\\b(?:true|false|nil|t)\\b")), QStringLiteral("Type")});

    // Numbers
    m_highlightRules.append({
        QRegularExpression(R"((?<=\b|\s|^)(?i)(?:(?:(?:(?:(?:\d+(?:'\d+)*)?\.(?:\d+(?:'\d+)*)(?:e[+-]?(?:\d+(?:'\d+)*))?)|(?:(?:\d+(?:'\d+)*)\.(?:e[+-]?(?:\d+(?:'\d+)*))?)|(?:(?:\d+(?:'\d+)*)(?:e[+-]?(?:\d+(?:'\d+)*)))|(?:0x(?:[0-9a-f]+(?:'[0-9a-f]+)*)?\.(?:[0-9a-f]+(?:'[0-9a-f]+)*)(?:p[+-]?(?:\d+(?:'\d+)*)))|(?:0x(?:[0-9a-f]+(?:'[0-9a-f]+)*)\.?(?:p[+-]?(?:\d+(?:'\d+)*))))[lf]?)|(?:(?:(?:[1-9]\d*(?:'\d+)*)|(?:0[0-7]*(?:'[0-7]+)*)|(?:0x[0-9a-f]+(?:'[0-9a-f]+)*)|(?:0b[01]+(?:'[01]+)*))(?:u?l{0,2}|l{0,2}u?)))(?=\b|\s|$))"),
        "Number"
    });

    // Strings
    m_highlightRules.append({
        QRegularExpression(R"("[^\n"]*")"),
        "String"
    });

    // Single line
    m_highlightRules.append({
        QRegularExpression(R"(;[^\n]*)"),
        "Comment"
    });

    const QStringList keywordList = keywords.values();
    if (!keywordList.isEmpty()) {
        m_highlightRules.append({QRegularExpression(symbolPattern(keywordList)), QStringLiteral("Keyword")});
    }

    const QStringList functionList = functions.values();
    if (!functionList.isEmpty()) {
        m_highlightRules.append({QRegularExpression(symbolPattern(functionList)), QStringLiteral("Function")});
    }

    m_blockRules.append({
        QRegularExpression(QStringLiteral("#\\|")),
        QRegularExpression(QStringLiteral("\\|#")),
        QStringLiteral("Comment")
    });
}

void LispHighlighter::highlightBlock(const QString& text)
{
    for (auto&& rule : m_highlightRules) {
        auto matchIterator = rule.pattern.globalMatch(text);

        while (matchIterator.hasNext()) {
            auto match = matchIterator.next();

            setFormat(
                match.capturedStart(),
                match.capturedLength(),
                syntaxStyle()->getFormat(rule.formatName)
            );
        }
    }

    {
        auto matchIterator = m_functionPattern.globalMatch(text);

        while (matchIterator.hasNext()) {
            auto match = matchIterator.next();

            setFormat(
                match.capturedStart(1),
                match.capturedLength(1),
                syntaxStyle()->getFormat(QStringLiteral("Function"))
            );
        }
    }

    {
        auto matchIterator = m_definitionPattern.globalMatch(text);

        while (matchIterator.hasNext()) {
            auto match = matchIterator.next();

            setFormat(
                match.capturedStart(1),
                match.capturedLength(1),
                syntaxStyle()->getFormat(QStringLiteral("Type"))
            );
        }
    }

    setCurrentBlockState(0);
    int startIndex = 0;
    int highlightRuleId = previousBlockState();

    if (highlightRuleId < 1 || highlightRuleId > m_blockRules.size()) {
        for (int i = 0; i < m_blockRules.size(); ++i) {
            startIndex = text.indexOf(m_blockRules.at(i).startPattern);
            if (startIndex >= 0) {
                highlightRuleId = i + 1;
                break;
            }
        }
    }

    while (startIndex >= 0) {
        const auto &rule = m_blockRules.at(highlightRuleId - 1);
        const auto match = rule.endPattern.match(text, startIndex);

        int endIndex = match.capturedStart();
        int matchLength = 0;

        if (endIndex == -1) {
            setCurrentBlockState(highlightRuleId);
            matchLength = text.length() - startIndex;
        } else {
            matchLength = endIndex - startIndex + match.capturedLength();
        }

        setFormat(startIndex, matchLength, syntaxStyle()->getFormat(rule.formatName));
        startIndex = text.indexOf(rule.startPattern, startIndex + matchLength);
    }
}
