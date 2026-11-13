/*
 * ClassroomSeatGrid.h - live seat grid with thumbnails and one-click scoring
 *
 * This file is part of the Ikeback classroom modification.
 */

#pragma once

#include <QImage>
#include <QMap>
#include <QPoint>
#include <QString>
#include <QStringList>
#include <QWidget>
#include <functional>

class QEvent;
class QFrame;
class QGridLayout;
class QLabel;
class QObject;
class QPushButton;
class QTableWidget;
class QTimer;
class ClassroomRoster;

//! Teacher-side seat map. Deliberately lives inside the plugin: Ikeback's
//! ComputerItemDelegate is compiled into the ikeback-master executable and is
//! not exported, so drawing/clicking inside that delegate would require the
//! master to depend on classroom business data. This grid uses only public
//! core APIs (ComputerControlInterface::scaledFramebuffer) via callbacks.
class ClassroomSeatGrid : public QWidget
{
	Q_OBJECT
public:
	explicit ClassroomSeatGrid( ClassroomRoster& roster, QWidget* parent = nullptr );

	void setClassName( const QString& className );

	//! Grid = live thumbnails, draggable cards. List = all information (including
	//! UWF state) without any student screen capture.
	enum class ViewMode { Grid, List };

	using ThumbnailProvider = std::function<QImage( const QString& ip )>;
	using ScoreProvider = std::function<int( const QString& seat )>;
	using ScoreSink = std::function<void( const QString& seat, int delta, const QString& reason )>;
	//! Client-process protection (student cannot quit the ikeback worker).
	using ProtectionProvider = std::function<bool( const QString& seat )>;
	//! OS-level UWF restore state, as text for the list view.
	using UwfProvider = std::function<QString( const QString& seat )>;
	using IdentityProvider = std::function<QString( const QString& seat )>;

	void setThumbnailProvider( ThumbnailProvider provider ) { m_thumbnailProvider = std::move( provider ); }
	void setScoreProvider( ScoreProvider provider ) { m_scoreProvider = std::move( provider ); }
	void setScoreSink( ScoreSink sink ) { m_scoreSink = std::move( sink ); }
	void setProtectionProvider( ProtectionProvider provider ) { m_protectionProvider = std::move( provider ); }
	void setUwfProvider( UwfProvider provider ) { m_uwfProvider = std::move( provider ); }
	void setIdentityProvider( IdentityProvider provider ) { m_identityProvider = std::move( provider ); }
	void setOnlineIps( const QStringList& ips );

	void setViewMode( ViewMode mode );
	ViewMode viewMode() const { return m_viewMode; }

protected:
	//! Drag-and-drop reordering of seat cards (installed as an event filter on
	//! each card and its thumbnail; the score button keeps its normal clicks).
	bool eventFilter( QObject* watched, QEvent* event ) override;

private:
	struct SeatCard
	{
		QFrame* root = nullptr;
		QLabel* thumbnail = nullptr;
		QPushButton* score = nullptr;
		QLabel* caption = nullptr;
		QString ip;
		QString expectedName;
		QString seat;
	};

	void rebuild();
	void rebuildList();
	void refresh();
	void refreshList();
	void chooseDelta( const QString& seat );
	void swapSeats( const QString& from, const QString& to );
	QString protectionIcon( const QString& seat ) const;

	ClassroomRoster& m_roster;
	QString m_className;
	QStringList m_onlineIps;
	ThumbnailProvider m_thumbnailProvider;
	ScoreProvider m_scoreProvider;
	ScoreSink m_scoreSink;
	ProtectionProvider m_protectionProvider;
	UwfProvider m_uwfProvider;
	IdentityProvider m_identityProvider;

	ViewMode m_viewMode = ViewMode::Grid;
	QWidget* m_gridPage = nullptr;
	QGridLayout* m_grid = nullptr;
	QTableWidget* m_table = nullptr;
	QPushButton* m_viewButton = nullptr;
	QLabel* m_summary = nullptr;
	QTimer* m_timer = nullptr;
	QMap<QString, SeatCard> m_cards;

	//! Persisted custom order. The file is shared with the mobile bridge.
	QStringList m_order;
	QPoint m_dragStart;
	QString m_dragSeat;
};
