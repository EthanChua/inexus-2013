#include "Config.h"
#include <PololuWheelEncoders.h>
#include <PololuQTRSensors.h>
#include "Claw.h"
#include "IR.h"
#include "LineSensors.h"
#include <Servo.h>
#include "Movement.h"
#include "Motor.h"
#include "MazeImports.h"
#include "GridMap.h"
#include "Routing.h"

#define CENTRE_DIST (300)
#define BLOCK_DIST (250)
#define BLOCK_TOLERANCE (15)
#define BLOCK_STOP (60)

#define TURN_RIGHT 90
#define TURN_LEFT -90
#define TURN_FRONT 0
#define TURN_BACK 180

//F=0, R=1, B=2, L=3
//enum RelativeDir {RelFront, RelRight, RelBack, RelLeft};
#define REL_RIGHT 1
#define REL_LEFT 3
#define REL_FRONT 0
#define REL_BACK 2

#define DIR_NORTH 1
#define DIR_EAST 2
#define DIR_SOUTH 3
#define DIR_WEST 4

Motor motors;
LineSensors ls;
void lineFollowDemoSetup();
void lineFollowDemo();

Movement mover(&motors, &ls);

IR frontIr(IR_SHORT_PIN, IR::shortRange);
IR backIr(IR_MEDIUML_PIN, IR::mediumRange);
IR leftIr(IR_MEDIUML_PIN, IR::mediumRange);
IR rightIr(IR_MEDIUML_PIN, IR::mediumRange);

Claw claw(CLAW_LEFT_PIN, CLAW_RIGHT_PIN);

Routing router;

Path path;

GridMap gridMap;

struct irValues
{
	int frnt;
	int bck;
	int lft;
	int rght;
};

//stores distance in mm read from each ir sensor
irValues irInMm = {100, 100, 100, 100};

//start at (GRID_MAX_X, 0)
Point currPoint(GRID_MAX_X, 0);

//used to use points as function parameters
Point tempPoint(0, 0);

bool haveBlock = false;

//TODO: Change type back to Direction
unsigned char facing = DIR_WEST;
int turn = TURN_RIGHT;

unsigned char findNewFacing(unsigned char relativeTurn)
{
	if( (facing == DIR_NORTH && relativeTurn == REL_FRONT) || (facing == DIR_EAST && relativeTurn == REL_LEFT) || (facing == DIR_SOUTH && relativeTurn == REL_BACK) || (facing == DIR_WEST && relativeTurn == REL_RIGHT) )
		return DIR_NORTH;
	else if( (facing == DIR_NORTH && relativeTurn == REL_RIGHT) || (facing == DIR_EAST && relativeTurn == REL_FRONT) || (facing == DIR_SOUTH && relativeTurn == REL_LEFT) || (facing == DIR_WEST && relativeTurn == REL_BACK) )
		return DIR_EAST;
	else if( (facing == DIR_NORTH && relativeTurn == REL_BACK) || (facing == DIR_EAST && relativeTurn == REL_RIGHT) || (facing == DIR_SOUTH && relativeTurn == REL_FRONT) || (facing == DIR_WEST && relativeTurn == REL_LEFT) )
		return DIR_SOUTH;
	else if( (facing == DIR_NORTH && relativeTurn == REL_LEFT) || (facing == DIR_EAST && relativeTurn == REL_BACK) || (facing == DIR_SOUTH && relativeTurn == REL_RIGHT) || (facing == DIR_WEST && relativeTurn == REL_FRONT) )
		return DIR_WEST;
	else
		return DIR_NORTH; //Explode
}

bool isBlock(unsigned char dir)
{
	if (dir == REL_LEFT)
		return (abs(BLOCK_DIST - irInMm.lft) < BLOCK_TOLERANCE);
	else if (dir == REL_RIGHT)
		return (abs(BLOCK_DIST - irInMm.rght) < BLOCK_TOLERANCE);
	else
		return false;
}

bool obtainFrontBlock()
{
	irInMm.frnt = frontIr.getDist();
	while (irInMm.frnt > BLOCK_STOP)
	{
		mover.moveForward(DEFAULT_SPEED);
		irInMm.frnt = frontIr.getDist();
	}
	motors.stop();
	claw.close();
	mover.moveTillPoint(DEFAULT_SPEED);
	mover.moveOffCross(DEFAULT_SPEED);
	irInMm.frnt = frontIr.getDist();
	if (irInMm.frnt <= BLOCK_STOP)
		return true;
	else
		return false;
}

