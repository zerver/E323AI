#ifndef CUNIT_H
#define CUNIT_H

#include <map>

#include "CE323AI.h"
#include "ARegistrar.h"

class CUnit: public ARegistrar {
	public:
		CUnit(): ARegistrar() {}
		CUnit(AIClasses *ai, int uid, int builder): ARegistrar(uid);
		~CUnit(){}

		const UnitDef *def;
		const UnitType *type;
		int   builder;

		/* Remove the unit from everywhere registered */
		void remove();

		/* Overloaded */
		void remove(ARegistrar &reg);

		/* Reset this object */
		void reset(int uid, int builder);

		/* Attack a unit */
		bool attack(int target);

		/* Move a unit forward by dist */
		bool moveForward(float dist);

		/* Move random */
		bool moveRandom(float radius, bool enqueue = false);

		/* Move unit with id uid to position pos */
		bool move(float3 &pos, bool enqueue = false);

		/* Set a unit (e.g. mmaker) on or off */
		bool setOnOff(bool on);

		/* Let unit with id uid build a unit with a certain facing (NORTH,
		 * SOUTH, EAST, WEST) at position pos 
		 */
		bool build(UnitType *toBuild, float3 &pos);

		/* Build a unit in a certain factory */
		bool factoryBuild(UnitType *toBuild);

		/* Repair (or assist) a certain unit */
		bool repair(int target);

		/* Guard a certain unit */
		bool guard(int target, bool enqueue = true);

		/* Stop doing what you did */
		bool stop();

		/* Wait with what you are doing */
		bool wait();

		/* Get best facing */
		facing getBestFacing(float3 &pos);

		/* Get quadrant */
		quadrant getQuadrant(float3 &pos);

	private:
		AIClasses *ai;
		char buf[1024];

		Command createPosCommand(int cmd, float3 pos, float radius = -1.0f, facing f = NONE);
		Command createTargetCommand(int cmd, int target);
};

#endif