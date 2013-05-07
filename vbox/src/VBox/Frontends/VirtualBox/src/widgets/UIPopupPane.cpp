/* $Id: UIPopupPane.cpp 45696 2013-04-24 13:09:42Z vboxsync $ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIPopupPane class implementation
 */

/*
 * Copyright (C) 2013 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* Qt includes: */
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QCheckBox>
#include <QKeyEvent>
#include <QPainter>
#include <QStateMachine>
#include <QPropertyAnimation>
#include <QSignalTransition>

/* GUI includes: */
#include "QIWithRetranslateUI.h"
#include "UIPopupPane.h"
#include "UIIconPool.h"
#include "QIToolButton.h"

/* Other VBox includes: */
#include <VBox/sup.h>


QStateMachine* UIAnimationFramework::installPropertyAnimation(QWidget *pTarget, const char *pszPropertyName,
                                                              const char *pszValuePropertyNameStart, const char *pszValuePropertyNameFinal,
                                                              const char *pSignalForward, const char *pSignalBackward,
                                                              bool fReversive /*= false*/, int iAnimationDuration /*= 300*/)
{
    /* State-machine: */
    QStateMachine *pStateMachine = new QStateMachine(pTarget);
    /* State-machine 'start' state: */
    QState *pStateStart = new QState(pStateMachine);
    /* State-machine 'final' state: */
    QState *pStateFinal = new QState(pStateMachine);

    /* State-machine 'forward' animation: */
    QPropertyAnimation *pForwardAnimation = new QPropertyAnimation(pTarget, pszPropertyName, pStateMachine);
    pForwardAnimation->setEasingCurve(QEasingCurve(QEasingCurve::InOutCubic));
    pForwardAnimation->setDuration(iAnimationDuration);
    pForwardAnimation->setStartValue(pTarget->property(pszValuePropertyNameStart));
    pForwardAnimation->setEndValue(pTarget->property(pszValuePropertyNameFinal));
    /* State-machine 'backward' animation: */
    QPropertyAnimation *pBackwardAnimation = new QPropertyAnimation(pTarget, pszPropertyName, pStateMachine);
    pBackwardAnimation->setEasingCurve(QEasingCurve(QEasingCurve::InOutCubic));
    pBackwardAnimation->setDuration(iAnimationDuration);
    pBackwardAnimation->setStartValue(pTarget->property(pszValuePropertyNameFinal));
    pBackwardAnimation->setEndValue(pTarget->property(pszValuePropertyNameStart));

    /* State-machine state transitions: */
    QSignalTransition *pDefaultToHovered = pStateStart->addTransition(pTarget, pSignalForward, pStateFinal);
    pDefaultToHovered->addAnimation(pForwardAnimation);
    QSignalTransition *pHoveredToDefault = pStateFinal->addTransition(pTarget, pSignalBackward, pStateStart);
    pHoveredToDefault->addAnimation(pBackwardAnimation);

    /* Initial state is 'start': */
    pStateMachine->setInitialState(!fReversive ? pStateStart : pStateFinal);
    /* Start hover-machine: */
    pStateMachine->start();
    /* Return machine: */
    return pStateMachine;
}


/* Popup-pane text-pane prototype class: */
class UIPopupPaneTextPane : public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;
    Q_PROPERTY(QSize collapsedSizeHint READ collapsedSizeHint);
    Q_PROPERTY(QSize expandedSizeHint READ expandedSizeHint);
    Q_PROPERTY(QSize minimumSizeHint READ minimumSizeHint WRITE setMinimumSizeHint);

signals:

    /* Notifiers: Parent propagation stuff: */
    void sigFocusEnter();
    void sigFocusLeave();

    /* Notifier: Layout stuff: */
    void sigSizeHintChanged();

public:

    /* Constructor: */
    UIPopupPaneTextPane(QWidget *pParent = 0);

    /* API: Text stuff: */
    void setText(const QString &strText);

    /* API: Desired-width stuff: */
    void setDesiredWidth(int iDesiredWidth);

    /* API: Auto-confirmation stuff: */
    void setProposeAutoConfirmation(bool fPropose);
    bool autoConfirmationProposed() const;
    bool isAutoConfirmed() const;

    /* API: Layout stuff: */
    QSize minimumSizeHint() const;
    void setMinimumSizeHint(const QSize &minimumSizeHint);
    void layoutContent();