unsigned char dirOfPoint(Point pt)
{
	if (pt.y == currPoint.y + 1)
		DIR_NORTH;
	else if (pt.y == currPoint.y - 1)
		DIR_SOUTH;
	else if (pt.y == currPoint.x + 1)
		DIR_EAST;
	else if (pt.y == currPoint.x - 1)
		DIR_WEST;
	//TODO: Add NWEST etc.
}

void moveToPoint(Point pt)
{
	unsigned char newDir = dirOfPoint(pt);
	turnInDir(newDir);
	facing = newDir;
	mover.moveTillPoint(DEFAULT_SPEED);
	mover.moveOffCross(DEFAULT_SPEED);
}

void dropOff()
{
	Path path;
	Point startPoint(GRID_MAX_X, 0);
	router.generateRoute(currPoint, startPoint, (Direction)facing, &path);
	for (int ii = 0; ii < path.length; ++ii)
	{
		moveToPoint(path.path[ii]);
	}
	claw.open();
}

void turnInDir(unsigned char newDir)
{
	signed char delta = newDir - facing;
	if (delta < 0)
		delta += 4;
	turn = (unsigned char) delta;
	switch (turn)
	{
		case REL_FRONT:
			mover.rotateAngle(TURN_FRONT, DEFAULT_SPEED);
			break;
		case REL_BACK:
			mover.rotateAngle(TURN_BACK, DEFAULT_SPEED);
			break;
		case REL_LEFT:
			mover.rotateAngle(TURN_LEFT, DEFAULT_SPEED);
			break;
		case REL_RIGHT:
			mover.rotateAngle(TURN_RIGHT, DEFAULT_SPEED);
			break;
	}
}

Point adjacentPoint(Point pt, unsigned char relativeTurn)
{
	unsigned char dir = findNewFacing(relativeTurn);
		
	switch(dir)
	{
		case DIR_NORTH:
			pt.y += 1;
			break;
		case DIR_SOUTH:
			pt.y -= 1;
			break;
		case DIR_EAST:
			pt.x += 1;
			break;
		case DIR_WEST:
			pt.x -= 1;
			break;
//		case NEAST:
//			pt.y += 1;
//			pt.x += 1;
//			break;
//		case NWEST:
//			pt.y += 1;
//			pt.x -= 1;
//			break;
//		case SEAST:
//			pt.y -= 1;
//			pt.x -= 1;
//			break;
//		case SWEST:
//			pt.y -= 1;
//			pt.x -= 1;
//			break;
	}
	return pt;
}

//Base case for traversal
//At start: scan front and right, if no blocks move to front point
//At next point: scan front and right, if no blocks turn right and move
void initTraverse()
{
	gridMap.setFlag(currPoint, VISITED);
	
	//scan front point and set seen flag
	irInMm.frnt = frontIr.getDist();
	tempPoint = adjacentPoint(currPoint, REL_FRONT);
	gridMap.setFlag(tempPoint, SEEN);
	
	//scan right point and set seen flag
	irInMm.rght = rightIr.getDist();
	tempPoint = adjacentPoint(currPoint, REL_RIGHT);
	gridMap.setFlag(tempPoint, SEEN);
	
	if (isBlock(REL_FRONT) )
	{
		tempPoint = adjacentPoint(currPoint, REL_FRONT);
		gridMap.setFlag(tempPoint, OCCUPIED);
		haveBlock = obtainFrontBlock();
		currPoint = adjacentPoint(currPoint, REL_FRONT);
	}
	else if (isBlock(REL_RIGHT) )
	{
		tempPoint = adjacentPoint(currPoint, REL_RIGHT);
		gridMap.setFlag(tempPoint, OCCUPIED);
		//turn right
		mover.rotateAngle(TURN_RIGHT, DEFAULT_SPEED);
		facing = findNewFacing(REL_RIGHT);
		//now facing block
		haveBlock = obtainFrontBlock();
		currPoint = adjacentPoint(currPoint, REL_FRONT);
	}
	else
	{
		//move to front point
		mover.moveTillPoint(DEFAULT_SPEED);
		mover.moveOffCross(DEFAULT_SPEED);
		currPoint = adjacentPoint(currPoint, REL_FRONT);
	}
}

