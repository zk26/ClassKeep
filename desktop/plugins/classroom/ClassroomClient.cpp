/*
 * ClassroomClient.cpp - non-topmost student card, popups and activity sampler
 *
 * This file is part of the Ikeback classroom modification.
 */

#include <QtGlobal>

#ifdef Q_OS_WIN
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0602
#endif
#include <windows.h>
#endif

#include <algorithm>

#include <QApplication>
#include <QBoxLayout>
#include <QJsonArray>
#include <QCloseEvent>
#include <QContextMenuEvent>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileInfo>
#include <QFormLayout>
#include <QGuiApplication>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QProcess>
#include <QPushButton>
#include <QScreen>
#include <QScrollBar>
#include <QStandardPaths>
#include <QTextBrowser>
#include <QTimer>
#include <QVariantMap>
#include <QVBoxLayout>

#include "ClassroomAiClient.h"
#include "ClassroomClient.h"
#include "ClassroomProtocol.h"
#include "CryptoCore.h"
#include "PlatformCoreFunctions.h"
#include "PlatformPluginInterface.h"
#include "PlatformUserFunctions.h"
#include "IkebackCore.h"


static void keepOnTop( QWidget* widget )
{
	widget->setWindowFlags( Qt::Tool | Qt::CustomizeWindowHint | Qt::FramelessWindowHint |
							Qt::WindowStaysOnTopHint );
	IkebackCore::platform().coreFunctions().raiseWindow( widget, true );
}


//! The student floating card is deliberately NOT always-on-top: it must not
//! cover the student's work. Only the popups opened from it stay on top.
static void keepFloating( QWidget* widget )
{
	widget->setWindowFlags( Qt::Tool | Qt::CustomizeWindowHint | Qt::FramelessWindowHint );
	IkebackCore::platform().coreFunctions().raiseWindow( widget, false );
}


static QPoint mouseGlobalPoint( QMouseEvent* event )
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	return event->globalPosition().toPoint();
#else
	return event->globalPos();
#endif
}


