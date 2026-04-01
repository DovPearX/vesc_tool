#pragma once

#include <QFrame>
#include <QLabel>

class QTextEdit;

class QLispSignatureHelp : public QFrame
{
    Q_OBJECT

public:
    explicit QLispSignatureHelp(QTextEdit *editor);

    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void updateDisplay();

private:
    void repositionPopup();
    static QPair<QString, int> detectContext(const QString &textUpToCursor);

    QTextEdit *m_editor;
    QLabel *m_label;
};
