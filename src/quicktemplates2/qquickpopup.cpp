/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Quick Templates 2 module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qquickpopup_p.h"
#include "qquickpopup_p_p.h"
#include "qquickapplicationwindow_p.h"
#include "qquickoverlay_p.h"
#include "qquickcontrol_p_p.h"

#include <QtQml/qqmlinfo.h>
#include <QtQuick/qquickitem.h>
#include <QtQuick/private/qquicktransition_p.h>
#include <QtQuick/private/qquickitem_p.h>

QT_BEGIN_NAMESPACE

/*!
    \qmltype Popup
    \inherits QtObject
    \instantiates QQuickPopup
    \inqmlmodule Qt.labs.controls
    \ingroup qtquickcontrols2-popups
    \brief The base type of popup-like user interface controls.

    Popup is the base type of popup-like user interface controls. It can be
    used with Window or ApplicationWindow.

    \qml
    import QtQuick.Window 2.2
    import Qt.labs.controls 1.0

    Window {
        id: window
        width: 400
        height: 400
        visible: true

        Button {
            text: "Open"
            onClicked: popup.open()
        }

        Popup {
            id: popup
            x: 100
            y: 100
            width: 200
            height: 300
            modal: true
            focus: true
            closePolicy: Popup.OnEscape | Popup.OnPressOutsideParent
        }
    }
    \endqml

    In order to ensure that a popup is displayed above other items in the
    scene, it is recommended to use ApplicationWindow. ApplicationWindow also
    provides background dimming effects.

    \labs
*/

static const QQuickItemPrivate::ChangeTypes AncestorChangeTypes = QQuickItemPrivate::Geometry
                                                                  | QQuickItemPrivate::Parent
                                                                  | QQuickItemPrivate::Children;

static const QQuickItemPrivate::ChangeTypes ItemChangeTypes = QQuickItemPrivate::Geometry
                                                             | QQuickItemPrivate::Parent
                                                             | QQuickItemPrivate::Destroyed;

QQuickPopupPrivate::QQuickPopupPrivate()
    : QObjectPrivate()
    , focus(false)
    , modal(false)
    , visible(false)
    , complete(false)
    , hasTopMargin(false)
    , hasLeftMargin(false)
    , hasRightMargin(false)
    , hasBottomMargin(false)
    , x(0)
    , y(0)
    , margins(0)
    , topMargin(0)
    , leftMargin(0)
    , rightMargin(0)
    , bottomMargin(0)
    , contentWidth(0)
    , contentHeight(0)
    , closePolicy(QQuickPopup::OnEscape | QQuickPopup::OnPressOutside)
    , parentItem(nullptr)
    , enter(nullptr)
    , exit(nullptr)
    , popupItem(nullptr)
    , positioner(this)
    , transitionManager(this)
{
}

void QQuickPopupPrivate::init()
{
    Q_Q(QQuickPopup);
    popupItem = new QQuickPopupItem(q);
    q->setParentItem(qobject_cast<QQuickItem *>(parent));
    QObject::connect(popupItem, &QQuickControl::paddingChanged, q, &QQuickPopup::paddingChanged);
}

bool QQuickPopupPrivate::tryClose(QQuickItem *item, QMouseEvent *event)
{
    Q_Q(QQuickPopup);
    const bool isPress = event->type() == QEvent::MouseButtonPress;
    const bool onOutside = closePolicy.testFlag(isPress ? QQuickPopup::OnPressOutside : QQuickPopup::OnReleaseOutside);
    const bool onOutsideParent = closePolicy.testFlag(isPress ? QQuickPopup::OnPressOutsideParent : QQuickPopup::OnReleaseOutsideParent);
    if (onOutside || onOutsideParent) {
        if (!popupItem->contains(item->mapToItem(popupItem, event->pos()))) {
            if (!onOutsideParent || !parentItem || !parentItem->contains(item->mapToItem(parentItem, event->pos()))) {
                q->close();
                return true;
            }
        }
    }
    return false;
}

void QQuickPopupPrivate::prepareEnterTransition(bool notify)
{
    Q_Q(QQuickPopup);
    QQuickWindow *quickWindow = q->window();
    if (!quickWindow) {
        qmlInfo(q) << "cannot find any window to open popup in.";
        return;
    }

    QQuickApplicationWindow *applicationWindow = qobject_cast<QQuickApplicationWindow*>(quickWindow);
    if (!applicationWindow) {
        quickWindow->installEventFilter(q);
        popupItem->setZ(10001); // DefaultWindowDecoration+1
        popupItem->setParentItem(quickWindow->contentItem());
    } else {
        popupItem->setParentItem(applicationWindow->overlay());
    }

    if (notify)
        emit q->aboutToShow();
    visible = notify;
    popupItem->setVisible(true);
    positioner.setParentItem(parentItem);
    emit q->visibleChanged();
}

void QQuickPopupPrivate::prepareExitTransition()
{
    Q_Q(QQuickPopup);
    QQuickWindow *quickWindow = q->window();
    if (quickWindow && !qobject_cast<QQuickApplicationWindow *>(quickWindow))
        quickWindow->removeEventFilter(q);
    if (focus)
        popupItem->setFocus(false);
    emit q->aboutToHide();
}

void QQuickPopupPrivate::finalizeEnterTransition()
{
    if (focus)
        popupItem->setFocus(true);
}

void QQuickPopupPrivate::finalizeExitTransition(bool hide)
{
    Q_Q(QQuickPopup);
    positioner.setParentItem(nullptr);
    if (hide) {
        popupItem->setParentItem(nullptr);
        popupItem->setVisible(false);
    }

    visible = false;
    emit q->visibleChanged();
}

QMarginsF QQuickPopupPrivate::getMargins() const
{
    Q_Q(const QQuickPopup);
    return QMarginsF(q->leftMargin(), q->topMargin(), q->rightMargin(), q->bottomMargin());
}

void QQuickPopupPrivate::setTopMargin(qreal value, bool reset)
{
    Q_Q(QQuickPopup);
    qreal oldMargin = q->topMargin();
    topMargin = value;
    hasTopMargin = !reset;
    if ((!reset && !qFuzzyCompare(oldMargin, value)) || (reset && !qFuzzyCompare(oldMargin, margins))) {
        emit q->topMarginChanged();
        q->marginsChange(QMarginsF(leftMargin, topMargin, rightMargin, bottomMargin),
                         QMarginsF(leftMargin, oldMargin, rightMargin, bottomMargin));
    }
}

