#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QDebug>
#include <QPalette>
#include <QClipboard>
#include <QApplication>
#include <QMessageBox>
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QMimeType>
#include <QImage>
#include <QImageReader>
#include <QTextDocument>
#include <QTextCursor>
#include <QTextImageFormat>
#include <QBuffer>
#include <QMenu>
#include <QDir>

#include "deepseeknetwork.h"

              MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // 设置窗口标题
    setWindowTitle("DeepSeek AI Assistant");

    // 设置颜色按钮组
    QButtonGroup *colorGroup = new QButtonGroup(this);
    colorGroup->addButton(ui->whiteBtn);
    colorGroup->addButton(ui->blackBtn);
    colorGroup->addButton(ui->blueBtn);
    colorGroup->addButton(ui->yellowBtn);

    // 设置默认颜色
    ui->whiteBtn->setChecked(true);
    setBackgroundColor("white");

    // 初始化组件
    initialize();
}

MainWindow::~MainWindow()
{
    if (mDeepSeekTask != nullptr) {
        if (mDeepSeekTask->isRunning()) {
            mDeepSeekTask->stop();
            mDeepSeekTask->wait(2000);
            mDeepSeekTask->deleteLater();
            mDeepSeekTask = nullptr;
        }
    }
    delete ui;
}

void MainWindow::initialize()
{
    mDeepSeekTask = new DeepSeekTask();

    // 连接输出信号
    connect(mDeepSeekTask, &DeepSeekTask::signalOutput, this, [=](DeepSeekOutput result) {
        if (result.status == 2) {
            // 用户停止
            ui->output_textEdit->append("\n[已停止] " + result.content);
        } else if (result.status == 1) {
            // 完成
            ui->output_textEdit->append("");
        } else if (result.status == -1) {
            // 错误
            ui->output_textEdit->append("\n[错误] " + result.content);
        } else {
            // 流式输出
            if (!result.content.isEmpty()) {
                QTextCursor cursor = ui->output_textEdit->textCursor();
                cursor.movePosition(QTextCursor::End);
                cursor.insertText(result.content);
                ui->output_textEdit->setTextCursor(cursor);
                ui->output_textEdit->ensureCursorVisible();
            }
        }
    }, Qt::QueuedConnection);

    // 连接停止信号
    connect(mDeepSeekTask, &DeepSeekTask::signalStopped, this, [=]() {
        ui->output_textEdit->append("\n[系统] 生成已停止。");
    }, Qt::QueuedConnection);

    mDeepSeekTask->start();
}

void MainWindow::on_seekBtn_clicked()
{
    if (mDeepSeekTask == nullptr) {
        return;
    }

    QString input = ui->input_textEdit->toPlainText().trimmed();

    if (input.isEmpty()) {
        QMessageBox::information(this, "提示", "请输入问题或指令。");
        return;
    }

    ui->output_textEdit->append("\n[用户]: " + input);
    mDeepSeekTask->seek(input);
}

void MainWindow::on_regenerateBtn_clicked()
{
    QString input = ui->input_textEdit->toPlainText().trimmed();

    if (input.isEmpty()) {
        QMessageBox::information(this, "提示", "输入框为空，请输入内容。");
        return;
    }

    if (mDeepSeekTask != nullptr) {
        ui->output_textEdit->append("\n[重新生成]: " + input);
        mDeepSeekTask->seek(input);
    }
}

void MainWindow::on_clearBtn_clicked()
{
    ui->output_textEdit->clear();
}

void MainWindow::setBackgroundColor(const QString &color)
{
    QPalette palette = ui->output_textEdit->palette();

    if (color == "white") {
        palette.setColor(QPalette::Base, Qt::white);
        palette.setColor(QPalette::Text, Qt::black);
    } else if (color == "black") {
        palette.setColor(QPalette::Base, Qt::black);
        palette.setColor(QPalette::Text, Qt::white);
    } else if (color == "blue") {
        palette.setColor(QPalette::Base, QColor(220, 240, 255));
        palette.setColor(QPalette::Text, Qt::black);
    } else if (color == "yellow") {
        palette.setColor(QPalette::Base, QColor(255, 255, 220));
        palette.setColor(QPalette::Text, Qt::black);
    }

    ui->output_textEdit->setPalette(palette);
}

void MainWindow::on_whiteBtn_clicked() { setBackgroundColor("white"); }
void MainWindow::on_blackBtn_clicked() { setBackgroundColor("black"); }
void MainWindow::on_blueBtn_clicked() { setBackgroundColor("blue"); }
void MainWindow::on_yellowBtn_clicked() { setBackgroundColor("yellow"); }

void MainWindow::on_copyBtn_clicked()
{
    QString text = ui->output_textEdit->toPlainText();

    if (text.isEmpty()) {
        QMessageBox::information(this, "提示", "没有内容可以复制。");
        return;
    }

    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setText(text);
    QMessageBox::information(this, "提示", "内容已复制到剪贴板！");
}

void MainWindow::on_stopBtn_clicked()
{
    if (mDeepSeekTask != nullptr) {
        mDeepSeekTask->stop();
    }
}

