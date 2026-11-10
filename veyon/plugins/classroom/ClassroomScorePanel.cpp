/*
 * ClassroomScorePanel.cpp - teacher UI for daily scoring, rules, word clouds and term reports
 *
 * This file is part of the Veyon classroom modification.
 */

#include <QAbstractItemView>
#include <QBoxLayout>
#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDateEdit>
#include <QFile>
#include <QFileDialog>
#include <QFont>
#include <QFontDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QSpinBox>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTextEdit>
#include <QVBoxLayout>

#include "ClassroomData.h"
#include "ClassroomProtocol.h"
#include "ClassroomScorePanel.h"
#include "ClassroomTermReport.h"
#include "ClassroomWordCloud.h"


ClassroomScorePanel::ClassroomScorePanel( const QString& dataDirectory, ClassroomRoster& roster,
										  ClassroomLogStore& store, const QJsonObject& settings,
										  QWidget* parent ) :
	QWidget( parent ),
	m_dataDirectory( dataDirectory ),
	m_roster( roster ),
	m_store( store ),
	m_settings( settings ),
	m_book( dataDirectory )
{
	m_tabs = new QTabWidget( this );
	auto* layout = new QVBoxLayout( this );
	layout->addWidget( m_tabs );

	setupDailyTab();
	setupRulesTab();
	setupCloudTab();
	setupReportTab();
}


void ClassroomScorePanel::setClassName( const QString& className )
{
	if( className.isEmpty() || className == m_className ) { return; }
	m_className = className;
	m_lessonId.clear();
	m_book.load( className );
	refresh();
}


void ClassroomScorePanel::refresh()
{
	if( m_className.isEmpty() ) { return; }
	m_book.load( m_className );
	refreshStudentTable();

	const auto rules = m_book.rules();
	m_baseScore->setValue( rules.baseScore );
	m_publish->setChecked( rules.published );
	m_rulesEditor->setHtml( rules.html );
	QStringList quick;
	for( const auto& action : rules.quickActions )
	{
		quick.append( QStringLiteral("%1:%2").arg( action.first ).arg( action.second ) );
	}
	m_quickActions->setPlainText( quick.join( QLatin1Char('\n') ) );

	const auto previous = m_cloudStudent->currentData().toString();
	m_cloudStudent->clear();
	if( const auto* item = m_roster.findClass( m_className ) )
	{
		for( const auto& student : item->students )
		{
			m_cloudStudent->addItem( QStringLiteral("%1 %2").arg( student.seat, student.name ), student.seat );
		}
	}
	m_cloudStudent->setCurrentIndex( m_cloudStudent->findData( previous ) < 0 ? 0 : m_cloudStudent->findData( previous ) );
}


void ClassroomScorePanel::showCall( const QString& ip, const QString& seat, const QString& student )
{
	m_callLabel->setText( tr( "Call from %1 (%2, seat %3)" ).arg( student, ip, seat ) );
	m_callLabel->setStyleSheet( QStringLiteral("color:#c0392b; font-weight:bold;") );
	m_tabs->setCurrentIndex( 0 );
}


