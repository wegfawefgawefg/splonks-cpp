#version 330

// Input vertex attributes (from vertex shader)
in vec2 fragTexCoord;
in vec4 fragColor;

// Input uniform values
uniform sampler2D texture0;
uniform vec4 colDiffuse;
// uniform float grayscaleFactor;
// pass in tile position
uniform vec2 tilePos;
uniform vec2 camPos;

// Output fragment color
out vec4 finalColor;

// NOTE: Add here your custom variables

void main()
{
    // Texel color fetching from texture sampler
    vec4 texelColor = texture(texture0, fragTexCoord)*colDiffuse*fragColor;


    // get distance from tile to camera
    // float dist = distance(tilePos, camPos);

    // // if distance is greater than k, then set texel color to black
    // let k = 20.0;
    // if (dist > k)
    // {
    //     texelColor = vec4(0.0, 0.0, 0.0, 1.0);
    // }
    
    // Convert texel color to grayscale using NTSC conversion weights
    // float gray = dot(texelColor.rgb, vec3(0.299, 0.587, 0.114));
    // if grayscale factor is 1, then the grayscale is the only color
    // if grayscale factor is 0, then the grayscale is the original color
    // float r_rot = 0.5 + 0.5 * cos(0.5 * grayscaleFactor * 1.0);
    // float g_rot = 0.5 + 0.5 * cos(0.5 * grayscaleFactor * 1.0 + 2.0);
    // float b_rot = 0.5 + 0.5 * cos(0.5 * grayscaleFactor * 1.0 + 4.0);
    // float r = mix(gray, texelColor.r, grayscaleFactor * r_rot);
    // float g = mix(gray, texelColor.g, grayscaleFactor * g_rot);
    // float b = mix(gray, texelColor.b, grayscaleFactor * b_rot);
    // float r = mix(gray, texelColor.r, grayscaleFactor);
    // float g = r;
    // float b = r;

    // Calculate final fragment color
    // finalColor = vec4(r, g, b, texelColor.a);
    finalColor = texelColor;
}