ClassroomClientWidget::ClassroomClientWidget( QWidget* parent ) :
	QWidget( parent ),
	m_aiClient( new ClassroomAiClient( this ) )
{
	setObjectName( QStringLiteral("classroomClient") );
	setWindowTitle( Classroom::productName() );
	setAttribute( Qt::WA_DeleteOnClose, false );
	keepFloating( this );

	const auto dark = IkebackCore::useDarkMode();
	setStyleSheet( QStringLiteral(
		"QWidget#classroomClient { background: %1; border: 1px solid %2; border-radius: 6px; }"
		"QLabel { color: %3; }"
		"QLineEdit { padding: 3px; }"
		"QPushButton { padding: 4px 8px; }" ).
		arg( dark ? QStringLiteral("#2b2b2b") : QStringLiteral("#f5f7f9"),
			 dark ? QStringLiteral("#555555") : QStringLiteral("#9fc4d0"),
			 dark ? QStringLiteral("#f0f0f0") : QStringLiteral("#202020") ) );

	m_identityLabel = new QLabel( this );
	m_identityLabel->setStyleSheet( QStringLiteral("font-size: 15px; font-weight: bold;") );
	m_statusLabel = new QLabel( this );
	m_statusLabel->setWordWrap( true );
	m_statusLabel->setStyleSheet( QStringLiteral("font-size: 10px;") );
	m_lockLabel = new QLabel( this );
	m_lockLabel->setAlignment( Qt::AlignRight | Qt::AlignVCenter );

	auto* header = new QHBoxLayout;
	header->addWidget( m_identityLabel, 1 );
	header->addWidget( m_lockLabel );

	m_seatEdit = new QLineEdit( this );
	m_seatEdit->setPlaceholderText( tr( "Seat number" ) );
	m_seatEdit->setMaxLength( 16 );
	m_nameEdit = new QLineEdit( this );
	m_nameEdit->setPlaceholderText( tr( "Name" ) );
	m_nameEdit->setMaxLength( 32 );

	// No key field on the card: the bridge key stays on the teacher machine
	// and the teacher's phone. Students sign in with seat + name only.
	auto* inputLayout = new QHBoxLayout;
	inputLayout->addWidget( m_seatEdit );
	inputLayout->addWidget( m_nameEdit );

	m_loginButton = new QPushButton( tr( "Sign in" ), this );
	m_aiButton = new QPushButton( tr( "AI assistant" ), this );
	m_aiButton->setEnabled( false );
	m_scoreButton = new QPushButton( tr( "My score" ), this );
	m_callButton = new QPushButton( tr( "Call teacher" ), this );

	// The card exposes exactly TWO action buttons: score query and AI. The
	// sign-in and call-teacher controls stay available but are only shown while
	// nobody is signed in (call-teacher also lives in the context menu).
	auto* buttonLayout = new QHBoxLayout;
	buttonLayout->addWidget( m_loginButton );
	buttonLayout->addWidget( m_callButton );
	buttonLayout->addWidget( m_scoreButton );
	buttonLayout->addWidget( m_aiButton );

	auto* hint = new QLabel( tr( "Right-click: call the teacher / protected exit" ), this );
	hint->setStyleSheet( QStringLiteral("font-size: 9px; opacity: 0.7;") );

	auto* mainLayout = new QVBoxLayout( this );
	mainLayout->setContentsMargins( 10, 8, 10, 8 );
	mainLayout->setSpacing( 6 );
	mainLayout->addLayout( header );
	mainLayout->addWidget( m_statusLabel );
	mainLayout->addLayout( inputLayout );
	mainLayout->addLayout( buttonLayout );
	mainLayout->addWidget( hint );

	setFixedWidth( 360 );
	adjustSize();
	placeOnScreen();
	updateVisibility();
	updateStatus();

	// "server 不退出就关闭不了": close requests are ignored and a watchdog
	// restores the card if anything else hides it (taskkill of the worker is
	// handled by the service watchdog, so this card lives as long as it does).
	m_guardTimer = new QTimer( this );
	m_guardTimer->setInterval( 1500 );
	connect( m_guardTimer, &QTimer::timeout, this, [this]() {
		if( m_hideAllowed == false && m_exitProtection && isVisible() == false ) { show(); }
	} );
	m_guardTimer->start();

	connect( m_loginButton, &QPushButton::clicked, this, &ClassroomClientWidget::handleLogin );
	connect( m_aiButton, &QPushButton::clicked, this, &ClassroomClientWidget::showAiChat );
	connect( m_nameEdit, &QLineEdit::returnPressed, this, &ClassroomClientWidget::handleLogin );
	connect( m_aiClient, &ClassroomAiClient::promptRequested,
			 this, &ClassroomClientWidget::aiPromptRequested );
	connect( m_scoreButton, &QPushButton::clicked, this, [this]() { Q_EMIT scoreRequested(); } );
	connect( m_callButton, &QPushButton::clicked, this, [this]() { Q_EMIT callTeacherRequested(); } );
}


void ClassroomClientWidget::updateVisibility()
{
	const auto signedIn = m_loggedIn;
	m_seatEdit->setVisible( signedIn == false );
	m_nameEdit->setVisible( signedIn == false );
	m_loginButton->setVisible( signedIn == false && m_allowSelfLogin );
	m_callButton->setVisible( signedIn == false );
	m_scoreButton->setVisible( true );
	m_aiButton->setVisible( true );
	adjustSize();
}


void ClassroomClientWidget::placeOnScreen()
{
	if( auto* screen = QGuiApplication::primaryScreen() )
	{
		const auto area = screen->availableGeometry();
		move( area.right() - width() - 16, area.top() + 16 );
	}
}


void ClassroomClientWidget::setSettings( const QJsonObject& settings )
{
	m_allowSelfLogin = settings.value( Classroom::SettingKey::allowSelfLogin() ).toBool( true );
	m_aiEnabled = settings.value( Classroom::SettingKey::aiEnabled() ).toBool( true );
	m_exitProtection = settings.value( Classroom::SettingKey::exitProtection() ).toBool( true );
	m_exitRequiresAdmin = settings.value( Classroom::SettingKey::exitRequiresAdmin() ).toBool( true );
	m_exitAdminUsername = settings.value( Classroom::SettingKey::exitAdminUsername() ).toString();

	// The API key, endpoint, model and system prompt stay on the teacher
	// machine. The student worker only submits a prompt through the bridge of
	// Ikeback feature messages (see ClassroomPlugin::handleFeatureMessage).
	ClassroomAiClient::Settings aiSettings;
	aiSettings.enabled = m_aiEnabled;
	m_aiClient->setProxyMode( true );
	m_aiClient->setSettings( aiSettings );

	m_aiButton->setEnabled( m_aiEnabled );
	m_loginButton->setEnabled( m_allowSelfLogin );
	updateVisibility();
	updateStatus();
}