void ClassroomScorePanel::setupDailyTab()
{
	auto* page = new QWidget( m_tabs );
	auto* layout = new QVBoxLayout( page );

	m_callLabel = new QLabel( page );
	layout->addWidget( m_callLabel );

	auto* lessonRow = new QHBoxLayout;
	m_lessonDate = new QDateEdit( page );
	m_lessonDate->setCalendarPopup( true );
	m_lessonDate->setDate( QDate::currentDate() );
	m_lessonTopic = new QLineEdit( page );
	m_lessonTopic->setPlaceholderText( tr( "Lesson content" ) );
	auto* lessonButton = new QPushButton( tr( "Set lesson" ), page );
	lessonRow->addWidget( new QLabel( tr( "Date:" ), page ) );
	lessonRow->addWidget( m_lessonDate );
	lessonRow->addWidget( m_lessonTopic, 1 );
	lessonRow->addWidget( lessonButton );
	layout->addLayout( lessonRow );
	connect( lessonButton, &QPushButton::clicked, this, [this]() { ensureLesson(); refreshStudentTable(); } );

	m_studentTable = new QTableWidget( 0, 4, page );
	m_studentTable->setHorizontalHeaderLabels( { tr( "Seat" ), tr( "Name" ),
												 tr( "This lesson" ), tr( "Current total" ) } );
	m_studentTable->horizontalHeader()->setSectionResizeMode( QHeaderView::Stretch );
	m_studentTable->setSelectionBehavior( QAbstractItemView::SelectRows );
	m_studentTable->setEditTriggers( QAbstractItemView::NoEditTriggers );
	layout->addWidget( m_studentTable, 1 );

	auto* entryRow = new QHBoxLayout;
	m_delta = new QSpinBox( page );
	m_delta->setRange( -100, 100 );
	auto* addButton = new QPushButton( tr( "Add entry" ), page );
	auto* removeButton = new QPushButton( tr( "Remove last entry" ), page );
	for( int delta : { -10, -5, 1, 3, 5 } )
	{
		auto* quick = new QPushButton( QString::number( delta ), page );
		entryRow->addWidget( quick );
		connect( quick, &QPushButton::clicked, this, [this, delta]() { addEntry( delta, QString::number( delta ) ); } );
	}
	entryRow->addWidget( m_delta );
	entryRow->addWidget( addButton );
	entryRow->addWidget( removeButton );
	layout->addLayout( entryRow );
	connect( addButton, &QPushButton::clicked, this, [this]() {
		addEntry( m_delta->value(), m_detail->toPlainText().trimmed() );
	} );
	connect( removeButton, &QPushButton::clicked, this, [this]() {
		ensureLesson();
		const auto entries = m_book.entriesForCell( m_lessonId, currentSeat() );
		if( entries.isEmpty() == false )
		{
			m_book.removeEntry( entries.last().id );
			refreshStudentTable();
		}
	} );

	m_detail = new QTextEdit( page );
	m_detail->setMaximumHeight( 130 );
	m_detail->setPlaceholderText( tr( "Rich text detail (bold, underline, colour, font, size, image)" ) );
	layout->addWidget( m_detail );
	setupRichTextTools( m_detail, layout );

	m_tabs->addTab( page, tr( "Daily score" ) );
}


void ClassroomScorePanel::setupRichTextTools( QTextEdit* editor, QBoxLayout* layout )
{
	auto* row = new QHBoxLayout;
	auto* bold = new QPushButton( tr( "Bold" ), this );
	auto* underline = new QPushButton( tr( "Underline" ), this );
	auto* colour = new QPushButton( tr( "Colour" ), this );
	auto* font = new QPushButton( tr( "Font" ), this );
	auto* size = new QSpinBox( this );
	size->setRange( 8, 48 );
	size->setValue( 12 );
	auto* image = new QPushButton( tr( "Insert image" ), this );
	for( auto* button : { bold, underline, colour, font, image } ) { row->addWidget( button ); }
	row->addWidget( size );
	row->addStretch( 1 );

	connect( bold, &QPushButton::clicked, this, [editor]() {
		editor->setFontWeight( editor->fontWeight() > QFont::Normal ? QFont::Normal : QFont::Bold );
	} );
	connect( underline, &QPushButton::clicked, this, [editor]() {
		editor->setFontUnderline( editor->fontUnderline() == false );
	} );
	connect( colour, &QPushButton::clicked, this, [editor, this]() {
		const auto selected = QColorDialog::getColor( editor->textColor(), this, tr( "Text colour" ) );
		if( selected.isValid() ) { editor->setTextColor( selected ); }
	} );
	connect( font, &QPushButton::clicked, this, [editor, this]() {
		bool ok = false;
		const auto selected = QFontDialog::getFont( &ok, editor->currentFont(), this, tr( "Font" ) );
		if( ok ) { editor->setCurrentFont( selected ); }
	} );
	connect( size, QOverload<int>::of( &QSpinBox::valueChanged ), this, [editor]( int value ) {
		editor->setFontPointSize( static_cast<qreal>( value ) );
	} );
	connect( image, &QPushButton::clicked, this, [this, editor]() { insertImage( editor ); } );

	layout->addLayout( row );
}


