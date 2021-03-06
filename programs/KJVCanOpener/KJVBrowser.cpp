/****************************************************************************
**
** Copyright (C) 2012 Donna Whisnant, a.k.a. Dewtronics.
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

#include "KJVBrowser.h"
#include "VerseListModel.h"
#include "UserNotesDatabase.h"
#include "dbDescriptors.h"

#include "BusyCursor.h"

#include <assert.h>

#include <QComboBox>
#include <QTextBrowser>
#include <QTextCharFormat>
#include <QTextBlock>
#include <QTextFragment>
#include <QApplication>
#include <QScrollBar>
#include <QToolTip>
#include <QMouseEvent>
#include <QStyleOptionSlider>
#ifndef PLASTIQUE_STATIC
#include <QStyle>
#include <QStyleFactory>
#include <QProxyStyle>
#endif

// ============================================================================

CKJVBrowser::CKJVBrowser(CVerseListModel *pModel, CBibleDatabasePtr pBibleDatabase, QWidget *parent) :
	QWidget(parent),
	m_pBibleDatabase(pBibleDatabase),
	m_ndxCurrent(0),
	m_SearchResultsHighlighter(pModel, false),
	m_ExcludedSearchResultsHighlighter(pModel, true),
	m_bShowExcludedSearchResults(CPersistentSettings::instance()->showExcludedSearchResultsInBrowser()),
	m_bDoingUpdate(false),
#ifndef PLASTIQUE_STATIC
	m_pPlastiqueStyle(NULL),
#endif
	m_bDoingPassageReference(false),
	m_pScriptureBrowser(NULL)
{
	assert(!m_pBibleDatabase.isNull());
	assert(!g_pUserNotesDatabase.isNull());

	ui.setupUi(this);

#ifndef PLASTIQUE_STATIC
	m_pPlastiqueStyle = new QProxyStyle(QStyleFactory::create("plastique"));
#endif

	initialize();

	assert(m_pScriptureBrowser != NULL);

	ui.lblBibleDatabaseName->setText(m_pBibleDatabase->description());

	setNavigationActivationDelay(CPersistentSettings::instance()->navigationActivationDelay());
	setPassageReferenceActivationDelay(CPersistentSettings::instance()->passageReferenceActivationDelay());
	setBrowserNavigationPaneMode(CPersistentSettings::instance()->browserNavigationPaneMode());

	connect(CPersistentSettings::instance(), SIGNAL(changedNavigationActivationDelay(int)), this, SLOT(setNavigationActivationDelay(int)));
	connect(CPersistentSettings::instance(), SIGNAL(changedPassageReferenceActivationDelay(int)), this, SLOT(setPassageReferenceActivationDelay(int)));
	connect(CPersistentSettings::instance(), SIGNAL(changedChapterScrollbarMode(CHAPTER_SCROLLBAR_MODE_ENUM)), this, SLOT(en_changedChapterScrollbarMode()));
	connect(CPersistentSettings::instance(), SIGNAL(changedBrowserNavigationPaneMode(BROWSER_NAVIGATION_PANE_MODE_ENUM)), this, SLOT(setBrowserNavigationPaneMode(BROWSER_NAVIGATION_PANE_MODE_ENUM)));

// Data Connections:
	connect(pModel, SIGNAL(verseListAboutToChange()), this, SLOT(en_SearchResultsVerseListAboutToChange()));
	connect(pModel, SIGNAL(verseListChanged()), this, SLOT(en_SearchResultsVerseListChanged()));

// UI Connections:
	connect(m_pScriptureBrowser, SIGNAL(gotoIndex(const TPhraseTag &)), &m_dlyGotoIndex, SLOT(trigger(const TPhraseTag &)));
	connect(&m_dlyGotoIndex, SIGNAL(triggered(const TPhraseTag &)), this, SLOT(gotoIndex(const TPhraseTag &)));
	connect(this, SIGNAL(en_gotoIndex(const TPhraseTag &)), m_pScriptureBrowser, SLOT(en_gotoIndex(const TPhraseTag &)));
	connect(m_pScriptureBrowser, SIGNAL(sourceChanged(const QUrl &)), this, SLOT(en_sourceChanged(const QUrl &)));
	connect(m_pScriptureBrowser, SIGNAL(cursorPositionChanged()), this, SLOT(en_selectionChanged()));

	connect(ui.btnHideNavigation, SIGNAL(clicked()), this, SLOT(en_clickedHideNavigationPane()));

	connect(ui.comboBk, SIGNAL(currentIndexChanged(int)), this, SLOT(delayBkComboIndexChanged(int)));
	connect(ui.comboBkChp, SIGNAL(currentIndexChanged(int)), this, SLOT(delayBkChpComboIndexChanged(int)));
	connect(ui.comboTstBk, SIGNAL(currentIndexChanged(int)), this, SLOT(delayTstBkComboIndexChanged(int)));
	connect(ui.comboTstChp, SIGNAL(currentIndexChanged(int)), this, SLOT(delayTstChpComboIndexChanged(int)));
	connect(ui.comboBibleBk, SIGNAL(currentIndexChanged(int)), this, SLOT(delayBibleBkComboIndexChanged(int)));
	connect(ui.comboBibleChp, SIGNAL(currentIndexChanged(int)), this, SLOT(delayBibleChpComboIndexChanged(int)));

	connect(ui.widgetPassageReference, SIGNAL(passageReferenceChanged(const TPhraseTag &)), this, SLOT(delayPassageReference(const TPhraseTag &)));
	connect(ui.widgetPassageReference, SIGNAL(enterPressed()), this, SLOT(PassageReferenceEnterPressed()));

	connect(&m_dlyBkCombo, SIGNAL(triggered(int)), this, SLOT(BkComboIndexChanged(int)));
	connect(&m_dlyBkChpCombo, SIGNAL(triggered(int)), this, SLOT(BkChpComboIndexChanged(int)));
	connect(&m_dlyTstBkCombo, SIGNAL(triggered(int)), this, SLOT(TstBkComboIndexChanged(int)));
	connect(&m_dlyTstChpCombo, SIGNAL(triggered(int)), this, SLOT(TstChpComboIndexChanged(int)));
	connect(&m_dlyBibleBkCombo, SIGNAL(triggered(int)), this, SLOT(BibleBkComboIndexChanged(int)));
	connect(&m_dlyBibleChpCombo, SIGNAL(triggered(int)), this, SLOT(BibleChpComboIndexChanged(int)));

	connect(&m_dlyPassageReference, SIGNAL(triggered(const TPhraseTag &)), this, SLOT(PassageReferenceChanged(const TPhraseTag &)));

	connect(m_pScriptureBrowser, SIGNAL(activatedScriptureText()), this, SLOT(en_activatedScriptureText()));
	connect(ui.widgetPassageReference, SIGNAL(activatedPassageReference()), this, SLOT(en_activatedPassageReference()));

	// Set Outgoing Pass-Through Signals:
	connect(m_pScriptureBrowser, SIGNAL(backwardAvailable(bool)), this, SIGNAL(backwardAvailable(bool)));
	connect(m_pScriptureBrowser, SIGNAL(forwardAvailable(bool)), this, SIGNAL(forwardAvailable(bool)));
	connect(m_pScriptureBrowser, SIGNAL(historyChanged()), this, SIGNAL(historyChanged()));

	// Set Incoming Pass-Through Signals:
	connect(this, SIGNAL(backward()), m_pScriptureBrowser, SLOT(backward()));
	connect(this, SIGNAL(forward()), m_pScriptureBrowser, SLOT(forward()));
	connect(this, SIGNAL(home()), m_pScriptureBrowser, SLOT(home()));
	connect(this, SIGNAL(reload()), m_pScriptureBrowser, SLOT(reload()));
	connect(this, SIGNAL(rerender()), m_pScriptureBrowser, SLOT(rerender()));

	// Highlighting colors changing:
	connect(CPersistentSettings::instance(), SIGNAL(changedColorSearchResults(const QColor &)), this, SLOT(en_SearchResultsColorChanged(const QColor &)));
	connect(CPersistentSettings::instance(), SIGNAL(changedColorWordsOfJesus(const QColor &)), this, SLOT(en_WordsOfJesusColorChanged(const QColor &)));
	connect(CPersistentSettings::instance(), SIGNAL(changedShowExcludedSearchResultsInBrowser(bool)), this, SLOT(en_ShowExcludedSearchResultsChanged(bool)));

	connect(g_pUserNotesDatabase.data(), SIGNAL(highlighterTagsAboutToChange(CBibleDatabasePtr, const QString &)), this, SLOT(en_highlighterTagsAboutToChange(CBibleDatabasePtr, const QString &)));
	connect(g_pUserNotesDatabase.data(), SIGNAL(highlighterTagsChanged(CBibleDatabasePtr, const QString &)), this, SLOT(en_highlighterTagsChanged(CBibleDatabasePtr, const QString &)));
	connect(g_pUserNotesDatabase.data(), SIGNAL(aboutToChangeHighlighters()), this, SLOT(en_highlightersAboutToChange()));
	connect(g_pUserNotesDatabase.data(), SIGNAL(changedHighlighters()), this, SLOT(en_highlightersChanged()));

	// User Notes changing:
	connect(g_pUserNotesDatabase.data(), SIGNAL(addedUserNote(const CRelIndex &)), this, SLOT(en_userNoteEvent(const CRelIndex &)));
	connect(g_pUserNotesDatabase.data(), SIGNAL(changedUserNote(const CRelIndex &)), this, SLOT(en_userNoteEvent(const CRelIndex &)));
	connect(g_pUserNotesDatabase.data(), SIGNAL(removedUserNote(const CRelIndex &)), this, SLOT(en_userNoteEvent(const CRelIndex &)));

	// Cross Refs changing:
	connect(g_pUserNotesDatabase.data(), SIGNAL(addedCrossRef(const CRelIndex &, const CRelIndex &)), this, SLOT(en_crossRefsEvent(const CRelIndex &, const CRelIndex &)));
	connect(g_pUserNotesDatabase.data(), SIGNAL(removedCrossRef(const CRelIndex &, const CRelIndex &)), this, SLOT(en_crossRefsEvent(const CRelIndex &, const CRelIndex &)));
	connect(g_pUserNotesDatabase.data(), SIGNAL(changedAllCrossRefs()), this, SLOT(en_allCrossRefsChanged()));
}

CKJVBrowser::~CKJVBrowser()
{
#ifndef PLASTIQUE_STATIC
	if (m_pPlastiqueStyle) {
		delete m_pPlastiqueStyle;
		m_pPlastiqueStyle = NULL;
	}
#endif
}

// ----------------------------------------------------------------------------

bool CKJVBrowser::eventFilter(QObject *obj, QEvent *ev)
{
	if ((ui.scrollbarChapter != NULL) &&
		(obj == ui.scrollbarChapter) &&
		(ev->type() == QEvent::MouseMove) &&
		(ui.scrollbarChapter->isSliderDown())) {
		QMouseEvent *pMouseEvent = static_cast<QMouseEvent*>(ev);
		m_ptChapterScrollerMousePos = pMouseEvent->globalPos();
	}

	return QWidget::eventFilter(obj, ev);
}

// ----------------------------------------------------------------------------

void CKJVBrowser::setNavigationActivationDelay(int nDelay)
{
	m_dlyBkCombo.setMinimumDelay(nDelay);
	m_dlyBkChpCombo.setMinimumDelay(nDelay);
	m_dlyTstBkCombo.setMinimumDelay(nDelay);
	m_dlyTstChpCombo.setMinimumDelay(nDelay);
	m_dlyBibleBkCombo.setMinimumDelay(nDelay);
	m_dlyBibleChpCombo.setMinimumDelay(nDelay);

	m_dlyGotoIndex.setMinimumDelay(nDelay);
}

void CKJVBrowser::setPassageReferenceActivationDelay(int nDelay)
{
	m_dlyPassageReference.setMinimumDelay(nDelay);
}

// ----------------------------------------------------------------------------

void CKJVBrowser::en_clickedHideNavigationPane()
{
	switch (CPersistentSettings::instance()->browserNavigationPaneMode()) {
		case BNPME_HIDDEN:
			CPersistentSettings::instance()->setBrowserNavigationPaneMode(BNPME_PASSAGE_REF_ONLY);
			break;
		case BNPME_PASSAGE_REF_ONLY:
			CPersistentSettings::instance()->setBrowserNavigationPaneMode(BNPME_COMPLETE);
			break;
		case BNPME_COMPLETE:
			CPersistentSettings::instance()->setBrowserNavigationPaneMode(BNPME_HIDDEN);
			break;
	}
}

void CKJVBrowser::setBrowserNavigationPaneMode(BROWSER_NAVIGATION_PANE_MODE_ENUM nBrowserNavigationPaneMode)
{
	switch (nBrowserNavigationPaneMode) {
		case BNPME_COMPLETE:
			ui.btnHideNavigation->setArrowType(Qt::DownArrow);
			ui.btnHideNavigation->setChecked(true);
			ui.frameNavigationPane->setVisible(true);
			ui.widgetPassageReference->setVisible(true);
			break;

		case BNPME_PASSAGE_REF_ONLY:
			ui.btnHideNavigation->setArrowType(Qt::RightArrow);
			ui.btnHideNavigation->setChecked(true);
			ui.frameNavigationPane->setVisible(false);
			ui.widgetPassageReference->setVisible(true);
			break;

		case BNPME_HIDDEN:
			ui.btnHideNavigation->setArrowType(Qt::UpArrow);
			ui.btnHideNavigation->setChecked(false);
			ui.frameNavigationPane->setVisible(false);
			ui.widgetPassageReference->setVisible(false);
			break;
	}
}

// ----------------------------------------------------------------------------

void CKJVBrowser::initialize()
{
	// --------------------------------------------------------------

	ui.widgetPassageReference->initialize(m_pBibleDatabase);

	// --------------------------------------------------------------

	//	Swapout the widgetKJVPassageNavigator from the layout with
	//		one that we can set the database on:

	int ndx = ui.gridLayout->indexOf(ui.textBrowserMainText);
	assert(ndx != -1);
	if (ndx == -1) return;
	int nRow;
	int nCol;
	int nRowSpan;
	int nColSpan;
	ui.gridLayout->getItemPosition(ndx, &nRow, &nCol, &nRowSpan, &nColSpan);

	int ndxChapterScrollbar = ui.gridLayout->indexOf(ui.scrollbarChapter);
	assert(ndxChapterScrollbar != -1);
	if (ndxChapterScrollbar == -1) return;
	int nRowChapterScrollbar;
	int nColChapterScrollbar;
	int nRowSpanChapterScrollbar;
	int nColSpanChapterScrollbar;
	ui.gridLayout->getItemPosition(ndxChapterScrollbar, &nRowChapterScrollbar, &nColChapterScrollbar, &nRowSpanChapterScrollbar, &nColSpanChapterScrollbar);

	assert(nRow == nRowChapterScrollbar);
	assert(nColSpan == 1);
	assert(nColSpanChapterScrollbar == 1);

	m_pScriptureBrowser = new CScriptureBrowser(m_pBibleDatabase, this);
	m_pScriptureBrowser->setObjectName(QString::fromUtf8("textBrowserMainText"));
	m_pScriptureBrowser->setMouseTracking(true);
	en_changedScrollbarsEnabled(CPersistentSettings::instance()->scrollbarsEnabled());
	m_pScriptureBrowser->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	m_pScriptureBrowser->setTabChangesFocus(false);
	m_pScriptureBrowser->setTextInteractionFlags(Qt::TextSelectableByKeyboard | Qt::TextSelectableByMouse | Qt::LinksAccessibleByMouse | Qt::LinksAccessibleByKeyboard);
	m_pScriptureBrowser->setOpenLinks(false);
	connect(CPersistentSettings::instance(), SIGNAL(changedScrollbarsEnabled(bool)), this, SLOT(en_changedScrollbarsEnabled(bool)));

	bool bChapterScrollNone = (CPersistentSettings::instance()->chapterScrollbarMode() == CSME_NONE);
	bool bChapterScrollLeft = (CPersistentSettings::instance()->chapterScrollbarMode() == CSME_LEFT);

	delete ui.textBrowserMainText;
	delete ui.scrollbarChapter;
	ui.textBrowserMainText = NULL;
	ui.scrollbarChapter = NULL;
	ui.gridLayout->addWidget(m_pScriptureBrowser, nRow, (bChapterScrollLeft ? nColChapterScrollbar : nCol), nRowSpan, (bChapterScrollNone ? (nColSpan + nColSpanChapterScrollbar) : nColSpan));

	if (!bChapterScrollNone) {
		ui.scrollbarChapter = new QScrollBar(this);
		ui.scrollbarChapter->setObjectName(QString::fromUtf8("scrollbarChapter"));
		ui.scrollbarChapter->setOrientation(Qt::Vertical);
		ui.gridLayout->addWidget(ui.scrollbarChapter, nRowChapterScrollbar, (bChapterScrollLeft ? nCol : nColChapterScrollbar), nRowSpanChapterScrollbar, nColSpanChapterScrollbar);
	}

	// Reinsert it in the correct TabOrder:
	QWidget::setTabOrder(ui.comboBkChp, m_pScriptureBrowser);
	QWidget::setTabOrder(m_pScriptureBrowser, ui.comboTstBk);

	// --------------------------------------------------------------

	begin_update();

	unsigned int nBibleChp = 0;
	ui.comboBk->clear();
	ui.comboBibleBk->clear();
	ui.comboBibleChp->clear();
	for (unsigned int ndxBk=1; ndxBk<=m_pBibleDatabase->bibleEntry().m_nNumBk; ++ndxBk) {
		const CBookEntry *pBook = m_pBibleDatabase->bookEntry(ndxBk);
		assert(pBook != NULL);
		nBibleChp += pBook->m_nNumChp;
		if (pBook->m_nNumWrd == 0) continue;		// Skip books that are empty (partial database support)
		ui.comboBk->addItem(pBook->m_strBkName, ndxBk);
		ui.comboBibleBk->addItem(QString("%1").arg(ndxBk), ndxBk);
		for (unsigned int ndxBibleChp=nBibleChp-pBook->m_nNumChp+1; ndxBibleChp<=nBibleChp; ++ndxBibleChp) {
			ui.comboBibleChp->addItem(QString("%1").arg(ndxBibleChp), ndxBibleChp);
		}
	}

	// Setup the Chapter Scroller:
	setupChapterScrollbar();

	end_update();
}

void CKJVBrowser::en_changedScrollbarsEnabled(bool bEnabled)
{
	assert(m_pScriptureBrowser != NULL);
	if (bEnabled) {
		m_pScriptureBrowser->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
	} else {
		m_pScriptureBrowser->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	}
}

void CKJVBrowser::en_changedChapterScrollbarMode()
{
	bool bChapterScrollNone = (CPersistentSettings::instance()->chapterScrollbarMode() == CSME_NONE);
	bool bChapterScrollLeft = (CPersistentSettings::instance()->chapterScrollbarMode() == CSME_LEFT);

	int ndx = ui.gridLayout->indexOf(m_pScriptureBrowser);
	assert(ndx != -1);
	if (ndx == -1) return;
	int nRow;
	int nCol;
	int nRowSpan;
	int nColSpan;
	ui.gridLayout->getItemPosition(ndx, &nRow, &nCol, &nRowSpan, &nColSpan);
	nCol = (bChapterScrollLeft ? 1 : 0);
	nColSpan = (bChapterScrollNone ? 2 : 1);
	ui.gridLayout->takeAt(ndx);

	int ndxChapterScrollbar = -1;
	if (ui.scrollbarChapter != NULL) {
		ndxChapterScrollbar = ui.gridLayout->indexOf(ui.scrollbarChapter);
		assert(ndxChapterScrollbar != -1);
		if (ndxChapterScrollbar == -1) return;
	}
	int nRowChapterScrollbar = nRow;
	int nColChapterScrollbar;		// This one will be set below
	int nRowSpanChapterScrollbar = nRowSpan;
	int nColSpanChapterScrollbar = 1;
	if (ndxChapterScrollbar != -1) {
		ui.gridLayout->getItemPosition(ndxChapterScrollbar, &nRowChapterScrollbar, &nColChapterScrollbar, &nRowSpanChapterScrollbar, &nColSpanChapterScrollbar);
		ui.gridLayout->takeAt(ndxChapterScrollbar);
	}
	assert(nRowChapterScrollbar == nRow);
	nColChapterScrollbar = (bChapterScrollLeft ? 0 : 1);
	assert(nRowSpanChapterScrollbar == nRowSpan);
	assert(nColSpanChapterScrollbar == 1);

	if (!bChapterScrollNone) {
		if (ui.scrollbarChapter == NULL) {
			ui.scrollbarChapter = new QScrollBar(this);
			setupChapterScrollbar();
		}
	} else {
		if (ui.scrollbarChapter != NULL) {
			delete ui.scrollbarChapter;
			ui.scrollbarChapter = NULL;
		}
	}

	ui.gridLayout->addWidget(m_pScriptureBrowser, nRow, nCol, nRowSpan, nColSpan);
	if (ui.scrollbarChapter != NULL) {
		ui.gridLayout->addWidget(ui.scrollbarChapter, nRowChapterScrollbar, nColChapterScrollbar, nRowSpanChapterScrollbar, nColSpanChapterScrollbar);
	}

	if (ui.scrollbarChapter != NULL) {
//		rerender();
		ui.scrollbarChapter->setValue(CRefCountCalc(m_pBibleDatabase.data(), CRefCountCalc::RTE_CHAPTER, m_ndxCurrent).ofBible().first);
	}
}

void CKJVBrowser::setupChapterScrollbar()
{
	if (ui.scrollbarChapter != NULL) {
#ifndef PLASTIQUE_STATIC
		assert(m_pPlastiqueStyle != NULL);
		ui.scrollbarChapter->setStyle(m_pPlastiqueStyle);
#else
		ui.scrollbarChapter->setStyle(&m_PlastiqueStyle);
#endif
		ui.scrollbarChapter->setRange(1, m_pBibleDatabase->bibleEntry().m_nNumChp);
		ui.scrollbarChapter->setTracking(true);
		ui.scrollbarChapter->setMouseTracking(true);
		ui.scrollbarChapter->installEventFilter(this);
		ui.scrollbarChapter->setSingleStep(1);
		ui.scrollbarChapter->setPageStep(3);

		connect(ui.scrollbarChapter, SIGNAL(valueChanged(int)), this, SLOT(ChapterSliderMoved(int)));
		connect(ui.scrollbarChapter, SIGNAL(sliderMoved(int)), this, SLOT(ChapterSliderMoved(int)));
		connect(ui.scrollbarChapter, SIGNAL(sliderReleased()), this, SLOT(ChapterSliderValueChanged()));
	}
}

void CKJVBrowser::gotoPassageReference(const QString &strPassageReference)
{
	ui.widgetPassageReference->setPassageReference(strPassageReference);
	PassageReferenceEnterPressed();				// Simulate pressing enter to immediately jump to it and focus the browser
}

void CKJVBrowser::gotoIndex(const TPhraseTag &tag)
{
	unsigned int nWordCount = ((tag.relIndex().word() != 0) ? tag.count() : 0);

	// Note: Special case !tag->relIndex().isSet() means reload current index
	// Note: Denormalize/Normalize allows us to automagically skip empty chapters (such as in the Additions to Esther in the Apocrypha)
	TPhraseTag tagActual = (tag.relIndex().isSet() ? TPhraseTag(m_pBibleDatabase->DenormalizeIndex(m_pBibleDatabase->NormalizeIndex(tag.relIndex())), nWordCount)
												   : TPhraseTag(m_ndxCurrent, tag.count()));

	begin_update();

	if (!m_bDoingPassageReference) ui.widgetPassageReference->clear();

	// If branching to a "book only", goto chapter 1 of that book:
	if ((tagActual.relIndex().book() != 0) &&
		(tagActual.relIndex().chapter() == 0) &&
		(tagActual.relIndex().word() == 0)) tagActual.relIndex().setChapter(1);

	m_pScriptureBrowser->setSource(QString("#%1").arg(tagActual.relIndex().asAnchor()));

	end_update();

	gotoIndex2(tag.relIndex().isSet() ? tagActual : tag);		// Pass special-case on to gotoIndex2
}

void CKJVBrowser::gotoIndex2(const TPhraseTag &tag)
{
	// Note: Special case !tag->relIndex().isSet() means reload current index
	TPhraseTag tagActual = (tag.relIndex().isSet() ? tag : TPhraseTag(m_ndxCurrent, tag.count()));

	setBook(tagActual.relIndex());
	setChapter(tagActual.relIndex());
	setVerse(tagActual.relIndex());
	setWord(tagActual);

	doHighlighting();

	emit en_gotoIndex(tagActual);
}

void CKJVBrowser::en_selectionChanged()
{
	TPhraseTag tagSelection = m_pScriptureBrowser->selection().primarySelection();

	if ((tagSelection.isSet()) && (tagSelection.count() < 2)) {
		emit wordUnderCursorChanged(m_pBibleDatabase->wordAtIndex(m_pBibleDatabase->NormalizeIndex(tagSelection.relIndex())));
	}
}

void CKJVBrowser::en_sourceChanged(const QUrl &src)
{
	assert(!m_pBibleDatabase.isNull());

	if (m_bDoingUpdate) return;

	QString strURL = src.toString();		// Internal URLs are in the form of "#nnnnnnnn" as anchors
	int nPos = strURL.indexOf('#');
	if (nPos > -1) {
		CRelIndex ndxRel(strURL.mid(nPos+1));
		if (ndxRel.isSet()) gotoIndex2(TPhraseTag(ndxRel));
	}
}

void CKJVBrowser::setFocusBrowser()
{
	m_pScriptureBrowser->setFocus();
}

bool CKJVBrowser::hasFocusBrowser() const
{
	return m_pScriptureBrowser->hasFocus();
}

void CKJVBrowser::setFocusPassageReferenceEditor()
{
	ui.widgetPassageReference->setFocus();
}

bool CKJVBrowser::hasFocusPassageReferenceEditor() const
{
	return ui.widgetPassageReference->hasFocusPassageReferenceEditor();
}

// ----------------------------------------------------------------------------

void CKJVBrowser::en_SearchResultsVerseListAboutToChange()
{
	doHighlighting(true);				// Remove existing highlighting
}

void CKJVBrowser::en_SearchResultsVerseListChanged()
{
	doHighlighting();					// Highlight using new tags
}

void CKJVBrowser::en_highlighterTagsAboutToChange(CBibleDatabasePtr pBibleDatabase, const QString &strUserDefinedHighlighterName)
{
	Q_UNUSED(strUserDefinedHighlighterName);
	if ((pBibleDatabase.isNull()) ||
		(pBibleDatabase->highlighterUUID().compare(m_pBibleDatabase->highlighterUUID(), Qt::CaseInsensitive) == 0)) {
		doHighlighting(true);				// Remove existing highlighting
	}
}

void CKJVBrowser::en_highlighterTagsChanged(CBibleDatabasePtr pBibleDatabase, const QString &strUserDefinedHighlighterName)
{
	Q_UNUSED(strUserDefinedHighlighterName);
	if ((pBibleDatabase.isNull()) ||
		(pBibleDatabase->highlighterUUID().compare(m_pBibleDatabase->highlighterUUID(), Qt::CaseInsensitive) == 0)) {
		doHighlighting();					// Highlight using new tags
	}
}

void CKJVBrowser::en_highlightersAboutToChange()
{
	doHighlighting(true);				// Remove existing highlighting
}

void CKJVBrowser::en_highlightersChanged()
{
	doHighlighting();					// Highlight using new tags
}

void CKJVBrowser::doHighlighting(bool bClear)
{
	m_pScriptureBrowser->navigator().doHighlighting(m_SearchResultsHighlighter, bClear, m_ndxCurrent);
	if (m_bShowExcludedSearchResults)
		m_pScriptureBrowser->navigator().doHighlighting(m_ExcludedSearchResultsHighlighter, bClear, m_ndxCurrent);

	assert(!g_pUserNotesDatabase.isNull());
	const THighlighterTagMap *pmapHighlighterTags = g_pUserNotesDatabase->highlighterTagsFor(m_pBibleDatabase);
	if (pmapHighlighterTags) {
		// Note: These are painted in sorted order so they overlay each other with alphabetical precedence:
		//			(the map is already sorted)
		for (THighlighterTagMap::const_iterator itrHighlighters = pmapHighlighterTags->begin(); itrHighlighters != pmapHighlighterTags->end(); ++itrHighlighters) {
			CUserDefinedHighlighter highlighter(itrHighlighters->first, itrHighlighters->second);
			m_pScriptureBrowser->navigator().doHighlighting(highlighter, bClear, m_ndxCurrent);
		}
	}

#ifdef WORKAROUND_QTBUG_BROWSER_BOUNCE		// I HATE this work-around when using it with the highlighters!  Annoying jumps!!
	// Work around Qt5 bug.  Without this, rendering goes Minnie Mouse and
	//		the scroll jumps back a half-line on some lines after doing the
	//		highlighting -- usually noticeable just after a gotoIndex call:
	TPhraseTag tagSelection = m_pScriptureBrowser->navigator().getSelection().primarySelection();
	m_pScriptureBrowser->navigator().selectWords(tagSelection);
#endif
}

void CKJVBrowser::en_WordsOfJesusColorChanged(const QColor &color)
{
	// Only way we can change the Words of Jesus color is by forcing a chapter re-render,
	//		after we change the richifier tags (which is done by the navigator's
	//		signal/slot connection to the persistent settings:
	Q_UNUSED(color);
	emit rerender();
}

void CKJVBrowser::en_SearchResultsColorChanged(const QColor &color)
{
	// Simply redo the highlighting again to change the highlight color:
	Q_UNUSED(color);
	doHighlighting();
}

void CKJVBrowser::en_ShowExcludedSearchResultsChanged(bool bShowExcludedSearchResults)
{
	if (m_bShowExcludedSearchResults == bShowExcludedSearchResults) return;

	// Clear with old setting, change to new setting, and re-highlight:
	doHighlighting(true);
	m_bShowExcludedSearchResults = bShowExcludedSearchResults;
	doHighlighting(false);
}

// ----------------------------------------------------------------------------

void CKJVBrowser::en_userNoteEvent(const CRelIndex &ndx)
{
	if (!selection().isSet()) return;
	CRelIndex ndxNote = ndx;
	TPhraseTagList tagsCurrentDisplay = m_pScriptureBrowser->navigator().currentChapterDisplayPhraseTagList(m_ndxCurrent);
	if ((!ndx.isSet()) ||
		(!tagsCurrentDisplay.isSet()) ||
		(tagsCurrentDisplay.intersects(m_pBibleDatabase.data(), TPhraseTag(ndxNote))) ||
		((ndx.chapter() == 0) && (ndx.book() == m_ndxCurrent.book())) ||			// This compare is needed for book notes rendered at the end of the book when we aren't displaying chapter 1
		((ndx.chapter() == 0) && (ndx.book() == (m_ndxCurrent.book()-1)))) {		// This compare is needed for book notes rendered at the end of the book when we are displaying the first chapter of the next book
		emit rerender();
	}
}

void CKJVBrowser::en_crossRefsEvent(const CRelIndex &ndxFirst, const CRelIndex &ndxSecond)
{
	if (!selection().isSet()) return;
	CRelIndex ndxCrossRefFirst = ndxFirst;
	CRelIndex ndxCrossRefSecond = ndxSecond;
	TPhraseTagList tagsCurrentDisplay = m_pScriptureBrowser->navigator().currentChapterDisplayPhraseTagList(m_ndxCurrent);
	if ((!ndxFirst.isSet()) ||
		(!ndxSecond.isSet()) ||
		(!tagsCurrentDisplay.isSet()) ||
		(tagsCurrentDisplay.intersects(m_pBibleDatabase.data(), TPhraseTag(ndxCrossRefFirst))) ||
		(tagsCurrentDisplay.intersects(m_pBibleDatabase.data(), TPhraseTag(ndxCrossRefSecond)))) {
		emit rerender();
	}

}

void CKJVBrowser::en_allCrossRefsChanged()
{
	emit rerender();					// Re-render text (note: The Note may be deleted as well as changed)
}

// ----------------------------------------------------------------------------

void CKJVBrowser::setFontScriptureBrowser(const QFont& aFont)
{
	m_pScriptureBrowser->setFont(aFont);
}

void CKJVBrowser::setTextBrightness(bool bInvert, int nBrightness)
{
	m_pScriptureBrowser->setTextBrightness(bInvert, nBrightness);
}

void CKJVBrowser::showDetails()
{
	m_pScriptureBrowser->showDetails();
}

void CKJVBrowser::showPassageNavigator()
{
	m_pScriptureBrowser->showPassageNavigator();
}

// ----------------------------------------------------------------------------

void CKJVBrowser::en_Bible_Beginning()
{
	assert(!m_pBibleDatabase.isNull());
	gotoIndex(TPhraseTag(m_pBibleDatabase->calcRelIndex(CRelIndex(), CBibleDatabase::RIME_Start)));
}

void CKJVBrowser::en_Bible_Ending()
{
	assert(!m_pBibleDatabase.isNull());
	gotoIndex(TPhraseTag(m_pBibleDatabase->calcRelIndex(CRelIndex(), CBibleDatabase::RIME_End)));
}

void CKJVBrowser::en_Book_Backward()
{
	assert(!m_pBibleDatabase.isNull());
	CRelIndex ndx = m_pBibleDatabase->calcRelIndex(m_ndxCurrent, CBibleDatabase::RIME_PreviousBook);
	if (ndx.isSet()) gotoIndex(TPhraseTag(ndx));
}

void CKJVBrowser::en_Book_Forward()
{
	assert(!m_pBibleDatabase.isNull());
	CRelIndex ndx = m_pBibleDatabase->calcRelIndex(m_ndxCurrent, CBibleDatabase::RIME_NextBook);
	if (ndx.isSet()) gotoIndex(TPhraseTag(ndx));
}

void CKJVBrowser::en_ChapterBackward()
{
	assert(!m_pBibleDatabase.isNull());
	CRelIndex ndx = m_pBibleDatabase->calcRelIndex(m_ndxCurrent, CBibleDatabase::RIME_PreviousChapter);
	if (ndx.isSet()) gotoIndex(TPhraseTag(ndx));
}

void CKJVBrowser::en_ChapterForward()
{
	assert(!m_pBibleDatabase.isNull());
	CRelIndex ndx = m_pBibleDatabase->calcRelIndex(m_ndxCurrent, CBibleDatabase::RIME_NextChapter);
	if (ndx.isSet()) gotoIndex(TPhraseTag(ndx));
}

// ----------------------------------------------------------------------------

void CKJVBrowser::setBook(const CRelIndex &ndx)
{
	assert(!m_pBibleDatabase.isNull());

	if (ndx.book() == 0) return;
	if (ndx.book() == m_ndxCurrent.book()) return;

	begin_update();

	m_ndxCurrent.setIndex(ndx.book(), 0, 0, 0);

	ui.comboBk->setCurrentIndex(ui.comboBk->findData(m_ndxCurrent.book()));
	ui.comboBibleBk->setCurrentIndex(ui.comboBibleBk->findData(m_ndxCurrent.book()));

	ui.comboTstBk->clear();
	ui.comboBkChp->clear();
	ui.comboTstChp->clear();

	if (m_ndxCurrent.book() > m_pBibleDatabase->bibleEntry().m_nNumBk) {
		// This can happen if the versification of the navigation reference doesn't match the active database
		m_pScriptureBrowser->clear();
		end_update();
		return;
	}

	const CBookEntry &book = *m_pBibleDatabase->bookEntry(m_ndxCurrent.book());

	unsigned int nTst = book.m_nTstNdx;
	QString strTemp = xc_dbDescriptors::translatedBibleTestamentName(m_pBibleDatabase->compatibilityUUID(), nTst);
	if (strTemp.isEmpty()) strTemp = m_pBibleDatabase->testamentEntry(nTst)->m_strTstName;
	ui.lblTestament->setText(strTemp + ":");

	unsigned int nTstStartBook = 0;
	unsigned int nTstStartChp = 0;
	for (unsigned int ndxTst=1; ndxTst<nTst; ++ndxTst) {
		nTstStartBook += m_pBibleDatabase->testamentEntry(ndxTst)->m_nNumBk;
		nTstStartChp += m_pBibleDatabase->testamentEntry(ndxTst)->m_nNumChp;
	}
	for (unsigned int ndxTstBk=1, nTstChp=0; ndxTstBk<=m_pBibleDatabase->testamentEntry(nTst)->m_nNumBk; ++ndxTstBk) {
		nTstChp += m_pBibleDatabase->bookEntry(nTstStartBook + ndxTstBk)->m_nNumChp;
		if (m_pBibleDatabase->bookEntry(nTstStartBook + ndxTstBk)->m_nNumWrd == 0) continue;		// Skip empty books to handle partial databases
		ui.comboTstBk->addItem(QString("%1").arg(ndxTstBk), ndxTstBk);
		for (unsigned int ndxBkChp=1; ndxBkChp <= m_pBibleDatabase->bookEntry(nTstStartBook + ndxTstBk)->m_nNumChp; ++ndxBkChp) {
			const CChapterEntry *pChapter = m_pBibleDatabase->chapterEntry(CRelIndex(nTstStartBook+ndxTstBk, ndxBkChp, 0, 0));
			if (pChapter == NULL) continue;
			if (pChapter->m_nNumWrd == 0) continue;		// Skip chapters that are empty for partial databases
			unsigned int ndxTstChp = nTstChp-m_pBibleDatabase->bookEntry(nTstStartBook + ndxTstBk)->m_nNumChp+ndxBkChp;
			ui.comboTstChp->addItem(QString("%1").arg(ndxTstChp), ndxTstChp);
		}
	}
	ui.comboTstBk->setCurrentIndex(ui.comboTstBk->findData(book.m_nTstBkNdx));

	for (unsigned int ndxBkChp=1; ndxBkChp<=book.m_nNumChp; ++ndxBkChp) {
		const CChapterEntry *pChapter = m_pBibleDatabase->chapterEntry(CRelIndex(m_ndxCurrent.book(), ndxBkChp, 0, 0));
		if (pChapter == NULL) continue;
		if (pChapter->m_nNumWrd == 0) continue;			// Skip chapters that are empty for partial databases
		ui.comboBkChp->addItem(QString("%1").arg(ndxBkChp), ndxBkChp);
	}

	end_update();
}

void CKJVBrowser::setChapter(const CRelIndex &ndx)
{
	assert(!m_pBibleDatabase.isNull());

// Note: The following works great to keep from having to reload the chapter
//	text when navigating to the same chapter.  However, the History log doesn't
//	work correctly if we don't actually call setHtml()...  so...  just leave
//	it out and regenerate it:
//	(Also, repainting colors won't work right if we enable this optimization,
//	as changing things like the Words of Jesus needs to force a repaint.
//	if (ndx.chapter() == m_ndxCurrent.chapter()) return;

	begin_update();

	m_ndxCurrent.setIndex(m_ndxCurrent.book(), ndx.chapter(), 0, 0);

	ui.comboBkChp->setCurrentIndex(ui.comboBkChp->findData(0));
	ui.comboBibleChp->setCurrentIndex(ui.comboBibleChp->findData(0));

	if ((m_ndxCurrent.book() == 0) || ((m_ndxCurrent.chapter() == 0) && (ndx.word() == 0))) {
		m_pScriptureBrowser->clear();
		end_update();
		return;
	}

	if (m_ndxCurrent.book() > m_pBibleDatabase->bibleEntry().m_nNumBk) {
		// This can happen if the versification of the navigation reference doesn't match the active database
		m_pScriptureBrowser->clear();
		end_update();
		return;
	}

	const CBookEntry &book = *m_pBibleDatabase->bookEntry(m_ndxCurrent.book());

	CRelIndex ndxVirtual = m_ndxCurrent;
	if (ndxVirtual.chapter() == 0) {
		if (!book.m_bHaveColophon) {
			m_pScriptureBrowser->clear();
			end_update();
			return;
		}
		ndxVirtual.setChapter(book.m_nNumChp);
	}

	ui.comboBkChp->setCurrentIndex(ui.comboBkChp->findData(ndxVirtual.chapter()));

	unsigned int nTstChp = 0;
	unsigned int nBibleChp = 0;
	for (unsigned int ndxBk=1; ndxBk<ndxVirtual.book(); ++ndxBk) {
		const CBookEntry *pBook = m_pBibleDatabase->bookEntry(ndxBk);
		assert(pBook != NULL);
		if (pBook == NULL) continue;
		if (pBook->m_nTstNdx == book.m_nTstNdx)
			nTstChp += pBook->m_nNumChp;
		nBibleChp += pBook->m_nNumChp;
	}
	nTstChp += ndxVirtual.chapter();
	nBibleChp += ndxVirtual.chapter();

	ui.comboTstChp->setCurrentIndex(ui.comboTstChp->findData(nTstChp));
	ui.comboBibleChp->setCurrentIndex(ui.comboBibleChp->findData(nBibleChp));

	// Set the chapter scroller to the chapter of the Bible:
	if (ui.scrollbarChapter != NULL) {
		ui.scrollbarChapter->setValue(CRefCountCalc(m_pBibleDatabase.data(), CRefCountCalc::RTE_CHAPTER, ndxVirtual).ofBible().first);
//		ui.scrollbarChapter->setToolTip(m_pBibleDatabase->PassageReferenceText(CRelIndex(ndx.book(), ndx.chapter(), 0, 0)));
	}

	end_update();

	m_pScriptureBrowser->navigator().setDocumentToChapter(ndxVirtual, defaultDocumentToChapterFlags | CPhraseNavigator::TRO_ScriptureBrowser);
}

void CKJVBrowser::setVerse(const CRelIndex &ndx)
{
	m_ndxCurrent.setIndex(m_ndxCurrent.book(), m_ndxCurrent.chapter(), ndx.verse(), 0);
}

void CKJVBrowser::setWord(const TPhraseTag &tag)
{
	m_ndxCurrent.setIndex(m_ndxCurrent.book(), m_ndxCurrent.chapter(), m_ndxCurrent.verse(), tag.relIndex().word());
	m_pScriptureBrowser->navigator().selectWords(tag);
}

// ----------------------------------------------------------------------------

void CKJVBrowser::BkComboIndexChanged(int index)
{
	assert(!m_pBibleDatabase.isNull());

	if (m_bDoingUpdate) return;

	CRelIndex ndxTarget;
	if (index != -1) {
		ndxTarget.setBook(ui.comboBk->itemData(index).toUInt());
		ndxTarget.setChapter(1);
	}
	gotoIndex(TPhraseTag(ndxTarget));
}

void CKJVBrowser::BkChpComboIndexChanged(int index)
{
	assert(!m_pBibleDatabase.isNull());

	if (m_bDoingUpdate) return;

	CRelIndex ndxTarget;
	ndxTarget.setBook(m_ndxCurrent.book());
	if (index != -1) {
		ndxTarget.setChapter(ui.comboBkChp->itemData(index).toUInt());
	}
	gotoIndex(TPhraseTag(ndxTarget));
}

void CKJVBrowser::TstBkComboIndexChanged(int index)
{
	assert(!m_pBibleDatabase.isNull());

	if (m_bDoingUpdate) return;

	CRelIndex ndxTarget;
	if ((index != -1) && (m_ndxCurrent.book() > 0)) {
		// Get BookEntry for current book so we know what testament we're currently in:
		const CBookEntry &book = *m_pBibleDatabase->bookEntry(m_ndxCurrent.book());
		ndxTarget = m_pBibleDatabase->calcRelIndex(0, 0, 0, ui.comboTstBk->itemData(index).toUInt(), book.m_nTstNdx);
		ndxTarget.setVerse(0);
		ndxTarget.setWord(0);
	}
	gotoIndex(TPhraseTag(ndxTarget));
}

void CKJVBrowser::TstChpComboIndexChanged(int index)
{
	assert(!m_pBibleDatabase.isNull());

	if (m_bDoingUpdate) return;

	CRelIndex ndxTarget;
	if ((index != -1) && (m_ndxCurrent.book() > 0)) {
		// Get BookEntry for current book so we know what testament we're currently in:
		const CBookEntry &book = *m_pBibleDatabase->bookEntry(m_ndxCurrent.book());
		ndxTarget = m_pBibleDatabase->calcRelIndex(0, 0, ui.comboTstChp->itemData(index).toUInt(), 0, book.m_nTstNdx);
		ndxTarget.setVerse(0);
		ndxTarget.setWord(0);
	}
	gotoIndex(TPhraseTag(ndxTarget));
}

void CKJVBrowser::BibleBkComboIndexChanged(int index)
{
	assert(!m_pBibleDatabase.isNull());

	if (m_bDoingUpdate) return;

	CRelIndex ndxTarget;
	if (index != -1) {
		ndxTarget.setBook(ui.comboBibleBk->itemData(index).toUInt());
		ndxTarget.setChapter(1);
	}
	gotoIndex(TPhraseTag(ndxTarget));
}

void CKJVBrowser::BibleChpComboIndexChanged(int index)
{
	assert(!m_pBibleDatabase.isNull());

	if (m_bDoingUpdate) return;

	CRelIndex ndxTarget;
	if (index != -1) {
		ndxTarget = m_pBibleDatabase->calcRelIndex(0, 0, ui.comboBibleChp->itemData(index).toUInt(), 0, 0);
		ndxTarget.setVerse(0);
		ndxTarget.setWord(0);
	}
	gotoIndex(TPhraseTag(ndxTarget));
}

void CKJVBrowser::PassageReferenceChanged(const TPhraseTag &tag)
{
	if (m_bDoingUpdate) return;
	m_bDoingPassageReference = true;
	if (tag.isSet()) gotoIndex(tag);
	m_bDoingPassageReference = false;
}

void CKJVBrowser::PassageReferenceEnterPressed()
{
	if (m_bDoingUpdate) return;
	m_dlyPassageReference.untrigger();
	gotoIndex(ui.widgetPassageReference->phraseTag());
	setFocusBrowser();
}

void CKJVBrowser::en_activatedPassageReference()
{
	emit activatedBrowser(true);
}

void CKJVBrowser::en_activatedScriptureText()
{
	emit activatedBrowser(false);
}

// ----------------------------------------------------------------------------

void CKJVBrowser::ChapterSliderMoved(int index)
{
	assert(ui.scrollbarChapter != NULL);
	if (ui.scrollbarChapter == NULL) return;

	if (m_bDoingUpdate) return;

	m_bDoingUpdate = true;

	CRelIndex ndxTarget(m_pBibleDatabase->calcRelIndex(0, 0, index, 0, 0));
	ndxTarget.setVerse(0);
	ndxTarget.setWord(0);
	// Note: Remove existing tooltip before displaying the new and/or even setting it on the control.
	//		This is an effort to fix the bug on Mac OSX seen where a call to QToolTip::showText()
	//		crashes trying to do QWindow::windowState() call from a dead object during the QCocoaWindow::setVisible()
	//		function.  In other words, the Qt wrapper for Cocoa seems to have a bug where it can get out-of-sync
	//		with object recycling of QToolTip...  Should probably report this to the Qt guys...
	QString strText = m_pBibleDatabase->PassageReferenceText(ndxTarget);
	QToolTip::showText(QPoint(), QString());
	if (!m_ptChapterScrollerMousePos.isNull()) {
		QToolTip::showText(m_ptChapterScrollerMousePos, strText);
	} else {
//		QToolTip::showText(ui.scrollbarChapter->mapToGlobal(QPoint( 0, 0 )), strText);
		QStyleOptionSlider opt;
		opt.initFrom(ui.scrollbarChapter);
		assert(ui.scrollbarChapter->style() != NULL);
		QRect rcSlider = ui.scrollbarChapter->style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderHandle, ui.scrollbarChapter);
		QToolTip::showText(ui.scrollbarChapter->mapToGlobal(rcSlider.bottomLeft()), strText);
	}
	ui.scrollbarChapter->setToolTip(strText);

	m_bDoingUpdate = false;

	if (ui.scrollbarChapter->isSliderDown()) return;		// Just set ToolTip and exit

	m_dlyGotoIndex.trigger(TPhraseTag(ndxTarget));
}

void CKJVBrowser::ChapterSliderValueChanged()
{
	assert(ui.scrollbarChapter != NULL);
	if (ui.scrollbarChapter == NULL) return;

	ChapterSliderMoved(ui.scrollbarChapter->value());
	m_ptChapterScrollerMousePos = QPoint();
}

// ----------------------------------------------------------------------------

void CKJVBrowser::delayBkComboIndexChanged(int index)
{
	if (m_bDoingUpdate) return;
	m_dlyBkCombo.trigger(index);
}

void CKJVBrowser::delayBkChpComboIndexChanged(int index)
{
	if (m_bDoingUpdate) return;
	m_dlyBkChpCombo.trigger(index);
}

void CKJVBrowser::delayTstBkComboIndexChanged(int index)
{
	if (m_bDoingUpdate) return;
	m_dlyTstBkCombo.trigger(index);
}

void CKJVBrowser::delayTstChpComboIndexChanged(int index)
{
	if (m_bDoingUpdate) return;
	m_dlyTstChpCombo.trigger(index);
}

void CKJVBrowser::delayBibleBkComboIndexChanged(int index)
{
	if (m_bDoingUpdate) return;
	m_dlyBibleBkCombo.trigger(index);
}

void CKJVBrowser::delayBibleChpComboIndexChanged(int index)
{
	if (m_bDoingUpdate) return;
	m_dlyBibleChpCombo.trigger(index);
}

void CKJVBrowser::delayPassageReference(const TPhraseTag &tag)
{
	if (m_bDoingUpdate) return;
	m_dlyPassageReference.trigger(tag);
}

// ----------------------------------------------------------------------------