void ClassroomClientWidget::setRoster( const QJsonObject& roster )
{
	m_roster = roster;
	const auto classes = roster.value( QStringLiteral("classes") ).toArray();
	if( m_className.isEmpty() && classes.isEmpty() == false )
	{
		m_className = classes.first().toObject().value( QStringLiteral("name") ).toString();
	}
	updateStatus();
}


void ClassroomClientWidget::setKeyConfigured( bool configured )
{
	m_keyConfigured = configured;
	updateStatus();
}


void ClassroomClientWidget::deliverAiReply( bool success, const QString& reply )
{
	if( m_aiClient ) { m_aiClient->deliverReply( success, reply ); }
}


void ClassroomClientWidget::setIdentity( const QString& className, const QString& seat, const QString& student )
{
	m_className = className;
	m_seat = seat;
	m_student = student;
	m_loggedIn = true;

	m_seatEdit->setText( seat );
	m_nameEdit->setText( student );
	m_loginButton->setText( tr( "Sign in" ) );
	updateVisibility();
	updateStatus();
	Q_EMIT identityChanged( m_seat, m_student, true );
}


void ClassroomClientWidget::clearIdentity()
{
	m_seat.clear();
	m_student.clear();
	m_loggedIn = false;
	m_seatEdit->clear();
	m_nameEdit->clear();
	m_loginButton->setText( tr( "Sign in" ) );
	updateVisibility();
	updateStatus();
	Q_EMIT identityChanged( {}, {}, false );
}


void ClassroomClientWidget::setLocked( bool locked )
{
	m_locked = locked;
	updateStatus();
}


void ClassroomClientWidget::updateStatus()
{
	m_statusLabel->setText( tr( "Class: %1  |  Lesson: %2" ).
							arg( m_className.isEmpty() ? tr( "not selected" ) : m_className,
								 m_keyConfigured ? tr( "started" ) : tr( "not started" ) ) );
	m_lockLabel->setText( m_locked ? tr( "locked" ) : tr( "unlocked" ) );

	// Prominent line: shield (client protection) + signed-in name + seat number.
	const auto shield = QString::fromUtf8( "\xF0\x9F\x9B\xA1" );
	m_identityLabel->setText( m_loggedIn ?
		QStringLiteral("%1 %2  |  %3 %4").arg( shield ).arg( m_student ).arg( tr( "Seat" ) ).arg( m_seat ) :
		QStringLiteral("%1 %2").arg( shield ).arg( tr( "Not signed in" ) ) );
}


bool ClassroomClientWidget::rosterAccepts( const QString& seat, const QString& name ) const
{
	const auto classes = m_roster.value( QStringLiteral("classes") ).toArray();
	if( classes.isEmpty() ) { return true; }

	QJsonArray acceptedClasses;
	if( m_className.isEmpty() ) { acceptedClasses = classes; }
	else
	{
		for( const auto& value : classes )
		{
			if( value.toObject().value( QStringLiteral("name") ).toString() == m_className )
			{
				acceptedClasses.append( value );
			}
		}
	}
	if( acceptedClasses.isEmpty() ) { return true; }

	for( const auto& value : acceptedClasses )
	{
		for( const auto& studentValue : value.toObject().value( QStringLiteral("students") ).toArray() )
		{
			const auto student = studentValue.toObject();
			if( student.value( QStringLiteral("seat") ).toString() == seat &&
				student.value( QStringLiteral("name") ).toString() == name )
			{
				return true;
			}
		}
	}
	return false;
}


