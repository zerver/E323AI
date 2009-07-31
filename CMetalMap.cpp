#include "MetalMap.h"

bool sortme(const CMetalMap::MSpot &a, const CMetalMap::MSpot &b) {return a.dist < b.dist;}

CMetalMap::CMetalMap(AIClasses *ai) {
	this->ai = ai;
	threshold = 64;

	X = ai->call->GetMapWidth() / 2;
	Z = ai->call->GetMapHeight() / 2;
	N = 100;

	callMap = ai->call->GetMetalMap();
	radius = (int) round( (ai->call->GetExtractorRadius()) / SCALAR);
	diameter = radius*2;
	squareRadius=radius*radius;

	map = new unsigned char[X*Z];
	coverage = new unsigned int[diameter*diameter];
	bestCoverage = new unsigned int[diameter*diameter];

	for (int i = 0; i < X*Z; i++)
		map[i] = callMap[i];
	delete[] callMap;

	findBestSpots();
}

CMetalMap::~CMetalMap() {
	delete[] map;
	delete[] coverage;
	delete[] bestCoverage;
}

CMetalMap::remove(ARegistrar &unit) {
	if (unit.type->cats&BUILDER)
		taken.erase(unit.key);
	else if (unit.type->cats&MEXTRACTOR)
		removeFromTaken(unit.key);
}

CMetalMap::addUnit(ARegistrar &unit) {
	unit.reg(*this);
}

void CMetalMap::findBestSpots() {
	bool metalSpots;
	int i, x, z, k, bestX, bestZ, highestSaturation, saturation;
	int counter = 0;

	while (true) {
		counter++;
		metalSpots = false;
		highestSaturation = 0;
		k = 0;
		
		for (z = radius; z < Z-radius; z+=2) {
			for (x = radius; x < X-radius; x+=2) {
				if (map[z*X+x] > threshold) {
					metalSpots = true;
					saturation = getSaturation(x, z, &k);
					if (saturation > highestSaturation) {
						highestSaturation = saturation;
						for (i = 0; i < k; i++)
							bestCoverage[i] = coverage[i];
						bestX = x;
						bestZ = z;
					}
				}
			}
		}
		
		if (!metalSpots) break;

		float3 pos = float3(bestX*SCALAR,ai->call->GetElevation(bestX*SCALAR,bestZ*SCALAR), bestZ*SCALAR);
		spots.push_back(MSpot(spots.size(), pos, highestSaturation));

		if (counter >= N) {
			LOGS("Speedmetal infects my skills... I won't play");
			break;
		}

		for (i = 0; i < k; i++)
			map[bestCoverage[i]] = 0;
	}

	std::random_shuffle(spots.begin(), spots.end());
}


int CMetalMap::getSaturation(int x, int z, int *k) {
	int index, i, j, sum;
	*k = sum = 0;
	float q,d;
	
	for (i = z-radius; i < z+radius; i++) {
		for (j = x-radius; j < x+radius; j+=2) {
			d = squareDist(x, z, j, i);
			if (d > squareRadius) continue;
			index = i*X+j;
			q = radius - sqrt(d)/radius;
			sum += (int) (q * (map[index]+map[index+1]));
			coverage[(*k)++] = index;
			coverage[(*k)++] = index+1;
		}
	}
	return sum;	
}


int CMetalMap::squareDist(int x, int z, int j, int i) {
	x -= j;
	z -= i;
	return x*x + z*z;
}

bool CMetalMap::buildMex(CUnit *builder, UnitType *mex) {
	if (taken.size() == spots.size()) return false;
	std::vector<MSpot> sorted;
	float3 pos = ai->call->GetUnitPos(builder.key);
	float pathLength;
	MSpot *ms, *bestMs;
	
	for (unsigned i = 0; i < spots.size(); i++) {
		ms = &(spots[i]);
		
		std::map<int,float3>::iterator j;
		bool skip = false;
		for (j = taken.begin(); j != taken.end(); j++) {
			float3 diff = (j->second - ms->f);
			if (diff.Length2D() <= ai->call->GetExtractorRadius()*1.5f)
				skip = true;
		}
		if (skip) continue;
		const UnitDef *ud = builder->def;
		pathLength = (pos - ms->f).Length2D() - ms->c*EPSILON;
		ms->dist = pathLength;
		sorted.push_back(*ms);
	}

	std::partial_sort(sorted.begin(), sorted.begin()+5, sorted.end(), sortme);

	float lowestThreat = MAX_FLOAT;
	for (unsigned i = 0; i < sorted.size(); i++) {
		float threat = ai->threatMap->getThreat(sorted[i].f);
		if (threat < lowestThreat) {
			bestMs = &sorted[i];
			lowestThreat = threat;
			if (lowestThreat < 1.0f) break;
		}
	}
		
	if (lowestThreat > 1.0f) return false;

	ai->metalMap->taken[builder.key] = bestMs->f;
	builder->build(mex, bestMs->f);
	return true;
}

void CMetalMap::removeFromTaken(int mex) {
	float3 pos = ai->call->GetUnitPos(mex);
	int erase = -1;
	std::map<int,float3>::iterator i;
	for (i = taken.begin(); i != taken.end(); i++) {
		float3 diff = pos - i->second;
		if (diff.Length2D() <= ai->call->GetExtractorRadius()*1.2f)
			erase = i->first;
	}
	assert(erase != -1);
	taken.erase(erase);
}