private slots:

    /* Handlers: Focus stuff: */
    void sltFocusEnter();
    void sltFocusLeave();

private:

    /* Helpers: Prepare stuff: */
    void prepare();
    void prepareContent();

    /* Helper: Translate stuff: */
    void retranslateUi();

    /* Helper: Layout stuff: */
    void updateSizeHint();

    /* Property: Focus stuff: */
    QSize collapsedSizeHint() const { return m_collapsedSizeHint; }
    QSize expandedSizeHint() const { return m_expandedSizeHint; }

    /* Variables: Layout stuff: */
    const int m_iLayoutMargin;
    const int m_iLayoutSpacing;
    QSize m_labelSizeHint;
    QSize m_checkBoxSizeHint;
    QSize m_collapsedSizeHint;
    QSize m_expandedSizeHint;
    QSize m_minimumSizeHint;

    /* Variables: Widget stuff: */
    QLabel *m_pLabel;
    int m_iDesiredLabelWidth;
    QCheckBox *m_pAutoConfirmCheckBox;
    bool m_fProposeAutoConfirmation;

    /* Variables: Focus stuff: */
    bool m_fFocused;
    QObject *m_pAnimation;
};


/* Popup-pane button-pane prototype class: */
class UIPopupPaneButtonPane : public QWidget
{
    Q_OBJECT;

signals:

    /* Notifier: Button stuff: */
    void sigButtonClicked(int iButtonID);

public:

    /* Constructor: */
    UIPopupPaneButtonPane(QWidget *pParent = 0);

    /* API: Button stuff: */
    void setButtons(const QMap<int, QString> &buttonDescriptions);

private slots:

    /* Handler: Button stuff: */
    void sltButtonClicked();

private:

    /* Helpers: Prepare/cleanup stuff: */
    void prepare();
    void prepareLayouts();
    void prepareButtons();
    void cleanupButtons();

    /* Handler: Event stuff: */
    void keyPressEvent(QKeyEvent *pEvent);

    /* Static helpers: Button stuff: */
    static QIToolButton* addButton(int iButtonID, const QString &strToolTip);
    static QString defaultToolTip(int iButtonID);
    static QIcon defaultIcon(int iButtonID);

    /* Variables: Widget stuff: */
    QHBoxLayout *m_pButtonLayout;
    QMap<int, QString> m_buttonDescriptions;
    QMap<int, QIToolButton*> m_buttons;
    int m_iDefaultButton;
    int m_iEscapeButton;
};


UIPopupPane::UIPopupPane(QWidget *pParent,
                         const QString &strMessage, const QString &strDetails,
                         const QMap<int, QString> &buttonDescriptions,
                         bool fProposeAutoConfirmation)
    : QWidget(pParent)
    , m_iLayoutMargin(10), m_iLayoutSpacing(5)
    , m_strMessage(strMessage), m_strDetails(strDetails)
    , m_fProposeAutoConfirmation(fProposeAutoConfirmation)
    , m_buttonDescriptions(buttonDescriptions)
    , m_fHovered(false)
    , m_iDefaultOpacity(128)
    , m_iHoveredOpacity(230)
    , m_iOpacity(m_iDefaultOpacity)
    , m_fFocused(false)
    , m_pTextPane(0), m_pButtonPane(0)
{
    /* Prepare: */
    prepare();
}

void UIPopupPane::setMessage(const QString &strMessage)
{
    /* Make sure somthing changed: */
    if (m_strMessage == strMessage)
        return;

    /* Fetch new message: */
    m_strMessage = strMessage;
    m_pTextPane->setText(m_strMessage);
}

void UIPopupPane::setDetails(const QString &strDetails)
{
    /* Make sure somthing changed: */
    if (m_strDetails == strDetails)
        return;

    /* Fetch new details: */
    m_strDetails = strDetails;
}

void UIPopupPane::setProposeAutoConfirmation(bool fPropose)
{
    /* Make sure something changed: */
    if (m_fProposeAutoConfirmation == fPropose)
        return;

    /* Fetch new auto-confirmation proposal: */
    m_fProposeAutoConfirmation = fPropose;
    m_pTextPane->setProposeAutoConfirmation(m_fProposeAutoConfirmation);
}

void UIPopupPane::setDesiredWidth(int iWidth)
{
    /* Make sure text-pane exists: */
    if (!m_pTextPane)
        return;

    /* Propagate desired width to the text-pane we have: */
    m_pTextPane->setDesiredWidth(iWidth - 2 * m_iLayoutMargin
                                        - m_pButtonPane->minimumSizeHint().width());
}

