#pragma once

#include <vector>

// Calcule le rebond d'un vecteur vitesse sur une surface
// v: vitesse incidente (m/s)
// n: vecteur normal unitaire de la surface (pointant vers l'extérieur)
// e: coefficient de restitution (0 = parfaitement inélastique, 1 = parfaitement élastique)
// deltaT: pas de temps (s) - pour cohérence avec l'API, bien que le rebond soit instantané
// Retourne la nouvelle vitesse après rebond
std::vector<float> rebond(const std::vector<float>& v, const std::vector<float>& n, float e, float deltaT);

// Applique la gravité à un vecteur vitesse
// v: vitesse actuelle (m/s)
// deltaT: pas de temps (s)
// Retourne la nouvelle vitesse après application de la gravité
std::vector<double> gravite(const std::vector<double>& v, double deltaT);