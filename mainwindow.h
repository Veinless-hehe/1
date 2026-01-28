#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QButtonGroup>

QT_BEGIN_NAMESPACE
namespace Ui
{
class MainWindow;
}
QT_END_NAMESPACE

class DeepSeekTask;
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

private slots:
    void on_seekBtn_clicked();
    void on_regenerateBtn_clicked();
    void on_clearBtn_clicked();   /*添加清除按键槽函数*/
    void on_whiteBtn_clicked();   /*添加颜色按键槽函数*/
    void on_blackBtn_clicked();
    void on_blueBtn_clicked();
    void on_yellowBtn_clicked();
    void on_copyBtn_clicked();
    void on_stopBtn_clicked();
    void on_exampleBtn_clicked();
    void on_uploadBtn_clicked();


private:
    Ui::MainWindow* ui;
    DeepSeekTask *mDeepSeekTask { nullptr };
    QString mCurrentId;
    QString lastMessage;  /* */
    void initialize();
    void setBackgroundColor(const QString &color); /*设置背景板颜色*/
    void copyToClipboard();
    void sendExampleRequest(const QString &question);
    QString getFileContent(const QString &filePath);

};
#endif   // MAINWINDOW_H
