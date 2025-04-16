#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QModbusTcpClient>
#include <QTimer>
#include <QButtonGroup>
#include <QMap>
#include <QVariant>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onModbusStateChanged(QModbusDevice::State state);
    void connectDevice();
    void toggleManualMode(bool checked);
    void processManualPress(QAbstractButton *button);
    void processManualRelease();
    void sendManualCommand();
    void pollErrors();
    void pollAutoData();
    void showSettingsDialog();
    void updateConnectionStatus();
    void handleModbusResponse();

private:
    Ui::MainWindow *ui;
    QModbusTcpClient *m_modbusDevice;
    QTimer *m_manualTimer;
    QTimer *m_pollTimer;
    QButtonGroup *m_manualButtonGroup;
    
    struct Register {
        quint16 address;
        quint16 size;
        QVariant value;
    };
    
    QMap<QString, Register> m_regMap;

    void writeCoil(quint16 address, bool state);
    void writeRegister(const QString &regName, const QVariant &value);
    void readRegister(const QString &regName);
    quint32 floatToUint32(float value);
    float uint32ToFloat(quint32 raw);
};

#endif // MAINWINDOW_H