void MainWindow::on_exampleBtn_clicked()
{
    // 创建示例问题菜单
    QMenu *exampleMenu = new QMenu(this);

    // 添加示例问题
    QAction *action1 = exampleMenu->addAction("1. 介绍一下你自己");
    QAction *action2 = exampleMenu->addAction("2. 什么是人工智能？");
    QAction *action3 = exampleMenu->addAction("3. 写一个简单的C++程序");

    QPoint pos = ui->exampleBtn->mapToGlobal(QPoint(0, ui->exampleBtn->height()));
    QAction *selectedAction = exampleMenu->exec(pos);

    if (selectedAction) {
        QString question;

        // 根据选择的示例设置问题
        if (selectedAction == action1) {
            question = "请介绍一下你自己，包括你的能力和特点。";
        } else if (selectedAction == action2) {
            question = "什么是人工智能？请简要解释一下。";
        } else if (selectedAction == action3) {
            question = "写一个简单的C++程序，实现一个计算器功能。";
        }
        sendExampleRequest(question);
    }

    delete exampleMenu;
}

void MainWindow::sendExampleRequest(const QString &question)
{
    if (mDeepSeekTask == nullptr) {
        return;
    }

    ui->input_textEdit->setText(question);   // 清空输入框并设置示例问题
    ui->output_textEdit->append("\n[示例问题]: " + question);  // 在输出框中显示示例问题
    mDeepSeekTask->seek(question);  // 发送请求
}

void MainWindow::on_uploadBtn_clicked()
{
    // 支持的文件类型
    QString filter = "所有文件 (*.*);;"
                     "文本文件 (*.txt);;"
                     "代码文件 (*.cpp *.h *.py *.java *.js *.html *.css *.md);;"
                     "图像文件 (*.png *.jpg *.jpeg *.bmp *.gif)";

    QString filePath = QFileDialog::getOpenFileName(this,
                                                    "选择文件",
                                                    QDir::homePath(),
                                                    filter);

    if (filePath.isEmpty()) {
        return; // 用户取消选择
    }

    QFileInfo fileInfo(filePath);
    QString fileName = fileInfo.fileName();
    qint64 fileSize = fileInfo.size();
    QString fileSizeStr;

    if (fileSize < 1024) {
        fileSizeStr = QString::number(fileSize) + " B";
    } else if (fileSize < 1024 * 1024) {
        fileSizeStr = QString::number(fileSize / 1024.0, 'f', 2) + " KB";
    } else {
        fileSizeStr = QString::number(fileSize / (1024.0 * 1024.0), 'f', 2) + " MB";
    }

    // 在界面中显示文件信息
    ui->output_textEdit->append("\n[上传文件]: " + fileName + " (" + fileSizeStr + ")");

    // 尝试提取文件内容
    QString content = getFileContent(filePath);

    if (content.isEmpty()) {
        ui->output_textEdit->append("  文件为空或无法读取内容。");
        QMessageBox::warning(this, "警告", "无法读取文件内容，文件可能为空或格式不受支持。");
    } else {
        // 显示文件内容摘要
        QString preview = content.left(500); // 预览前500个字符
        if (content.length() > 500) {
            preview += "...";
        }
        ui->output_textEdit->append("  内容预览: " + preview);

        // 将文件内容添加到输入框
        QString currentText = ui->input_textEdit->toPlainText();
        if (!currentText.isEmpty()) {
            currentText += "\n\n";
        }
        currentText += "请分析以下文件内容：\n" + content;
        ui->input_textEdit->setText(currentText);

        // 询问用户是否要发送
        QMessageBox::StandardButton reply;
        reply = QMessageBox::question(this, "文件上传",
                                      "文件已成功上传。是否立即发送给AI进行分析？",
                                      QMessageBox::Yes | QMessageBox::No);

        if (reply == QMessageBox::Yes) {
            on_seekBtn_clicked();
        }
    }
}

QString MainWindow::getFileContent(const QString &filePath)
{
    QFile file(filePath);

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "错误", "无法打开文件: " + filePath);
        return QString();
    }

    QFileInfo fileInfo(filePath);
    QString suffix = fileInfo.suffix().toLower();

    // 根据文件类型处理内容
    if (suffix == "txt" || suffix == "md" ||
        suffix == "cpp" || suffix == "h" || suffix == "hpp" ||
        suffix == "py" || suffix == "java" ||
        suffix == "js" || suffix == "html" || suffix == "css" ||
        suffix == "json" || suffix == "xml" || suffix == "csv") {
        // 文本文件，直接读取
        QTextStream in(&file);
        in.setEncoding(QStringConverter::Utf8);
        return in.readAll();
    }
    else if (suffix == "jpg" || suffix == "jpeg" ||
             suffix == "png" || suffix == "bmp" ||
             suffix == "gif") {
        // 图像文件 - 返回基本信息
        QImage image(filePath);
        if (image.isNull()) {
            return "无法读取图像文件";
        }
        return QString("图像文件: %1x%2 像素，格式: %3，大小: %4")
            .arg(image.width())
            .arg(image.height())
            .arg(suffix.toUpper())
            .arg(fileInfo.size() < 1024 ?
                     QString::number(fileInfo.size()) + " B" :
                     QString::number(fileInfo.size() / 1024.0, 'f', 2) + " KB");
    }
    else {
        // 其他文件类型
        return QString("文件: %1\n"
                       "类型: %2\n"
                       "大小: %3\n"
                       "注意: 此文件类型的内容无法直接读取，请手动描述文件内容。")
            .arg(fileInfo.fileName())
            .arg(suffix.toUpper())
            .arg(fileInfo.size() < 1024 ?
                     QString::number(fileInfo.size()) + " B" :
                     QString::number(fileInfo.size() / 1024.0, 'f', 2) + " KB");
    }
}
