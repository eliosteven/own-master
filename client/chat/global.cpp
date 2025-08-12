#include "global.h"

QString gate_url_prefix="";


std::function<void(QWidget*)> repolish = [](QWidget* w){
    w->style()->unpolish(w);
    w->style()->polish(w);
};

std::function<QString(QString)> md5Encrypt  = [](QString input){
    QByteArray byteArray = input.toUtf8(); // 转为字节数组
    QByteArray hash = QCryptographicHash::hash(byteArray, QCryptographicHash::Md5); // 计算MD5
    return QString(hash.toHex()); // 转16进制字符串返回
};