int UIPopupPane::minimumWidthHint() const
{
    /* Prepare width hint: */
    int iWidthHint = 0;

    /* Take into account layout: */
    iWidthHint += 2 * m_iLayoutMargin;
    {
        /* Take into account widgets: */
        iWidthHint += m_pTextPane->minimumSizeHint().width();
        iWidthHint += m_iLayoutSpacing;
        iWidthHint += m_pButtonPane->minimumSizeHint().width();
    }

    /* Return width hint: */
    return iWidthHint;
}

int UIPopupPane::minimumHeightHint() const
{
    /* Prepare height hint: */
    int iHeightHint = 0;

    /* Take into account layout: */
    iHeightHint += 2 * m_iLayoutMargin;
    {
        /* Take into account widgets: */
        const int iTextPaneHeight = m_pTextPane->minimumSizeHint().height();
        const int iButtonBoxHeight = m_pButtonPane->minimumSizeHint().height();
        iHeightHint += qMax(iTextPaneHeight, iButtonBoxHeight);
    }

    /* Return height hint: */
    return iHeightHint;
}

QSize UIPopupPane::minimumSizeHint() const
{
    /* Wrap reimplemented getters: */
    return QSize(minimumWidthHint(), minimumHeightHint());
}

void UIPopupPane::layoutContent()
{
    /* Variables: */
    const int iWidth = width();
    const int iHeight = height();
    const QSize buttonPaneMinimumSizeHint = m_pButtonPane->minimumSizeHint();
    const int iButtonPaneMinimumWidth = buttonPaneMinimumSizeHint.width();
    const int iButtonPaneMinimumHeight = buttonPaneMinimumSizeHint.height();
    const int iTextPaneWidth = iWidth - 2 * m_iLayoutMargin - m_iLayoutSpacing - iButtonPaneMinimumWidth;
    const int iTextPaneHeight = m_pTextPane->minimumSizeHint().height();
    const int iMaximumHeight = qMax(iTextPaneHeight, iButtonPaneMinimumHeight);
    const int iMinimumHeight = qMin(iTextPaneHeight, iButtonPaneMinimumHeight);
    const int iHeightShift = (iMaximumHeight - iMinimumHeight) / 2;
    const bool fTextPaneShifted = iTextPaneHeight < iButtonPaneMinimumHeight;

    /* Text-pane: */
    m_pTextPane->move(m_iLayoutMargin,
                      fTextPaneShifted ? m_iLayoutMargin + iHeightShift : m_iLayoutMargin);
    m_pTextPane->resize(iTextPaneWidth,
                        iTextPaneHeight);
    m_pTextPane->layoutContent();
    /* Button-box: */
    m_pButtonPane->move(m_iLayoutMargin + iTextPaneWidth + m_iLayoutSpacing,
                        m_iLayoutMargin);
    m_pButtonPane->resize(iButtonPaneMinimumWidth,
                          iHeight - 2 * m_iLayoutMargin);
}

void UIPopupPane::sltButtonClicked(int iButtonID)
{
    done(iButtonID & AlertButtonMask);
}

void UIPopupPane::prepare()
{
    /* Install 'hover' animation for 'opacity' property: */
    UIAnimationFramework::installPropertyAnimation(this, "opacity", "defaultOpacity", "hoveredOpacity",
                                                   SIGNAL(sigHoverEnter()), SIGNAL(sigHoverLeave()));
    /* Prepare content: */
    prepareContent();
}

void UIPopupPane::prepareContent()
{
    /* Prepare this: */
    installEventFilter(this);
    /* Create message-label: */
    m_pTextPane = new UIPopupPaneTextPane(this);
    {
        /* Prepare label: */
        connect(m_pTextPane, SIGNAL(sigSizeHintChanged()),
                this, SIGNAL(sigSizeHintChanged()));
        m_pTextPane->installEventFilter(this);
        m_pTextPane->setText(m_strMessage);
        m_pTextPane->setProposeAutoConfirmation(m_fProposeAutoConfirmation);
    }
    /* Create button-box: */
    m_pButtonPane = new UIPopupPaneButtonPane(this);
    {
        /* Prepare button-box: */
        connect(m_pButtonPane, SIGNAL(sigButtonClicked(int)),
                this, SLOT(sltButtonClicked(int)));
        m_pButtonPane->installEventFilter(this);
        m_pButtonPane->setButtons(m_buttonDescriptions);
    }

    /* Prepare focus rules: */
    setFocusPolicy(Qt::StrongFocus);
    m_pTextPane->setFocusPolicy(Qt::StrongFocus);
    m_pButtonPane->setFocusPolicy(Qt::StrongFocus);
    setFocusProxy(m_pButtonPane);
    m_pTextPane->setFocusProxy(m_pButtonPane);
}

