
#version 330 core

out vec4 FragColor;

in vec3 FragPos;  
in vec3 Normal;  
  
uniform vec3 lightPos; 
uniform vec3 viewPos; 
uniform vec3 lightColor;
uniform vec3 objectColor;

void main()
{
    // 1. Ambiant
    float ambientStrength = 0.1;
    vec3 ambient = ambientStrength * lightColor;
  	
    // 2. Diffus
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;
    
    // 3. Spéculaire (Phong classique)
    float specularStrength = 0.5;
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
    vec3 specular = specularStrength * spec * lightColor;
            
    // Atténuation avec la distance
    float dist = length(lightPos - FragPos);
    float att = 1.0 / (1.0 + 0.07 * dist + 0.017 * dist * dist);

    vec3 result = (ambient + (diffuse + specular) * att) * objectColor;
    FragColor = vec4(result, 1.0);
}
