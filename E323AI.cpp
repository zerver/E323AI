#include "E323AI.h"

CE323AI::CE323AI() {
}

CE323AI::~CE323AI() {
	/* close the logfile */
	ai->logger->close();

	delete ai->metalMap;
	delete ai->unitTable;
	delete ai->metaCmds;
	delete ai->eco;
	delete ai->logger;
	delete ai->tasks;
	delete ai->threatMap;
	delete ai->pf;
	delete ai->intel;
	delete ai->military;
	delete ai;
}

void CE323AI::InitAI(IGlobalAICallback* callback, int team) {
	ai          = new AIClasses();
	ai->call    = callback->GetAICallback();
	ai->cheat   = callback->GetCheatInterface();
	unitCreated = 0;

	/* Retrieve mapname, time and team info for the log file */
	std::string mapname = std::string(ai->call->GetMapName());
	mapname.resize(mapname.size() - 4);

	time_t now1;
	time(&now1);
	struct tm* now2 = localtime(&now1);

	std::sprintf(buf, "%s%s-%2.2d-%2.2d-%4.4d-%2.2d%2.2d-team(%d).log", 
		std::string(LOG_FOLDER).c_str(), mapname.c_str(), now2->tm_mon + 1, 
		now2->tm_mday, now2->tm_year + 1900, now2->tm_hour, now2->tm_min, team);

	printf("logfile @ %s\n", buf);

	ai->logger		= new std::ofstream(buf, std::ios::app);
	LOGN("v" << AI_VERSION);

	ai->metalMap	= new CMetalMap(ai);
	ai->unitTable	= new CUnitTable(ai);
	ai->metaCmds	= new CMetaCommands(ai);
	ai->eco     	= new CEconomy(ai);
	ai->tasks     	= new CTaskPlan(ai);
	ai->threatMap   = new CThreatMap(ai);
	ai->pf          = new CPathfinder(ai, ai->threatMap->W, ai->threatMap->H, ai->threatMap->REAL);
	ai->intel       = new CIntel(ai);
	ai->military    = new CMilitary(ai);

	ai->call->SendTextMsg("*** " AI_NOTES " ***", 0);
	ai->call->SendTextMsg("*** " AI_CREDITS " ***", 0);
	LOG("\n\n\nBEGIN\n\n\n");
}


/************************
 * Unit related callins *
 ************************/

/* Called when units are spawned in a factory or when game starts */
void CE323AI::UnitCreated(int unit) {
	const UnitDef *ud = ai->call->GetUnitDef(unit);
	sprintf(buf, "[CE323AI::UnitCreated]\t %s(%d) created", ud->humanName.c_str(), unit);
	LOGN(buf);
	UnitType *ut = UT(ud->id);
	unsigned int c = ut->cats;

	if (unitCreated == 1 && ud->isCommander) {
		ai->eco->init(unit);
		ai->military->init(unit);
	}

	if (c&MEXTRACTOR) {
		ai->metalMap->taken[unit] = ai->call->GetUnitPos(unit);
	}
	else if (c&MMAKER) {
		ai->eco->gameMetalMakers[unit] = true;
	}

	unitCreated++;
}

/* Called when units are finished in a factory and able to move */
void CE323AI::UnitFinished(int unit) {
	const UnitDef *ud = ai->call->GetUnitDef(unit);
	sprintf(buf, "[CE323AI::UnitFinished]\t %s(%d) finished", ud->humanName.c_str(), unit);
	LOGN(buf);
	UnitType* ut = UT(ud->id);
	unsigned int c = ut->cats;

	if (!(c&ATTACKER) || c&COMMANDER) {
		if (c&FACTORY) {
			ai->eco->gameFactories[unit]      = true;
			ai->eco->gameIdle[unit]           = ut;
			ai->tasks->updateBuildPlans(unit);
		}

		if (c&BUILDER && c&MOBILE) {
			ai->eco->gameBuilders[unit]  = ut;
			ai->metaCmds->moveForward(unit, -70.0f);
		}

		if (c&MEXTRACTOR || c&MMAKER || c&MSTORAGE) {
			ai->eco->gameMetal[unit]     = ut;
			ai->tasks->updateBuildPlans(unit);
			if (c&MEXTRACTOR) {
				ai->metalMap->taken[unit] = ai->call->GetUnitPos(unit);
			}
		}

		if (c&EMAKER || c& ESTORAGE) {
			ai->eco->gameEnergy[unit]    = ut;
			ai->tasks->updateBuildPlans(unit);
		}
	}
	else {
		if (c&MOBILE) {
			ai->metaCmds->moveForward(unit, 300.0f);
			if (c&SCOUT) {
				ai->military->scouts[unit] = unit;
			}
			else {
				ai->military->addToGroup(unit);
			}
		}
	}

	ai->unitTable->gameAllUnits[unit] = ut;
}

