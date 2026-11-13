/*
 * ClassroomClient.h - non-topmost, non-closable student card, popups and sampler
 *
 * This file is part of the Ikeback classroom modification.
 */

#pragma once

#include <QJsonObject>
#include <QObject>
#include <QPair>
#include <QPoint>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QWidget>

class QCloseEvent;
class QContextMenuEvent;
class QLabel;
class QLineEdit;
class QMouseEvent;
class QPushButton;
class QTextBrowser;
class QTimer;
class ClassroomAiChatWidget;
class ClassroomAiClient;
class ClassroomCheckInDialog;
class ClassroomScoreView;
class ClassroomSilencePage;
class QTextBrowser;

class ClassroomClientWidget : public QWidget
{
	Q_OBJECT
public:
	explicit ClassroomClientWidget( QWidget* parent = nullptr );

	void setSettings( const QJsonObject& settings );
	void setRoster( const QJsonObject& roster );
	//! The worker never receives the bridge key, only this boolean.
	void setKeyConfigured( bool configured );
	void deliverAiReply( bool success, const QString& reply );
	void setIdentity( const QString& className, const QString& seat, const QString& student );
	void clearIdentity();
	void setLocked( bool locked );
	void setScoreHtml( const QString& html );
	void setSilence( bool visible, const QString& text );
	void showCheckIn();
	//! Only the teacher's StopClient command may hide the card.
	void allowHide();
	void reactivate();
	//! Used by the ikeback tray icon context menu.
	void openAiAssistant();
	void requestProtectedQuit();
	void requestScore();

	QString className() const { return m_className; }
	QString seat() const { return m_seat; }
	QString student() const { return m_student; }
	bool loggedIn() const { return m_loggedIn; }

Q_SIGNALS:
	void eventOccurred( const QString& type, const QString& detail, const QString& app );
	void identityChanged( const QString& seat, const QString& student, bool loggedIn );
	void quitAuthorized();
	void scoreRequested();
	void callTeacherRequested();
	void checkInSubmitted( const QString& name );
	//! Forwarded by the plugin to the teacher machine, which owns the API key.
	void aiPromptRequested( const QString& prompt, const QVariantList& history );

protected:
	void closeEvent( QCloseEvent* event ) override;
	void contextMenuEvent( QContextMenuEvent* event ) override;
	void mousePressEvent( QMouseEvent* event ) override;
	void mouseMoveEvent( QMouseEvent* event ) override;

private:
	void handleLogin();
	void showAiChat();
	void requestQuit();
	void updateStatus();
	void updateVisibility();
	void placeOnScreen();
	bool rosterAccepts( const QString& seat, const QString& name ) const;

	QLabel* m_identityLabel = nullptr;
	QLabel* m_statusLabel = nullptr;
	QLabel* m_lockLabel = nullptr;
	QLineEdit* m_seatEdit = nullptr;
	QLineEdit* m_nameEdit = nullptr;
	QPushButton* m_loginButton = nullptr;
	QPushButton* m_aiButton = nullptr;
	QPushButton* m_scoreButton = nullptr;
	QPushButton* m_callButton = nullptr;

	ClassroomAiClient* m_aiClient = nullptr;
	ClassroomAiChatWidget* m_aiChat = nullptr;
	ClassroomScoreView* m_scoreView = nullptr;
	ClassroomSilencePage* m_silencePage = nullptr;
	ClassroomCheckInDialog* m_checkIn = nullptr;

	bool m_keyConfigured = false;
	QString m_className;
	QString m_seat;
	QString m_student;
	QJsonObject m_roster;

	bool m_loggedIn = false;
	bool m_locked = false;
	bool m_allowSelfLogin = true;
	bool m_aiEnabled = true;
	bool m_exitProtection = true;
	bool m_exitRequiresAdmin = true;
	QString m_exitAdminUsername;
	QPoint m_dragPosition;
	//! The card must stay on screen for as long as the ikeback service runs.
	QTimer* m_guardTimer = nullptr;
	bool m_hideAllowed = false;
};


class ClassroomAiChatWidget : public QWidget
{
	Q_OBJECT
public:
	explicit ClassroomAiChatWidget( ClassroomAiClient* client, QWidget* parent = nullptr );

	void setIdentityLabel( const QString& label );

Q_SIGNALS:
	void eventOccurred( const QString& type, const QString& detail, const QString& app );

protected:
	void closeEvent( QCloseEvent* event ) override;
	void mousePressEvent( QMouseEvent* event ) override;
	void mouseMoveEvent( QMouseEvent* event ) override;

private:
	void sendMessage();
	void appendLine( const QString& html );

	ClassroomAiClient* m_aiClient = nullptr;
	QTextBrowser* m_view = nullptr;
	QLineEdit* m_input = nullptr;
	QPushButton* m_sendButton = nullptr;
	QVariantList m_history;
	QString m_lastPrompt;
	QPoint m_dragPosition;
};


class ClassroomScoreView : public QWidget
{
	Q_OBJECT
public:
	explicit ClassroomScoreView( QWidget* parent = nullptr );
	void setHtml( const QString& html );

private:
	QTextBrowser* m_view = nullptr;
};


class ClassroomSilencePage : public QWidget
{
	Q_OBJECT
public:
	explicit ClassroomSilencePage( QWidget* parent = nullptr );
	void setText( const QString& text );

private:
	QLabel* m_label = nullptr;
};


//! Attendance prompt shown on every student desktop when the teacher starts a check-in.
class ClassroomCheckInDialog : public QWidget
{
	Q_OBJECT
public:
	explicit ClassroomCheckInDialog( QWidget* parent = nullptr );

	void prepare( const QString& className, const QString& seat );

Q_SIGNALS:
	void submitted( const QString& name );

private:
	void submit();

	QLabel* m_info = nullptr;
	QLineEdit* m_nameEdit = nullptr;
};


class ClassroomMonitor : public QObject
{
	Q_OBJECT
public:
	explicit ClassroomMonitor( QObject* parent = nullptr );

	void setSettings( const QJsonObject& settings );
	void start();
	void stop();

Q_SIGNALS:
	void activity( const QString& app, const QString& title, bool browser );
	//! Emitted when a fuzzy (case-insensitive substring) app / website rule matches.
	void blockedActivity( const QString& app, const QString& title, const QString& rule, bool browser );

private:
	void sample();
	static QPair<QString, QString> foregroundWindowInfo();

	QTimer* m_timer = nullptr;
	QString m_lastSample;
	QStringList m_blockedApps;
	QStringList m_blockedWebsites;
	bool m_enabled = true;
	bool m_captureWindowTitle = true;
	bool m_captureProcess = true;
};