void QQuickPopupPrivate::setLeftMargin(qreal value, bool reset)
{
    Q_Q(QQuickPopup);
    qreal oldMargin = q->leftMargin();
    leftMargin = value;
    hasLeftMargin = !reset;
    if ((!reset && !qFuzzyCompare(oldMargin, value)) || (reset && !qFuzzyCompare(oldMargin, margins))) {
        emit q->leftMarginChanged();
        q->marginsChange(QMarginsF(leftMargin, topMargin, rightMargin, bottomMargin),
                         QMarginsF(oldMargin, topMargin, rightMargin, bottomMargin));
    }
}

void QQuickPopupPrivate::setRightMargin(qreal value, bool reset)
{
    Q_Q(QQuickPopup);
    qreal oldMargin = q->rightMargin();
    rightMargin = value;
    hasRightMargin = !reset;
    if ((!reset && !qFuzzyCompare(oldMargin, value)) || (reset && !qFuzzyCompare(oldMargin, margins))) {
        emit q->rightMarginChanged();
        q->marginsChange(QMarginsF(leftMargin, topMargin, rightMargin, bottomMargin),
                         QMarginsF(leftMargin, topMargin, oldMargin, bottomMargin));
    }
}

void QQuickPopupPrivate::setBottomMargin(qreal value, bool reset)
{
    Q_Q(QQuickPopup);
    qreal oldMargin = q->bottomMargin();
    bottomMargin = value;
    hasBottomMargin = !reset;
    if ((!reset && !qFuzzyCompare(oldMargin, value)) || (reset && !qFuzzyCompare(oldMargin, margins))) {
        emit q->bottomMarginChanged();
        q->marginsChange(QMarginsF(leftMargin, topMargin, rightMargin, bottomMargin),
                         QMarginsF(leftMargin, topMargin, rightMargin, oldMargin));
    }
}

class QQuickPopupItemPrivate : public QQuickControlPrivate
{
    Q_DECLARE_PUBLIC(QQuickPopupItem)

public:
    QQuickPopupItemPrivate(QQuickPopup *popup);

    void implicitWidthChanged() override;
    void implicitHeightChanged() override;

    void resolveFont() override;

    QQuickPopup *popup;
};

QQuickPopupItemPrivate::QQuickPopupItemPrivate(QQuickPopup *popup) : popup(popup)
{
    isTabFence = true;
}

void QQuickPopupItemPrivate::implicitWidthChanged()
{
    QQuickControlPrivate::implicitWidthChanged();
    emit popup->implicitWidthChanged();
}

void QQuickPopupItemPrivate::implicitHeightChanged()
{
    QQuickControlPrivate::implicitHeightChanged();
    emit popup->implicitHeightChanged();
}

void QQuickPopupItemPrivate::resolveFont()
{
    if (QQuickApplicationWindow *window = qobject_cast<QQuickApplicationWindow *>(popup->window()))
        inheritFont(window->font());
}

QQuickPopupItem::QQuickPopupItem(QQuickPopup *popup) :
    QQuickControl(*(new QQuickPopupItemPrivate(popup)), nullptr)
{
    setParent(popup);
    setVisible(false);
    setFlag(ItemIsFocusScope);
    setAcceptedMouseButtons(Qt::AllButtons);
}

bool QQuickPopupItem::childMouseEventFilter(QQuickItem *child, QEvent *event)
{
    Q_D(QQuickPopupItem);
    return d->popup->childMouseEventFilter(child, event);
}

void QQuickPopupItem::focusInEvent(QFocusEvent *event)
{
    Q_D(QQuickPopupItem);
    d->popup->focusInEvent(event);
}

void QQuickPopupItem::focusOutEvent(QFocusEvent *event)
{
    Q_D(QQuickPopupItem);
    d->popup->focusOutEvent(event);
}

void QQuickPopupItem::keyPressEvent(QKeyEvent *event)
{
    Q_D(QQuickPopupItem);
    d->popup->keyPressEvent(event);
}

void QQuickPopupItem::keyReleaseEvent(QKeyEvent *event)
{
    Q_D(QQuickPopupItem);
    d->popup->keyReleaseEvent(event);
}

void QQuickPopupItem::mousePressEvent(QMouseEvent *event)
{
    Q_D(QQuickPopupItem);
    d->popup->mousePressEvent(event);
}

void QQuickPopupItem::mouseMoveEvent(QMouseEvent *event)
{
    Q_D(QQuickPopupItem);
    d->popup->mouseMoveEvent(event);
}

void QQuickPopupItem::mouseReleaseEvent(QMouseEvent *event)
{
    Q_D(QQuickPopupItem);
    d->popup->mouseReleaseEvent(event);
}

void QQuickPopupItem::mouseDoubleClickEvent(QMouseEvent *event)
{
    Q_D(QQuickPopupItem);
    d->popup->mouseDoubleClickEvent(event);
}

void QQuickPopupItem::mouseUngrabEvent()
{
    Q_D(QQuickPopupItem);
    d->popup->mouseUngrabEvent();
}

void QQuickPopupItem::wheelEvent(QWheelEvent *event)
{
    Q_D(QQuickPopupItem);
    d->popup->wheelEvent(event);
}

void QQuickPopupItem::contentItemChange(QQuickItem *newItem, QQuickItem *oldItem)
{
    Q_D(QQuickPopupItem);
    QQuickControl::contentItemChange(newItem, oldItem);
    d->popup->contentItemChange(newItem, oldItem);
}

void QQuickPopupItem::fontChange(const QFont &newFont, const QFont &oldFont)
{
    Q_D(QQuickPopupItem);
    QQuickControl::fontChange(newFont, oldFont);
    d->popup->fontChange(newFont, oldFont);
}

void QQuickPopupItem::geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    Q_D(QQuickPopupItem);
    QQuickControl::geometryChanged(newGeometry, oldGeometry);
    d->popup->geometryChanged(newGeometry, oldGeometry);
}

void QQuickPopupItem::localeChange(const QLocale &newLocale, const QLocale &oldLocale)
{
    Q_D(QQuickPopupItem);
    QQuickControl::localeChange(newLocale, oldLocale);
    d->popup->localeChange(newLocale, oldLocale);
}

