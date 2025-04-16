#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "settingsdialog.h"
#include <QModbusDataUnit>
#include <QMessageBox>
#include <QSettings>
#include <QDebug>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow),
      m_modbusDevice(new QModbusTcpClient(this)),
      m_manualTimer(new QTimer(this)),
      m_pollTimer(new QTimer(this)),
      m_manualButtonGroup(new QButtonGroup(this))
{
    ui->setupUi(this);

    QSettings settings;
    m_modbusDevice->setConnectionParameter(QModbusDevice::NetworkPortParameter, 502);
    m_modbusDevice->setConnectionParameter(QModbusDevice::NetworkAddressParameter,
        settings.value("Modbus/IP", "127.0.0.1").toString());

    connect(m_modbusDevice, &QModbusClient::stateChanged,
            this, &MainWindow::onModbusStateChanged);

    m_regMap = {
        {"manual_commands", {0, 1, false}},
        {"errors", {1, 1, 0}},
        {"current_rack", {2, 1, 0}},
        {"current_weight", {5, 2, 0.0f}}
    };

    m_manualTimer->setInterval(100);
    connect(m_manualTimer, &QTimer::timeout, this, &MainWindow::sendManualCommand);

    m_manualButtonGroup->addButton(ui->btnClimbUp, 5);
    m_manualButtonGroup->addButton(ui->btnClimbDown, 6);
    m_manualButtonGroup->addButton(ui->btnShiftIn, 7);
    m_manualButtonGroup->addButton(ui->btnShiftOut, 8);
    
    connect(m_manualButtonGroup, QOverload<QAbstractButton*>::of(&QButtonGroup::buttonPressed),
            this, &MainWindow::processManualPress);
    connect(m_manualButtonGroup, QOverload<QAbstractButton*>::of(&QButtonGroup::buttonReleased),
            this, &MainWindow::processManualRelease);

    m_pollTimer->setInterval(1000);
    connect(m_pollTimer, &QTimer::timeout, this, [this](){
        pollErrors();
        pollAutoData();
    });

    connect(ui->btnConnect, &QPushButton::clicked, this, &MainWindow::connectDevice);
    connect(ui->btnManualMode, &QPushButton::toggled, this, &MainWindow::toggleManualMode);
    connect(ui->btnSettings, &QPushButton::clicked, this, &MainWindow::showSettingsDialog);

    updateConnectionStatus();
}

MainWindow::~MainWindow()
{
    if (m_modbusDevice)
        m_modbusDevice->disconnectDevice();
    delete ui;
}

void MainWindow::onModbusStateChanged(QModbusDevice::State state)
{
    Q_UNUSED(state)
    updateConnectionStatus();
}

void MainWindow::connectDevice()
{
    if(m_modbusDevice->state() != QModbusDevice::ConnectedState) {
        if(!m_modbusDevice->connectDevice()) {
            QMessageBox::critical(this, "Error", "Connection failed: " + m_modbusDevice->errorString());
        }
    } else {
        m_modbusDevice->disconnectDevice();
    }
}

void MainWindow::toggleManualMode(bool checked)
{
    ui->groupManual->setEnabled(checked);
    writeCoil(m_regMap["manual_commands"].address + 4, checked);
}

void MainWindow::processManualPress(QAbstractButton *button)
{
    quint16 address = m_manualButtonGroup->id(button);
    m_regMap["Manual_cmd"].address = address;
    m_manualTimer->start();
}

void MainWindow::processManualRelease()
{
    m_manualTimer->stop();
    writeCoil(m_regMap["Manual_cmd"].address, false);
}

void MainWindow::sendManualCommand()
{
    writeCoil(m_regMap["Manual_cmd"].address, true);
}

void MainWindow::pollErrors()
{
    readRegister("errors");
}

void MainWindow::pollAutoData()
{
    readRegister("current_rack");
    readRegister("current_weight");
}

void MainWindow::showSettingsDialog()
{
    SettingsDialog dlg(this);
    if(dlg.exec() == QDialog::Accepted) {
        QSettings settings;
        QString newIp = dlg.ipAddress();
        settings.setValue("Modbus/IP", newIp);
        m_modbusDevice->setConnectionParameter(
            QModbusDevice::NetworkAddressParameter, newIp);
        if(m_modbusDevice->state() == QModbusDevice::ConnectedState) {
            m_modbusDevice->disconnectDevice();
            connectDevice();
        }
    }
}

void MainWindow::updateConnectionStatus()
{
    const bool connected = m_modbusDevice->state() == QModbusDevice::ConnectedState;
    ui->lblStatus->setText(connected ? "Connected" : "Disconnected");
    ui->lblStatus->setStyleSheet(connected ? "color: green;" : "color: red;");
}

void MainWindow::writeCoil(quint16 address, bool state)
{
    if (!m_modbusDevice || m_modbusDevice->state() != QModbusDevice::ConnectedState)
        return;

    QModbusDataUnit request(QModbusDataUnit::Coils, address, 1);
    request.setValue(0, state);

    if (auto *reply = m_modbusDevice->sendWriteRequest(request, 1)) {
        connect(reply, &QModbusReply::finished, this, &MainWindow::handleModbusResponse);
    }
}

void MainWindow::writeRegister(const QString &regName, const QVariant &value)
{
    if (!m_regMap.contains(regName)) return;

    Register reg = m_regMap[regName];
    QModbusDataUnit request(QModbusDataUnit::HoldingRegisters, reg.address, reg.size);

    if (value.userType() == QMetaType::Float && reg.size == 2) {
        quint32 raw = floatToUint32(value.toFloat());
        request.setValue(0, raw >> 16);
        request.setValue(1, raw & 0xFFFF);
    } else {
        request.setValue(0, value.toUInt());
    }

    if (auto *reply = m_modbusDevice->sendWriteRequest(request, 1)) {
        connect(reply, &QModbusReply::finished, this, &MainWindow::handleModbusResponse);
    }
}

void MainWindow::readRegister(const QString &regName)
{
    if (!m_regMap.contains(regName)) return;

    Register reg = m_regMap[regName];
    QModbusDataUnit request(QModbusDataUnit::HoldingRegisters, reg.address, reg.size);

    if (auto *reply = m_modbusDevice->sendReadRequest(request, 1)) {
        connect(reply, &QModbusReply::finished, this, &MainWindow::handleModbusResponse);
    }
}

void MainWindow::handleModbusResponse()
{
    auto *reply = qobject_cast<QModbusReply*>(sender());
    if (!reply || reply->error() != QModbusDevice::NoError) {
        if (reply) reply->deleteLater();
        return;
    }

    const QModbusDataUnit data = reply->result();
    quint16 address = data.startAddress();

    if (address == m_regMap["current_rack"].address) {
        qint16 rack = static_cast<qint16>(data.value(0));
        ui->lblCurrentRackValue->setText(QString::number(rack));
    }
    else if (address == m_regMap["current_weight"].address && data.valueCount() >= 2) {
        quint32 raw = (data.value(0) << 16) | data.value(1);
        float weight = uint32ToFloat(raw);
        ui->lblWeightValue->setText(QString::number(weight, 'f', 2) + " kg");
    }

    reply->deleteLater();
}

quint32 MainWindow::floatToUint32(float value)
{
    quint32 raw;
    memcpy(&raw, &value, sizeof(float));
    return raw;
}

float MainWindow::uint32ToFloat(quint32 raw)
{
    float value;
    memcpy(&value, &raw, sizeof(float));
    return value;
}
