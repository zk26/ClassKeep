/*
 * ClassroomConfigurationPage.cpp - Veyon Configurator page for the classroom extension
 *
 * This file is part of the Veyon classroom modification.
 */

#include <QCheckBox>
#include <QFormLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>

#include "ClassroomConfiguration.h"
#include "ClassroomConfigurationPage.h"
#include "Configuration/UiMapping.h"


ClassroomConfigurationPage::ClassroomConfigurationPage( ClassroomConfiguration& configuration, QWidget* parent ) :
	ConfigurationPage( parent ),
	m_configuration( configuration )
{
	setWindowTitle( QStringLiteral("ClassKeep") );
	setWindowIcon( QIcon( QStringLiteral(":/core/icon32.png") ) );

	auto* form = new QFormLayout( this );
	const auto addWidget = [form]( QWidget* widget ) {
		Configuration::UiMapping::setFlags( widget, Configuration::Property::Flag::Standard );
		form->addWidget( widget );
		return widget;
	};
	const auto addRow = [form]( const QString& label, QWidget* widget ) {
		Configuration::UiMapping::setFlags( widget, Configuration::Property::Flag::Standard );
		form->addRow( label, widget );
		return widget;
	};

	m_enabled = qobject_cast<QCheckBox*>( addWidget( new QCheckBox( tr( "Enable classroom features" ), this ) ) );
	m_dataDirectory = qobject_cast<QLineEdit*>( addRow( tr( "Data directory:" ), new QLineEdit( this ) ) );
	m_sessionKey = qobject_cast<QLineEdit*>( addRow( tr( "Connection key:" ), new QLineEdit( this ) ) );
	m_sessionKey->setEchoMode( QLineEdit::Password );
	m_exitProtection = qobject_cast<QCheckBox*>( addWidget( new QCheckBox( tr( "Protect the client against being closed" ), this ) ) );
	m_exitRequiresAdmin = qobject_cast<QCheckBox*>( addWidget( new QCheckBox( tr( "Require administrator credentials to exit" ), this ) ) );
	m_exitAdminUsername = qobject_cast<QLineEdit*>( addRow( tr( "Administrator account:" ), new QLineEdit( this ) ) );
	m_allowSelfLogin = qobject_cast<QCheckBox*>( addWidget( new QCheckBox( tr( "Allow students to sign in themselves" ), this ) ) );

	form->addRow( new QLabel( tr( "AI assistant" ), this ) );
	m_aiEnabled = qobject_cast<QCheckBox*>( addWidget( new QCheckBox( tr( "Enable AI assistant" ), this ) ) );
	m_aiEndpoint = qobject_cast<QLineEdit*>( addRow( tr( "Local or network API endpoint:" ), new QLineEdit( this ) ) );
	m_aiApiKey = qobject_cast<QLineEdit*>( addRow( tr( "API key (optional):" ), new QLineEdit( this ) ) );
	m_aiApiKey->setEchoMode( QLineEdit::Password );
	m_aiModel = qobject_cast<QLineEdit*>( addRow( tr( "Model:" ), new QLineEdit( this ) ) );
	m_aiSystemPrompt = qobject_cast<QLineEdit*>( addRow( tr( "System prompt:" ), new QLineEdit( this ) ) );
	m_aiTimeout = qobject_cast<QSpinBox*>( addRow( tr( "Timeout (seconds):" ), new QSpinBox( this ) ) );
	m_aiTimeout->setRange( 5, 600 );

	form->addRow( new QLabel( tr( "Behaviour monitoring" ), this ) );
	m_monitoringEnabled = qobject_cast<QCheckBox*>( addWidget( new QCheckBox( tr( "Record active windows and applications" ), this ) ) );
	m_captureInterval = qobject_cast<QSpinBox*>( addRow( tr( "Sampling interval (seconds):" ), new QSpinBox( this ) ) );
	m_captureInterval->setRange( 1, 300 );
	m_retentionDays = qobject_cast<QSpinBox*>( addRow( tr( "Log retention (days):" ), new QSpinBox( this ) ) );
	m_retentionDays->setRange( 1, 3650 );

	form->addRow( new QLabel( tr( "Scoring, devices and bridge" ), this ) );
	m_scoreBaseScore = qobject_cast<QSpinBox*>( addRow( tr( "Base score:" ), new QSpinBox( this ) ) );
	m_scoreBaseScore->setRange( 0, 10000 );
	m_uwfEnabled = qobject_cast<QCheckBox*>( addWidget( new QCheckBox( tr( "Enable UWF integration" ), this ) ) );
	m_internetGuard = qobject_cast<QCheckBox*>( addWidget( new QCheckBox( tr( "Enable InternetGuard (WFP)" ), this ) ) );
	m_usbInputBlock = qobject_cast<QCheckBox*>( addWidget( new QCheckBox( tr( "Block USB input while locking" ), this ) ) );
	m_bridgePort = qobject_cast<QSpinBox*>( addRow( tr( "Mobile bridge port:" ), new QSpinBox( this ) ) );
	m_bridgePort->setRange( 1024, 65535 );
	m_bridgeEnabled = qobject_cast<QCheckBox*>( addWidget( new QCheckBox( tr( "Enable mobile bridge" ), this ) ) );
	m_syncEnabled = qobject_cast<QCheckBox*>( addWidget( new QCheckBox( tr( "Enable automatic LAN sync" ), this ) ) );
	m_silencePageText = qobject_cast<QLineEdit*>( addRow( tr( "Silence page text:" ), new QLineEdit( this ) ) );
	m_wordCloudStopWords = qobject_cast<QLineEdit*>( addRow( tr( "Extra word cloud stop words:" ), new QLineEdit( this ) ) );

	Configuration::UiMapping::setFlags( this, Configuration::Property::Flag::Standard );
}


