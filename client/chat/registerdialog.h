#ifndef REGISTERDIALOG_H
#define REGISTERDIALOG_H

#include <QDialog>
#include "global.h"

namespace Ui {
class RegisterDialog;
}

class RegisterDialog : public QDialog
{
    Q_OBJECT

public:
    explicit RegisterDialog(QWidget *parent = nullptr);
    ~RegisterDialog();

private slots:
    void on_verified_get_btn_clicked();
    void slot_reg_mode_finish(ReqId id, QString res, ErrorCodes err);

    void on_sure_btn_clicked();

    void on_return_btn_clicked();

    void on_cancel_btn_clicked();

private:
    void initHttpHandlers();
    void showTip(QString str, bool b_ok);
    void AddTipErr(TipErr te, QString tips);
    void DelTipErr(TipErr te);
    bool checkUserValid();
    bool checkEmailValid();
    bool checkPassValid();
    bool checkVarifyValid();
    bool checkConfirmValid();
    void ChangeTipPage();
    QMap<TipErr, QString> _tip_errs;
    Ui::RegisterDialog *ui;
    QMap<ReqId, std::function<void(const QJsonObject&)>> _handlers; // 策略模式??

    QTimer * _countdown_timer;
    int _countdown;

signals:
    void sigSwitchLogin();
};

#endif // REGISTERDIALOG_H
