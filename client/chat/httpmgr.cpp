#include "httpmgr.h"

httpMgr::~httpMgr()
{

}

httpMgr::httpMgr()
{
    connect(this, &httpMgr::sig_http_finish, this, &httpMgr::slot_http_finish);
}

void httpMgr::PostHttpReq(QUrl url, QJsonObject json, ReqId req_id, Modules mod)
{
    QByteArray data = QJsonDocument(json).toJson();
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader,"application/json");
    request.setHeader(QNetworkRequest::ContentLengthHeader, QByteArray::number(data.length())); // 网络编程：大端小端怎么处理
    auto self = shared_from_this(); // B包技术，c++没有这个，共享引用计数
    QNetworkReply * reply = _manager.post(request, data);
    QObject::connect(reply, &QNetworkReply::finished, [self, reply, req_id, mod](){
        // deal with error condition
        if(reply->error() != QNetworkReply::NoError){
            qDebug() << reply->errorString();
            // Send signal to announce finished
            emit self->sig_http_finish(req_id, "", ErrorCodes::ERR_NETWORK, mod);
            reply->deleteLater();
            return;
        }

        // No error condition
        QString res = reply->readAll();
        // send signals to announce finished
        emit self->sig_http_finish(req_id, res, ErrorCodes::SUCCESS, mod);
        reply->deleteLater();
        return;
    });


}

void httpMgr::slot_http_finish(ReqId id, QString res, ErrorCodes err, Modules mod)
{
    if(mod == Modules::REGISTERMOD){
        //发送信号通知指定模块http响应结束
        emit sig_reg_mod_finish(id, res, err);
    }

    if(mod == Modules::RESETMOD){
        //发送信号通知指定模块http响应结束
        emit sig_reset_mod_finish(id, res, err);
    }

    if(mod == Modules::LOGINMOD){
        emit sig_login_mod_finish(id, res, err);
    }
}