void ClassroomScorePanel::insertImage( QTextEdit* editor )
{
	const auto fileName = QFileDialog::getOpenFileName( this, tr( "Insert image" ), {},
													   tr( "Images (*.png *.jpg *.jpeg *.bmp)" ) );
	if( fileName.isEmpty() ) { return; }
	QFile file( fileName );
	if( file.open( QFile::ReadOnly ) == false ) { return; }
	const auto mime = fileName.endsWith( QStringLiteral(".png"), Qt::CaseInsensitive ) ?
						  QStringLiteral("image/png") : QStringLiteral("image/jpeg");
	editor->insertHtml( QStringLiteral("<img src=\"data:%1;base64,%2\" />")
							.arg( mime, QString::fromLatin1( file.readAll().toBase64() ) ) );
}


void ClassroomScorePanel::setupRulesTab()
{
	auto* page = new QWidget( m_tabs );
	auto* layout = new QVBoxLayout( page );
	auto* form = new QHBoxLayout;
	m_baseScore = new QSpinBox( page );
	m_baseScore->setRange( 0, 10000 );
	m_baseScore->setValue( 100 );
	m_publish = new QCheckBox( tr( "Publish rules to students" ), page );
	form->addWidget( new QLabel( tr( "Base score:" ), page ) );
	form->addWidget( m_baseScore );
	form->addWidget( m_publish );
	form->addStretch( 1 );
	layout->addLayout( form );

	m_rulesEditor = new QTextEdit( page );
	m_rulesEditor->setPlaceholderText( tr( "Scoring rules (rich text)" ) );
	layout->addWidget( m_rulesEditor, 1 );
	setupRichTextTools( m_rulesEditor, layout );

	m_quickActions = new QPlainTextEdit( page );
	m_quickActions->setMaximumHeight( 90 );
	m_quickActions->setPlaceholderText( tr( "Quick actions, one per line: label:delta" ) );
	layout->addWidget( m_quickActions );

	auto* saveButton = new QPushButton( tr( "Save and publish rules" ), page );
	layout->addWidget( saveButton );
	connect( saveButton, &QPushButton::clicked, this, [this]() { saveRules(); } );
	m_tabs->addTab( page, tr( "Rules" ) );
}


void ClassroomScorePanel::saveRules()
{
	ClassroomScoreRules rules;
	rules.published = m_publish->isChecked();
	rules.baseScore = m_baseScore->value();
	rules.html = m_rulesEditor->toHtml();
	const auto lines = m_quickActions->toPlainText().split( QLatin1Char('\n'), Qt::SkipEmptyParts );
	for( const auto& line : lines )
	{
		const auto parts = line.split( QLatin1Char(':'), Qt::SkipEmptyParts );
		if( parts.size() >= 2 ) { rules.quickActions.append( { parts.at( 0 ).trimmed(), parts.at( 1 ).trimmed().toInt() } ); }
	}
	m_book.setRules( rules );
	QMessageBox::information( this, windowTitle(), tr( "Scoring rules saved." ) );
}