void ClassroomClientWidget::handleLogin()
{
	const auto seat = m_seatEdit->text().trimmed();
	const auto name = m_nameEdit->text().trimmed();

	if( seat.isEmpty() || name.isEmpty() )
	{
		QMessageBox::warning( this, windowTitle(), tr( "Please enter both the seat number and your name." ) );
		return;
	}
	// The bridge key never reaches this process. Self sign-in is gated by the
	// teacher having started a lesson plus the roster check below.
	if( m_keyConfigured == false )
	{
		Q_EMIT eventOccurred( Classroom::EventType::keyFailed(), tr( "no lesson started by the teacher" ), {} );
		QMessageBox::warning( this, windowTitle(), tr( "The teacher has not started a lesson yet." ) );
		return;
	}
	if( m_allowSelfLogin == false )
	{
		QMessageBox::information( this, windowTitle(), tr( "Self sign-in is disabled. Please wait for the teacher to sign you in." ) );
		return;
	}
	if( rosterAccepts( seat, name ) == false )
	{
		Q_EMIT eventOccurred( Classroom::EventType::keyFailed(), tr( "seat/name mismatch for %1" ).arg( seat ), {} );
		QMessageBox::warning( this, windowTitle(), tr( "The seat number and name do not match the class roster." ) );
		return;
	}
	if( m_loggedIn )
	{
		m_loggedIn = false;
		updateVisibility();
	}

	setIdentity( m_className, seat, name );
	setLocked( true );
	Q_EMIT eventOccurred( Classroom::EventType::login(), tr( "student signed in at seat %1" ).arg( seat ), {} );
}


void ClassroomClientWidget::openAiAssistant()
{
	showAiChat();
}


void ClassroomClientWidget::requestProtectedQuit()
{
	requestQuit();
}


void ClassroomClientWidget::requestScore()
{
	Q_EMIT scoreRequested();
}


void ClassroomClientWidget::showAiChat()
{
	if( m_aiEnabled == false )
	{
		QMessageBox::information( this, windowTitle(), tr( "The teacher has disabled the AI assistant." ) );
		return;
	}

	if( m_aiChat == nullptr )
	{
		m_aiChat = new ClassroomAiChatWidget( m_aiClient, this );
		connect( m_aiChat, &ClassroomAiChatWidget::eventOccurred,
				 this, &ClassroomClientWidget::eventOccurred );
	}
	m_aiChat->setIdentityLabel( m_loggedIn ? QStringLiteral("%1 (%2)").arg( m_student, m_seat ) : tr( "not signed in" ) );
	m_aiChat->show();
	m_aiChat->raise();
	IkebackCore::platform().coreFunctions().raiseWindow( m_aiChat, true );
}


void ClassroomClientWidget::requestQuit()
{
	if( m_exitProtection == false )
	{
		Q_EMIT eventOccurred( Classroom::EventType::quitAllowed(), tr( "exit protection disabled" ), {} );
		Q_EMIT quitAuthorized();
		return;
	}

	QDialog dialog( this );
	dialog.setWindowTitle( tr( "Exit classroom client" ) );
	auto* form = new QFormLayout( &dialog );
	auto* username = new QLineEdit( &dialog );
	auto* password = new QLineEdit( &dialog );
	username->setText( IkebackCore::platform().userFunctions().
					   queryCurrentUserProperty( PlatformUserFunctions::UserProperty::LoginName ) );
	password->setEchoMode( QLineEdit::Password );
	form->addRow( tr( "Administrator account:" ), username );
	form->addRow( tr( "Administrator password:" ), password );
	auto* buttons = new QDialogButtonBox( QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog );
	form->addRow( buttons );
	connect( buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept );
	connect( buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject );

	if( dialog.exec() != QDialog::Accepted ) { return; }

	// From 5.2.0 Windows administrator credentials are the ONLY exit
	// credential. The bridge key is never present in this process, so the old
	// "connection key" fallback is gone; exitRequiresAdmin is kept in the
	// configuration for backwards compatibility but no longer weakens this.
	Q_UNUSED( m_exitRequiresAdmin )
	bool authorized = false;
	const auto expectedUser = m_exitAdminUsername.trimmed();
	const auto userMatches = expectedUser.isEmpty() ||
							 username->text().trimmed().compare( expectedUser, Qt::CaseInsensitive ) == 0;
	if( userMatches && password->text().isEmpty() == false )
	{
		const CryptoCore::PlaintextPassword plainPassword = password->text().toUtf8();
		authorized = IkebackCore::platform().userFunctions().authenticate( username->text().trimmed(), plainPassword );
	}

	if( authorized )
	{
		Q_EMIT eventOccurred( Classroom::EventType::quitAllowed(), tr( "authorized client exit" ), {} );
		Q_EMIT quitAuthorized();
	}
	else
	{
		Q_EMIT eventOccurred( Classroom::EventType::quitAttempt(), tr( "rejected client exit" ), {} );
		QMessageBox::warning( this, windowTitle(),
							  tr( "Administrator credentials are not valid. This attempt has been logged." ) );
	}
}


