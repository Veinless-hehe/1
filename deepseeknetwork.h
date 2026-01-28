#ifndef DEEPSEEKNETWORK_H
#define DEEPSEEKNETWORK_H


#include <QThread>
#include <QNetworkRequest>
#include <QNetworkAccessManager>
#include <QNetworkReply>

typedef struct tabDeepSeekOutput {
    QString id; // 消息id
    QString content; // 消息内容
    int status { 0 }; // 消息状态：-1=失败， 0=正在发送，1=完成
}DeepSeekOutput;
Q_DECLARE_METATYPE(DeepSeekOutput)

///
/// \brief The DeepSeekNetwork class
/// deepseek chat
/// 参考文档：https://api-docs.deepseek.com/zh-cn/
///
class DeepSeekNetwork;
class DeepSeekTask : public QThread
{
    Q_OBJECT
public:
    explicit DeepSeekTask(QObject* parent = nullptr);
    void seek(const QString &string);
    void stop();

signals:
    void signalOutput(DeepSeekOutput data);
    void signalStopped();

protected:
    void run() override;

private:
    DeepSeekNetwork *mDeepSeekNetwork { nullptr };
    void initialize();
};

class DeepSeekNetwork : public QObject
{
    Q_OBJECT
public:
    explicit DeepSeekNetwork(QObject* parent = nullptr);
    Q_INVOKABLE void seek(const QString &string);
    Q_INVOKABLE void stop();

signals:
    void signalOutput(DeepSeekOutput data);
    void signalStopped();

private:
    QNetworkRequest mRequest; // 封装网络请求信息
    QNetworkAccessManager *mNetWorkManager { nullptr }; // 管理网络请求和响应
    QNetworkReply *mNetWorkReply { nullptr }; // 网络请求响应
    // base_url:https://api.deepseek.com
    // 出于与 OpenAI 兼容考虑，您也可以将 base_url 设置为 https://api.deepseek.com/v1 来使用，但注意，此处 v1 与模型版本无关。
    // deepseek-chat 模型已全面升级为 DeepSeek-V3，接口不变。 通过指定 model='deepseek-chat' 即可调用 DeepSeek-V3。
    // deepseek-reasoner 是 DeepSeek 最新推出的推理模型 DeepSeek-R1。通过指定 model='deepseek-reasoner'，即可调用 DeepSeek-R1。
    QString mCurlHttps = "https://api.moonshot.cn/v1/chat/completions";
    QString mModel = "kimi-k2.5";
    QString mApiKey = "sk-C2e9Np6UxnaT7CEt1TakhcvV1hw0Cn85vR7inkccrAdjoN8O";
    bool mIsStream{true}; // 是否数据流接收数据
    bool mIsStopped{false};

private slots:
    void onReadyRead();

private:
    void initialize();
    void request(const QByteArray &postData);
};

#endif   // DEEPSEEKNETWORK_H
