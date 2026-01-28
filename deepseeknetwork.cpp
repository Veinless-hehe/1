#include "deepseeknetwork.h"
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSslSocket>
#include <QSslConfiguration>

DeepSeekTask::DeepSeekTask(QObject* parent) : QThread { parent } {}

void DeepSeekTask::initialize()
{
    mDeepSeekNetwork = new DeepSeekNetwork();
    connect(mDeepSeekNetwork, &DeepSeekNetwork::signalOutput, this, [=](DeepSeekOutput data) {
        emit signalOutput(data);
    }, Qt::DirectConnection);
}

void DeepSeekTask::seek(const QString &string)
{
    if (mDeepSeekNetwork == nullptr) {
        return;
    }
    QMetaObject::invokeMethod(mDeepSeekNetwork,"seek", Qt::BlockingQueuedConnection
                              ,Q_ARG(const QString&, string));
}

void DeepSeekTask::run()
{
    qDebug() << Q_FUNC_INFO << QThread::currentThread();

    initialize();

    exec();

    if (mDeepSeekNetwork != nullptr) {
        delete mDeepSeekNetwork;
        mDeepSeekNetwork = nullptr;
    }
}
void DeepSeekTask::stop()
{
    if (mDeepSeekNetwork == nullptr) {
        return;
    }
    QMetaObject::invokeMethod(mDeepSeekNetwork,"stop", Qt::BlockingQueuedConnection);
}


DeepSeekNetwork::DeepSeekNetwork(QObject* parent) : QObject { parent }
{
    initialize();
}

void DeepSeekNetwork::seek(const QString &string)
{
    mIsStopped = false;
    // messages部分
    QJsonArray messages;
    QJsonObject role;
    role["role"] = "system";
    role["content"] = "You are a helpful assistant.";
    messages.append(role);
    QJsonObject role2;
    role2["role"] = "user";
    role2["content"] = string;
    messages.append(role2);

    QJsonObject data;
    data["model"] = mModel;
    data["messages"] = messages;
    data["stream"] = true;

    QJsonDocument doc(data);
    QByteArray postData = doc.toJson();
    request(postData);
}
void DeepSeekNetwork::stop()
{
    if (mNetWorkReply != nullptr && mNetWorkReply->isRunning()) {
        qDebug() << Q_FUNC_INFO << "Stopping network request...";
        mIsStopped = true;
        mNetWorkReply->abort();
        emit signalStopped();
    }
}

void DeepSeekNetwork::initialize()
{
    mNetWorkManager = new QNetworkAccessManager(this);

    qDebug() << Q_FUNC_INFO << "QSslSocket:" << QSslSocket::sslLibraryBuildVersionString();
    qDebug() << Q_FUNC_INFO << "supportsSsl:" << QSslSocket::supportsSsl();
}

void DeepSeekNetwork::onReadyRead()
{
    if (mIsStopped) {
        return;
    }

    DeepSeekOutput result;
    result.status = 0;
    QString output;
    if (mNetWorkReply != nullptr && mNetWorkReply->error() == QNetworkReply::NoError) {
        QByteArray readData = (mNetWorkReply->readAll());
        qDebug() <<  readData;
        if (mIsStream) {
             QByteArray key = "data: ";
            QString str = QString::fromUtf8(readData);
            if (str.contains(key)) {
                QStringList strList = str.split(key);
                foreach (QString var, strList) {
                    if (var.isEmpty()) continue;
                    //对返回的json数据进行解析
                    QByteArray data = var.toUtf8();
                    QJsonObject obj = QJsonDocument::fromJson(data).object();
                    if (obj.contains("id") && obj.value("id").isString()) {
                        result.id = obj.value("id").toString();
                    }
                    qDebug() << Q_FUNC_INFO << QThread::currentThread() << "onReadyRead:" << obj;
                    if (obj.contains("error") && obj.value("error").isObject()) {
                        QJsonObject valueObj = obj.value("error").toObject();
                        QString message = valueObj.value("message").toString();
                        output = message;
                        result.status = -1;
                    } else {
                        QJsonArray choicesarray = obj.value("choices").toArray();
                        for (int i = 0; i < choicesarray.size(); i++) {
                            QJsonObject choiceobj = choicesarray[i].toObject();
                            if (choiceobj.contains("delta") && choiceobj.value("delta").isObject()) {
                                QJsonObject deltaObj = choiceobj.value("delta").toObject();
                                if (deltaObj.contains("content") && deltaObj.value("content").isString()) {
                                    output += deltaObj.value("content").toString();
                                }
                            }
                            if (choiceobj.contains("finish_reason") && choiceobj.value("finish_reason").isString()) {
                                QString finish_reason = choiceobj.value("finish_reason").toString();
                                if (finish_reason == "stop") result.status = 1;
                            }
                        }
                    }
                }
            }
        } else {
            //对返回的json数据进行解析
            QJsonObject obj = QJsonDocument::fromJson(readData).object();
            qDebug() << Q_FUNC_INFO << "onReadyRead:" << obj;
            if (obj.contains("id") && obj.value("id").isString()) {
                result.id = obj.value("id").toString();
            }
            if (obj.contains("error") && obj.value("error").isObject()) {
                QJsonObject valueObj = obj.value("error").toObject();
                QString code = valueObj.value("code").toString();
                QString message = valueObj.value("message").toString();
                output = message;
                result.status = -1;
            } else {
                QJsonArray choicesarray = obj.value("choices").toArray();
                for (int i = 0; i < choicesarray.size(); i++) {
                    QJsonObject choiceobj = choicesarray[i].toObject();
                    if (choiceobj.contains("message") && choiceobj.value("message").isObject()) {
                        QJsonObject messageobj = choiceobj.value("message").toObject();
                        if (messageobj.contains("content") && messageobj.value("content").isString()) {
                            output += messageobj.value("content").toString();
                        }
                    }
                }
            }
        }

    } else {
        if (mIsStopped) {
            output = "Generation stopped by user.";
            result.status = 2;
    } else {
        output = mNetWorkReply->errorString();
        result.status = -1;
        qDebug() << Q_FUNC_INFO << "https request error:" << output;
    }
    }
    if (!mIsStopped) {
    result.content = output;
    emit signalOutput(result);
    }
}

void DeepSeekNetwork::request(const QByteArray &postData)
{
    qDebug() << Q_FUNC_INFO << QThread::currentThread() << postData;
    // url
    mRequest.setUrl(QUrl(mCurlHttps));
    // head
    mRequest.setRawHeader("Authorization", ("Bearer " + mApiKey).toUtf8());
    mRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    // ssl
    QSslConfiguration config = QSslConfiguration::defaultConfiguration();
    config.setProtocol(QSsl::AnyProtocol);
    config.setPeerVerifyMode(QSslSocket::VerifyNone);
    mRequest.setSslConfiguration(config);

    //发送post请求
    mNetWorkReply = mNetWorkManager->post(mRequest, postData);
    // 链接槽函数
    if (mNetWorkReply != nullptr) {
        disconnect(mNetWorkReply, &QIODevice::readyRead, this, &DeepSeekNetwork::onReadyRead);
        connect(mNetWorkReply, &QIODevice::readyRead, this, &DeepSeekNetwork::onReadyRead);
    }
}