void ClassroomClientWidget::allowHide()
{
	m_hideAllowed = true;
	if( m_guardTimer ) { m_guardTimer->stop(); }
	hide();
}


void ClassroomClientWidget::reactivate()
{
	m_hideAllowed = false;
	if( m_guardTimer && m_guardTimer->isActive() == false ) { m_guardTimer->start(); }
	show();
	raise();
}


void ClassroomClientWidget::closeEvent( QCloseEvent* event )
{
	// The card cannot be closed while the ikeback service is running. Only
	// the teacher's StopClient command (or an authorized protected exit) may
	// stop it; a plain close/hide request is simply refused.
	// With client protection OFF (teacher switched it off) the student may hide
	// the card; the worker itself is still kept alive by the service watchdog.
	if( m_hideAllowed || m_exitProtection == false )
	{
		event->accept();
		hide();
		return;
	}
	event->ignore();
	show();
	raise();
}


void ClassroomClientWidget::contextMenuEvent( QContextMenuEvent* event )
{
	QMenu menu( this );
	menu.addAction( tr( "Show AI assistant" ), this, &ClassroomClientWidget::showAiChat );
	menu.addAction( tr( "Show my score" ), this, [this]() { Q_EMIT scoreRequested(); } );
	menu.addAction( tr( "Call the teacher" ), this, [this]() { Q_EMIT callTeacherRequested(); } );
	menu.addSeparator();
	menu.addAction( tr( "Exit client (protected)..." ), this, &ClassroomClientWidget::requestQuit );
	menu.exec( event->globalPos() );
}


void ClassroomClientWidget::mousePressEvent( QMouseEvent* event )
{
	if( event->button() == Qt::LeftButton )
	{
		m_dragPosition = mouseGlobalPoint( event ) - frameGeometry().topLeft();
	}
	QWidget::mousePressEvent( event );
}


void ClassroomClientWidget::mouseMoveEvent( QMouseEvent* event )
{
	if( event->buttons() & Qt::LeftButton )
	{
		move( mouseGlobalPoint( event ) - m_dragPosition );
	}
	QWidget::mouseMoveEvent( event );
}


void ClassroomClientWidget::setScoreHtml( const QString& html )
{
	if( m_scoreView == nullptr )
	{
		m_scoreView = new ClassroomScoreView( this );
	}
	m_scoreView->setHtml( html );
	m_scoreView->show();
	m_scoreView->raise();
}


void ClassroomClientWidget::setSilence( bool visible, const QString& text )
{
	if( m_silencePage == nullptr )
	{
		m_silencePage = new ClassroomSilencePage( this );
	}
	m_silencePage->setText( text );
	m_silencePage->setVisible( visible );
	if( visible ) { m_silencePage->raise(); }
}


void ClassroomClientWidget::showCheckIn()
{
	if( m_checkIn == nullptr )
	{
		m_checkIn = new ClassroomCheckInDialog( this );
		connect( m_checkIn, &ClassroomCheckInDialog::submitted,
				 this, &ClassroomClientWidget::checkInSubmitted );
	}
	m_checkIn->prepare( m_className, m_seat );
	m_checkIn->show();
	m_checkIn->raise();
}


ClassroomCheckInDialog::ClassroomCheckInDialog( QWidget* parent ) :
	QWidget( parent )
{
	setWindowTitle( tr( "Attendance check-in" ) );
	setWindowFlags( Qt::Tool | Qt::WindowStaysOnTopHint );
	resize( 460, 210 );
	setStyleSheet( QStringLiteral("background:#ffffff;") );

	m_info = new QLabel( this );
	m_info->setWordWrap( true );
	m_info->setStyleSheet( QStringLiteral("color:#1f2d3d; font-size:15px; font-weight:bold;") );

	m_nameEdit = new QLineEdit( this );
	m_nameEdit->setPlaceholderText( tr( "Type your full name" ) );
	connect( m_nameEdit, &QLineEdit::returnPressed, this, &ClassroomCheckInDialog::submit );

	auto* submitButton = new QPushButton( tr( "Check in" ), this );
	connect( submitButton, &QPushButton::clicked, this, &ClassroomCheckInDialog::submit );

	auto* form = new QFormLayout;
	form->addRow( tr( "Name:" ), m_nameEdit );

	auto* layout = new QVBoxLayout( this );
	layout->addWidget( m_info );
	layout->addLayout( form );
	layout->addWidget( submitButton );
}


