/*
 * ClassroomSeatGrid.h - live seat grid with thumbnails and one-click scoring
 *
 * This file is part of the Veyon classroom modification.
 */

#pragma once

#include <QImage>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QWidget>
#include <functional>

class QGridLayout;
class QLabel;
class QPushButton;
class QTimer;
class ClassroomRoster;

//! Teacher-side seat map. Deliberately lives inside the plugin: Veyon's
//! ComputerItemDelegate is compiled into the veyon-master executable and is
//! not exported, so drawing/clicking inside that delegate would require the
//! master to depend on classroom business data. This grid uses only public
//! core APIs (ComputerControlInterface::scaledFramebuffer) via callbacks.
class ClassroomSeatGrid : public QWidget
{
	Q_OBJECT
public:
	explicit ClassroomSeatGrid( ClassroomRoster& roster, QWidget* parent = nullptr );

	void setClassName( const QString& className );

	using ThumbnailProvider = std::function<QImage( const QString& ip )>;
	using ScoreProvider = std::function<int( const QString& seat )>;
	using ScoreSink = std::function<void( const QString& seat, int delta, const QString& reason )>;

	void setThumbnailProvider( ThumbnailProvider provider ) { m_thumbnailProvider = std::move( provider ); }
	void setScoreProvider( ScoreProvider provider ) { m_scoreProvider = std::move( provider ); }
	void setScoreSink( ScoreSink sink ) { m_scoreSink = std::move( sink ); }
	void setOnlineIps( const QStringList& ips );

private:
	struct SeatCard
	{
		QLabel* thumbnail = nullptr;
		QPushButton* score = nullptr;
		QLabel* caption = nullptr;
		QString ip;
		QString expectedName;
		QString seat;
	};

	void rebuild();
	void refresh();
	void chooseDelta( const QString& seat );

	ClassroomRoster& m_roster;
	QString m_className;
	QStringList m_onlineIps;
	ThumbnailProvider m_thumbnailProvider;
	ScoreProvider m_scoreProvider;
	ScoreSink m_scoreSink;

	QGridLayout* m_grid = nullptr;
	QLabel* m_summary = nullptr;
	QTimer* m_timer = nullptr;
	QMap<QString, SeatCard> m_cards;
};