void ClassroomConfigurationPage::resetWidgets()
{
	m_enabled->setChecked( m_configuration.enabled() );
	m_dataDirectory->setText( m_configuration.dataDirectory() );
	m_sessionKey->setText( m_configuration.sessionKey() );
	m_exitProtection->setChecked( m_configuration.exitProtection() );
	m_exitRequiresAdmin->setChecked( m_configuration.exitRequiresAdmin() );
	m_exitAdminUsername->setText( m_configuration.exitAdminUsername() );
	m_allowSelfLogin->setChecked( m_configuration.allowSelfLogin() );
	m_aiEnabled->setChecked( m_configuration.aiEnabled() );
	m_aiEndpoint->setText( m_configuration.aiEndpoint() );
	m_aiApiKey->setText( m_configuration.aiApiKey() );
	m_aiModel->setText( m_configuration.aiModel() );
	m_aiSystemPrompt->setText( m_configuration.aiSystemPrompt() );
	m_aiTimeout->setValue( m_configuration.aiTimeout() );
	m_monitoringEnabled->setChecked( m_configuration.monitoringEnabled() );
	m_captureInterval->setValue( m_configuration.captureInterval() );
	m_retentionDays->setValue( m_configuration.retentionDays() );
	m_scoreBaseScore->setValue( m_configuration.scoreBaseScore() );
	m_uwfEnabled->setChecked( m_configuration.uwfEnabled() );
	m_internetGuard->setChecked( m_configuration.internetGuard() );
	m_usbInputBlock->setChecked( m_configuration.usbInputBlock() );
	m_bridgePort->setValue( m_configuration.bridgePort() );
	m_bridgeEnabled->setChecked( m_configuration.bridgeEnabled() );
	m_syncEnabled->setChecked( m_configuration.syncEnabled() );
	m_silencePageText->setText( m_configuration.silencePageText() );
	m_wordCloudStopWords->setText( m_configuration.wordCloudStopWords() );
}


