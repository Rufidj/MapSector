#include "WallDialog.h"

WallDialog::WallDialog(const QStringList &sectorNames, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Configurar Pared");

    QFormLayout *layout = new QFormLayout(this);

    // Combo para sector 1
    sector1Combo = new QComboBox(this);
    for (int i = 0; i < sectorNames.size(); i++) {
        sector1Combo->addItem(sectorNames[i], i);
    }
    layout->addRow("Sector 1:", sector1Combo);

    // Combo para sector 2 (0 = pared sólida)
    sector2Combo = new QComboBox(this);
    sector2Combo->addItem("Pared sólida (sin portal)", 0);
    for (int i = 0; i < sectorNames.size(); i++) {
        sector2Combo->addItem(sectorNames[i], i);
    }
    layout->addRow("Sector 2:", sector2Combo);

    // Combo para textura
    textureCombo = new QComboBox(this);
    textureCombo->addItem("Sin textura", 0);
    layout->addRow("Textura:", textureCombo);

    // Botones OK/Cancel
    QDialogButtonBox *buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addRow(buttonBox);
}