void ClassroomCheckInDialog::prepare( const QString& className, const QString& seat )
{
	m_info->setText( tr( "%1 - seat %2: teacher started a check-in. Type your name to confirm." )
					 .arg( className, seat.isEmpty() ? tr( "unknown" ) : seat ) );
	m_nameEdit->clear();
	m_nameEdit->setFocus();
}


void ClassroomCheckInDialog::submit()
{
	const auto name = m_nameEdit->text().trimmed();
	if( name.isEmpty() )
	{
		QMessageBox::information( this, windowTitle(), tr( "Please type your name first." ) );
		return;
	}
	Q_EMIT submitted( name );
	hide();
}


ClassroomScoreView::ClassroomScoreView( QWidget* parent ) :
	QWidget( parent )
{
	setWindowTitle( tr( "My score" ) );
	setWindowFlags( Qt::Tool | Qt::WindowStaysOnTopHint );
	resize( 620, 620 );
	m_view = new QTextBrowser( this );
	m_view->setOpenExternalLinks( false );
	auto* layout = new QVBoxLayout( this );
	layout->addWidget( m_view );
	auto* closeButton = new QPushButton( tr( "Close" ), this );
	layout->addWidget( closeButton );
	connect( closeButton, &QPushButton::clicked, this, &QWidget::hide );
}


void ClassroomScoreView::setHtml( const QString& html )
{
	m_view->setHtml( html );
}


ClassroomSilencePage::ClassroomSilencePage( QWidget* parent ) :
	QWidget( parent )
{
	setWindowFlags( Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint |
					Qt::WindowTransparentForInput );
	setWindowState( Qt::WindowFullScreen );
	setStyleSheet( QStringLiteral("background:#0d1b2a;") );
	m_label = new QLabel( this );
	m_label->setAlignment( Qt::AlignCenter );
	m_label->setWordWrap( true );
	m_label->setStyleSheet( QStringLiteral("color:#f0f0f0; font-size:48px; font-weight:bold;") );
	auto* layout = new QVBoxLayout( this );
	layout->addWidget( m_label );
}


void ClassroomSilencePage::setText( const QString& text )
{
	m_label->setText( text );
}


ClassroomAiChatWidget::ClassroomAiChatWidget( ClassroomAiClient* client, QWidget* parent ) :
	QWidget( parent ),
	m_aiClient( client )
{
	setWindowTitle( tr( "AI assistant" ) );
	setAttribute( Qt::WA_DeleteOnClose, false );
	keepOnTop( this );
	setFixedSize( 420, 460 );

	const auto dark = IkebackCore::useDarkMode();
	setStyleSheet( QStringLiteral(
		"QWidget#classroomAi { background: %1; border: 1px solid %2; border-radius: 6px; }"
		"QLabel, QTextBrowser { color: %3; }" ).
		arg( dark ? QStringLiteral("#2b2b2b") : QStringLiteral("#f5f7f9"),
			 dark ? QStringLiteral("#555555") : QStringLiteral("#9fc4d0"),
			 dark ? QStringLiteral("#f0f0f0") : QStringLiteral("#202020") ) );
	setObjectName( QStringLiteral("classroomAi") );

	auto* title = new QLabel( tr( "AI assistant (configured by the teacher)" ), this );
	title->setStyleSheet( QStringLiteral("font-weight: bold; padding: 4px;") );
	m_view = new QTextBrowser( this );
	m_view->setReadOnly( true );
	m_view->setOpenExternalLinks( true );
	m_input = new QLineEdit( this );
	m_input->setPlaceholderText( tr( "Ask a question..." ) );
	m_sendButton = new QPushButton( tr( "Send" ), this );

	auto* inputLayout = new QHBoxLayout;
	inputLayout->addWidget( m_input, 1 );
	inputLayout->addWidget( m_sendButton );

	auto* mainLayout = new QVBoxLayout( this );
	mainLayout->setContentsMargins( 8, 8, 8, 8 );
	mainLayout->addWidget( title );
	mainLayout->addWidget( m_view, 1 );
	mainLayout->addLayout( inputLayout );

	appendLine( tr( "Welcome! Your questions and this session are recorded in the classroom log." ) );

	connect( m_sendButton, &QPushButton::clicked, this, &ClassroomAiChatWidget::sendMessage );
	connect( m_input, &QLineEdit::returnPressed, this, &ClassroomAiChatWidget::sendMessage );
	connect( m_aiClient, &ClassroomAiClient::finished, this, [this]( bool success, const QString& reply ) {
		m_sendButton->setEnabled( true );
		if( success )
		{
			m_history.append( QVariantMap{ { QStringLiteral("role"), QStringLiteral("user") },
										   { QStringLiteral("content"), m_lastPrompt } } );
			m_history.append( QVariantMap{ { QStringLiteral("role"), QStringLiteral("assistant") },
										   { QStringLiteral("content"), reply } } );
			while( m_history.size() > 20 ) { m_history.removeFirst(); }
			appendLine( QStringLiteral("<b>AI:</b> %1").arg( reply.toHtmlEscaped() ) );
		}
		else
		{
			appendLine( QStringLiteral("<i>%1</i>").arg( reply.toHtmlEscaped() ) );
		}
	} );
}


