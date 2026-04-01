#include <QLispSignatureHelp>
#include <QLispSymbolDB>

#include <QEvent>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QHelpEvent>
#include <QKeyEvent>
#include <QTextCursor>
#include <QTextEdit>
#include <QToolTip>

QLispSignatureHelp::QLispSignatureHelp(QTextEdit *editor)
    : QFrame(editor)
    , m_editor(editor)
    , m_label(new QLabel(this))
{
    setObjectName(QStringLiteral("lbmSigHelp"));
    setStyleSheet(QStringLiteral(
        "QFrame#lbmSigHelp {"
        "  background-color: #1a2433;"
        "  border: 1px solid #3a6080;"
        "  border-radius: 4px;"
        "}"));

    QFont mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    mono.setPointSize(9);
    m_label->setFont(mono);
    m_label->setTextFormat(Qt::RichText);
    m_label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_label->setStyleSheet(QStringLiteral("background: transparent;"));

    auto *lay = new QHBoxLayout(this);
    lay->setContentsMargins(7, 3, 7, 3);
    lay->addWidget(m_label);

    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    hide();

    connect(editor, &QTextEdit::cursorPositionChanged,
            this, &QLispSignatureHelp::updateDisplay);

    editor->viewport()->installEventFilter(this);
    editor->installEventFilter(this);
}

bool QLispSignatureHelp::eventFilter(QObject *watched, QEvent *event)
{
    if (!m_editor) {
        return QFrame::eventFilter(watched, event);
    }

    if (watched == m_editor && event->type() == QEvent::FocusOut) {
        hide();
        return false;
    }

    if (watched == m_editor && event->type() == QEvent::KeyPress) {
        const auto *ke = static_cast<QKeyEvent *>(event);
        if (ke->key() == Qt::Key_Escape) {
            hide();
        }
        return false;
    }

    if (watched == m_editor->viewport() && event->type() == QEvent::ToolTip) {
        const auto *he = static_cast<QHelpEvent *>(event);

        QTextCursor cursor = m_editor->cursorForPosition(he->pos());
        cursor.select(QTextCursor::WordUnderCursor);
        const QString word = cursor.selectedText().trimmed();

        if (!word.isEmpty()) {
            const LispSymbol *sym = QLispSymbolDB::find(word);
            if (sym) {
                const QString sig = sym->params.isEmpty()
                    ? sym->name.toHtmlEscaped()
                    : sym->name.toHtmlEscaped()
                        + QLatin1Char(' ')
                        + sym->params.join(QLatin1Char(' ')).toHtmlEscaped();

                const QString tip = QStringLiteral(
                    "<b style='color:#7df0ff;'>%1</b><br/>"
                    "<i style='color:#99bbcc;'>%2</i>")
                    .arg(sig, sym->doc.toHtmlEscaped());

                QToolTip::showText(he->globalPos(), tip, m_editor);
                return true;
            }
        }

        QToolTip::hideText();
        return false;
    }

    return QFrame::eventFilter(watched, event);
}

QPair<QString, int> QLispSignatureHelp::detectContext(const QString &textUpToCursor)
{
    int depth = 0;
    int openAt = -1;

    for (int i = textUpToCursor.length() - 1; i >= 0; --i) {
        const QChar c = textUpToCursor.at(i);
        if (c == QLatin1Char(')')) {
            ++depth;
        } else if (c == QLatin1Char('(')) {
            if (depth > 0) {
                --depth;
            } else {
                openAt = i;
                break;
            }
        }
    }

    if (openAt < 0) {
        return {};
    }

    const QString inner = textUpToCursor.mid(openAt + 1);

    int i = 0;
    while (i < inner.length() && inner.at(i).isSpace()) {
        ++i;
    }
    if (i >= inner.length()) {
        return {};
    }

    const int nameStart = i;
    while (i < inner.length() && !inner.at(i).isSpace()) {
        ++i;
    }
    const QString funcName = inner.mid(nameStart, i - nameStart);
    if (funcName.isEmpty()) {
        return {};
    }

    int argIndex = 0;
    int d = 0;
    bool inToken = false;

    for (; i < inner.length(); ++i) {
        const QChar c = inner.at(i);
        if (c == QLatin1Char('(') || c == QLatin1Char('[') || c == QLatin1Char('{')) {
            ++d;
            inToken = true;
        } else if (c == QLatin1Char(')') || c == QLatin1Char(']') || c == QLatin1Char('}')) {
            if (d > 0) {
                --d;
            }
            inToken = true;
        } else if (d == 0) {
            if (c.isSpace()) {
                if (inToken) {
                    ++argIndex;
                    inToken = false;
                }
            } else {
                inToken = true;
            }
        }
    }

    return {funcName, argIndex};
}

void QLispSignatureHelp::updateDisplay()
{
    if (!m_editor) {
        return;
    }

    const QTextCursor cursor = m_editor->textCursor();
    const QString textUpTo = m_editor->toPlainText().left(cursor.position());

    const auto context = detectContext(textUpTo);
    const QString funcName = context.first;
    const int argIndex = context.second;

    if (funcName.isEmpty()) {
        hide();
        return;
    }

    const LispSymbol *sym = QLispSymbolDB::find(funcName);
    if (!sym) {
        hide();
        return;
    }

    int highlightIdx = argIndex;
    if (!sym->params.isEmpty()
        && argIndex >= sym->params.size()
        && sym->params.last() == QStringLiteral("...")) {
        highlightIdx = sym->params.size() - 1;
    }

    QString html;
    html += QStringLiteral("<span style='color:#7a8fa8;font-weight:bold;'>%1</span>")
            .arg(sym->name.toHtmlEscaped());

    for (int k = 0; k < sym->params.size(); ++k) {
        html += QLatin1Char(' ');
        const QString p = sym->params.at(k).toHtmlEscaped();
        if (k == highlightIdx) {
            html += QStringLiteral("<b style='color:#7df0ff;'>%1</b>").arg(p);
        } else if (k < highlightIdx) {
            html += QStringLiteral("<span style='color:#3a5a78;'>%1</span>").arg(p);
        } else {
            html += QStringLiteral("<span style='color:#4e6e88;'>%1</span>").arg(p);
        }
    }

    html += QStringLiteral("&nbsp;&nbsp;<i style='color:#506070;'>%1</i>")
            .arg(sym->doc.toHtmlEscaped());

    m_label->setText(html);
    adjustSize();
    repositionPopup();
    show();
    raise();
}

void QLispSignatureHelp::repositionPopup()
{
    if (!m_editor) {
        return;
    }

    const QRect cursorRect = m_editor->cursorRect();
    const QPoint viewPt = cursorRect.bottomLeft() + QPoint(0, 4);
    const QPoint editorPt = m_editor->viewport()->mapTo(m_editor, viewPt);

    int x = editorPt.x();
    int y = editorPt.y();

    const int w = qMax(width(), 280);
    if (x + w > m_editor->width() - 4) {
        x = qMax(4, m_editor->width() - w - 4);
    }

    if (y + height() > m_editor->height() - 4) {
        const QPoint abovePt = m_editor->viewport()->mapTo(
            m_editor, cursorRect.topLeft() + QPoint(0, -height() - 4));
        y = abovePt.y();
    }

    move(x, y);
}
