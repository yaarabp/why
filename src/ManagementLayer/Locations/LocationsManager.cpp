#include "LocationsManager.h"

#include "LocationsNavigatorManager.h"
#include "LocationsDataEditManager.h"

#include <DataLayer/DataStorageLayer/StorageFacade.h>
#include <DataLayer/DataStorageLayer/LocationStorage.h>

#include <QWidget>
#include <QSplitter>
#include <QHBoxLayout>
#include <QMessageBox>

using ManagementLayer::LocationsManager;
using ManagementLayer::LocationsNavigatorManager;
using ManagementLayer::LocationsDataEditManager;


LocationsManager::LocationsManager(QObject* _parent, QWidget* _parentWidget) :
	QObject(_parent),
	m_view(new QWidget(_parentWidget)),
	m_navigatorManager(new LocationsNavigatorManager(this, m_view)),
	m_dataEditManager(new LocationsDataEditManager(this, m_view))
{
	initView();
	initConnections();
}

QWidget* LocationsManager::view() const
{
	return m_view;
}

void LocationsManager::loadCurrentProject()
{
	m_dataEditManager->clean();
	m_navigatorManager->loadLocations();
}

void LocationsManager::aboutAddLocation(const QString& _name)
{
	DataStorageLayer::StorageFacade::locationStorage()->storeLocation(_name);
	m_navigatorManager->chooseLocation(_name);
}

void LocationsManager::aboutEditLocation(const QString& _name)
{
	//
	// Загрузить в редактор данных данные
	//
	m_dataEditManager->editLocation(_name);
}

void LocationsManager::aboutRemoveLocation(const QString& _name)
{
	//
	// Если пользователь серьёзно намерен удалить локацию
	//
	if (QMessageBox::question(
			m_view,
			tr("Remove Location"),
			tr("Are you shure to remove location?"),
			QMessageBox::Yes | QMessageBox::No) == 	QMessageBox::Yes) {
		//
		// ... удалим её
		//
		DataStorageLayer::StorageFacade::locationStorage()->removeLocation(_name);

		//
		// ... очистим редактор
		//
		m_dataEditManager->clean();
	}
}

void LocationsManager::initView()
{
	QSplitter* splitter = new QSplitter(m_view);
	splitter->addWidget(m_navigatorManager->view());
	splitter->addWidget(m_dataEditManager->view());
	splitter->setStretchFactor(1, 1);

	QHBoxLayout* layout = new QHBoxLayout;
	layout->setContentsMargins(QMargins());
	layout->setSpacing(0);
	layout->addWidget(splitter);

	m_view->setLayout(layout);
}

void LocationsManager::initConnections()
{
	connect(m_navigatorManager, SIGNAL(addLocation(QString)), this, SLOT(aboutAddLocation(QString)));
	connect(m_navigatorManager, SIGNAL(editLocation(QString)), this, SLOT(aboutEditLocation(QString)));
	connect(m_navigatorManager, SIGNAL(removeLocation(QString)), this, SLOT(aboutRemoveLocation(QString)));
	connect(m_navigatorManager, SIGNAL(refreshLocations()), this, SIGNAL(refreshLocations()));

	connect(m_dataEditManager, SIGNAL(locationNameChanged(QString,QString)), this, SIGNAL(locationNameChanged(QString,QString)));
}