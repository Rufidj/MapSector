#ifndef WALLDIALOG_H
#define WALLDIALOG_H

#include <QDialog>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>

class WallDialog : public QDialog
{
    Q_OBJECT

public:
    WallDialog(const QStringList &sectorNames, QWidget *parent = nullptr);

    int getSector1Id() const { return sector1Combo->currentData().toInt(); }
    int getSector2Id() const { return sector2Combo->currentData().toInt(); }
    int getTextureId() const { return textureCombo->currentData().toInt(); }

private:
    QComboBox *sector1Combo;
    QComboBox *sector2Combo;
    QComboBox *textureCombo;
};

#endif // WALLDIALOG_H
