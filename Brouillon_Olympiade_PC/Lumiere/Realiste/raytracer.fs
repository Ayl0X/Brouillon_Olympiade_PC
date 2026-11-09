
#version 330

out vec4 FragColor;

uniform vec2 uResolution;

// Constantes
const float PI = 3.14159265359;

// Camera
uniform vec3 uCameraPosition;
uniform vec3 uCameraForward;
uniform vec3 uCameraRight;
uniform vec3 uCameraUp;
uniform float uFov;

// --------------------------------------------------
// Structures
// --------------------------------------------------

struct Ray {
    vec3 origin;
    vec3 direction;
};

struct Material {
    vec3 albedo;
    float roughness;
    float metallic;
    float ior;
};

struct Hit {
    float distance;
    vec3 position;
    vec3 normal;
    bool frontFace;
    Material material;
};

struct Sphere {
    vec3 center;
    float radius;
    Material material;
};

struct Plane {
    vec3 point;
    vec3 normal;
    Material material;
};

struct PointLight {
    vec3 position;
    vec3 intensity;
};


// --------------------------------------------------
// Ray / Sphere intersection
// --------------------------------------------------

bool intersectSphere(
    Ray ray,
    Sphere sphere,
    out Hit hit
) {
    vec3 oc = ray.origin - sphere.center;

    float a = dot(ray.direction, ray.direction);
    float b = 2.0 * dot(oc, ray.direction);
    float c = dot(oc, oc) - sphere.radius * sphere.radius;

    float discriminant = b * b - 4.0 * a * c;

    if (discriminant < 0.0)
        return false;

    float sqrtD = sqrt(discriminant);

    float t1 = (-b - sqrtD) / (2.0 * a);
    float t2 = (-b + sqrtD) / (2.0 * a);

    float t = t1 > 0.001 ? t1 : t2;

    if (t <= 0.001)
        return false;

    hit.material = sphere.material;

    hit.distance = t;
    hit.position = ray.origin + t * ray.direction;

    vec3 outwardNormal =
        normalize(hit.position - sphere.center);

    hit.frontFace =
        dot(ray.direction, outwardNormal) < 0.0;

    hit.normal = hit.frontFace
        ? outwardNormal
        : -outwardNormal;

    return true;
}

// --------------------------------------------------
// Ray / Plane intersection
// --------------------------------------------------

bool intersectPlane(
    Ray ray,
    Plane plane,
    out Hit hit
) {
    vec3 outwardNormal = normalize(plane.normal);

    float denominator =
        dot(ray.direction, outwardNormal);

    if (abs(denominator) < 0.000001)
        return false;

    float t = dot(
        plane.point - ray.origin,
        outwardNormal
    ) / denominator;

    if (t <= 0.001)
        return false;

    hit.material = plane.material;

    hit.distance = t;
    hit.position = ray.origin + t * ray.direction;

    hit.frontFace =
        dot(ray.direction, outwardNormal) < 0.0;

    hit.normal = hit.frontFace
        ? outwardNormal
        : -outwardNormal;

    return true;
}

// --------------------------------------------------
// Scene intersection
// --------------------------------------------------

bool closestHit(
    Ray ray,
    Sphere sphere,
    Plane plane,
    out Hit closest
) {
    Hit temp;
    bool found = false;

    closest.distance = 1e30;

    if (intersectSphere(ray, sphere, temp)) {
        closest = temp;
        found = true;
    }

    if (intersectPlane(ray, plane, temp)) {
        if (temp.distance < closest.distance) {
            closest = temp;
            found = true;
        }
    }

    return found;
}

// --------------------------------------------------
// Camera ray generation
// --------------------------------------------------

Ray generateRay(vec2 uv) {
    float aspectRatio = uResolution.x / uResolution.y;

    float scale = tan(radians(uFov) * 0.5);

    vec2 screen = (uv * 2.0 - 1.0);

    screen.x *= aspectRatio;
    screen *= scale;

    Ray ray;

    ray.origin = uCameraPosition;

    ray.direction = normalize(
        uCameraForward +
        screen.x * uCameraRight +
        screen.y * uCameraUp
    );

    return ray;
}

// --------------------------------------------------
// Microfacet BRDF
// --------------------------------------------------

// Normal Distribution Function (GGX)
float distributionGGX(
    vec3 N,
    vec3 H,
    float roughness
) {
    float a = roughness * roughness;
    float a2 = a * a;

    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float denominator =
        NdotH2 * (a2 - 1.0) + 1.0;

    return a2 /
        max(PI * denominator * denominator, 0.000001);
}

vec3 calculateF0(Material material) {
    float f0 = pow(
        (material.ior - 1.0) /
        (material.ior + 1.0),
        2.0
    );

    return mix(
        vec3(f0),
        material.albedo,
        material.metallic
    );
}


// Fresnel - Schlick approximation
vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) *
        pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// Geometry - Schlick GGX