void ClassroomConfigurationPage::connectWidgetsToProperties()
{
	const auto changed = [this]() { Q_EMIT widgetsChanged(); };

	connect( m_enabled, &QCheckBox::toggled, this, [this, changed]( bool value ) {
		m_configuration.setEnabled( value ); changed(); } );
	connect( m_dataDirectory, &QLineEdit::textChanged, this, [this, changed]( const QString& value ) {
		m_configuration.setDataDirectory( value ); changed(); } );
	connect( m_sessionKey, &QLineEdit::textChanged, this, [this, changed]( const QString& value ) {
		m_configuration.setSessionKey( value ); changed(); } );
	connect( m_exitProtection, &QCheckBox::toggled, this, [this, changed]( bool value ) {
		m_configuration.setExitProtection( value ); changed(); } );
	connect( m_exitRequiresAdmin, &QCheckBox::toggled, this, [this, changed]( bool value ) {
		m_configuration.setExitRequiresAdmin( value ); changed(); } );
	connect( m_exitAdminUsername, &QLineEdit::textChanged, this, [this, changed]( const QString& value ) {
		m_configuration.setExitAdminUsername( value ); changed(); } );
	connect( m_allowSelfLogin, &QCheckBox::toggled, this, [this, changed]( bool value ) {
		m_configuration.setAllowSelfLogin( value ); changed(); } );
	connect( m_aiEnabled, &QCheckBox::toggled, this, [this, changed]( bool value ) {
		m_configuration.setAiEnabled( value ); changed(); } );
	connect( m_aiEndpoint, &QLineEdit::textChanged, this, [this, changed]( const QString& value ) {
		m_configuration.setAiEndpoint( value ); changed(); } );
	connect( m_aiApiKey, &QLineEdit::textChanged, this, [this, changed]( const QString& value ) {
		m_configuration.setAiApiKey( value ); changed(); } );
	connect( m_aiModel, &QLineEdit::textChanged, this, [this, changed]( const QString& value ) {
		m_configuration.setAiModel( value ); changed(); } );
	connect( m_aiSystemPrompt, &QLineEdit::textChanged, this, [this, changed]( const QString& value ) {
		m_configuration.setAiSystemPrompt( value ); changed(); } );
	connect( m_aiTimeout, QOverload<int>::of( &QSpinBox::valueChanged ), this, [this, changed]( int value ) {
		m_configuration.setAiTimeout( value ); changed(); } );
	connect( m_monitoringEnabled, &QCheckBox::toggled, this, [this, changed]( bool value ) {
		m_configuration.setMonitoringEnabled( value ); changed(); } );
	connect( m_captureInterval, QOverload<int>::of( &QSpinBox::valueChanged ), this, [this, changed]( int value ) {
		m_configuration.setCaptureInterval( value ); changed(); } );
	connect( m_retentionDays, QOverload<int>::of( &QSpinBox::valueChanged ), this, [this, changed]( int value ) {
		m_configuration.setRetentionDays( value ); changed(); } );
	connect( m_scoreBaseScore, QOverload<int>::of( &QSpinBox::valueChanged ), this, [this, changed]( int value ) {
		m_configuration.setScoreBaseScore( value ); changed(); } );
	connect( m_uwfEnabled, &QCheckBox::toggled, this, [this, changed]( bool value ) {
		m_configuration.setUwfEnabled( value ); changed(); } );
	connect( m_internetGuard, &QCheckBox::toggled, this, [this, changed]( bool value ) {
		m_configuration.setInternetGuard( value ); changed(); } );
	connect( m_usbInputBlock, &QCheckBox::toggled, this, [this, changed]( bool value ) {
		m_configuration.setUsbInputBlock( value ); changed(); } );
	connect( m_bridgePort, QOverload<int>::of( &QSpinBox::valueChanged ), this, [this, changed]( int value ) {
		m_configuration.setBridgePort( value ); changed(); } );
	connect( m_bridgeEnabled, &QCheckBox::toggled, this, [this, changed]( bool value ) {
		m_configuration.setBridgeEnabled( value ); changed(); } );
	connect( m_syncEnabled, &QCheckBox::toggled, this, [this, changed]( bool value ) {
		m_configuration.setSyncEnabled( value ); changed(); } );
	connect( m_silencePageText, &QLineEdit::textChanged, this, [this, changed]( const QString& value ) {
		m_configuration.setSilencePageText( value ); changed(); } );
	connect( m_wordCloudStopWords, &QLineEdit::textChanged, this, [this, changed]( const QString& value ) {
		m_configuration.setWordCloudStopWords( value ); changed(); } );
}