void QQuickPopupItem::itemChange(ItemChange change, const ItemChangeData &data)
{
    Q_D(QQuickPopupItem);
    QQuickControl::itemChange(change, data);
    d->popup->itemChange(change, data);
}

void QQuickPopupItem::paddingChange(const QMarginsF &newPadding, const QMarginsF &oldPadding)
{
    Q_D(QQuickPopupItem);
    QQuickControl::paddingChange(newPadding, oldPadding);
    d->popup->paddingChange(newPadding, oldPadding);
}

QFont QQuickPopupItem::defaultFont() const
{
    Q_D(const QQuickPopupItem);
    return d->popup->defaultFont();
}

#ifndef QT_NO_ACCESSIBILITY
QAccessible::Role QQuickPopupItem::accessibleRole() const
{
    Q_D(const QQuickPopupItem);
    return d->popup->accessibleRole();
}
#endif // QT_NO_ACCESSIBILITY

QQuickPopupPositioner::QQuickPopupPositioner(QQuickPopupPrivate *popup) :
    m_parentItem(nullptr),
    m_popup(popup)
{
}

QQuickPopupPositioner::~QQuickPopupPositioner()
{
    if (m_parentItem) {
        QQuickItemPrivate::get(m_parentItem)->removeItemChangeListener(this, ItemChangeTypes);
        removeAncestorListeners(m_parentItem->parentItem());
    }
}

QQuickItem *QQuickPopupPositioner::parentItem() const
{
    return m_parentItem;
}

void QQuickPopupPositioner::setParentItem(QQuickItem *parent)
{
    if (m_parentItem == parent)
        return;

    if (m_parentItem) {
        QQuickItemPrivate::get(m_parentItem)->removeItemChangeListener(this, ItemChangeTypes);
        removeAncestorListeners(m_parentItem->parentItem());
    }

    m_parentItem = parent;

    if (!parent)
        return;

    QQuickItemPrivate::get(parent)->addItemChangeListener(this, ItemChangeTypes);
    addAncestorListeners(parent->parentItem());

    if (m_popup->popupItem->isVisible())
        m_popup->reposition();
}

void QQuickPopupPositioner::itemGeometryChanged(QQuickItem *, const QRectF &, const QRectF &)
{
    if (m_parentItem && m_popup->popupItem->isVisible())
        m_popup->reposition();
}

void QQuickPopupPositioner::itemParentChanged(QQuickItem *, QQuickItem *parent)
{
    addAncestorListeners(parent);
}

void QQuickPopupPositioner::itemChildRemoved(QQuickItem *item, QQuickItem *child)
{
    if (isAncestor(child))
        removeAncestorListeners(item);
}

void QQuickPopupPositioner::itemDestroyed(QQuickItem *item)
{
    Q_ASSERT(m_parentItem == item);

    m_parentItem = nullptr;
    m_popup->parentItem = nullptr;
    QQuickItemPrivate::get(item)->removeItemChangeListener(this, ItemChangeTypes);
    removeAncestorListeners(item->parentItem());
}

void QQuickPopupPrivate::reposition()
{
    Q_Q(QQuickPopup);
    const qreal w = popupItem->width();
    const qreal h = popupItem->height();
    const qreal iw = popupItem->implicitWidth();
    const qreal ih = popupItem->implicitHeight();

    bool adjusted = false;
    QRectF rect(x, y, iw > 0 ? iw : w, ih > 0 ? ih : h);
    if (parentItem) {
        rect = parentItem->mapRectToScene(rect);

        QQuickWindow *window = q->window();
        if (window) {
            const QMarginsF margins = getMargins();
            const QRectF bounds = QRectF(0, 0, window->width(), window->height()).marginsRemoved(margins);

            // push inside the margins
            if (margins.top() > 0 && rect.top() < bounds.top())
                rect.moveTop(margins.top());
            if (margins.bottom() > 0 && rect.bottom() > bounds.bottom())
                rect.moveBottom(bounds.bottom());
            if (margins.left() > 0 && rect.left() < bounds.left())
                rect.moveLeft(margins.left());
            if (margins.right() > 0 && rect.right() > bounds.right())
                rect.moveRight(bounds.right());

            if (rect.top() < bounds.top() || rect.bottom() > bounds.bottom()) {
                // if the popup doesn't fit inside the window, try flipping it around (below <-> above)
                const QRectF flipped = parentItem->mapRectToScene(QRectF(x, parentItem->height() - y - rect.height(), rect.width(), rect.height()));
                if (flipped.top() >= bounds.top() && flipped.bottom() < bounds.bottom()) {
                    adjusted = true;
                    rect = flipped;
                } else if (ih > 0) {
                    // neither the flipped around geometry fits inside the window, choose
                    // whichever side (above vs. below) fits larger part of the popup
                    const QRectF primary = rect.intersected(bounds);
                    const QRectF secondary = flipped.intersected(bounds);

                    if (primary.height() > secondary.height()) {
                        rect.setY(primary.y());
                        rect.setHeight(primary.height());
                    } else {
                        rect.setY(secondary.y());
                        rect.setHeight(secondary.height());
                    }
                    adjusted = true;
                }
            }
        }
    }

    popupItem->setPosition(rect.topLeft());
    if (adjusted && ih > 0)
        popupItem->setHeight(rect.height());
}

void QQuickPopupPositioner::removeAncestorListeners(QQuickItem *item)
{
    if (item == m_parentItem)
        return;

    QQuickItem *p = item;
    while (p) {
        QQuickItemPrivate::get(p)->removeItemChangeListener(this, AncestorChangeTypes);
        p = p->parentItem();
    }
}

void QQuickPopupPositioner::addAncestorListeners(QQuickItem *item)
{
    if (item == m_parentItem)
        return;

    QQuickItem *p = item;
    while (p) {
        QQuickItemPrivate::get(p)->addItemChangeListener(this, AncestorChangeTypes);
        p = p->parentItem();
    }
}

// TODO: use QQuickItem::isAncestorOf() in dev/5.7
bool QQuickPopupPositioner::isAncestor(QQuickItem *item) const
{
    if (!m_parentItem)
        return false;

    QQuickItem *parent = m_parentItem;
    while (parent) {
        if (parent == item)
            return true;
        parent = parent->parentItem();
    }
    return false;
}

