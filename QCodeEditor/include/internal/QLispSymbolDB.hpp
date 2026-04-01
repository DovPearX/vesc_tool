#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

struct LispSymbol {
    QString name;
    QStringList params;
    QString doc;
};

class QLispSymbolDB {
public:
    static const QVector<LispSymbol> &all();
    static const LispSymbol *find(const QString &name);
};