void secondTraverse()
{
	gridMap.setFlag(currPoint, VISITED);
	
	//scan front point and set seen flag
	irInMm.frnt = frontIr.getDist();
	tempPoint = adjacentPoint(currPoint, REL_FRONT);
	gridMap.setFlag(tempPoint, SEEN);
	
	//scan right point and set seen flag
	irInMm.rght = rightIr.getDist();
	tempPoint = adjacentPoint(currPoint, REL_RIGHT);
	gridMap.setFlag(tempPoint, SEEN);
	
	if (isBlock(REL_FRONT) )
	{
		tempPoint = adjacentPoint(currPoint, REL_FRONT);
		gridMap.setFlag(tempPoint, OCCUPIED);
		haveBlock = obtainFrontBlock();
		currPoint = adjacentPoint(currPoint, REL_FRONT);
	}
	else if (isBlock(REL_RIGHT) )
	{
		tempPoint = adjacentPoint(currPoint, REL_RIGHT);
		gridMap.setFlag(tempPoint, OCCUPIED);
		//turn right
		mover.rotateAngle(TURN_RIGHT, DEFAULT_SPEED);
		facing = findNewFacing(REL_RIGHT);
		//now facing block
		haveBlock = obtainFrontBlock();
		currPoint = adjacentPoint(currPoint, REL_FRONT);
	}
	else
	{
		//move to right point
		mover.rotateAngle(TURN_RIGHT, DEFAULT_SPEED);
		facing = findNewFacing(REL_RIGHT);
		mover.moveTillPoint(DEFAULT_SPEED);
		mover.moveOffCross(DEFAULT_SPEED);
		currPoint = adjacentPoint(currPoint, REL_FRONT);
	}
}

void traverseLong()
{
	gridMap.setFlag(currPoint, VISITED);
	
	tempPoint = adjacentPoint(currPoint, REL_FRONT);
	if (gridMap.contains(tempPoint) )
	{
		//scan front point and set seen flag
		irInMm.frnt = frontIr.getDist();
		tempPoint = adjacentPoint(currPoint, REL_FRONT);
		gridMap.setFlag(tempPoint, SEEN);
		
		//scan right point and set seen flag
		irInMm.rght = rightIr.getDist();
		tempPoint = adjacentPoint(currPoint, REL_RIGHT);
		gridMap.setFlag(tempPoint, SEEN);
		
		//scan left point and set seen flag
		irInMm.lft = leftIr.getDist();
		tempPoint = adjacentPoint(currPoint, REL_LEFT);
		gridMap.setFlag(tempPoint, SEEN);
		
		if (isBlock(REL_FRONT) )
		{
			tempPoint = adjacentPoint(currPoint, REL_FRONT);
			gridMap.setFlag(tempPoint, OCCUPIED);
			haveBlock = obtainFrontBlock();
			currPoint = adjacentPoint(currPoint, REL_FRONT);
		}
		else if (isBlock(REL_RIGHT) )
		{
			tempPoint = adjacentPoint(currPoint, REL_RIGHT);
			gridMap.setFlag(tempPoint, OCCUPIED);
			//turn right
			mover.rotateAngle(TURN_RIGHT, DEFAULT_SPEED);
			facing = findNewFacing(REL_RIGHT);
			//now facing block
			haveBlock = obtainFrontBlock();
			currPoint = adjacentPoint(currPoint, REL_FRONT);
		}
		else if (isBlock(REL_LEFT) )
		{
			tempPoint = adjacentPoint(currPoint, REL_LEFT);
			gridMap.setFlag(tempPoint, OCCUPIED);
			//turn left
			mover.rotateAngle(TURN_LEFT, DEFAULT_SPEED);
			facing = findNewFacing(REL_LEFT);
			//now facing block
			haveBlock = obtainFrontBlock();
			currPoint = adjacentPoint(currPoint, REL_FRONT);
		}	
		else
		{
			mover.moveTillPoint(DEFAULT_SPEED);
			mover.moveOffCross(DEFAULT_SPEED);
			currPoint = adjacentPoint(currPoint, REL_FRONT);
		}
	}
	else
	{
		//scan right point and set seen flag
		irInMm.rght = rightIr.getDist();
		tempPoint = adjacentPoint(currPoint, REL_RIGHT);
		gridMap.setFlag(tempPoint, SEEN);
		
		//scan left point and set seen flag
		irInMm.lft = leftIr.getDist();
		tempPoint = adjacentPoint(currPoint, REL_LEFT);
		gridMap.setFlag(tempPoint, SEEN);
		
		if (isBlock(REL_RIGHT) )
		{
			tempPoint = adjacentPoint(currPoint, REL_RIGHT);
			gridMap.setFlag(tempPoint, OCCUPIED);
			//turn right
			mover.rotateAngle(TURN_RIGHT, DEFAULT_SPEED);
			facing = findNewFacing(REL_RIGHT);
			//now facing block
			haveBlock = obtainFrontBlock();
			currPoint = adjacentPoint(currPoint, REL_FRONT);
		}
		else if (isBlock(REL_LEFT) )
		{
			tempPoint = adjacentPoint(currPoint, REL_LEFT);
			gridMap.setFlag(tempPoint, OCCUPIED);
			//turn left
			mover.rotateAngle(TURN_LEFT, DEFAULT_SPEED);
			facing = findNewFacing(REL_LEFT);
			//now facing block
			haveBlock = obtainFrontBlock();
			currPoint = adjacentPoint(currPoint, REL_FRONT);
		}
		else
		{
			//turn left
			mover.rotateAngle(TURN_LEFT, DEFAULT_SPEED);
			facing = findNewFacing(REL_LEFT);
			//move to (GRID_MAX_X - 1, 1)
			mover.moveTillPoint(DEFAULT_SPEED);
			mover.moveOffCross(DEFAULT_SPEED);
			currPoint = adjacentPoint(currPoint, REL_FRONT);
		}	
	}
}