void ClassroomAiChatWidget::setIdentityLabel( const QString& label )
{
	setWindowTitle( tr( "AI assistant - %1" ).arg( label ) );
}


void ClassroomAiChatWidget::appendLine( const QString& html )
{
	m_view->append( html );
	m_view->verticalScrollBar()->setValue( m_view->verticalScrollBar()->maximum() );
}


void ClassroomAiChatWidget::sendMessage()
{
	const auto prompt = m_input->text().trimmed();
	if( prompt.isEmpty() || m_sendButton->isEnabled() == false ) { return; }

	m_lastPrompt = prompt;
	m_input->clear();
	m_sendButton->setEnabled( false );
	appendLine( QStringLiteral("<b>%1</b> %2").arg( tr( "Me:" ), prompt.toHtmlEscaped() ) );
	Q_EMIT eventOccurred( Classroom::EventType::ai(), prompt.left( 200 ), QStringLiteral("AI") );
	m_aiClient->chat( m_history, prompt );
}


void ClassroomAiChatWidget::closeEvent( QCloseEvent* event )
{
	event->ignore();
	hide();
}


void ClassroomAiChatWidget::mousePressEvent( QMouseEvent* event )
{
	if( event->button() == Qt::LeftButton )
	{
		m_dragPosition = mouseGlobalPoint( event ) - frameGeometry().topLeft();
	}
	QWidget::mousePressEvent( event );
}


void ClassroomAiChatWidget::mouseMoveEvent( QMouseEvent* event )
{
	if( event->buttons() & Qt::LeftButton )
	{
		move( mouseGlobalPoint( event ) - m_dragPosition );
	}
	QWidget::mouseMoveEvent( event );
}


QPair<QString, QString> ClassroomMonitor::foregroundWindowInfo()
{
#ifdef Q_OS_WIN
	const auto window = GetForegroundWindow();
	if( window == nullptr ) { return {}; }

	wchar_t titleBuffer[512]{};
	const auto titleLength = GetWindowTextW( window, titleBuffer, 512 );
	const auto title = QString::fromWCharArray( titleBuffer, titleLength );

	DWORD processId = 0;
	GetWindowThreadProcessId( window, &processId );
	QString application;
	const auto process = OpenProcess( PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId );
	if( process != nullptr )
	{
		wchar_t pathBuffer[MAX_PATH]{};
		DWORD pathLength = MAX_PATH;
		if( QueryFullProcessImageNameW( process, 0, pathBuffer, &pathLength ) )
		{
			application = QFileInfo( QString::fromWCharArray( pathBuffer ) ).fileName();
		}
		CloseHandle( process );
	}
	return { application, title };
#else
	static const auto xprop = QStandardPaths::findExecutable( QStringLiteral("xprop") );
	if( xprop.isEmpty() ) { return {}; }

	QProcess process;
	process.start( xprop, { QStringLiteral("-root"), QStringLiteral("_NET_ACTIVE_WINDOW") } );
	process.waitForFinished( 300 );
	const auto output = QString::fromUtf8( process.readAllStandardOutput() );
	const auto id = output.section( QLatin1Char('#'), 1 ).trimmed();
	if( id.isEmpty() || id == QStringLiteral("0x0") ) { return {}; }

	process.start( xprop, { QStringLiteral("-id"), id, QStringLiteral("WM_NAME"), QStringLiteral("WM_CLASS") } );
	process.waitForFinished( 300 );
	const auto details = QString::fromUtf8( process.readAllStandardOutput() );
	QString title;
	QString application;
	for( const auto& line : details.split( QLatin1Char('\n') ) )
	{
		if( line.contains( QStringLiteral("WM_NAME") ) ) { title = line.section( QLatin1Char('='), 1 ).trimmed(); }
		if( line.contains( QStringLiteral("WM_CLASS") ) )
		{
			const auto values = line.section( QLatin1Char('='), 1 ).split( QLatin1Char(',') );
			application = values.value( values.size() - 1 ).trimmed().remove( QLatin1Char('"') );
		}
	}
	return { application, title.remove( QLatin1Char('"') ) };
#endif
}


