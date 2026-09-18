#include "maproutecollection.h"
#include "maproute.h"
MapRouteCollection::MapRouteCollection()
{
    mRouteNow = 0;
}

MapRoute& MapRouteCollection::getRoute(int ind)
{
    static MapRoute emptyRoute;
    if (mCollection.isEmpty()) {
        return emptyRoute;
    }

    if (ind < 0) {
        if (mRouteNow >= 0 && mRouteNow < mCollection.size()) {
            return mCollection[mRouteNow];
        } else {
            return emptyRoute;
        }
    } else {
        if (mCollection.size() > ind) {
            return mCollection[ind];
        } else {
            return emptyRoute;
        }
    }
}

QList<MapRoute>& MapRouteCollection::getRoutes()
{
    return mCollection;
}

void MapRouteCollection::setRoute(const MapRoute &route)
{
    mCollection[mRouteNow] = route;
}


void MapRouteCollection::addRoute(const MapRoute &route)
{
    while (!mCollection.isEmpty() &&
           mCollection.last().isEmpty() &&
           mRouteNow < mCollection.size()) {
        mCollection.removeLast();
    }
    mCollection.append(route);
}

int MapRouteCollection::getRouteNum()
{
    return mCollection.size();
}

void MapRouteCollection::clearRoute()
{
    mCollection[mRouteNow].clear();
}

void MapRouteCollection::clearAllRoutes()
{
    for (int i = 0;i < mCollection.size();i++) {
        mCollection[i].clear();
    }
    mCollection.clear();
}

void MapRouteCollection::setRouteNow(int routeNow)
{
    mRouteNow = routeNow;
    while (mCollection.size() < (mRouteNow + 1)) {
        MapRoute l;
        mCollection.append(l);
    }

    // Clean empty routes
    while (mRouteNow < (mCollection.size() - 1)) {
        if (mCollection.last().isEmpty()) {
            mCollection.removeLast();
        } else {
            break;
        }
    }
}


void MapRouteCollection::clear()
{
    mCollection.clear();
}

void MapRouteCollection::append(MapRoute &maproute)
{
    mCollection.append(maproute);
}


int MapRouteCollection::getRouteNow() const
{
    return mRouteNow;
}

int MapRouteCollection::size()
{
    return mCollection.size();
}

MapRoute& MapRouteCollection::getCurrent()
{
    qDebug() << "Route No: " << mRouteNow;
    return mCollection[mRouteNow];
}

MapRoute* MapRouteCollection::getCurrentP()
{
    return &(mCollection[mRouteNow]);
}

QList<MapRoute>::const_iterator MapRouteCollection::begin() const
{
    return mCollection.begin();
}

QList<MapRoute>::const_iterator MapRouteCollection::end() const
{
    return mCollection.end();
}

MapRoute& MapRouteCollection::at(int i)
{
    return mCollection[i];
}

MapRoute& MapRouteCollection::first()
{
    return mCollection.first();
}

MapRoute& MapRouteCollection::last()
{
    return mCollection.last();
}

void MapRouteCollection::prepend(const MapRoute &value)
{
    mCollection.prepend(value);
}

void MapRouteCollection::removeAt(int i)
{
    mCollection.removeAt(i);
}

