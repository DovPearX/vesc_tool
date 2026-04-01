/*
    Copyright 2022 Benjamin Vedder	benjamin@vedder.se

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

#include <QDateTime>
#include <QMessageBox>
#include <QProgressDialog>
#include <QGroupBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QSpinBox>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include "pagelisp.h"
#include "ui_pagelisp.h"
#include "utility.h"
#include "widgets/helpdialog.h"
#include "QLispBM.h"

extern "C" {
#include "extensions.h"
#include "print.h"
#include "eval_cps.h"
#include "extensions/display_extensions.h"
}

static bool display_render_callback(image_buffer_t *img, uint16_t x, uint16_t y, color_t *colors) {
    DisplayCanvas *canvas = DisplayCanvas::instance();
    if (!canvas || !img || !img->data) {
        return false;
    }
    
    DisplayCanvas::ColorFormat fmt;
    switch (img->fmt) {
        case indexed2: fmt = DisplayCanvas::Indexed2; break;
        case indexed4: fmt = DisplayCanvas::Indexed4; break;
        case indexed16: fmt = DisplayCanvas::Indexed16; break;
        case rgb332: fmt = DisplayCanvas::RGB332; break;
        case rgb565: fmt = DisplayCanvas::RGB565; break;
        case rgb888: fmt = DisplayCanvas::RGB888; break;
        default: return false;
    }
    
    canvas->setImageData(img->data, img->width, img->height, fmt);
    return true;
}

static void display_clear_callback(uint32_t color) {
    DisplayCanvas *canvas = DisplayCanvas::instance();
    if (canvas) {
        canvas->clearScreen();
    }
}

static void display_reset_callback(void) {
    DisplayCanvas *canvas = DisplayCanvas::instance();
    if (canvas) {
        canvas->clearScreen();
    }
}

static int ceil_div(int value, int divisor) {
    return (value + divisor - 1) / divisor;
}

PageLisp::PageLisp(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::PageLisp)
{
    ui->setupUi(this);
    mVesc = nullptr;
    mLocalEval = nullptr;
    mDisplayTab = nullptr;
    mDisplayCanvas = nullptr;

    QSettings settings;
    mDisplayWidth = settings.value("pagelisp/local_display_width", mDisplayWidth).toInt();
    mDisplayHeight = settings.value("pagelisp/local_display_height", mDisplayHeight).toInt();
    mHeapCells = settings.value("pagelisp/local_heap_cells", mHeapCells).toInt();
    mMemoryBlocks = settings.value("pagelisp/local_memory_blocks", mMemoryBlocks).toInt();

    Utility::setPlotColors(ui->bindingPlot);

    QPushButton *plusButton = new QPushButton();
    plusButton->setIcon(Utility::getIcon("icons/Plus Math-96.png"));
    ui->runButton->setIcon(Utility::getIcon("icons/Circled Play-96.png"));
    ui->stopButton->setIcon(Utility::getIcon("icons/Shutdown-96.png"));
    ui->infoButton->setIcon(Utility::getIcon("icons/Help-96.png"));
    ui->helpButton->setIcon(Utility::getIcon("icons/Help-96.png"));
    ui->clearConsoleButton->setIcon(Utility::getIcon("icons/Delete-96.png"));
    ui->openRecentButton->setIcon(Utility::getIcon("icons/Open Folder-96.png"));
    ui->removeSelectedButton->setIcon(Utility::getIcon("icons/Delete-96.png"));
    ui->clearRecentButton->setIcon(Utility::getIcon("icons/Delete-96.png"));
    ui->openExampleButton->setIcon(Utility::getIcon("icons/Open Folder-96.png"));
    ui->uploadButton->setIcon(Utility::getIcon("icons/Download-96.png"));
    ui->readExistingButton->setIcon(Utility::getIcon("icons/Upload-96.png"));
    ui->eraseButton->setIcon(Utility::getIcon("icons/Delete-96.png"));
    ui->replHelpButton->setIcon(Utility::getIcon("icons/Help-96.png"));
    ui->streamButton->setIcon(Utility::getIcon("icons/Download-96.png"));
    ui->recentFilterClearButton->setIcon(Utility::getIcon("icons/Cancel-96.png"));
    ui->exampleFilterClearButton->setIcon(Utility::getIcon("icons/Cancel-96.png"));

    QIcon mycon = Utility::getIcon( "icons/expand_off.png");
    mycon.addPixmap(Utility::getIcon("icons/expand_on.png"), QIcon::Normal, QIcon::On);
    mycon.addPixmap(Utility::getIcon("icons/expand_off.png"), QIcon::Normal, QIcon::Off);
    ui->zoomHButton->setIcon(mycon);

    mycon = Utility::getIcon( "icons/expand_v_off.png");
    mycon.addPixmap(Utility::getIcon("icons/expand_v_on.png"), QIcon::Normal, QIcon::On);
    mycon.addPixmap(Utility::getIcon("icons/expand_v_off.png"), QIcon::Normal, QIcon::Off);
    ui->zoomVButton->setIcon(mycon);

    mycon = Utility::getIcon( "icons/size_off.png");
    mycon.addPixmap(Utility::getIcon("icons/size_on.png"), QIcon::Normal, QIcon::On);
    mycon.addPixmap(Utility::getIcon("icons/size_off.png"), QIcon::Normal, QIcon::Off);
    ui->autoscaleButton->setIcon(mycon);

    plusButton->setFlat(true);
    plusButton->setText("New Tab");
    ui->fileTabs->setCornerWidget(plusButton);
    connect(plusButton, &QPushButton::clicked, [this]() {
        createEditorTab("", "");
    });

    QSettings set;
    {
        int size = set.beginReadArray("pagelisp/recentfiles");
        for (int i = 0; i < size; ++i) {
            set.setArrayIndex(i);
            QString path = set.value("path").toString();
            QFileInfo f(path);
            if (f.exists()) {
                mRecentFiles.append(path);
            }
        }
        set.endArray();
    }
    {
        int size = set.beginReadArray("pagelisp/recentopenfiles");
        for (int i = 0; i < size; ++i) {
            set.setArrayIndex(i);
            QString path = set.value("path").toString();
            QFileInfo f(path);
            if (f.exists()) {
                QFile file(path);
                if (file.open(QIODevice::ReadOnly)) {
                    createEditorTab(path, file.readAll());
                }
            }
        }
        set.endArray();
    }

    if (ui->fileTabs->count() == 0) {
        createEditorTab("", "");
    }

    ui->fileTabs->setCurrentIndex(0);
    updateRecentList();

    // Load examples
    foreach (auto &fi, QDir("://res/LispBM/Examples/").entryInfoList(QDir::NoFilter, QDir::Name)) {
        QListWidgetItem *item = new QListWidgetItem;
        item->setText(fi.fileName());
        item->setData(Qt::UserRole, fi.filePath());
        ui->exampleList->addItem(item);
    }

    ui->splitter->setSizes(QList<int>({1000, 100}));
    ui->splitter_2->setSizes(QList<int>({1000, 600}));

    mPollTimer.start(1000 / ui->pollRateBox->value());
    connect(&mPollTimer, &QTimer::timeout, [this]() {
        if (!ui->pollOffButton->isChecked()) {
            if (mVesc) {
                mVesc->commands()->lispGetStats(ui->pollAllButton->isChecked());
            }
        }
    });

    connect(ui->pollRateBox, QOverload<int>::of(&QSpinBox::valueChanged), [this](int val) {
        mPollTimer.setInterval(1000 / val);
    });

    auto updateZoom = [this]() {
        Qt::Orientations plotOrientations = Qt::Orientations(
                    ((ui->zoomHButton->isChecked() ? Qt::Horizontal : 0) |
                     (ui->zoomVButton->isChecked() ? Qt::Vertical : 0)));

        ui->bindingPlot->axisRect()->setRangeZoom(plotOrientations);
    };

    connect(ui->zoomHButton, &QPushButton::clicked, updateZoom);
    connect(ui->zoomVButton, &QPushButton::clicked, updateZoom);
    updateZoom();

    QFont legendFont = font();
    legendFont.setPointSize(9);
    ui->bindingPlot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
    ui->bindingPlot->legend->setVisible(true);
    ui->bindingPlot->legend->setFont(legendFont);
    ui->bindingPlot->axisRect()->insetLayout()->setInsetAlignment(0, Qt::AlignRight|Qt::AlignBottom);
    ui->bindingPlot->xAxis->setLabel("Age (s)");
    ui->bindingPlot->yAxis->setLabel("Value");
    ui->bindingPlot->xAxis->setRangeReversed(true);
    createDisplayTab();
    
    createSettingsTab();
}

PageLisp::~PageLisp()
{
    saveStateToSettings();
    delete ui;
}

void PageLisp::saveStateToSettings()
{
    QSettings set;
    set.setValue("pagelisp/local_display_width", mDisplayWidth);
    set.setValue("pagelisp/local_display_height", mDisplayHeight);
    set.setValue("pagelisp/local_heap_cells", mHeapCells);
    set.setValue("pagelisp/local_memory_blocks", mMemoryBlocks);

    {
        set.remove("pagelisp/recentfiles");
        set.beginWriteArray("pagelisp/recentfiles");
        int ind = 0;
        foreach (auto f, mRecentFiles) {
            set.setArrayIndex(ind);
            set.setValue("path", f);
            ind++;
        }
        set.endArray();
    }
    {
        set.remove("pagelisp/recentopenfiles");
        set.beginWriteArray("pagelisp/recentopenfiles");
        for (int i = 0;i < ui->fileTabs->count();i++) {
            auto e = qobject_cast<ScriptEditor*>(ui->fileTabs->widget(i));
            set.setArrayIndex(i);
            set.setValue("path", e->fileNow());
        }
        set.endArray();
    }
}

VescInterface *PageLisp::vesc() const
{
    return mVesc;
}

void PageLisp::setVesc(VescInterface *vesc)
{
    mVesc = vesc;
    mLoader.setVesc(vesc);
    ui->experimentPlot->setVesc(vesc);

    connect(mVesc->commands(), &Commands::lispPrintReceived, [this](QString str) {
        ui->debugEdit->doAtEndWithScroll([this, str](QTextCursor cursor) {
            cursor.insertText(str + "\n");
        });
        
        int maxLines = QSettings().value("scripting/replMaxLines", 5000).toInt();
        ui->debugEdit->trimKeepingLastLines(maxLines);        
    });

    connect(mVesc->commands(), &Commands::lispRunningResRx, [this](bool ok) {
        if (!ok) {
            mVesc->emitMessageDialog(tr("Start/Stop LispBM"),
                                     tr("Start/Stop LispBM failed. Did you forget to upload the code?"), false);
        }
    });

    connect(mVesc->commands(), &Commands::lispStatsRx, [this](LISP_STATS stats) {
        ui->cpuBar->setValue(stats.cpu_use * 10);
        ui->cpuBar->setFormat(QString("%1%").arg(stats.cpu_use, 0, 'f', 1));
        ui->heapBar->setValue(stats.heap_use);
        ui->memBar->setValue(stats.mem_use);

        ui->bindingTable->setColumnCount(2);
        ui->bindingTable->setRowCount(stats.number_bindings.size());

        QStringList selectedBindings;
        qint64 maxAgeMs = 5000;
        auto timeNow = QDateTime::currentMSecsSinceEpoch();

        int ind = 0;
        for (auto b: stats.number_bindings) {
            ui->bindingTable->setItem(ind, 0, new QTableWidgetItem(b.first));
            auto data = new QTableWidgetItem(QString::number(b.second, 'f'));
            data->setData(Qt::UserRole, b.second);
            ui->bindingTable->setItem(ind, 1, data);

            if (ui->bindingTable->item(ind, 1)->isSelected()) {
                selectedBindings.append(b.first);
                if (!mBindingData.contains(b.first)) {
                    mBindingData.insert(b.first, QVector<QPair<qint64, double>>());
                }

                mBindingData[b.first].append(qMakePair(timeNow, b.second));

                // Remove old data points
                while (!mBindingData[b.first].isEmpty() && mBindingData[b.first].at(0).first < (timeNow - maxAgeMs)) {
                    mBindingData[b.first].removeFirst();
                }
            }

            ind++;
        }

        // Remove bindings not selected
        for (auto it = mBindingData.begin(); it != mBindingData.end();) {
            if (!selectedBindings.contains(it.key())) {
                it = mBindingData.erase(it);
            } else {
                ++it;
            }
        }

        // Add graphs
        ui->bindingPlot->clearGraphs();
        int graphInd = 0;
        for (auto it = mBindingData.begin(); it != mBindingData.end();++it) {
            auto name = it.key();
            QVector<double> xAxis, yAxis;
            for (auto p: it.value()) {
                xAxis.append(double(timeNow - p.first) / 1000.0);
                yAxis.append(p.second);
            }

            ui->bindingPlot->addGraph();
            ui->bindingPlot->graph()->setPen(QPen(Utility::getAppQColor(QString("plot_graph%1").arg(graphInd + 1))));
            ui->bindingPlot->graph()->setName(name);
            ui->bindingPlot->graph()->setData(xAxis, yAxis);
            graphInd++;
        }

        if (ui->autoscaleButton->isChecked()) {
            ui->bindingPlot->rescaleAxes();
        }

        ui->bindingPlot->replotWhenVisible();
    });
}

void PageLisp::reloadParams()
{

}

bool PageLisp::hasUnsavedTabs()
{
    for (int i = 0; i < ui->fileTabs->count(); i++) {
        auto e = qobject_cast<ScriptEditor*>(ui->fileTabs->widget(i));
        if (e->hasUnsavedContent()) {
            return true;
        }
    }

    return false;
}

void PageLisp::disablePolling()
{
    ui->pollOffButton->setChecked(true);
}

bool PageLisp::openFileTab(QString fileName)
{
    QFileInfo f(fileName);
    if (f.exists()) {
        QFile file(fileName);
        if (file.open(QIODevice::ReadOnly)) {
            createEditorTab(fileName, file.readAll());

            mRecentFiles.removeAll(fileName);
            mRecentFiles.prepend(fileName);
            updateRecentList();
            ui->recentList->setCurrentRow(0);

            return true;
        }
    }

    return false;
}

void PageLisp::updateRecentList()
{
    ui->recentList->clear();
    foreach (auto f, mRecentFiles) {
        ui->recentList->addItem(f);
    }

    on_recentFilterEdit_textChanged(ui->recentFilterEdit->text());
}

void PageLisp::makeEditorConnections(ScriptEditor *editor)
{
    connect(editor->codeEditor(), &QCodeEditor::textChanged, [editor, this]() {
        setEditorDirty(editor);
    });
    connect(editor->codeEditor(), &QCodeEditor::clearConsoleTriggered, [this]() {
        ui->debugEdit->clear();
    });
    connect(editor, &ScriptEditor::fileOpened, [editor, this](QString fileName) {
        mRecentFiles.removeAll(fileName);
        mRecentFiles.prepend(fileName);
        updateRecentList();
        setEditorClean(editor);
    });
    connect(editor, &ScriptEditor::fileSaved, [editor, this](QString fileName) {
        if (mVesc) {
            mVesc->emitStatusMessage("Saved " + fileName, true);
        }

        mRecentFiles.removeAll(fileName);
        mRecentFiles.prepend(fileName);
        updateRecentList();

        setEditorClean(editor);
    });
    connect(editor->codeEditor(), &QCodeEditor::runEmbeddedTriggered, [this]() {
        on_uploadButton_clicked();
    });
    connect(editor->codeEditor(), &QCodeEditor::runWindowTriggered, [this]() {
        on_streamButton_clicked();
    });
    connect(editor->codeEditor(), &QCodeEditor::stopTriggered, [this]() {
        on_stopButton_clicked();
    });
    connect(editor->codeEditor(), &QCodeEditor::runBlockTriggered, [this](QString text) {
        if (mVesc && mVesc->isPortConnected()) {
            if (text.length() > 400) {
                CodeLoader loader;
                loader.setVesc(mVesc);
                loader.lispStreamString(text, 0);
            } else if (mVesc) {
                mVesc->commands()->lispSendReplCmd(text);
            }
        } else {
            evalLocalExpression(text);
        }
    });
}

void PageLisp::ensureLocalEvaluator()
{
    if (!mLocalEval) {
        mLocalEval = new QLispBM(this);

        connect(mLocalEval, &QLispBM::output, this, [this](const QString &text) {
            ui->debugEdit->doAtEndWithScroll([text](QTextCursor cursor) {
                cursor.insertText(text);
            });

            int maxLines = QSettings().value("scripting/replMaxLines", 5000).toInt();
            ui->debugEdit->trimKeepingLastLines(maxLines);
        });

        connect(mLocalEval, &QLispBM::evalFinished, this, [this](int cid, const QString &result) {
            Q_UNUSED(cid);
            ui->debugEdit->doAtEndWithScroll([result](QTextCursor cursor) {
                cursor.insertText(result + "\n");
            });
        });

        connect(mLocalEval, &QLispBM::evalFailed, this, [this](int cid, const QString &error) {
            Q_UNUSED(cid);
            ui->debugEdit->doAtEndWithScroll([error](QTextCursor cursor) {
                cursor.insertHtml(QString("<font color=\"red\">%1</font><br>").arg(error.toHtmlEscaped()));
            });
        });
    }

    if (!mLocalEval->isInitialized()) {
        QLispBMConfig config;
        config.extensions = QLispBMConfig::ExtAll;
        config.heapCells = qMax(mHeapCells, 16384);
        config.memoryBlocks = qMax(mMemoryBlocks, requiredMemoryBlocks());
        config.imageWords = requiredImageWords();
        if (!mLocalEval->init(&config)) {
            return;
        }
        
        lbm_display_extensions_set_callbacks(
            (bool(*)(image_buffer_t*, uint16_t, uint16_t, color_t*))display_render_callback,
            (void(*)(uint32_t))display_clear_callback,
            (void(*)(void))display_reset_callback
        );
    }

    if (!mLocalEval->isRunning()) {
        mLocalEval->start();
    }
}

void PageLisp::resetLocalEvaluator()
{
    if (!mLocalEval) {
        return;
    }

    if (mLocalEval->isRunning()) {
        mLocalEval->stop();
    }

    delete mLocalEval;
    mLocalEval = nullptr;
}

void PageLisp::stopLocalEvaluator()
{
    if (mLocalEval && mLocalEval->isRunning()) {
        mLocalEval->stop();
    }
}

void PageLisp::runLocalProgram()
{
    auto editor = qobject_cast<ScriptEditor*>(ui->fileTabs->widget(ui->fileTabs->currentIndex()));

    if (!editor) {
        if (mVesc) {
            mVesc->emitMessageDialog(tr("No tab is open"), tr(""), false);
        }
        return;
    }

    ensureLocalEvaluator();
    if (mLocalEval && mLocalEval->isRunning()) {
        mLocalEval->evalProgram(editor->contentAsText());
    }
}

void PageLisp::evalLocalExpression(const QString &code)
{
    ensureLocalEvaluator();
    if (mLocalEval && mLocalEval->isRunning()) {
        mLocalEval->eval(code);
    }
}

void PageLisp::createDisplayTab()
{
    mDisplayTab = new QWidget(this);
    mDisplayTab->setStyleSheet("background-color: #484848;");

    QVBoxLayout *outerLayout = new QVBoxLayout(mDisplayTab);
    outerLayout->setContentsMargins(0, 0, 0, 0);

    QHBoxLayout *centerLayout = new QHBoxLayout();
    centerLayout->setContentsMargins(12, 12, 12, 12);
    centerLayout->addStretch();

    mDisplayCanvas = new DisplayCanvas(mDisplayWidth, mDisplayHeight, mDisplayTab);
    centerLayout->addWidget(mDisplayCanvas, 0, Qt::AlignCenter);
    centerLayout->addStretch();

    outerLayout->addStretch();
    outerLayout->addLayout(centerLayout);
    outerLayout->addStretch();

    ui->tabWidget->addTab(mDisplayTab, "Display");
}

void PageLisp::createEditorTab(QString fileName, QString content)
{
    if (ui->fileTabs->count() > 0 && !content.isEmpty()) {
        auto e = qobject_cast<ScriptEditor*>(ui->fileTabs->widget(ui->fileTabs->currentIndex()));

        if (e != nullptr && e->codeEditor()->toPlainText().isEmpty()) {
            e->codeEditor()->setPlainText(content);
            e->setFileNow(fileName);
            return;
        }
    }

    ScriptEditor *editor = new ScriptEditor();
    editor->setModeLisp();
    int tabIndex = ui->fileTabs->addTab(editor, "");
    ui->fileTabs->setCurrentIndex(tabIndex);

    makeEditorConnections(editor);

    connect(editor, &ScriptEditor::fileNameChanged, [this, editor](QString fileName) {
        QFileInfo f(fileName);
        QString txt = "Unnamed";

        if (f.exists()) {
            txt = f.fileName();
        } else if (!fileName.isEmpty()) {
            txt = fileName;
        }

        ui->fileTabs->setTabText(ui->fileTabs->indexOf(editor), txt);
    });

    editor->setFileNow(fileName);
    editor->codeEditor()->setPlainText(content);

    QPushButton *closeButton = new QPushButton();
    closeButton->setIcon(Utility::getIcon("icons/Cancel-96.png"));
    closeButton->setFlat(true);
    ui->fileTabs->tabBar()->setTabButton(tabIndex, QTabBar::RightSide, closeButton);

    connect(closeButton, &QPushButton::clicked, [this, editor]() {
       removeEditor(editor);
    });

    // Do this at the end to make sure to account for the changes from loading the initial text
    setEditorClean(editor);
}

void PageLisp::removeEditor(ScriptEditor *editor)
{
    bool shouldCloseTab = false;

    // Check if tab is dirty
    if (editor->hasUnsavedContent()) {
        // Ask user for confirmation
        QMessageBox::StandardButton answer = QMessageBox::question(
             this,
             tr("Delete the tab"),
             tr("Do you want to delete the tab contents?"),
             QMessageBox::Yes | QMessageBox::No
        );

        if (answer == QMessageBox::Yes) {
           shouldCloseTab = true;
        } else {
           shouldCloseTab = false;
        }

    } else {
        shouldCloseTab = true;
    }

    if (shouldCloseTab) {        
        if (ui->fileTabs->count() == 1) {
            editor->codeEditor()->clear();
            editor->setFileNow("");
        } else {
            ui->fileTabs->removeTab(ui->fileTabs->indexOf(editor));
        }
    }
}

void PageLisp::setEditorDirty(ScriptEditor *editor)
{
    // Check if the editor is not already dirty
    if (editor->isDirty == false) {
        // Set editor as dirty
        editor->isDirty = true;

        // Get editor index
        int tabIdx = ui->fileTabs->indexOf(editor);

        // Append a `*` to signify a dirty editor
        QString tabText = ui->fileTabs->tabText(tabIdx);
        ui->fileTabs->setTabText(tabIdx, tabText + "*");
    }
}

void PageLisp::setEditorClean(ScriptEditor *editor)
{
    // Check if the editor is not already clean
    if (editor->isDirty == true) {
        // Set editor as clean
        editor->isDirty = false;

        // Get editor index
        int tabIdx = ui->fileTabs->indexOf(editor);

        // Get the tab's label
        QString tabText = ui->fileTabs->tabText(tabIdx);

        // Check if the final character is a `*`, which indicated it was a dirty file
        if (tabText.back() == "*") {
            // Remove the terminal `*`
            tabText.chop(1);
            ui->fileTabs->setTabText(tabIdx, tabText);
        }
    }
}

void PageLisp::openExample()
{
    QListWidgetItem *item = ui->exampleList->currentItem();

    if (item) {
        QFile file(item->data(Qt::UserRole).toString());

        if (!file.open(QIODevice::ReadOnly)) {
            QMessageBox::critical(this, "Open LispBM File",
                                  "Could not open example for reading");
            return;
        }

        createEditorTab("", file.readAll());

        file.close();
    } else {
        QMessageBox::critical(this, "Open Example",
                              "Please select one example.");
    }
}

void PageLisp::openRecentList()
{
    auto *item = ui->recentList->currentItem();

    if (item) {
        QString fileName = item->text();
        QFile file(fileName);

        if (!file.open(QIODevice::ReadOnly)) {
            QMessageBox::critical(this, "Open LispBM File",
                                  "Could not open\n " + fileName + " for reading");
            return;
        }

        bool fileOpen = false;
        for (int i = 0;i < ui->fileTabs->count(); i++) {
            auto e = qobject_cast<ScriptEditor*>(ui->fileTabs->widget(i));
            if (e->fileNow() == fileName) {
                ui->fileTabs->setCurrentIndex(i);
                fileOpen = true;
                break;
            }
        }

        if (!fileOpen) {
            createEditorTab(fileName, file.readAll());
        }

        mRecentFiles.removeAll(fileName);
        mRecentFiles.prepend(fileName);
        updateRecentList();
        ui->recentList->setCurrentRow(0);

        file.close();
    } else {
        QMessageBox::critical(this, "Open Recent",
                              "Please select a file.");
    }
}

bool PageLisp::eraseCode(int size)
{
    QProgressDialog dialog("Erasing old script...", QString(), 0, 0, this);
    dialog.setWindowModality(Qt::WindowModal);
    dialog.show();
    auto res = mLoader.lispErase(size);
    dialog.close();
    return res;
}

void PageLisp::on_openRecentButton_clicked()
{
    openRecentList();
}

void PageLisp::on_removeSelectedButton_clicked()
{
    auto *item = ui->recentList->currentItem();

    if (item) {
        QString fileName = item->text();
        mRecentFiles.removeOne(fileName);
        updateRecentList();
    }
}

void PageLisp::on_clearRecentButton_clicked()
{
    auto res = QMessageBox::question(this, "Clear Recent Files",
                                     "This will clear the list with recent files. Are you sure?");

    if (res == QMessageBox::Yes) {
        mRecentFiles.clear();
        updateRecentList();
    }
}

void PageLisp::on_recentList_doubleClicked()
{
    openRecentList();
}

void PageLisp::on_openExampleButton_clicked()
{
    openExample();
}

void PageLisp::on_exampleList_doubleClicked()
{
    openExample();
}

void PageLisp::on_runButton_clicked()
{
    if (mVesc && mVesc->isPortConnected()) {
        mVesc->commands()->lispSetRunning(1);
    } else {
        runLocalProgram();
    }
}

void PageLisp::on_stopButton_clicked()
{
    if (mVesc && mVesc->isPortConnected()) {
        mVesc->commands()->lispSetRunning(0);
    } else {
        stopLocalEvaluator();
    }
}

void PageLisp::on_uploadButton_clicked()
{
    if (!(mVesc && mVesc->isPortConnected())) {
        runLocalProgram();
        return;
    }

    QProgressDialog dialog(tr("Erasing..."), tr("Cancel"), 0, 0, this);
    dialog.setWindowModality(Qt::WindowModal);
    dialog.show();

    auto e = qobject_cast<ScriptEditor*>(ui->fileTabs->widget(ui->fileTabs->currentIndex()));

    QString codeStr = "";
    QString editorPath = QDir::currentPath();

    if (e == nullptr) {
        mVesc->emitMessageDialog(
            tr("No tab is open"),
            tr(""),
            false);
        return;
    }

    codeStr = e->contentAsText();
    QFileInfo fi(e->fileNow());
    if (fi.exists()) {
        editorPath = fi.canonicalPath();
    }

    QSettings set;
    VByteArray vb = mLoader.lispPackImports(
        codeStr, editorPath, set.value("reduceLisp", false).toBool());
    if (vb.isEmpty()) {
        return;
    }

    QTimer closeStopTimer;
    closeStopTimer.start(100);
    auto conn1 = connect(&closeStopTimer, &QTimer::timeout, [&dialog, this]() {
        if (!dialog.isVisible() && dialog.value() > 0) {
            dialog.show();
        }

        if (dialog.wasCanceled()) {
            mLoader.abortDownloadUpload();
        }
    });

    if (!eraseCode(vb.size() + 100)) {
        disconnect(conn1);
        return;
    }

    auto conn2 = connect(&mLoader, &CodeLoader::lispUploadProgress, [&dialog](qint64 bytes, qint64 bytesTotal) {
        dialog.setMaximum(bytesTotal);
        dialog.setValue(bytes);
    });

    dialog.setLabelText("Uploading");
    bool ok = mLoader.lispUpload(vb);

    disconnect(conn1);
    disconnect(conn2);

    if (ok && ui->autoRunBox->isChecked()) {
        on_runButton_clicked();
    }
}

void PageLisp::on_readExistingButton_clicked()
{
    if (!(mVesc && mVesc->isPortConnected())) {
        return;
    }

    QProgressDialog dialog(tr("Reading Code..."), tr("Cancel"), 0, 0, this);
    dialog.setWindowModality(Qt::WindowModal);
    dialog.show();

    QTimer closeStopTimer;
    closeStopTimer.start(100);
    auto conn1 = connect(&closeStopTimer, &QTimer::timeout, [&dialog, this]() {
        if (!dialog.isVisible() && dialog.value() > 0) {
            dialog.show();
        }

        if (dialog.wasCanceled()) {
            mLoader.abortDownloadUpload();
        }
    });

    auto conn2 = connect(&mLoader, &CodeLoader::lispUploadProgress, [&dialog](qint64 bytes, qint64 bytesTotal) {
        dialog.setMaximum(bytesTotal);
        dialog.setValue(bytes);
    });

    QString lispPath = "From VESC";
    auto code = mLoader.lispRead(this, lispPath);

    if (!code.isEmpty()) {
        createEditorTab(lispPath, code);
    }

    disconnect(conn1);
    disconnect(conn2);
}

void PageLisp::on_eraseButton_clicked()
{
    if (!(mVesc && mVesc->isPortConnected())) {
        return;
    }

    eraseCode(16);
}

void PageLisp::on_rescaleButton_clicked()
{
    ui->bindingPlot->rescaleAxes();
    ui->bindingPlot->replotWhenVisible();
}

void PageLisp::on_helpButton_clicked()
{
    QString html = "<b>VESC LispBM Editor</b><br><br>"
                   ""
                   "For documentation, go to<br>"
                   "<a href=\"https://github.com/vedderb/bldc/blob/master/lispBM/README.md\">https://github.com/vedderb/bldc/blob/master/lispBM/README.md</a><br><br>"
                   ""
                   "You can also join the unofficial VESC Discord server and ask questions "
                   "in the lisp-and-qml-scripting chat at<br>"
                   "<a href=\"https://discord.gg/JgvV5NwYts\">https://discord.gg/JgvV5NwYts</a><br><br>"
                   "<b>Keyboard Commands</b><br>"
                   "Ctrl + '+'         : Increase font size<br>"
                   "Ctrl + '-'         : Decrease font size<br>"
                   "Ctrl + space       : Show auto-complete suggestions<br>"
                   "Ctrl + '/'         : Toggle auto-comment on line or block<br>"
                   "Ctrl + '#'         : Toggle auto-comment on line or block<br>"
                   "Ctrl + 'i'         : Auto-indent selected lines<br>"
                   "Ctrl + 'f'         : Open search (and replace) bar<br>"
                   "Ctrl + 'e'         : Upload (and run if set) application<br>"
                   "Ctrl + 'w'         : Stream application<br>"
                   "Ctrl + 'q'         : Stop application<br>"
                   "Ctrl + 'd'         : Clear console<br>"
                   "Ctrl + 's'         : Save file<br>"
                   "Ctrl + 'r'         : Run selected block in REPL<br>"
                   "Ctrl + Shift + 'd' : Duplicate current line<br>";

    HelpDialog::showHelpMonospace(this, "VESC Tool Script Editor", html);
}

void PageLisp::on_replEdit_returnPressed()
{
    QString cmd = ui->replEdit->text().trimmed();

    if (mVesc && mVesc->isPortConnected()) {
        mVesc->commands()->lispSendReplCmd(cmd);
    } else {
        evalLocalExpression(cmd);
    }

    ui->replEdit->clear();
}

void PageLisp::on_replHelpButton_clicked()
{
    if (mVesc && mVesc->isPortConnected()) {
        mVesc->commands()->lispSendReplCmd(":help");
    } else {
        evalLocalExpression("(help)");
    }
}

void PageLisp::on_streamButton_clicked()
{
    if (!(mVesc && mVesc->isPortConnected())) {
        runLocalProgram();
        return;
    }

    QProgressDialog dialog(tr("Streaming..."), QString(), 0, 0, this);
    dialog.setWindowModality(Qt::WindowModal);
    dialog.show();

    auto e = qobject_cast<ScriptEditor*>(ui->fileTabs->widget(ui->fileTabs->currentIndex()));

    QString codeStr = "";

    if (e == nullptr) {
        mVesc->emitMessageDialog(
            tr("No tab is open"),
            tr(""),
            false);
        return;
    }

    codeStr = e->contentAsText();

    mLoader.lispStream(codeStr.toLocal8Bit(), ui->streamModeBox->currentIndex());
}

void PageLisp::on_recentFilterEdit_textChanged(const QString &filter)
{
    for (int row = 0; row < ui->recentList->count(); ++row) {
        if (filter.isEmpty()) {
            ui->recentList->item(row)->setHidden(false);
        } else {
            ui->recentList->item(row)->setHidden(!ui->recentList->item(row)->text().
                                                 contains(filter, Qt::CaseInsensitive));
        }
    }
}

void PageLisp::on_exampleFilterEdit_textChanged(const QString &filter)
{
    for (int row = 0; row < ui->exampleList->count(); ++row) {
        if (filter.isEmpty()) {
            ui->exampleList->item(row)->setHidden(false);
        } else {
            ui->exampleList->item(row)->setHidden(!ui->exampleList->item(row)->text().
                                                 contains(filter, Qt::CaseInsensitive));
        }
    }
}

void PageLisp::on_infoButton_clicked()
{
    auto e = qobject_cast<ScriptEditor*>(ui->fileTabs->widget(ui->fileTabs->currentIndex()));

    QString codeStr = "";
    QString editorPath = QDir::currentPath();

    if (e == nullptr) {
        mVesc->emitMessageDialog(
            tr("No tab is open"),
            tr(""),
            false);
        return;
    }

    codeStr = e->contentAsText();
    QFileInfo fi(e->fileNow());
    if (fi.exists()) {
        editorPath = fi.canonicalPath();
    }

    VByteArray vbNoReduce = mLoader.lispPackImports(codeStr, editorPath, false);
    VByteArray vbReduce = mLoader.lispPackImports(codeStr, editorPath, true);
    bool reduceEnabled = QSettings().value("reduceLisp", false).toBool();

    QString html = QString(
                       "<b>File Info</b><br>"
                       "<br>"
                       "Size                : %1<br>"
                       "Size after reduction: %2<br>"
                       "Reduction enabled   : %3<br>"
                       "<br>"
                       "The sizes include the size of all imports. All "
                       "imports that end with .lbm or .lisp are also reduced. "
                       "The reduction option can be enabled in the preferences."
                       ).arg(vbNoReduce.size()).
                   arg(vbReduce.size()).
                   arg(reduceEnabled);

    HelpDialog::showHelpMonospace(this, "File Info", html);
}

void PageLisp::createSettingsTab()
{
    QWidget *settingsWidget = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(settingsWidget);
    
    QGroupBox *displayGroup = new QGroupBox("Display Settings", settingsWidget);
    QGridLayout *displayLayout = new QGridLayout(displayGroup);
    
    displayLayout->addWidget(new QLabel("Width:"), 0, 0);
    QSpinBox *widthSpinBox = new QSpinBox();
    widthSpinBox->setMinimum(16);
    widthSpinBox->setMaximum(1280);
    widthSpinBox->setValue(mDisplayWidth);
    displayLayout->addWidget(widthSpinBox, 0, 1);
    
    displayLayout->addWidget(new QLabel("Height:"), 1, 0);
    QSpinBox *heightSpinBox = new QSpinBox();
    heightSpinBox->setMinimum(16);
    heightSpinBox->setMaximum(960);
    heightSpinBox->setValue(mDisplayHeight);
    displayLayout->addWidget(heightSpinBox, 1, 1);
    
    QPushButton *applyDisplayButton = new QPushButton("Apply Display Size");
    connect(applyDisplayButton, &QPushButton::clicked, [this, widthSpinBox, heightSpinBox]() {
        mDisplayWidth = widthSpinBox->value();
        mDisplayHeight = heightSpinBox->value();
        on_displaySizeChanged();
    });
    displayLayout->addWidget(applyDisplayButton, 2, 0, 1, 2);
    
    layout->addWidget(displayGroup);
    
    QGroupBox *memoryGroup = new QGroupBox("LispBM Memory Settings", settingsWidget);
    QGridLayout *memoryLayout = new QGridLayout(memoryGroup);
    
    memoryLayout->addWidget(new QLabel("Heap Cells:"), 0, 0);
    QSpinBox *heapSpinBox = new QSpinBox();
    heapSpinBox->setMinimum(1024);
    heapSpinBox->setMaximum(262144);
    heapSpinBox->setValue(mHeapCells);
    heapSpinBox->setSingleStep(1024);
    memoryLayout->addWidget(heapSpinBox, 0, 1);
    
    memoryLayout->addWidget(new QLabel("Memory Blocks:"), 1, 0);
    QSpinBox *blockSpinBox = new QSpinBox();
    blockSpinBox->setMinimum(256);
    blockSpinBox->setSingleStep(64);
    blockSpinBox->setMaximum(16384);
    blockSpinBox->setValue(mMemoryBlocks);
    memoryLayout->addWidget(blockSpinBox, 1, 1);

    QLabel *requiredMemLabel = new QLabel(
        QString("Minimum recommended blocks for %1x%2 rgb888: %3")
            .arg(mDisplayWidth)
            .arg(mDisplayHeight)
            .arg(requiredMemoryBlocks())
    );
    requiredMemLabel->setStyleSheet("color: gray; font-size: 12px;");
    memoryLayout->addWidget(requiredMemLabel, 2, 0, 1, 2);
    
    QLabel *infoLabel = new QLabel("Changes restart the local evaluator so new img-buffer allocations use the updated limits.");
    infoLabel->setStyleSheet("color: gray; font-size: 18px;");
    memoryLayout->addWidget(infoLabel, 3, 0, 1, 2);
    
    QPushButton *applyMemoryButton = new QPushButton("Apply Memory Settings");
    connect(applyMemoryButton, &QPushButton::clicked, [this, heapSpinBox, blockSpinBox]() {
        mHeapCells = heapSpinBox->value();
        mMemoryBlocks = blockSpinBox->value();
        on_memorySettingsChanged();
    });
    memoryLayout->addWidget(applyMemoryButton, 4, 0, 1, 2);
    
    layout->addWidget(memoryGroup);
    layout->addStretch();
    
    ui->tabWidget->addTab(settingsWidget, "Settings");
}

void PageLisp::on_displaySizeChanged()
{
    resetLocalEvaluator();

    if (mDisplayTab) {
        int index = ui->tabWidget->indexOf(mDisplayTab);
        if (index >= 0) {
            ui->tabWidget->removeTab(index);
        }
        delete mDisplayTab;
        mDisplayTab = nullptr;
        mDisplayCanvas = nullptr;
    }

    createDisplayTab();
    int displayIndex = ui->tabWidget->indexOf(mDisplayTab);
    if (displayIndex >= 0 && displayIndex != 4) {
        ui->tabWidget->removeTab(displayIndex);
        ui->tabWidget->insertTab(4, mDisplayTab, "Display");
    }
    ui->tabWidget->setCurrentIndex(4);
}

void PageLisp::on_memorySettingsChanged()
{
    resetLocalEvaluator();

    if (mVesc) {
        mVesc->emitStatusMessage(QString("Memory settings applied (Heap: %1, Blocks: %2, Required blocks: %3).")
                                .arg(mHeapCells).arg(mMemoryBlocks).arg(requiredMemoryBlocks()), true);
    }
}

int PageLisp::requiredImageWords() const
{
    const int imageBytes = (mDisplayWidth * mDisplayHeight * 3) + 5;
    return qMax(32768, ceil_div(imageBytes, int(sizeof(uint32_t))));
}

int PageLisp::requiredMemoryBlocks() const
{
    const int imageBytes = (mDisplayWidth * mDisplayHeight * 3) + 5;
    const int imageBlocks = ceil_div(imageBytes, 64);

    return qMax(512, imageBlocks + 768);
}