void ClassroomScorePanel::setupCloudTab()
{
	auto* page = new QWidget( m_tabs );
	auto* layout = new QVBoxLayout( page );
	auto* row = new QHBoxLayout;
	m_cloudStudent = new QComboBox( page );
	auto* generate = new QPushButton( tr( "Generate word cloud" ), page );
	auto* exportSvg = new QPushButton( tr( "Export SVG" ), page );
	row->addWidget( m_cloudStudent, 1 );
	row->addWidget( generate );
	row->addWidget( exportSvg );
	layout->addLayout( row );

	m_cloudView = new QLabel( page );
	m_cloudView->setAlignment( Qt::AlignCenter );
	m_cloudView->setMinimumSize( 640, 380 );
	layout->addWidget( m_cloudView, 1 );

	connect( generate, &QPushButton::clicked, this, [this]() { generateCloud(); } );
	connect( exportSvg, &QPushButton::clicked, this, [this]() {
		generateCloud();
		const auto fileName = QFileDialog::getSaveFileName( this, tr( "Export word cloud" ), {},
														   tr( "SVG files (*.svg)" ) );
		if( fileName.isEmpty() ) { return; }
		QFile file( fileName );
		if( file.open( QFile::WriteOnly | QFile::Truncate ) ) { file.write( m_lastCloudSvg ); }
	} );
	m_tabs->addTab( page, tr( "Word cloud" ) );
}


void ClassroomScorePanel::generateCloud()
{
	const auto seat = m_cloudStudent->currentData().toString();
	QStringList lines;
	const auto events = m_store.load( m_reportFrom ? m_reportFrom->date() : QDate::currentDate().addDays( -180 ),
									  m_reportTo ? m_reportTo->date() : QDate::currentDate() );
	for( const auto& event : events )
	{
		if( event.seat == seat && event.className == m_className )
		{
			lines.append( QStringLiteral("%1 %2").arg( event.detail, event.app ) );
		}
	}

	const auto categories = ClassroomInterestAnalyzer::categoriesFromJson(
		m_settings.value( QStringLiteral("interestCategories") ).toString() );
	QStringList keywords;
	for( const auto& category : categories ) { keywords += category.keywords; }
	QStringList stopWords = m_settings.value( Classroom::SettingKey::wordCloudStopWords() ).
							toString().split( QRegularExpression( QStringLiteral("[,;\\s]+") ), Qt::SkipEmptyParts );

	const auto words = ClassroomWordCloud::tokenize( lines, keywords, 60, stopWords );
	m_lastCloudSvg = ClassroomWordCloud::toSvg( words );
	m_cloudView->setPixmap( ClassroomWordCloud::toPixmap( words, 640, 380 ) );
}


void ClassroomScorePanel::setupReportTab()
{
	auto* page = new QWidget( m_tabs );
	auto* layout = new QVBoxLayout( page );
	auto* row = new QHBoxLayout;
	m_reportFrom = new QDateEdit( page );
	m_reportFrom->setCalendarPopup( true );
	m_reportFrom->setDate( QDate::currentDate().addDays( -180 ) );
	m_reportTo = new QDateEdit( page );
	m_reportTo->setCalendarPopup( true );
	m_reportTo->setDate( QDate::currentDate() );
	m_term = new QLineEdit( page );
	m_term->setPlaceholderText( tr( "Term name" ) );
	row->addWidget( new QLabel( tr( "From:" ), page ) );
	row->addWidget( m_reportFrom );
	row->addWidget( new QLabel( tr( "To:" ), page ) );
	row->addWidget( m_reportTo );
	row->addWidget( m_term, 1 );
	layout->addLayout( row );

	m_includeRules = new QCheckBox( tr( "Include scoring rules sheet" ), page );
	m_attendanceColumn = new QCheckBox( tr( "Include attendance column" ), page );
	m_attendanceColumn->setChecked( true );
	layout->addWidget( m_includeRules );
	layout->addWidget( m_attendanceColumn );

	auto* buttons = new QHBoxLayout;
	auto* xlsButton = new QPushButton( tr( "Export Excel (.xls)" ), page );
	auto* csvButton = new QPushButton( tr( "Export CSV" ), page );
	buttons->addWidget( xlsButton );
	buttons->addWidget( csvButton );
	buttons->addStretch( 1 );
	layout->addLayout( buttons );
	layout->addStretch( 1 );
	connect( xlsButton, &QPushButton::clicked, this, [this]() { exportReport( true ); } );
	connect( csvButton, &QPushButton::clicked, this, [this]() { exportReport( false ); } );
	m_tabs->addTab( page, tr( "Term report" ) );
}