QQuickPopupTransitionManager::QQuickPopupTransitionManager(QQuickPopupPrivate *popup)
    : QQuickTransitionManager()
    , state(Off)
    , popup(popup)
{
}

void QQuickPopupTransitionManager::transitionEnter()
{
    if (state == Enter && isRunning())
        return;

    state = Enter;
    popup->prepareEnterTransition();
    transition(popup->enterActions, popup->enter, popup->q_func());
}

void QQuickPopupTransitionManager::transitionExit()
{
    if (state == Exit && isRunning())
        return;

    state = Exit;
    popup->prepareExitTransition();
    transition(popup->exitActions, popup->exit, popup->q_func());
}

void QQuickPopupTransitionManager::finished()
{
    if (state == Enter)
        popup->finalizeEnterTransition();
    else if (state == Exit)
        popup->finalizeExitTransition();

    state = Off;
}

QQuickPopup::QQuickPopup(QObject *parent)
    : QObject(*(new QQuickPopupPrivate), parent)
{
    Q_D(QQuickPopup);
    d->init();
}

QQuickPopup::QQuickPopup(QQuickPopupPrivate &dd, QObject *parent)
    : QObject(dd, parent)
{
    Q_D(QQuickPopup);
    d->init();
}

QQuickPopup::~QQuickPopup()
{
    Q_D(QQuickPopup);
    d->positioner.setParentItem(nullptr);
    delete d->popupItem;
}

/*!
    \qmlmethod void Qt.labs.controls::Popup::open()

    Opens the popup.
*/
void QQuickPopup::open()
{
    Q_D(QQuickPopup);
    if (d->visible)
        return;

    if (d->complete)
        d->transitionManager.transitionEnter();
}

/*!
    \qmlmethod void Qt.labs.controls::Popup::close()

    Closes the popup.
*/
void QQuickPopup::close()
{
    Q_D(QQuickPopup);
    if (!d->visible)
        return;

    if (d->complete)
        d->transitionManager.transitionExit();
}

/*!
    \qmlproperty real Qt.labs.controls::Popup::x

    This property holds the x-coordinate of the popup.
*/
qreal QQuickPopup::x() const
{
    Q_D(const QQuickPopup);
    return d->x;
}

void QQuickPopup::setX(qreal x)
{
    Q_D(QQuickPopup);
    if (qFuzzyCompare(d->x, x))
        return;

    d->x = x;
    if (d->popupItem->isVisible())
        d->reposition();
    emit xChanged();
}

/*!
    \qmlproperty real Qt.labs.controls::Popup::y

    This property holds the y-coordinate of the popup.
*/
qreal QQuickPopup::y() const
{
    Q_D(const QQuickPopup);
    return d->y;
}

void QQuickPopup::setY(qreal y)
{
    Q_D(QQuickPopup);
    if (qFuzzyCompare(d->y, y))
        return;

    d->y = y;
    if (d->popupItem->isVisible())
        d->reposition();
    emit yChanged();
}

QPointF QQuickPopup::position() const
{
    Q_D(const QQuickPopup);
    return QPointF(d->x, d->y);
}

void QQuickPopup::setPosition(const QPointF &pos)
{
    Q_D(QQuickPopup);
    const bool xChange = !qFuzzyCompare(d->x, pos.x());
    const bool yChange = !qFuzzyCompare(d->y, pos.y());
    if (!xChange && !yChange)
        return;

    d->x = pos.x();
    d->y = pos.y();
    if (d->popupItem->isVisible())
        d->reposition();
    if (xChange)
        emit xChanged();
    if (yChange)
        emit yChanged();
}

/*!
    \qmlproperty real Qt.labs.controls::Popup::z

    This property holds the z-value of the popup. Z-value determines
    the stacking order of popups. The default z-value is \c 0.
*/
qreal QQuickPopup::z() const
{
    Q_D(const QQuickPopup);
    return d->popupItem->z();
}

void QQuickPopup::setZ(qreal z)
{
    Q_D(QQuickPopup);
    if (qFuzzyCompare(z, d->popupItem->z()))
        return;
    d->popupItem->setZ(z);
    emit zChanged();
}

/*!
    \qmlproperty real Qt.labs.controls::Popup::width

    This property holds the width of the popup.
*/
qreal QQuickPopup::width() const
{
    Q_D(const QQuickPopup);
    return d->popupItem->width();
}

void QQuickPopup::setWidth(qreal width)
{
    Q_D(QQuickPopup);
    d->popupItem->setWidth(width);
}

void QQuickPopup::resetWidth()
{
    Q_D(QQuickPopup);
    d->popupItem->resetWidth();
}

/*!
    \qmlproperty real Qt.labs.controls::Popup::height

    This property holds the height of the popup.
*/
qreal QQuickPopup::height() const
{
    Q_D(const QQuickPopup);
    return d->popupItem->height();
}

void QQuickPopup::setHeight(qreal height)
{
    Q_D(QQuickPopup);
    d->popupItem->setHeight(height);
}

void QQuickPopup::resetHeight()
{
    Q_D(QQuickPopup);
    d->popupItem->resetHeight();
}

/*!
    \qmlproperty real Qt.labs.controls::Popup::implicitWidth

    This property holds the implicit width of the popup.
*/
qreal QQuickPopup::implicitWidth() const
{
    Q_D(const QQuickPopup);
    return d->popupItem->implicitWidth();
}

void QQuickPopup::setImplicitWidth(qreal width)
{
    Q_D(QQuickPopup);
    d->popupItem->setImplicitWidth(width);
}

/*!
    \qmlproperty real Qt.labs.controls::Popup::implicitHeight

    This property holds the implicit height of the popup.
*/
qreal QQuickPopup::implicitHeight() const
{
    Q_D(const QQuickPopup);
    return d->popupItem->implicitHeight();
}

void QQuickPopup::setImplicitHeight(qreal height)
{
    Q_D(QQuickPopup);
    d->popupItem->setImplicitHeight(height);
}

/*!
    \qmlproperty real Qt.labs.controls::Popup::contentWidth

    This property holds the content width. It is used for calculating the
    total implicit width of the Popup.

    \note If only a single item is used within the Popup, the implicit width
          of its contained item is used as the content width.
*/
qreal QQuickPopup::contentWidth() const
{
    Q_D(const QQuickPopup);
    return d->contentWidth;
}

