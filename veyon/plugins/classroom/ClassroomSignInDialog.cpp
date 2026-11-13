/*
 * ClassroomSignInDialog.cpp - ikeback name + seat sign-in dialog
 *
 * This file is part of the Veyon classroom modification.
 */

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include "ClassroomSignInDialog.h"


ClassroomSignInDialog::ClassroomSignInDialog( const QStringList& classNames,
											  const QString& currentClassName,
											  QWidget* parent ) :
	QDialog( parent ),
	m_classCombo( new QComboBox( this ) ),
	m_nameEdit( new QLineEdit( this ) ),
	m_seatEdit( new QLineEdit( this ) ),
	m_buttons( new QDialogButtonBox( QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this ) )
{
	setWindowTitle( tr( "Student sign-in" ) );
	setMinimumWidth( 360 );

	m_classCombo->addItems( classNames );
	if( classNames.isEmpty() ) { m_classCombo->addItem( tr( "(no classes in roster)" ) ); }
	const auto index = m_classCombo->findText( currentClassName );
	if( index >= 0 ) { m_classCombo->setCurrentIndex( index ); }

	m_nameEdit->setPlaceholderText( tr( "Student name" ) );
	m_seatEdit->setPlaceholderText( tr( "Seat number" ) );

	auto* form = new QFormLayout;
	form->addRow( tr( "Class:" ), m_classCombo );
	form->addRow( tr( "Name:" ), m_nameEdit );
	form->addRow( tr( "Seat:" ), m_seatEdit );

	auto* note = new QLabel( tr( "Sign-in is by name and seat number only. "
								 "No Windows password is required or stored." ), this );
	note->setWordWrap( true );

	auto* layout = new QVBoxLayout( this );
	layout->addLayout( form );
	layout->addWidget( note );
	layout->addWidget( m_buttons );

	connect( m_nameEdit, &QLineEdit::textChanged, this, &ClassroomSignInDialog::updateOkButton );
	connect( m_seatEdit, &QLineEdit::textChanged, this, &ClassroomSignInDialog::updateOkButton );
	connect( m_buttons, &QDialogButtonBox::accepted, this, &QDialog::accept );
	connect( m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject );
	updateOkButton();
}


void ClassroomSignInDialog::updateOkButton()
{
	auto* ok = m_buttons->button( QDialogButtonBox::Ok );
	if( ok )
	{
		ok->setEnabled( m_nameEdit->text().trimmed().isEmpty() == false &&
						m_seatEdit->text().trimmed().isEmpty() == false );
	}
}


QString ClassroomSignInDialog::className() const
{
	return m_classCombo->currentText();
}


QString ClassroomSignInDialog::studentName() const
{
	return m_nameEdit->text().trimmed();
}


QString ClassroomSignInDialog::seat() const
{
	return m_seatEdit->text().trimmed();
}
