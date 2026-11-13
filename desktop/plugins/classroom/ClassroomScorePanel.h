/*
 * ClassroomScorePanel.h - teacher UI for daily scoring, rules, word clouds and term reports
 *
 * This file is part of the Ikeback classroom modification.
 */

#pragma once

#include <QByteArray>
#include <QDate>
#include <QJsonObject>
#include <QWidget>

#include "ClassroomScore.h"

class QBoxLayout;
class QCheckBox;
class QComboBox;
class QDateEdit;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QSpinBox;
class QTabWidget;
class QTableWidget;
class QTextEdit;
class ClassroomLogStore;
class ClassroomRoster;

class ClassroomScorePanel : public QWidget
{
	Q_OBJECT
public:
	ClassroomScorePanel( const QString& dataDirectory, ClassroomRoster& roster,
						 ClassroomLogStore& store, const QJsonObject& settings,
						 QWidget* parent = nullptr );

	void setClassName( const QString& className );
	void refresh();
	void showCall( const QString& ip, const QString& seat, const QString& student );

	//! Used by the seat grid: current total for a seat, and quick +/- scoring.
	int scoreForSeat( const QString& seat ) const;
	void addEntryForSeat( const QString& seat, int delta, const QString& label,
						  const QString& richText = QString{} );

private:
	void setupDailyTab();
	void setupRulesTab();
	void setupCloudTab();
	void setupReportTab();
	void setupRichTextTools( QTextEdit* editor, QBoxLayout* layout );

	void refreshStudentTable();
	void ensureLesson();
	void addEntry( int delta, const QString& label );
	void saveRules();
	void generateCloud();
	void exportReport( bool xls );
	void insertImage( QTextEdit* editor );

	QString currentSeat() const;
	QString studentName( const QString& seat ) const;

	QString m_dataDirectory;
	ClassroomRoster& m_roster;
	ClassroomLogStore& m_store;
	QJsonObject m_settings;
	mutable ClassroomScoreBook m_book;
	QString m_className;
	QString m_lessonId;

	QTabWidget* m_tabs = nullptr;
	QDateEdit* m_lessonDate = nullptr;
	QLineEdit* m_lessonTopic = nullptr;
	QTableWidget* m_studentTable = nullptr;
	QSpinBox* m_delta = nullptr;
	QTextEdit* m_detail = nullptr;
	QLabel* m_callLabel = nullptr;

	QSpinBox* m_baseScore = nullptr;
	QCheckBox* m_publish = nullptr;
	QTextEdit* m_rulesEditor = nullptr;
	QPlainTextEdit* m_quickActions = nullptr;

	QComboBox* m_cloudStudent = nullptr;
	QLabel* m_cloudView = nullptr;
	QByteArray m_lastCloudSvg;

	QDateEdit* m_reportFrom = nullptr;
	QDateEdit* m_reportTo = nullptr;
	QLineEdit* m_term = nullptr;
	QCheckBox* m_includeRules = nullptr;
	QCheckBox* m_attendanceColumn = nullptr;
	QCheckBox* m_includeDetails = nullptr;
};
