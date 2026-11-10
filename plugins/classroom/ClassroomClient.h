/*
 * ClassroomClient.h - always-on-top student widgets and activity sampler
 *
 * This file is part of the Veyon classroom modification.
 */

#pragma once

#include <QJsonObject>
#include <QObject>
#include <QPair>
#include <QPoint>
#include <QString>
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
	void setSessionKey( const QString& key );
	void setIdentity( const QString& className, const QString& seat, const QString& student );
	void clearIdentity();
	void setLocked( bool locked );
	void setScoreHtml( const QString& html );
	void setSilence( bool visible, const QString& text );

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
	void placeOnScreen();
	bool rosterAccepts( const QString& seat, const QString& name ) const;

	QLabel* m_statusLabel = nullptr;
	QLabel* m_lockLabel = nullptr;
	QLineEdit* m_seatEdit = nullptr;
	QLineEdit* m_nameEdit = nullptr;
	QLineEdit* m_keyEdit = nullptr;
	QPushButton* m_loginButton = nullptr;
	QPushButton* m_aiButton = nullptr;
	QPushButton* m_scoreButton = nullptr;
	QPushButton* m_callButton = nullptr;

	ClassroomAiClient* m_aiClient = nullptr;
	ClassroomAiChatWidget* m_aiChat = nullptr;
	ClassroomScoreView* m_scoreView = nullptr;
	ClassroomSilencePage* m_silencePage = nullptr;

	QString m_sessionKey;
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

private:
	void sample();
	static QPair<QString, QString> foregroundWindowInfo();

	QTimer* m_timer = nullptr;
	QString m_lastSample;
	bool m_enabled = true;
	bool m_captureWindowTitle = true;
	bool m_captureProcess = true;
};
