#include "textureselectordialog.h"
#include <QVBoxLayout>

TextureSelectorDialog::TextureSelectorDialog(QWidget *parent) : QDialog(parent) {
    listWidget = new QListWidget(this);
    listWidget->setViewMode(QListWidget::IconMode);
    listWidget->setIconSize(QSize(64, 64));
    listWidget->setResizeMode(QListWidget::Adjust);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(listWidget);
    setLayout(layout);

    connect(listWidget, &QListWidget::itemDoubleClicked, this, &TextureSelectorDialog::onItemDoubleClicked);
}

void TextureSelectorDialog::setTextures(const QVector<TextureEntry> &tex) {
    textures = tex;
    listWidget->clear();

    for (const TextureEntry &entry : textures) {
        QListWidgetItem *item = new QListWidgetItem();
        if (!entry.pixmap.isNull()) {
            QIcon icon(entry.pixmap.scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            item->setIcon(icon);
        }
        item->setText(QString("%1: %2").arg(entry.id).arg(entry.filename));
        item->setData(Qt::UserRole, entry.id);
        listWidget->addItem(item);
    }
}

void TextureSelectorDialog::onItemDoubleClicked(QListWidgetItem *item) {
    m_selectedTextureId = item->data(Qt::UserRole).toInt();
    emit textureSelected(m_selectedTextureId);
    accept();
}