void QQuickPopup::setContentWidth(qreal width)
{
    Q_D(QQuickPopup);
    if (qFuzzyCompare(d->contentWidth, width))
        return;

    d->contentWidth = width;
    emit contentWidthChanged();
}

/*!
    \qmlproperty real Qt.labs.controls::Popup::contentHeight

    This property holds the content height. It is used for calculating the
    total implicit height of the Popup.

    \note If only a single item is used within the Popup, the implicit height
          of its contained item is used as the content height.
*/
qreal QQuickPopup::contentHeight() const
{
    Q_D(const QQuickPopup);
    return d->contentHeight;
}

void QQuickPopup::setContentHeight(qreal height)
{
    Q_D(QQuickPopup);
    if (qFuzzyCompare(d->contentHeight, height))
        return;

    d->contentHeight = height;
    emit contentHeightChanged();
}

/*!
    \qmlproperty real Qt.labs.controls::Popup::availableWidth
    \readonly

    This property holds the width available after deducting horizontal padding.

    \sa padding, leftPadding, rightPadding
*/
qreal QQuickPopup::availableWidth() const
{
    Q_D(const QQuickPopup);
    return d->popupItem->availableWidth();
}

/*!
    \qmlproperty real Qt.labs.controls::Popup::availableHeight
    \readonly

    This property holds the height available after deducting vertical padding.

    \sa padding, topPadding, bottomPadding
*/
qreal QQuickPopup::availableHeight() const
{
    Q_D(const QQuickPopup);
    return d->popupItem->availableHeight();
}

/*!
    \qmlproperty real Qt.labs.controls::Popup::margins

    This property holds the default margins around the popup.

    \sa topMargin, leftMargin, rightMargin, bottomMargin
*/
qreal QQuickPopup::margins() const
{
    Q_D(const QQuickPopup);
    return d->margins;
}

void QQuickPopup::setMargins(qreal margins)
{
    Q_D(QQuickPopup);
    if (qFuzzyCompare(d->margins, margins))
        return;
    QMarginsF oldMargins(leftMargin(), topMargin(), rightMargin(), bottomMargin());
    d->margins = margins;
    emit marginsChanged();
    QMarginsF newMargins(leftMargin(), topMargin(), rightMargin(), bottomMargin());
    if (!qFuzzyCompare(newMargins.top(), oldMargins.top()))
        emit topMarginChanged();
    if (!qFuzzyCompare(newMargins.left(), oldMargins.left()))
        emit leftMarginChanged();
    if (!qFuzzyCompare(newMargins.right(), oldMargins.right()))
        emit rightMarginChanged();
    if (!qFuzzyCompare(newMargins.bottom(), oldMargins.bottom()))
        emit bottomMarginChanged();
    marginsChange(newMargins, oldMargins);
}

void QQuickPopup::resetMargins()
{
    setMargins(0);
}

/*!
    \qmlproperty real Qt.labs.controls::Popup::topMargin

    This property holds the top margin around the popup.

    \sa margins, bottomMargin
*/
qreal QQuickPopup::topMargin() const
{
    Q_D(const QQuickPopup);
    if (d->hasTopMargin)
        return d->topMargin;
    return d->margins;
}

void QQuickPopup::setTopMargin(qreal margin)
{
    Q_D(QQuickPopup);
    d->setTopMargin(margin);
}

void QQuickPopup::resetTopMargin()
{
    Q_D(QQuickPopup);
    d->setTopMargin(0, true);
}

/*!
    \qmlproperty real Qt.labs.controls::Popup::leftMargin

    This property holds the left margin around the popup.

    \sa margins, rightMargin
*/
qreal QQuickPopup::leftMargin() const
{
    Q_D(const QQuickPopup);
    if (d->hasLeftMargin)
        return d->leftMargin;
    return d->margins;
}

void QQuickPopup::setLeftMargin(qreal margin)
{
    Q_D(QQuickPopup);
    d->setLeftMargin(margin);
}

void QQuickPopup::resetLeftMargin()
{
    Q_D(QQuickPopup);
    d->setLeftMargin(0, true);
}

/*!
    \qmlproperty real Qt.labs.controls::Popup::rightMargin

    This property holds the right margin around the popup.

    \sa margins, leftMargin
*/
qreal QQuickPopup::rightMargin() const
{
    Q_D(const QQuickPopup);
    if (d->hasRightMargin)
        return d->rightMargin;
    return d->margins;
}

void QQuickPopup::setRightMargin(qreal margin)
{
    Q_D(QQuickPopup);
    d->setRightMargin(margin);
}

void QQuickPopup::resetRightMargin()
{
    Q_D(QQuickPopup);
    d->setRightMargin(0, true);
}

/*!
    \qmlproperty real Qt.labs.controls::Popup::bottomMargin

    This property holds the bottom margin around the popup.

    \sa margins, topMargin
*/
qreal QQuickPopup::bottomMargin() const
{
    Q_D(const QQuickPopup);
    if (d->hasBottomMargin)
        return d->bottomMargin;
    return d->margins;
}

void QQuickPopup::setBottomMargin(qreal margin)
{
    Q_D(QQuickPopup);
    d->setBottomMargin(margin);
}

void QQuickPopup::resetBottomMargin()
{
    Q_D(QQuickPopup);
    d->setBottomMargin(0, true);
}

/*!
    \qmlproperty real Qt.labs.controls::Popup::padding

    This property holds the default padding.

    \sa availableWidth, availableHeight, topPadding, leftPadding, rightPadding, bottomPadding
*/
qreal QQuickPopup::padding() const
{
    Q_D(const QQuickPopup);
    return d->popupItem->padding();
}

void QQuickPopup::setPadding(qreal padding)
{
    Q_D(QQuickPopup);
    d->popupItem->setPadding(padding);
}

void QQuickPopup::resetPadding()
{
    Q_D(QQuickPopup);
    d->popupItem->resetPadding();
}

/*!
    \qmlproperty real Qt.labs.controls::Popup::topPadding

    This property holds the top padding.

    \sa padding, bottomPadding, availableHeight
*/
qreal QQuickPopup::topPadding() const
{
    Q_D(const QQuickPopup);
    return d->popupItem->topPadding();
}

