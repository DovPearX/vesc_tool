#pragma once

// QCodeEditor
#include <QStyleSyntaxHighlighter> // Required for inheritance
#include <QHighlightBlockRule>
#include <QHighlightRule>

// Qt
#include <QRegularExpression>
#include <QVector>

class QSyntaxStyle;

/**
 * @brief Class, that describes C++ code
 * highlighter.
 */
class LispHighlighter : public QStyleSyntaxHighlighter
{
    Q_OBJECT
public:

    /**
     * @brief Constructor.
     * @param document Pointer to document.
     */
    explicit LispHighlighter(QTextDocument* document=nullptr);

protected:
    void highlightBlock(const QString& text) override;

private:
    QVector<QHighlightRule> m_highlightRules;
    QVector<QHighlightBlockRule> m_blockRules;
    QRegularExpression m_functionPattern;
    QRegularExpression m_definitionPattern;

};

