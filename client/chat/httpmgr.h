#ifndef HTTPMGR_H
#define HTTPMGR_H
#include "singleton.h"
#include <QString>
#include <QUrl>
#include <QObject>
#include <QNetworkAccessManager>
#include <QJsonObject>
#include <QJsonDocument> //反序列化

//CRTP 奇异递归模板
//异步发送http

class httpMgr:public QObject, public Singleton<httpMgr>,
                public std::enable_shared_from_this<httpMgr>
{
    Q_OBJECT
public:
    ~httpMgr();
    void PostHttpReq(QUrl url, QJsonObject json, ReqId req_id, Modules mod);
private:
    friend class Singleton<httpMgr>;
    httpMgr();
    QNetworkAccessManager _manager;

private slots:
    void slot_http_finish(ReqId id, QString res, ErrorCodes err, Modules mod);
signals:
    void sig_http_finish(ReqId id, QString res, ErrorCodes err, Modules mod);
    void sig_reg_mod_finish(ReqId id, QString res, ErrorCodes err);
    void sig_reset_mod_finish(ReqId id, QString res, ErrorCodes err);
    void sig_login_mod_finish(ReqId id, QString res, ErrorCodes err);
};

#endif // HTTPMGR_H