void QQuickPopup::setTopPadding(qreal padding)
{
    Q_D(QQuickPopup);
    d->popupItem->setTopPadding(padding);
}

void QQuickPopup::resetTopPadding()
{
    Q_D(QQuickPopup);
    d->popupItem->resetTopPadding();
}

/*!
    \qmlproperty real Qt.labs.controls::Popup::leftPadding

    This property holds the left padding.

    \sa padding, rightPadding, availableWidth
*/
qreal QQuickPopup::leftPadding() const
{
    Q_D(const QQuickPopup);
    return d->popupItem->leftPadding();
}

void QQuickPopup::setLeftPadding(qreal padding)
{
    Q_D(QQuickPopup);
    d->popupItem->setLeftPadding(padding);
}

void QQuickPopup::resetLeftPadding()
{
    Q_D(QQuickPopup);
    d->popupItem->resetLeftPadding();
}

/*!
    \qmlproperty real Qt.labs.controls::Popup::rightPadding

    This property holds the right padding.

    \sa padding, leftPadding, availableWidth
*/
qreal QQuickPopup::rightPadding() const
{
    Q_D(const QQuickPopup);
    return d->popupItem->rightPadding();
}

void QQuickPopup::setRightPadding(qreal padding)
{
    Q_D(QQuickPopup);
    d->popupItem->setRightPadding(padding);
}

void QQuickPopup::resetRightPadding()
{
    Q_D(QQuickPopup);
    d->popupItem->resetRightPadding();
}

/*!
    \qmlproperty real Qt.labs.controls::Popup::bottomPadding

    This property holds the bottom padding.

    \sa padding, topPadding, availableHeight
*/
qreal QQuickPopup::bottomPadding() const
{
    Q_D(const QQuickPopup);
    return d->popupItem->bottomPadding();
}

void QQuickPopup::setBottomPadding(qreal padding)
{
    Q_D(QQuickPopup);
    d->popupItem->setBottomPadding(padding);
}

void QQuickPopup::resetBottomPadding()
{
    Q_D(QQuickPopup);
    d->popupItem->resetBottomPadding();
}

/*!
    \qmlproperty Locale Qt.labs.controls::Popup::locale

    This property holds the locale of the popup.

    \sa {LayoutMirroring}{LayoutMirroring}
*/
QLocale QQuickPopup::locale() const
{
    Q_D(const QQuickPopup);
    return d->popupItem->locale();
}

void QQuickPopup::setLocale(const QLocale &locale)
{
    Q_D(QQuickPopup);
    d->popupItem->setLocale(locale);
}

void QQuickPopup::resetLocale()
{
    Q_D(QQuickPopup);
    d->popupItem->resetLocale();
}

/*!
    \qmlproperty font Qt.labs.controls::Popup::font

    This property holds the font currently set for the popup.
*/
QFont QQuickPopup::font() const
{
    Q_D(const QQuickPopup);
    return d->popupItem->font();
}

void QQuickPopup::setFont(const QFont &font)
{
    Q_D(QQuickPopup);
    d->popupItem->setFont(font);
}

void QQuickPopup::resetFont()
{
    Q_D(QQuickPopup);
    d->popupItem->resetFont();
}

QQuickWindow *QQuickPopup::window() const
{
    Q_D(const QQuickPopup);
    if (!d->parentItem)
        return nullptr;

    return d->parentItem->window();
}

QQuickItem *QQuickPopup::popupItem() const
{
    Q_D(const QQuickPopup);
    return d->popupItem;
}

/*!
    \qmlproperty Item Qt.labs.popups::Popup::parent

    This property holds the parent item.
*/
QQuickItem *QQuickPopup::parentItem() const
{
    Q_D(const QQuickPopup);
    return d->parentItem;
}

void QQuickPopup::setParentItem(QQuickItem *parent)
{
    Q_D(QQuickPopup);
    if (d->parentItem == parent)
        return;

    QQuickWindow *oldWindow = window();

    d->parentItem = parent;
    if (d->positioner.parentItem())
        d->positioner.setParentItem(parent);
    if (parent) {
        QQuickControlPrivate *p = QQuickControlPrivate::get(d->popupItem);
        p->resolveFont();
        if (QQuickApplicationWindow *window = qobject_cast<QQuickApplicationWindow *>(parent->window()))
            p->updateLocale(window->locale(), false); // explicit=false
    }
    emit parentChanged();

    QQuickWindow *newWindow = window();
    if (oldWindow != newWindow)
        emit windowChanged(newWindow);
}

/*!
    \qmlproperty Item Qt.labs.popups::Popup::background

    This property holds the background item.

    \note If the background item has no explicit size specified, it automatically
          follows the popup's size. In most cases, there is no need to specify
          width or height for a background item.
*/
QQuickItem *QQuickPopup::background() const
{
    Q_D(const QQuickPopup);
    return d->popupItem->background();
}

void QQuickPopup::setBackground(QQuickItem *background)
{
    Q_D(QQuickPopup);
    if (d->popupItem->background() == background)
        return;

    d->popupItem->setBackground(background);
    emit backgroundChanged();
}

/*!
    \qmlproperty Item Qt.labs.controls::Popup::contentItem

    This property holds the content item of the popup.

    The content item is the visual implementation of the popup. When the
    popup is made visible, the content item is automatically reparented to
    the \l {ApplicationWindow::overlay}{overlay item} of its application
    window.
*/
QQuickItem *QQuickPopup::contentItem() const
{
    Q_D(const QQuickPopup);
    return d->popupItem->contentItem();
}

void QQuickPopup::setContentItem(QQuickItem *item)
{
    Q_D(QQuickPopup);
    d->popupItem->setContentItem(item);
}

/*!
    \qmlproperty list<Object> Qt.labs.controls::Popup::contentData
    \default

    This property holds the list of content data.

    \sa Item::data
*/
QQmlListProperty<QObject> QQuickPopup::contentData()
{
    Q_D(QQuickPopup);
    return QQmlListProperty<QObject>(d->popupItem->contentItem(), nullptr,
                                     QQuickItemPrivate::data_append,
                                     QQuickItemPrivate::data_count,
                                     QQuickItemPrivate::data_at,
                                     QQuickItemPrivate::data_clear);
}

