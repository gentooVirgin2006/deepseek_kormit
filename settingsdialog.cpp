#include "settingsdialog.h"
#include "ui_settingsdialog.h"
#include <QSettings>
#include <QRegExpValidator>

SettingsDialog::SettingsDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::SettingsDialog)
{
    ui->setupUi(this);
    QRegExp ipRegex("^((25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\.){3}(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$");
    ui->lineEditIP->setValidator(new QRegExpValidator(ipRegex, this));
    connect(ui->btnSave, &QPushButton::clicked, this, &SettingsDialog::saveSettings);
    connect(ui->btnCancel, &QPushButton::clicked, this, &SettingsDialog::reject);
    loadSettings();
}

SettingsDialog::~SettingsDialog()
{
    delete ui;
}

QString SettingsDialog::ipAddress() const
{
    return ui->lineEditIP->text();
}

void SettingsDialog::loadSettings()
{
    QSettings settings;
    ui->lineEditIP->setText(settings.value("Modbus/IP", "127.0.0.1").toString());
}

void SettingsDialog::saveSettings()
{
    QSettings settings;
    settings.setValue("Modbus/IP", ui->lineEditIP->text());
    accept();
}