bool UIPopupPane::eventFilter(QObject *pWatched, QEvent *pEvent)
{
    /* Depending on event-type: */
    switch (pEvent->type())
    {
        /* Something is hovered: */
        case QEvent::HoverEnter:
        case QEvent::Enter:
        {
            if (!m_fHovered)
            {
                m_fHovered = true;
                emit sigHoverEnter();
            }
            break;
        }
        /* Nothing is hovered: */
        case QEvent::Leave:
        {
            if (pWatched == this && m_fHovered)
            {
                m_fHovered = false;
                emit sigHoverLeave();
            }
            break;
        }
        case QEvent::FocusIn:
        {
            if (!m_fFocused)
            {
                m_fFocused = true;
                emit sigFocusEnter();
            }
            break;
        }
        case QEvent::FocusOut:
        {
            if (m_fFocused)
            {
                m_fFocused = false;
                emit sigFocusLeave();
            }
            break;
        }
        /* Default case: */
        default: break;
    }
    /* Do not filter anything: */
    return false;
}

void UIPopupPane::paintEvent(QPaintEvent*)
{
    /* Compose painting rectangle: */
    const QRect rect(0, 0, width(), height());

    /* Create painter: */
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    /* Configure painter clipping: */
    QPainterPath path;
    int iDiameter = 6;
    QSizeF arcSize(2 * iDiameter, 2 * iDiameter);
    path.moveTo(iDiameter, 0);
    path.arcTo(QRectF(path.currentPosition(), arcSize).translated(-iDiameter, 0), 90, 90);
    path.lineTo(path.currentPosition().x(), rect.height() - iDiameter);
    path.arcTo(QRectF(path.currentPosition(), arcSize).translated(0, -iDiameter), 180, 90);
    path.lineTo(rect.width() - iDiameter, path.currentPosition().y());
    path.arcTo(QRectF(path.currentPosition(), arcSize).translated(-iDiameter, -2 * iDiameter), 270, 90);
    path.lineTo(path.currentPosition().x(), iDiameter);
    path.arcTo(QRectF(path.currentPosition(), arcSize).translated(-2 * iDiameter, -iDiameter), 0, 90);
    path.closeSubpath();
    painter.setClipPath(path);

    /* Fill with background: */
    QColor currentColor(palette().color(QPalette::Window));
    QColor newColor1(currentColor.red(), currentColor.green(), currentColor.blue(), opacity());
    QColor newColor2 = newColor1.darker(115);
    QLinearGradient headerGradient(rect.topLeft(), rect.topRight());
    headerGradient.setColorAt(0, newColor1);
    headerGradient.setColorAt(1, newColor2);
    painter.fillRect(rect, headerGradient);
}

void UIPopupPane::done(int iResultCode)
{
    /* Close the window: */
    close();

    /* Was the popup auto-confirmed? */
    if (m_pTextPane->isAutoConfirmed())
        iResultCode |= AlertOption_AutoConfirmed;

    /* Notify listeners: */
    emit sigDone(iResultCode);
}


UIPopupPaneTextPane::UIPopupPaneTextPane(QWidget *pParent /*= 0*/)
    : QIWithRetranslateUI<QWidget>(pParent)
    , m_iLayoutMargin(0)
    , m_iLayoutSpacing(10)
    , m_pLabel(0)
    , m_iDesiredLabelWidth(-1)
    , m_pAutoConfirmCheckBox(0)
    , m_fProposeAutoConfirmation(false)
    , m_fFocused(false)
    , m_pAnimation(0)
{
    /* Prepare: */
    prepare();
}

void UIPopupPaneTextPane::setText(const QString &strText)
{
    /* Make sure the text is changed: */
    if (m_pLabel->text() == strText)
        return;
    /* Update the pane for the new text: */
    m_pLabel->setText(strText);
    updateSizeHint();
}

