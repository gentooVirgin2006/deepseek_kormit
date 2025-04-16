#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>

namespace Ui {
class SettingsDialog;
}

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget *parent = nullptr);
    ~SettingsDialog();
    QString ipAddress() const;

private slots:
    void saveSettings();

private:
    Ui::SettingsDialog *ui;
    void loadSettings();
};

#endif // SETTINGSDIALOG_H
