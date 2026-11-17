#version 330 core

out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;

#define NB_LUMIERES 2

uniform vec3  lightPos[NB_LUMIERES];
uniform vec3  lightColor[NB_LUMIERES];   // noir = lumiere eteinte
uniform vec3  viewPos;
uniform vec3  objectColor;
uniform float specularStrength;          // force du reflet (liege : tres faible)
uniform float shininess;                 // finesse du reflet (liege : tres large)

// Ombre de la balle sur le support (test rayon / sphere)
uniform int   receiveShadow;
uniform vec3  ballCenter;
uniform float ballRadius;

bool blockedByBall(vec3 from, vec3 to)
{
    vec3 d = to - from;
    float len = length(d);
    d /= len;
    vec3 oc = from - ballCenter;
    float b = dot(oc, d);
    float c = dot(oc, oc) - ballRadius * ballRadius;
    float disc = b * b - c;
    if (disc < 0.0) return false;
    float t = -b - sqrt(disc);
    return (t > 0.02 && t < len);
}

void main()
{
    // 1. Ambiant
    float ambientStrength = 0.1;
    vec3 ambient = ambientStrength * vec3(1.0);

    vec3 norm    = normalize(Normal);
    vec3 viewDir = normalize(viewPos - FragPos);

    vec3 sum = vec3(0.0);

    for (int i = 0; i < NB_LUMIERES; i++) {
        vec3 lightDir = normalize(lightPos[i] - FragPos);

        // 2. Diffus
        float diff = max(dot(norm, lightDir), 0.0);
        vec3 diffuse = diff * lightColor[i];

        // 3. Speculaire (Phong classique)
        vec3 reflectDir = reflect(-lightDir, norm);
        float spec = pow(max(dot(viewDir, reflectDir), 0.0), shininess);
        vec3 specular = specularStrength * spec * lightColor[i];

        // Attenuation avec la distance (distances en cm)
        float dist = length(lightPos[i] - FragPos);
        float att = 1.0 / (1.0 + 0.07 * dist + 0.017 * dist * dist);

        float shadow = 1.0;
        if (receiveShadow == 1 && blockedByBall(FragPos, lightPos[i])) shadow = 0.0;

        sum += (diffuse + specular) * att * shadow;
    }

    vec3 result = (ambient + sum) * objectColor;
    FragColor = vec4(result, 1.0);
}