void UIPopupPaneTextPane::setDesiredWidth(int iDesiredWidth)
{
    /* Make sure the desired-width is changed: */
    if (m_iDesiredLabelWidth == iDesiredWidth)
        return;
    /* Update the pane for the new desired-width: */
    m_iDesiredLabelWidth = iDesiredWidth;
    updateSizeHint();
}

void UIPopupPaneTextPane::setProposeAutoConfirmation(bool fPropose)
{
    /* Make sure the auto-confirmation-proposal is changed: */
    if (m_fProposeAutoConfirmation == fPropose)
        return;
    /* Update the pane for the new auto-confirmation-proposal: */
    m_fProposeAutoConfirmation = fPropose;
    updateSizeHint();
}

bool UIPopupPaneTextPane::autoConfirmationProposed() const
{
    return m_fProposeAutoConfirmation;
}

bool UIPopupPaneTextPane::isAutoConfirmed() const
{
    return autoConfirmationProposed() &&
           m_pAutoConfirmCheckBox &&
           m_pAutoConfirmCheckBox->isChecked();
}

QSize UIPopupPaneTextPane::minimumSizeHint() const
{
    /* Check if desired-width set: */
    if (m_iDesiredLabelWidth >= 0)
        /* Dependent size-hint: */
        return m_minimumSizeHint;
    /* Golden-rule size-hint by default: */
    return QWidget::minimumSizeHint();
}

void UIPopupPaneTextPane::setMinimumSizeHint(const QSize &minimumSizeHint)
{
    /* Make sure the size-hint is changed: */
    if (m_minimumSizeHint == minimumSizeHint)
        return;
    /* Assign new size-hint: */
    m_minimumSizeHint = minimumSizeHint;
    /* Notify parent popup-pane: */
    emit sigSizeHintChanged();
}

void UIPopupPaneTextPane::layoutContent()
{
    /* Variables: */
    const int iWidth = width();
    const int iHeight = height();
    const int iLabelWidth = m_labelSizeHint.width();
    const int iLabelHeight = m_labelSizeHint.height();
    /* Label: */
    m_pLabel->move(m_iLayoutMargin, m_iLayoutMargin);
    m_pLabel->resize(qMin(iWidth, iLabelWidth), qMin(iHeight, iLabelHeight));

    /* Check-box: */
    if (m_fProposeAutoConfirmation)
    {
        /* Variables: */
        const int iCheckBoxWidth = m_checkBoxSizeHint.width();
        const int iCheckBoxHeight = m_checkBoxSizeHint.height();
        const int iCheckBoxY = m_iLayoutMargin + iLabelHeight + m_iLayoutSpacing;
        /* Layout check-box: */
        if (iHeight - m_iLayoutMargin - iCheckBoxHeight - iCheckBoxY >= 0)
        {
            m_pAutoConfirmCheckBox->move(m_iLayoutMargin, iCheckBoxY);
            m_pAutoConfirmCheckBox->resize(iCheckBoxWidth, iCheckBoxHeight);
            if (m_pAutoConfirmCheckBox->isHidden())
                m_pAutoConfirmCheckBox->show();
        }
        else if (!m_pAutoConfirmCheckBox->isHidden())
            m_pAutoConfirmCheckBox->hide();
    }
    else if (!m_pAutoConfirmCheckBox->isHidden())
        m_pAutoConfirmCheckBox->hide();
}

void UIPopupPaneTextPane::sltFocusEnter()
{
    /* Ignore if already focused: */
    if (m_fFocused)
        return;

    /* Update focus state: */
    m_fFocused = true;

    /* Notify listeners: */
    emit sigFocusEnter();
}

void UIPopupPaneTextPane::sltFocusLeave()
{
    /* Ignore if already unfocused: */
    if (!m_fFocused)
        return;

    /* Update focus state: */
    m_fFocused = false;

    /* Notify listeners: */
    emit sigFocusLeave();
}

void UIPopupPaneTextPane::prepare()
{
    /* Propagate parent signals: */
    connect(parent(), SIGNAL(sigFocusEnter()), this, SLOT(sltFocusEnter()));
    connect(parent(), SIGNAL(sigFocusLeave()), this, SLOT(sltFocusLeave()));
    /* Prepare content: */
    prepareContent();
}

