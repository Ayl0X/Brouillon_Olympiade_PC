#include "RebondEtGravite/realiste/mainRR.h"
#include "RebondEtGravite/simpliste/mainSR.h"

enum mains {
	EmainRR,
	EmainSR
};


int main() {
	mains mains = EmainSR;

	switch (mains)
	{
	case mains::EmainRR:
		return mainRR();
	case mains::EmainSR:
		return mainSR();
	default:
		break;
	}
}