void traverseEast()
{
	gridMap.setFlag(currPoint, VISITED);
	
	//scan front point and set seen flag
	irInMm.frnt = frontIr.getDist();
	tempPoint = adjacentPoint(currPoint, REL_FRONT);
	gridMap.setFlag(tempPoint, SEEN);
	
	//scan left point and set seen flag
	irInMm.lft = leftIr.getDist();
	tempPoint = adjacentPoint(currPoint, REL_LEFT);
	gridMap.setFlag(tempPoint, SEEN);
	
	if (isBlock(REL_FRONT) )
	{
		tempPoint = adjacentPoint(currPoint, REL_FRONT);
		gridMap.setFlag(tempPoint, OCCUPIED);
		haveBlock = obtainFrontBlock();
		currPoint = adjacentPoint(currPoint, REL_FRONT);
	}
	else if (isBlock(REL_LEFT) )
	{
		tempPoint = adjacentPoint(currPoint, REL_LEFT);
		gridMap.setFlag(tempPoint, OCCUPIED);
		//turn left
		mover.rotateAngle(TURN_LEFT, DEFAULT_SPEED);
		facing = findNewFacing(REL_LEFT);
		//now facing block
		haveBlock = obtainFrontBlock();
		currPoint = adjacentPoint(currPoint, REL_FRONT);
	}	
	else
	{
		mover.rotateAngle(TURN_LEFT, DEFAULT_SPEED);
		facing = findNewFacing(REL_LEFT);
		//move to (GRID_MAX_X - 1, 1)
		mover.moveTillPoint(DEFAULT_SPEED);
		mover.moveOffCross(DEFAULT_SPEED);
		currPoint = adjacentPoint(currPoint, REL_FRONT);
	}
}

void traverseWest()
{
	gridMap.setFlag(currPoint, VISITED);
	
	//scan front point and set seen flag
	irInMm.frnt = frontIr.getDist();
	tempPoint = adjacentPoint(currPoint, REL_FRONT);
	gridMap.setFlag(tempPoint, SEEN);
	
	//scan left point and set seen flag
	irInMm.lft = leftIr.getDist();
	tempPoint = adjacentPoint(currPoint, REL_LEFT);
	gridMap.setFlag(tempPoint, SEEN);
	
	if (isBlock(REL_FRONT) )
	{
		tempPoint = adjacentPoint(currPoint, REL_FRONT);
		gridMap.setFlag(tempPoint, OCCUPIED);
		haveBlock = obtainFrontBlock();
		currPoint = adjacentPoint(currPoint, REL_FRONT);
	}
	else if (isBlock(REL_LEFT) )
	{
		tempPoint = adjacentPoint(currPoint, REL_LEFT);
		gridMap.setFlag(tempPoint, OCCUPIED);
		//turn left
		mover.rotateAngle(TURN_LEFT, DEFAULT_SPEED);
		facing = findNewFacing(REL_LEFT);
		//now facing block
		haveBlock = obtainFrontBlock();
		currPoint = adjacentPoint(currPoint, REL_FRONT);
	}	
	else
	{
		mover.rotateAngle(TURN_LEFT, DEFAULT_SPEED);
		facing = findNewFacing(REL_LEFT);
		//move to (GRID_MAX_X - 1, 1)
		mover.moveTillPoint(DEFAULT_SPEED);
		mover.moveOffCross(DEFAULT_SPEED);
		currPoint = adjacentPoint(currPoint, REL_FRONT);
	}
}