void UIPopupPaneTextPane::prepareContent()
{
    /* Create label: */
    m_pLabel = new QLabel(this);
    {
        /* Prepare label: */
        QFont currentFont = m_pLabel->font();
#ifdef Q_WS_MAC
        currentFont.setPointSize(currentFont.pointSize() - 2);
#else /* Q_WS_MAC */
        currentFont.setPointSize(currentFont.pointSize() - 1);
#endif /* !Q_WS_MAC */
        m_pLabel->setFont(currentFont);
        m_pLabel->setWordWrap(true);
        m_pLabel->setFocusPolicy(Qt::NoFocus);
    }
    /* Create check-box: */
    m_pAutoConfirmCheckBox = new QCheckBox(this);
    {
        /* Prepare check-box: */
        QFont currentFont = m_pAutoConfirmCheckBox->font();
#ifdef Q_WS_MAC
        currentFont.setPointSize(currentFont.pointSize() - 2);
#else /* Q_WS_MAC */
        currentFont.setPointSize(currentFont.pointSize() - 1);
#endif /* !Q_WS_MAC */
        m_pAutoConfirmCheckBox->setFont(currentFont);
        m_pAutoConfirmCheckBox->setFocusPolicy(Qt::NoFocus);
    }
    /* Translate UI finally: */
    retranslateUi();
}

void UIPopupPaneTextPane::retranslateUi()
{
    /* Translate auto-confirm check-box: */
    m_pAutoConfirmCheckBox->setText(QApplication::translate("UIMessageCenter", "Do not show this message again"));
}

void UIPopupPaneTextPane::updateSizeHint()
{
    /* Recalculate collapsed size-hint: */
    {
        /* Collapsed size-hint contains only one-text-line label: */
        QFontMetrics fm(m_pLabel->font(), m_pLabel);
        m_collapsedSizeHint = QSize(m_iDesiredLabelWidth, fm.height());
    }

    /* Recalculate expanded size-hint: */
    {
        /* Recalculate label size-hint: */
        m_labelSizeHint = QSize(m_iDesiredLabelWidth, m_pLabel->heightForWidth(m_iDesiredLabelWidth));
        /* Recalculate check-box size-hint: */
        m_checkBoxSizeHint = m_fProposeAutoConfirmation ? m_pAutoConfirmCheckBox->sizeHint() : QSize();
        /* Expanded size-hint contains full-size label: */
        m_expandedSizeHint = m_labelSizeHint;
        /* Expanded size-hint can contain check-box: */
        if (m_checkBoxSizeHint.isValid())
        {
            m_expandedSizeHint.setWidth(qMax(m_expandedSizeHint.width(), m_checkBoxSizeHint.width()));
            m_expandedSizeHint.setHeight(m_expandedSizeHint.height() + m_iLayoutSpacing + m_checkBoxSizeHint.height());
        }
    }

    /* Update current size-hint: */
    m_minimumSizeHint = m_fFocused ? m_expandedSizeHint : m_collapsedSizeHint;

    /* And reinstall size-hint animation: */
    delete m_pAnimation;
    m_pAnimation = UIAnimationFramework::installPropertyAnimation(this, "minimumSizeHint", "collapsedSizeHint", "expandedSizeHint",
                                                                  SIGNAL(sigFocusEnter()), SIGNAL(sigFocusLeave()),
                                                                  m_fFocused);
}


UIPopupPaneButtonPane::UIPopupPaneButtonPane(QWidget *pParent /*= 0*/)
    : QWidget(pParent)
    , m_iDefaultButton(0)
    , m_iEscapeButton(0)
{
    /* Prepare: */
    prepare();
}

void UIPopupPaneButtonPane::setButtons(const QMap<int, QString> &buttonDescriptions)
{
    /* Make sure something changed: */
    if (m_buttonDescriptions == buttonDescriptions)
        return;

    /* Assign new button-descriptions: */
    m_buttonDescriptions = buttonDescriptions;
    /* Recreate buttons: */
    cleanupButtons();
    prepareButtons();
}

void UIPopupPaneButtonPane::sltButtonClicked()
{
    /* Make sure the slot is called by the button: */
    QIToolButton *pButton = qobject_cast<QIToolButton*>(sender());
    if (!pButton)
        return;

    /* Make sure we still have that button: */
    int iButtonID = m_buttons.key(pButton, 0);
    if (!iButtonID)
        return;

    /* Notify listeners button was clicked: */
    emit sigButtonClicked(iButtonID);
}

