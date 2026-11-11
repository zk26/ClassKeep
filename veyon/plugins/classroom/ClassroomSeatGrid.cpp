/*
 * ClassroomSeatGrid.cpp - live seat grid with thumbnails and one-click scoring
 *
 * This file is part of the Veyon classroom modification.
 */

#include "ClassroomSeatGrid.h"

#include <QtGlobal>

#include <QCursor>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFrame>
#include <QGridLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QPixmap>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

#include "ClassroomData.h"
#include "ClassroomSeatOrder.h"

namespace
{
constexpr int Columns = 4;
constexpr int ThumbWidth = 168;
constexpr int ThumbHeight = 105;
}


ClassroomSeatGrid::ClassroomSeatGrid( ClassroomRoster& roster, QWidget* parent ) :
	QWidget( parent ),
	m_roster( roster )
{
	auto* root = new QVBoxLayout( this );
	m_summary = new QLabel( this );
	m_summary->setWordWrap( true );
	root->addWidget( m_summary );

	m_grid = new QGridLayout;
	m_grid->setSpacing( 10 );
	root->addLayout( m_grid );
	root->addStretch( 1 );

	m_timer = new QTimer( this );
	m_timer->setInterval( 2000 );
	connect( m_timer, &QTimer::timeout, this, &ClassroomSeatGrid::refresh );
	m_timer->start();
}


void ClassroomSeatGrid::setClassName( const QString& className )
{
	if( m_className == className ) { return; }
	m_className = className;
	m_order = Classroom::loadSeatOrder( className );
	rebuild();
}


void ClassroomSeatGrid::setOnlineIps( const QStringList& ips )
{
	if( m_onlineIps == ips ) { return; }
	m_onlineIps = ips;
	refresh();
}


void ClassroomSeatGrid::rebuild()
{
	while( m_grid->count() > 0 )
	{
		auto* item = m_grid->takeAt( 0 );
		if( item->widget() ) { item->widget()->deleteLater(); }
		delete item;
	}
	m_cards.clear();

	const auto* classItem = m_className.isEmpty() ? nullptr : m_roster.findClass( m_className );
	if( classItem == nullptr )
	{
		m_summary->setText( tr( "No class selected." ) );
		return;
	}

	// Replay the persisted drag order, then append newly added students.
	QVector<ClassroomStudent> ordered;
	for( const auto& seat : m_order )
	{
		for( const auto& student : classItem->students )
		{
			if( student.seat == seat ) { ordered.append( student ); break; }
		}
	}
	for( const auto& student : classItem->students )
	{
		bool placed = false;
		for( const auto& item : ordered )
		{
			if( item.seat == student.seat ) { placed = true; break; }
		}
		if( placed == false ) { ordered.append( student ); }
	}
	m_order.clear();
	for( const auto& student : ordered ) { m_order.append( student.seat ); }
	Classroom::saveSeatOrder( m_className, m_order );

	int index = 0;
	for( const auto& student : ordered )
	{
		auto* card = new QFrame( this );
		card->setFrameShape( QFrame::StyledPanel );
		card->setStyleSheet( QStringLiteral("background:#ffffff; border:1px solid #dce3ea; border-radius:8px;") );
		card->setFixedWidth( ThumbWidth + 16 );
		card->setToolTip( tr( "Drag this card onto another one to swap positions" ) );
		card->setProperty( "classkeepSeat", student.seat );
		card->setAcceptDrops( true );
		card->installEventFilter( this );

		auto* layout = new QVBoxLayout( card );
		layout->setContentsMargins( 8, 8, 8, 8 );

		auto* thumbnail = new QLabel( card );
		thumbnail->setFixedSize( ThumbWidth, ThumbHeight );
		thumbnail->setAlignment( Qt::AlignCenter );
		thumbnail->setStyleSheet( QStringLiteral("background:#eef3f6; border-radius:4px;") );
		thumbnail->setProperty( "classkeepSeat", student.seat );
		thumbnail->setAcceptDrops( true );
		thumbnail->installEventFilter( this );
		layout->addWidget( thumbnail );

		auto* score = new QPushButton( card );
		score->setToolTip( tr( "Click to add or deduct points (-5..+5)" ) );
		layout->addWidget( score );

		auto* caption = new QLabel( card );
		caption->setWordWrap( true );
		layout->addWidget( caption );

		const auto seat = student.seat;
		connect( score, &QPushButton::clicked, this, [this, seat]() { chooseDelta( seat ); } );

		m_grid->addWidget( card, index / Columns, index % Columns );
		m_cards.insert( seat, SeatCard{ card, thumbnail, score, caption, student.ip, student.name, seat } );
		++index;
	}

	m_summary->setText( tr( "%1 students in %2. Drag cards to reorder (persisted); click a score for -5..+5." )
						.arg( classItem->students.size() ).arg( m_className ) );
	refresh();
}