/* Called on a destroyed unit, wanna take revenge? :P */
void CE323AI::UnitDestroyed(int unit, int attacker) {
	const UnitDef *ud = ai->call->GetUnitDef(unit);
	sprintf(buf, "[CE323AI::UnitDestroyed]\t %s(%d) destroyed", ud->humanName.c_str(), unit);
	LOGN(buf);
	UnitType* ut = UT(ud->id);
	unsigned int c = ut->cats;

	if (!(c&ATTACKER) || c&COMMANDER) {
		if (c&FACTORY) {
			ai->eco->gameFactories.erase(unit);
			ai->eco->gameFactoriesBuilding.erase(unit);
		}

		if (c&BUILDER && c&MOBILE) {
			ai->eco->gameBuilders.erase(unit);
			ai->eco->removeMyGuards(unit);
			ai->metalMap->taken.erase(unit);
		}

		if (c&MEXTRACTOR || c&MMAKER || c&MSTORAGE) {
			ai->eco->gameMetal.erase(unit);
			if (c&MEXTRACTOR) {
				ai->metalMap->removeFromTaken(unit);
			}
			else if (c&MMAKER) {
				ai->eco->gameMetalMakers.erase(unit);
			}
		}

		if (c&EMAKER || c& ESTORAGE) {
			ai->eco->gameEnergy.erase(unit);
		}
		ai->eco->gameIdle.erase(unit);
		ai->eco->gameGuarding.erase(unit);
	}
	else {
		if (c&SCOUT) {
			ai->military->scouts.erase(unit);
		}
		else {
			ai->military->removeFromGroup(unit);
		}
	}

	ai->unitTable->gameAllUnits.erase(unit);
	ai->tasks->buildplans.erase(unit);
}

/* Called when unit is idle */
void CE323AI::UnitIdle(int unit) {
	const UnitDef *ud = ai->call->GetUnitDef(unit);
	sprintf(buf, "[CE323AI::UnitIdle]\t %s(%d) idling", ud->humanName.c_str(), unit);
	LOGN(buf);
	UnitType* ut = UT(ud->id);
	unsigned int c = ut->cats;

	if (c&BUILDER)
		ai->eco->gameIdle[unit] = ut;
}

/* Called when unit is damaged */
void CE323AI::UnitDamaged(int damaged, int attacker, float damage, float3 dir) {
}

/* Called on move fail e.g. can't reach point */
void CE323AI::UnitMoveFailed(int unit) {
	ai->metaCmds->moveRandom(unit, 100.0f);
}



/***********************
 * LOS related callins *
 ***********************/

void CE323AI::EnemyEnterLOS(int enemy) {
}

void CE323AI::EnemyLeaveLOS(int enemy) {
}

void CE323AI::EnemyEnterRadar(int enemy) {
}

void CE323AI::EnemyLeaveRadar(int enemy) {
}

void CE323AI::EnemyDestroyed(int enemy, int attacker) {
}

void CE323AI::EnemyDamaged(int damaged, int attacker, float damage, float3 dir) {
}


/****************
 * Misc callins *
 ****************/

void CE323AI::GotChatMsg(const char* msg, int player) {
}

int CE323AI::HandleEvent(int msg, const void* data) {
	return 0;
}


/* Update AI per logical frame = 1/30 sec on gamespeed 1.0 */
void CE323AI::Update() {
	int frame = ai->call->GetCurrentFrame();

	if (frame > 1 && frame % 300 == 0) {
		float3 start, goal;
		start.x = 100.0f;
		start.z = 100.0f;
		start.y = ai->call->GetElevation(start.x, start.z);
		goal.x = ai->call->GetMapWidth()*8-rng.RandFloat()*500-100.0f;
		goal.z = ai->call->GetMapHeight()*8-rng.RandFloat()*500-100.0f;
		goal.y = ai->call->GetElevation(goal.x, goal.z)+10;
		std::vector<float3> path;
		ai->pf->path(start, goal, path);
		for (unsigned i = 1; i < path.size(); i++) {
			ai->call->CreateLineFigure(path[i-1], path[i], 10, 0, 300, 1);
			ai->call->CreateLineFigure(path[i-1], path[i], 10, 0, 300, 1);
		}
	}

	/* Rotate through the different update events to distribute computations */
	switch(frame % 6) {
		case 0: /* update threatmap */
			ai->threatMap->update(frame);
		break;

		case 1: /* update pathfinder with threatmap */
			ai->pf->update(ai->threatMap->map);
		break;

		case 2: /* update enemy intel */
			ai->intel->update(frame);
		break;

		case 3: /* update military */
			ai->military->update(frame);
		break;

		case 4: /* update incomes */
			ai->eco->updateIncomes(frame);
		break;

		case 5: /* update economy */
			ai->eco->update(frame);
		break;
	}
}
