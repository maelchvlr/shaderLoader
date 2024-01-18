#version 430 core

layout (local_size_x = 10) in;

struct Particle {
    vec2 position;
    vec2 velocity;
    vec4 color;
    float age;
    float lifeTime;
};

layout(std430, binding = 0) buffer ParticleBuffer {
    Particle particles[];
};

uniform vec2 mousePos;
uniform float deltaTime;

float generateRandomLifetime(uint id) {
    float randomValue = fract(sin(float(id) * 78.233 + deltaTime) * 43758.5453123);
    return mix(1.0, 5.0, randomValue); // Lifetime between 1 and 5 seconds
}

void main() {
    uint id = gl_GlobalInvocationID.x;
    if (id < particles.length()) {
        // Increment age
        particles[id].age += deltaTime;

        // Fade effect as the particle's age approaches its lifetime
        particles[id].color.a = 1.0 - (particles[id].age / particles[id].lifeTime);

        // Check if age exceeds lifetime
        if (particles[id].age >= particles[id].lifeTime) {
            // Reset particle position, age, and restore opacity
            particles[id].position = mousePos;
            particles[id].age = 0.0;
            particles[id].lifeTime = generateRandomLifetime(id); 
            particles[id].color.a = 1.0; // Restore full opacity
        } else {
            // Update particle position
            particles[id].position += particles[id].velocity * deltaTime;
        }
    }
}