/* === This file is part of Calamares - <http://github.com/calamares> ===
 *
 *   Copyright 2014, Aurélien Gâteau <agateau@kde.org>
 *   Copyright 2015, Teo Mrnjavac <teo@kde.org>
 *
 *   Calamares is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   Calamares is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with Calamares. If not, see <http://www.gnu.org/licenses/>.
 */
#include "gui/PartitionBarsView.h"

#include <core/PartitionModel.h>
#include <core/ColorUtils.h>

#include <kpmcore/core/device.h>

#include <utils/CalamaresUtilsGui.h>
#include <utils/Logger.h>


// Qt
#include <QDebug>
#include <QMouseEvent>
#include <QPainter>


static const int VIEW_HEIGHT = CalamaresUtils::defaultFontHeight() + 8;
static const int CORNER_RADIUS = 3;
static const int EXTENDED_PARTITION_MARGIN = 4;


PartitionBarsView::PartitionBarsView( QWidget* parent )
    : QAbstractItemView( parent )
    , m_hoveredIndex( QModelIndex() )
{
    setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Fixed );
    setFrameStyle( QFrame::NoFrame );
    setSelectionBehavior( QAbstractItemView::SelectRows );
    setSelectionMode( QAbstractItemView::SingleSelection );

    // Debug
    connect( this, &PartitionBarsView::clicked,
             this, [=]( const QModelIndex& index )
    {
        cDebug() << "Clicked row" << index.row();
    } );
    setMouseTracking( true );
}


PartitionBarsView::~PartitionBarsView()
{
}


QSize
PartitionBarsView::minimumSizeHint() const
{
    return sizeHint();
}


QSize
PartitionBarsView::sizeHint() const
{
    return QSize( -1, VIEW_HEIGHT );
}


void
PartitionBarsView::paintEvent( QPaintEvent* event )
{
    QPainter painter( viewport() );
    painter.fillRect( rect(), palette().window() );
    painter.setRenderHint( QPainter::Antialiasing );

    QRect partitionsRect = rect();
    partitionsRect.setHeight( VIEW_HEIGHT );

    painter.save();
    drawPartitions( &painter, partitionsRect, QModelIndex() );
    painter.restore();
}


void
PartitionBarsView::drawSection( QPainter* painter, const QRect& rect_, int x, int width, const QModelIndex& index )
{
    QColor color = index.isValid() ?
                   index.data( Qt::DecorationRole ).value< QColor >() :
                   ColorUtils::unknownDisklabelColor();
    bool isFreeSpace = index.isValid() ?
                       index.data( PartitionModel::IsFreeSpaceRole ).toBool() :
                       true;

    QRect rect = rect_;
    const int y = rect.y();
    const int height = rect.height();
    const int radius = qMax( 1, CORNER_RADIUS - ( VIEW_HEIGHT - height ) / 2 );
    painter->setClipRect( x, y, width, height );
    painter->translate( 0.5, 0.5 );

    rect.adjust( 0, 0, -1, -1 );


    QColor borderColor;
    if ( m_hoveredIndex.isValid() &&
         index == m_hoveredIndex )
    {
        borderColor = palette().highlight().color();
        painter->setBrush( color.lighter( 115 ) );
    }
    else
    {
        borderColor = color.darker();
        painter->setBrush( color );
    }

    painter->setPen( borderColor );

    painter->drawRoundedRect( rect, radius, radius );

    // Draw shade
    if ( !isFreeSpace )
        rect.adjust( 2, 2, -2, -2 );

    QLinearGradient gradient( 0, 0, 0, height / 2 );

    qreal c = isFreeSpace ? 0 : 1;
    gradient.setColorAt( 0, QColor::fromRgbF( c, c, c, 0.3 ) );
    gradient.setColorAt( 1, QColor::fromRgbF( c, c, c, 0 ) );

    painter->setPen( Qt::NoPen );
    painter->setBrush( gradient );
    painter->drawRoundedRect( rect, radius, radius );

    painter->translate( -0.5, -0.5 );
}


void
PartitionBarsView::drawPartitions( QPainter* painter, const QRect& rect, const QModelIndex& parent )
{
    PartitionModel* modl = qobject_cast< PartitionModel* >( model() );
    if ( !modl )
        return;
    const int count = modl->rowCount( parent );
    const int totalWidth = rect.width();
    qDebug() << "count:" << count << "totalWidth:" << totalWidth;

    auto pair = computeItemsVector( parent );
    QVector< PartitionBarsView::Item >& items = pair.first;
    qreal& total = pair.second;
    int x = rect.x();
    for ( int row = 0; row < count; ++row )
    {
        const auto& item = items[ row ];
        int width;
        if ( row < count - 1 )
            width = totalWidth * ( item.size / total );
        else
            // Make sure we fill the last pixel column
            width = rect.right() - x + 1;

        drawSection( painter, rect, x, width, item.index );
        if ( modl->hasChildren( item.index ) )
        {
            QRect subRect(
                x + EXTENDED_PARTITION_MARGIN,
                rect.y() + EXTENDED_PARTITION_MARGIN,
                width - 2 * EXTENDED_PARTITION_MARGIN,
                rect.height() - 2 * EXTENDED_PARTITION_MARGIN
            );
            drawPartitions( painter, subRect, item.index );
        }
        x += width;
    }

    if ( !count &&
         !modl->device()->partitionTable() ) // No disklabel or unknown
    {
        int width = rect.right() - rect.x() + 1;
        drawSection( painter, rect, rect.x(), width, QModelIndex() );
    }
}


