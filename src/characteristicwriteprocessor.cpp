#include "characteristicwriteprocessor.h"
#include <QSettings>

CharacteristicWriteProcessor::CharacteristicWriteProcessor(double bikeResistanceGain, uint8_t bikeResistanceOffset,
                                                           bluetoothdevice *bike, QObject *parent)
    : QObject(parent), bikeResistanceOffset(bikeResistanceOffset), bikeResistanceGain(bikeResistanceGain), Bike(bike) {}

void CharacteristicWriteProcessor::changePower(uint16_t power) { Bike->changePower(power); }

void CharacteristicWriteProcessor::changeSlope(int16_t iresistance, uint8_t crr, uint8_t cw) {
    bluetoothdevice::BLUETOOTH_TYPE dt = Bike->deviceType();
    QSettings settings;
    bool force_resistance =
        settings.value(QZSettings::virtualbike_forceresistance, QZSettings::default_virtualbike_forceresistance)
            .toBool();
    bool erg_mode = settings.value(QZSettings::zwift_erg, QZSettings::default_zwift_erg).toBool();
    bool zwift_negative_inclination_x2 =
        settings.value(QZSettings::zwift_negative_inclination_x2, QZSettings::default_zwift_negative_inclination_x2)
            .toBool();
    double offset =
        settings.value(QZSettings::zwift_inclination_offset, QZSettings::default_zwift_inclination_offset).toDouble();
    double gain =
        settings.value(QZSettings::zwift_inclination_gain, QZSettings::default_zwift_inclination_gain).toDouble();
    double CRRGain = settings.value(QZSettings::CRRGain, QZSettings::default_CRRGain).toDouble();
    double CWGain = settings.value(QZSettings::CWGain, QZSettings::default_CWGain).toDouble();

    qDebug() << QStringLiteral("new requested resistance zwift erg grade ") + QString::number(iresistance) +
                    QStringLiteral(" enabled ") + force_resistance;
    double resistance = ((double)iresistance * 1.5) / 100.0;
    qDebug() << QStringLiteral("calculated erg grade ") + QString::number(resistance);

    double grade = ((iresistance / 100.0) * gain) + offset;

    /*
    Surface	Road Crr	MTB Crr	Gravel Crr (Namebrand)	Zwift Gravel Crr
    Pavement	.004	.01	.008	.008
        Sand	.004	.01	.008	.008
        Brick	.0055	.01	.008	.008
        Wood	.0065	.01	.008	.008
        Cobbles	.0065	.01	.008	.008
        Ice/Snow	.0075	.014	.018	.018
        Dirt	.025	.014	.016	.018
        Grass	 	.042
    */
    const double fCRR = crr / 10000.0;
    const double CRR_offset = ((crr - 40) * 0.05) * CRRGain;

    const double fCW = cw / 100.0;
    const double CW_offset = ((crr - 40) * 0.05) * CWGain;

    qDebug() << "changeSlope CRR = " << fCRR << CRR_offset << "CW = " << fCW;

    if (dt == bluetoothdevice::BIKE) {

        // if the bike doesn't have the inclination by hardware, i'm simulating inclination with the value received
        // form Zwift
        if (!((bike *)Bike)->inclinationAvailableByHardware())
            Bike->setInclination(grade + CRR_offset + CW_offset);

        if (iresistance >= 0 || !zwift_negative_inclination_x2)
            emit changeInclination(grade, ((qTan(qDegreesToRadians(iresistance / 100.0)) * 100.0) * gain) + offset);
        else
            emit changeInclination((((iresistance / 100.0) * 2.0) * gain) + offset,
                                   (((qTan(qDegreesToRadians(iresistance / 100.0)) * 100.0) * 2.0) * gain) + offset);

        if (force_resistance && !erg_mode) {
            // same on the training program
            Bike->changeResistance((resistance_t)(round(resistance * bikeResistanceGain)) + bikeResistanceOffset + 1 +
                                   CRR_offset + CW_offset); // resistance start from 1
        }
    } else if (dt == bluetoothdevice::TREADMILL) {
        emit changeInclination(((iresistance / 100.0) * gain) + offset,
                               ((qTan(qDegreesToRadians(iresistance / 100.0)) * 100.0) * gain) + offset);
    } else if (dt == bluetoothdevice::ELLIPTICAL) {
        emit changeInclination(((iresistance / 100.0) * gain) + offset,
                               ((qTan(qDegreesToRadians(iresistance / 100.0)) * 100.0) * gain) + offset);

        if (!((elliptical *)Bike)->inclinationAvailableByHardware()) {
            if (force_resistance && !erg_mode) {
                // same on the training program
                Bike->changeResistance((resistance_t)(round(resistance * bikeResistanceGain)) + bikeResistanceOffset +
                                       1 + CRR_offset + CW_offset); // resistance start from 1
            }
        }
    }
    emit slopeChanged();
}