void ClassroomSeatGrid::refresh()
{
	int online = 0;
	for( auto it = m_cards.begin(); it != m_cards.end(); ++it )
	{
		auto& card = it.value();
		const auto onlineNow = m_onlineIps.contains( card.ip );

		if( m_thumbnailProvider && onlineNow )
		{
			const auto image = m_thumbnailProvider( card.ip );
			if( image.isNull() == false )
			{
				card.thumbnail->setPixmap( QPixmap::fromImage( image ).scaled(
					ThumbWidth, ThumbHeight, Qt::KeepAspectRatio, Qt::SmoothTransformation ) );
			}
		}
		else
		{
			card.thumbnail->setPixmap( QPixmap() );
			card.thumbnail->setText( tr( "offline" ) );
		}

		int total = 100;
		if( m_scoreProvider ) { total = m_scoreProvider( card.seat ); }
		card.score->setText( tr( "%1: %2 points" ).arg( card.seat ).arg( total ) );
		card.score->setEnabled( true );
		const auto state = onlineNow ? tr( "online" ) : tr( "offline" );
		card.caption->setText( QStringLiteral("%1 · %2").arg( card.expectedName, state ) );
		if( onlineNow ) { ++online; }
	}
	if( m_cards.isEmpty() == false )
	{
		m_summary->setText( tr( "%1: %2 of %3 students online. Click a score to add/deduct points." )
							.arg( m_className, QString::number( online ), QString::number( m_cards.size() ) ) );
	}
}


void ClassroomSeatGrid::chooseDelta( const QString& seat )
{
	if( m_scoreSink == nullptr ) { return; }

	QMenu menu( this );
	for( int delta = 5; delta >= -5; --delta )
	{
		const auto text = delta > 0 ? QStringLiteral("+%1").arg( delta ) : QString::number( delta );
		auto* action = menu.addAction( text );
		action->setData( delta );
	}
	auto* chosen = menu.exec( QCursor::pos() );
	if( chosen == nullptr ) { return; }

	const auto delta = chosen->data().toInt();
	if( delta == 0 ) { return; }

	bool accepted = false;
	const auto reason = QInputDialog::getText( this, tr( "Score reason" ),
											   tr( "Reason for %1 points at seat %2:" )
												   .arg( delta ).arg( seat ),
											   QLineEdit::Normal, {}, &accepted );
	if( accepted == false ) { return; }

	m_scoreSink( seat, delta, reason.trimmed() );
	refresh();
}


bool ClassroomSeatGrid::eventFilter( QObject* watched, QEvent* event )
{
	const auto seat = watched->property( "classkeepSeat" ).toString();
	if( seat.isEmpty() ) { return QWidget::eventFilter( watched, event ); }

	if( event->type() == QEvent::MouseButtonPress )
	{
		auto* mouse = static_cast<QMouseEvent*>( event );
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
		m_dragStart = mouse->position().toPoint();
#else
		m_dragStart = mouse->pos();
#endif
		m_dragSeat = seat;
		return false;
	}

	if( event->type() == QEvent::MouseMove && m_dragSeat.isEmpty() == false )
	{
		auto* mouse = static_cast<QMouseEvent*>( event );
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
		const QPoint current = mouse->position().toPoint();
#else
		const QPoint current = mouse->pos();
#endif
		if( ( mouse->buttons() & Qt::LeftButton ) &&
			( current - m_dragStart ).manhattanLength() > 12 )
		{
			auto* drag = new QDrag( watched );
			auto* mime = new QMimeData;
			mime->setText( m_dragSeat );
			drag->setMimeData( mime );
			drag->exec( Qt::MoveAction );
			m_dragSeat.clear();
			return true;
		}
		return false;
	}

	if( event->type() == QEvent::DragEnter )
	{
		auto* enter = static_cast<QDragEnterEvent*>( event );
		if( enter->mimeData()->hasText() )
		{
			enter->acceptProposedAction();
			return true;
		}
	}

	if( event->type() == QEvent::Drop )
	{
		auto* drop = static_cast<QDropEvent*>( event );
		const auto from = drop->mimeData()->text();
		if( from.isEmpty() == false && from != seat )
		{
			swapSeats( from, seat );
			drop->acceptProposedAction();
			return true;
		}
	}

	return QWidget::eventFilter( watched, event );
}


void ClassroomSeatGrid::swapSeats( const QString& from, const QString& to )
{
	const auto left = m_order.indexOf( from );
	const auto right = m_order.indexOf( to );
	if( left < 0 || right < 0 || left == right ) { return; }

	const auto previous = m_order.at( left );
	m_order[ left ] = m_order.at( right );
	m_order[ right ] = previous;

	// "下一次开机不变": persist immediately, not on exit.
	Classroom::saveSeatOrder( m_className, m_order );
	rebuild();
}