QModelIndex
PartitionBarsView::indexAt( const QPoint& point ) const
{
    return indexAt( point, rect(), QModelIndex() );
}


QModelIndex
PartitionBarsView::indexAt( const QPoint &point,
                            const QRect &rect,
                            const QModelIndex& parent ) const
{
    PartitionModel* modl = qobject_cast< PartitionModel* >( model() );
    if ( !modl )
        return QModelIndex();
    const int count = modl->rowCount( parent );
    const int totalWidth = rect.width();

    auto pair = computeItemsVector( parent );
    QVector< PartitionBarsView::Item >& items = pair.first;
    qreal& total = pair.second;
    int x = rect.x();
    for ( int row = 0; row < count; ++row )
    {
        const auto& item = items[ row ];
        int width;
        if ( row < count - 1 )
            width = totalWidth * ( item.size / total );
        else
            // Make sure we fill the last pixel column
            width = rect.right() - x + 1;

        QRect thisItemRect( x, rect.y(), width, rect.height() );
        if ( thisItemRect.contains( point ) )
        {
            if ( modl->hasChildren( item.index ) )
            {
                QRect subRect(
                    x + EXTENDED_PARTITION_MARGIN,
                    rect.y() + EXTENDED_PARTITION_MARGIN,
                    width - 2 * EXTENDED_PARTITION_MARGIN,
                    rect.height() - 2 * EXTENDED_PARTITION_MARGIN
                );
                if ( subRect.contains( point ) )
                    return indexAt( point, subRect, item.index );
                return item.index;
            }
            else // contains but no children, we win
                return item.index;
        }
        x += width;
    }

    return QModelIndex();
}


QRect
PartitionBarsView::visualRect( const QModelIndex& index ) const
{
    return visualRect( index, rect(), QModelIndex() );
}


QRect
PartitionBarsView::visualRect( const QModelIndex& index,
                               const QRect& rect,
                               const QModelIndex& parent ) const
{
    PartitionModel* modl = qobject_cast< PartitionModel* >( model() );
    if ( !modl )
        return QRect();
    const int count = modl->rowCount( parent );
    const int totalWidth = rect.width();

    auto pair = computeItemsVector( parent );
    QVector< PartitionBarsView::Item >& items = pair.first;
    qreal& total = pair.second;
    int x = rect.x();
    for ( int row = 0; row < count; ++row )
    {
        const auto& item = items[ row ];
        int width;
        if ( row < count - 1 )
            width = totalWidth * ( item.size / total );
        else
            // Make sure we fill the last pixel column
            width = rect.right() - x + 1;

        QRect thisItemRect( x, rect.y(), width, rect.height() );
        if ( item.index == index )
            return thisItemRect;

        if ( modl->hasChildren( item.index ) &&
             index.parent() == item.index )
        {
            QRect subRect(
                x + EXTENDED_PARTITION_MARGIN,
                rect.y() + EXTENDED_PARTITION_MARGIN,
                width - 2 * EXTENDED_PARTITION_MARGIN,
                rect.height() - 2 * EXTENDED_PARTITION_MARGIN
            );
            QRect candidateVisualRect = visualRect( index, subRect, item.index );
            if ( !candidateVisualRect.isNull() )
                return candidateVisualRect;
        }

        x += width;
    }

    return QRect();
}


QRegion
PartitionBarsView::visualRegionForSelection( const QItemSelection& selection ) const
{
    return QRegion();
}


int
PartitionBarsView::horizontalOffset() const
{
    return 0;
}


int
PartitionBarsView::verticalOffset() const
{
    return 0;
}


void
PartitionBarsView::scrollTo( const QModelIndex& index, ScrollHint hint )
{
    Q_UNUSED( index )
    Q_UNUSED( hint )
}


QModelIndex
PartitionBarsView::moveCursor( CursorAction cursorAction, Qt::KeyboardModifiers modifiers )
{
    return QModelIndex();
}


bool
PartitionBarsView::isIndexHidden( const QModelIndex& index ) const
{
    return false;
}


void
PartitionBarsView::setSelection( const QRect& rect, QItemSelectionModel::SelectionFlags flags )
{
    selectionModel()->select( indexAt( rect.topLeft() ), flags );
    cDebug() << "selected items:" << selectedIndexes();
}


void
PartitionBarsView::mouseMoveEvent( QMouseEvent* event )
{
    QModelIndex candidateIndex = indexAt( event->pos() );
    QPersistentModelIndex oldHoveredIndex = m_hoveredIndex;
    if ( candidateIndex.isValid() )
    {
        m_hoveredIndex = candidateIndex;
    }
    else
        m_hoveredIndex = QModelIndex();

    if ( oldHoveredIndex != m_hoveredIndex )
    {
        viewport()->repaint();
    }
}


void
PartitionBarsView::leaveEvent( QEvent* event )
{
    if ( m_hoveredIndex.isValid() )
    {
        m_hoveredIndex = QModelIndex();
        viewport()->repaint();
    }
}


void
PartitionBarsView::updateGeometries()
{
    updateGeometry(); //get a new rect() for redrawing all the labels
}


QPair< QVector< PartitionBarsView::Item >, qreal >
PartitionBarsView::computeItemsVector( const QModelIndex& parent ) const
{
    const int count = model()->rowCount( parent );
    QVector< PartitionBarsView::Item > items( count );

    qreal total = 0;
    for ( int row = 0; row < count; ++row )
    {
        QModelIndex index = model()->index( row, 0, parent );
        qreal size = index.data( PartitionModel::SizeRole ).toLongLong();
        total += size;
        items[ row ] = { size, index };
    }

    return qMakePair( items, total );
}

