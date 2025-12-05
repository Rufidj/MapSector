#include "MapStructures.h"
#include <QFile>
#include <QMessageBox>

bool ModernMap::saveToWLD(const QString &filename) {
    FILE *f = fopen(filename.toUtf8().constData(), "wb");
    if (!f) return false;

    // Escribir header WLD exacto como divmap3d
    const char header[8] = {'w', 'l', 'd', 0x1a, 0x0d, 0x0a, 0x01, 0x00};
    fwrite(header, 8, 1, f);

    // Calcular tamaño total - 548 bytes de paths + datos
    int total = 548 + 4 + points.size() * sizeof(ModernPoint)
                + 4 + walls.size() * sizeof(ModernWall)
                + 4 + regions.size() * sizeof(ModernRegion)
                + 4 + 0 * sizeof(int) + 4; // flags = 0

    fwrite(&total, 4, 1, f);

    // Escribir paths (vacíos por ahora)
    char path[256] = {0};
    char name[16] = {0};
    fwrite(path, 256, 1, f);           // m3d_path
    fwrite(name, 16, 1, f);            // m3d_name
    int numero = 0;
    fwrite(&numero, 4, 1, f);          // numero
    fwrite(path, 256, 1, f);           // fpg_path
    fwrite(name, 16, 1, f);            // fpg_name

    // Escribir puntos
    int32_t count = points.size();
    fwrite(&count, 4, 1, f);
    for (const ModernPoint &p : points) {
        p.writeToWLD(f);
    }

    // Escribir paredes
    count = walls.size();
    fwrite(&count, 4, 1, f);
    for (const ModernWall &w : walls) {
        w.writeToWLD(f);
    }

    // Escribir regiones
    count = regions.size();
    fwrite(&count, 4, 1, f);
    for (const ModernRegion &r : regions) {
        r.writeToWLD(f);
    }

    // Escribir flags (0)
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
    fread(path, 256, 1, f);
    fread(name, 16, 1, f);

    // Leer puntos
    int32_t count;
    fread(&count, 4, 1, f);
    points.resize(count);
    for (int i = 0; i < count; i++) {
        points[i].readFromWLD(f);
    }

    // Leer paredes
    fread(&count, 4, 1, f);
    walls.resize(count);
    for (int i = 0; i < count; i++) {
        walls[i].readFromWLD(f);
    }

    // Leer regiones
    fread(&count, 4, 1, f);
    regions.resize(count);
    for (int i = 0; i < count; i++) {
        regions[i].readFromWLD(f);
    }

    // Leer flags
    fread(&count, 4, 1, f);

    // Leer fondo
    int fondo;
    fread(&fondo, 4, 1, f);

    fclose(f);
    return true;
}
