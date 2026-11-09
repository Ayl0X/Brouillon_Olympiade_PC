#pragma once

#include "..\..\Math.h"
#include "..\..\Constantes.h"
#include <vector>

// Calcule le rebond d'un vecteur vitesse sur une surface
// v: vitesse incidente (m/s)
// n: vecteur normal unitaire de la surface (pointant vers l'extérieur)
// e: coefficient de restitution (0 = parfaitement inélastique, 1 = parfaitement élastique)
// deltaT: pas de temps (s) - pour cohérence avec l'API, bien que le rebond soit instantané
// Retourne la nouvelle vitesse après rebond
Vec3 Srebond(const Vec3& v, const Vec3& n, float deltaT);

// Collision avec le sol (y = 0). A appeler apres Update(deltaT) :
// - detection : la balle a-t-elle traverse le sol ?
// - reponse   : on la replace au niveau du sol et on fait rebondir sa vitesse
//   avec la formule de Rebond.cpp : v' = v - (1 + e) * (v.n) * n
void Scollision(particule& p);

// Applique la gravité à un vecteur vitesse
// v: vitesse actuelle (m/s)
// deltaT: pas de temps (s)
// Retourne la nouvelle vitesse après application de la gravité
Vec3 Sgravite(const Vec3& v, float deltaT);