void findBlock()
{
	//start facing west
	facing = DIR_WEST;
	turn = TURN_RIGHT;

	initTraverse();
if (!haveBlock)
{
secondTraverse();
	while (!haveBlock)
	{
		if (facing == DIR_NORTH || facing == DIR_SOUTH) // Travel till end
			traverseLong();
		else if (facing == DIR_EAST)
		{
			traverseEast();
		}
		else if (facing == DIR_WEST)
			traverseWest();
		//else EXPLODE!
	}
}
	dropOff();
	delay(100000);
}

/*
void testSensors()
{
	unsigned char blockDir = 99;
	claw.shut();	
	irInMm.frnt = frontIr.getDist();
	irInMm.lft = leftIr.getDist();
	irInMm.rght = rightIr.getDist();
	while (blockDir != REL_FRONT && blockDir != REL_LEFT && blockDir != REL_RIGHT)
	{
		if (irInMm.frnt > BLOCK_DIST - BLOCK_TOLERANCE)
			blockDir = REL_FRONT;
		else if (irInMm.lft > BLOCK_DIST - BLOCK_TOLERANCE)
			blockDir = REL_LEFT;
		else if (irInMm.rght > BLOCK_DIST - BLOCK_TOLERANCE)
			blockDir = REL_RIGHT;
		else
		{
			irInMm.frnt = frontIr.getDist();
			irInMm.lft = leftIr.getDist();
			irInMm.rght = rightIr.getDist();
		}
	}
	switch (blockDir)
	{
		case REL_LEFT:
			mover.rotateAngle(TURN_LEFT, DEFAULT_SPEED);
			break;
		case REL_RIGHT:
			mover.rotateAngle(TURN_RIGHT, DEFAULT_SPEED);
			break;
	}
	claw.open();
	irInMm.frnt = frontIr.getDist();
	while (irInMm.frnt > BLOCK_STOP)
	{
		mover.moveForward(DEFAULT_SPEED);
		irInMm.frnt = frontIr.getDist();
	}
	motors.stop();
	claw.close();
	mover.moveTillPoint(DEFAULT_SPEED);
	mover.moveOffCross(DEFAULT_SPEED);
	motors.stop();
	
	claw.open();

	delay(100000);
}*/

void lineFollowDemoSetup()
{
	
  ls.calibrate();
	delay(3000);
	ls.calibrate();
}

void setup()
{  
	Serial.begin(9600);	
	pinMode(LED_PIN, OUTPUT);
	claw.setup();
	motors.setup();
	digitalWrite(LED_PIN,HIGH);
	pinMode(PUSH_PIN, INPUT);
	digitalWrite(PUSH_PIN, HIGH);
	while(digitalRead(PUSH_PIN) == HIGH)
	{
		delay(500); 
	}
	digitalWrite(LED_PIN, LOW);
	lineFollowDemoSetup();
}

void loop()
{
	findBlock();
}

//Note, this is NOT the best way to do it.  Just a quick example of how to use the class.
void lineFollowDemo()
{
	LineSensor_ColourValues leftWhite[8] = {NUL,NUL,WHT,NUL,NUL,NUL,NUL,NUL};
	LineSensor_ColourValues rightWhite[8] = {NUL,NUL,NUL,NUL,NUL,WHT,NUL,NUL};
	LineSensor_ColourValues allBlack[8] = {BLK,BLK,BLK,BLK,BLK,BLK,BLK,BLK};
	ls.readCalibrated();
	if(ls.see(leftWhite))
	{
		motors.left(127);
		motors.right(0);
	}
	if(ls.see(rightWhite))
	{
		motors.right(127);
		motors.left(0);
	}
	if(ls.see(allBlack))
	motors.stop();
}

//void setup(){}
//void loop(){}
