#include "RebondEtGravite/realiste/mainRC.h"
#include "RebondEtGravite/simpliste/mainFISR.h"
#include "Lumiere/Realiste/pathTracer.h"
#include "Lumiere/simpliste/phong.h"
#include "MecaniqueDesFluides/simpliste/MecaniqueDesFluidesCoef.h"
#include "MecaniqueDesFluides/realiste/VisualisationFluides.h"
#include "MecaniqueDesFluides/tests/testVisu.h"
#include "MecaniqueDesFluides/tests/testSimulation.h"

// FISR   -> Fast Inverse Square Root
// RC     -> Rebond Coefficient
// PT     -> Path Tracer
// MDF    -> Mecanique Des Fluides
// TMDF   -> Test Mecanique Des Fluides (utilise pour les tests unitaires de la simulation de fluides, n'est pas utilise dans la version finale)

enum mains {
	mainFISR,
	mainRC,
	mainPT,
	mainPhong,
	mainMDFCoef,
	mainMDF,

	// n'est pas utilise dans la version finale, mais sert pour les tests unitaires de la simulation de fluides
	mainTMDF_Champ,
	mainTMDF_Visu,
	mainTMDF_Simulation,
};


int main() {
	mains mains = mainMDFCoef;

	switch (mains)
	{
	case mains::mainFISR:
		return FastInverseSquareRoot();
	case mains::mainRC:
		return rebondCoef();
	case mains::mainPT:
		return pathTracer();
	case mains::mainPhong:
		return phong();
	case mains::mainMDFCoef:
		return mecaniqueDesFluidesCoef();
	case mains::mainMDF:
		return visualisationFluides();

	// Fichiers de test
	case mains::mainTMDF_Visu:
		return testVisu();
	case mains::mainTMDF_Simulation:
		return testSimulation();
	default:
		break;
	}
}