/****************************************************************************
**
** Copyright (C) 2013 Donna Whisnant, a.k.a. Dewtronics.
** Contact: http://www.dewtronics.com/
**
** This file is part of the KJVCanOpener Application as originally written
** and developed for Bethel Church, Festus, MO.
**
** GNU General Public License Usage
** This file may be used under the terms of the GNU General Public License
** version 3.0 as published by the Free Software Foundation and appearing
** in the file gpl-3.0.txt included in the packaging of this file. Please
** review the following information to ensure the GNU General Public License
** version 3.0 requirements will be met:
** http://www.gnu.org/copyleft/gpl.html.
**
** Other Usage
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and
** Dewtronics.
**
****************************************************************************/

#include "SubControls.h"

#include <QFontMetrics>
#include <QStyle>
#include <QApplication>
#include <QTextDocumentFragment>
#include <QAbstractItemView>

#if QT_VERSION < 0x050000
#include <QInputContext>
#endif

// ============================================================================

CComboBox::CComboBox(QWidget *pParent)
	:	QComboBox(pParent)
{

}

CComboBox::~CComboBox()
{

}

void CComboBox::keyPressEvent(QKeyEvent *pEvent)
{
	// let base class handle the event
	QComboBox::keyPressEvent(pEvent);

	if ((pEvent->key() == Qt::Key_Enter) || (pEvent->key() == Qt::Key_Return)) {
		// Process enter/return so it won't propagate and "accept" the parent dialog:
		pEvent->accept();
		emit enterPressed();
	}
}

// ============================================================================

CFontComboBox::CFontComboBox(QWidget *pParent)
	:	QFontComboBox(pParent)
{

}

CFontComboBox::~CFontComboBox()
{

}

void CFontComboBox::keyPressEvent(QKeyEvent *pEvent)
{
	// let base class handle the event
	QFontComboBox::keyPressEvent(pEvent);

	if ((pEvent->key() == Qt::Key_Enter) || (pEvent->key() == Qt::Key_Return)) {
		// Process enter/return so it won't propagate and "accept" the parent dialog:
		pEvent->accept();
		emit enterPressed();
	}
}

// ============================================================================

CSpinBox::CSpinBox(QWidget *pParent)
	:	QSpinBox(pParent)
{

}

CSpinBox::~CSpinBox()
{

}

void CSpinBox::keyPressEvent(QKeyEvent *pEvent)
{
	// let base class handle the event
	QSpinBox::keyPressEvent(pEvent);

	if ((pEvent->key() == Qt::Key_Enter) || (pEvent->key() == Qt::Key_Return)) {
		// Process enter/return so it won't propagate and "accept" the parent dialog:
		pEvent->accept();
		emit enterPressed();
	}
}

// ============================================================================

CDoubleSpinBox::CDoubleSpinBox(QWidget *pParent)
	:	QDoubleSpinBox(pParent)
{

}

CDoubleSpinBox::~CDoubleSpinBox()
{

}

void CDoubleSpinBox::keyPressEvent(QKeyEvent *pEvent)
{
	// let base class handle the event
	QDoubleSpinBox::keyPressEvent(pEvent);

	if ((pEvent->key() == Qt::Key_Enter) || (pEvent->key() == Qt::Key_Return)) {
		// Process enter/return so it won't propagate and "accept" the parent dialog:
		pEvent->accept();
		emit enterPressed();
	}
}

// ============================================================================

#if QT_VERSION < 0x050000

bool CComposingCompleter::eventFilter(QObject *obj, QEvent *ev)
{
	// The act of popping our completer, will cause the inputContext to
	//		shift focus from the editor to the popup and after dismissing the
	//		popup, it doesn't go back to the editor.  So, since we are eating
	//		FocusOut events in the popup, push the inputContext focus back to
	//		the editor when we "focus out".  It's our focusProxy anyway:
	if ((ev->type() == QEvent::FocusOut) && (obj == widget())) {
		if ((popup()) && (popup()->isVisible())) {
			QInputContext *pInputContext = popup()->inputContext();
			if (pInputContext) pInputContext->setFocusWidget(popup());
		}
	}

	return QCompleter::eventFilter(obj, ev);
}

#endif

// ============================================================================

CSingleLineTextEdit::CSingleLineTextEdit(int nMinHeight, QWidget *pParent)
	:	QTextEdit(pParent),
		m_nMinHeight(nMinHeight),
		m_bUpdateInProgress(false)
{
	connect(this, SIGNAL(textChanged()), this, SLOT(en_textChanged()));
	connect(this, SIGNAL(cursorPositionChanged()), this, SLOT(en_cursorPositionChanged()));
}