ClassroomMonitor::ClassroomMonitor( QObject* parent ) :
	QObject( parent ),
	m_timer( new QTimer( this ) )
{
	connect( m_timer, &QTimer::timeout, this, &ClassroomMonitor::sample );
}


void ClassroomMonitor::setSettings( const QJsonObject& settings )
{
	m_enabled = settings.value( Classroom::SettingKey::monitoringEnabled() ).toBool( true );
	m_captureWindowTitle = settings.value( Classroom::SettingKey::captureWindowTitle() ).toBool( true );
	m_captureProcess = settings.value( Classroom::SettingKey::captureProcess() ).toBool( true );

	const auto toStringList = [&settings]( const QString& key ) {
		QStringList rules;
		for( const auto& value : settings.value( key ).toArray() )
		{
			const auto rule = value.toString().trimmed();
			// Fuzzy substring matching plus taskkill /F /IM: a one-character
			// rule such as "e" would terminate dozens of unrelated processes.
			if( rule.size() >= 4 ) { rules.append( rule ); }
		}
		return rules;
	};
	m_blockedApps = toStringList( Classroom::SettingKey::blockedApps() );
	m_blockedWebsites = toStringList( Classroom::SettingKey::blockedWebsites() );

	const auto interval = settings.value( Classroom::SettingKey::captureInterval() ).toInt( 5 );
	m_timer->setInterval( std::max( 1, interval ) * 1000 );
	if( m_enabled ) { start(); }
	else { stop(); }
}


void ClassroomMonitor::start()
{
	if( m_enabled ) { m_timer->start(); }
}


void ClassroomMonitor::stop()
{
	m_timer->stop();
}


void ClassroomMonitor::sample()
{
	const auto info = foregroundWindowInfo();
	const auto title = m_captureWindowTitle ? info.second : QString{};
	const auto application = m_captureProcess ? info.first : QString{};
	if( title.isEmpty() && application.isEmpty() ) { return; }
	if( title.contains( Classroom::productName() ) || title.contains( tr( "AI assistant" ) ) ||
		title.contains( tr( "My score" ) ) || title.contains( tr( "Attendance check-in" ) ) ) { return; }

	const QString key = application + QLatin1Char('|') + title;
	if( key == m_lastSample ) { return; }
	m_lastSample = key;

	static const QStringList browsers{ QStringLiteral("chrome"), QStringLiteral("msedge"), QStringLiteral("firefox"),
									  QStringLiteral("iexplore"), QStringLiteral("opera"), QStringLiteral("brave"),
									  QStringLiteral("360"), QStringLiteral("qqbrowser"), QStringLiteral("safari") };
	bool browser = false;
	for( const auto& candidate : browsers )
	{
		if( application.toLower().contains( candidate ) ) { browser = true; break; }
	}

	// Fuzzy blocking: every rule is a case-insensitive substring of either the
	// foreground process name (apps) or the browser window title (websites).
	const auto matchRule = []( const QString& value, const QStringList& rules ) {
		for( const auto& rule : rules )
		{
			if( rule.isEmpty() == false && value.contains( rule, Qt::CaseInsensitive ) ) { return rule; }
		}
		return QString{};
	};
	const auto appRule = matchRule( application, m_blockedApps );
	const auto webRule = browser ?
		matchRule( title + QLatin1Char(' ') + application, m_blockedWebsites ) : QString{};
	if( appRule.isEmpty() == false || webRule.isEmpty() == false )
	{
		Q_EMIT blockedActivity( application, title,
								appRule.isEmpty() ? webRule : appRule,
								appRule.isEmpty() );
	}

	Q_EMIT activity( application, title, browser );
}