/*!
    \qmlproperty list<Item> Qt.labs.controls::Popup::contentChildren

    This property holds the list of content children.

    \sa Item::children
*/
QQmlListProperty<QQuickItem> QQuickPopup::contentChildren()
{
    Q_D(QQuickPopup);
    return QQmlListProperty<QQuickItem>(d->popupItem->contentItem(), nullptr,
                                        QQuickItemPrivate::children_append,
                                        QQuickItemPrivate::children_count,
                                        QQuickItemPrivate::children_at,
                                        QQuickItemPrivate::children_clear);
}

/*!
    \qmlproperty bool Qt.labs.controls::Popup::clip

    This property holds whether clipping is enabled. The default value is \c false.
*/
bool QQuickPopup::clip() const
{
    Q_D(const QQuickPopup);
    return d->popupItem->clip();
}

void QQuickPopup::setClip(bool clip)
{
    Q_D(QQuickPopup);
    if (clip == d->popupItem->clip())
        return;
    d->popupItem->setClip(clip);
    emit clipChanged();
}

/*!
    \qmlproperty bool Qt.labs.controls::Popup::focus

    This property holds whether the popup has focus. The default value is \c false.
*/
bool QQuickPopup::hasFocus() const
{
    Q_D(const QQuickPopup);
    return d->focus;
}

void QQuickPopup::setFocus(bool focus)
{
    Q_D(QQuickPopup);
    if (d->focus == focus)
        return;
    d->focus = focus;
    emit focusChanged();
}

/*!
    \qmlproperty bool Qt.labs.controls::Popup::activeFocus
    \readonly

    This property holds whether the popup has active focus.
*/
bool QQuickPopup::hasActiveFocus() const
{
    Q_D(const QQuickPopup);
    return d->popupItem->hasActiveFocus();
}

/*!
    \qmlproperty bool Qt.labs.controls::Popup::modal

    This property holds whether the popup is modal. The default value is \c false.
*/
bool QQuickPopup::isModal() const
{
    Q_D(const QQuickPopup);
    return d->modal;
}

void QQuickPopup::setModal(bool modal)
{
    Q_D(QQuickPopup);
    if (d->modal == modal)
        return;
    d->modal = modal;
    emit modalChanged();
}

/*!
    \qmlproperty bool Qt.labs.controls::Popup::visible

    This property holds whether the popup is visible. The default value is \c false.
*/
bool QQuickPopup::isVisible() const
{
    Q_D(const QQuickPopup);
    return d->visible && d->popupItem->isVisible();
}

void QQuickPopup::setVisible(bool visible)
{
    Q_D(QQuickPopup);
    if (d->visible == visible)
        return;

    d->visible = visible;
    if (d->complete) {
        if (visible)
            d->transitionManager.transitionEnter();
        else
            d->transitionManager.transitionExit();
    }
}

/*!
    \qmlproperty real Qt.labs.controls::Popup::opacity

    This property holds the opacity of the popup. The default value is \c 1.0.
*/
qreal QQuickPopup::opacity() const
{
    Q_D(const QQuickPopup);
    return d->popupItem->opacity();
}

void QQuickPopup::setOpacity(qreal opacity)
{
    Q_D(QQuickPopup);
    d->popupItem->setOpacity(opacity);
}

/*!
    \qmlproperty real Qt.labs.controls::Popup::scale

    This property holds the scale factor of the popup. The default value is \c 1.0.
*/
qreal QQuickPopup::scale() const
{
    Q_D(const QQuickPopup);
    return d->popupItem->scale();
}

void QQuickPopup::setScale(qreal scale)
{
    Q_D(QQuickPopup);
    if (qFuzzyCompare(scale, d->popupItem->scale()))
        return;
    d->popupItem->setScale(scale);
    emit scaleChanged();
}

/*!
    \qmlproperty enumeration Qt.labs.controls::Popup::closePolicy

    This property determines the circumstances under which the popup closes.
    The flags can be combined to allow several ways of closing the popup.

    The available values are:
    \value Popup.NoAutoClose The popup will only close when manually instructed to do so.
    \value Popup.OnPressOutside The popup will close when the mouse is pressed outside of it.
    \value Popup.OnPressOutsideParent The popup will close when the mouse is pressed outside of its parent.
    \value Popup.OnReleaseOutside The popup will close when the mouse is released outside of it.
    \value Popup.OnReleaseOutsideParent The popup will close when the mouse is released outside of its parent.
    \value Popup.OnEscape The popup will close when the escape key is pressed while the popup
        has active focus.

    The default value is \c {Popup.OnEscape | Popup.OnPressOutside}.
*/
QQuickPopup::ClosePolicy QQuickPopup::closePolicy() const
{
    Q_D(const QQuickPopup);
    return d->closePolicy;
}

void QQuickPopup::setClosePolicy(ClosePolicy policy)
{
    Q_D(QQuickPopup);
    if (d->closePolicy == policy)
        return;
    d->closePolicy = policy;
    emit closePolicyChanged();
}

/*!
    \qmlproperty enumeration Qt.labs.controls::Popup::transformOrigin

    This property holds the origin point for transformations in enter and exit transitions.

    Nine transform origins are available, as shown in the image below.
    The default transform origin is \c Popup.Center.

    \image qtquickcontrols2-popup-transformorigin.png

    \sa enter, exit, Item::transformOrigin
*/
QQuickPopup::TransformOrigin QQuickPopup::transformOrigin() const
{
    Q_D(const QQuickPopup);
    return static_cast<TransformOrigin>(d->popupItem->transformOrigin());
}

void QQuickPopup::setTransformOrigin(TransformOrigin origin)
{
    Q_D(QQuickPopup);
    d->popupItem->setTransformOrigin(static_cast<QQuickItem::TransformOrigin>(origin));
}

/*!
    \qmlproperty Transition Qt.labs.controls::Popup::enter

    This property holds the transition that is applied to the content item
    when the popup is opened and enters the screen.
*/
QQuickTransition *QQuickPopup::enter() const
{
    Q_D(const QQuickPopup);
    return d->enter;
}

void QQuickPopup::setEnter(QQuickTransition *transition)
{
    Q_D(QQuickPopup);
    if (d->enter == transition)
        return;
    d->enter = transition;
    emit enterChanged();
}

/*!
    \qmlproperty Transition Qt.labs.controls::Popup::exit

    This property holds the transition that is applied to the content item
    when the popup is closed and exits the screen.
*/
QQuickTransition *QQuickPopup::exit() const
{
    Q_D(const QQuickPopup);
    return d->exit;
}

