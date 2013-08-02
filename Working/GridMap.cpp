#include "GridMap.h"

GridMap::GridMap()
{
	for (unsigned char x = 0; x <= GRID_MAX_X; ++x)
	{
		for (unsigned char y = 0; y <= GRID_MAX_Y; ++y)
		{
			status[x][y] = 0x00;
		}
	}
}

//Change desired flag(s) to 1 for passed point
void GridMap::setFlag(Point pt, unsigned char inFlag)
{
	status[pt.x][pt.y] |= inFlag;
}

//Change desired flag(s) to 0 for passed point 
void GridMap::removeFlag(Point pt, unsigned char inFlag)
{
	status[pt.x][pt.y] &= ~inFlag;
}

//Returns true if the passed flag(s) is/are set
bool GridMap::isFlagSet(Point pt, unsigned char inFlag)
{
	return status[pt.x][pt.y] & inFlag;
}

//Returns true if point is in grid
bool GridMap::contains(Point point)
{
	return (point.x >= 0) && (point.x <= GRID_MAX_X) && (point.y >= 0) && (point.y <= GRID_MAX_Y);
}

//Returns true if there is a connection between pt1 and pt2, and are both inMaze()
bool GridMap::joined(Point pt1, Point pt2)
{
	unsigned char xDel = abs(pt1.x - pt2.x);
	unsigned char yDel = abs(pt1.y - pt2.y);
	return contains(pt1) && contains(pt2) && ((xDel == 1 || yDel == 1) && (xDel != yDel));
}

//Returns true if point is not occupied
bool GridMap::isPassable(Point point)
{
	return !(isFlagSet(point, OCCUPIED));
}