CSingleLineTextEdit::~CSingleLineTextEdit()
{

}

QSize CSingleLineTextEdit::sizeHint() const
{
	QFontMetrics fm(font());
	int h = qMax(fm.height(), 14) + 4;
	int w = fm.width(QLatin1Char('x')) * 17 + 4;
	QStyleOptionFrameV2 opt;
	opt.initFrom(this);
	QSize szHint = style()->sizeFromContents(QStyle::CT_LineEdit, &opt, QSize(w, h).
											 expandedTo(QApplication::globalStrut()), this);
	return (QSize(szHint.width(), ((m_nMinHeight != -1) ? qMax(szHint.height(), m_nMinHeight) : szHint.height())));
}

void CSingleLineTextEdit::insertFromMimeData(const QMimeData * source)
{
	if (!(textInteractionFlags() & Qt::TextEditable) || !source) return;

	// For reference if we ever re-enable rich text:  (don't forget to change acceptRichText setting in constructor)
	//	if (source->hasFormat(QLatin1String("application/x-qrichtext")) && acceptRichText()) {
	//		// x-qrichtext is always UTF-8 (taken from Qt3 since we don't use it anymore).
	//		QString richtext = QString::fromUtf8(source->data(QLatin1String("application/x-qrichtext")));
	//		richtext.prepend(QLatin1String("<meta name=\"qrichtext\" content=\"1\" />"));
	//		fragment = QTextDocumentFragment::fromHtml(richtext, document());
	//		bHasData = true;
	//	} else if (source->hasHtml() && acceptRichText()) {
	//		fragment = QTextDocumentFragment::fromHtml(source->html(), document());
	//		bHasData = true;
	//	} else {


	if (source->hasText()) {
		bool bHasData = false;
		QTextDocumentFragment fragment;
		QString text = source->text();
		// Change all newlines to spaces, since we are simulating a single-line editor:
		if (!text.isNull()) {
			text.replace("\r","");
			text.replace("\n"," ");
			if (!text.isEmpty()) {
				fragment = QTextDocumentFragment::fromPlainText(text);
				bHasData = true;
			}
		}
		if (bHasData) textCursor().insertFragment(fragment);
	}

	ensureCursorVisible();
}

bool CSingleLineTextEdit::canInsertFromMimeData(const QMimeData *source) const
{
	return QTextEdit::canInsertFromMimeData(source);
}

void CSingleLineTextEdit::wheelEvent(QWheelEvent *event)
{
	event->ignore();
}

void CSingleLineTextEdit::focusInEvent(QFocusEvent *event)
{
	QTextEdit::focusInEvent(event);

#if QT_VERSION < 0x050000
	// The following is needed to fix the QCompleter bug
	//	where the inputContext doesn't shift correctly
	//	from the QCompleter->popup back to the editor:
	//	(Only applies to Qt 4.8.x and seems to be fixed in Qt5)
	QInputContext *pInputContext = inputContext();
	if (pInputContext) pInputContext->setFocusWidget(this);
#endif
}

void CSingleLineTextEdit::keyPressEvent(QKeyEvent *event)
{
	bool bForceCompleter = false;

	switch (event->key()) {
		case Qt::Key_Enter:
		case Qt::Key_Return:
			emit enterTriggered();
			// fall-through to the event-ignore() so we can still process it for the completer logic
		case Qt::Key_Escape:
		case Qt::Key_Tab:
		case Qt::Key_Control:			// Control is needed here to keep Ctrl-Home/Ctrl-End used in the QCompleter from trigger redoing the QCompleter in setupCompleter()
			event->ignore();
			return;

		case Qt::Key_Down:
			bForceCompleter = true;
			break;
	}

	QTextEdit::keyPressEvent(event);

	// Ctrl/Cmd modifier is needed here to keep QCompleter from triggering on our editor shortcuts via setupCompleter()
	if (event->modifiers() & Qt::ControlModifier) return;

	setupCompleter(event->text(), bForceCompleter);
}

void CSingleLineTextEdit::inputMethodEvent(QInputMethodEvent *event)
{
	// Call parent:
	QTextEdit::inputMethodEvent(event);
	setupCompleter(event->commitString(), false);
}

QString CSingleLineTextEdit::textUnderCursor() const
{
	QTextCursor cursor = textCursor();
	cursor.select(QTextCursor::WordUnderCursor);
	return cursor.selectedText();
}

void CSingleLineTextEdit::en_textChanged()
{
	if (!updateInProgress()) UpdateCompleter();
}

void CSingleLineTextEdit::en_cursorPositionChanged()
{
	if (!updateInProgress()) UpdateCompleter();
}

// ============================================================================