void ClassroomScorePanel::exportReport( bool xls )
{
	ClassroomTermReport::Options options;
	options.className = m_className;
	options.term = m_term->text().trimmed();
	options.from = m_reportFrom->date();
	options.to = m_reportTo->date();
	options.includeRules = m_includeRules->isChecked();
	options.attendanceColumn = m_attendanceColumn->isChecked();

	const auto filter = xls ? tr( "Excel 2003 XML (*.xls)" ) : tr( "CSV files (*.csv)" );
	const auto fileName = QFileDialog::getSaveFileName( this, tr( "Export class report" ), {}, filter );
	if( fileName.isEmpty() ) { return; }

	QString error;
	bool ok = false;
	if( xls )
	{
		ok = ClassroomTermReport::exportXls( fileName, m_roster, m_book, options, &error );
	}
	else
	{
		QFile file( fileName );
		ok = file.open( QFile::WriteOnly | QFile::Truncate );
		if( ok ) { ok = file.write( ClassroomTermReport::toCsv( m_roster, m_book, options ) ) > 0; }
		if( ok == false ) { error = file.errorString(); }
	}
	QMessageBox::information( this, windowTitle(), ok ? tr( "Report exported." ) : error );
}


void ClassroomScorePanel::ensureLesson()
{
	if( m_className.isEmpty() ) { return; }
	m_book.load( m_className );
	m_lessonId = m_book.ensureLesson( m_lessonDate->date(), m_lessonTopic->text().trimmed() );
}


void ClassroomScorePanel::refreshStudentTable()
{
	const auto* item = m_roster.findClass( m_className );
	if( item == nullptr ) { m_studentTable->setRowCount( 0 ); return; }

	ensureLesson();
	m_studentTable->setRowCount( item->students.size() );
	int row = 0;
	for( const auto& student : item->students )
	{
		QStringList labels;
		for( const auto& entry : m_book.entriesForCell( m_lessonId, student.seat ) ) { labels.append( entry.label ); }
		m_studentTable->setItem( row, 0, new QTableWidgetItem( student.seat ) );
		m_studentTable->setItem( row, 1, new QTableWidgetItem( student.name ) );
		m_studentTable->setItem( row, 2, new QTableWidgetItem( labels.join( QStringLiteral("; ") ) ) );
		m_studentTable->setItem( row, 3, new QTableWidgetItem( QString::number( m_book.total( student.seat, m_lessonId ) ) ) );
		++row;
	}
}


QString ClassroomScorePanel::currentSeat() const
{
	const auto row = m_studentTable->currentRow();
	const auto* item = row >= 0 ? m_studentTable->item( row, 0 ) : nullptr;
	return item ? item->text() : QString{};
}


QString ClassroomScorePanel::studentName( const QString& seat ) const
{
	if( const auto* item = m_roster.findClass( m_className ) )
	{
		for( const auto& student : item->students )
		{
			if( student.seat == seat ) { return student.name; }
		}
	}
	return {};
}


void ClassroomScorePanel::addEntry( int delta, const QString& label )
{
	const auto seat = currentSeat();
	if( seat.isEmpty() )
	{
		QMessageBox::information( this, windowTitle(), tr( "Please select a student row first." ) );
		return;
	}
	ensureLesson();

	ClassroomScoreEntry entry;
	entry.lessonId = m_lessonId;
	entry.className = m_className;
	entry.seat = seat;
	entry.student = studentName( seat );
	entry.delta = delta;
	entry.label = label.isEmpty() ? QString::number( delta ) : label;
	entry.richText = m_detail->toHtml();
	m_book.addEntry( entry );
	m_detail->clear();
	refreshStudentTable();
}