void QQuickPopup::setExit(QQuickTransition *transition)
{
    Q_D(QQuickPopup);
    if (d->exit == transition)
        return;
    d->exit = transition;
    emit exitChanged();
}

bool QQuickPopup::filtersChildMouseEvents() const
{
    Q_D(const QQuickPopup);
    return d->popupItem->filtersChildMouseEvents();
}

void QQuickPopup::setFiltersChildMouseEvents(bool filter)
{
    Q_D(QQuickPopup);
    d->popupItem->setFiltersChildMouseEvents(filter);
}

void QQuickPopup::classBegin()
{
}

void QQuickPopup::componentComplete()
{
    Q_D(QQuickPopup);
    d->complete = true;
    if (!parentItem())
        setParentItem(qobject_cast<QQuickItem *>(parent()));
    if (d->visible)
        d->transitionManager.transitionEnter();
}

bool QQuickPopup::isComponentComplete() const
{
    Q_D(const QQuickPopup);
    return d->complete;
}

bool QQuickPopup::eventFilter(QObject *object, QEvent *event)
{
    if (QQuickWindow *window = qobject_cast<QQuickWindow *>(object))
        return overlayEvent(window->contentItem(), event);
    return false;
}

bool QQuickPopup::childMouseEventFilter(QQuickItem *child, QEvent *event)
{
    Q_UNUSED(child);
    Q_UNUSED(event);
    return false;
}

void QQuickPopup::focusInEvent(QFocusEvent *event)
{
    event->accept();
}

void QQuickPopup::focusOutEvent(QFocusEvent *event)
{
    event->accept();
}

void QQuickPopup::keyPressEvent(QKeyEvent *event)
{
    Q_D(QQuickPopup);
    event->accept();

    if (hasActiveFocus() && (event->key() == Qt::Key_Tab || event->key() == Qt::Key_Backtab))
        QQuickItemPrivate::focusNextPrev(d->popupItem, event->key() == Qt::Key_Tab);

    if (event->key() != Qt::Key_Escape)
        return;

    if (d->closePolicy.testFlag(OnEscape))
        close();
}

void QQuickPopup::keyReleaseEvent(QKeyEvent *event)
{
    event->accept();
}

void QQuickPopup::mousePressEvent(QMouseEvent *event)
{
    event->accept();
}

void QQuickPopup::mouseMoveEvent(QMouseEvent *event)
{
    event->accept();
}

void QQuickPopup::mouseReleaseEvent(QMouseEvent *event)
{
    event->accept();
}

void QQuickPopup::mouseDoubleClickEvent(QMouseEvent *event)
{
    event->accept();
}

void QQuickPopup::mouseUngrabEvent()
{
}

bool QQuickPopup::overlayEvent(QQuickItem *item, QEvent *event)
{
    Q_D(QQuickPopup);
    switch (event->type()) {
    case QEvent::KeyPress:
    case QEvent::KeyRelease:
    case QEvent::MouseMove:
    case QEvent::Wheel:
        if (d->modal)
            event->accept();
        return d->modal;

    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonRelease:
        if (d->modal)
            event->accept();
        d->tryClose(item, static_cast<QMouseEvent *>(event));
        return d->modal;

    default:
        return false;
    }
}

void QQuickPopup::wheelEvent(QWheelEvent *event)
{
    event->accept();
}

void QQuickPopup::contentItemChange(QQuickItem *newItem, QQuickItem *oldItem)
{
    Q_UNUSED(newItem);
    Q_UNUSED(oldItem);
    emit contentItemChanged();
}

void QQuickPopup::fontChange(const QFont &newFont, const QFont &oldFont)
{
    Q_UNUSED(newFont);
    Q_UNUSED(oldFont);
    emit fontChanged();
}

void QQuickPopup::geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    Q_D(QQuickPopup);
    d->reposition();
    if (!qFuzzyCompare(newGeometry.width(), oldGeometry.width())) {
        emit widthChanged();
        emit availableWidthChanged();
    }
    if (!qFuzzyCompare(newGeometry.height(), oldGeometry.height())) {
        emit heightChanged();
        emit availableHeightChanged();
    }
}

void QQuickPopup::itemChange(QQuickItem::ItemChange change, const QQuickItem::ItemChangeData &data)
{
    Q_UNUSED(data);

    switch (change) {
    case QQuickItem::ItemActiveFocusHasChanged:
        emit activeFocusChanged();
        break;
    case QQuickItem::ItemOpacityHasChanged:
        emit opacityChanged();
        break;
    default:
        break;
    }
}

void QQuickPopup::localeChange(const QLocale &newLocale, const QLocale &oldLocale)
{
    Q_UNUSED(newLocale);
    Q_UNUSED(oldLocale);
    emit localeChanged();
}

void QQuickPopup::marginsChange(const QMarginsF &newMargins, const QMarginsF &oldMargins)
{
    Q_D(QQuickPopup);
    Q_UNUSED(newMargins);
    Q_UNUSED(oldMargins);
    d->reposition();
}

void QQuickPopup::paddingChange(const QMarginsF &newPadding, const QMarginsF &oldPadding)
{
    const bool tp = !qFuzzyCompare(newPadding.top(), oldPadding.top());
    const bool lp = !qFuzzyCompare(newPadding.left(), oldPadding.left());
    const bool rp = !qFuzzyCompare(newPadding.right(), oldPadding.right());
    const bool bp = !qFuzzyCompare(newPadding.bottom(), oldPadding.bottom());

    if (tp)
        emit topPaddingChanged();
    if (lp)
        emit leftPaddingChanged();
    if (rp)
        emit rightPaddingChanged();
    if (bp)
        emit bottomPaddingChanged();

    if (lp || rp)
        emit availableWidthChanged();
    if (tp || bp)
        emit availableHeightChanged();
}

QFont QQuickPopup::defaultFont() const
{
    return QQuickControlPrivate::themeFont(QPlatformTheme::SystemFont);
}

#ifndef QT_NO_ACCESSIBILITY
QAccessible::Role QQuickPopup::accessibleRole() const
{
    return QAccessible::LayeredPane;
}
#endif // QT_NO_ACCESSIBILITY

QT_END_NAMESPACE

#include "moc_qquickpopup_p.cpp"