void UIPopupPaneButtonPane::prepare()
{
    /* Prepare layouts: */
    prepareLayouts();
}

void UIPopupPaneButtonPane::prepareLayouts()
{
    /* Create layouts: */
    m_pButtonLayout = new QHBoxLayout;
    m_pButtonLayout->setContentsMargins(0, 0, 0, 0);
    m_pButtonLayout->setSpacing(0);
    QVBoxLayout *pMainLayout = new QVBoxLayout(this);
    pMainLayout->setContentsMargins(0, 0, 0, 0);
    pMainLayout->setSpacing(0);
    pMainLayout->addLayout(m_pButtonLayout);
    pMainLayout->addStretch();
}

void UIPopupPaneButtonPane::prepareButtons()
{
    /* Add all the buttons: */
    const QList<int> &buttonsIDs = m_buttonDescriptions.keys();
    foreach (int iButtonID, buttonsIDs)
        if (QIToolButton *pButton = addButton(iButtonID, m_buttonDescriptions[iButtonID]))
        {
            m_pButtonLayout->addWidget(pButton);
            m_buttons[iButtonID] = pButton;
            connect(pButton, SIGNAL(clicked(bool)), this, SLOT(sltButtonClicked()));
            if (pButton->property("default").toBool())
                m_iDefaultButton = iButtonID;
            if (pButton->property("escape").toBool())
                m_iEscapeButton = iButtonID;
        }
}

void UIPopupPaneButtonPane::cleanupButtons()
{
    /* Remove all the buttons: */
    const QList<int> &buttonsIDs = m_buttons.keys();
    foreach (int iButtonID, buttonsIDs)
    {
        delete m_buttons[iButtonID];
        m_buttons.remove(iButtonID);
    }
}

void UIPopupPaneButtonPane::keyPressEvent(QKeyEvent *pEvent)
{
    /* Depending on pressed key: */
    switch (pEvent->key())
    {
        case Qt::Key_Enter:
        case Qt::Key_Return:
        {
            if (m_iDefaultButton)
            {
                pEvent->accept();
                emit sigButtonClicked(m_iDefaultButton);
                return;
            }
            break;
        }
        case Qt::Key_Escape:
        {
            if (m_iEscapeButton)
            {
                pEvent->accept();
                emit sigButtonClicked(m_iEscapeButton);
                return;
            }
            break;
        }
        default:
            break;
    }
    /* Call to base-class: */
    QWidget::keyPressEvent(pEvent);
}

/* static */
QIToolButton* UIPopupPaneButtonPane::addButton(int iButtonID, const QString &strToolTip)
{
    /* Create button: */
    QIToolButton *pButton = new QIToolButton;
    pButton->removeBorder();
    pButton->setToolTip(strToolTip.isEmpty() ? defaultToolTip(iButtonID) : strToolTip);
    pButton->setIcon(defaultIcon(iButtonID));

    /* Sign the 'default' button: */
    if (iButtonID & AlertButtonOption_Default)
        pButton->setProperty("default", true);
    /* Sign the 'escape' button: */
    if (iButtonID & AlertButtonOption_Escape)
        pButton->setProperty("escape", true);

    /* Return button: */
    return pButton;
}

/* static */
QString UIPopupPaneButtonPane::defaultToolTip(int iButtonID)
{
    QString strToolTip;
    switch (iButtonID & AlertButtonMask)
    {
        case AlertButton_Ok:      strToolTip = QIMessageBox::tr("OK");     break;
        case AlertButton_Cancel:  strToolTip = QIMessageBox::tr("Cancel"); break;
        case AlertButton_Choice1: strToolTip = QIMessageBox::tr("Yes");    break;
        case AlertButton_Choice2: strToolTip = QIMessageBox::tr("No");     break;
        default:                  strToolTip = QString();                  break;
    }
    return strToolTip;
}

/* static */
QIcon UIPopupPaneButtonPane::defaultIcon(int iButtonID)
{
    QIcon icon;
    switch (iButtonID & AlertButtonMask)
    {
        case AlertButton_Ok:      icon = UIIconPool::iconSet(":/ok_16px.png");     break;
        case AlertButton_Cancel:  icon = UIIconPool::iconSet(":/cancel_16px.png"); break;
        case AlertButton_Choice1: break;
        case AlertButton_Choice2: break;
        default:                  break;
    }
    return icon;
}

#include "UIPopupPane.moc"

