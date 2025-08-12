#include "mainwindow.h"

#include <QApplication>
#include <QFile>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QFile qss(":/style/stylesheet.qss");
    if(qss.open(QFile::ReadOnly)){
        qDebug("Open success");
        QString style = QLatin1String(qss.readAll());
        a.setStyleSheet(style);
        qss.close();
    }else{
        qDebug("Open failed");
    }

    QString filename = "config.ini";
    QString app_path = QCoreApplication::applicationDirPath();
    QString config_path = QDir::toNativeSeparators(app_path + QDir::separator() + filename);
    qDebug() << "config_path =" << config_path
             << ", exists =" << QFileInfo::exists(config_path);
    QSettings settings(config_path, QSettings::IniFormat);settings.sync();
    qDebug() << "QSettings status =" << settings.status(); // 0=NoError

    // 列出它实际看到的组/键，快速排除大小写/BOM等问题
    qDebug() << "groups:" << settings.childGroups();
    settings.beginGroup("GateServer");
    qDebug() << "GateServer keys:" << settings.childKeys();
    qDebug() << "GateServer/host =" << settings.value("host").toString();
    qDebug() << "GateServer/port =" << settings.value("port").toString();
    settings.endGroup();
    QString gate_host = settings.value("GateServer/host").toString();
    QString gate_port = settings.value("GateServer/port").toString();
    gate_url_prefix = "http://" + gate_host + ":" + gate_port;

    MainWindow w;
    w.show();
    return a.exec();
}