float geometrySchlickGGX(float NdotV, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;

    return NdotV /
        (NdotV * (1.0 - k) + k);
}

// Geometry - Smith
float geometrySmith(
    vec3 N,
    vec3 V,
    vec3 L,
    float roughness
) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);

    float ggxV = geometrySchlickGGX(NdotV, roughness);
    float ggxL = geometrySchlickGGX(NdotL, roughness);

    return ggxV * ggxL;
}

// --------------------------------------------------
// Diffuse BRDF
// --------------------------------------------------

vec3 calculateDiffuse(Material material, vec3 F) {
    vec3 kd = (1.0 - F) * (1.0 - material.metallic);

    return kd * material.albedo / PI;
}

// --------------------------------------------------
// Specular BRDF - Cook Torrance
// --------------------------------------------------

vec3 calculateSpecular(
    Material material,
    vec3 N,
    vec3 V,
    vec3 L
) {
    vec3 H = normalize(V + L);

    float roughness = clamp(material.roughness, 0.045, 1.0);

    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float NdotH = max(dot(N, H), 0.0);
    float VdotH = max(dot(V, H), 0.0);

    if (NdotV <= 0.0 || NdotL <= 0.0)
        return vec3(0.0);

    vec3 F0 = calculateF0(material);
    vec3 F = fresnelSchlick(VdotH, F0);

    float D = distributionGGX(N, H, roughness);
    float G = geometrySmith(N, V, L, roughness);

    return (D * G * F) /
        max(4.0 * NdotV * NdotL, 0.000001);
}

// --------------------------------------------------
// Shadow ray
// --------------------------------------------------

bool isOccluded(
    Ray ray,
    Sphere sphere,
    Plane plane,
    float maxDistance
) {
    Hit hit;

    if (intersectSphere(ray, sphere, hit)) {
        if (hit.distance < maxDistance)
            return true;
    }

    if (intersectPlane(ray, plane, hit)) {
        if (hit.distance < maxDistance)
            return true;
    }

    return false;
}

// --------------------------------------------------
// Direct lighting
// --------------------------------------------------

vec3 calculateDirectLighting(
    Hit hit,
    Ray cameraRay,
    PointLight light,
    Sphere sphere,
    Plane plane
) {
    vec3 N = hit.normal;
    vec3 V = normalize(cameraRay.origin - hit.position);

    vec3 toLight = light.position - hit.position;

    float distanceSquared = dot(toLight, toLight);
    float distanceToLight = sqrt(distanceSquared);

    vec3 L = toLight / distanceToLight;

    float NdotL = max(dot(N, L), 0.0);

    if (NdotL <= 0.0)
        return vec3(0.0);

    // Shadow ray
    Ray shadowRay;
    shadowRay.origin = hit.position + N * 0.001;
    shadowRay.direction = L;

    if (isOccluded(
        shadowRay,
        sphere,
        plane,
        distanceToLight - 0.002
    )) {
        return vec3(0.0);
    }

    // BRDF
    vec3 F0 = calculateF0(hit.material);
    vec3 H = normalize(V + L);

    vec3 F = fresnelSchlick(
        max(dot(V, H), 0.0),
        F0
    );

    vec3 diffuse = calculateDiffuse(hit.material, F);

    vec3 specular = calculateSpecular(
        hit.material,
        N,
        V,
        L
    );

    vec3 BRDF = diffuse + specular;

    // Inverse square law
    vec3 irradiance = light.intensity / distanceSquared;

    return BRDF * irradiance * NdotL;
}


// --------------------------------------------------
// Main
// --------------------------------------------------

void main() {
    vec2 uv = gl_FragCoord.xy / uResolution;

    Ray ray = generateRay(uv);

    // Sphere
    Sphere sphere;
    sphere.center = vec3(0.0, 0.0, -3.0);
    sphere.radius = 1.0;

    sphere.material.albedo = vec3(0.35, 0.17, 0.08);
    sphere.material.roughness = 0.9;
    sphere.material.metallic = 0.0;
    sphere.material.ior = 1.46;

    // Plane
    Plane plane;
    plane.point = vec3(0.0, -1.0, 0.0);
    plane.normal = vec3(0.0, 1.0, 0.0);

    plane.material.albedo = vec3(0.7);
    plane.material.roughness = 0.2;
    plane.material.metallic = 0.0;
    plane.material.ior = 1.5;

    // Point light
    PointLight light;
    light.position = vec3(-2.0, 3.0, 0.0);
    light.intensity = vec3(80.0);

    Hit hit;

    if (closestHit(ray, sphere, plane, hit)) {

        vec3 color = calculateDirectLighting(
            hit,
            ray,
            light,
            sphere,
            plane
        );

        // Temporary display mapping
        color = color / (color + vec3(1.0));
        color = pow(color, vec3(1.0 / 2.2));

        FragColor = vec4(color, 1.0);
    }
    else {
        FragColor = vec4(0.02, 0.02, 0.02, 1.0);
    }
}
