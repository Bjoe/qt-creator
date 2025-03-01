// Copyright (C) 2016 Jochen Becher
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0 WITH Qt-GPL-exception-1.0

#pragma once

#include "qmt/infrastructure/qmt_global.h"

#include <QPointF>
#include <QSizeF>

namespace qmt {

class QMT_EXPORT ShapeValueF
{
public:
    enum Origin {
        OriginSmart,
        OriginTop,
        OriginLeft = OriginTop,
        OriginTopOrLeft = OriginTop,
        OriginBottom,
        OriginRight = OriginBottom,
        OriginBottomOrRight = OriginBottom,
        OriginCenter
    };

    enum Unit {
        UnitAbsolute,
        UnitRelative,
        UnitScaled,
        UnitPercentage
    };

    ShapeValueF()
        : m_value(0.0),
          m_unit(UnitRelative),
          m_origin(OriginSmart)
    {
    }

    explicit ShapeValueF(qreal value, Unit unit = UnitRelative, Origin origin = OriginSmart)
        : m_value(value),
          m_unit(unit),
          m_origin(origin)
    {
    }

    qreal value() const { return m_value; }
    void setValue(qreal value) { m_value = value; }
    Unit unit() const { return m_unit; }
    void setUnit(Unit unit) { m_unit = unit; }
    Origin origin() const { return m_origin; }
    void setOrigin(Origin origin) { m_origin = origin; }

    qreal mapTo(qreal origin, qreal size) const;
    qreal mapScaledTo(qreal scaledOrigin, qreal originalSize, qreal actualSize) const;
    qreal mapScaledTo(qreal scaledOrigin, qreal originalSize, qreal baseSize,
                      qreal actualSize) const;

private:
    qreal m_value = 0.0;
    Unit m_unit = UnitRelative;
    Origin m_origin = OriginSmart;
};

class QMT_EXPORT ShapePointF
{
public:
    ShapePointF() = default;

    ShapePointF(const ShapeValueF &x, const ShapeValueF &y)
        : m_x(x),
          m_y(y)
    {
    }

    ShapeValueF x() const { return m_x; }
    void setX(const ShapeValueF &x) { m_x = x; }
    ShapeValueF y() const { return m_y; }
    void setY(const ShapeValueF &y) { m_y = y; }

    QPointF mapTo(const QPointF &origin, const QSizeF &size) const;
    QPointF mapScaledTo(const QPointF &scaledOrigin, const QSizeF &originalSize,
                        const QSizeF &actualSize) const;
    QPointF mapScaledTo(const QPointF &scaledOrigin, const QSizeF &originalSize,
                        const QSizeF &baseSize, const QSizeF &actualSize) const;

private:
    ShapeValueF m_x;
    ShapeValueF m_y;
};

class QMT_EXPORT ShapeSizeF
{
public:
    ShapeSizeF() = default;

    ShapeSizeF(const ShapeValueF &width, const ShapeValueF &height)
        : m_width(width),
          m_height(height)
    {
    }

    ShapeValueF width() const { return m_width; }
    void setWidth(const ShapeValueF &width) { m_width = width; }
    ShapeValueF height() const { return m_height; }
    void setHeight(const ShapeValueF &height) { m_height = height; }

    QSizeF mapTo(const QPointF &origin, const QSizeF &size) const;
    QSizeF mapScaledTo(const QPointF &scaledOrigin, const QSizeF &originalSize,
                       const QSizeF &actualSize) const;
    QSizeF mapScaledTo(const QPointF &scaledOrigin, const QSizeF &originalSize,
                       const QSizeF &baseSize, const QSizeF &actualSize) const;

private:
    ShapeValueF m_width;
    ShapeValueF m_height;
};

} // namespace qmt
