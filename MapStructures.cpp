#include "MapStructures.h"
#include "divmap3d.hpp"
#include <QFileInfo>
#include <QString>
#include <QByteArray>
#include <QFile>
#include <cstring>
#include <cstdio>

#pragma pack(push, 1)

// Verificar que las estructuras tengan los tamaños correctos
static_assert(sizeof(tpoint) == 16, "tpoint size mismatch");
static_assert(sizeof(twall) == 40, "twall size mismatch");
static_assert(sizeof(tregion) == 28, "tregion size mismatch");
static_assert(sizeof(tflag) == 16, "tflag size mismatch");

bool ModernMap::saveToWLD(const QString &filename) {
    FILE *f = fopen(filename.toUtf8().constData(), "wb");
    if (!f) return false;

    // *** DEBUG: Verificar tamaños ***
    printf("sizeof(tpoint) = %d (debe ser 16)\n", sizeof(tpoint));
    printf("sizeof(twall) = %d (debe ser 40)\n", sizeof(twall));
    printf("sizeof(tregion) = %d (debe ser 32)\n", sizeof(tregion));
    printf("sizeof(tflag) = %d (debe ser 16)\n", sizeof(tflag));

    // Header exacto como divmap3d
    fwrite("wld\x1a\x0d\x0a\x01\x00", 8, 1, f);

    // Calcular tamaño total exacto como divmap3d
    int total = 548 + 4 + points.size() * sizeof(tpoint)
                + 4 + walls.size() * sizeof(twall)
                + 4 + regions.size() * sizeof(tregion)
                + 4 + 0 * sizeof(tflag) + 4;
    fwrite(&total, 4, 1, f);

    // Paths exactamente como divmap3d
    char path[256] = {0};
    char name[16] = {0};

    // m3d_path - ruta completa del WLD
    QByteArray wldPath = filename.toUtf8();
    memset(path, 0, 256);
    memcpy(path, wldPath.constData(), qMin(wldPath.size(), 255));
    fwrite(path, 256, 1, f);

    // m3d_name - nombre del archivo WLD
    QFileInfo wldInfo(filename);
    QString wldName = wldInfo.fileName();
    QByteArray wldNameBytes = wldName.toUtf8();
    memset(name, 0, 16);
    memcpy(name, wldNameBytes.constData(), qMin(wldNameBytes.size(), 15));
    fwrite(name, 16, 1, f);

    // numero - siempre 0
    int numero = 0;
    fwrite(&numero, 4, 1, f);

    // fpg_path y fpg_name
    if (!textures.isEmpty()) {
        QString fpgPath = textures[0].filename;
        QByteArray fpgBytes = fpgPath.toUtf8();

        memset(path, 0, 256);
        memcpy(path, fpgBytes.constData(), qMin(fpgBytes.size(), 255));
        fwrite(path, 256, 1, f);

        QFileInfo fpgInfo(fpgPath);
        QString fpgName = fpgInfo.fileName();
        QByteArray fpgNameBytes = fpgName.toUtf8();
        memset(name, 0, 16);
        memcpy(name, fpgNameBytes.constData(), qMin(fpgNameBytes.size(), 15));
        fwrite(name, 16, 1, f);
    } else {
        fwrite(path, 256, 1, f);  // fpg_path vacío
        fwrite(name, 16, 1, f);   // fpg_name vacío
    }

    // Escribir puntos como tpoint
    int32_t count = points.size();
    fwrite(&count, 4, 1, f);
    for (const ModernPoint &p : points) {
        tpoint tp = {p.active, p.x, p.y, p.links};
        fwrite(&tp, sizeof(tpoint), 1, f);
    }

    // Escribir paredes como twall
    count = walls.size();
    fwrite(&count, 4, 1, f);
    for (const ModernWall &w : walls) {
        twall tw = {w.active, w.type, w.p1, w.p2, w.front_region, w.back_region,
                    w.texture, w.texture_top, w.texture_bot, w.fade};
        fwrite(&tw, sizeof(twall), 1, f);
    }

    // Escribir regiones como tregion
    count = regions.size();
    fwrite(&count, 4, 1, f);
    for (const ModernRegion &r : regions) {
        tregion tr = {r.active, r.type, r.floor_height, r.ceiling_height,
                      r.floor_tex, r.ceil_tex, r.fade};
        fwrite(&tr, sizeof(tregion), 1, f);
    }

    // Escribir flags (siempre 0)
    count = 0;
    fwrite(&count, 4, 1, f);

    // Escribir fondo
    int fondo = 0;
    fwrite(&fondo, 4, 1, f);

    fclose(f);
    return true;
}

bool ModernMap::loadFromWLD(const QString &filename) {
    FILE *f = fopen(filename.toUtf8().constData(), "rb");
    if (!f) return false;

    // Validar header
    char header[8];
    fread(header, 8, 1, f);
    if (strncmp(header, "wld\x1a\x0d\x0a\x01\x00", 8) != 0) {
        fclose(f);
        return false;
    }

    // Leer tamaño total
    int total;
    fread(&total, 4, 1, f);

    // Leer paths
    char path[256], name[16];
    fread(path, 256, 1, f);
    fread(name, 16, 1, f);
    int numero;
    fread(&numero, 4, 1, f);

    // Leer fpg_path y fpg_name
    char fpg_path[256], fpg_name[16];
    fread(fpg_path, 256, 1, f);
    fread(fpg_name, 16, 1, f);

    // Leer puntos como tpoint
    int32_t count;
    fread(&count, 4, 1, f);
    points.resize(count);
    for (int i = 0; i < count; i++) {
        tpoint tp;
        fread(&tp, sizeof(tpoint), 1, f);
        points[i].active = tp.active;
        points[i].x = tp.x;
        points[i].y = tp.y;
        points[i].links = tp.links;
    }

    // Leer paredes como twall
    fread(&count, 4, 1, f);
    walls.resize(count);
    for (int i = 0; i < count; i++) {
        twall tw;
        fread(&tw, sizeof(twall), 1, f);
        walls[i].active = tw.active;
        walls[i].type = tw.type;
        walls[i].p1 = tw.p1;
        walls[i].p2 = tw.p2;
        walls[i].front_region = tw.front_region;
        walls[i].back_region = tw.back_region;
        walls[i].texture = tw.texture;
        walls[i].texture_top = tw.texture_top;
        walls[i].texture_bot = tw.texture_bot;
        walls[i].fade = tw.fade;
    }

    // Leer regiones como tregion
    fread(&count, 4, 1, f);
    regions.resize(count);
    for (int i = 0; i < count; i++) {
        tregion tr;
        fread(&tr, sizeof(tregion), 1, f);
        regions[i].active = tr.active;
        regions[i].type = tr.type;
        regions[i].floor_height = tr.floor_height;
        regions[i].ceiling_height = tr.ceil_height;
        regions[i].floor_tex = tr.floor_tex;
        regions[i].ceil_tex = tr.ceil_tex;
        regions[i].fade = tr.fade;
    }

    // Leer flags
    fread(&count, 4, 1, f);

    // Leer fondo
    int fondo;
    fread(&fondo, 4, 1, f);

    fclose(f);
    return true;
}

#pragma pack(pop)
