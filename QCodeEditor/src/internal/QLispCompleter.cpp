/*
    Copyright 2021 Benjamin Vedder	benjamin@vedder.se

    This file is part of VESC Tool.

    VESC Tool is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    VESC Tool is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
    */

#include <QLispCompleter>
#include <QLispSymbolDB>
#include <QLanguage>
#include <QStringListModel>
#include <QFile>
#include <QSet>

QLispCompleter::QLispCompleter(QObject *parent) :
    QCompleter(parent)
{
    QSet<QString> unique;

    for (const LispSymbol &sym : QLispSymbolDB::all()) {
        unique.insert(sym.name);
    }

    Q_INIT_RESOURCE(qcodeeditor_resources);
    QFile fl(":/languages/lisp.xml");

    if (fl.open(QIODevice::ReadOnly)) {
        QLanguage language(&fl);

        if (language.isLoaded()) {
            auto keys = language.keys();
            foreach (auto&& key, keys) {
                for (auto name: language.names(key)) {
                    unique.insert(name);
                }
            }
        }

        fl.close();
    }

    QStringList symbols = unique.values();
    symbols.sort(Qt::CaseInsensitive);

    setModel(new QStringListModel(symbols, this));
    setCompletionMode(QCompleter::PopupCompletion);
    setModelSorting(QCompleter::CaseInsensitivelySortedModel);
    setCaseSensitivity(Qt::CaseInsensitive);
    setWrapAround(true);
}
