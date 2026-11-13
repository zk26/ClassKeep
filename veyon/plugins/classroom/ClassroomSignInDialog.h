/*
 * ClassroomSignInDialog.h - ikeback name + seat sign-in dialog
 *
 * Replaces Veyon's native username/password logon (usersessioncontrol) with
 * the classroom identity model: a student name and a seat number. No Windows
 * credentials are collected, stored or transmitted.
 *
 * This file is part of the Veyon classroom modification.
 */

#pragma once

#include <QDialog>
#include <QString>
#include <QStringList>

class QComboBox;
class QDialogButtonBox;
class QLineEdit;

class ClassroomSignInDialog : public QDialog
{
	Q_OBJECT
public:
	explicit ClassroomSignInDialog( const QStringList& classNames,
									const QString& currentClassName,
									QWidget* parent = nullptr );

	QString className() const;
	QString studentName() const;
	QString seat() const;

private:
	void updateOkButton();

	QComboBox* m_classCombo = nullptr;
	QLineEdit* m_nameEdit = nullptr;
	QLineEdit* m_seatEdit = nullptr;
	QDialogButtonBox* m_buttons = nullptr;
};
