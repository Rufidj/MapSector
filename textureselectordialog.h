#ifndef TEXTURESELECTORDIALOG_H
#define TEXTURESELECTORDIALOG_H

#include <QDialog>
#include <QListWidget>
#include "MapStructures.h"

class TextureSelectorDialog : public QDialog {
    Q_OBJECT
public:
    TextureSelectorDialog(QWidget *parent = nullptr);
    void setTextures(const QVector<TextureEntry> &textures);  // QVector en lugar de std::vector
    int selectedTextureId() const { return m_selectedTextureId; }

signals:
    void textureSelected(int textureId);

private slots:
    void onItemDoubleClicked(QListWidgetItem *item);

private:
    QListWidget *listWidget;
    QVector<TextureEntry> textures;  // QVector en lugar de std::vector
    int m_selectedTextureId = 0;
};

#endif
