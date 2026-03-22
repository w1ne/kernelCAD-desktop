#pragma once
#include <QIcon>
#include <QPixmap>
#include <QString>

/// Programmatic icon factory -- generates all toolbar icons via QPainter.
/// No image files needed; icons are drawn as clean vector shapes,
/// bright on dark background (dark-theme friendly).
class IconFactory {
public:
    /// Create an icon by name. Size is the pixel dimension (square).
    static QIcon createIcon(const QString& name, int size = 32);

};
